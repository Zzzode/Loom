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
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <openssl/ssl.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif
#include <signal.h>
#include <sys/ioctl.h>
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
        switch (ch) {
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (ch == '"' || ch == '\\') result += '\\';
            result += ch;
        }
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
                std::string recovery = {}, std::string answer = {}, bool tls = false)
        : background_(background), permission_(permission), direct_(direct), target_(target),
          recovery_(std::move(recovery)), answer_(std::move(answer)) {
        if (tls) {
            tls_context_.reset(SSL_CTX_new(TLS_server_method()));
            require(tls_context_ != nullptr, "TLS server context unavailable");
            require(SSL_CTX_use_certificate_file(tls_context_.get(), ::getenv("LOOM_PTY_TLS_CA_CERT"), SSL_FILETYPE_PEM) == 1, "TLS fixture certificate invalid");
            require(SSL_CTX_use_PrivateKey_file(tls_context_.get(), ::getenv("LOOM_PTY_TLS_CA_KEY"), SSL_FILETYPE_PEM) == 1, "TLS fixture private key invalid");
        }
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
    std::string url() const { return std::string(tls_context_ ? "https://127.0.0.1:" : "http://127.0.0.1:") + std::to_string(port_); }
    std::atomic<int> parked{0};
    std::atomic<int> closed{0};
    std::atomic<int> requests{0};
    std::atomic<bool> retry_fault{true};
    std::atomic<bool> selection_grow{false};
    std::atomic<bool> chain_finish{false};
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
        std::unique_ptr<SSL, decltype(&SSL_free)> tls(nullptr, SSL_free);
        if (tls_context_) {
            tls.reset(SSL_new(tls_context_.get()));
            if (!tls || SSL_set_fd(tls.get(), fd) != 1 || SSL_accept(tls.get()) != 1) return;
        }
        auto read_bytes = [&](char* bytes, std::size_t size) -> ssize_t {
            return tls ? SSL_read(tls.get(), bytes, static_cast<int>(size)) : ::recv(fd, bytes, size, 0);
        };
        auto send_bytes = [&](const std::string& bytes) {
            if (!tls) return send_all(fd, bytes);
            std::size_t offset = 0;
            while (offset < bytes.size()) {
                const int count = SSL_write(tls.get(), bytes.data() + offset,
                                            static_cast<int>(bytes.size() - offset));
                if (count <= 0) return false;
                offset += static_cast<std::size_t>(count);
            }
            return true;
        };
        std::string request;
        std::array<char, 8192> buffer{};
        std::size_t body_end = std::string::npos;
        while (body_end == std::string::npos || request.size() < body_end) {
            const auto count = read_bytes(buffer.data(), buffer.size());
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
        if (recovery_ == "compressed-chain-live") {
            if (index == 0) {
                const auto command = "printf PTY-ACTIVITY; for attempt in {1..300}; do [ -f '" + target_.string() +
                    "' ] && break; sleep 0.1; done";
                const auto body = tool_response("Bash", "{\"command\":\"" + escape_json(command) + "\"}", "pty-chain-bash");
                send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " +
                    std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                ::shutdown(fd, SHUT_WR);
                return;
            }
            const auto prefix = message_start() +
                event("content_block_start", R"({"type":"content_block_start","index":0,"content_block":{"type":"thinking","thinking":""}})") +
                event("content_block_delta", R"({"type":"content_block_delta","index":0,"delta":{"type":"thinking_delta","thinking":"PTY chain reasoning"}})");
            if (!send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n" + prefix)) return;
            ++parked;
            while (!chain_finish.load() && !stopping_) {
                if (selection_grow.exchange(false) && !send_bytes(event("content_block_delta",
                    R"({"type":"content_block_delta","index":0,"delta":{"type":"thinking_delta","thinking":" UPDATED"}})"))) return;
                pollfd descriptor{fd, POLLIN, 0};
                if (::poll(&descriptor, 1, 20) > 0 && read_bytes(buffer.data(), buffer.size()) <= 0) return;
            }
            if (stopping_) return;
            const auto suffix = event("content_block_stop", R"({"type":"content_block_stop","index":0})") +
                event("content_block_start", R"({"type":"content_block_start","index":1,"content_block":{"type":"text","text":""}})") +
                event("content_block_delta", R"({"type":"content_block_delta","index":1,"delta":{"type":"text_delta","text":"PTY chain complete"}})") +
                event("content_block_stop", R"({"type":"content_block_stop","index":1})") +
                event("message_delta", R"({"type":"message_delta","delta":{"stop_reason":"end_turn","stop_sequence":null},"usage":{"output_tokens":5}})") +
                event("message_stop", R"({"type":"message_stop"})");
            send_bytes(suffix);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        if (!answer_.empty()) {
            if (recovery_ == "selection-streaming") {
                const auto body = message_start() +
                    event("content_block_start", R"({"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}})") +
                    event("content_block_delta", "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"" + escape_json(answer_) + "\"}}");
                if (!send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n" + body)) return;
                ++parked;
                for (;;) {
                    if (selection_grow.exchange(false)) {
                        if (!send_bytes(event("content_block_delta",
                            R"({"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":" UPDATED"}})"))) break;
                    }
                    pollfd descriptor{fd, POLLIN, 0};
                    if (::poll(&descriptor, 1, 20) > 0 && read_bytes(buffer.data(), buffer.size()) <= 0) break;
                }
                ++closed;
                closed_cv_.notify_all();
                return;
            }
            const auto body = text_response(answer_);
            send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " +
                std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
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
            send_bytes("HTTP/1.1 " + status + "\r\nContent-Type: " + content_type +
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
            send_bytes("HTTP/1.1 " + status + "\r\nContent-Type: " + content_type +
                "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        if (direct_) {
            // Emit the answer through the same text deltas as the interactive
            // fixture, so the real streaming UI receives visible content.
            const auto body = text_response("direct model probe complete");
            send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        if (index == 0) {
            const std::string input = permission_
                ? "{\"file_path\":\"" + escape_json(target_.string()) + "\",\"content\":\"must not execute\"}"
                : "{\"description\":\"PTY lifecycle probe\",\"prompt\":\"PTY child stream\",\"name\":\"pty-agent\",\"run_in_background\":" + std::string(background_ ? "true" : "false") + "}";
            const auto body = tool_response(permission_ ? "Write" : "Agent", input);
            send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        const auto prefix = message_start() + event("content_block_start", R"({"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}})") +
            event("content_block_delta", R"({"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"parked streaming response"}})");
        if (!send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n" + prefix)) return;
        ++parked;
        // EOF is the gate: a cancelled producer must close its real TCP socket.
        while (read_bytes(buffer.data(), buffer.size()) > 0) {}
        {
            std::lock_guard lock(mutex_);
            ++closed;
        }
        closed_cv_.notify_all();
    }
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> tls_context_{nullptr, SSL_CTX_free};
    bool background_;
    bool permission_;
    bool direct_;
    fs::path target_;
    std::string recovery_;
    std::string answer_;
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
    bool capture_clipboard = false;
    bool shell_handoff = false;
    std::string handoff_helper;
    bool tls = false;
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
             const ModelOptions& options) : root_(root), shell_handoff_(options.shell_handoff) {
        if (options.tls) {
            require(::getenv("LOOM_PTY_TLS_CA_CERT") && ::getenv("LOOM_PTY_TLS_CA_KEY"),
                    "TLS fixture certificate paths unavailable");
#if defined(__APPLE__)
            require(::getenv("LOOM_PTY_TLS_CA_LIBRARY") != nullptr, "TLS test CA library unavailable");
#endif
        }
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
        if (options.shell_handoff) {
            const auto shell = ::access("/bin/zsh", X_OK) == 0 ? "/bin/zsh" : "/usr/bin/zsh";
            require(::access(shell, X_OK) == 0, "zsh unavailable for shell handoff");
            std::vector<std::string> wrapped{shell, "-f", "-c", R"zsh(
"$@" < /dev/tty &
loom_pid=$!
wait "$loom_pid"
loom_status=$?
printf '%s' "$loom_status" > "$PTY_HANDOFF_DIR/loom.status"
printf '\nPTY-SHELL-READY\n'
IFS= read -r shell_line
printf '%s' "$shell_line" > "$PTY_HANDOFF_DIR/shell-input.txt"
printf '\nPTY-SHELL-RECEIVED\n'
)zsh", "zsh"};
            wrapped.insert(wrapped.end(), {options.handoff_helper, "--shell-loom-child"});
            wrapped.insert(wrapped.end(), arguments.begin(), arguments.end());
            arguments = std::move(wrapped);
        }
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
            if (options.capture_clipboard) {
                const char* inherited = ::getenv("PATH");
                const auto path = (root / "clipboard-bin").string() + ":" +
                    (inherited ? inherited : "/usr/bin:/bin");
                ::setenv("PATH", path.c_str(), 1);
                ::setenv("PTY_CLIPBOARD_FILE", (root / "clipboard.txt").c_str(), 1);
                ::unsetenv("SSH_CONNECTION");
                ::unsetenv("SSH_TTY");
            }
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
            ::setenv("PTY_HANDOFF_DIR", root.c_str(), 1);
            // The server ignores SIGPIPE; the real application must inherit
            // the user's default signal disposition for cancellation tests.
            ::signal(SIGPIPE, SIG_DFL);
            if (options.tls) {
                ::setenv("LOOM_PTY_TLS_CA_FILE", ::getenv("LOOM_PTY_TLS_CA_CERT"), 1);
#if defined(__APPLE__)
                ::setenv("DYLD_INSERT_LIBRARIES", ::getenv("LOOM_PTY_TLS_CA_LIBRARY"), 1);
#else
                ::setenv("SSL_CERT_FILE", ::getenv("LOOM_PTY_TLS_CA_CERT"), 1);
#endif
            }
            ::execv(arguments.front().c_str(), argv.data());
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
            if (shell_handoff_) {
                ::kill(-pid_, SIGCONT);
                ::kill(-pid_, SIGKILL);
            } else ::kill(pid_, SIGKILL);
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
    void signal(int number) {
        const auto child = shell_handoff_ ? static_cast<pid_t>(std::stoi(read_file(root_ / "loom.pid"))) : pid_;
        require(::kill(child, number) == 0, "signal failed");
    }
    bool mode_enabled(int mode) const {
        const auto prefix = "\x1b[?" + std::to_string(mode);
        const auto enabled = output.rfind(prefix + 'h');
        const auto disabled = output.rfind(prefix + 'l');
        return enabled != std::string::npos && (disabled == std::string::npos || enabled > disabled);
    }
    void resize(unsigned short columns, unsigned short rows) {
        winsize dimensions{};
        dimensions.ws_col = columns;
        dimensions.ws_row = rows;
        require(::ioctl(master_, TIOCSWINSZ, &dimensions) == 0, "PTY resize failed");
        signal(SIGWINCH);
    }
    void prepare_transcript_shortcut() {
        // On macOS, IEXTEN + VDISCARD can consume Ctrl+O before FTXUI receives
        // it. Isolate that separate driver issue only for the transcript case;
        // verify_exit still requires Loom to restore the original attributes.
        termios attributes{};
        require(::tcgetattr(master_, &attributes) == 0, "transcript terminal attributes unavailable");
        std::ofstream(root_ / "transcript-driver.txt")
            << "original VDISCARD=" << static_cast<unsigned>(attributes.c_cc[VDISCARD])
            << "\noriginal IEXTEN=" << ((attributes.c_lflag & IEXTEN) != 0)
            << "\nfixture VDISCARD=" << static_cast<unsigned>(_POSIX_VDISABLE) << '\n';
        attributes.c_cc[VDISCARD] = _POSIX_VDISABLE;
        require(::tcsetattr(master_, TCSANOW, &attributes) == 0, "could not isolate transcript control-key driver");
    }
    std::string process_state() const {
        if (!exited_) return "alive";
        if (WIFEXITED(status_)) return "exit=" + std::to_string(WEXITSTATUS(status_));
        if (WIFSIGNALED(status_)) return "signal=" + std::to_string(WTERMSIG(status_));
        return "status=" + std::to_string(status_);
    }
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
    // FTXUI terminates each flushed full-screen frame with NUL.
    std::string latest_frame() const {
        const auto end = output.rfind('\0');
        if (end == std::string::npos) return {};
        const auto previous = end == 0 ? std::string::npos : output.rfind('\0', end - 1);
        const auto start = previous == std::string::npos ? 0 : previous + 1;
        return output.substr(start, end - start);
    }
    std::string latest_frame_text() const {
        // Complete frames exclude historical output and partial stream data.
        return terminal_text(latest_frame());
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
    bool shell_handoff_ = false;
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

// Inspect complete real-screen frames, not historical output or the input
// buffer: both boundary glyphs must be visible on distinct physical rows.
int prompt_row_distance(const std::string& frame) {
    const auto head = frame.find("首");
    const auto tail = frame.find("終");
    if (head == std::string::npos || tail == std::string::npos || tail <= head)
        return -1;
    return static_cast<int>(std::count(
        frame.begin() + static_cast<std::ptrdiff_t>(head),
        frame.begin() + static_cast<std::ptrdiff_t>(tail), '\n'));
}

void run_prompt_wrap_case(const std::string& binary, const fs::path& root) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":false,"permissionMode":"default","extendedThinking":false})";
    ModelServer server(false, false, root / "unused.txt", true);
    Terminal terminal(binary, root, server.url(), ModelOptions{});
    server.start();
    require(terminal.wait([&] { return terminal.installed(); }),
            "terminal installation not observed");
    std::string prompt = "首 中文🙂 ";
    for (int i = 0; i < 4; ++i)
        prompt += "block-" + std::to_string(i) + "-abcdefghijklmnopqrstuv 中文🙂 ";
    prompt += "終";
    terminal.input(prompt);
    require(terminal.wait([&] {
        return prompt_row_distance(terminal.latest_frame_text()) >= 1;
    }), "long mixed prompt tail was clipped instead of appearing on a wrapped row");
    const auto wide_rows = prompt_row_distance(terminal.latest_frame_text());
    require(server.requests.load() == 0, "typing unexpectedly submitted the prompt");

    terminal.resize(60, 40);
    require(terminal.wait([&] {
        return prompt_row_distance(terminal.latest_frame_text()) > wide_rows;
    }), "real PTY resize did not reflow the prompt while keeping both ends visible");
    const auto resized_frame = terminal.latest_frame_text();
    require(resized_frame.find("中文") != std::string::npos &&
            resized_frame.find("🙂") != std::string::npos,
            "resize lost Chinese or emoji glyphs");
    require(server.requests.load() == 0, "resize unexpectedly submitted the prompt");

    terminal.input("\r");
    require(terminal.wait([&] {
        return terminal.idle_response_visible("direct model probe complete");
    }), "wrapped prompt did not submit and finish through the local model fixture");
    const auto bodies = server.request_bodies();
    require(bodies.size() == 1, "wrapped prompt caused duplicate model requests");
    const auto messages = request_messages(bodies.front());
    require(messages.find("\"content\":\"" + escape_json(prompt) + "\"") != std::string::npos &&
            occurrences(messages, prompt) == 1,
            "visual wrapping or resize changed the original submitted prompt bytes");
    terminal.input("\x04");
    terminal.verify_exit(0);
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
            // A Preparing activity can mention pwd before its dialog exists.
            // Historical/partial output can retain the first dialog's Allow;
            // approve only the corrected command in one complete current frame.
            const auto frame = terminal.latest_frame_text();
            return frame.find("Permission") != std::string::npos &&
                frame.find(R"({"command":"pwd"})") != std::string::npos &&
                frame.find("[y] Allow  [n] Deny") != std::string::npos;
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

// Decode actual ANSI frames into terminal cells. The fixture owns a fixed
// 120 x 40 PTY, so mouse reports and highlight assertions use physical cells.
struct TerminalCell {
    std::string text = " ";
    std::string background;
};
struct TerminalFrame {
    std::array<std::array<TerminalCell, 120>, 40> cells{};
    std::string row(int y) const {
        std::string result;
        for (const auto& cell : cells.at(y)) result += cell.text;
        while (!result.empty() && result.back() == ' ') result.pop_back();
        return result;
    }
    std::pair<int, int> find(const std::string& label) const {
        for (int y = 0; y < 40; ++y) {
            for (int x = 0; x < 120; ++x) {
                std::string value;
                for (int next = x; next < 120 && value.size() < label.size(); ++next)
                    value += cells[y][next].text;
                if (value == label) return {x, y};
            }
        }
        throw std::runtime_error("visible frame lacks " + label);
    }
};

TerminalFrame decode_frame(const std::string& bytes) {
    TerminalFrame frame;
    int x = 0, y = 0;
    std::string background;
    for (std::size_t i = 0; i < bytes.size();) {
        const auto ch = static_cast<unsigned char>(bytes[i]);
        if (ch == 0x1b && i + 1 < bytes.size()) {
            if (bytes[i + 1] == '[') {
                auto end = i + 2;
                while (end < bytes.size() && !(bytes[end] >= '@' && bytes[end] <= '~')) ++end;
                if (end == bytes.size()) break;
                const auto params = bytes.substr(i + 2, end - i - 2);
                std::vector<int> values;
                std::size_t start = 0;
                while (start <= params.size()) {
                    const auto semi = params.find(';', start);
                    const auto value = params.substr(start, semi - start);
                    values.push_back(value.empty() ? 0 : std::atoi(value.c_str()));
                    if (semi == std::string::npos) break;
                    start = semi + 1;
                }
                const auto amount = std::max(1, values.front());
                switch (bytes[end]) {
                case 'A': y = std::max(0, y - amount); break;
                case 'B': y = std::min(39, y + amount); break;
                case 'C': x = std::min(119, x + amount); break;
                case 'D': x = std::max(0, x - amount); break;
                case 'H': case 'f':
                    y = std::clamp(amount - 1, 0, 39);
                    x = values.size() > 1 ? std::clamp(values[1] - 1, 0, 119) : 0;
                    break;
                case 'm':
                    for (std::size_t n = 0; n < values.size(); ++n) {
                        if (values[n] == 0 || values[n] == 49) background.clear();
                        else if (values[n] >= 40 && values[n] <= 47)
                            background = std::to_string(values[n]);
                        else if (values[n] >= 100 && values[n] <= 107)
                            background = std::to_string(values[n]);
                        else if (values[n] == 38 || values[n] == 48) {
                            const bool bg = values[n] == 48;
                            const auto count = n + 1 < values.size() && values[n + 1] == 2 ? 4U : 2U;
                            if (n + count < values.size()) {
                                if (bg) {
                                    background = "48";
                                    for (std::size_t k = 1; k <= count; ++k)
                                        background += ";" + std::to_string(values[n + k]);
                                }
                                n += count;
                            }
                        }
                    }
                    break;
                default: break;
                }
                i = end + 1;
                continue;
            }
            if (bytes[i + 1] == ']') {
                i += 2;
                while (i < bytes.size() && bytes[i] != '\a' &&
                       !(bytes[i] == '\x1b' && i + 1 < bytes.size() && bytes[i + 1] == '\\')) ++i;
                i += i < bytes.size() && bytes[i] == '\x1b' ? 2 : 1;
                continue;
            }
            i += 2;
            continue;
        }
        if (ch == '\r') { x = 0; ++i; continue; }
        if (ch == '\n') { y = std::min(39, y + 1); ++i; continue; }
        if (ch < 32 || ch == 127) { ++i; continue; }
        const std::size_t length = ch < 0x80 ? 1 : ch < 0xe0 ? 2 : ch < 0xf0 ? 3 : 4;
        const auto glyph = bytes.substr(i, length);
        // Most fixture content is ASCII; the wide-glyph case and FTXUI chrome
        // require CJK/emoji widths to retain physical mouse coordinates.
        unsigned codepoint = ch & (length == 1 ? 0x7f : length == 2 ? 0x1f : length == 3 ? 0x0f : 0x07);
        for (std::size_t n = 1; n < length && i + n < bytes.size(); ++n)
            codepoint = (codepoint << 6) | (static_cast<unsigned char>(bytes[i + n]) & 0x3f);
        const int width = (codepoint >= 0x1100 && codepoint <= 0x115f) ||
            (codepoint >= 0x2e80 && codepoint <= 0xa4cf) ||
            (codepoint >= 0xac00 && codepoint <= 0xd7a3) ||
            (codepoint >= 0xf900 && codepoint <= 0xfaff) || codepoint >= 0x1f300 ? 2 : 1;
        if (x < 120) frame.cells[y][x] = {glyph, background};
        if (width == 2 && x + 1 < 120) frame.cells[y][x + 1] = {"", background};
        x += width;
        i += length;
    }
    return frame;
}

std::string mouse_report(int button, std::pair<int, int> position, bool release = false) {
    return "\x1b[<" + std::to_string(button) + ';' + std::to_string(position.first + 1) + ';' +
        std::to_string(position.second + 1) + (release ? "m" : "M");
}

// RFC 0014: exercise real FTXUI hit-testing and stage lifecycle rather than
// rendering synthetic VisibleRows. Model and tool completion are separate gates.
void run_compressed_chain_case(const std::string& binary, const fs::path& root) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":true,"permissionMode":"default","extendedThinking":false})";
    const auto release = root / "release.txt";
    ModelServer server(false, false, release, false, "compressed-chain-live");
    Terminal terminal(binary, root, server.url(), ModelOptions{});
    server.start();
    require(terminal.wait([&] { return terminal.installed(); }), "terminal installation not observed");
    auto save_frame = [&](const std::string& name) {
        std::ofstream(root / (name + ".raw"), std::ios::binary) << terminal.latest_frame();
        const auto frame = decode_frame(terminal.latest_frame());
        std::ofstream text(root / (name + ".txt"));
        for (int y = 0; y < 40; ++y) text << frame.row(y) << '\n';
    };
    auto elapsed_seconds = [&](const std::string& status) -> int {
        const auto frame = decode_frame(terminal.latest_frame());
        for (int y = 0; y < 40; ++y) {
            const auto row = frame.row(y);
            const auto label = row.find(status + " for ");
            if (label == std::string::npos) continue;
            auto position = label + status.size() + 5;
            int total = 0;
            while (position < row.size()) {
                while (position < row.size() && row[position] == ' ') ++position;
                if (position == row.size() || !std::isdigit(static_cast<unsigned char>(row[position]))) break;
                int value = 0;
                while (position < row.size() && std::isdigit(static_cast<unsigned char>(row[position])))
                    value = value * 10 + row[position++] - '0';
                if (position == row.size()) return -1;
                const auto unit = row[position++];
                if (unit == 'h') total += value * 3600;
                else if (unit == 'm') total += value * 60;
                else if (unit == 's') { total += value; return total; }
                else return -1;
            }
            return total;
        }
        return -1;
    };
    auto click_label = [&](const std::string& label) {
        const auto point = decode_frame(terminal.latest_frame()).find(label);
        terminal.input(mouse_report(0, point) + mouse_report(0, point, true));
    };
    terminal.input("Run the compressed chain fixture\r");
    require(terminal.wait([&] {
        const auto frame = terminal.latest_frame_text();
        return frame.find("[y] Allow") != std::string::npos && frame.find("Working") != std::string::npos &&
            frame.find("Preparing") != std::string::npos && frame.find("Show details") != std::string::npos;
    }), "pending chain and authoritative permission prompt were not both visible");
    save_frame("pending");
    terminal.input("y");
    require(terminal.wait([&] {
        const auto frame = terminal.latest_frame_text();
        return frame.find("Running") != std::string::npos && frame.find("PTY-ACTIVITY") != std::string::npos &&
            frame.find("[y] Allow") == std::string::npos;
    }), "running tool's actual command was absent from collapsed activity line");
    save_frame("running");
    const auto initial_elapsed = std::max(0, elapsed_seconds("Working"));
    require(terminal.wait([&] { return elapsed_seconds("Working") >= initial_elapsed + 2; }),
            "live stage elapsed did not advance in the real terminal");
    const auto live_elapsed = elapsed_seconds("Working");
    save_frame("ticking");

    // Click the activity body, not the disclosure action: the entire two-line
    // synthetic row must share the expansion hit target.
    click_label("Running");
    require(terminal.wait([&] {
        return terminal.latest_frame_text().find("Hide details") != std::string::npos;
    }), "clicking the activity line did not expand the chain");
    save_frame("expanded");
    for (const auto width : {60, 120}) {
        const auto before = terminal.output.rfind('\0');
        terminal.resize(static_cast<unsigned short>(width), 40);
        require(terminal.wait([&] {
            const auto frame = terminal.latest_frame_text();
            return terminal.output.rfind('\0') > before && frame.find("Hide details") != std::string::npos &&
                frame.find("Running") != std::string::npos && frame.find("PTY-ACTIVITY") != std::string::npos;
        }), "resize lost the live activity or explicit expansion state");
        save_frame("expanded-" + std::to_string(width));
    }
    std::ofstream(release) << "release fixture command\n";
    require(terminal.wait([&] {
        const auto frame = terminal.latest_frame_text();
        return server.parked.load() == 1 && frame.find("Waiting for model") != std::string::npos &&
            frame.find("Hide details") != std::string::npos;
    }), "finished tool did not leave an expanded live stage waiting for the model");
    server.selection_grow = true;
    require(terminal.wait([&] {
        const auto frame = terminal.latest_frame_text();
        return frame.find("UPDATED") != std::string::npos && frame.find("Hide details") != std::string::npos;
    }), "SSE thinking update lost explicit expansion state");
    save_frame("waiting-updated");
    server.chain_finish = true;
    require(terminal.wait([&] {
        return terminal.idle_response_visible("PTY chain complete") && elapsed_seconds("Worked") >= live_elapsed;
    }), "completion decreased elapsed duration or never reached the idle final response");
    require(terminal.latest_frame_text().find("Waiting for model") == std::string::npos &&
            terminal.latest_frame_text().find("Running") == std::string::npos,
            "completed chain retained the current-activity line");
    require(terminal.latest_frame_text().find("Hide details") != std::string::npos,
            "completion reset explicit chain expansion");
    const auto completed_elapsed = elapsed_seconds("Worked");
    save_frame("completed-expanded");
    click_label("Hide details");
    require(terminal.wait([&] {
        return terminal.latest_frame_text().find("Show details") != std::string::npos &&
            elapsed_seconds("Worked") == completed_elapsed;
    }), "collapsed completion changed duration or disclosure interaction");
    save_frame("completed-collapsed");
    // A fresh resize after at least a full display second forces projection and
    // rendering again; a wall-clock recomputation would incorrectly grow here.
    const auto frozen_until = Clock::now() + 1500ms;
    require(terminal.wait([&] { return Clock::now() >= frozen_until; }), "terminal exited during freeze probe");
    const auto before = terminal.output.rfind('\0');
    terminal.resize(60, 40);
    require(terminal.wait([&] {
        return terminal.output.rfind('\0') > before && elapsed_seconds("Worked") == completed_elapsed;
    }), "completed elapsed did not remain frozen after a later terminal redraw");
    save_frame("completed-frozen-60");
    std::ofstream(root / "timing.txt") << "initial=" << initial_elapsed << "s\nlive=" << live_elapsed
        << "s\ncompleted=" << completed_elapsed << "s\nfrozen=" << elapsed_seconds("Worked") << "s\n";
    terminal.input("\x04");
    terminal.verify_exit(0);
}

void install_clipboard_capture(const fs::path& root) {
    fs::create_directories(root / "clipboard-bin");
    for (const auto& name : {"pbcopy", "wl-copy", "xclip"}) {
        const auto path = root / "clipboard-bin" / name;
        std::ofstream(path) << "#!/bin/sh\n/bin/cat > \"${PTY_CLIPBOARD_FILE}.pending\"\n"
            "/bin/mv \"${PTY_CLIPBOARD_FILE}.pending\" \"${PTY_CLIPBOARD_FILE}\"\n";
        fs::permissions(path, fs::perms::owner_all);
    }
}

std::string selection_label(char prefix, int number) {
    const auto digits = std::to_string(number);
    return std::string(1, prefix) + std::string(3 - digits.size(), '0') + digits;
}

void run_selection_case(const std::string& binary, const std::string& scenario, const fs::path& root) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":true,"permissionMode":"default","extendedThinking":false})";
    install_clipboard_capture(root);
    const bool streaming = scenario.starts_with("selection-streaming");
    const bool transcript = scenario == "selection-streaming-transcript";
    const bool wide = scenario == "selection-wide";
    const bool release_only = scenario == "selection-release-only";
    const bool basic = scenario == "selection-basic" || streaming || wide || release_only;
    const bool code = scenario.find("code") != std::string::npos;
    const bool reverse = scenario.ends_with("-up");
    const bool batched = scenario == "selection-wrap-batched";
    const char prefix = code ? 'C' : 'P';
    const int count = code ? 100 : 1000;
    std::string answer = basic ? "PTYSELECT start alpha beta gamma 中文选择 甲乙丙丁 end" : "";
    if (!basic) {
        if (code) answer += "```text\n";
        for (int n = 0; n < count; ++n) {
            answer += selection_label(prefix, n);
            answer += code ? " fixture code line\n" : " ";
        }
        if (code) answer += "```";
    }
    std::ofstream(root / "answer.txt") << answer;
    ModelServer server(false, false, root / "unused.txt", false, streaming ? "selection-streaming" : "", answer);
    ModelOptions options;
    options.capture_clipboard = true;
    Terminal terminal(binary, root, server.url(), options);
    server.start();
    require(terminal.wait([&] { return terminal.installed(); }), "terminal installation not observed");
    terminal.input("Return the selection fixture\r");
    const auto tail = basic ? answer : selection_label(prefix, count - 1);
    require(terminal.wait([&] {
        return streaming ? server.parked.load() == 1 && terminal.latest_frame_text().find(tail) != std::string::npos
                         : terminal.idle_response_visible(tail);
    }), "selection response not rendered");
    if (transcript) {
        terminal.prepare_transcript_shortcut();
        // The user-message background disappears in transcript mode. Its real
        // ANSI cells prove Ctrl+O reached the handler rather than a timer redraw.
        const auto before_mode = decode_frame(terminal.latest_frame());
        const auto user_position = before_mode.find("Return the selection fixture");
        const auto user_background = before_mode.cells[user_position.second][user_position.first].background;
        terminal.input("\x0f");
        require(terminal.wait([&] {
            const auto candidate = decode_frame(terminal.latest_frame());
            const auto user = candidate.find("Return the selection fixture");
            return candidate.cells[user.second][user.first].background != user_background &&
                terminal.latest_frame_text().find(tail) != std::string::npos;
        }), "real Ctrl+O did not change the user-message transcript background");
        // Ctrl+E lifts the transcript's message cap after mode entry is proved.
        const auto before = terminal.output.rfind('\0');
        terminal.input("\x05");
        require(terminal.wait([&] {
            return terminal.output.rfind('\0') > before &&
                terminal.latest_frame_text().find(tail) != std::string::npos;
        }), "real Ctrl+E did not redraw the paused transcript response");
    }

    auto baseline = decode_frame(terminal.latest_frame());
    std::ofstream(root / "baseline.txt") << terminal.latest_frame_text();
    if (basic) {
        const auto start = baseline.find(wide ? "甲乙丙丁" : "alpha");
        terminal.input(mouse_report(0, start) + mouse_report(0, start, true));
        const auto click_output = terminal.output.size();
        terminal.input(mouse_report(35, {start.first + 1, start.second}));
        require(terminal.wait([&] { return terminal.output.size() > click_output; }), "same-cell click did not redraw");
        require(!fs::exists(root / "clipboard.txt"), "same-cell click unexpectedly copied text");
        const auto finish = std::pair{start.first + (wide ? 3 : 4), start.second};
        terminal.input(mouse_report(0, start));
        if (!release_only) {
            terminal.input(mouse_report(32, finish));
            require(terminal.wait([&] {
                const auto frame = decode_frame(terminal.latest_frame());
                return frame.cells[start.second][start.first].background != baseline.cells[start.second][start.first].background;
            }), "real SGR drag produced no selected-cell background highlight");
            const auto frame = decode_frame(terminal.latest_frame());
            const auto bg = frame.cells[start.second][start.first].background;
            for (int x = start.first; x <= finish.first; ++x)
                require(frame.cells[start.second][x].background == bg, "drag highlight skipped a selected physical cell");
            for (int x : {start.first - 1, finish.first + 1})
                require(frame.cells[start.second][x].background == baseline.cells[start.second][x].background,
                        "drag highlight included an unselected adjacent cell");
        }
        terminal.input(mouse_report(0, finish, true));
        require(terminal.wait([&] { return fs::exists(root / "clipboard.txt"); }), "mouse release produced no native clipboard payload");
        require(read_file(root / "clipboard.txt") == (wide ? "甲乙" : "alpha"), "single-line drag copied wrong bytes");
        require(terminal.wait([&] { return terminal.latest_frame_text().find("chars to clipboard") != std::string::npos; }),
                "native backend success feedback not rendered");
        std::ofstream(root / "selected.raw", std::ios::binary) << terminal.latest_frame();
        std::ofstream(root / "copied.txt") << read_file(root / "clipboard.txt");
        std::ofstream(root / "expected.txt") << (wide ? "甲乙" : "alpha");
        if (streaming) {
            fs::remove(root / "clipboard.txt");
            terminal.input(mouse_report(0, start) + mouse_report(32, finish));
            require(terminal.wait([&] {
                const auto frame = decode_frame(terminal.latest_frame());
                return frame.cells[start.second][start.first].background != baseline.cells[start.second][start.first].background;
            }), "second paused-stream drag did not highlight");
            server.selection_grow = true;
            require(terminal.wait([&] {
                const auto frame = decode_frame(terminal.latest_frame());
                return terminal.latest_frame_text().find("UPDATED") != std::string::npos &&
                    frame.cells[start.second][start.first].background == baseline.cells[start.second][start.first].background;
            }), "growing streaming content did not cancel the stale highlighted selection");
            std::ofstream(root / "growth.raw", std::ios::binary) << terminal.latest_frame();
            terminal.input(mouse_report(0, finish, true));
            const auto settled = Clock::now() + 250ms;
            require(terminal.wait([&] { return Clock::now() >= settled; }), "terminal closed while checking stale release");
            require(!fs::exists(root / "clipboard.txt"), "stale selection copied after streaming text changed");
        }
    } else {
        // Collect each physical fixture row before selection. This independent
        // oracle checks the exact rendered payload, including wrap indentation.
        std::map<int, std::string> rendered_rows;
        auto collect = [&] {
            baseline = decode_frame(terminal.latest_frame());
            for (int y = 0; y < 40; ++y) {
                const auto row = baseline.row(y);
                const auto label = row.find(prefix);
                if (label == std::string::npos || label + 4 > row.size() ||
                    !std::isdigit(static_cast<unsigned char>(row[label + 1])) ||
                    !std::isdigit(static_cast<unsigned char>(row[label + 2])) ||
                    !std::isdigit(static_cast<unsigned char>(row[label + 3]))) continue;
                rendered_rows[std::stoi(row.substr(label + 1, 3))] = row;
            }
        };
        auto visible = [&](const std::string& label) {
            return terminal.latest_frame_text().find(label) != std::string::npos;
        };
        auto wheel = [&](bool up, std::pair<int, int> position = {110, 15}, int reports = 1) {
            const auto before = terminal.output.rfind('\0');
            std::string input;
            for (int n = 0; n < reports; ++n) input += mouse_report(up ? 64 : 65, position);
            terminal.input(input);
            require(terminal.wait([&] { return terminal.output.rfind('\0') > before; }),
                    "wheel step did not produce a complete terminal frame");
        };
        collect();
        for (int steps = 0; !visible(selection_label(prefix, 0)); ++steps) {
            require(steps < 100, "could not scroll fixture start into view");
            wheel(true);
            collect();
        }
        const auto head = selection_label(prefix, 0);
        std::string expected;
        for (const auto& [number, row] : rendered_rows) {
            auto text = row;
            if (number == 0) text.erase(0, text.find(head));
            const auto last = text.find(tail);
            if (last != std::string::npos)
                text.resize(last + tail.size() + (code ? std::string_view(" fixture code line").size() : 0));
            if (!expected.empty()) expected += '\n';
            expected += text;
        }
        // Ensure the baseline oracle itself has every original token in order.
        std::size_t token_position = 0;
        for (int n = 0; n < count; ++n) {
            token_position = expected.find(selection_label(prefix, n), token_position);
            require(token_position != std::string::npos, "baseline oracle skipped fixture token " + std::to_string(n));
            token_position += 4;
        }
        std::ofstream(root / "expected.txt") << expected;
        if (reverse) {
            for (int steps = 0; !visible(tail); ++steps) {
                require(steps < 100, "could not return fixture end into view");
                wheel(false);
            }
        }
        baseline = decode_frame(terminal.latest_frame());
        auto start = baseline.find(reverse ? tail : head);
        if (reverse) start.first += code ? 21 : 3;
        terminal.input(mouse_report(0, start));
        const auto initial_finish = std::pair{start.first + (reverse ? -1 : 1), start.second};
        terminal.input(mouse_report(32, initial_finish));
        std::string selection_background;
        require(terminal.wait([&] {
            const auto frame = decode_frame(terminal.latest_frame());
            selection_background = frame.cells[start.second][start.first].background;
            return selection_background != baseline.cells[start.second][start.first].background;
        }), "wrapped/code same-screen drag produced no ANSI highlight");
        const auto target = reverse ? head : tail;
        int steps = 0;
        for (; !visible(target); ++steps) {
            require(steps < 100, "selected wheel did not reach the opposite endpoint");
            wheel(reverse, {reverse ? 0 : 119, reverse ? 3 : 32}, batched ? 4 : 1);
            auto frame = decode_frame(terminal.latest_frame());
            int first_row = -1, last_row = -1;
            for (int y = 0; y < 40; ++y) {
                const auto row = frame.row(y);
                const auto marker = row.find(prefix);
                if (marker != std::string::npos && marker + 4 <= row.size() &&
                    std::isdigit(static_cast<unsigned char>(row[marker + 1])) &&
                    std::isdigit(static_cast<unsigned char>(row[marker + 2])) &&
                    std::isdigit(static_cast<unsigned char>(row[marker + 3]))) {
                    if (first_row == -1) first_row = y;
                    last_row = y;
                }
            }
            require(first_row >= 0 && last_row >= 0, "scrolled frame has no fixture rows");
            const auto endpoint = std::pair{reverse ? 0 : 119, reverse ? first_row : last_row};
            const auto before = terminal.output.rfind('\0');
            terminal.input(mouse_report(32, endpoint));
            require(terminal.wait([&] {
                if (terminal.output.rfind('\0') <= before) return false;
                const auto candidate = decode_frame(terminal.latest_frame());
                const auto row = candidate.row(endpoint.second);
                const auto marker = row.find(prefix);
                return marker < 120 && candidate.cells[endpoint.second][marker].background == selection_background;
            }), "drag motion after wheel did not highlight its current endpoint");
            frame = decode_frame(terminal.latest_frame());
            for (int y = first_row; y <= last_row; ++y) {
                const auto row = frame.row(y);
                const auto marker = row.find(prefix);
                const bool anchor_visible = row.find(reverse ? tail : head) != std::string::npos;
                const int begin = !reverse && anchor_visible ? start.first : 0;
                const int end = reverse && anchor_visible ? start.first : static_cast<int>(row.size()) - 1;
                for (int x = begin; x <= end && x < 120; ++x)
                    require(frame.cells[y][x].background == selection_background,
                            "scrolled selection lost or misplaced ANSI highlight at " +
                            std::to_string(x) + "," + std::to_string(y) + " (row " +
                            (marker == std::string::npos ? "blank" : row.substr(marker, 4)) + ")");
            }
        }
        require(steps > 0 && !visible(reverse ? tail : head),
                "long selection never moved its start offscreen");
        auto finish = decode_frame(terminal.latest_frame()).find(target);
        if (!reverse) finish.first += code ? 21 : 3;
        terminal.input(mouse_report(0, finish, true));
        require(terminal.wait([&] { return fs::exists(root / "clipboard.txt"); }), "long selection produced no native clipboard payload");
        require(read_file(root / "clipboard.txt") == expected, "long selection clipboard bytes differ from complete rendered baseline");
        require(terminal.wait([&] { return terminal.latest_frame_text().find("chars to clipboard") != std::string::npos; }),
                "long selection native success feedback not rendered");
        std::ofstream(root / "selected.raw", std::ios::binary) << terminal.latest_frame();
        std::ofstream(root / "copied.txt") << read_file(root / "clipboard.txt");
    }
    terminal.input("\x04");
    terminal.verify_exit(0);
}

void run_escape_case(const std::string& binary, const std::string& scenario, const fs::path& root) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":true,"permissionMode":"default","extendedThinking":false})";
    const bool tls = scenario == "escape-tls";
    if (tls)
        require(::getenv("LOOM_PTY_TLS_CA_CERT") && ::getenv("LOOM_PTY_TLS_CA_KEY"),
                "TLS fixture certificate paths unavailable");
    const bool streaming = scenario.starts_with("escape-stream") || tls;
    const bool tool = scenario.starts_with("escape-tool");
    const bool selection = scenario.starts_with("escape-selection");
    const auto answer = std::string("PTY ESC alpha beta gamma response");
    ModelServer server(false, false, root / "unused.txt", false,
        streaming ? "selection-streaming" : "", tool ? "" : answer, tls);
    ModelOptions options;
    options.capture_clipboard = selection;
    options.tls = tls;
    if (selection) install_clipboard_capture(root);
    Terminal terminal(binary, root, server.url(), options);
    server.start();
    require(terminal.wait([&] { return terminal.installed() && !terminal.latest_frame().empty(); }),
            "initial complete terminal frame unavailable");
    std::pair<int, int> anchor{};
    std::string before_background;
    if (!scenario.starts_with("escape-idle")) {
        terminal.input("Return the Esc probe\r");
        if (tool) {
            require(terminal.wait([&] { return terminal.permission_visible(); }), "Agent permission unavailable");
            terminal.input("y");
            require(terminal.wait([&] {
                return server.parked.load() == 1 &&
                    compact_json(read_file(root / "runtime" / "pty-agent.json")).find("\"status\":\"running\"") != std::string::npos;
            }), "actual Agent execution did not start");
        } else {
            require(terminal.wait([&] {
                return streaming ? server.parked.load() == 1 && terminal.latest_frame_text().find(answer) != std::string::npos
                                 : terminal.idle_response_visible(answer);
            }), "Esc probe response unavailable");
        }
        if (selection) {
            const auto frame = decode_frame(terminal.latest_frame());
            anchor = frame.find("alpha");
            before_background = frame.cells[anchor.second][anchor.first].background;
            terminal.input(mouse_report(0, anchor) + mouse_report(32, {anchor.first + 4, anchor.second}));
            require(terminal.wait([&] {
                return decode_frame(terminal.latest_frame()).cells[anchor.second][anchor.first].background != before_background;
            }), "active drag highlight unavailable before Esc");
        }
    }
    std::ofstream(root / "before-escape.raw", std::ios::binary) << terminal.latest_frame();
    const auto before = terminal.output.size();
    const auto requests_before_escape = server.requests.load();
    auto input = std::string("\x1b");
    if (scenario.ends_with("-urxvt")) input += "\x1b[39;74;83M";
    if (scenario.ends_with("-sgr")) input += "\x1b[<35;74;23M";
    terminal.input(input);
    const auto settle = Clock::now() + 700ms;
    terminal.wait([&] { return Clock::now() >= settle; }, 2s);
    const bool alive = terminal.alive();
    const auto suffix = terminal.output.substr(before);
    std::ofstream(root / "escape-result.txt") << "after single Esc: " << terminal.process_state()
        << "\nalternate-screen teardown=" << (suffix.find("\x1b[?1049l") != std::string::npos)
        << "\nmouse1003 teardown=" << (suffix.find("\x1b[?1003l") != std::string::npos)
        << "\nmouse1006 teardown=" << (suffix.find("\x1b[?1006l") != std::string::npos) << '\n';
    std::cout << scenario << " after single Esc: " << terminal.process_state()
              << " alt teardown=" << (suffix.find("\x1b[?1049l") != std::string::npos)
              << " mouse teardown=" << (suffix.find("\x1b[?1003l") != std::string::npos) << '\n';
    require(alive, "single Esc unexpectedly ended Loom: " + terminal.process_state());
    require(suffix.find("\x1b[?1049l") == std::string::npos &&
            suffix.find("\x1b[?1003l") == std::string::npos &&
            suffix.find("\x1b[?1006l") == std::string::npos,
            "Esc uninstalled terminal modes while the app should remain interactive");
    if (streaming || tool) {
        require(terminal.wait([&] {
            return server.closed.load() == server.parked.load() && terminal.idle_response_visible("");
        }), "single Esc did not cancel the active request and return idle: " + terminal.process_state());
        require(server.requests.load() == requests_before_escape,
                "cancellation issued another model POST without a new prompt");
    }

    if (selection) {
        require(decode_frame(terminal.latest_frame()).cells[anchor.second][anchor.first].background == before_background,
                "Esc did not clear active drag highlight");
        terminal.input(mouse_report(0, {anchor.first + 4, anchor.second}, true));
        require(!fs::exists(root / "clipboard.txt"), "Esc-cancelled drag copied text");
    }
    std::ofstream(root / "after-escape.raw", std::ios::binary) << terminal.latest_frame();
    const auto requests = server.requests.load();
    terminal.input("Continue after one Esc\r");
    require(terminal.wait([&] { return server.requests.load() > requests; }), "Loom did not accept new input after Esc");
    const auto bodies = server.request_bodies();
    const auto messages = request_messages(bodies.back());
    require(messages.find("\"content\":\"Continue after one Esc\"") != std::string::npos ||
            messages.find("\"text\":\"Continue after one Esc\"") != std::string::npos,
            "mouse report bytes contaminated the follow-up user prompt");
    terminal.input("\x04");
    terminal.verify_exit(0);
    std::cout << "final deliberate Ctrl+D: " << terminal.process_state() << '\n';
}

void run_handoff_case(const std::string& binary, const std::string& scenario, const fs::path& root, const std::string& harness) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":true,"permissionMode":"default","extendedThinking":false})";
    ModelServer server(false, false, root / "unused.txt", false, {}, "PTY shell response");
    ModelOptions options;
    options.shell_handoff = true;
    options.handoff_helper = harness;
    Terminal terminal(binary, root, server.url(), options);
    server.start();
    require(terminal.wait([&] {
        return terminal.installed() && !terminal.latest_frame().empty() && fs::exists(root / "loom.pid");
    }), "Loom shell child did not install its terminal");
    require(terminal.mode_enabled(1003) && terminal.mode_enabled(1006), "terminal emulator mouse tracking not enabled");
    // Freeze the consumer while tracking remains enabled. These reports are
    // queued before exit, not fabricated after the emulator receives reset.
    terminal.signal(SIGSTOP);
    require(terminal.wait([&] { return fs::exists(root / "loom.stopped"); }),
            "Loom did not stop before queuing terminal reports");
    require(terminal.mode_enabled(1003) && terminal.mode_enabled(1006),
            "emulator received mouse reset before the queued input");
    std::string pending;
    if (scenario == "handoff-ctrl-d") pending += '\x04';
    if (scenario == "handoff-exit") pending += "/exit\r";
    for (int n = 0; n < 20; ++n) pending += "\x1b[<35;74;23M";
    terminal.input(pending);
    if (scenario == "handoff-term") terminal.signal(SIGTERM);
    if (scenario == "handoff-hup") terminal.signal(SIGHUP);
    terminal.signal(SIGCONT);
    require(terminal.wait([&] { return terminal.output.find("PTY-SHELL-READY") != std::string::npos; }), "zsh did not regain the terminal");
    for (const int mode : {1000, 1002, 1003, 1006, 1015})
        require(!terminal.mode_enabled(mode), "mouse tracking remained enabled at shell handoff");
    terminal.input("shell-handoff-ok\r");
    require(terminal.wait([&] { return fs::exists(root / "shell-input.txt"); }), "real zsh did not read the follow-up line");
    const auto received = read_file(root / "shell-input.txt");
    std::cout << "shell received " << received.size() << " bytes, loom status " << read_file(root / "loom.status") << '\n';
    std::ofstream(root / "handoff-result.txt")
        << "reports queued before mouse reset=20\nactual Loom exit=" << read_file(root / "loom.actual-status")
        << "\nzsh observed Loom exit=" << read_file(root / "loom.status")
        << "\nshell input bytes=" << received.size() << "\nmouse modes disabled=1000,1002,1003,1006,1015\n";
    require(read_file(root / "loom.actual-status") == read_file(root / "loom.status"),
            "zsh observed a different Loom termination status");
    require(read_file(root / "loom.status") == (scenario == "handoff-term" || scenario == "handoff-hup" ? "1" : "0"), "unexpected actual Loom exit status");
    require(received == "shell-handoff-ok", "queued mouse reports leaked into the real zsh consumer (" + std::to_string(received.size()) + " bytes)");
    terminal.verify_exit(0);
}

void run_case(const std::string& binary, const std::string& scenario, const fs::path& root,
              const std::string& harness) {
    if (scenario.starts_with("escape-")) {
        run_escape_case(binary, scenario, root);
        return;
    }
    if (scenario.starts_with("handoff-")) {
        run_handoff_case(binary, scenario, root, harness);
        return;
    }
    if (scenario == "compressed-chain-live") {
        run_compressed_chain_case(binary, root);
        return;
    }
    if (scenario.starts_with("selection-")) {
        run_selection_case(binary, scenario, root);
        return;
    }
    if (scenario == "prompt-wrap") {
        run_prompt_wrap_case(binary, root);
        return;
    }
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
    if (argc >= 3 && std::string_view(argv[1]) == "--shell-loom-child") {
        const fs::path directory(::getenv("PTY_HANDOFF_DIR"));
        const pid_t child = ::fork();
        if (child < 0) return 126;
        if (child == 0) {
            ::signal(SIGPIPE, SIG_DFL);
            ::execv(argv[2], argv + 2);
            ::_exit(127);
        }
        std::ofstream(directory / "loom.pid") << child;
        int status = 0;
        for (;;) {
            if (::waitpid(child, &status, WUNTRACED) < 0) {
                if (errno == EINTR) continue;
                return 126;
            }
            if (WIFSTOPPED(status)) {
                std::ofstream(directory / "loom.stopped") << WSTOPSIG(status);
                continue;
            }
            break;
        }
        const int code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        std::ofstream(directory / "loom.actual-status") << code;
        return code;
    }
    ::signal(SIGPIPE, SIG_IGN);
    if (argc != 2) return 2;
    const char* binary = ::getenv("LOOM_BINARY");
    if (!binary || ::access(binary, X_OK) != 0) return 125;
    // Bash's destructive-command guard also examines path arguments; a
    // recovery path containing "shutdown" would reject even fixture reads.
    const std::string scenario(argv[1]);
    std::string pattern = scenario == "bash-failure" || scenario == "permission-denial" || scenario.starts_with("api-retry") || scenario.starts_with("round-") || scenario.starts_with("compressed-chain-")
        ? "/tmp/loom_pty_recovery_XXXXXX" : "/tmp/loom_pty_shutdown_XXXXXX";
    const char* directory = ::mkdtemp(pattern.data());
    if (!directory) return 1;
    const fs::path root(directory);
    if (scenario.starts_with("handoff-") && ::access("/bin/zsh", X_OK) != 0 && ::access("/usr/bin/zsh", X_OK) != 0) {
        std::error_code ignored;
        fs::remove_all(root, ignored);
        return 125;
    }
    int result = 0;
    try {
        fs::create_directories(root / "config");
        run_case(binary, argv[1], root, fs::absolute(argv[0]).string());
        std::cout << "PASS " << argv[1] << '\n';
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << argv[1] << ": " << error.what() << '\n';
        result = 1;
    }
    std::error_code ignored;
    if (result == 0) {
        if (const char* evidence = ::getenv("LOOM_PTY_EVIDENCE_DIR"); evidence && (scenario.starts_with("selection-") || scenario.starts_with("escape-") || scenario.starts_with("handoff-") || scenario.starts_with("compressed-chain-"))) {
            const auto destination = fs::path(evidence) / scenario;
            fs::create_directories(destination);
            for (const auto& entry : fs::directory_iterator(root)) {
                if (entry.path().extension() == ".raw" || entry.path().extension() == ".txt")
                    fs::copy_file(entry.path(), destination / entry.path().filename(), fs::copy_options::overwrite_existing);
            }
        }
        fs::remove_all(root, ignored);
    }
    else std::cerr << "Diagnostic directory: " << root << '\n';
    return result;
}
