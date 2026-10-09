/// @file test_misc_services.cpp
/// @brief Service layer tests split from test_services.cpp.

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif

#include <gtest/gtest.h>
#include <httplib.h>
#include <limits>



import std;
import loom.cli.ccr_client;
import loom.cli.sse_transport;
import loom.bridge.core;
import loom.config.config;
import loom.constants.paths;
import loom.services.api.client;
import loom.services.api.errors;
import loom.services.api.session_ingress;
import loom.services.api.streaming;
import loom.services.compact.api_microcompact;
import loom.services.lsp.LSPServerManager;
import loom.services.lsp.client;
import loom.services.mcp.client;
import loom.services.mcp.auth;
import loom.services.mcp.channel_permissions;
import loom.services.mcp.config;
import loom.services.mcp.connection_manager;
import loom.services.mcp.elicitation_handler;
import loom.services.mcp.headers_helper;
import loom.services.mcp.vscode_sdk_mcp;
import loom.services.memory.sessionMemory;
import loom.services.extract_memories;
import loom.services.mcp.types;
import loom.services.mcp.xaa;
import loom.services.mcp.xaa_idp_login;
import loom.services.mcp.oauth_port;
import loom.services.rate_limit;
import loom.services.token_estimation;
import loom.services.prompt_suggestion;
import loom.server.server_routes;
import loom.server.server_main;
import loom.session.storage;
import loom.session.history;
import loom.commands.config;
import loom.commands.mcp.core_settings_loader;
import loom.orchestration.tools.mcp;
import loom.query.query_engine;
import loom.memdir.paths;
import loom.tools.agent_runtime;
import loom.tools.team;
import loom.tools.tool;
import loom.types.types;
import loom.utils.error;
import loom.services.ide_integration;
import loom.serdes.json;
import loom.teams.team_helpers;
import loom.fs.atomic_replace;
import loom.daemon.worker_registry;
import loom.server.types;

namespace fs = std::filesystem;

namespace {

struct EnvironmentGuard {
    std::string name;
    std::optional<std::string> previous;

    EnvironmentGuard(std::string key, const std::string& value) : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        setenv(name.c_str(), value.c_str(), 1);
    }

    ~EnvironmentGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

bool send_all(int fd, std::string_view data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        auto n = ::send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

std::string json_id_literal(loom::utils::json::JsonVal id) {
    if (id.is_num()) return std::to_string(id.as_int());
    if (id.is_str()) return "\"" + std::string(id.as_str()) + "\"";
    return "null";
}

struct LocalCcrHttpRequest {
    std::string method;
    std::string path;
    std::string headers;
    std::string body;
};

class LocalCcrHttpServer {
public:
    LocalCcrHttpServer() {
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

    ~LocalCcrHttpServer() {
        running_.store(false);
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
    }

    [[nodiscard]] bool ready() const noexcept { return listen_fd_ >= 0 && port_ != 0; }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    [[nodiscard]] std::optional<std::vector<LocalCcrHttpRequest>> wait_for_requests(
        std::size_t count,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this, count] { return requests_.size() >= count; })) {
            return std::nullopt;
        }
        return requests_;
    }

private:
    void accept_loop(std::stop_token stop) {
        while (!stop.stop_requested() && running_.load()) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (client_fd < 0) {
                if (!running_.load()) break;
                continue;
            }
            handle_client(client_fd);
            ::close(client_fd);
        }
    }

    static std::optional<LocalCcrHttpRequest> read_request(int fd) {
        std::string request;
        char buffer[4096];
        std::size_t header_end = std::string::npos;
        while ((header_end = request.find("\r\n\r\n")) == std::string::npos) {
            auto n = ::recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0) return std::nullopt;
            request.append(buffer, buffer + n);
            if (request.size() > 64 * 1024) return std::nullopt;
        }

        const auto headers = request.substr(0, header_end + 4);
        std::size_t content_length = 0;
        auto length_pos = headers.find("Content-Length:");
        if (length_pos == std::string::npos) {
            length_pos = headers.find("content-length:");
        }
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
            auto n = ::recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0) break;
            request.append(buffer, buffer + n);
        }

        auto first_line_end = headers.find("\r\n");
        if (first_line_end == std::string::npos) return std::nullopt;
        std::istringstream first_line(headers.substr(0, first_line_end));
        LocalCcrHttpRequest parsed;
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
        return send_all(fd, response);
    }

    static bool send_sse_response(int fd, std::string_view body) {
        const auto response = std::format(
            "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
            body.size(),
            body);
        return send_all(fd, response);
    }

    void handle_client(int fd) {
        auto request = read_request(fd);
        if (!request) return;
        {
            std::lock_guard lock(mutex_);
            requests_.push_back(*request);
        }
        cv_.notify_all();

        if (request->method == "POST" && request->path == "/api/sessions") {
            send_response(fd, 201, "Created", R"({"id":"remote-session-1"})");
            return;
        }
        if (request->method == "POST" && request->path == "/api/sessions/remote-session-1/messages") {
            send_response(fd, 200, "OK", R"({"content":"remote-ok"})");
            return;
        }
        if (request->method == "POST" && request->path == "/v1/sessions/session_1/events") {
            send_response(fd, 201, "Created", R"({"ok":true})");
            return;
        }
        if (request->method == "POST" && request->path == "/api/sessions/remote-session-1/messages?stream=true") {
            send_sse_response(fd,
                "event: message\r\n"
                "data: {\"delta\":\"one\"}\r\n"
                "\r\n"
                "data: {\"delta\":\"two\"}\r\n"
                "\r\n"
                "data: [DONE]\r\n"
                "\r\n");
            return;
        }
        if (request->method == "DELETE" && request->path == "/api/sessions/remote-session-1") {
            send_response(fd, 204, "No Content", "");
            return;
        }
        send_response(fd, 404, "Not Found", R"({"error":"not found"})");
    }

    int listen_fd_ = -1;
    std::uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::jthread accept_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<LocalCcrHttpRequest> requests_;
};

class LocalWebSocketMcpServer {
public:
    LocalWebSocketMcpServer() {
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

    ~LocalWebSocketMcpServer() {
        running_.store(false);
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
    }

    [[nodiscard]] bool ready() const noexcept { return listen_fd_ >= 0 && port_ != 0; }
    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

    [[nodiscard]] std::vector<std::string> requests() const {
        std::lock_guard lock(mutex_);
        return requests_;
    }

    [[nodiscard]] std::string handshake_headers() const {
        std::lock_guard lock(mutex_);
        return handshake_headers_;
    }

    [[nodiscard]] bool wait_for_tool_call(
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return saw_tool_call_; });
    }

private:
    void accept_loop(std::stop_token stop) {
        while (!stop.stop_requested() && running_.load()) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (client_fd < 0) {
                if (!running_.load()) break;
                continue;
            }
            handle_client(client_fd);
            ::close(client_fd);
            break;
        }
    }

    static std::optional<std::string> read_http_headers(int fd) {
        std::string request;
        char buffer[1024];
        while (request.find("\r\n\r\n") == std::string::npos) {
            auto n = ::recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0) return std::nullopt;
            request.append(buffer, buffer + n);
            if (request.size() > 64 * 1024) return std::nullopt;
        }
        return request;
    }

    struct Frame {
        std::uint8_t opcode = 0;
        std::string payload;
    };

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
        return send_all(fd, frame);
    }

    void handle_client(int fd) {
        auto headers = read_http_headers(fd);
        if (!headers) return;
        {
            std::lock_guard lock(mutex_);
            handshake_headers_ = *headers;
        }

        const std::string response =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: local-test\r\n"
            "Sec-WebSocket-Protocol: mcp\r\n\r\n";
        if (!send_all(fd, response)) return;

        while (running_.load()) {
            auto frame = read_frame(fd);
            if (!frame) break;
            if (frame->opcode == 0x8) break;
            if (frame->opcode != 0x1 && frame->opcode != 0x2) continue;

            {
                std::lock_guard lock(mutex_);
                requests_.push_back(frame->payload);
            }

            auto doc = loom::utils::json::parse(frame->payload);
            if (!doc) continue;
            auto root = doc->root();
            const auto method = std::string(root.get("method").as_str());
            auto id = root.get("id");
            if (method == "initialize") {
                auto payload = std::format(
                    R"({{"jsonrpc":"2.0","id":{},"result":{{"protocolVersion":"2024-11-05","capabilities":{{"tools":{{}}}},"serverInfo":{{"name":"ide-ws-test","version":"1.0.0"}}}}}})",
                    json_id_literal(id));
                send_text_frame(fd, payload);
            } else if (method == "tools/call") {
                const auto params = root.get("params");
                const auto name = std::string(params.get("name").as_str());
                auto payload = std::format(
                    R"({{"jsonrpc":"2.0","id":{},"result":{{"content":[{{"type":"text","text":"called:{}"}}],"isError":false}}}})",
                    json_id_literal(id),
                    name);
                send_text_frame(fd, payload);
                {
                    std::lock_guard lock(mutex_);
                    saw_tool_call_ = true;
                }
                cv_.notify_all();
                break;
            }
        }
    }

    int listen_fd_ = -1;
    std::uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::jthread accept_thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::string handshake_headers_;
    std::vector<std::string> requests_;
    bool saw_tool_call_ = false;
};

class LocalSseMcpServer {
public:
    LocalSseMcpServer() {
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

    ~LocalSseMcpServer() {
        running_.store(false);
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
        {
            std::lock_guard lock(sse_mutex_);
            if (sse_fd_ >= 0) {
                ::shutdown(sse_fd_, SHUT_RDWR);
            }
        }
        if (accept_thread_.joinable()) {
            accept_thread_.request_stop();
            accept_thread_.join();
        }
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    [[nodiscard]] bool ready() const {
        return listen_fd_ >= 0 && port_ != 0;
    }

    [[nodiscard]] std::string url() const {
        return std::format("http://127.0.0.1:{}/sse", port_);
    }

    [[nodiscard]] uint16_t port() const {
        return port_;
    }

    [[nodiscard]] std::vector<std::string> post_bodies() const {
        std::lock_guard lock(posts_mutex_);
        return post_bodies_;
    }

private:
    void accept_loop(std::stop_token stop) {
        while (!stop.stop_requested() && running_.load()) {
            int fd = ::accept(listen_fd_, nullptr, nullptr);
            if (fd < 0) {
                if (!running_.load()) return;
                continue;
            }
            workers_.emplace_back([this, fd](std::stop_token) {
                handle_connection(fd);
            });
        }
    }

    static std::string read_http_request(int fd) {
        std::string request;
        char c;
        while (request.size() < 16384) {
            if (::recv(fd, &c, 1, 0) <= 0) return request;
            request += c;
            if (request.find("\r\n\r\n") != std::string::npos) break;
        }

        const auto content_length_pos = request.find("Content-Length:");
        if (content_length_pos == std::string::npos) return request;
        auto value_start = content_length_pos + std::string_view("Content-Length:").size();
        while (value_start < request.size() && request[value_start] == ' ') ++value_start;
        auto value_end = request.find("\r\n", value_start);
        const auto body_len = static_cast<std::size_t>(std::atoi(request.substr(value_start, value_end - value_start).c_str()));
        const auto body_start = request.find("\r\n\r\n") + 4;
        while (request.size() < body_start + body_len) {
            char buf[4096];
            auto n = ::recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            request.append(buf, static_cast<std::size_t>(n));
        }
        return request;
    }

    void handle_connection(int fd) {
        auto request = read_http_request(fd);
        if (request.starts_with("GET /sse ")) {
            handle_sse(fd);
            return;
        }
        if (request.starts_with("POST /messages ")) {
            handle_post(fd, request);
            return;
        }
        send_all(fd, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        ::close(fd);
    }

    void handle_sse(int fd) {
        {
            std::lock_guard lock(sse_mutex_);
            sse_fd_ = fd;
        }
        send_all(fd,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n\r\n"
            "event: endpoint\n"
            "data: /messages\n\n");

        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }
        ::close(fd);
        {
            std::lock_guard lock(sse_mutex_);
            if (sse_fd_ == fd) sse_fd_ = -1;
        }
    }

    void handle_post(int fd, const std::string& request) {
        const auto body_start = request.find("\r\n\r\n");
        const auto body = body_start == std::string::npos ? std::string{} : request.substr(body_start + 4);
        {
            std::lock_guard lock(posts_mutex_);
            post_bodies_.push_back(body);
        }

        send_all(fd, "HTTP/1.1 202 Accepted\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        ::close(fd);

        auto parsed = loom::utils::json::parse(body);
        if (!parsed) return;
        auto root = parsed->root();
        const auto method = std::string(root.get("method").as_str());
        const auto id = json_id_literal(root.get("id"));
        if (method == "initialize") {
            send_sse_json(std::format(
                R"({{"jsonrpc":"2.0","id":{},"result":{{"protocolVersion":"2024-11-05","capabilities":{{"tools":{{}}}},"serverInfo":{{"name":"sse-fixture","version":"1.0.0"}}}}}})",
                id));
        } else if (method == "tools/list") {
            send_sse_json(std::format(
                R"({{"jsonrpc":"2.0","id":{},"result":{{"tools":[{{"name":"sse_lookup","description":"Lookup through SSE","inputSchema":{{"type":"object"}}}}]}}}})",
                id));
        } else if (method == "tools/call") {
            const auto params = root.get("params");
            const auto name = params.is_obj() ? std::string(params.get("name").as_str()) : std::string{};
            send_sse_json(std::format(
                R"({{"jsonrpc":"2.0","id":{},"result":{{"content":[{{"type":"text","text":"called:{}"}}],"isError":false}}}})",
                id,
                name));
        }
    }

    void send_sse_json(const std::string& json) {
        std::lock_guard lock(sse_mutex_);
        if (sse_fd_ < 0) return;
        send_all(sse_fd_, "data: " + json + "\n\n");
    }

    int listen_fd_ = -1;
    uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::jthread accept_thread_;
    std::vector<std::jthread> workers_;
    mutable std::mutex sse_mutex_;
    int sse_fd_ = -1;
    mutable std::mutex posts_mutex_;
    std::vector<std::string> post_bodies_;
};

loom::bridge::TokenRefreshScheduler::Params fast_refresh_params() {
    loom::bridge::TokenRefreshScheduler::Params p;
    // expires_in=1s with a 0 buffer → 5s floor in the scheduler; keep the
    // test bounded by driving expires_in near zero.
    p.refresh_buffer_ms = std::chrono::milliseconds{0};
    p.label = "test";
    return p;
}

} // namespace

TEST(CcrClient, UsesDefaultHttpTransportForRemoteSessionLifecycle) {
    LocalCcrHttpServer server;
    ASSERT_TRUE(server.ready());

    loom::cli::CcrClient client;
    auto connected = client.connect(server.base_url() + "/api", "ccr-token");
    ASSERT_TRUE(connected.has_value()) << connected.error();

    auto message = client.send_message("hello \"remote\"\nline");
    ASSERT_TRUE(message.has_value()) << message.error();
    EXPECT_EQ(*message, R"({"content":"remote-ok"})");

    auto info = client.get_session_info();
    EXPECT_EQ(info.id, "remote-session-1");
    EXPECT_EQ(info.status, "connected");
    EXPECT_EQ(info.messages_sent, 1u);
    EXPECT_EQ(info.messages_received, 1u);

    client.disconnect();

    auto requests = server.wait_for_requests(3);
    ASSERT_TRUE(requests.has_value());
    ASSERT_GE(requests->size(), 3u);

    EXPECT_EQ((*requests)[0].method, "POST");
    EXPECT_EQ((*requests)[0].path, "/api/sessions");
    EXPECT_NE((*requests)[0].headers.find("Authorization: Bearer ccr-token"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("token":"ccr-token")"), std::string::npos);

    EXPECT_EQ((*requests)[1].method, "POST");
    EXPECT_EQ((*requests)[1].path, "/api/sessions/remote-session-1/messages");
    EXPECT_NE((*requests)[1].headers.find("Authorization: Bearer ccr-token"), std::string::npos);
    EXPECT_NE((*requests)[1].body.find(R"("session_id":"remote-session-1")"), std::string::npos);
    EXPECT_NE((*requests)[1].body.find(R"(hello \"remote\"\nline)"), std::string::npos);

    EXPECT_EQ((*requests)[2].method, "DELETE");
    EXPECT_EQ((*requests)[2].path, "/api/sessions/remote-session-1");
    EXPECT_NE((*requests)[2].headers.find("Authorization: Bearer ccr-token"), std::string::npos);
}

TEST(CcrClient, StreamsMessagesThroughDefaultHttpTransport) {
    LocalCcrHttpServer server;
    ASSERT_TRUE(server.ready());

    loom::cli::CcrClient client;
    auto connected = client.connect(server.base_url() + "/api", "ccr-token");
    ASSERT_TRUE(connected.has_value()) << connected.error();

    std::vector<std::pair<std::string, bool>> chunks;
    auto streamed = client.send_message_streaming("stream \"remote\"", [&](std::string_view chunk, bool is_final) {
        chunks.emplace_back(std::string(chunk), is_final);
    });
    ASSERT_TRUE(streamed.has_value()) << streamed.error();

    ASSERT_EQ(chunks.size(), 3u);
    EXPECT_EQ(chunks[0].first, R"({"delta":"one"})");
    EXPECT_FALSE(chunks[0].second);
    EXPECT_EQ(chunks[1].first, R"({"delta":"two"})");
    EXPECT_FALSE(chunks[1].second);
    EXPECT_EQ(chunks[2].first, "[DONE]");
    EXPECT_TRUE(chunks[2].second);

    auto info = client.get_session_info();
    EXPECT_EQ(info.id, "remote-session-1");
    EXPECT_EQ(info.messages_sent, 1u);
    EXPECT_EQ(info.messages_received, 1u);

    client.disconnect();

    auto requests = server.wait_for_requests(3);
    ASSERT_TRUE(requests.has_value());
    ASSERT_GE(requests->size(), 3u);

    EXPECT_EQ((*requests)[1].method, "POST");
    EXPECT_EQ((*requests)[1].path, "/api/sessions/remote-session-1/messages?stream=true");
    EXPECT_NE((*requests)[1].headers.find("Authorization: Bearer ccr-token"), std::string::npos);
    EXPECT_NE((*requests)[1].headers.find("Accept: text/event-stream"), std::string::npos);
    EXPECT_NE((*requests)[1].body.find(R"("session_id":"remote-session-1")"), std::string::npos);
    EXPECT_NE((*requests)[1].body.find(R"(stream \"remote\")"), std::string::npos);

    EXPECT_EQ((*requests)[2].method, "DELETE");
    EXPECT_EQ((*requests)[2].path, "/api/sessions/remote-session-1");
}

TEST(CcrClient, DoesNotInventSessionWhenRemoteHandshakeFails) {
    loom::cli::CcrClient client;
    client.set_http_transport([](const loom::cli::CcrHttpRequest&)
        -> std::expected<loom::cli::CcrHttpResponse, std::string> {
        return std::unexpected("network down");
    });

    auto connected = client.connect("https://remote.example/api", "ccr-token");
    ASSERT_FALSE(connected.has_value());
    EXPECT_NE(connected.error().find("Remote session handshake failed: network down"), std::string::npos);
    EXPECT_FALSE(client.is_connected());
}

TEST(IdeIntegration, ReadsLockfileAndCallsIdeMcpTool) {
    LocalSseMcpServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto temp_home = fs::temp_directory_path() / ("loom_ide_home_" + std::to_string(suffix));
    const auto ide_dir = temp_home / ".loom" / "ide";
    fs::create_directories(ide_dir);
    EnvironmentGuard home("HOME", temp_home.string());

    const auto lockfile_path = ide_dir / (std::to_string(server.port()) + ".lock");
    {
        std::ofstream lockfile(lockfile_path);
        lockfile << std::format(
            R"({{"workspaceFolders":["{}"],"pid":{},"ideName":"VS Code","transport":"sse"}})",
            fs::current_path().string(),
            static_cast<int>(::getpid()));
    }

    loom::utils::ide::IdeLockfileScanner scanner;
    auto lockfiles = scanner.scan();
    ASSERT_EQ(lockfiles.size(), 1u);
    EXPECT_EQ(lockfiles.front().port, server.port());
    EXPECT_EQ(lockfiles.front().name, "VS Code");
    ASSERT_EQ(lockfiles.front().workspace_folders.size(), 1u);
    EXPECT_EQ(lockfiles.front().workspace_folders.front(), fs::current_path());

    auto response = loom::utils::ide::callIdeRpc(
        "openFile",
        R"({"filePath":"/tmp/example.ts","preview":false})");
    EXPECT_TRUE(response.success) << response.error.value_or(response.result);
    EXPECT_NE(response.result.find("called:openFile"), std::string::npos) << response.result;

    const auto posts = server.post_bodies();
    std::string joined;
    for (const auto& body : posts) joined += body + "\n";
    EXPECT_NE(joined.find(R"("method":"initialize")"), std::string::npos) << joined;
    EXPECT_NE(joined.find(R"("method":"tools/call")"), std::string::npos) << joined;
    EXPECT_NE(joined.find(R"("name":"openFile")"), std::string::npos) << joined;

    fs::remove_all(temp_home);
}

TEST(IdeIntegration, CallsIdeWebSocketMcpToolFromLockfile) {
    LocalWebSocketMcpServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto temp_home = fs::temp_directory_path() / ("loom_ide_ws_home_" + std::to_string(suffix));
    const auto ide_dir = temp_home / ".loom" / "ide";
    fs::create_directories(ide_dir);
    EnvironmentGuard home("HOME", temp_home.string());

    const auto lockfile_path = ide_dir / (std::to_string(server.port()) + ".lock");
    {
        std::ofstream lockfile(lockfile_path);
        lockfile << std::format(
            R"({{"workspaceFolders":["{}"],"pid":{},"ideName":"Cursor","transport":"ws","authToken":"test-token"}})",
            fs::current_path().string(),
            static_cast<int>(::getpid()));
    }

    auto response = loom::utils::ide::callIdeRpc(
        "openFile",
        R"({"filePath":"/tmp/example.ts","preview":false})");
    EXPECT_TRUE(response.success) << response.error.value_or(response.result);
    EXPECT_NE(response.result.find("called:openFile"), std::string::npos) << response.result;
    EXPECT_TRUE(server.wait_for_tool_call());

    const auto headers = server.handshake_headers();
    EXPECT_NE(headers.find("Sec-WebSocket-Protocol: mcp"), std::string::npos) << headers;
    EXPECT_NE(headers.find("X-Loom-Code-Ide-Authorization: test-token"), std::string::npos) << headers;

    const auto requests = server.requests();
    std::string joined;
    for (const auto& request : requests) joined += request + "\n";
    EXPECT_NE(joined.find(R"("method":"initialize")"), std::string::npos) << joined;
    EXPECT_NE(joined.find(R"("method":"notifications/initialized")"), std::string::npos) << joined;
    EXPECT_NE(joined.find(R"("method":"tools/call")"), std::string::npos) << joined;
    EXPECT_NE(joined.find(R"("name":"openFile")"), std::string::npos) << joined;

    fs::remove_all(temp_home);
}

TEST(IdeIntegration, DiscoversVSCodeWorkspaceMcpServersFromObjectConfig) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_vscode_mcp_workspace_" + std::to_string(suffix));
    const auto workspace = root / "workspace";
    const auto home = root / "home";
    fs::create_directories(workspace / ".vscode");
    fs::create_directories(home);
    EnvironmentGuard home_guard("HOME", home.string());

    {
        std::ofstream config(workspace / ".vscode" / "mcp.json");
        config << R"JSON({
  "servers": {
    "workspace-stdio": {
      "type": "stdio",
      "command": "node",
      "args": ["server.js"]
    },
    "workspace-sse": {
      "type": "sse",
      "url": "http://127.0.0.1:9012/sse"
    }
  },
  "mcpServers": {
    "legacy-stdio": {
      "command": "python"
    }
  }
})JSON";
    }

    auto servers = loom::services::mcp::discover_vscode_mcp_servers(workspace.string());
    auto find_server = [&servers](std::string_view name) {
        return std::find_if(servers.begin(), servers.end(), [name](const auto& server) {
            return server.name == name;
        });
    };

    auto stdio = find_server("workspace-stdio");
    ASSERT_NE(stdio, servers.end());
    EXPECT_EQ(stdio->transport_type, "stdio");
    EXPECT_EQ(stdio->connection_string, "node");
    EXPECT_TRUE(loom::services::mcp::connect_vscode_mcp(*stdio));

    auto sse = find_server("workspace-sse");
    ASSERT_NE(sse, servers.end());
    EXPECT_EQ(sse->transport_type, "sse");
    EXPECT_EQ(sse->connection_string, "http://127.0.0.1:9012/sse");
    EXPECT_TRUE(loom::services::mcp::connect_vscode_mcp(*sse));

    auto legacy = find_server("legacy-stdio");
    ASSERT_NE(legacy, servers.end());
    EXPECT_EQ(legacy->transport_type, "stdio");
    EXPECT_EQ(legacy->connection_string, "python");

    fs::remove_all(root);
}

TEST(IdeIntegration, DiscoversVSCodeExtensionContributedMcpServers) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_vscode_mcp_extension_" + std::to_string(suffix));
    const auto workspace = root / "workspace";
    const auto home = root / "home";
    const auto extension = home / ".vscode" / "extensions" / "publisher.fixture-1.0.0";
    fs::create_directories(workspace);
    fs::create_directories(extension);
    EnvironmentGuard home_guard("HOME", home.string());

    {
        std::ofstream package(extension / "package.json");
        package << R"JSON({
  "name": "fixture-extension",
  "contributes": {
    "mcp": {
      "servers": {
        "extension-stdio": {
          "command": "node"
        },
        "extension-ws": {
          "url": "ws://127.0.0.1:8020/mcp"
        }
      }
    }
  }
})JSON";
    }

    auto servers = loom::services::mcp::discover_vscode_mcp_servers(workspace.string());
    auto find_server = [&servers](std::string_view name) {
        return std::find_if(servers.begin(), servers.end(), [name](const auto& server) {
            return server.name == name;
        });
    };

    auto stdio = find_server("extension-stdio");
    ASSERT_NE(stdio, servers.end());
    EXPECT_EQ(stdio->transport_type, "stdio");
    EXPECT_EQ(stdio->connection_string, "node");
    EXPECT_TRUE(loom::services::mcp::connect_vscode_mcp(*stdio));

    auto ws = find_server("extension-ws");
    ASSERT_NE(ws, servers.end());
    EXPECT_EQ(ws->transport_type, "ws");
    EXPECT_EQ(ws->connection_string, "ws://127.0.0.1:8020/mcp");
    EXPECT_TRUE(loom::services::mcp::connect_vscode_mcp(*ws));

    fs::remove_all(root);
}

TEST(RateLimitManager, UpdatesStateFromHeadersAndWarnsNearLimits) {
    loom::services::RateLimitManager manager;
    manager.update_from_headers({
        {"x-ratelimit-remaining-requests", "3"},
        {"x-ratelimit-remaining-tokens", "9000"},
        {"retry-after", "2"},
    });

    const auto& state = manager.get_state();
    EXPECT_EQ(state.current_info.requests_remaining, 3);
    EXPECT_EQ(state.current_info.tokens_remaining, 9000);
    EXPECT_EQ(state.current_info.retry_after, std::chrono::milliseconds(2000));
    EXPECT_TRUE(manager.get_warning_message().has_value());
}

TEST(RateLimitManager, MockRateLimitControlsLimitedState) {
    loom::services::RateLimitManager manager;
    manager.mock_rate_limit({.simulate_429 = true});
    EXPECT_TRUE(manager.is_rate_limited());

    manager.clear_mock();
    EXPECT_FALSE(manager.is_rate_limited());
}

TEST(TokenEstimator, EstimatesTextImagesToolsAndModelLimits) {
    using loom::services::ImageDetail;
    using loom::services::TokenEstimator;

    EXPECT_GT(TokenEstimator::estimate_text("Hello world"), 0u);
    EXPECT_EQ(TokenEstimator::estimate_text(""), 1u);
    EXPECT_EQ(TokenEstimator::estimate_image(512, 512, ImageDetail::low), 85u);
    EXPECT_GT(TokenEstimator::estimate_tool_use("Read", R"({"file_path":"main.cpp"})"), 50u);
    EXPECT_EQ(TokenEstimator::get_model_limit("test-model"), 100000u);
    EXPECT_TRUE(TokenEstimator::fits_in_context(1000, "test-model"));
}


// ─── P2-07: WorkerRegistry + Server types smoke tests ───────────────────────

import loom.daemon.worker_registry;
import loom.server.types;


TEST(WorkerRegistry, ExpiresStale) {
    auto& r = loom::daemon::WorkerRegistry::instance();
    r.clear();

    loom::daemon::WorkerInfo w;
    w.kind = loom::daemon::WorkerKind::InProcess;
    w.hostname = "localhost";
    w.capabilities = {"query"};
    w.max_concurrent_tasks = 1;
    auto id_r = r.register_worker(std::move(w));
    ASSERT_TRUE(id_r.has_value());
    const std::string id = *id_r;

    // Advance heartbeat to a known timestamp then manually expire it.
    (void)r.heartbeat(id, 0.0, 0, 0, loom::daemon::WorkerHealth::Healthy);
    // Use a very short TTL (1ms) + a 20ms sleep so the worker is older than TTL.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const size_t expired = r.expire_stale(std::chrono::milliseconds(1));
    EXPECT_EQ(expired, 1u);
    EXPECT_FALSE(r.lookup(id).has_value());
}

TEST(WorkerRegistry, PickBest) {
    auto& r = loom::daemon::WorkerRegistry::instance();
    r.clear();

    auto mk = [&](int cur, int max, uint64_t mem_used, uint64_t mem_limit,
                  std::string suffix) -> std::string {
        loom::daemon::WorkerInfo w;
        w.kind = loom::daemon::WorkerKind::Subprocess;
        w.hostname = "host" + suffix;
        w.capabilities = {"query"};
        w.current_tasks = cur;
        w.max_concurrent_tasks = max;
        w.memory_used_bytes = mem_used;
        w.memory_limit_bytes = mem_limit;
        return *r.register_worker(std::move(w));
    };

    const std::string id_a = mk(0, 1, 100, 1000, "A");   // ratio 0/1, free mem 900
    const std::string id_b = mk(1, 4, 200, 1000, "B");   // ratio 0.25
    const std::string id_c = mk(7, 8, 50,  1000, "C");   // ratio 0.875
    (void)id_c;

    loom::daemon::WorkerQueryFilters f;
    f.capability_required = "query";
    f.min_free_tasks = 0;          // do not filter on free slots here
    f.require_heartbeat_within_ms = 0;
    auto best = r.pick_best(f);
    ASSERT_TRUE(best.has_value());
    // Lowest load ratio = id_a (0/1).  If tie, most free memory (A still wins here).
    EXPECT_EQ(best->id, id_a);
}

TEST(WorkerRegistry, Cordon) {
    auto& r = loom::daemon::WorkerRegistry::instance();
    r.clear();

    loom::daemon::WorkerInfo w;
    w.kind = loom::daemon::WorkerKind::InProcess;
    w.hostname = "cordon";
    w.capabilities = {"query"};
    w.max_concurrent_tasks = 2;
    const std::string id = *r.register_worker(std::move(w));

    loom::daemon::WorkerQueryFilters f;
    f.capability_required = "query";
    f.min_free_tasks = 0;
    f.require_heartbeat_within_ms = 0;

    f.include_cordoned = false;
    EXPECT_EQ(r.find_matching(f).size(), 1u);

    EXPECT_TRUE(r.set_cordon(id, true));
    EXPECT_EQ(r.find_matching(f).size(), 0u);

    f.include_cordoned = true;
    EXPECT_EQ(r.find_matching(f).size(), 1u);
    EXPECT_TRUE(r.find_matching(f).front().cordoned);
}


// ── Speculation suggestion engine (port of TS speculation.ts) ────────────────
// The deterministic ranker replaces the former single hardcoded suggestion.
// These tests pin the ranker's contract without a live LLM.


TEST(SpeculationSuggestion, EmptyTurnsReturnsNothing) {
    using loom::services::prompt_suggestion::rank_candidate_suggestions;
    using loom::services::prompt_suggestion::SuggestionRequest;
    SuggestionRequest req;
    EXPECT_TRUE(rank_candidate_suggestions(req).empty());
}

TEST(SpeculationSuggestion, NoAssistantTurnReturnsNothing) {
    using namespace loom::services::prompt_suggestion;
    SuggestionRequest req;
    req.recent_turns.push_back({.role = "user", .content = "implement the login flow"});
    // Early gate: needs >= 1 assistant turn before suggesting.
    EXPECT_TRUE(rank_candidate_suggestions(req).empty());
}

TEST(SpeculationSuggestion, AssistantTurnYieldsRankedCandidates) {
    using namespace loom::services::prompt_suggestion;
    SuggestionRequest req;
    req.recent_turns.push_back({.role = "user", .content = "implement the login flow"});
    req.recent_turns.push_back({.role = "assistant", .content = "I implemented the login flow with tests."});
    const auto r = rank_candidate_suggestions(req);
    EXPECT_FALSE(r.empty());
    for (const auto& s : r) {
        EXPECT_GE(s.confidence, 0.0);
        EXPECT_LE(s.confidence, 1.0);
        EXPECT_FALSE(s.text.empty());
    }
}

TEST(SpeculationSuggestion, ResultsSortedByConfidenceDesc) {
    using namespace loom::services::prompt_suggestion;
    SuggestionRequest req;
    req.recent_turns.push_back({.role = "user", .content = "refactor the module"});
    req.recent_turns.push_back({.role = "assistant", .content = "I refactored it and added tests."});
    const auto r = rank_candidate_suggestions(req);
    for (std::size_t i = 1; i < r.size(); ++i) {
        EXPECT_GE(r[i - 1].confidence, r[i].confidence);
    }
}

TEST(SpeculationSuggestion, RespectsMaxSuggestions) {
    using namespace loom::services::prompt_suggestion;
    SuggestionRequest req;
    req.max_suggestions = 2;
    req.recent_turns.push_back({.role = "user", .content = "ship the feature"});
    req.recent_turns.push_back({.role = "assistant", .content = "Shipped with full test coverage."});
    const auto r = rank_candidate_suggestions(req);
    EXPECT_LE(r.size(), 2u);
}

TEST(SpeculationSuggestion, QualityFilterRejectsEmpty) {
    using loom::services::prompt_suggestion::should_filter_suggestion;
    EXPECT_TRUE(should_filter_suggestion(""));
    EXPECT_TRUE(should_filter_suggestion("   "));
}


// ===========================================================================
// TokenRefreshScheduler — proactive refresh must carry a real token.
// Regression guard: the refresh callback used to return void, so on_refresh
// always fired with an empty OAuth token (log-only refresh).
// ===========================================================================

TEST(BridgeTokenRefresh, RefreshDeliversFetchedTokenToCallback) {
    auto params = fast_refresh_params();
    params.get_access_token_async =
        [](std::string_view) -> std::expected<std::string, std::string> {
            return std::string{"fresh-oauth-token"};
        };

    std::mutex m;
    std::condition_variable cv;
    std::string seen_session;
    std::string seen_token;
    params.on_refresh = [&](const std::string& sid, const std::string& tok) {
        std::lock_guard lock(m);
        seen_session = sid;
        seen_token = tok;
        cv.notify_one();
    };

    loom::bridge::TokenRefreshScheduler scheduler(std::move(params));
    scheduler.schedule_from_expires_in("ses_test", /*expires_in_s=*/1);
    EXPECT_TRUE(scheduler.is_scheduled());

    std::unique_lock lock(m);
    const bool fired = cv.wait_for(lock, std::chrono::seconds(20),
                                   [&] { return !seen_token.empty(); });
    ASSERT_TRUE(fired) << "refresh callback never delivered a token";
    EXPECT_EQ(seen_session, "ses_test");
    // The regression: this used to be empty.
    EXPECT_EQ(seen_token, "fresh-oauth-token");

    lock.unlock();
    scheduler.cancel_all();
}

TEST(BridgeTokenRefresh, EmptyTokenTriggersBoundedRetriesNotASilentStop) {
    auto params = fast_refresh_params();
    std::atomic<int> fetch_calls{0};
    params.get_access_token_async =
        [&](std::string_view) -> std::expected<std::string, std::string> {
            fetch_calls.fetch_add(1);
            return std::unexpected("no OAuth access token available");
        };
    std::atomic<int> refresh_calls{0};
    params.on_refresh = [&](const std::string&, const std::string&) {
        refresh_calls.fetch_add(1);
    };

    loom::bridge::TokenRefreshScheduler scheduler(std::move(params));
    scheduler.schedule_from_expires_in("ses_retry", /*expires_in_s=*/1);

    // First attempt happens ~5s in; retries are 60s apart, so within a short
    // window we expect exactly the initial fetch and NO on_refresh.
    std::this_thread::sleep_for(std::chrono::seconds(8));
    EXPECT_GE(fetch_calls.load(), 1);
    EXPECT_EQ(refresh_calls.load(), 0)
        << "a failed token fetch must not fire on_refresh";

    scheduler.cancel_all();
}

TEST(BridgeTokenRefresh, CancelAllStopsScheduledRefresh) {
    auto params = fast_refresh_params();
    params.get_access_token_async =
        [](std::string_view) -> std::expected<std::string, std::string> {
            return std::string{"tok"};
        };
    std::atomic<int> refresh_calls{0};
    params.on_refresh = [&](const std::string&, const std::string&) {
        refresh_calls.fetch_add(1);
    };

    loom::bridge::TokenRefreshScheduler scheduler(std::move(params));
    scheduler.schedule_from_expires_in("ses_cancel", /*expires_in_s=*/30);
    EXPECT_TRUE(scheduler.is_scheduled());

    scheduler.cancel_all();
    EXPECT_FALSE(scheduler.is_scheduled());

    // Well past the point a refresh would have fired.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(refresh_calls.load(), 0);
}

// End of file
