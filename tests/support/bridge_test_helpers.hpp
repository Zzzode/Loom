/// @file bridge_test_helpers.hpp
/// @brief Shared test helpers for bridge daemon + SDK protocol tests.
///
/// Extracted from test_bridge.cpp (domain split into transport / api /
/// daemon TUs). All helpers live in loom::test::bridge — a named namespace,
/// not anonymous — so that helpers unused in a given TU do not trigger
/// -Wunused-function. Free functions are inline to avoid ODR violations
/// across the three test TUs that include this header.
///
/// NOTE: This header contains `import` declarations for loom modules
/// (loom.bridge.*, loom.daemon.*) because those types have no textual
/// headers — they exist only as C++23 modules. The including TU does not
/// need to re-import them. Textual standard-library and POSIX headers are
/// included normally above the imports.

#pragma once

// ── Textual standard-library includes ──────────────────────────────────
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// ── Textual POSIX / system includes ───────────────────────────────────
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <unistd.h>

// ── Loom module imports ───────────────────────────────────────────────
// These types are only available via C++23 named modules (no textual
// headers exist for them). The imports are TU-scoped: any .cpp that
// includes this header gets them automatically.
import loom.bridge.api;
import loom.bridge.config;
import loom.bridge.messages;
import loom.bridge.session_id_compat;
import loom.bridge.transport;
import loom.bridge.work_secret;
import loom.daemon.daemon_client;
import loom.daemon.daemon_server;

namespace loom::test::bridge {

class ScopedEnvVar {
public:
    explicit ScopedEnvVar(const char* name) : name_(name) {
        if (auto* value = std::getenv(name)) previous_ = value;
        unsetenv(name);
    }
    ScopedEnvVar(const char* name, std::string_view value) : name_(name) {
        if (auto* previous = std::getenv(name)) previous_ = previous;
        setenv(name_, std::string(value).c_str(), 1);
    }
    ~ScopedEnvVar() {
        if (previous_) setenv(name_, previous_->c_str(), 1);
        else unsetenv(name_);
    }
    ScopedEnvVar(const ScopedEnvVar&) = delete;
    ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

private:
    const char* name_;
    std::optional<std::string> previous_;
};

inline std::filesystem::path unique_temp_file(std::string_view suffix) {
    return std::filesystem::temp_directory_path() / (loom::bridge::generate_session_id() + std::string(suffix));
}

inline std::string extract_json_string_field(std::string_view json, std::string_view key) {
    const auto pattern = std::string("\"") + std::string(key) + "\":\"";
    auto pos = json.find(pattern);
    if (pos == std::string_view::npos) return {};
    pos += pattern.size();
    auto end = json.find('"', pos);
    if (end == std::string_view::npos) return {};
    return std::string(json.substr(pos, end - pos));
}

inline bool bridge_test_send_all(int fd, std::string_view data) {
    while (!data.empty()) {
        auto n = ::send(fd, data.data(), data.size(), 0);
        if (n <= 0) return false;
        data.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

inline std::string bridge_test_base64_encode(const unsigned char* data, std::size_t len) {
    static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (std::size_t i = 0; i < len; i += 3) {
        std::uint32_t n = static_cast<std::uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<std::uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(table[(n >> 18) & 0x3f]);
        out.push_back(table[(n >> 12) & 0x3f]);
        out.push_back((i + 1 < len) ? table[(n >> 6) & 0x3f] : '=');
        out.push_back((i + 2 < len) ? table[n & 0x3f] : '=');
    }
    return out;
}

inline std::string bridge_test_base64url_encode(std::string_view data) {
    auto encoded = bridge_test_base64_encode(
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size());
    for (char& ch : encoded) {
        if (ch == '+') ch = '-';
        else if (ch == '/') ch = '_';
    }
    while (!encoded.empty() && encoded.back() == '=') encoded.pop_back();
    return encoded;
}

inline std::string bridge_test_work_secret_json() {
    return R"({"version":1,"session_ingress_token":"session-token-from-secret","api_base_url":"http://session-ingress.local","use_code_sessions":true,"code_session_mode":"code-session","sources":[{"type":"github","id":"source-1"}],"auth":{"records":[{"type":"oauth","name":"console"}]},"mcp_config":{"servers":{"linear":{"transport":"http","url":"https://mcp.example"}}},"environment_variables":{"REMOTE_FLAG":"enabled","REMOTE_COUNT":42}})";
}

inline std::string bridge_test_encoded_work_secret() {
    return bridge_test_base64url_encode(bridge_test_work_secret_json());
}

inline std::string bridge_test_encoded_work_secret(std::string_view api_base_url) {
    return bridge_test_base64url_encode(std::format(
        R"({{"version":1,"session_ingress_token":"session-token-from-secret","api_base_url":"{}","use_code_sessions":true,"code_session_mode":"code-session","sources":[{{"type":"github","id":"source-1"}}],"auth":{{"records":[{{"type":"oauth","name":"console"}}]}},"mcp_config":{{"servers":{{"linear":{{"transport":"http","url":"https://mcp.example"}}}}}},"environment_variables":{{"REMOTE_FLAG":"enabled","REMOTE_COUNT":42}}}})",
        api_base_url));
}

inline std::filesystem::path native_loom_binary_path() {
    if (const char* path = std::getenv("LOOM_TEST_NATIVE_BINARY"); path && *path) {
        return path;
    }
#ifdef LOOM_NATIVE_BINARY_PATH
    return std::filesystem::path{LOOM_NATIVE_BINARY_PATH};
#else
    return {};
#endif
}

inline std::string bridge_test_v1_work_secret_json() {
    return R"({"version":1,"session_ingress_token":"session-token-from-secret","api_base_url":"http://127.0.0.1:19191","use_code_sessions":false})";
}

inline std::string bridge_test_encoded_v1_work_secret() {
    return bridge_test_base64url_encode(bridge_test_v1_work_secret_json());
}

inline std::string bridge_test_accept_key(std::string_view key) {
    const std::string input = std::string(key) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::array<unsigned char, 20> hash{};
    SHA1(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash.data());
    return bridge_test_base64_encode(hash.data(), hash.size());
}

class LocalBridgeWebSocketServer {
public:
    LocalBridgeWebSocketServer() {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) return;
        int yes = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return;
        }
        if (::listen(listen_fd_, 4) != 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return;
        }

        socklen_t len = sizeof(addr);
        if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
            port_ = ntohs(addr.sin_port);
        }
        running_.store(true);
        accept_thread_ = std::jthread([this](std::stop_token stop) {
            accept_loop(stop);
        });
    }

    ~LocalBridgeWebSocketServer() {
        running_.store(false);
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
        const int client_fd = client_fd_.exchange(-1);
        if (client_fd >= 0) {
            ::shutdown(client_fd, SHUT_RDWR);
            ::close(client_fd);
        }
        if (accept_thread_.joinable()) {
            accept_thread_.request_stop();
            accept_thread_.join();
        }
    }

    [[nodiscard]] bool ready() const noexcept { return listen_fd_ >= 0 && port_ != 0; }
    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

    [[nodiscard]] std::optional<std::vector<std::string>> wait_for_frames(
        std::size_t count,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this, count] { return frames_.size() >= count; })) {
            return std::nullopt;
        }
        return frames_;
    }

private:
    struct Frame {
        std::uint8_t opcode = 0;
        std::string payload;
    };

    void accept_loop(std::stop_token stop) {
        while (!stop.stop_requested() && running_.load()) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (fd < 0) {
                if (!running_.load()) break;
                continue;
            }
            client_fd_.store(fd);
            handle_client(fd);
            const int owned_fd = client_fd_.exchange(-1);
            if (owned_fd >= 0) ::close(owned_fd);
            break;
        }
    }

    static std::optional<std::string> read_http_headers(int fd) {
        std::string headers;
        std::array<char, 1024> buffer{};
        while (headers.find("\r\n\r\n") == std::string::npos) {
            auto n = ::recv(fd, buffer.data(), buffer.size(), 0);
            if (n <= 0) return std::nullopt;
            headers.append(buffer.data(), static_cast<std::size_t>(n));
            if (headers.size() > 64 * 1024) return std::nullopt;
        }
        return headers;
    }

    static std::string header_value(std::string_view headers, std::string_view name) {
        auto pos = headers.find(name);
        if (pos == std::string_view::npos) return {};
        auto value_start = headers.find(':', pos);
        if (value_start == std::string_view::npos) return {};
        ++value_start;
        while (value_start < headers.size() && headers[value_start] == ' ') ++value_start;
        auto value_end = headers.find("\r\n", value_start);
        if (value_end == std::string_view::npos) return {};
        return std::string(headers.substr(value_start, value_end - value_start));
    }

    static bool read_exact(int fd, char* data, std::size_t size) {
        while (size > 0) {
            auto n = ::recv(fd, data, size, 0);
            if (n <= 0) return false;
            data += n;
            size -= static_cast<std::size_t>(n);
        }
        return true;
    }

    static std::optional<Frame> read_frame(int fd) {
        unsigned char header[2]{};
        if (!read_exact(fd, reinterpret_cast<char*>(header), 2)) return std::nullopt;

        Frame frame;
        frame.opcode = header[0] & 0x0f;
        const bool masked = (header[1] & 0x80) != 0;
        std::uint64_t len = header[1] & 0x7f;
        if (len == 126) {
            unsigned char ext[2]{};
            if (!read_exact(fd, reinterpret_cast<char*>(ext), 2)) return std::nullopt;
            len = (static_cast<std::uint64_t>(ext[0]) << 8) | ext[1];
        } else if (len == 127) {
            unsigned char ext[8]{};
            if (!read_exact(fd, reinterpret_cast<char*>(ext), 8)) return std::nullopt;
            len = 0;
            for (unsigned char byte : ext) len = (len << 8) | byte;
        }

        std::array<unsigned char, 4> mask{};
        if (masked && !read_exact(fd, reinterpret_cast<char*>(mask.data()), mask.size())) {
            return std::nullopt;
        }

        frame.payload.resize(static_cast<std::size_t>(len));
        if (len > 0 && !read_exact(fd, frame.payload.data(), frame.payload.size())) {
            return std::nullopt;
        }
        if (masked) {
            for (std::size_t i = 0; i < frame.payload.size(); ++i) {
                frame.payload[i] = static_cast<char>(frame.payload[i] ^ mask[i % 4]);
            }
        }
        return frame;
    }

    static bool send_text_frame(int fd, std::string_view payload) {
        std::string frame;
        frame.push_back(static_cast<char>(0x81));
        const auto len = payload.size();
        if (len < 126) {
            frame.push_back(static_cast<char>(len));
        } else if (len <= 0xffff) {
            frame.push_back(static_cast<char>(126));
            frame.push_back(static_cast<char>((len >> 8) & 0xff));
            frame.push_back(static_cast<char>(len & 0xff));
        } else {
            frame.push_back(static_cast<char>(127));
            for (int shift = 56; shift >= 0; shift -= 8) {
                frame.push_back(static_cast<char>((len >> shift) & 0xff));
            }
        }
        frame.append(payload);
        return bridge_test_send_all(fd, frame);
    }

    void handle_client(int fd) {
        auto headers = read_http_headers(fd);
        if (!headers) return;
        const auto key = header_value(*headers, "Sec-WebSocket-Key");
        const auto response = std::format(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: {}\r\n\r\n",
            bridge_test_accept_key(key));
        if (!bridge_test_send_all(fd, response)) return;

        bool sent_inbound = false;
        while (running_.load()) {
            auto frame = read_frame(fd);
            if (!frame) break;
            if (frame->opcode == 0x8) break;
            if (frame->opcode != 0x1 && frame->opcode != 0x2) continue;
            {
                std::lock_guard lock(mutex_);
                frames_.push_back(frame->payload);
            }
            cv_.notify_all();
            if (!sent_inbound) {
                sent_inbound = true;
                send_text_frame(fd,
                    R"({"id":"srv-1","type":"event","method":"server/pong","payload":{"ok":true},"priority":"normal"})");
            }
        }
    }

    int listen_fd_ = -1;
    std::atomic<int> client_fd_{-1};
    std::uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::jthread accept_thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::string> frames_;
};

struct LocalBridgeApiRequest {
    std::string method;
    std::string path;
    std::string headers;
    std::string body;
};

class LocalBridgeApiHttpServer {
public:
    LocalBridgeApiHttpServer() {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) return;
        int yes = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return;
        }
        if (::listen(listen_fd_, 8) != 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return;
        }

        socklen_t len = sizeof(addr);
        if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
            port_ = ntohs(addr.sin_port);
        }
        running_.store(true);
        accept_thread_ = std::jthread([this](std::stop_token stop) {
            accept_loop(stop);
        });
    }

    ~LocalBridgeApiHttpServer() {
        running_.store(false);
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
        if (accept_thread_.joinable()) {
            accept_thread_.request_stop();
            accept_thread_.join();
        }
    }

    [[nodiscard]] bool ready() const noexcept { return listen_fd_ >= 0 && port_ != 0; }
    [[nodiscard]] std::string base_url() const { return std::format("http://127.0.0.1:{}", port_); }

    [[nodiscard]] std::optional<std::vector<LocalBridgeApiRequest>> wait_for_requests(
        std::size_t count,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this, count] { return requests_.size() >= count; })) {
            return std::nullopt;
        }
        return requests_;
    }

    void fail_next_work_polls(int count, int status, std::string reason, std::string body) {
        std::lock_guard lock(mutex_);
        work_poll_failures_remaining_ = count;
        work_poll_failure_status_ = status;
        work_poll_failure_reason_ = std::move(reason);
        work_poll_failure_body_ = std::move(body);
    }

    void fail_next_heartbeats(int count, int status, std::string reason, std::string body) {
        std::lock_guard lock(mutex_);
        heartbeat_failures_remaining_ = count;
        heartbeat_failure_status_ = status;
        heartbeat_failure_reason_ = std::move(reason);
        heartbeat_failure_body_ = std::move(body);
    }

    void set_work_secret(std::string secret) {
        std::lock_guard lock(mutex_);
        work_secret_ = std::move(secret);
    }

private:
    void accept_loop(std::stop_token stop) {
        while (!stop.stop_requested() && running_.load()) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (fd < 0) {
                if (!running_.load()) break;
                continue;
            }
            handle_client(fd);
            ::close(fd);
        }
    }

    static std::optional<LocalBridgeApiRequest> read_request(int fd) {
        std::string request;
        std::array<char, 4096> buffer{};
        std::size_t header_end = std::string::npos;
        while ((header_end = request.find("\r\n\r\n")) == std::string::npos) {
            auto n = ::recv(fd, buffer.data(), buffer.size(), 0);
            if (n <= 0) return std::nullopt;
            request.append(buffer.data(), static_cast<std::size_t>(n));
            if (request.size() > 64 * 1024) return std::nullopt;
        }

        const auto headers = request.substr(0, header_end + 4);
        std::size_t content_length = 0;
        auto length_pos = headers.find("Content-Length:");
        if (length_pos == std::string::npos) length_pos = headers.find("content-length:");
        if (length_pos != std::string::npos) {
            auto value_start = headers.find(':', length_pos);
            auto value_end = headers.find("\r\n", value_start);
            if (value_start != std::string::npos && value_end != std::string::npos) {
                content_length = static_cast<std::size_t>(
                    std::stoul(headers.substr(value_start + 1, value_end - value_start - 1)));
            }
        }

        const std::size_t body_start = header_end + 4;
        while (request.size() - body_start < content_length) {
            auto n = ::recv(fd, buffer.data(), buffer.size(), 0);
            if (n <= 0) break;
            request.append(buffer.data(), static_cast<std::size_t>(n));
        }

        auto first_line_end = headers.find("\r\n");
        if (first_line_end == std::string::npos) return std::nullopt;
        std::istringstream first_line(headers.substr(0, first_line_end));
        LocalBridgeApiRequest parsed;
        first_line >> parsed.method >> parsed.path;
        parsed.headers = headers;
        parsed.body = request.substr(body_start, content_length);
        return parsed;
    }

    static bool send_response(int fd, int status, std::string_view reason, std::string_view body) {
        const auto response = std::format(
            "HTTP/1.1 {} {}\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
            status,
            reason,
            body.size(),
            body);
        return bridge_test_send_all(fd, response);
    }

    static bool send_sse_response(int fd, std::string_view body) {
        const auto response = std::format(
            "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
            body.size(),
            body);
        return bridge_test_send_all(fd, response);
    }

    [[nodiscard]] std::string work_secret() const {
        std::lock_guard lock(mutex_);
        return work_secret_;
    }

    bool consume_work_poll_failure(int& status, std::string& reason, std::string& body) {
        std::lock_guard lock(mutex_);
        if (work_poll_failures_remaining_ <= 0) return false;
        --work_poll_failures_remaining_;
        status = work_poll_failure_status_;
        reason = work_poll_failure_reason_;
        body = work_poll_failure_body_;
        return true;
    }

    bool consume_heartbeat_failure(int& status, std::string& reason, std::string& body) {
        std::lock_guard lock(mutex_);
        if (heartbeat_failures_remaining_ <= 0) return false;
        --heartbeat_failures_remaining_;
        status = heartbeat_failure_status_;
        reason = heartbeat_failure_reason_;
        body = heartbeat_failure_body_;
        return true;
    }

    void handle_client(int fd) {
        auto request = read_request(fd);
        if (!request) return;
        {
            std::lock_guard lock(mutex_);
            requests_.push_back(*request);
        }
        cv_.notify_all();

        if (request->method == "POST" && request->path == "/v1/environments/bridge") {
            send_response(fd, 201, "Created",
                R"({"environment_id":"env_backend_1","environment_secret":"env_secret_1"})");
            return;
        }
        if (request->method == "POST" && request->path == "/bridge/messages") {
            send_response(fd, 202, "Accepted", R"({"ok":true})");
            return;
        }
        if (request->method == "GET" && request->path == "/bridge/poll") {
            send_response(fd, 200, "OK",
                R"({"messages":[{"id":"poll-1","type":"event","method":"server/poll","payload":{"ok":true},"priority":"high","correlation_id":"corr-1"}]})");
            return;
        }
        if (request->method == "GET" &&
            request->path.rfind("/v1/environments/env_backend_1/work/poll", 0) == 0) {
            int status = 0;
            std::string reason;
            std::string body;
            if (consume_work_poll_failure(status, reason, body)) {
                send_response(fd, status, reason, body);
                return;
            }
        }
        if (request->method == "GET" &&
            request->path == "/v1/environments/env_backend_1/work/poll?reclaim_older_than_ms=12345") {
            send_response(fd, 200, "OK",
                std::format(
                    R"({{"id":"work_1","secret":"{}","data":{{"type":"session","id":"session_1"}}}})",
                    work_secret()));
            return;
        }
        if (request->method == "GET" && request->path == "/v1/environments/env_backend_1/work/poll") {
            send_response(fd, 200, "OK",
                std::format(
                    R"({{"id":"work_1","secret":"{}","data":{{"type":"session","id":"session_1"}}}})",
                    work_secret()));
            return;
        }
        if (request->method == "POST" && request->path == "/v1/environments/env_backend_1/work/work_1/ack") {
            send_response(fd, 200, "OK", R"({"ok":true})");
            return;
        }
        if (request->method == "POST" && request->path == "/v1/code/sessions/session_1/worker/register") {
            send_response(fd, 200, "OK", R"({"worker_epoch":"42"})");
            return;
        }
        if (request->method == "POST" && request->path == "/v1/code/sessions/session_1/worker/events") {
            send_response(fd, 201, "Created", R"({"ok":true})");
            return;
        }
        if (request->method == "GET" && request->path == "/v1/code/sessions/session_1/worker/events/stream") {
            const int count = next_sse_stream_count();
            if (count == 1) {
                send_sse_response(fd,
                    "id: 1\n"
                    "event: client_event\n"
                    "data: {\"event_id\":\"evt_daemon_product_1\",\"sequence_num\":1,\"event_type\":\"user\",\"source\":\"test\",\"created_at\":\"2026-06-07T00:00:01Z\",\"payload\":{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":\"run native daemon bridge product e2e\"},\"parent_tool_use_id\":null,\"session_id\":\"session_1\"}}\n\n");
                return;
            }
            send_sse_response(fd,
                "id: 2\n"
                "event: client_event\n"
                "data: {\"event_id\":\"evt_daemon_product_keepalive\",\"sequence_num\":2,\"event_type\":\"keep_alive\",\"source\":\"test\",\"created_at\":\"2026-06-07T00:00:02Z\",\"payload\":{\"type\":\"keep_alive\",\"session_id\":\"session_1\"}}\n\n");
            return;
        }
        if (request->method == "PUT" && request->path == "/v1/code/sessions/session_1/worker") {
            send_response(fd, 200, "OK", R"({"ok":true})");
            return;
        }
        if (request->method == "POST" && request->path == "/v1/code/sessions/session_1/worker/heartbeat") {
            send_response(fd, 200, "OK", R"({"ok":true})");
            return;
        }
        if (request->method == "POST" && request->path == "/v1/code/sessions/session_1/worker/events/delivery") {
            send_response(fd, 200, "OK", R"({"ok":true})");
            return;
        }
        if (request->method == "POST" && request->path == "/v1/messages") {
            send_response(fd, 200, "OK",
                R"({"id":"msg_bridge_daemon_product","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"native daemon bridge product reply"}],"stop_reason":"end_turn","usage":{"input_tokens":6,"output_tokens":8}})");
            return;
        }
        if (request->method == "POST" && request->path == "/v1/environments/env_backend_1/work/work_1/heartbeat") {
            int status = 0;
            std::string reason;
            std::string body;
            if (consume_heartbeat_failure(status, reason, body)) {
                send_response(fd, status, reason, body);
                return;
            }
            send_response(fd, 200, "OK",
                R"({"lease_extended":true,"state":"active","last_heartbeat":"2026-06-07T00:00:00Z","ttl_seconds":30})");
            return;
        }
        if (request->method == "POST" && request->path == "/v1/sessions/session_1/events") {
            send_response(fd, 201, "Created", R"({"ok":true})");
            return;
        }
        if (request->method == "POST" && request->path == "/v1/sessions/session_1/archive") {
            send_response(fd, 200, "OK", R"({"ok":true})");
            return;
        }
        if (request->method == "POST" && request->path == "/v1/environments/env_backend_1/work/work_1/stop") {
            send_response(fd, 200, "OK", R"({"ok":true})");
            return;
        }
        if (request->method == "DELETE" && request->path == "/v1/environments/bridge/env_backend_1") {
            send_response(fd, 204, "No Content", "");
            return;
        }
        send_response(fd, 404, "Not Found", R"({"error":"not found"})");
    }

    int next_sse_stream_count() {
        std::lock_guard lock(mutex_);
        return ++sse_stream_count_;
    }

    int listen_fd_ = -1;
    std::uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::jthread accept_thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<LocalBridgeApiRequest> requests_;
    int work_poll_failures_remaining_ = 0;
    int work_poll_failure_status_ = 500;
    std::string work_poll_failure_reason_{"Internal Server Error"};
    std::string work_poll_failure_body_{R"({"error":"server error"})"};
    int heartbeat_failures_remaining_ = 0;
    int heartbeat_failure_status_ = 500;
    std::string heartbeat_failure_reason_{"Internal Server Error"};
    std::string heartbeat_failure_body_{R"({"error":"server error"})"};
    std::string work_secret_{bridge_test_encoded_work_secret()};
    int sse_stream_count_ = 0;
};

} // namespace loom::test::bridge
