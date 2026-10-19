/// Real terminal lifecycle, tool recovery, and optional round-budget workflows.
/// The SSE fixture gates interactions on requests and complete visible frames.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <condition_variable>
#include <cmath>
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
#include <optional>
#include <regex>
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

std::string occupancy_response(const std::string& text, int input_tokens, int output_tokens) {
    auto body = text_response(text);
    const auto start = body.find("\"content\":[]");
    require(start != std::string::npos, "occupancy fixture has no message start");
    body.insert(start, "\"usage\":{\"input_tokens\":" + std::to_string(input_tokens) + "},");
    const auto output = body.find("\"output_tokens\":5");
    require(output != std::string::npos, "occupancy fixture has no output usage");
    body.replace(output, std::string("\"output_tokens\":5").size(),
                 "\"output_tokens\":" + std::to_string(output_tokens));
    return body;
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
    std::atomic<int> metadata_requests{0};
    std::atomic<bool> retry_fault{true};
    std::atomic<bool> selection_grow{false};
    std::atomic<bool> chain_finish{false};
    std::atomic<bool> chain_tools{false};
    std::atomic<int> occupancy_business{0};
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
            // Metadata GETs never consume the business POST fixture sequence.
            if (request.starts_with("GET ")) {
                ++metadata_requests;
                const bool available = recovery_ == "model-metadata" &&
                    request.starts_with("GET /v1/models ");
                const std::string body = available ?
                    R"({"data":[{"id":"metadata-large","context_length":1048576},{"id":"metadata-small","context_length":262144,"max_output_tokens":4096}]})" :
                    R"({"error":"metadata unavailable"})";
                send_bytes(std::string("HTTP/1.1 ") + (available ? "200 OK" : "404 Not Found") +
                    "\r\nContent-Type: application/json\r\nContent-Length: " +
                    std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                ::shutdown(fd, SHUT_WR);
                return;
            }
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
        if (recovery_.starts_with("context-occupancy")) {
            const bool summary = request.find("CONTEXT CHECKPOINT") != std::string::npos;
            std::string body;
            if (summary) {
                const std::string checkpoint =
                    "## Goal\nFinish occupancy validation.\n## Constraints\n"
                    "Late constraint: preserve OCCUPANCY_KEEP_47; do not commit.\n"
                    "## Decisions\nUse the completed read.\n## Progress\nCompleted result=47.\n"
                    "## Next Steps\nReturn the answer.\n## References\noccupancy-history.txt\n";
                body = "{\"id\":\"occupancy-summary\",\"type\":\"message\",\"role\":\"assistant\","
                    "\"content\":[{\"type\":\"text\",\"text\":\"" + escape_json(checkpoint) +
                    "\"}],\"stop_reason\":\"end_turn\",\"usage\":{\"input_tokens\":1000,\"output_tokens\":100}}";
            } else {
                const int turn = occupancy_business.fetch_add(1);
                const int input = recovery_ == "context-occupancy-repeated" ? 100000 : turn == 0 ? 350000 : 8000;
                body = occupancy_response("PTY occupancy reply " + std::to_string(turn), input, 64);
            }
            send_bytes("HTTP/1.1 200 OK\r\nContent-Type: " +
                std::string(summary ? "application/json" : "text/event-stream") +
                "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        if (recovery_ == "model-context" || recovery_ == "model-metadata") {
            const auto body = text_response("PTY model context reply " + std::to_string(index));
            send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " +
                std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        if (recovery_ == "task-completion") {
            if (index == 1) {
                ++parked;
                const auto deadline = Clock::now() + 15s;
                while (!fs::exists(target_) && !stopping_ && Clock::now() < deadline)
                    std::this_thread::sleep_for(20ms);
                if (stopping_ || !fs::exists(target_)) return;
            }
            std::string body;
            if (index == 0) body = tool_response("todo_write",
                R"({"todos":[{"content":"PTY inspect source","status":"in_progress"},{"content":"PTY validate result","status":"pending"},{"content":"PTY record evidence","status":"pending"}]})", "pty-todo-active");
            else if (index == 1) body = tool_response("todo_write",
                R"({"todos":[{"content":"PTY inspect source","status":"completed"},{"content":"PTY validate result","status":"completed"},{"content":"PTY record evidence","status":"completed"}]})", "pty-todo-complete");
            else if (index == 3) body = tool_response("todo_write", R"({"todos":[]})", "pty-todo-clear");
            else body = text_response(index == 2 ? "PTY completion finished" : "PTY active tasks cleared");
            send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " +
                std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        if (recovery_ == "task-response-layout") {
            if (index == 0) {
                const auto body = tool_response("todo_write",
                    R"({"todos":[{"content":"PTY active task","status":"in_progress"},{"content":"PTY pending task","status":"pending"},{"content":"PTY completed task","status":"completed"}]})");
                send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " +
                    std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                ::shutdown(fd, SHUT_WR);
            } else {
                if (!send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n" + message_start())) return;
                ++parked;
                while (read_bytes(buffer.data(), buffer.size()) > 0) {}
                ++closed;
                closed_cv_.notify_all();
            }
            return;
        }
        if (recovery_.starts_with("generation-tps-")) {
            const auto delta = [&](const std::string& text) {
                return event("content_block_delta", "{\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"" +
                    escape_json(text) + "\"}}");
            };
            const auto prefix = message_start() + event("content_block_start",
                R"({"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}})");
            const auto finish = event("content_block_stop", R"({"type":"content_block_stop","index":0})") +
                event("message_delta", R"({"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":200}})") +
                event("message_stop", R"({"type":"message_stop"})");
            if (!send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n" + prefix)) return;
            if (index > 0) {
                for (int i = 0; i < 6; ++i) {
                    if (!send_bytes(delta("Follow-up streamed segment. "))) return;
                    std::this_thread::sleep_for(120ms);
                }
                send_bytes(delta("Follow-up complete") + finish);
                ::shutdown(fd, SHUT_WR);
                return;
            }
            ++parked;
            const auto connected = [&](int milliseconds) {
                if (stopping_) return false;
                pollfd descriptor{fd, POLLIN, 0};
                const int ready = ::poll(&descriptor, 1, milliseconds);
                if (ready < 0) return errno == EINTR;
                return ready == 0 || read_bytes(buffer.data(), buffer.size()) > 0;
            };
            const auto gate = [&](std::string_view name) {
                return fs::exists(target_.parent_path() / ("generation-" + std::string(name)));
            };
            bool live = true;
            while (live && !gate("start")) live = connected(20);
            int chunk = 0;
            while (live && !gate("pause")) {
                live = send_bytes(delta("Measured output segment " + std::to_string(chunk++) + ". ")) && connected(100);
            }
            while (live && !gate("resume")) live = connected(20);
            while (live && !gate("finish")) {
                live = send_bytes(delta("Resumed output segment " + std::to_string(chunk++) + ". ")) && connected(100);
            }
            if (live) {
                send_bytes(finish);
                ::shutdown(fd, SHUT_WR);
            }
            ++closed;
            closed_cv_.notify_all();
            return;
        }
        if (recovery_.starts_with("question-")) {
            std::string body;
            if (index == 0) {
                std::string input = R"({"question":"PTY exact question words")";
                if (recovery_ != "question-resize" && recovery_ != "question-empty-no-default")
                    input += ",\"default_answer\":\"" +
                        std::string(recovery_ == "question-unicode" ? "甲乙🙂" : "seed") + "\"";
                input += '}';
                body = tool_response("ask_user_question", input, "pty-status-question");
            } else {
                body = text_response(index == 1 ? "PTY question complete" : "PTY question follow-up complete");
            }
            send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: " +
                std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        if (recovery_.starts_with("osc7501-")) {
            if (recovery_ == "osc7501-cancel" || recovery_ == "osc7501-handoff") {
                const auto prefix = message_start() +
                    event("content_block_start", R"({"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}})") +
                    event("content_block_delta", R"({"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"PTY status streaming"}})");
                if (!send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n" + prefix)) return;
                ++parked;
                while (read_bytes(buffer.data(), buffer.size()) > 0) {}
                ++closed;
                closed_cv_.notify_all();
                return;
            }
            std::string body;
            const bool failed = recovery_ == "osc7501-error";
            if (failed) {
                body = R"({"type":"error","error":{"type":"invalid_request_error","message":"PTY terminal status failure"}})";
            } else if (index == 0 && recovery_ == "osc7501-permission") {
                body = tool_response("Write", "{\"file_path\":\"" + escape_json(target_.string()) +
                    "\",\"content\":\"PTY status write\"}", "pty-status-permission");
            } else if (index == 0 && recovery_ == "osc7501-question") {
                body = tool_response("ask_user_question", R"({"question":"PTY status question","default_answer":"PTY status answer"})", "pty-status-question");
            } else {
                body = text_response("PTY status complete");
            }
            send_bytes("HTTP/1.1 " + std::string(failed ? "400 Bad Request" : "200 OK") +
                "\r\nContent-Type: " + (failed ? "application/json" : "text/event-stream") +
                "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            ::shutdown(fd, SHUT_WR);
            return;
        }
        if (recovery_ == "compressed-chain-live") {
            if (index == 0) {
                const auto command = "printf PTY-ACTIVITY; for attempt in {1..300}; do [ -f '" + target_.string() +
                    "' ] && break; sleep 0.1; done";
                const auto prefix = message_start() +
                    event("content_block_start", R"({"type":"content_block_start","index":0,"content_block":{"type":"thinking","thinking":""}})") +
                    event("content_block_delta", R"({"type":"content_block_delta","index":0,"delta":{"type":"thinking_delta","thinking":"PTY initial planning"}})");
                if (!send_bytes("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n" + prefix)) return;
                const auto deadline = Clock::now() + 15s;
                while (!chain_tools.load() && !stopping_ && Clock::now() < deadline) {
                    pollfd descriptor{fd, POLLIN, 0};
                    if (::poll(&descriptor, 1, 20) > 0 && read_bytes(buffer.data(), buffer.size()) <= 0) return;
                }
                if (stopping_ || !chain_tools.load()) return;
                const auto input = "{\"command\":\"" + escape_json(command) + "\"}";
                const auto body = event("content_block_stop", R"({"type":"content_block_stop","index":0})") +
                    event("content_block_start", R"({"type":"content_block_start","index":1,"content_block":{"type":"tool_use","id":"pty-chain-bash","name":"Bash","input":{}}})") +
                    event("content_block_delta", "{\"type\":\"content_block_delta\",\"index\":1,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"" + escape_json(input) + "\"}}") +
                    event("content_block_stop", R"({"type":"content_block_stop","index":1})") +
                    event("message_delta", R"({"type":"message_delta","delta":{"stop_reason":"tool_use","stop_sequence":null},"usage":{"output_tokens":12}})") +
                    event("message_stop", R"({"type":"message_stop"})");
                send_bytes(body);
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
    unsigned short initial_columns = 120;
    bool direct = false;
    bool capture_clipboard = false;
    bool shell_handoff = false;
    std::string handoff_helper;
    bool tls = false;
    bool isolate_budget_environment = false;
    bool use_configured_thinking = false;
    std::string runtime_tool_input;
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
                options.runtime_tool_input.empty()
                    ? R"({"description":"Direct model probe","prompt":"Return the probe result","name":"pty-agent"})"
                    : options.runtime_tool_input});
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
        dimensions.ws_col = options.initial_columns;
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
            ::setenv("COLORTERM", "truecolor", 1);
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
            if (options.isolate_budget_environment) {
                // These cases prove persisted CLI settings, independent of the test runner.
                ::unsetenv("LOOM_MAX_TOKENS");
                ::setenv("LOOM_ENABLE_MEMORY_EXTRACTION", "0", 1);
                ::setenv("LOOM_DISABLE_AUTO_MEMORY", "1", 1);
            }
            if (options.environment_model.empty()) ::unsetenv("LOOM_MODEL");
            else ::setenv("LOOM_MODEL", options.environment_model.c_str(), 1);
            if (options.use_configured_thinking) ::unsetenv("LOOM_DISABLE_THINKING");
            else ::setenv("LOOM_DISABLE_THINKING", "1", 1);
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
    void capture_driver_state(const std::string& name) const {
        termios current{};
        std::ofstream out(root_ / (name + ".txt"));
        out << "process=" << process_state() << "\nforeground_pgid=" << ::tcgetpgrp(master_) << '\n';
        if (::tcgetattr(master_, &current) == 0)
            out << "ICANON=" << ((current.c_lflag & ICANON) != 0)
                << "\nECHO=" << ((current.c_lflag & ECHO) != 0)
                << "\nVMIN=" << static_cast<int>(current.c_cc[VMIN])
                << "\nVTIME=" << static_cast<int>(current.c_cc[VTIME]) << '\n';
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
    std::string first_resize_frame_since(std::size_t start) const {
        // FTXUI clears each previous screen row with EL when Draw changes its
        // dimensions. Scan every complete frame, so a later corrected frame
        // in the same PTY read cannot hide the first resized frame.
        for (auto end = output.find('\0', start); end != std::string::npos;
             end = output.find('\0', start)) {
            auto frame = output.substr(start, end - start);
            if (frame.find("\x1b[2K") != std::string::npos) return frame;
            start = end + 1;
        }
        return {};
    }
    std::string latest_frame_text() const {
        // Complete frames exclude historical output and partial stream data.
        return terminal_text(latest_frame());
    }
    bool idle_response_visible(const std::string& response) const {
        const auto frame = latest_frame_text();
        if (frame.find(response) == std::string::npos || frame.find(" to interrupt") != std::string::npos ||
            frame.find("Model request failed") != std::string::npos || frame.find("Retry request") != std::string::npos) return false;
        constexpr std::array<std::string_view, 18> spinner_glyphs = {
            "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏",
            "✻", "❋", "✦", "✧", "✶", "✷", "✸", "✹"
        };
        return std::none_of(spinner_glyphs.begin(), spinner_glyphs.end(), [&](auto glyph) {
            const auto prefix = std::string(glyph) + " ";
            return frame.starts_with(prefix) || frame.find("\n" + prefix) != std::string::npos;
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

void run_welcome_monogram_case(const std::string& binary, const fs::path& root) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":false,"permissionMode":"default","extendedThinking":false})";
    ModelServer server(false, false, root / "unused.txt", true);
    Terminal terminal(binary, root, server.url(), ModelOptions{});
    server.start();
    require(terminal.wait([&] {
        return terminal.latest_frame_text().find("▋██▀▀▜▉▃▅▅▜█▉") != std::string::npos;
    }), "startup wordmark did not reach a complete terminal frame");

    const auto verify_frame = [&](const std::string& raw, const std::string& name, int width) {
        const auto text = terminal_text(raw);
        const bool compact = occurrences(text, "▟█▃█▋▃▜▇") == 1 &&
                occurrences(text, "█▋ ▏▋ ▍▁ ▊") == 1 &&
                occurrences(text, "▂█▋▜█▆▊▁▆▆▁") == 1;
        const bool large = occurrences(text, "▋██▀▀▜▉▃▅▅▜█▉") == 1 &&
                occurrences(text, "▋██▀▜██▀▜█▊") == 1 &&
                occurrences(text, "▝▁██ ▃███") == 1;
        require(width >= 70 ? large && !compact : compact && !large,
                "welcome wordmark was clipped or duplicated in " + name);
        require(text.find("Loom") != std::string::npos &&
                text.find("pty-test-model") != std::string::npos,
                "welcome metadata was lost in " + name);
        require(text.find("█░█") == std::string::npos,
                "legacy weave remained in " + name);
        std::ofstream(root / (name + ".raw"), std::ios::binary) << raw;
        std::ofstream(root / (name + ".txt")) << text;
    };
    verify_frame(terminal.latest_frame(), "banner-initial-120", 120);
    for (const auto width : {80, 69, 70, 40, 120}) {
        const auto start = terminal.output.rfind('\0') + 1;
        terminal.resize(static_cast<unsigned short>(width), 24);
        require(terminal.wait([&] {
            return !terminal.first_resize_frame_since(start).empty();
        }), "welcome resize did not flush a complete frame");
        verify_frame(terminal.first_resize_frame_since(start),
                     "banner-" + std::to_string(width), width);
    }
    require(server.requests.load() == 0, "welcome rendering submitted a model request");
    terminal.input("\x04");
    terminal.verify_exit(0);
}

// RFC 0018: real API streams, silence expiry and cancellation at a fixed prompt.
void run_generation_tps_case(const std::string& binary, const std::string& scenario,
                             const fs::path& root) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":false,"permissionMode":"default","extendedThinking":false})";
    ModelServer server(false, false, root / "unused.txt", false, scenario);
    Terminal terminal(binary, root, server.url(), ModelOptions{});
    server.start();
    require(terminal.wait([&] { return terminal.installed(); }), "TPS terminal startup not observed");
    const auto has_rate = [&] {
        const auto text = terminal.latest_frame_text();
        const auto suffix = text.find(" tok/s");
        return suffix != std::string::npos && text.rfind('~', suffix) != std::string::npos;
    };
    const auto capture = [&](const std::string& name) {
        const auto raw = terminal.latest_frame();
        std::ofstream(root / (name + ".raw"), std::ios::binary) << raw;
        std::ofstream(root / (name + ".txt")) << terminal_text(raw);
    };
    const auto prompt_row = [&] {
        const auto text = terminal.latest_frame_text();
        const auto marker = text.rfind("❯");
        require(marker != std::string::npos, "TPS editor marker not visible");
        return std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(marker), '\n');
    };
    terminal.input("Measure the live output rate\r");
    require(terminal.wait([&] { return server.requests.load() >= 1 &&
        terminal.latest_frame_text().find("Thinking") != std::string::npos; }),
        "TPS model request not observed");
    require(!has_rate(), "TPS displayed before model output");
    const auto editor_row = prompt_row();
    capture("waiting");
    std::ofstream(root / "generation-start") << "go";
    require(terminal.wait(has_rate), "live estimated TPS did not appear");
    require(prompt_row() == editor_row, "TPS moved the editor");
    capture("generating-120");
    for (const auto width : {80, 40, 120}) {
        const auto start = terminal.output.rfind('\0') + 1;
        terminal.resize(static_cast<unsigned short>(width), 40);
        require(terminal.wait([&] { return !terminal.first_resize_frame_since(start).empty() && has_rate(); }),
                "TPS resize frame not observed");
        require(prompt_row() == editor_row, "TPS resize moved the editor vertically");
        capture("generating-" + std::to_string(width));
    }
    if (scenario == "generation-tps-cancel") {
        terminal.input("\x1b");
        require(terminal.wait([&] { return !has_rate() && server.closed.load() == 1; }),
                "TPS remained visible after cancellation");
        capture("cancelled");
    } else {
        std::ofstream(root / "generation-pause") << "go";
        require(terminal.wait([&] { return !has_rate(); }), "TPS did not expire during a silent stream");
        require(server.closed.load() == 0, "silence fixture ended before expiry");
        capture("expired");
        std::ofstream(root / "generation-resume") << "go";
        require(terminal.wait(has_rate), "TPS did not recover with fresh output");
        capture("resumed");
        std::ofstream(root / "generation-finish") << "go";
        require(terminal.wait([&] { return !has_rate() && server.closed.load() == 1; }),
                "TPS remained after stream completion");
        capture("completed");
    }
    require(terminal.wait([&] { return terminal.idle_response_visible(""); }),
            "generation probe did not return idle");
    terminal.input("Check the next generation rate\r");
    require(terminal.wait([&] { return server.requests.load() >= 2 && has_rate(); }),
            "TPS did not recover on a new submission");
    capture("follow-up-generating");
    require(terminal.wait([&] { return terminal.idle_response_visible("Follow-up complete") && !has_rate(); }),
            "follow-up generation did not finish without TPS");
    capture("follow-up-completed");
    terminal.input("\x04");
    terminal.verify_exit(0);
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

    const auto resize_start = terminal.output.rfind('\0') + 1;
    terminal.resize(60, 40);
    require(terminal.wait([&] {
        return !terminal.first_resize_frame_since(resize_start).empty();
    }), "terminal did not emit a complete resized prompt frame");
    const auto resized_raw = terminal.first_resize_frame_since(resize_start);
    const auto resized_frame = terminal_text(resized_raw);
    std::ofstream(root / "prompt-resize-first.raw", std::ios::binary) << resized_raw;
    std::ofstream(root / "prompt-resize-first.txt") << resized_frame;
    require(prompt_row_distance(resized_frame) > wide_rows,
            "first resized frame did not reflow the prompt while keeping both ends visible");
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

// Attribute evidence uses the same complete ANSI frames as interaction tests.
enum class ChainAttention { Working, Permission, Completed };
void save_attention_frame(const fs::path& root, const std::string& name, const std::string& raw);
void verify_normal_attention(const std::string& raw);
void verify_chain_attention(const std::string& raw, ChainAttention attention);
void verify_failure_attention(const std::string& raw, const std::string& diagnostic);
void verify_corrected_permission_attention(const std::string& raw, const std::string& diagnosis);

void run_recovery_case(const std::string& binary, const std::string& scenario, const fs::path& root) {
    const bool retry = scenario.starts_with("api-retry");
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":false,"permissionMode":"default","extendedThinking":false,"theme":"dark"})";
    const auto sentinel = root / "once.txt";
    ModelServer server(false, false, sentinel, false, scenario);
    Terminal terminal(binary, root, server.url(), ModelOptions{});
    server.start();
    require(terminal.wait([&] {
        return terminal.installed() && terminal.latest_frame_text().find("loom_pty_") != std::string::npos;
    }), "complete normal terminal frame not observed");
    save_attention_frame(root, "normal", terminal.latest_frame());
    verify_normal_attention(terminal.latest_frame());
    constexpr auto prompt = "Run the PTY recovery probe";
    terminal.input(std::string(prompt) + "\r");
    require(terminal.wait([&] {
        return terminal.latest_frame_text().find("[y] Allow  [n] Deny") != std::string::npos;
    }), "initial Bash permission UI not observed");
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
            const auto text = terminal.latest_frame_text();
            return text.find("Model request failed") != std::string::npos &&
                text.find(scenario == "api-retry-network" ? "HTTP request failed" : "PTY injected request failure") != std::string::npos &&
                text.find("Retry request") != std::string::npos && text.find("Dismiss error") != std::string::npos;
        }), "idle retryable model error and keyboard actions were not visible");
        save_attention_frame(root, "failed-request", terminal.latest_frame());
        verify_failure_attention(terminal.latest_frame(), scenario == "api-retry-network"
            ? "HTTP request failed" : "PTY injected request failure");
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
        save_attention_frame(root, "corrected-permission", terminal.latest_frame());
        verify_corrected_permission_attention(terminal.latest_frame(), scenario == "permission-denial"
            ? "Permission denied" : "No such file or directory");
        terminal.input("y");
    }

    require(terminal.wait([&] {
        return terminal.idle_response_visible("PTY recovery complete");
    }), "conversation did not recover to its final assistant response");
    save_attention_frame(root, "recovered", terminal.latest_frame());
    verify_normal_attention(terminal.latest_frame());
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
    std::string foreground;
    bool bold = false;
    bool dim = false;
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
    std::string background, foreground;
    bool bold = false, dim = false;
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
                        const auto attribute = values[n];
                        if (attribute == 0) {
                            background.clear();
                            foreground.clear();
                            bold = dim = false;
                        } else if (attribute == 1) bold = true;
                        else if (attribute == 2) dim = true;
                        else if (attribute == 22) bold = dim = false;
                        else if (attribute == 39) foreground.clear();
                        else if (attribute == 49) background.clear();
                        else if ((attribute >= 30 && attribute <= 37) ||
                                 (attribute >= 90 && attribute <= 97))
                            foreground = std::to_string(attribute);
                        else if ((attribute >= 40 && attribute <= 47) ||
                                 (attribute >= 100 && attribute <= 107))
                            background = std::to_string(attribute);
                        else if (attribute == 38 || attribute == 48) {
                            if (n + 1 >= values.size()) continue;
                            const auto mode = values[n + 1];
                            const auto count = mode == 2 ? 4U : mode == 5 ? 2U : 0U;
                            if (count && n + count < values.size()) {
                                auto color = std::to_string(attribute);
                                for (std::size_t k = 1; k <= count; ++k)
                                    color += ";" + std::to_string(values[n + k]);
                                (attribute == 48 ? background : foreground) = std::move(color);
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
        if (x < 120) frame.cells[y][x] = {glyph, background, foreground, bold, dim};
        if (width == 2 && x + 1 < 120) frame.cells[y][x + 1] = {"", background, foreground, bold, dim};
        x += width;
        i += length;
    }
    return frame;
}

// Expected colors come from the explicitly selected dark theme's existing
// tokens. Other themes are covered by native pixel tests, not this PTY fixture.
constexpr std::string_view kDarkPrimary = "38;2;224;137;105";
constexpr std::string_view kDarkText = "38;2;224;218;210";
constexpr std::string_view kDarkMuted = "38;2;184;184;184";
constexpr std::string_view kDarkBackground = "48;2;32;33;36";
constexpr std::string_view kDarkWarning = "38;2;255;193;7";
constexpr std::string_view kDarkDanger = "38;2;255;107;128";
constexpr std::array<std::string_view, 10> kCanonicalWorkGlyphs = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};

std::pair<int, int> attention_label(const TerminalFrame& frame, const std::string& label,
                                   bool last = false) {
    for (int step = 0; step < 40; ++step) {
        const int y = last ? 39 - step : step;
        for (int x = 0; x < 120; ++x) {
            std::string value;
            for (int next = x; next < 120 && value.size() < label.size(); ++next)
                value += frame.cells[y][next].text;
            if (value == label) return {x, y};
        }
    }
    throw std::runtime_error("visible attention frame lacks " + label);
}

void require_attention_cell(const TerminalCell& cell, std::string_view foreground,
                            bool bold, const std::string& label) {
    require(cell.foreground == foreground && cell.bold == bold && !cell.dim,
            label + " unexpected style: fg=" + cell.foreground + " bold=" +
            std::to_string(cell.bold) + " dim=" + std::to_string(cell.dim));
}

void require_attention_label(const TerminalFrame& frame, const std::string& label,
                             std::string_view foreground, bool bold, bool last = false) {
    const auto [x, y] = attention_label(frame, label, last);
    std::size_t consumed = 0;
    for (int column = x; column < 120 && consumed < label.size(); ++column) {
        const auto& cell = frame.cells[y][column];
        if (!cell.text.empty() && cell.text != " ")
            require_attention_cell(cell, foreground, bold, label);
        consumed += cell.text.size();
    }
    require(consumed == label.size(), "attention label crossed the physical row: " + label);
}

std::pair<int, int> bottom_work_glyph(const TerminalFrame& frame) {
    for (int y = 39; y >= 30; --y) {
        const auto& glyph = frame.cells[y][0].text;
        if (std::find(kCanonicalWorkGlyphs.begin(), kCanonicalWorkGlyphs.end(), glyph) !=
            kCanonicalWorkGlyphs.end()) return {0, y};
        constexpr std::array<std::string_view, 8> legacy = {"✻", "❋", "✦", "✧", "✶", "✷", "✸", "✹"};
        require(std::find(legacy.begin(), legacy.end(), glyph) == legacy.end(),
                "ordinary work retained a legacy star spinner");
    }
    return {-1, -1};
}

void save_attention_frame(const fs::path& root, const std::string& name, const std::string& raw) {
    require(!raw.empty(), "cannot retain an empty terminal frame");
    std::ofstream(root / (name + ".raw"), std::ios::binary) << raw;
    const auto frame = decode_frame(raw);
    std::ofstream text(root / (name + ".txt"));
    std::ofstream styles(root / (name + ".styles.txt"));
    styles << "positions are 1-based physical terminal cells; default foreground/background are empty\n";
    for (int y = 0; y < 40; ++y) {
        text << frame.row(y) << '\n';
        for (int x = 0; x < 120; ++x) {
            const auto& cell = frame.cells[y][x];
            if (cell.text == " " && cell.background.empty()) continue;
            styles << "row=" << y + 1 << " col=" << x + 1 << " glyph=\"" << escape_json(cell.text)
                   << "\" fg=" << cell.foreground << " bg=" << cell.background
                   << " bold=" << cell.bold << " dim=" << cell.dim << '\n';
        }
    }
}

void run_task_response_layout_case(const std::string& binary, const fs::path& root) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":false,"permissionMode":"default","extendedThinking":false})";
    ModelServer server(false, false, root / "unused.txt", false, "task-response-layout");
    Terminal terminal(binary, root, server.url(), ModelOptions{});
    server.start();
    require(terminal.wait([&] { return terminal.installed(); }), "task layout startup not observed");
    terminal.input("Track the task layout\r");
    require(terminal.wait([&] {
        return terminal.latest_frame_text().find("[y] Allow  [n] Deny") != std::string::npos;
    }), "todo_write permission prompt not observed");
    terminal.input("y");
    require(terminal.wait([&] {
        return server.parked.load() == 1 && terminal.latest_frame_text().find("Tasks 1 active · 1/3 done") != std::string::npos;
    }), "real todo_write result did not reach the pinned task list");
    for (const int width : {40, 80, 120}) {
        const auto start = terminal.output.rfind('\0') + 1;
        terminal.resize(static_cast<unsigned short>(width), 40);
        require(terminal.wait([&] { return !terminal.first_resize_frame_since(start).empty(); }),
                "task layout resize frame not observed");
        const auto raw = terminal.first_resize_frame_since(start);
        const auto frame = decode_frame(raw);
        const auto spinner = bottom_work_glyph(frame);
        require(spinner.first == 0, "work row is not aligned with the input's left edge");
        const int header_y = spinner.second + 1;
        require(header_y + 3 < 40, "task response extends beyond the terminal");
        require(frame.cells[header_y][2].text == "⎿" && frame.cells[header_y][5].text == "T",
                "task header is not a MessageResponse directly below work");
        for (int row = 1; row <= 3; ++row) {
            for (int x = 0; x < 7; ++x)
                require(frame.cells[header_y + row][x].text == " ", "task details repeat the hook or lose indentation");
            require(frame.cells[header_y + row][7].text == (row == 1 ? "●" : row == 2 ? "○" : "✓"),
                    "task child must begin two cells beyond its heading");
        }
        require(frame.row(header_y + 1).find("PTY active task") != std::string::npos &&
                frame.row(header_y + 2).find("PTY pending task") != std::string::npos &&
                frame.row(header_y + 3).find("PTY completed task") != std::string::npos,
                "task response lost status rows during resize");
        save_attention_frame(root, "tasks-" + std::to_string(width), raw);
    }
    terminal.input("\x04");
    terminal.verify_exit(0);
}

// Completed data must survive UI expiry, explicit active-list clear and restart.
void run_task_completion_case(const std::string& binary, const fs::path& root) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":false,"permissionMode":"default","extendedThinking":false})";
    const auto gate = root / "complete-tasks";
    ModelServer server(false, false, gate, false, "task-completion");
    ModelOptions options;
    server.start();
    {
        Terminal terminal(binary, root, server.url(), options);
        require(terminal.wait([&] { return terminal.installed(); }), "task completion startup not observed");
        terminal.input("Track completion\r");
        require(terminal.wait([&] {
            return terminal.latest_frame_text().find("[y] Allow  [n] Deny") != std::string::npos;
        }), "task permission not observed");
        terminal.input("y");
        require(terminal.wait([&] {
            return server.parked.load() == 1 && terminal.latest_frame_text().find("Tasks 1 active · 0/3 done") != std::string::npos;
        }), "active checklist not observed");
        save_attention_frame(root, "completion-running", terminal.latest_frame());
        std::ofstream(gate) << "complete";
        bool permission_answered = false;
        require(terminal.wait([&] {
            const auto text = terminal.latest_frame_text();
            if (!permission_answered && text.find("[y] Allow  [n] Deny") != std::string::npos) {
                terminal.input("y");
                permission_answered = true;
            }
            return text.find("PTY completion finished") != std::string::npos &&
                text.find("Tasks 3/3 completed") != std::string::npos;
        }), "completed checklist and final response not observed");
        const auto header_row = [](const auto& frame) {
            int found = -1;
            for (int y = 0; y < 40; ++y)
                if (frame.row(y).find("Tasks 3/3 completed") != std::string::npos) found = y;
            return found;
        };
        const auto completed = decode_frame(terminal.latest_frame());
        const int expanded_y = header_row(completed);
        require(expanded_y >= 0 && expanded_y + 3 < 40 &&
            completed.row(expanded_y + 1).find("PTY inspect source") != std::string::npos &&
            completed.row(expanded_y + 3).find("PTY record evidence") != std::string::npos,
            "completed detail rows disappeared before grace expiry");
        save_attention_frame(root, "completion-expanded", terminal.latest_frame());
        require(terminal.wait([&] {
            const auto frame = decode_frame(terminal.latest_frame());
            const int y = header_row(frame);
            return y >= 0 && frame.row(y).find("✓ Tasks 3/3 completed") != std::string::npos &&
                frame.row(y + 1).find("PTY inspect source") == std::string::npos;
        }), "idle checklist did not collapse without user input");
        const auto collapsed = decode_frame(terminal.latest_frame());
        require(collapsed.cells[header_row(collapsed)][0].text == "✓",
            "idle completed summary retained an orphan MessageResponse prefix");
        save_attention_frame(root, "completion-collapsed", terminal.latest_frame());
        terminal.input("/resume missing-task-session\r");
        require(terminal.wait([&] {
            const auto text = terminal.latest_frame_text();
            return text.find("not found") != std::string::npos &&
                text.find("✓ Tasks 3/3 completed") != std::string::npos;
        }), "failed resume changed the current completed checklist");
        std::string current_id;
        for (const auto& entry : fs::directory_iterator(root / "config" / "sessions"))
            if (fs::exists(entry.path() / "messages.jsonl")) current_id = entry.path().filename().string();
        require(!current_id.empty(), "current task session not persisted");
        terminal.input("/resume " + current_id + "\r");
        require(terminal.wait([&] {
            const auto text = terminal.latest_frame_text();
            return text.find("Resuming session:") != std::string::npos &&
                text.find("✓ Tasks 3/3 completed") == std::string::npos;
        }), "same-process resume leaked the previous activity summary");
        terminal.prepare_transcript_shortcut();
        terminal.input("\x0f");
        require(terminal.wait([&] {
            return terminal.latest_frame_text().find("✓ PTY record evidence") != std::string::npos;
        }), "same-process resume lost the completed history details");
        save_attention_frame(root, "completion-same-process-resume", terminal.latest_frame());
        terminal.input("Clear active tasks\r");
        permission_answered = false;
        require(terminal.wait([&] {
            const auto text = terminal.latest_frame_text();
            if (!permission_answered && text.find("[y] Allow  [n] Deny") != std::string::npos) {
                terminal.input("y");
                permission_answered = true;
            }
            return text.find("PTY active tasks cleared") != std::string::npos;
        }), "explicit active-list clear did not finish");
        const auto cleared = terminal.latest_frame_text();
        require(cleared.find("✓ Tasks 3/3 completed") == std::string::npos &&
            cleared.find("Tasks 3/3 completed") != std::string::npos &&
            cleared.find("✓ PTY record evidence") != std::string::npos,
            "clearing active tasks removed the completed history record");
        terminal.input("\x04");
        terminal.verify_exit(0);
    }
    std::string saved_id;
    for (const auto& entry : fs::directory_iterator(root / "config" / "sessions"))
        if (fs::exists(entry.path() / "messages.jsonl")) saved_id = entry.path().filename().string();
    require(!saved_id.empty(), "completed session history was not persisted");
    {
        Terminal replay(binary, root, server.url(), options);
        require(replay.wait([&] { return replay.installed(); }), "history replay startup not observed");
        replay.input("/resume " + saved_id + "\r");
        require(replay.wait([&] {
            return replay.latest_frame_text().find("Resuming session:") != std::string::npos;
        }), "interactive resume did not finish");
        replay.prepare_transcript_shortcut();
        replay.input("\x0f");
        require(replay.wait([&] {
            const auto text = replay.latest_frame_text();
            return text.find("Tasks 3/3 completed") != std::string::npos &&
                text.find("✓ PTY inspect source") != std::string::npos &&
                text.find("✓ PTY record evidence") != std::string::npos;
        }), "saved completion checklist was not visible after resume");
        save_attention_frame(root, "completion-resumed-history", replay.latest_frame());
        replay.input("\x04");
        replay.verify_exit(0);
    }
}

void verify_decoder_attention_attributes() {
    const auto frame = decode_frame("\x1b[31;44;1;2mA\x1b[22;39mB\x1b[49mC"
        "\x1b[38;5;123;48;5;45;1mD\x1b[38;2;1;2;3;48;2;4;5;6;2mE\x1b[0mF"
        "\x1b[91;104mG");
    const auto& cells = frame.cells[0];
    require(cells[0].foreground == "31" && cells[0].background == "44" && cells[0].bold && cells[0].dim,
            "SGR decoder lost 16-color/bold/dim attributes");
    require(cells[1].foreground.empty() && cells[1].background == "44" && !cells[1].bold && !cells[1].dim,
            "SGR22/39 reset did not preserve the independent background");
    require(cells[2].background.empty(), "SGR49 reset did not clear the background");
    require(cells[3].foreground == "38;5;123" && cells[3].background == "48;5;45" && cells[3].bold && !cells[3].dim,
            "SGR decoder lost indexed256 colors or inherited attributes");
    require(cells[4].foreground == "38;2;1;2;3" && cells[4].background == "48;2;4;5;6" && cells[4].bold && cells[4].dim,
            "SGR decoder lost truecolor or independent bold/dim attributes");
    require(cells[5].foreground.empty() && cells[5].background.empty() && !cells[5].bold && !cells[5].dim,
            "SGR0 did not reset all decoded attributes");
    require(cells[6].foreground == "91" && cells[6].background == "104",
            "SGR decoder lost bright16 colors");
}

void verify_bottom_work_attention(const TerminalFrame& frame) {
    const auto spinner = bottom_work_glyph(frame);
    require(spinner.first >= 0, "working frame lacks a canonical bottom braille spinner");
    const auto& glyph = frame.cells[spinner.second][spinner.first];
    require(glyph.foreground == kDarkPrimary && !glyph.dim,
            "canonical work glyph did not use readable primary ink");
    bool saw_label = false;
    for (int x = spinner.first + 2; x < 120; ++x) {
        const auto& cell = frame.cells[spinner.second][x];
        if (cell.text == "…" || cell.text == ".") break;
        if (cell.text.empty() || cell.text == " ") continue;
        require_attention_cell(cell, kDarkMuted, false, "bottom work label");
        saw_label = true;
    }
    require(saw_label, "canonical bottom spinner lacks a readable work label");
}

void verify_thinking_work_attention(const std::string& raw) {
    const auto frame = decode_frame(raw);
    const auto title = attention_label(frame, "Thinking");
    const auto disclosure = attention_label(frame, "Show details");
    require(title.second == disclosure.second, "thinking title and disclosure changed row geometry");
    for (int x = title.first; x < disclosure.first - 1; ++x) {
        const auto& cell = frame.cells[title.second][x];
        if (!cell.text.empty() && cell.text != " ")
            require_attention_cell(cell, kDarkPrimary, true, "thinking work summary");
    }
    verify_bottom_work_attention(frame);
}

void verify_builtin_metadata_attention(const TerminalFrame& frame) {
    const auto model = attention_label(frame, "pty-test-model", /*last=*/true);
    const auto path = attention_label(frame, "loom_pty_", /*last=*/true);
    require(model.second >= 35 && path.second == model.second,
            "metadata oracle matched welcome/history text instead of the real built-in statusline");
    require_attention_label(frame, "pty-test-model", kDarkText, true, /*last=*/true);
    require_attention_label(frame, "loom_pty_", kDarkMuted, false, /*last=*/true);
}

void verify_normal_attention(const std::string& raw) {
    const auto frame = decode_frame(raw);
    verify_builtin_metadata_attention(frame);
    require(bottom_work_glyph(frame).first < 0, "normal frame retained a competing bottom work spinner");
}

void verify_chain_attention(const std::string& raw, ChainAttention attention) {
    const auto frame = decode_frame(raw);
    const bool working = attention == ChainAttention::Working;
    const bool completed = attention == ChainAttention::Completed;
    const auto title = attention_label(frame, completed ? "Worked" : "Working");
    const bool expanded = terminal_text(raw).find("Hide details") != std::string::npos;
    const auto disclosure = attention_label(frame, expanded ? "Hide details" : "Show details");
    require(title.second == disclosure.second, "chain title and disclosure changed row geometry");
    for (int x = title.first; x < disclosure.first - 1; ++x) {
        const auto& cell = frame.cells[title.second][x];
        if (cell.text.empty() || cell.text == " ") continue;
        require_attention_cell(cell, working ? kDarkPrimary : kDarkMuted, working, "chain summary");
        if (expanded)
            require(cell.background == kDarkBackground,
                    "expanded chain summary did not use the theme background surface");
    }
    if (!completed) {
        require_attention_label(frame, working ? "Running Bash" : "Preparing Bash", kDarkText, false);
        require_attention_label(frame, "PTY-ACTIVITY", kDarkText, false);
    }
    const auto spinner = bottom_work_glyph(frame);
    if (working) {
        verify_bottom_work_attention(frame);
    } else {
        require(spinner.first < 0, "permission/completed frame retained a competing bottom work spinner");
    }
    if (attention == ChainAttention::Permission) {
        require_attention_label(frame, "(?) Permission", kDarkWarning, true);
        require_attention_label(frame, R"("command":"printf PTY-ACTIVITY;)", kDarkText, false);
        const auto instruction = attention_label(frame, "[y] Allow  [n] Deny");
        for (int x = instruction.first; x < instruction.first + 19; ++x)
            require(!frame.cells[instruction.second][x].dim, "required permission instruction was dim");
    }
}

void verify_failure_attention(const std::string& raw, const std::string& diagnostic) {
    const auto frame = decode_frame(raw);
    require_attention_label(frame, "Model request failed", kDarkDanger, true);
    require_attention_label(frame, diagnostic, kDarkText, false);
    require_attention_label(frame, "Retry request", kDarkText, false);
    require_attention_label(frame, "Dismiss error", kDarkText, false);
    require(bottom_work_glyph(frame).first < 0, "failed request retained a competing bottom work spinner");
    verify_builtin_metadata_attention(frame);
}

void verify_corrected_permission_attention(const std::string& raw, const std::string& diagnosis) {
    const auto frame = decode_frame(raw);
    const auto text = terminal_text(raw);
    require(text.find(diagnosis) != std::string::npos, "corrected approval hid the prior tool diagnosis");
    require(text.find("Model request failed") == std::string::npos,
            "old tool failure became a global model-request failure");
    // The real centered overlay can cover this later chain's title/activity.
    // Its neutral style is asserted in the unobscured compressed-chain case;
    // here inspect only visible diagnosis, decision target and actual controls.
    require_attention_label(frame, "(?) Permission", kDarkWarning, true);
    require_attention_label(frame, R"({"command":"pwd"})", kDarkText, false);
    const auto keys = attention_label(frame, "[y] Allow  [n] Deny");
    for (int x = keys.first; x < keys.first + 19; ++x)
        require(!frame.cells[keys.second][x].dim, "corrected approval instructions were dim");
    require(bottom_work_glyph(frame).first < 0, "corrected approval competed with a work spinner");
}

std::string mouse_report(int button, std::pair<int, int> position, bool release = false) {
    return "\x1b[<" + std::to_string(button) + ';' + std::to_string(position.first + 1) + ';' +
        std::to_string(position.second + 1) + (release ? "m" : "M");
}

// RFC 0014: exercise real FTXUI hit-testing and stage lifecycle rather than
// rendering synthetic VisibleRows. Model and tool completion are separate gates.
void run_permission_footer_case(const std::string& binary, const fs::path& root) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":true,"permissionMode":"default","extendedThinking":false,"theme":"dark"})";
    const auto target = root / "scope-target.txt";
    ModelServer server(false, true, target.filename());
    Terminal terminal(binary, root, server.url(), ModelOptions{});
    server.start();
    require(terminal.wait([&] { return terminal.installed(); }), "terminal installation not observed");
    terminal.input("Request permission footer fixture\r");
    require(terminal.wait([&] {
        return terminal.latest_frame_text().find("[y] Allow") != std::string::npos;
    }), "permission prompt not observed");
    bool scopes_checked = false;
    auto verify_footer = [&](const std::string& raw, int width, int height) {
        const auto frame = decode_frame(raw);
        for (const auto* label : {"Permission", "Always deny this tool", "Always allow this scope",
                "Sandbox", "[y] Allow", "[n] Deny", "[a] Always allow", "[d] Always deny",
                "[s] Toggle sandbox", "[Esc] Cancel", "[PgUp/PgDn] Details"}) {
            const auto point = frame.find(label);
            require(point.first + static_cast<int>(std::string_view(label).size()) <= width &&
                    point.second < height, "footer label exceeds viewport: " + std::string(label));
        }
        for (const auto* label : {"Always deny this tool", "Always allow this scope"})
            (void)frame.find(std::string(scopes_checked ? "[x] " : "[ ] ") + label);
        (void)frame.find(scopes_checked ? "Sandbox [ON]" : "Sandbox [OFF]");
        require(!fs::exists(target), "reflow or state-only keys approved the pending write");
        require(server.requests.load() == 1, "pending permission unexpectedly advanced the query");
    };
    save_attention_frame(root, "initial-120x40", terminal.latest_frame());
    verify_footer(terminal.latest_frame(), 120, 40);
    terminal.input("12s");
    require(terminal.wait([&] {
        const auto text = terminal.latest_frame_text();
        return text.find("[x] Always deny this tool") != std::string::npos &&
               text.find("[x] Always allow this scope") != std::string::npos &&
               text.find("Sandbox [ON]") != std::string::npos;
    }), "scope/sandbox toggles did not render");
    scopes_checked = true;
    for (const auto [width, height] : {std::pair{80, 40}, {60, 40}, {40, 40}, {120, 24}, {80, 24}, {60, 24}, {40, 24}}) {
        const auto start = terminal.output.rfind('\0') + 1;
        terminal.resize(static_cast<unsigned short>(width), static_cast<unsigned short>(height));
        require(terminal.wait([&] { return !terminal.first_resize_frame_since(start).empty(); }),
                "permission resize did not produce a complete frame");
        const auto raw = terminal.first_resize_frame_since(start);
        save_attention_frame(root, "first-" + std::to_string(width) + "x" + std::to_string(height), raw);
        verify_footer(raw, width, height);
    }
    std::string paged_text = terminal.latest_frame_text();
    for (int page = 0; page < 8; ++page) {
        const auto start = terminal.output.rfind('\0');
        terminal.input("\x1b[6~");
        require(terminal.wait([&] { return terminal.output.rfind('\0') > start; }),
                "PageDown did not redraw permission details");
        paged_text += terminal.latest_frame_text();
        save_attention_frame(root, "details-page-" + std::to_string(page), terminal.latest_frame());
        verify_footer(terminal.latest_frame(), 40, 24);
    }
    require(paged_text.find("scope-target.txt") != std::string::npos &&
            paged_text.find("execute") != std::string::npos,
            "paging could not reach the request target and final content");
    terminal.input("\x1b[5~");
    const auto wide_start = terminal.output.rfind('\0') + 1;
    terminal.resize(120, 40);
    require(terminal.wait([&] { return !terminal.first_resize_frame_since(wide_start).empty(); }),
            "resize after paging did not complete");
    verify_footer(terminal.first_resize_frame_since(wide_start), 120, 40);
    // State survived every resize and page; toggling back never commits a rule.
    save_attention_frame(root, "toggled", terminal.latest_frame());
    verify_footer(terminal.latest_frame(), 120, 40);
    terminal.input("12s");
    require(terminal.wait([&] {
        const auto text = terminal.latest_frame_text();
        return text.find("[ ] Always deny this tool") != std::string::npos &&
               text.find("[ ] Always allow this scope") != std::string::npos &&
               text.find("Sandbox [OFF]") != std::string::npos;
    }), "scope checkbox reset was not rendered");
    scopes_checked = false;
    verify_footer(terminal.latest_frame(), 120, 40);
    terminal.input("\x1b");
    require(terminal.wait([&] { return terminal.latest_frame_text().find("[y] Allow") == std::string::npos; }),
            "visible Escape shortcut did not cancel permission");
    require(!fs::exists(target), "cancelled write was executed");
    terminal.input("\x04");
    terminal.verify_exit(0);
}

void run_compressed_chain_case(const std::string& binary, const fs::path& root) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":true,"permissionMode":"default","extendedThinking":false,"theme":"dark"})";
    const auto release = root / "release.txt";
    ModelServer server(false, false, release, false, "compressed-chain-live");
    Terminal terminal(binary, root, server.url(), ModelOptions{});
    server.start();
    require(terminal.wait([&] {
        return terminal.installed() && terminal.latest_frame_text().find("loom_pty_") != std::string::npos;
    }), "complete normal terminal frame not observed");
    verify_decoder_attention_attributes();
    save_attention_frame(root, "normal", terminal.latest_frame());
    verify_normal_attention(terminal.latest_frame());
    auto save_frame = [&](const std::string& name, std::string raw = {}) {
        if (raw.empty()) raw = terminal.latest_frame();
        save_attention_frame(root, name, raw);
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
        return frame.find("Thinking") != std::string::npos && frame.find("Show details") != std::string::npos &&
            bottom_work_glyph(decode_frame(terminal.latest_frame())).first >= 0;
    }), "initial thinking and bottom canonical work spinner were not visible");
    save_frame("working");
    verify_thinking_work_attention(terminal.latest_frame());
    server.chain_tools = true;
    require(terminal.wait([&] {
        const auto frame = terminal.latest_frame_text();
        return frame.find("[y] Allow  [n] Deny") != std::string::npos && frame.find("Working") != std::string::npos &&
            frame.find("Preparing") != std::string::npos && frame.find("Show details") != std::string::npos &&
            frame.find(R"("command":"printf PTY-ACTIVITY;)") != std::string::npos &&
            frame.find("'" + release.string() + "'") != std::string::npos;
    }), "pending chain, exact command target and authoritative permission prompt were not visible together");
    save_frame("pending");
    verify_chain_attention(terminal.latest_frame(), ChainAttention::Permission);
    terminal.input("y");
    require(terminal.wait([&] {
        const auto frame = terminal.latest_frame_text();
        return frame.find("Running") != std::string::npos && frame.find("PTY-ACTIVITY") != std::string::npos &&
            frame.find("[y] Allow") == std::string::npos;
    }), "running tool's actual command was absent from collapsed activity line");
    save_frame("running");
    verify_chain_attention(terminal.latest_frame(), ChainAttention::Working);
    const auto initial_elapsed = std::max(0, elapsed_seconds("Working"));
    require(terminal.wait([&] { return elapsed_seconds("Working") >= initial_elapsed + 2; }),
            "live stage elapsed did not advance in the real terminal");
    const auto live_elapsed = elapsed_seconds("Working");
    save_frame("ticking");
    verify_chain_attention(terminal.latest_frame(), ChainAttention::Working);

    // Click the activity body, not the disclosure action: the entire two-line
    // synthetic row must share the expansion hit target.
    click_label("Running");
    require(terminal.wait([&] {
        return terminal.latest_frame_text().find("Hide details") != std::string::npos;
    }), "clicking the activity line did not expand the chain");
    save_frame("expanded");
    int resize_step = 0;
    for (const auto width : {60, 120, 40, 80, 120}) {
        const auto start = terminal.output.rfind('\0') + 1;
        terminal.resize(static_cast<unsigned short>(width), 40);
        require(terminal.wait([&] {
            return !terminal.first_resize_frame_since(start).empty();
        }), "terminal did not emit a complete resized frame");
        const auto raw = terminal.first_resize_frame_since(start);
        save_frame("expanded-first-" + std::to_string(++resize_step) + "-" + std::to_string(width), raw);
        const auto frame = decode_frame(raw);
        const auto activity = frame.find("Running");
        require(frame.row(activity.second).find("PTY-ACTIVITY") != std::string::npos,
                "first resized frame lost the current activity target");
        require(frame.find("Hide details").first < width,
                "first resized frame lost explicit expansion state");
        int prompt_rules = 0;
        for (int y = 0; y < 40; ++y) {
            bool full_width = true;
            for (int x = 0; x < width; ++x)
                full_width = full_width && frame.cells[y][x].text == "─";
            if (full_width) ++prompt_rules;
        }
        require(prompt_rules >= 2,
                "first resized frame retained the previous prompt width at " + std::to_string(width) + " columns");
        bool activity_ellipsis = false;
        for (int x = width - 4; x < width; ++x)
            activity_ellipsis = activity_ellipsis || frame.cells[activity.second][x].text == "…";
        require(activity_ellipsis,
                "first resized frame retained the previous activity width at " + std::to_string(width) + " columns");
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
    verify_chain_attention(terminal.latest_frame(), ChainAttention::Completed);
    verify_normal_attention(terminal.latest_frame());
    click_label("Hide details");
    require(terminal.wait([&] {
        return terminal.latest_frame_text().find("Show details") != std::string::npos &&
            elapsed_seconds("Worked") == completed_elapsed;
    }), "collapsed completion changed duration or disclosure interaction");
    save_frame("completed-collapsed");
    verify_chain_attention(terminal.latest_frame(), ChainAttention::Completed);
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

void install_clipboard_capture(const fs::path& root, const fs::path& harness) {
    fs::create_directories(root / "clipboard-bin");
    // Reuse the running native fixture. On this macOS host, fresh executable
    // scripts stalled before interpreter entry during launch-policy checks.
    for (const auto& name : {"pbcopy", "wl-copy", "xclip"})
        fs::create_symlink(fs::canonical(harness), root / "clipboard-bin" / name);
}

int capture_clipboard() {
    const char* destination = ::getenv("PTY_CLIPBOARD_FILE");
    if (!destination || !*destination) return 2;
    const fs::path path(destination);
    const auto pending = path.string() + ".pending";
    std::ofstream output(pending, std::ios::binary);
    output << std::cin.rdbuf();
    output.close();
    if (!output || std::cin.bad()) return 1;
    std::error_code error;
    fs::rename(pending, path, error);
    return error ? 1 : 0;
}

std::string selection_label(char prefix, int number) {
    const auto digits = std::to_string(number);
    return std::string(1, prefix) + std::string(3 - digits.size(), '0') + digits;
}

void run_selection_case(const std::string& binary, const std::string& scenario, const fs::path& root,
                        const fs::path& harness) {
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":true,"permissionMode":"default","extendedThinking":false})";
    install_clipboard_capture(root, harness);
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
        const auto copied_frame = decode_frame(terminal.latest_frame());
        require(copied_frame.find("Copied").second == baseline.find("────────").second - 1,
                "clipboard feedback is not on the existing above-editor row");
        require(copied_frame.find("PTYSELECT start").second == baseline.find("PTYSELECT start").second,
                "clipboard feedback moved the transcript");
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

void run_escape_case(const std::string& binary, const std::string& scenario, const fs::path& root,
                     const fs::path& harness) {
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
    if (selection) install_clipboard_capture(root, harness);
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

// Retain the blocked application's stacks before Terminal's failure cleanup
// kills its process group. Sampling is optional and never a test prerequisite.
void sample_handoff_timeout(const fs::path& root) {
#if defined(__APPLE__)
    if (::access("/usr/bin/sample", X_OK) != 0) return;
    const auto target = read_file(root / "loom.pid");
    const auto destination = (root / "loom.sample.txt").string();
    const auto sampler = ::fork();
    if (sampler == 0) {
        ::execl("/usr/bin/sample", "sample", target.c_str(), "1", "-file", destination.c_str(), nullptr);
        ::_exit(127);
    }
    if (sampler < 0) return;
    const auto deadline = Clock::now() + 5s;
    while (::waitpid(sampler, nullptr, WNOHANG) == 0) {
        if (Clock::now() >= deadline) {
            ::kill(sampler, SIGKILL);
            while (::waitpid(sampler, nullptr, 0) < 0 && errno == EINTR) {}
            break;
        }
        ::poll(nullptr, 0, 20);
    }
#else
    (void)root;
#endif
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
    terminal.capture_driver_state("before-stop");
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
    const bool handed_off = terminal.wait([&] { return terminal.output.find("PTY-SHELL-READY") != std::string::npos; });
    if (!handed_off) {
        terminal.capture_driver_state("handoff-timeout");
        sample_handoff_timeout(root);
    }
    require(handed_off, "zsh did not regain the terminal");
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

constexpr std::string_view status_probe = "\x1b]7501;?\x1b\\";

// Parse the actual PTY byte stream independently of Loom's encoder. Reports
// must contain only the root identity, state and optional blocked kind.
std::vector<std::string> status_reports(std::string_view output) {
    constexpr std::string_view prefix = "\x1b]7501;";
    std::vector<std::string> reports;
    std::size_t offset = 0;
    while ((offset = output.find(prefix, offset)) != std::string_view::npos) {
        const auto start = offset + prefix.size();
        const auto end = output.find("\x1b\\", start);
        if (end == std::string_view::npos) break;
        const auto payload = output.substr(start, end - start);
        offset = end + 2;
        if (payload == "?") continue;
        std::map<std::string, std::string> fields;
        for (std::size_t at = 0; at < payload.size();) {
            const auto delimiter = payload.find(':', at);
            const auto item = payload.substr(at, delimiter == std::string_view::npos ? payload.size() - at : delimiter - at);
            const auto equal = item.find('=');
            require(equal != std::string_view::npos, "OSC 7501 report field has no value");
            const auto key = std::string(item.substr(0, equal));
            require(key == "state" || key == "app" || key == "kind", "OSC 7501 leaked an unexpected field: " + key);
            require(fields.emplace(key, item.substr(equal + 1)).second, "duplicate OSC 7501 field");
            if (delimiter == std::string_view::npos) break;
            at = delimiter + 1;
        }
        require(fields["app"] == "loom", "OSC 7501 report lost root app identity");
        const auto& state = fields["state"];
        require(state == "idle" || state == "working" || state == "blocked" || state == "done" || state == "error",
                "OSC 7501 report has an unknown state");
        const auto kind = fields.find("kind");
        require(kind == fields.end() || (state == "blocked" &&
            (kind->second == "permission" || kind->second == "question" || kind->second == "auth")),
            "OSC 7501 emitted kind outside a blocked request");
        reports.push_back(state + (kind == fields.end() ? "" : ":" + kind->second));
    }
    return reports;
}

std::string describe_statuses(const std::vector<std::string>& states) {
    std::string result;
    for (const auto& state : states) result += (result.empty() ? "" : " -> ") + state;
    return result;
}

void expect_statuses(const Terminal& terminal, const std::vector<std::string>& expected) {
    const auto actual = status_reports(terminal.output);
    require(actual == expected, "OSC 7501 order: expected " + describe_statuses(expected) +
            "; received " + describe_statuses(actual));
}

// Find one complete tool-result object, rather than accepting answer text
// merely echoed inside the preceding assistant tool-use input.
std::string status_question_result(const std::string& request) {
    const auto messages = request_messages(request);
    std::vector<std::size_t> objects;
    bool quoted = false;
    bool escaped = false;
    for (std::size_t at = 0; at < messages.size(); ++at) {
        const char ch = messages[at];
        if (ch == '"' && !escaped) quoted = !quoted;
        if (!quoted) {
            if (ch == '{') objects.push_back(at);
            if (ch == '}') {
                require(!objects.empty(), "malformed status fixture request object");
                const auto object = messages.substr(objects.back(), at - objects.back() + 1);
                objects.pop_back();
                if (object.starts_with("{\"type\":\"tool_result\",") &&
                    object.find("\"tool_use_id\":\"pty-status-question\"") != std::string::npos)
                    return object;
            }
        }
        escaped = ch == '\\' && !escaped;
    }
    throw std::runtime_error("question tool result missing from the real model request");
}

void settle_terminal(Terminal& terminal, std::chrono::milliseconds duration) {
    const auto deadline = Clock::now() + duration;
    require(terminal.wait([&] { return Clock::now() >= deadline; }, 3s),
            "terminal exited during the observation window");
}

void negotiate_status(Terminal& terminal, const std::string& scenario) {
    require(terminal.wait([&] { return terminal.output.find(status_probe) != std::string::npos; }),
            "OSC 7501 capability probe was not observed");
    const auto install = terminal.output.find("\x1b[?1049h");
    require(install != std::string::npos && install < terminal.output.find(status_probe),
            "OSC 7501 probe preceded raw terminal installation");
    require(status_reports(terminal.output).empty(), "OSC 7501 reported before capability reply");
    if (scenario == "osc7501-unsupported") return;
    if (scenario == "osc7501-late") settle_terminal(terminal, 1200ms);
    if (scenario == "osc7501-truncated") {
        terminal.input("\x1b]7501;?:states=idle,work");
        settle_terminal(terminal, 1200ms);
        return;
    }
    if (scenario == "osc7501-fragmented") {
        // Split the command itself after ESC ] and then the payload. Both
        // pauses exceed FTXUI's original 50ms incomplete-sequence timeout.
        terminal.input("\x1b]75");
        settle_terminal(terminal, 100ms);
        terminal.input("01;?:states=idle,working,");
        settle_terminal(terminal, 100ms);
        terminal.input("done,blocked,error:kinds=permission,question,auth\x1b\\");
    } else if (scenario == "osc7501-restricted-states") {
        terminal.input("\x1b]7501;?:states=idle,working,done,blocked\x1b\\");
    } else if (scenario == "osc7501-restricted-kind") {
        terminal.input("\x1b]7501;?:kinds=auth\x1b\\");
    } else if (scenario == "osc7501-bel") {
        terminal.input("\x1b]7501;?\a");
    } else if (scenario == "osc7501-st") {
        terminal.input("\x1b]7501;?: states = idle,working,done,blocked,error : unknown = ignored : kinds = permission,question,auth \x1b\\");
    } else {
        terminal.input(std::string(status_probe));
    }
    if (scenario == "osc7501-late" || scenario == "osc7501-restricted-states") {
        settle_terminal(terminal, 100ms);
        require(status_reports(terminal.output).empty(), "late/restricted capabilities enabled OSC 7501 reports");
    } else {
        require(terminal.wait([&] { return status_reports(terminal.output) == std::vector<std::string>{"idle"}; }),
                "capability reply did not emit exactly one initial idle");
    }
}

void run_status_handoff_case(const std::string& binary, const fs::path& root, const std::string& harness) {
    std::ofstream summary(root / "status-handoff.txt");
    for (const std::string kind : {"ctrl-d", "term", "hup"}) {
        const auto isolated = root / kind;
        fs::create_directories(isolated / "config");
        std::ofstream(isolated / "config" / "settings.json") <<
            R"({"mouseMode":true,"permissionMode":"default","extendedThinking":false})";
        ModelServer server(false, false, isolated / "unused.txt", false, "osc7501-handoff");
        ModelOptions options;
        options.shell_handoff = true;
        options.handoff_helper = harness;
        Terminal terminal(binary, isolated, server.url(), options);
        server.start();
        negotiate_status(terminal, "osc7501-st");
        require(terminal.wait([&] { return fs::exists(isolated / "loom.pid") && terminal.mode_enabled(1003); }),
                "status handoff child did not install mouse input");
        terminal.input("PTY status handoff\r");
        require(terminal.wait([&] { return server.parked.load() == 1 &&
            status_reports(terminal.output) == std::vector<std::string>{"idle", "working"}; }),
            "status handoff operation did not start");
        terminal.capture_driver_state("before-stop");
        terminal.signal(SIGSTOP);
        require(terminal.wait([&] { return fs::exists(isolated / "loom.stopped"); }),
                "status handoff did not stop before queued input");
        std::string pending = kind == "ctrl-d" ? "\x04" : "";
        for (int report = 0; report < 20; ++report) pending += "\x1b[<35;74;23M";
        pending += status_probe;
        terminal.input(pending);
        if (kind == "term") terminal.signal(SIGTERM);
        if (kind == "hup") terminal.signal(SIGHUP);
        terminal.signal(SIGCONT);
        const bool ready = terminal.wait([&] { return terminal.output.find("PTY-SHELL-READY") != std::string::npos; });
        if (!ready) {
            terminal.capture_driver_state("handoff-timeout");
            sample_handoff_timeout(isolated);
        }
        require(ready, "status handoff did not return to real zsh");
        expect_statuses(terminal, {"idle", "working", "idle"});
        require(terminal.output.rfind("\x1b]7501;") < terminal.output.find("PTY-SHELL-READY"),
                "OSC 7501 wrote after parent-shell handoff");
        for (const int mode : {1000, 1002, 1003, 1006, 1015})
            require(!terminal.mode_enabled(mode), "status handoff left mouse tracking enabled");
        terminal.input("shell-handoff-ok\r");
        require(terminal.wait([&] { return fs::exists(isolated / "shell-input.txt"); }),
                "real shell did not consume status handoff follow-up");
        require(read_file(isolated / "shell-input.txt") == "shell-handoff-ok",
                "queued terminal control bytes polluted real shell input");
        const std::string expected_exit = kind == "ctrl-d" ? "0" : "1";
        require(read_file(isolated / "loom.actual-status") == expected_exit &&
                read_file(isolated / "loom.status") == expected_exit, "status integration changed signal exit codes");
        terminal.verify_exit(0);
        summary << kind << ": " << describe_statuses(status_reports(terminal.output))
                << "; exact shell input=16 bytes; mouse modes reset; termios restored; Loom exit=" << expected_exit << '\n';
        std::ofstream(root / ("handoff-" + kind + ".raw"), std::ios::binary) << terminal.output;
    }
}

void run_status_case(const std::string& binary, const std::string& scenario, const fs::path& root,
                     const std::string& harness) {
    if (scenario == "osc7501-handoff") {
        run_status_handoff_case(binary, root, harness);
        return;
    }
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":true,"permissionMode":"default","extendedThinking":false})";
    const bool disabled = scenario == "osc7501-unsupported" || scenario == "osc7501-late" ||
        scenario == "osc7501-restricted-states" || scenario == "osc7501-truncated";
    const bool permission = scenario == "osc7501-permission" || scenario == "osc7501-restricted-kind";
    const bool question = scenario == "osc7501-question";
    const bool budget = scenario == "osc7501-budget";
    const bool cancel = scenario == "osc7501-cancel";
    const bool error = scenario == "osc7501-error";
    const auto fixture = budget ? "round-click" : permission ? "osc7501-permission" : scenario;
    const auto target = root / "status-tool.txt";
    ModelServer server(false, false, target, false, fixture);
    ModelOptions options;
    if (budget) options.extra_arguments = {"--max-tool-rounds=1"};
    Terminal terminal(binary, root, server.url(), options);
    server.start();
    negotiate_status(terminal, scenario);
    if (disabled && scenario == "osc7501-unsupported") settle_terminal(terminal, 1200ms);
    constexpr auto prompt = "PTY status prompt";
    terminal.input(std::string(prompt) + '\r');
    if (permission || question || budget) {
        const auto blocked = scenario == "osc7501-restricted-kind" ? "blocked" : "blocked:permission";
        require(terminal.wait([&] { return terminal.permission_visible() &&
            status_reports(terminal.output) == std::vector<std::string>{"idle", "working", blocked}; }),
            "actual tool permission was not reported as blocked");
        require(!fs::exists(target), "unapproved status fixture executed a file write");
        terminal.input("y");
    }
    if (question) {
        // This runtime tool is checked both by QueryEngine and by its live
        // registry permission checker. Approve each actual request; neither
        // approval is a response to the subsequent ask-user dialog.
        require(terminal.wait([&] { return terminal.permission_visible() &&
            status_reports(terminal.output) == std::vector<std::string>{"idle", "working", "blocked:permission", "working", "blocked:permission"}; }),
            "runtime ask-user permission was not reported as blocked permission");
        terminal.input("y");
        require(terminal.wait([&] { return terminal.latest_frame_text().find("Question") != std::string::npos &&
            terminal.latest_frame_text().find("[Enter] Submit") != std::string::npos &&
            status_reports(terminal.output) == std::vector<std::string>{"idle", "working", "blocked:permission", "working", "blocked:permission", "working", "blocked:question"}; }),
            "actual ask-user dialog was not reported as blocked question");
        // The existing prompt UI accepts its supplied default on Return; it
        // does not implement free-form editing. Exercise that real decision.
        terminal.input("\r");
    }
    if (budget) {
        require(terminal.wait([&] { return terminal.latest_frame_text().find("[C] Continue") != std::string::npos &&
            status_reports(terminal.output) == std::vector<std::string>{"idle", "working", "blocked:permission", "working", "blocked:question"}; }),
            "actual tool-round limit was not reported as blocked question");
        require(server.requests.load() == 1, "round budget continued without human input");
        terminal.input("c");
    }
    std::vector<std::string> expected = disabled ? std::vector<std::string>{} : std::vector<std::string>{"idle", "working"};
    if (permission) expected.insert(expected.end(), {scenario == "osc7501-restricted-kind" ? "blocked" : "blocked:permission", "working"});
    if (question) expected.insert(expected.end(), {"blocked:permission", "working", "blocked:permission", "working", "blocked:question", "working"});
    if (budget) expected.insert(expected.end(), {"blocked:permission", "working", "blocked:question", "working"});
    if (cancel) {
        require(terminal.wait([&] { return server.parked.load() == 1 &&
            terminal.latest_frame_text().find("PTY status streaming") != std::string::npos; }),
            "status cancellation stream did not start");
        terminal.input("\x1b");
        expected.push_back("idle");
        require(terminal.wait([&] { return server.closed.load() == 1 && status_reports(terminal.output) == expected; }),
                "actual Esc cancellation did not finish idle");
        require(server.requests.load() == 1, "cancellation issued an extra model request");
    } else if (error) {
        expected.push_back("error");
        require(terminal.wait([&] { return terminal.latest_frame_text().find("Model request failed") != std::string::npos &&
            status_reports(terminal.output) == expected; }), "stopped model error did not report error");
    } else {
        if (!disabled) expected.push_back("done");
        require(terminal.wait([&] { return terminal.idle_response_visible(budget ? "PTY round-budget complete" : "PTY status complete") &&
            status_reports(terminal.output) == expected; }), "whole operation did not finish with the expected terminal status");
    }
    const auto bodies = server.request_bodies();
    require(!bodies.empty(), "status fixture saw no real model request");
    const auto messages = request_messages(bodies.front());
    require(messages.find("\"content\":\"PTY status prompt\"") != std::string::npos ||
            messages.find("\"text\":\"PTY status prompt\"") != std::string::npos,
            "terminal capability bytes contaminated the submitted prompt");
    if (question) {
        const auto result = status_question_result(bodies.back());
        require(result == R"({"type":"tool_result","tool_use_id":"pty-status-question","is_error":false,"content":"PTY status answer"})",
                "accepted default answer did not reach the exact successful tool result");
    }
    if (permission) require(read_file(target) == "PTY status write", "approved permission did not execute its actual file write");
    if (budget) require(read_file(target) == "PTY-round-0\n", "round continuation repeated or lost the committed tool");
    const auto before_resize = terminal.output.size();
    terminal.resize(100, 36);
    require(terminal.wait([&] { return !terminal.first_resize_frame_since(before_resize).empty(); }),
            "status resize did not produce a complete real frame");
    // Synthetic redraws, duplicate/malformed control replies and mouse motion
    // must not acknowledge a retained done/error or create duplicate reports.
    terminal.input(std::string(status_probe) + "\x1b]7501;?:states=invalid\x1b\\"
                   "\x1b[<35;74;23M\x1b[<32;74;23M\x1b[<34;74;23M");
    settle_terminal(terminal, 100ms);
    expect_statuses(terminal, expected);
    terminal.input("\x04");
    terminal.verify_exit(0);
    expect_statuses(terminal, expected);
    require(occurrences(terminal.output, std::string(status_probe)) == 1,
            "OSC 7501 sent more than one capability probe");
    std::ofstream(root / "status-result.txt") << describe_statuses(expected)
        << "\nexact submitted prompt retained; resize/control/motion deduplicated; direct Ctrl+D outcome retained; termios restored\n";
}

std::string first_question_frame_since(const Terminal& terminal, std::size_t start) {
    for (auto end = terminal.output.find('\0', start); end != std::string::npos;
         end = terminal.output.find('\0', start)) {
        const auto frame = terminal.output.substr(start, end - start);
        const auto visible = terminal_text(frame);
        if (visible.find("Question") != std::string::npos && visible.find("[Enter] Submit") != std::string::npos)
            return frame;
        start = end + 1;
    }
    return {};
}

void verify_question_spaces(const std::string& raw, const fs::path& artifacts, const std::string& label) {
    std::ofstream(artifacts / (label + ".raw"), std::ios::binary) << raw;
    std::ofstream(artifacts / (label + ".txt")) << terminal_text(raw);
    const auto frame = decode_frame(raw);
    const auto position = frame.find("PTY exact question words");
    require(position.first >= 0, "question words lost their actual spaces in " + label);
    require(terminal_text(raw).find("[Enter] Submit") != std::string::npos &&
            terminal_text(raw).find("[Esc] Cancel") != std::string::npos,
            "question decision hints were clipped in " + label);
}

void run_question_session(const std::string& binary, const std::string& scenario, const fs::path& root,
                          const fs::path& artifacts, unsigned short columns, const std::string& label) {
    fs::create_directories(root / "config");
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":true,"permissionMode":"default","extendedThinking":false})";
    ModelServer server(false, false, root / "unused.txt", false, scenario);
    ModelOptions options;
    options.initial_columns = columns;
    Terminal terminal(binary, root, server.url(), options);
    server.start();
    negotiate_status(terminal, "osc7501-st");
    terminal.input("PTY question request\r");
    std::vector<std::string> blocked{"idle", "working"};
    std::size_t question_start = 0;
    for (int gate = 0; gate < 2; ++gate) {
        blocked.push_back("blocked:permission");
        require(terminal.wait([&] { return terminal.permission_visible() && status_reports(terminal.output) == blocked; }),
                "question fixture did not reach its actual tool permission");
        question_start = terminal.output.rfind('\0') + 1;
        terminal.input("y");
        blocked.push_back("working");
    }
    blocked.push_back("blocked:question");
    require(terminal.wait([&] { return status_reports(terminal.output) == blocked &&
        !first_question_frame_since(terminal, question_start).empty(); }),
        "actual question did not reach a complete first frame and blocked status");
    verify_question_spaces(first_question_frame_since(terminal, question_start), artifacts, label + "-first");

    auto expected_answer = scenario == "question-unicode" ? std::string("甲乙🙂") :
        scenario == "question-resize" || scenario == "question-empty-no-default" ? std::string{} : std::string("seed");
    const auto edit = [&](const std::string& bytes, const std::string& visible) {
        const auto before = terminal.output.size();
        terminal.input(bytes);
        require(terminal.wait([&] { return terminal.output.size() > before &&
            terminal.latest_frame_text().find(visible) != std::string::npos; }),
            "question edit did not appear in the actual input row");
        settle_terminal(terminal, 80ms);
        expect_statuses(terminal, blocked);
        require(server.requests.load() == 1, "editing an answer prematurely submitted to the model");
    };
    if (scenario == "question-edit") {
        edit("\x1b[H\x1b[3~S\x1b[F edited", "Seed edited");
        expected_answer = "Seed edited";
    } else if (scenario == "question-unicode") {
        edit("\x7f\x1b[D\x1b[3~中文\x1b[H\x1b[3~\x1b[F🙂", "中文🙂");
        expected_answer = "中文🙂";
    } else if (scenario == "question-resize") {
        for (int segment = 0; segment < 20; ++segment) expected_answer += "word" + std::to_string(segment) + " ";
        expected_answer += "尾🙂 ENDMARK";
        edit(expected_answer, "ENDMARK");
        for (const unsigned short width : {40, 80, 120, 40}) {
            const auto start = terminal.output.rfind('\0') + 1;
            terminal.resize(width, 40);
            require(terminal.wait([&] { return !terminal.first_resize_frame_since(start).empty(); }),
                    "question resize did not produce a first complete frame");
            const auto raw = terminal.first_resize_frame_since(start);
            verify_question_spaces(raw, artifacts, label + "-resized-" + std::to_string(width));
            require(terminal_text(raw).find("ENDMARK") != std::string::npos,
                    "long answer cursor/end vanished in the first resized frame");
            expect_statuses(terminal, blocked);
        }
        edit("\x1b[HBEGIN ", "BEGIN");
        expected_answer = "BEGIN " + expected_answer;
        edit("\x1b[F END", "END");
        expected_answer += " END";
    } else if (scenario == "question-empty-default") {
        // Home followed by Delete repeatedly clears the entire prefill.
        terminal.input("\x1b[H\x1b[3~\x1b[3~\x1b[3~\x1b[3~");
        settle_terminal(terminal, 100ms);
        expect_statuses(terminal, blocked);
        expected_answer.clear();
    }
    std::ofstream(artifacts / (label + "-edited.raw"), std::ios::binary) << terminal.latest_frame();
    const bool cancelled = scenario == "question-cancel";
    terminal.input(cancelled ? "\x1b" : "\r");
    auto completed = blocked;
    completed.insert(completed.end(), {"working", "done"});
    require(terminal.wait([&] { return terminal.idle_response_visible("PTY question complete") &&
        status_reports(terminal.output) == completed; }), "question decision did not resolve and recover the model turn");
    auto bodies = server.request_bodies();
    require(bodies.size() == 2, "question decision issued duplicate or missing model requests");
    const auto result = status_question_result(bodies.back());
    const auto expected_content = cancelled ? "User cancelled the prompt" : expected_answer;
    const auto expected_result = std::string("{\"type\":\"tool_result\",\"tool_use_id\":\"pty-status-question\",\"is_error\":") +
        (cancelled ? "true" : "false") + ",\"content\":\"" + escape_json(expected_content) + "\"}";
    require(result == expected_result, "question result differs from the actual edited/empty/cancelled answer: " + result);

    // A new real prompt must start empty: none of the dialog's edits or
    // navigation bytes may have entered the underlying REPL input buffer.
    terminal.input("PTY question follow-up\r");
    completed.insert(completed.end(), {"idle", "working", "done"});
    require(terminal.wait([&] { return terminal.idle_response_visible("PTY question follow-up complete") &&
        status_reports(terminal.output) == completed; }), "REPL could not continue cleanly after the question");
    bodies = server.request_bodies();
    require(bodies.size() == 3, "question follow-up issued unexpected model requests");
    const auto messages = request_messages(bodies.back());
    require(messages.find("\"content\":\"PTY question follow-up\"") != std::string::npos ||
            messages.find("\"text\":\"PTY question follow-up\"") != std::string::npos,
            "question answer/edit keys leaked into the real REPL prompt");
    terminal.input("\x04");
    terminal.verify_exit(0);
    expect_statuses(terminal, completed);
    std::ofstream(artifacts / (label + "-session.raw"), std::ios::binary) << terminal.output;
    std::ofstream(artifacts / (label + "-result.txt")) << "initial columns=" << columns
        << "\nexact tool result=" << result << "\nstatus=" << describe_statuses(completed)
        << "\nfollow-up prompt exact; termios restored\n";
}

void run_question_case(const std::string& binary, const std::string& scenario, const fs::path& root) {
    if (scenario == "question-layout") {
        for (const unsigned short width : {40, 80, 120}) {
            const auto label = "question-layout-" + std::to_string(width);
            run_question_session(binary, scenario, root / label, root, width, label);
        }
    } else if (scenario == "question-empty") {
        for (const std::string variant : {"question-empty-default", "question-empty-no-default"})
            run_question_session(binary, variant, root / variant, root, 120, variant);
    } else {
        run_question_session(binary, scenario, root, root, 120, scenario);
    }
}

struct OccupancyFooter {
    double tokens;
    double window;
    int percent;
};

std::optional<OccupancyFooter> occupancy_footer(const Terminal& terminal) {
    const auto frame = terminal.latest_frame_text();
    static const std::regex pattern(R"(~([0-9]+)%\s+([0-9]+(?:\.[0-9]+)?)(K?)/([0-9]+(?:\.[0-9]+)?)(K?))");
    std::optional<OccupancyFooter> result;
    for (std::sregex_iterator it(frame.begin(), frame.end(), pattern), end; it != end; ++it) {
        const auto& match = *it;
        result = OccupancyFooter{std::stod(match[2]) * (match[3] == "K" ? 1000 : 1),
            std::stod(match[4]) * (match[5] == "K" ? 1000 : 1), std::stoi(match[1])};
    }
    return result;
}

OccupancyFooter wait_occupancy(Terminal& terminal, double window) {
    require(terminal.wait([&] {
        const auto current = occupancy_footer(terminal);
        return current && current->window == window;
    }), "complete estimated occupancy footer with expected window was not rendered");
    const auto value = *occupancy_footer(terminal);
    // The footer rounds displayed thousands to 100 tokens and percentage to an integer.
    require(std::abs(value.percent - std::lround(100.0 * value.tokens / value.window)) <= 1,
            "footer percentage and displayed current tokens disagree");
    return value;
}

void seed_occupancy_history(const fs::path& path) {
    std::ofstream out(path);
    out << R"({"active_conversation_id":"occupancy-seed","conversations":[{"id":"occupancy-seed","title":"Occupancy history","messages":[)";
    const auto text = [&](const std::string& role, const std::string& id, const std::string& content) {
        out << "{\"role\":\"" << role << "\",\"id\":\"" << id
            << "\",\"timestamp_ms\":1700000000000,\"content\":[{\"type\":\"text\",\"text\":\""
            << escape_json(content) << "\"}]}";
    };
    text("user", "occupancy-task", "Inspect the historical fixture; preserve completed result 47.");
    out << R"(,{"role":"assistant","id":"occupancy-read","timestamp_ms":1700000000000,"content":[{"type":"tool_use","id":"occupancy-read-call","name":"Read","input_json":"{\"file_path\":\"occupancy-history.txt\"}"}],"tool_name":"Read","tool_input_json":"{\"file_path\":\"occupancy-history.txt\"}"},)";
    out << R"({"role":"tool","id":"occupancy-result","timestamp_ms":1700000000000,"tool_name":"Read","tool_use_id":"occupancy-read-call","is_error":false,"content":[{"type":"text","text":")"
        << std::string(1200000, 'x') << R"( completed result=47"}]},)";
    text("assistant", "occupancy-read-done", "Historical read completed.");
    for (int i = 0; i < 20; ++i) {
        out << ',';
        text("user", "occupancy-old-u-" + std::to_string(i), "Completed historical turn " + std::to_string(i));
        out << ',';
        text("assistant", "occupancy-old-a-" + std::to_string(i), "Recorded historical progress " + std::to_string(i));
    }
    out << ',';
    text("user", "occupancy-late-constraint", "Late constraint: preserve OCCUPANCY_KEEP_47; do not commit.");
    out << ',';
    text("assistant", "occupancy-recent", std::string(12000, 'r') + " Completed result=47; ready for verification.");
    out << "]}]}";
    require(out.good(), "occupancy seed conversation was not written");
}

void run_context_occupancy_case(const std::string& binary, const std::string& scenario,
                                const fs::path& root) {
    const bool repeated = scenario == "context-occupancy-repeated";
    std::ofstream(root / "config" / "settings.json") <<
        R"({"mouseMode":false,"extendedThinking":false,"model":"occupancy-1m","maxOutputTokens":64000,"modelContextLimits":[{"model":"occupancy-1m","contextWindowSize":1000000,"maxOutputTokens":64000},{"model":"occupancy-256k","contextWindowSize":256000,"maxOutputTokens":4096}]})";
    ModelOptions options;
    options.environment_model.clear();
    options.isolate_budget_environment = true;
    if (!repeated) {
        seed_occupancy_history(root / "seed.json");
        options.extra_arguments = {"--session-store", (root / "seed.json").string(), "--continue"};
    }
    ModelServer server(false, false, root / "unused.txt", false, scenario);
    server.start();
    Terminal terminal(binary, root, server.url(), options);
    const auto initial = wait_occupancy(terminal, 1000000);
    require(server.requests.load() == 0, "occupancy status read sampled at startup");
    const auto sample = [&](int turn) {
        terminal.input("Verify occupancy; keep OCCUPANCY_KEEP_47 and completed result=47, turn " + std::to_string(turn) + "\r");
        require(terminal.wait([&] {
            return server.occupancy_business.load() == turn + 1 &&
                terminal.idle_response_visible("PTY occupancy reply " + std::to_string(turn));
        }), "occupancy business request did not complete");
    };
    sample(0);
    const auto first = wait_occupancy(terminal, 1000000);
    save_attention_frame(root, "occupancy-first", terminal.latest_frame());
    // Precise cumulative billing is covered through the native App status-command
    // JSON contract. These PTY cases exercise visible current-context behavior.
    if (repeated) {
        sample(1);
        const auto second = wait_occupancy(terminal, 1000000);
        require(first.tokens >= 99000 && second.tokens >= 99000 && second.tokens < 150000,
                "repeated business usage accumulated into current occupancy");
        require(second.tokens < first.tokens * 1.2, "small retained output doubled current context");
        require(server.requests.load() == 2, "short repeated requests unexpectedly compacted or retried");
        save_attention_frame(root, "occupancy-repeated", terminal.latest_frame());
        terminal.input("/clear\r");
        require(terminal.wait([&] {
            const auto current = occupancy_footer(terminal);
            return current && current->tokens < first.tokens / 4;
        }), "clear retained the previous request's occupancy floor");
        require(server.requests.load() == 2, "clear sampled the model");
    } else {
        require(initial.tokens > 250000 && first.tokens > 300000,
                "restored large history did not cross the smaller model's capacity");
        terminal.input("/model occupancy-256k\r");
        const auto switched = wait_occupancy(terminal, 256000);
        require(switched.tokens > 256000 && switched.percent > 100,
                "real over-capacity working history was hidden or clamped");
        require(server.requests.load() == 1, "model switch sampled before the business boundary");
        sample(1);
        const auto compacted = wait_occupancy(terminal, 256000);
        require(compacted.tokens < first.tokens / 4, "compaction did not lower displayed current occupancy");
        const auto bodies = server.request_bodies();
        int summaries = 0;
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].find("CONTEXT CHECKPOINT") != std::string::npos) ++summaries;
            std::ofstream(root / ("occupancy-request-" + std::to_string(i) + ".txt")) << bodies[i];
        }
        require(summaries >= 1 && summaries <= 16 && bodies.size() == static_cast<std::size_t>(summaries + 2),
                "compaction did not separate bounded summary calls from exactly two business requests");
        require(bodies.front().size() > 1200000 && bodies.back().size() < bodies.front().size() / 4,
                "actual compacted business request did not shrink");
        const auto first_body = compact_json(bodies.front());
        const auto last_body = compact_json(bodies.back());
        require(first_body.find("\"model\":\"occupancy-1m\"") != std::string::npos &&
                first_body.find("\"max_tokens\":64000") != std::string::npos &&
                last_body.find("\"model\":\"occupancy-256k\"") != std::string::npos &&
                last_body.find("\"max_tokens\":4096") != std::string::npos,
                "footer model/window changes disagree with actual business model/output limits");
        require(bodies.back().find("Late constraint: preserve OCCUPANCY_KEEP_47; do not commit.") != std::string::npos &&
                bodies.back().find("completed result=47") != std::string::npos,
                "compacted request lost its task constraint or completed progress");
        std::string saved_session;
        std::string checkpoint;
        for (const auto& entry : fs::directory_iterator(root / "config" / "sessions")) {
            const auto candidate = compact_json(read_file(entry.path() / "context-checkpoints.jsonl"));
            if (candidate.find("\"kind\":\"context_checkpoint\"") == std::string::npos) continue;
            require(saved_session.empty(), "fixture committed checkpoints in multiple sessions");
            saved_session = entry.path().filename().string();
            checkpoint = candidate;
        }
        require(occurrences(checkpoint, "\"kind\":\"context_checkpoint\"") == 1 &&
                checkpoint.find("OCCUPANCY_KEEP_47") != std::string::npos,
                "one durable semantic checkpoint with the constraint was not committed");
        std::ofstream(root / "occupancy-checkpoint.txt") << checkpoint;
        save_attention_frame(root, "occupancy-compacted", terminal.latest_frame());
        const int requests = server.requests.load();
        terminal.input("/model occupancy-1m\r");
        const auto restored_window = wait_occupancy(terminal, 1000000);
        require(restored_window.tokens < first.tokens / 4, "switch-back revived old large occupancy");
        require(server.requests.load() == requests, "switch-back sampled the model");
        terminal.input("/clear\r");
        require(terminal.wait([&] {
            const auto value = occupancy_footer(terminal);
            return value && value->tokens + 500 < restored_window.tokens;
        }), "clear did not remove retained checkpoint history from occupancy");
        const auto cleared = wait_occupancy(terminal, 1000000);
        save_attention_frame(root, "occupancy-cleared", terminal.latest_frame());
        terminal.input("/resume " + saved_session + "\r");
        require(terminal.wait([&] {
            const auto value = occupancy_footer(terminal);
            return terminal.latest_frame_text().find("Resuming session:") != std::string::npos &&
                value && value->tokens > cleared.tokens + 500 && value->tokens < first.tokens / 4;
        }), "resume failed to recompute the compacted working context");
        save_attention_frame(root, "occupancy-resumed", terminal.latest_frame());
        require(server.requests.load() == requests, "switch-back, clear or resume sampled the model");
    }
    std::ofstream(root / "occupancy-counts.txt") << "initial=" << initial.tokens << "\nfirst=" << first.tokens
        << "\nrequests=" << server.requests.load() << '\n';
    terminal.input("\x04");
    terminal.verify_exit(0);
}

// RFC 0028: real CLI assembly and settings persistence must agree with the
// provider-default policy, including the environment normally disabled here.
void run_generation_defaults_case(const std::string& binary, const std::string& scenario,
                                  const fs::path& root) {
    const bool explicit_values = scenario == "generation-defaults-explicit";
    const bool manual = scenario == "generation-defaults-manual";
    const std::vector<std::string> variants = manual
        ? std::vector<std::string>{"absent", "true"} : std::vector<std::string>{"probe"};
    for (const auto& variant : variants) {
        const auto directory = manual ? root / variant : root;
        fs::create_directories(directory / "config");
        const auto user_path = directory / "config" / "settings.json";
        std::string fixture = R"({"mouseMode":false,"permissionMode":"default","model":"generation-private","theme":"dark")";
        if (explicit_values)
            fixture += R"(,"maxOutputTokens":1024,"temperature":0.4,"extendedThinking":false,"thinkingBudget":4096)";
        if (manual) {
            fixture += R"(,"thinkingBudget":2048)";
            if (variant == "true") fixture += R"(,"extendedThinking":true)";
        }
        fixture += '}';
        { std::ofstream output(user_path); output << fixture; require(output.good(), "generation fixture settings were not written"); }
        ModelOptions options;
        options.environment_model.clear();
        options.isolate_budget_environment = true;
        options.use_configured_thinking = true;
        ModelServer server(false, false, directory / "unused.txt", false, "model-context");
        server.start();
        Terminal terminal(binary, directory, server.url(), options);
        require(terminal.wait([&] {
            return terminal.installed() && terminal.latest_frame_text().find("generation-private") != std::string::npos;
        }), "generation CLI did not show its configured model");
        require(server.requests.load() == 0, "generation startup unexpectedly sampled the model");
        save_attention_frame(root, variant + "-initial", terminal.latest_frame());
        if (!manual) {
            terminal.input("/config\r");
            require(terminal.wait([&] {
                const auto frame = terminal.latest_frame_text();
                return frame.find("Settings") != std::string::npos && frame.find("Theme") != std::string::npos &&
                    frame.find("Ctrl+S Save") != std::string::npos;
            }), "generation settings panel did not open");
            save_attention_frame(root, "settings-before", terminal.latest_frame());
            terminal.input("\r");  // The first General field changes dark to light.
            require(terminal.wait([&] {
                const auto frame = terminal.latest_frame_text();
                return frame.find("[light]") != std::string::npos && frame.find("unsaved changes") != std::string::npos;
            }), "generation settings theme change did not render");
            terminal.input("\x13");
            const auto project_path = directory / ".loom" / "settings.json";
            require(terminal.wait([&] {
                const auto frame = terminal.latest_frame_text();
                return frame.find("Settings saved") != std::string::npos &&
                    compact_json(read_file(project_path)).find("\"theme\":\"light\"") != std::string::npos;
            }), "generation settings did not save the visible theme to the project file");
            save_attention_frame(root, "settings-saved", terminal.latest_frame());
            const auto saved = compact_json(read_file(project_path));
            std::ofstream(root / "settings-saved.txt") << read_file(project_path);
            require(read_file(user_path) == fixture, "settings save unexpectedly changed the source user file");
            if (explicit_values) {
                for (const std::string field : {"\"maxOutputTokens\":1024", "\"temperature\":0.4", "\"extendedThinking\":false", "\"thinkingBudget\":4096"})
                    require(saved.find(field) != std::string::npos, "settings save lost explicit field " + field);
            } else {
                for (const std::string key : {"maxOutputTokens", "temperature", "extendedThinking", "thinkingBudget"})
                    require(saved.find("\"" + key + "\":") == std::string::npos, "settings save materialized automatic field " + key);
            }
            terminal.input("\x1b");
            require(terminal.wait([&] {
                const auto frame = terminal.latest_frame_text();
                return frame.find("Ctrl+S Save") == std::string::npos && frame.find("generation-private") != std::string::npos;
            }), "settings panel did not return to the model prompt");
        }
        terminal.input("Return the generation defaults probe\r");
        require(terminal.wait([&] {
            return server.requests.load() == 1 && terminal.idle_response_visible("PTY model context reply 0");
        }), "generation defaults request did not finish");
        const auto bodies = server.request_bodies();
        require(bodies.size() == 1, "generation defaults duplicated the business request");
        std::ofstream(root / (variant + "-request.txt")) << bodies.front();
        const auto body = compact_json(bodies.front());
        require(body.find("\"model\":\"generation-private\"") != std::string::npos,
                "generation request used the wrong actual model");
        std::smatch output_match;
        require(std::regex_search(body, output_match, std::regex(R"("max_tokens":([1-9][0-9]*)[,}])")),
                "Messages automatic output was omitted or nonpositive");
        const auto output = std::stoull(output_match[1].str());
        if (explicit_values) {
            require(output == 1024, "CLI did not preserve its explicit output override");
            std::smatch temperature_match;
            require(std::regex_search(body, temperature_match, std::regex(R"("temperature":([0-9.]+)[,}])")) &&
                std::abs(std::stod(temperature_match[1].str()) - 0.4) < 0.000000001,
                "CLI did not preserve its explicit sampling temperature");
            require(body.find("\"thinking\":") == std::string::npos,
                    "legacy false activated thinking or its ignored budget");
        } else {
            require(body.find("\"temperature\":") == std::string::npos,
                    "CLI invented an automatic sampling temperature");
            if (manual) {
                require(body.find("\"thinking\":{\"type\":\"enabled\",\"budget_tokens\":2048}") != std::string::npos,
                        "CLI did not activate the configured manual thinking budget");
                require(output > 2048, "manual thinking budget exhausted its output allowance");
            } else require(body.find("\"thinking\":") == std::string::npos,
                           "CLI invented a provider-default thinking override");
        }
        save_attention_frame(root, variant + "-completed", terminal.latest_frame());
        terminal.input("\x04");
        terminal.verify_exit(0);
    }
}

// RFC 0026: exercise main.cpp's real CLI loader, not only QueryEngine assembly.
void run_model_context_case(const std::string& binary, const std::string& scenario,
                            const fs::path& root) {
    const auto write_settings = [](const fs::path& directory, bool invalid, bool fallback) {
        fs::create_directories(directory / "config");
        std::ofstream settings(directory / "config" / "settings.json");
        settings << R"({"mouseMode":false,"permissionMode":"default","extendedThinking":false,"model":")"
            << (fallback ? "budget-private" : "budget-1m")
            << R"(","contextWindowSize":345678,"maxOutputTokens":64000,"modelContextLimits":[{"model":"budget-1m","contextWindowSize":)"
            << (invalid ? 0 : 1000000)
            << R"(,"maxOutputTokens":64000},{"model":"budget-256k","contextWindowSize":256000,"maxOutputTokens":4096}]})";
        require(settings.good(), "model-context fixture settings were not written");
    };
    ModelOptions options;
    options.environment_model.clear();
    options.isolate_budget_environment = true;
    if (scenario == "model-context-metadata" || scenario == "model-context-metadata-direct") {
        const bool direct = scenario == "model-context-metadata-direct";
        std::ofstream(root / "config" / "settings.json") <<
            R"({"mouseMode":false,"permissionMode":"default","model":"metadata-large"})";
        ModelServer server(false, false, root / "unused.txt", false, "model-metadata");
        server.start();
        options.direct = direct;
        if (direct) options.runtime_tool_input =
            R"({"description":"Discovered child capacity","prompt":"Return the child result","name":"pty-agent","model":"metadata-small"})";
        Terminal terminal(binary, root, server.url(), options);
        if (direct) {
            terminal.verify_completion(0);
            require(server.metadata_requests.load() == 1, "direct entry did not prepare metadata exactly once");
            const auto bodies = server.request_bodies();
            require(bodies.size() == 1, "direct metadata child changed business request count");
            const auto body = compact_json(bodies.front());
            require(body.find("\"model\":\"metadata-small\"") != std::string::npos &&
                    body.find("\"max_tokens\":4096") != std::string::npos,
                    "direct child did not inherit the service's model-specific policy");
            std::ofstream(root / "metadata-child-request.txt") << bodies.front();
        } else {
            const auto footer = [&](const std::string& model, const std::string& window) {
                require(terminal.wait([&] {
                    const auto frame = terminal.latest_frame_text();
                    return terminal.installed() && frame.find(model) != std::string::npos &&
                        frame.find("/" + window) != std::string::npos;
                }), "service metadata was not reflected in the real CLI footer");
            };
            footer("metadata-large", "1048.6K");
            require(server.requests.load() == 0, "metadata discovery sampled the model");
            for (int index = 0; index < 3; ++index) {
                const std::string model = index == 1 ? "metadata-small" : "metadata-large";
                if (index > 0) terminal.input("/model " + model + "\r");
                footer(model, index == 1 ? "262.1K" : "1048.6K");
                save_attention_frame(root, "metadata-" + std::to_string(index), terminal.latest_frame());
                terminal.input("Return discovered capacity probe " + std::to_string(index) + "\r");
                require(terminal.wait([&] {
                    return server.requests.load() == index + 1 &&
                        terminal.idle_response_visible("PTY model context reply " + std::to_string(index));
                }), "discovered-capacity business request did not complete");
                const auto body = compact_json(server.request_bodies().back());
                require(body.find("\"model\":\"" + model + "\"") != std::string::npos &&
                        body.find("\"max_tokens\":" + std::to_string(index == 1 ? 4096 : 16384)) != std::string::npos,
                        "metadata capacity and business output policy disagreed");
                std::ofstream(root / ("metadata-request-" + std::to_string(index) + ".txt"))
                    << server.request_bodies().back();
            }
            terminal.input("\x04");
            terminal.verify_exit(0);
            require(server.metadata_requests.load() == 1, "model switching repeated metadata discovery");
        }
        const auto saved = read_file(root / "config" / "settings.json");
        require(saved.find("contextWindowSize") == std::string::npos &&
                saved.find("modelContextLimits") == std::string::npos,
                "service metadata was persisted as a user override");
        return;
    }
    if (scenario == "model-context-tolerance") {
        options.environment_model = "budget-default";
        for (const std::string kind : {"missing", "soft-user", "soft-local"}) {
            const auto directory = root / kind;
            fs::create_directories(directory / "config");
            if (kind == "soft-user")
                std::ofstream(directory / "config" / "settings.json") << "legacy_key=legacy_value\n";
            if (kind == "soft-local") {
                fs::create_directories(directory / ".loom");
                std::ofstream(directory / ".loom" / "settings.local.json") << "legacy_key=legacy_value\n";
            }
            ModelServer server(false, false, directory / "unused.txt", false, "model-context");
            server.start();
            Terminal terminal(binary, directory, server.url(), options);
            require(terminal.wait([&] {
                const auto frame = terminal.latest_frame_text();
                return terminal.installed() && frame.find("budget-default") != std::string::npos &&
                    frame.find("/200.0K") != std::string::npos;
            }), kind + " configuration did not preserve usable default CLI budgets");
            require(server.requests.load() == 0, kind + " configuration unexpectedly sampled at startup");
            save_attention_frame(root, kind, terminal.latest_frame());
            terminal.input("Return the default CLI budget probe\r");
            require(terminal.wait([&] {
                return server.requests.load() == 1 && terminal.idle_response_visible("PTY model context reply 0");
            }), kind + " configuration could not complete a request");
            const auto bodies = server.request_bodies();
            require(bodies.size() == 1, kind + " configuration duplicated the request");
            const auto body = compact_json(bodies.front());
            require(body.find("\"model\":\"budget-default\"") != std::string::npos &&
                    (body.find("\"max_tokens\":16384,") != std::string::npos ||
                     body.find("\"max_tokens\":16384}") != std::string::npos),
                    kind + " configuration lost environment model or default output tokens");
            std::ofstream(root / (kind + "-request.txt")) << bodies.front();
            terminal.input("\x04");
            terminal.verify_exit(0);
        }
        return;
    }
    if (scenario == "model-context-invalid") {
        // Both CLI entry points must propagate strict profile-load failure.
        for (const bool direct : {false, true}) {
            const auto directory = root / (direct ? "direct" : "interactive");
            write_settings(directory, true, false);
            ModelServer server(false, false, directory / "unused.txt", false, "model-context");
            server.start();
            options.direct = direct;
            Terminal terminal(binary, directory, server.url(), options);
            terminal.verify_completion(1);
            std::ofstream(root / (direct ? "invalid-direct.txt" : "invalid-interactive.txt"))
                << terminal_text(terminal.output);
            require(terminal.output.find("modelContextLimits") != std::string::npos,
                    "invalid profile did not produce its configuration diagnostic");
            require(server.requests.load() == 0, "invalid profile reached the model endpoint");
            require(!terminal.installed(), "invalid profile entered the interactive application");
        }
        return;
    }
    const bool direct = scenario == "model-context-direct";
    const bool fallback = scenario == "model-context-fallback";
    write_settings(root, false, fallback);
    options.direct = direct;
    if (direct) options.runtime_tool_input =
        R"({"description":"Child budget probe","prompt":"Return the child result","name":"pty-agent","model":"budget-256k"})";
    ModelServer server(false, false, root / "unused.txt", false, "model-context");
    server.start();
    Terminal terminal(binary, root, server.url(), options);
    const auto verify_request = [&](std::size_t index, const std::string& model, int output) {
        const auto bodies = server.request_bodies();
        require(bodies.size() == index + 1, "model-context request count changed unexpectedly");
        const auto body = compact_json(bodies[index]);
        require(body.find("\"model\":\"" + model + "\"") != std::string::npos,
                "CLI selected the wrong actual model");
        require(body.find("\"max_tokens\":" + std::to_string(output) + ",") != std::string::npos ||
                body.find("\"max_tokens\":" + std::to_string(output) + "}") != std::string::npos,
                "CLI did not apply the expected output budget " + std::to_string(output));
        std::ofstream(root / ("request-" + std::to_string(index) + ".txt")) << bodies[index];
    };
    if (direct) {
        terminal.verify_completion(0);
        verify_request(0, "budget-256k", 4096);
        require(terminal.output.find("PTY model context reply 0") != std::string::npos,
                "direct child did not return its model response");
        require(compact_json(read_file(root / "runtime" / "pty-agent.json")).find("\"status\":\"completed\"") != std::string::npos,
                "direct child did not complete durably");
        return;
    }
    const auto verify_footer = [&](const std::string& model, const std::string& capacity) {
        require(terminal.wait([&] {
            const auto frame = terminal.latest_frame_text();
            return terminal.installed() && frame.find(model) != std::string::npos &&
                frame.find("/" + capacity) != std::string::npos;
        }), "CLI footer did not show " + model + " /" + capacity);
    };
    verify_footer(fallback ? "budget-private" : "budget-1m", fallback ? "345.7K" : "1000.0K");
    require(server.requests.load() == 0, "CLI startup unexpectedly sampled the model");
    const std::vector<std::string> models = fallback
        ? std::vector<std::string>{"budget-private"}
        : std::vector<std::string>{"budget-1m", "budget-256k", "budget-1m"};
    for (std::size_t index = 0; index < models.size(); ++index) {
        const auto& model = models[index];
        if (index > 0) {
            terminal.input("/model " + model + "\r");
            require(terminal.wait([&] {
                const auto frame = terminal.latest_frame_text();
                return frame.find("Switched model:") != std::string::npos &&
                    frame.find(models[index - 1] + " → " + model) != std::string::npos &&
                    frame.find("model-switch") == std::string::npos &&
                    frame.find("Dialog not implemented") == std::string::npos;
            }), "model switch did not render its local confirmation without an overlay");
            verify_footer(model, index == 1 ? "256.0K" : "1000.0K");
            require(server.requests.load() == static_cast<int>(index),
                    "model setter sent an unexpected HTTP request");
        }
        save_attention_frame(root, "budget-" + std::to_string(index), terminal.latest_frame());
        terminal.input("Return model budget probe " + std::to_string(index) + "\r");
        require(terminal.wait([&] {
            return server.requests.load() == static_cast<int>(index + 1) &&
                terminal.idle_response_visible("PTY model context reply " + std::to_string(index));
        }), "CLI model-budget request did not complete");
        verify_request(index, model, model == "budget-256k" ? 4096 : 64000);
    }
    terminal.input("\x04");
    terminal.verify_exit(0);
}

void run_case(const std::string& binary, const std::string& scenario, const fs::path& root,
              const std::string& harness) {
    if (scenario.starts_with("generation-defaults-")) {
        run_generation_defaults_case(binary, scenario, root);
        return;
    }
    if (scenario.starts_with("context-occupancy-")) {
        run_context_occupancy_case(binary, scenario, root);
        return;
    }
    if (scenario.starts_with("model-context-")) {
        run_model_context_case(binary, scenario, root);
        return;
    }
    if (scenario.starts_with("question-")) {
        run_question_case(binary, scenario, root);
        return;
    }
    if (scenario.starts_with("osc7501-")) {
        run_status_case(binary, scenario, root, harness);
        return;
    }
    if (scenario.starts_with("generation-tps-")) {
        run_generation_tps_case(binary, scenario, root);
        return;
    }
    if (scenario == "welcome-monogram") {
        run_welcome_monogram_case(binary, root);
        return;
    }
    if (scenario.starts_with("escape-")) {
        run_escape_case(binary, scenario, root, harness);
        return;
    }
    if (scenario == "handoff-hup-stress") {
        for (int attempt = 0; attempt < 20; ++attempt) {
            const auto isolated = root / ("attempt-" + std::to_string(attempt));
            fs::create_directories(isolated / "config");
            run_handoff_case(binary, "handoff-hup", isolated, harness);
        }
        return;
    }
    if (scenario.starts_with("handoff-")) {
        run_handoff_case(binary, scenario, root, harness);
        return;
    }
    if (scenario == "permission-footer") {
        run_permission_footer_case(binary, root);
        return;
    }
    if (scenario == "compressed-chain-live") {
        run_compressed_chain_case(binary, root);
        return;
    }
    if (scenario == "task-completion") {
        run_task_completion_case(binary, root);
        return;
    }
    if (scenario == "task-response-layout") {
        run_task_response_layout_case(binary, root);
        return;
    }
    if (scenario.starts_with("selection-")) {
        run_selection_case(binary, scenario, root, harness);
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
    const auto invocation = fs::path(argv[0]).filename();
    if (invocation == "pbcopy" || invocation == "wl-copy" || invocation == "xclip")
        return capture_clipboard();
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
    std::string pattern = scenario == "bash-failure" || scenario == "permission-denial" || scenario.starts_with("api-retry") || scenario.starts_with("round-") || scenario.starts_with("compressed-chain-") || scenario.starts_with("osc7501-") || scenario.starts_with("question-")
        ? "/tmp/loom_pty_recovery_XXXXXX" : "/tmp/loom_pty_shutdown_XXXXXX";
    const char* directory = ::mkdtemp(pattern.data());
    if (!directory) return 1;
    const fs::path root(directory);
    if ((scenario.starts_with("handoff-") || scenario == "osc7501-handoff") && ::access("/bin/zsh", X_OK) != 0 && ::access("/usr/bin/zsh", X_OK) != 0) {
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
        if (const char* evidence = ::getenv("LOOM_PTY_EVIDENCE_DIR"); evidence && (scenario.starts_with("generation-defaults-") || scenario.starts_with("context-occupancy-") || scenario.starts_with("model-context-") || scenario.starts_with("selection-") || scenario.starts_with("escape-") || scenario.starts_with("handoff-") || scenario.starts_with("compressed-chain-") || scenario.starts_with("osc7501-") || scenario.starts_with("question-") || scenario == "welcome-monogram" || scenario.starts_with("task-") || scenario.starts_with("generation-tps-") || scenario == "prompt-wrap" || scenario == "permission-footer" || scenario == "bash-failure" || scenario == "permission-denial" || scenario.starts_with("api-retry"))) {
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
