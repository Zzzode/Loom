/// Real terminal lifecycle, tool recovery, and optional round-budget workflows.
/// The SSE fixture gates interactions on requests and complete visible frames.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), {}};
}

std::string escape_json(const std::string& value) {
    std::string result;
    for (char ch : value) {
        if (ch == '"' || ch == '\\') result += '\\';
        result += ch;
    }
    return result;
}

std::string event(const std::string& type, const std::string& data) {
    return "event: " + type + "\ndata: " + data + "\n\n";
}

std::string message_start() {
    return event("message_start", R"({"type":"message_start","message":{"id":"msg_pty","type":"message","role":"assistant","model":"pty-test","content":[]}})");
}

std::string tool_response(const std::string& name, const std::string& input,
                          const std::string& id = "tool_pty") {
    return message_start() +
        event("content_block_start", "{\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"" + id + "\",\"name\":\"" + name + "\",\"input\":{}}}") +
        event("content_block_delta", "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"" + escape_json(input) + "\"}}") +
        event("content_block_stop", R"({"type":"content_block_stop","index":0})") +
        event("message_delta", R"({"type":"message_delta","delta":{"stop_reason":"tool_use","stop_sequence":null},"usage":{"output_tokens":12}})") +
        event("message_stop", R"({"type":"message_stop"})");
}

std::string text_response(const std::string& text) {
    return message_start() +
        event("content_block_start", R"({"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}})") +
        event("content_block_delta", "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"" + escape_json(text) + "\"}}") +
        event("content_block_stop", R"({"type":"content_block_stop","index":0})") +
        event("message_delta", R"({"type":"message_delta","delta":{"stop_reason":"end_turn","stop_sequence":null},"usage":{"output_tokens":5}})") +
        event("message_stop", R"({"type":"message_stop"})");
}

bool send_all(int fd, const std::string& bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto written = ::send(fd, bytes.data() + offset, bytes.size() - offset, 0);
        if (written <= 0) return false;
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

// Own the sockets so peer closure is observable without sending heartbeat data.
class ModelServer {
public:
    ModelServer(bool background, bool permission, const fs::path& target, bool direct = false,
                std::string recovery = {})
        : background_(background), permission_(permission), direct_(direct), target_(target),
          recovery_(std::move(recovery)) {
        listener_ = ::socket(AF_INET, SOCK_STREAM, 0);
        require(listener_ >= 0, "socket failed");
        ::fcntl(listener_, F_SETFD, FD_CLOEXEC);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        require(::bind(listener_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind failed");
        socklen_t size = sizeof(address);
        require(::getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &size) == 0, "getsockname failed");
        port_ = ntohs(address.sin_port);
        require(::listen(listener_, 8) == 0, "listen failed");
    }
    void start() {
        accept_thread_ = std::thread([this] {
            while (!stopping_) {
                pollfd descriptor{listener_, POLLIN, 0};
                if (::poll(&descriptor, 1, 100) <= 0) continue;
                const int fd = ::accept(listener_, nullptr, nullptr);
                if (fd < 0) continue;
                std::lock_guard lock(mutex_);
                sockets_.push_back(fd);
                clients_.emplace_back([this, fd] { serve(fd); });
            }
        });
    }
    ~ModelServer() {
        stopping_ = true;
        if (accept_thread_.joinable()) accept_thread_.join();
        {
            std::lock_guard lock(mutex_);
            for (int fd : sockets_) ::shutdown(fd, SHUT_RDWR);
        }
        for (auto& client : clients_) client.join();
        for (int fd : sockets_) ::close(fd);
        ::close(listener_);
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }
    std::atomic<int> parked{0};
    std::atomic<int> closed{0};
    std::atomic<int> requests{0};
    std::atomic<bool> retry_fault{true};
    std::vector<std::string> request_bodies() {
        std::lock_guard lock(mutex_);
        return request_bodies_;
    }
    bool connections_closed() {
        std::unique_lock lock(mutex_);
        return closed_cv_.wait_for(lock, 2s, [this] { return closed.load() == parked.load(); });
    }
private:
    void serve(int fd) {
        std::string request;
        std::array<char, 8192> buffer{};
        std::size_t body_end = std::string::npos;
        while (body_end == std::string::npos || request.size() < body_end) {
            const auto count = ::recv(fd, buffer.data(), buffer.size(), 0);
            if (count <= 0) return;
            request.append(buffer.data(), static_cast<std::size_t>(count));
            const auto headers_end = request.find("\r\n\r\n");
            if (headers_end == std::string::npos) continue;
            std::string headers = request.substr(0, headers_end);
            std::transform(headers.begin(), headers.end(), headers.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            const auto length = headers.find("content-length:");
            if (length == std::string::npos) return;
            body_end = headers_end + 4 + std::stoul(headers.substr(length + 15));
        }
        {
            std::lock_guard lock(mutex_);
            request_bodies_.push_back(request.substr(request.find("\r\n\r\n") + 4));
        }
        const int index = requests.fetch_add(1);
        if (recovery_.starts_with("round-")) {
            const int rounds = recovery_ == "round-unbounded" ? 22 : recovery_ == "round-budget" ? 3 : 1;
            std::string body;
            std::string status = "200 OK";
            std::string content_type = "text/event-stream";
            if (index < rounds) {
                const auto command = "printf 'PTY-round-" + std::to_string(index) + "\\n' >> '" + target_.string() + "'";
                body = tool_response("Bash", "{\"command\":\"" + escape_json(command) + "\"}", "pty-round-" + std::to_string(index));
            } else if (recovery_ == "round-retry" && retry_fault.load()) {
                status = "503 Service Unavailable";
                content_type = "application/json";
                body = R"({"type":"error","error":{"type":"api_error","message":"PTY request failure after Continue"}})";
            } else {
                body = text_response("PTY round-budget complete");
            }
            send_all(fd, "HTTP/1.1 " + status + "\r\nContent-Type: " + content_type +
                "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        if (!recovery_.empty()) {
            const bool retry = recovery_.starts_with("api-retry");
            std::string body;
            std::string status = "200 OK";
            std::string content_type = "text/event-stream";
            if (index == 0) {
                const auto command = recovery_ == "bash-failure"
                    ? "cat '" + (target_.parent_path() / "missing.txt").string() + "'"
                    : "printf " + std::string(recovery_ == "permission-denial" ? "forbidden > '" : "PTY-once >> '") + target_.string() + "'";
                body = tool_response("Bash", "{\"command\":\"" + escape_json(command) + "\"}", "pty-first-bash");
            } else if (retry && retry_fault.load()) {
                if (recovery_ == "api-retry-network") {
                    ::shutdown(fd, SHUT_RDWR);
                    return;
                }
                status = "503 Service Unavailable";
                content_type = "application/json";
                body = R"({"type":"error","error":{"type":"api_error","message":"PTY injected request failure"}})";
            } else if (!retry && index == 1) {
                body = tool_response("Bash", R"({"command":"pwd"})", "pty-corrected-bash");
            } else {
                const bool follow_up = retry
                    ? resumed_responses_.fetch_add(1) != 0 : index >= 3;
                body = text_response(follow_up ? "PTY follow-up complete" : "PTY recovery complete");
            }
            send_all(fd, "HTTP/1.1 " + status + "\r\nContent-Type: " + content_type +
                "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        if (direct_) {
            const auto body = message_start() +
                event("content_block_start", R"({"type":"content_block_start","index":0,"content_block":{"type":"text","text":"direct model probe complete"}})") +
                event("content_block_stop", R"({"type":"content_block_stop","index":0})") +
                event("message_delta", R"({"type":"message_delta","delta":{"stop_reason":"end_turn","stop_sequence":null},"usage":{"output_tokens":5}})") +
                event("message_stop", R"({"type":"message_stop"})");
            send_all(fd, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        if (index == 0) {
            const std::string input = permission_
                ? "{\"file_path\":\"" + escape_json(target_.string()) + "\",\"content\":\"must not execute\"}"
                : "{\"description\":\"PTY lifecycle probe\",\"prompt\":\"PTY child stream\",\"name\":\"pty-agent\",\"run_in_background\":" + std::string(background_ ? "true" : "false") + "}";
            const auto body = tool_response(permission_ ? "Write" : "Agent", input);
            send_all(fd, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        const auto prefix = message_start() + event("content_block_start", R"({"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}})") +
            event("content_block_delta", R"({"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"parked streaming response"}})");
        if (!send_all(fd, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n" + prefix)) return;
        ++parked;
        // EOF is the gate: a cancelled producer must close its real TCP socket.
        while (::recv(fd, buffer.data(), buffer.size(), 0) > 0) {}
        {
            std::lock_guard lock(mutex_);
            ++closed;
        }
        closed_cv_.notify_all();
    }
    bool background_;
    bool permission_;
    bool direct_;
    fs::path target_;
    std::string recovery_;
    std::atomic<int> resumed_responses_{0};
    int listener_ = -1;
    unsigned short port_ = 0;
    std::atomic<bool> stopping_{false};
    std::mutex mutex_;
    std::condition_variable closed_cv_;
    std::vector<int> sockets_;
    std::vector<std::string> request_bodies_;
    std::vector<std::thread> clients_;
    std::thread accept_thread_;
};

// FTXUI may change styles inside a label. Read labels independently of SGR.
std::string terminal_text(const std::string& raw) {
    std::string result;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const auto ch = static_cast<unsigned char>(raw[i]);
        if (ch != 0x1b) {
            if (ch >= 32 || ch == '\n' || ch == '\r') result += raw[i];
            continue;
        }
        if (++i >= raw.size()) break;
        if (raw[i] == '[') {
            while (++i < raw.size()) {
                const auto control = static_cast<unsigned char>(raw[i]);
                if (control >= 0x40 && control <= 0x7e) break;
            }
        } else if (raw[i] == ']') {
            while (++i < raw.size()) {
                if (raw[i] == '\a') break;
                if (raw[i] == '\x1b' && i + 1 < raw.size() && raw[i + 1] == '\\') {
                    ++i;
                    break;
                }
            }
        }
    }
    return result;
}

struct ModelOptions {
    bool direct = false;
    std::string file_model;
    std::string environment_model = "pty-test-model";
    std::string settings_model;
    std::string cli_model;
    std::string runtime_model;
    std::string expected_model;
    std::vector<std::string> extra_arguments;
};

ModelOptions model_options(const std::string& scenario) {
    ModelOptions options;
    if (!scenario.starts_with("model-")) return options;
    options.direct = scenario.ends_with("-direct");
    options.file_model = "pty-file-model";
    options.environment_model.clear();
    options.expected_model = options.file_model;
    if (!scenario.starts_with("model-file-")) {
        options.environment_model = "pty-environment-model";
        options.expected_model = options.environment_model;
    }
    if (scenario.starts_with("model-settings-") || scenario.starts_with("model-cli-") ||
        scenario.starts_with("model-runtime-")) {
        options.settings_model = "pty-settings-model";
        options.expected_model = options.settings_model;
    }
    if (scenario.starts_with("model-cli-") || scenario.starts_with("model-runtime-")) {
        options.cli_model = "pty-cli-model";
        options.expected_model = options.cli_model;
    }
    if (scenario.starts_with("model-runtime-")) {
        options.runtime_model = "pty-runtime-model";
        options.expected_model = options.runtime_model;
    }
    return options;
}

class Terminal {
public:
    Terminal(const std::string& binary, const fs::path& root, const std::string& url,
             const ModelOptions& options) : root_(root) {
        std::vector<std::string> arguments{binary};
        if (options.direct) {
            arguments.insert(arguments.end(), {"--run-runtime-tool", "Agent", "--runtime-tool-input",
                R"({"description":"Direct model probe","prompt":"Return the probe result","name":"pty-agent"})"});
        } else arguments.push_back("--permissions");
        if (!options.settings_model.empty()) {
            arguments.push_back("--settings");
            arguments.push_back("{\"model\":\"" + options.settings_model + "\"}");
        }
        if (!options.cli_model.empty()) {
            arguments.push_back("--model");
            arguments.push_back(options.cli_model);
        }
        arguments.insert(arguments.end(), options.extra_arguments.begin(), options.extra_arguments.end());
        std::vector<char*> argv;
        for (auto& argument : arguments) argv.push_back(argument.data());
        argv.push_back(nullptr);
        winsize dimensions{};
        dimensions.ws_row = 40;
        dimensions.ws_col = 120;
        // Capture the attributes before exec can enter raw mode. Supplying
        // them to forkpty removes a startup race in the restoration assertion.
        int initial_master = -1;
        int initial_slave = -1;
        require(::openpty(&initial_master, &initial_slave, nullptr, nullptr, &dimensions) == 0, "openpty failed");
        const bool initial_ok = ::tcgetattr(initial_slave, &original_) == 0;
        ::close(initial_master);
        ::close(initial_slave);
        require(initial_ok, "initial terminal attributes unavailable");
        pid_ = ::forkpty(&master_, nullptr, &original_, &dimensions);
        require(pid_ >= 0, "forkpty failed");
        if (pid_ == 0) {
            ::chdir(root.c_str());
            ::setenv("TERM", "xterm-256color", 1);
            ::setenv("HOME", root.c_str(), 1);
            ::setenv("LOOM_CONFIG_DIR", (root / "config").c_str(), 1);
            ::setenv("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").c_str(), 1);
            ::setenv("LOOM_API_KEY", "pty-test-key", 1);
            ::setenv("LOOM_BASE_URL", url.c_str(), 1);
            if (options.environment_model.empty()) ::unsetenv("LOOM_MODEL");
            else ::setenv("LOOM_MODEL", options.environment_model.c_str(), 1);
            ::setenv("LOOM_DISABLE_THINKING", "1", 1);
            ::setenv("LOOM_WIRE_API", "messages", 1);
            ::setenv("LOOM_USE_BEDROCK", "0", 1);
            ::setenv("LOOM_USE_VERTEX", "0", 1);
            ::setenv("LOOM_USE_FOUNDRY", "0", 1);
            ::setenv("NO_PROXY", "127.0.0.1,localhost", 1);
            ::setenv("no_proxy", "127.0.0.1,localhost", 1);
            ::unsetenv("LOOM_AUTH_TOKEN");
            ::unsetenv("LOOM_TEAM_NAME");
            ::unsetenv("LOOM_AGENT_NAME");
            ::unsetenv("LOOM_AGENTS_JSON");
            ::unsetenv("LOOM_NON_INTERACTIVE");
            ::unsetenv("LOOM_SIMPLE_UI");
            ::execv(binary.c_str(), argv.data());
            ::_exit(127);
        }
        ::fcntl(master_, F_SETFL, ::fcntl(master_, F_GETFL) | O_NONBLOCK);
    }
    ~Terminal() {
        pump();
        std::ofstream(root_ / "terminal.raw", std::ios::binary) << output;
        // Release the PTY before reaping a forcibly stopped child: on macOS
        // slave teardown can wait for an open master to drain terminal data.
        ::close(master_);
        master_ = -1;
        if (!exited_) {
            ::kill(pid_, SIGKILL);
            const auto deadline = Clock::now() + 5s;
            for (;;) {
                const auto waited = ::waitpid(pid_, nullptr, WNOHANG);
                if (waited == pid_ || (waited < 0 && errno == ECHILD)) break;
                if (Clock::now() >= deadline) {
                    std::cerr << "PTY child could not be reaped after closing its master\n";
                    break;
                }
                ::poll(nullptr, 0, 20);
            }
        }
    }
    void input(const std::string& bytes) { require(::write(master_, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size()), "PTY input failed"); }
    void signal(int number) { require(::kill(pid_, number) == 0, "signal failed"); }
    bool alive() { pump(); return !exited_; }
    bool permission_visible() const {
        const auto text = terminal_text(output);
        return text.find("Allow") != std::string::npos && text.find("Deny") != std::string::npos;
    }
    bool wait(const std::function<bool()>& predicate, std::chrono::seconds timeout = 10s) {
        const auto deadline = Clock::now() + timeout;
        do {
            pump();
            if (predicate()) return true;
            if (exited_) return false;
            pollfd descriptor{master_, POLLIN, 0};
            ::poll(&descriptor, 1, 20);
        } while (Clock::now() < deadline);
        return false;
    }
    bool installed() const { return output.find("\x1b[?1049h") != std::string::npos && output.find("\x1b[?25") != std::string::npos; }
    std::string latest_frame_text() const {
        // FTXUI terminates each flushed full-screen frame with NUL. Historical
        // output and a partial streaming frame cannot prove the input is idle.
        const auto end = output.rfind('\0');
        if (end == std::string::npos) return {};
        const auto previous = end == 0 ? std::string::npos : output.rfind('\0', end - 1);
        const auto start = previous == std::string::npos ? 0 : previous + 1;
        return terminal_text(output.substr(start, end - start));
    }
    bool idle_response_visible(const std::string& response) const {
        const auto frame = latest_frame_text();
        if (frame.find(response) == std::string::npos || frame.find(" to interrupt") != std::string::npos ||
            frame.find("Model request failed") != std::string::npos || frame.find("Retry request") != std::string::npos) return false;
        constexpr std::array<std::string_view, 8> spinner_glyphs = {"✻", "❋", "✦", "✧", "✶", "✷", "✸", "✹"};
        return std::none_of(spinner_glyphs.begin(), spinner_glyphs.end(), [&](auto glyph) {
            return frame.find("  " + std::string(glyph) + " ") != std::string::npos;
        });
    }
    void verify_completion(int expected) {
        require(wait([&] { return exited_; }, 5s), "process did not finish");
        require(WIFEXITED(status_) && WEXITSTATUS(status_) == expected, "unexpected process exit status " + std::to_string(status_));
    }
    void verify_exit(int expected) {
        verify_completion(expected);
        require(output.find("\x1b[?1049l") != std::string::npos, "alternate screen not restored");
        termios restored{};
        require(::tcgetattr(master_, &restored) == 0, "final terminal attributes unavailable");
        require(original_.c_lflag == restored.c_lflag && original_.c_iflag == restored.c_iflag &&
                original_.c_oflag == restored.c_oflag && original_.c_cflag == restored.c_cflag &&
                std::equal(std::begin(original_.c_cc), std::end(original_.c_cc), std::begin(restored.c_cc)), "terminal attributes not restored");
    }
    std::string output;
private:
    void pump() {
        std::array<char, 16384> bytes{};
        for (;;) {
            const auto count = ::read(master_, bytes.data(), bytes.size());
            if (count <= 0) break;
            output.append(bytes.data(), static_cast<std::size_t>(count));
            const std::string_view chunk(bytes.data(), static_cast<std::size_t>(count));
            if (chunk.find("\x1b[6n") != std::string_view::npos) input("\x1b[1;1R");
        }
        if (!exited_ && ::waitpid(pid_, &status_, WNOHANG) == pid_) exited_ = true;
    }
    fs::path root_;
    int master_ = -1;
    pid_t pid_ = -1;
    bool exited_ = false;
    int status_ = 0;
    termios original_{};
};

std::string compact_json(std::string text) {
    // The inspected scalar fields contain no whitespace; preserve quoted paths.
    bool quoted = false;
    bool escaped = false;
    std::string result;
    for (char ch : text) {
        if (ch == '"' && !escaped) quoted = !quoted;
        if (quoted || !std::isspace(static_cast<unsigned char>(ch))) result += ch;
        escaped = ch == '\\' && !escaped;
    }
    return result;
}

std::string request_messages(const std::string& request) {
    const auto json = compact_json(request);
    const auto field = json.find("\"messages\":[");
    require(field != std::string::npos, "request has no messages array");
    const auto start = field + std::string("\"messages\":").size();
    bool quoted = false;
    bool escaped = false;
    int depth = 0;
    for (std::size_t i = start; i < json.size(); ++i) {
        const char ch = json[i];
        if (ch == '"' && !escaped) quoted = !quoted;
        if (!quoted) {
            if (ch == '[') ++depth;
            else if (ch == ']' && --depth == 0) return json.substr(start, i - start + 1);
        }
        escaped = ch == '\\' && !escaped;
    }
    throw std::runtime_error("unterminated messages array");
}

std::size_t occurrences(const std::string& text, const std::string& needle) {
    std::size_t count = 0;
    for (std::size_t pos = 0; (pos = text.find(needle, pos)) != std::string::npos; pos += needle.size()) ++count;
    return count;
}

void run_recovery_case(const std::string& binary, const std::string& scenario, const fs::path& root) {
    const bool retry = scenario.starts_with("api-retry");
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":false,"permissionMode":"default","extendedThinking":false})";
    const auto sentinel = root / "once.txt";
    ModelServer server(false, false, sentinel, false, scenario);
    Terminal terminal(binary, root, server.url(), ModelOptions{});
    server.start();
    require(terminal.wait([&] { return terminal.installed(); }), "terminal installation not observed");
    constexpr auto prompt = "Run the PTY recovery probe";
    terminal.input(std::string(prompt) + "\r");
    require(terminal.wait([&] { return terminal.permission_visible(); }), "initial Bash permission UI not observed");
    const auto after_first_permission = terminal.output.size();
    // Approval and denial exercise the actual keyboard permission path; the
    // only write command targets this test's private sentinel.
    terminal.input(scenario == "permission-denial" ? "n" : "y");
    require(terminal.wait([&] { return server.requests.load() >= 2; }), "failed / completed Bash result was not sent to the model");
    auto bodies = server.request_bodies();
    const auto first_result = request_messages(bodies.at(1));
    require(first_result.find("\"tool_use_id\":\"pty-first-bash\"") != std::string::npos, "committed Bash result lost its call identity");
    require(occurrences(first_result, prompt) == 1, "original user prompt was duplicated");

    if (retry) {
        require(read_file(sentinel) == "PTY-once", "approved Bash did not execute exactly once before API failure");
        require(first_result.find("\"is_error\":true") == std::string::npos, "completed Bash was marked failed");
        require(terminal.wait([&] {
            const auto text = terminal_text(terminal.output.substr(after_first_permission));
            return text.find("Model request failed") != std::string::npos &&
                text.find(scenario == "api-retry-network" ? "HTTP request failed" : "PTY injected request failure") != std::string::npos &&
                text.find("Retry request") != std::string::npos && text.find("Dismiss error") != std::string::npos;
        }), "idle retryable model error and keyboard actions were not visible");
        const auto failed_requests = server.requests.load();
        server.retry_fault = false;
        terminal.input("r");
        require(terminal.wait([&] { return server.requests.load() > failed_requests; }), "real R key did not retry the model request");
        bodies = server.request_bodies();
        for (std::size_t i = 1; i < bodies.size(); ++i) {
            require(request_messages(bodies[i]) == first_result, "request retry changed committed messages or tool results");
        }
    } else {
        require(first_result.find("\"is_error\":true") != std::string::npos, "failed / denied Bash result was not marked as an error");
        if (scenario == "permission-denial") {
            require(first_result.find("Permission denied") != std::string::npos, "permission denial diagnosis was lost");
            require(!fs::exists(sentinel), "denied Bash executed its write command");
        } else {
            require(first_result.find("No such file or directory") != std::string::npos, "Bash failure output was lost");
        }
        require(terminal.wait([&] {
            const auto text = terminal_text(terminal.output.substr(after_first_permission));
            return text.find("pwd") != std::string::npos &&
                text.find("[y] Allow  [n] Deny") != std::string::npos;
        }), "corrected Bash permission UI not observed");
        terminal.input("y");
    }

    require(terminal.wait([&] {
        return terminal.idle_response_visible("PTY recovery complete");
    }), "conversation did not recover to its final assistant response");
    const auto recovered_text = terminal_text(terminal.output);
    if (!retry) {
        require(recovered_text.find(scenario == "permission-denial" ? "Permission denied" : "No such file or directory") != std::string::npos,
                "tool error was not visible in the real terminal");
        require(recovered_text.find("Model request failed") == std::string::npos, "recoverable tool failure became a fatal model error");
        bodies = server.request_bodies();
        require(bodies.size() == 3, "Bash recovery repeated a completed request");
        const auto final_messages = request_messages(bodies.back());
        require(occurrences(final_messages, "\"type\":\"tool_use\"") == 2, "Bash recovery executed an unexpected tool call");
        require(final_messages.find("\"tool_use_id\":\"pty-corrected-bash\"") != std::string::npos,
                "corrected Bash result was not committed");
        require(occurrences(final_messages, "\"is_error\":false") == 1,
                "corrected Bash did not execute successfully");
    }
    terminal.input("Continue the PTY probe\r");
    require(terminal.wait([&] {
        return terminal.idle_response_visible("PTY follow-up complete");
    }), "new user input could not continue the recovered conversation");
    bodies = server.request_bodies();
    const auto continued_messages = request_messages(bodies.back());
    require(occurrences(continued_messages, prompt) == 1 &&
            occurrences(continued_messages, "Continue the PTY probe") == 1,
            "continuation duplicated or dropped a user prompt");
    if (retry) require(read_file(sentinel) == "PTY-once", "request retry / continuation executed the completed Bash again");
    terminal.input("\x04");
    terminal.verify_exit(0);
}

void run_round_budget_case(const std::string& binary, const std::string& scenario, const fs::path& root) {
    const bool unlimited = scenario == "round-unbounded";
    const bool click = scenario == "round-click";
    const bool retry = scenario == "round-retry";
    const int rounds = unlimited ? 22 : scenario == "round-budget" ? 3 : 1;
    std::ofstream(root / "config" / "settings.json") <<
        "{\"mouseMode\":" << (click ? "true" : "false") <<
        R"(,"permissionMode":"default","extendedThinking":false})";
    ModelOptions options;
    if (!unlimited) options.extra_arguments = click
        ? std::vector<std::string>{"--max-tool-rounds=1"}
        : std::vector<std::string>{"--max-tool-rounds", "1"};
    const auto sentinel = root / "rounds.txt";
    ModelServer server(false, false, sentinel, false, scenario);
    Terminal terminal(binary, root, server.url(), options);
    server.start();
    require(terminal.wait([&] { return terminal.installed(); }), "terminal installation not observed");
    constexpr auto prompt = "Run the PTY round-budget probe";
    terminal.input(std::string(prompt) + "\r");
    std::string expected_effects;
    for (int round = 0; round < rounds; ++round) {
        const auto marker = "PTY-round-" + std::to_string(round);
        require(terminal.wait([&] {
            const auto frame = terminal.latest_frame_text();
            return server.requests.load() == round + 1 && frame.find(marker) != std::string::npos &&
                frame.find("[y] Allow  [n] Deny") != std::string::npos;
        }), "new round's explicit Bash permission was not visible");
        // Approve only the fixture's single append in its isolated directory.
        terminal.input("y");
        expected_effects += marker + '\n';
        if (!unlimited) {
            require(terminal.wait([&] {
                const auto frame = terminal.latest_frame_text();
                return frame.find("Tool-round budget reached (1). Progress saved.") != std::string::npos &&
                    frame.find("[C] Continue") != std::string::npos;
            }), "committed tool-round pause and Continue action were not visible");
            require(server.requests.load() == round + 1, "explicit budget continued without user action");
            require(read_file(sentinel) == expected_effects, "paused tools were skipped or executed repeatedly");
            require(terminal.latest_frame_text().find("Retry request") == std::string::npos,
                    "tool-round pause offered API Retry instead of Continue");
            if (click) {
                const auto frame = terminal.latest_frame_text();
                const auto position = frame.find("Continue");
                const auto line_start = frame.rfind('\n', position);
                const auto column = position - (line_start == std::string::npos ? 0 : line_start + 1) + 3;
                const auto row = std::count(frame.begin(), frame.begin() + static_cast<std::ptrdiff_t>(position), '\n') + 1;
                const auto point = std::to_string(column) + ";" + std::to_string(row);
                terminal.input("\x1b[<0;" + point + "M\x1b[<0;" + point + "m");
            } else terminal.input(round % 2 == 0 ? "c" : "C");
        }
    }
    if (retry) {
        require(terminal.wait([&] {
            const auto frame = terminal.latest_frame_text();
            return frame.find("Model request failed") != std::string::npos &&
                frame.find("PTY request failure after Continue") != std::string::npos &&
                frame.find("Retry request") != std::string::npos;
        }), "Continue did not expose a separately retryable API failure");
        require(terminal.latest_frame_text().find("[C] Continue") == std::string::npos,
                "API failure retained a stale tool-round Continue action");
        const auto bodies = server.request_bodies();
        require(bodies.size() >= 3, "stream and fallback failures were not observed");
        const auto committed = request_messages(bodies.at(1));
        server.retry_fault = false;
        terminal.input("r");
        require(terminal.wait([&] { return terminal.idle_response_visible("PTY round-budget complete"); }),
                "API Retry after Continue did not complete");
        const auto resumed_bodies = server.request_bodies();
        for (std::size_t i = 1; i < resumed_bodies.size(); ++i)
            require(request_messages(resumed_bodies[i]) == committed,
                    "Continue / API Retry changed committed messages or repeated tools");
    } else require(terminal.wait([&] { return terminal.idle_response_visible("PTY round-budget complete"); }),
                   "tool rounds did not reach an idle final response");
    require(read_file(sentinel) == expected_effects, "continuation executed a completed append again");
    const auto bodies = server.request_bodies();
    const auto final_messages = request_messages(bodies.back());
    require(occurrences(final_messages, prompt) == 1, "round continuation repeated the original user prompt");
    require(occurrences(final_messages, "\"type\":\"tool_result\"") == static_cast<std::size_t>(rounds) &&
            occurrences(final_messages, "\"is_error\":false") == static_cast<std::size_t>(rounds),
            "round continuation lost or duplicated committed successful results");
    if (!retry) require(bodies.size() == static_cast<std::size_t>(rounds + 1),
                        "round continuation repeated a completed model request");
    terminal.input("\x04");
    terminal.verify_exit(0);
}

void run_round_budget_cli_case(const std::string& binary, const fs::path& root) {
    const std::vector<std::vector<std::string>> invalid_arguments = {
        {"--max-tool-rounds"}, {"--max-tool-rounds="},
        {"--max-tool-rounds", "0"}, {"--max-tool-rounds=0"},
        {"--max-tool-rounds", "-1"}, {"--max-tool-rounds=-1"},
        {"--max-tool-rounds", "1x"}, {"--max-tool-rounds=1x"},
        {"--max-tool-rounds", "4294967296"}, {"--max-tool-rounds=4294967296"},
        {"--max-tool-rounds=999999999999999999999999999999"},
        {"--max-tool-rounds=+1"}, {"--max-tool-rounds= 1"}, {"--max-tool-rounds=1 "},
        {"--max-tool-rounds=1", "--headless"},
        {"--max-tool-rounds", "1", "--simple-ui"},
    };
    for (const auto& arguments : invalid_arguments) {
        ModelOptions options;
        options.extra_arguments = arguments;
        Terminal terminal(binary, root, "http://127.0.0.1:9", options);
        terminal.verify_completion(1);
        require(!terminal.installed(), "invalid round budget entered the terminal UI");
        require(terminal_text(terminal.output).find("--max-tool-rounds") != std::string::npos,
                "invalid round budget had no actionable CLI diagnostic");
    }
    for (const auto& arguments : std::vector<std::vector<std::string>>{
             {"--max-tool-rounds", "4294967295", "--help"}, {"--max-tool-rounds=4294967295", "--help"}}) {
        ModelOptions options;
        options.extra_arguments = arguments;
        Terminal terminal(binary, root, "http://127.0.0.1:9", options);
        terminal.verify_completion(0);
        require(terminal_text(terminal.output).find("Usage:") != std::string::npos,
                "maximum valid uint32 round budget was rejected");
    }
}

void run_case(const std::string& binary, const std::string& scenario, const fs::path& root) {
    if (scenario == "round-cli") {
        run_round_budget_cli_case(binary, root);
        return;
    }
    if (scenario.starts_with("round-")) {
        run_round_budget_case(binary, scenario, root);
        return;
    }
    if (scenario == "bash-failure" || scenario == "permission-denial" || scenario.starts_with("api-retry")) {
        run_recovery_case(binary, scenario, root);
        return;
    }
    const bool idle = scenario.starts_with("idle-");
    const bool background = scenario.find("background-") != std::string::npos;
    const auto options = model_options(scenario);
    {
        std::ofstream settings(root / "config" / "settings.json");
        settings << "{\"mouseMode\":false,\"permissionMode\":\"default\",\"extendedThinking\":false";
        if (!options.file_model.empty()) settings << ",\"model\":\"" << options.file_model << "\"";
        settings << '}';
    }
    const bool permission = scenario == "permission-ctrl-d";
    ModelServer server(background, permission, root / "forbidden.txt", options.direct);
    Terminal terminal(binary, root, server.url(), options);
    server.start();
    if (options.direct) {
        terminal.verify_completion(0);
        const auto bodies = server.request_bodies();
        require(bodies.size() == 1, "direct Agent model request not captured");
        require(compact_json(bodies.front()).find("\"model\":\"" + options.expected_model + "\"") != std::string::npos,
                "direct Agent did not inherit final model " + options.expected_model);
        const auto record = compact_json(read_file(root / "runtime" / "pty-agent.json"));
        require(record.find("\"status\":\"completed\"") != std::string::npos, "direct Agent did not complete");
        return;
    }
    require(terminal.wait([&] { return terminal.installed(); }), "terminal installation not observed");
    if (scenario == "idle-ctrl-c") {
        terminal.input("\x03");
        require(terminal.wait([&] { return terminal_text(terminal.output).find("Press Ctrl-C again to exit") != std::string::npos; }), "first real Ctrl+C bypassed the UI interrupt handler");
        require(terminal.alive(), "first idle Ctrl+C exited");
        terminal.input("\x03\x03");
        terminal.verify_exit(0);
        return;
    }
    if (!options.runtime_model.empty()) {
        terminal.input("/model " + options.runtime_model + "\r");
        require(terminal.wait([&] {
            return terminal_text(terminal.output).find(options.runtime_model) != std::string::npos;
        }), "runtime model switch not rendered");
        require(server.requests.load() == 0, "model switch unexpectedly sent a model request");
    }
    if (!idle) {
        terminal.input("Run the lifecycle probe\r");
        if (permission) {
            require(terminal.wait([&] { return terminal.permission_visible(); }), "permission UI not observed");
        } else {
            // Default permission mode asks before delegation. Approve only
            // this fixture-controlled Agent invocation, never a file tool.
            require(terminal.wait([&] {
                return terminal.permission_visible() || server.parked.load() > 0;
            }), "Agent permission / request not observed");
            if (terminal.permission_visible()) terminal.input("y");
            require(terminal.wait([&] {
                return server.parked.load() >= (background ? 2 : 1) &&
                    compact_json(read_file(root / "runtime" / "pty-agent.json")).find("\"status\":\"running\"") != std::string::npos;
            }), "running Agent and parked model request not observed");
        }
    }
    if (!options.expected_model.empty()) {
        const auto bodies = server.request_bodies();
        require(bodies.size() >= static_cast<std::size_t>(background ? 3 : 2),
                "parent and child model requests not captured");
        const auto model = "\"model\":\"" + options.expected_model + "\"";
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            require(compact_json(bodies[i]).find(model) != std::string::npos,
                    "request " + std::to_string(i) + " did not inherit final model " + options.expected_model);
        }
    }
    if (scenario.ends_with("sighup")) terminal.signal(SIGHUP);
    else if (scenario.ends_with("sigterm")) terminal.signal(SIGTERM);
    else terminal.input("\x04");
    terminal.verify_exit(scenario.ends_with("ctrl-d") ? 0 : 1);
    if (!idle && !permission) {
        const auto record = compact_json(read_file(root / "runtime" / "pty-agent.json"));
        require(record.find("\"status\":\"cancelled\"") != std::string::npos && record.find("\"cancel_requested\":true") != std::string::npos, "Agent cancellation was not persisted");
        require(server.connections_closed(), "model connections remained open after exit");
    }
    if (permission) require(!fs::exists(root / "forbidden.txt"), "pending permission executed Write");
}
} // namespace

int main(int argc, char** argv) {
    ::signal(SIGPIPE, SIG_IGN);
    if (argc != 2) return 2;
    const char* binary = ::getenv("LOOM_BINARY");
    if (!binary || ::access(binary, X_OK) != 0) return 125;
    // Bash's destructive-command guard also examines path arguments; a
    // recovery path containing "shutdown" would reject even fixture reads.
    const std::string scenario(argv[1]);
    std::string pattern = scenario == "bash-failure" || scenario == "permission-denial" || scenario.starts_with("api-retry") || scenario.starts_with("round-")
        ? "/tmp/loom_pty_recovery_XXXXXX" : "/tmp/loom_pty_shutdown_XXXXXX";
    const char* directory = ::mkdtemp(pattern.data());
    if (!directory) return 1;
    const fs::path root(directory);
    int result = 0;
    try {
        fs::create_directories(root / "config");
        run_case(binary, argv[1], root);
        std::cout << "PASS " << argv[1] << '\n';
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << argv[1] << ": " << error.what() << '\n';
        result = 1;
    }
    std::error_code ignored;
    if (result == 0) fs::remove_all(root, ignored);
    else std::cerr << "Diagnostic directory: " << root << '\n';
    return result;
}
