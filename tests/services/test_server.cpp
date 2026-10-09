/// @file test_server.cpp
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

struct CurrentPathGuard {
    fs::path previous;

    explicit CurrentPathGuard(const fs::path& next) : previous(fs::current_path()) {
        fs::current_path(next);
    }

    ~CurrentPathGuard() {
        std::error_code ec;
        fs::current_path(previous, ec);
    }
};

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

class LocalMessagesServer {
public:
    explicit LocalMessagesServer(
        std::vector<std::string> response_bodies = {},
        std::vector<int> response_statuses = {})
        : response_bodies_(std::move(response_bodies)),
          response_statuses_(std::move(response_statuses)) {
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

    ~LocalMessagesServer() {
        running_.store(false);
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
    }

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    [[nodiscard]] std::optional<std::string> wait_for_body(
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this] { return request_body_.has_value(); })) {
            return std::nullopt;
        }
        return request_body_;
    }

    [[nodiscard]] std::optional<std::string> wait_for_headers(
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this] { return request_headers_.has_value(); })) {
            return std::nullopt;
        }
        return request_headers_;
    }

    [[nodiscard]] std::optional<std::vector<std::string>> wait_for_bodies(
        std::size_t count,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this, count] { return request_bodies_.size() >= count; })) {
            return std::nullopt;
        }
        return request_bodies_;
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

    struct RawRequest {
        std::string headers;
        std::string body;
    };

    static RawRequest read_request(int fd) {
        std::string request;
        char buffer[4096];
        std::size_t header_end = std::string::npos;
        while ((header_end = request.find("\r\n\r\n")) == std::string::npos) {
            auto n = ::recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0) return {};
            request.append(buffer, buffer + n);
        }

        auto header = request.substr(0, header_end + 4);
        std::size_t content_length = 0;
        auto length_pos = header.find("Content-Length:");
        if (length_pos == std::string::npos) {
            length_pos = header.find("content-length:");
        }
        if (length_pos != std::string::npos) {
            auto value_start = header.find(':', length_pos);
            auto value_end = header.find("\r\n", value_start);
            if (value_start != std::string::npos && value_end != std::string::npos) {
                content_length = static_cast<std::size_t>(
                    std::stoul(header.substr(value_start + 1, value_end - value_start - 1)));
            }
        }

        const std::size_t body_start = header_end + 4;
        while (request.size() - body_start < content_length) {
            auto n = ::recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0) break;
            request.append(buffer, buffer + n);
        }
        return RawRequest{header, request.substr(body_start, content_length)};
    }

    void handle_client(int fd) {
        auto request = read_request(fd);
        auto body = request.body;
        std::size_t request_index = 0;
        {
            std::lock_guard lock(mutex_);
            if (!request_body_) request_body_ = body;
            if (!request_headers_) request_headers_ = request.headers;
            request_bodies_.push_back(body);
            request_index = request_bodies_.size() - 1;
        }
        cv_.notify_all();

        std::string response_body =
            R"({"id":"msg_test","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"ok"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})";
        if (!response_bodies_.empty()) {
            response_body = response_bodies_[std::min(request_index, response_bodies_.size() - 1)];
        }
        int response_status = 200;
        std::string response_reason = "OK";
        if (!response_statuses_.empty()) {
            response_status = response_statuses_[std::min(request_index, response_statuses_.size() - 1)];
            if (response_status == 413) {
                response_reason = "Payload Too Large";
            } else if (response_status >= 400) {
                response_reason = "Error";
            }
        }
        const auto response = std::format(
            "HTTP/1.1 {} {}\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
            response_status,
            response_reason,
            response_body.size(),
            response_body);
        send_all(fd, response);
    }

    int listen_fd_ = -1;
    std::uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::jthread accept_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<std::string> request_body_;
    std::optional<std::string> request_headers_;
    std::vector<std::string> request_bodies_;
    std::vector<std::string> response_bodies_;
    std::vector<int> response_statuses_;
};

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

std::string remote_ws_base64_encode(const unsigned char* data, std::size_t len) {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((len + 2) / 3) * 4);
    for (std::size_t i = 0; i < len; i += 3) {
        std::uint32_t n = static_cast<std::uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<std::uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<std::uint32_t>(data[i + 2]);
        result.push_back(table[(n >> 18) & 0x3f]);
        result.push_back(table[(n >> 12) & 0x3f]);
        result.push_back((i + 1 < len) ? table[(n >> 6) & 0x3f] : '=');
        result.push_back((i + 2 < len) ? table[n & 0x3f] : '=');
    }
    return result;
}

std::string remote_ws_accept_key(std::string_view key) {
    const std::string input = std::string(key) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::array<unsigned char, 20> hash{};
#ifdef __APPLE__
    CC_SHA1(input.data(), static_cast<CC_LONG>(input.size()), hash.data());
#else
    SHA1(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash.data());
#endif
    return remote_ws_base64_encode(hash.data(), hash.size());
}

struct DirectConnectHttpResponse {
    int status = 0;
    std::string body;
};

struct DirectConnectWsFrame {
    std::uint8_t opcode = 0;
    std::string payload;
};

void direct_connect_set_timeouts(int fd) {
    timeval timeout{};
    timeout.tv_sec = 3;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

int direct_connect_open_socket(std::uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    direct_connect_set_timeouts(fd);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

std::optional<DirectConnectHttpResponse> direct_connect_http_request(
    std::uint16_t port,
    std::string_view method,
    std::string_view target,
    std::string_view body = {}
) {
    const int fd = direct_connect_open_socket(port);
    if (fd < 0) return std::nullopt;

    std::ostringstream request;
    request << method << " " << target << " HTTP/1.1\r\n"
            << "Host: 127.0.0.1:" << port << "\r\n"
            << "Connection: close\r\n";
    if (!body.empty() || method == "POST") {
        request << "Content-Type: application/json\r\n"
                << "Content-Length: " << body.size() << "\r\n";
    }
    request << "\r\n" << body;
    if (!send_all(fd, request.str())) {
        ::close(fd);
        return std::nullopt;
    }

    std::string raw;
    std::array<char, 4096> buffer{};
    while (true) {
        auto n = ::recv(fd, buffer.data(), buffer.size(), 0);
        if (n <= 0) break;
        raw.append(buffer.data(), static_cast<std::size_t>(n));
    }
    ::close(fd);

    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) return std::nullopt;

    DirectConnectHttpResponse response;
    std::istringstream first_line(raw.substr(0, raw.find("\r\n")));
    std::string http_version;
    first_line >> http_version >> response.status;
    response.body = raw.substr(header_end + 4);
    return response;
}

bool direct_connect_read_exact(int fd, char* data, std::size_t size) {
    while (size > 0) {
        auto n = ::recv(fd, data, size, 0);
        if (n <= 0) return false;
        data += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

std::optional<DirectConnectWsFrame> direct_connect_read_ws_frame(int fd) {
    unsigned char header[2]{};
    if (!direct_connect_read_exact(fd, reinterpret_cast<char*>(header), 2)) return std::nullopt;

    DirectConnectWsFrame frame;
    frame.opcode = header[0] & 0x0f;
    const bool masked = (header[1] & 0x80) != 0;
    std::uint64_t len = header[1] & 0x7f;
    if (len == 126) {
        unsigned char ext[2]{};
        if (!direct_connect_read_exact(fd, reinterpret_cast<char*>(ext), 2)) return std::nullopt;
        len = (static_cast<std::uint64_t>(ext[0]) << 8) | ext[1];
    } else if (len == 127) {
        unsigned char ext[8]{};
        if (!direct_connect_read_exact(fd, reinterpret_cast<char*>(ext), 8)) return std::nullopt;
        len = 0;
        for (unsigned char byte : ext) len = (len << 8) | byte;
    }

    std::array<unsigned char, 4> mask{};
    if (masked && !direct_connect_read_exact(fd, reinterpret_cast<char*>(mask.data()), mask.size())) {
        return std::nullopt;
    }

    frame.payload.resize(static_cast<std::size_t>(len));
    if (len > 0 && !direct_connect_read_exact(fd, frame.payload.data(), frame.payload.size())) {
        return std::nullopt;
    }
    if (masked) {
        for (std::size_t i = 0; i < frame.payload.size(); ++i) {
            frame.payload[i] = static_cast<char>(frame.payload[i] ^ mask[i % 4]);
        }
    }
    return frame;
}

bool direct_connect_send_client_text_frame(int fd, std::string_view payload) {
    std::string frame;
    frame.push_back(static_cast<char>(0x81));
    const auto len = payload.size();
    if (len < 126) {
        frame.push_back(static_cast<char>(0x80 | len));
    } else if (len <= 0xffff) {
        frame.push_back(static_cast<char>(0x80 | 126));
        frame.push_back(static_cast<char>((len >> 8) & 0xff));
        frame.push_back(static_cast<char>(len & 0xff));
    } else {
        frame.push_back(static_cast<char>(0x80 | 127));
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<char>((len >> shift) & 0xff));
        }
    }

    constexpr std::array<unsigned char, 4> mask{0x11, 0x22, 0x33, 0x44};
    for (auto byte : mask) frame.push_back(static_cast<char>(byte));
    for (std::size_t i = 0; i < payload.size(); ++i) {
        frame.push_back(static_cast<char>(payload[i] ^ mask[i % mask.size()]));
    }
    return send_all(fd, frame);
}

std::optional<int> direct_connect_open_websocket(std::uint16_t port, std::string_view path) {
    const int fd = direct_connect_open_socket(port);
    if (fd < 0) return std::nullopt;

    const std::string key = "dGhlIHNhbXBsZSBub25jZQ==";
    const auto request = std::format(
        "GET {} HTTP/1.1\r\n"
        "Host: 127.0.0.1:{}\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: {}\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n",
        path,
        port,
        key);
    if (!send_all(fd, request)) {
        ::close(fd);
        return std::nullopt;
    }

    std::string headers;
    std::array<char, 1024> buffer{};
    while (headers.find("\r\n\r\n") == std::string::npos) {
        auto n = ::recv(fd, buffer.data(), buffer.size(), 0);
        if (n <= 0) {
            ::close(fd);
            return std::nullopt;
        }
        headers.append(buffer.data(), static_cast<std::size_t>(n));
        if (headers.size() > 64 * 1024) {
            ::close(fd);
            return std::nullopt;
        }
    }
    if (headers.find("101 Switching Protocols") == std::string::npos ||
        headers.find(remote_ws_accept_key(key)) == std::string::npos) {
        ::close(fd);
        return std::nullopt;
    }
    return fd;
}

std::string direct_connect_trim_json_line(std::string payload) {
    while (!payload.empty() && std::isspace(static_cast<unsigned char>(payload.back()))) {
        payload.pop_back();
    }
    return payload;
}

bool direct_connect_send_permission_response(
    int fd,
    std::string_view request_id,
    std::string_view behavior,
    std::string_view extra_response_fields_json = {}
) {
    const auto payload = std::format(
        R"({{"type":"control_response","response":{{"subtype":"success","request_id":"{}","response":{{"behavior":"{}"{}}}}}}})",
        request_id,
        behavior,
        extra_response_fields_json);
    return direct_connect_send_client_text_frame(fd, payload);
}

bool direct_connect_send_permission_error_response(
    int fd,
    std::string_view request_id,
    std::string_view error
) {
    const auto payload = std::format(
        R"({{"type":"control_response","response":{{"subtype":"error","request_id":"{}","error":"{}"}}}})",
        request_id,
        error);
    return direct_connect_send_client_text_frame(fd, payload);
}

} // namespace

TEST(ServerRoutes, MessageSessionsAndCompactUsePersistentState) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_server_routes_test_" + std::to_string(suffix));
    const auto sessions_dir = root / "sessions";
    fs::remove_all(root);
    fs::create_directories(sessions_dir);

    LocalMessagesServer server;
    ASSERT_NE(server.port(), 0);
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "direct-route-test-model");
    CurrentPathGuard cwd_guard(root);

    loom::server::reset_route_state_for_testing();
    loom::server::set_sessions_dir_for_testing(sessions_dir);
    auto routes = loom::server::get_default_routes();
    auto find_route = [&](std::string_view method, std::string_view path) -> const loom::server::Route* {
        auto it = std::ranges::find_if(routes, [&](const auto& route) {
            return route.method == method && route.path == path;
        });
        return it == routes.end() ? nullptr : &*it;
    };

    const auto* message_route = find_route("POST", "/message");
    const auto* sessions_route = find_route("GET", "/sessions");
    const auto* compact_route = find_route("POST", "/compact");
    ASSERT_NE(message_route, nullptr);
    ASSERT_NE(sessions_route, nullptr);
    ASSERT_NE(compact_route, nullptr);

    auto first_response = message_route->handler({{"content", "hello server route"}, {"model", "test-model"}});
    auto first_json = loom::utils::json::parse(first_response);
    ASSERT_TRUE(first_json.has_value()) << first_response;
    auto session_id_value = first_json->root().get("session_id");
    ASSERT_TRUE(session_id_value.is_str());
    std::string session_id(session_id_value.as_str());
    EXPECT_EQ(first_json->root().get_string("status"), "completed");
    EXPECT_EQ(first_json->root().get_string("response"), "ok");
    EXPECT_EQ(first_json->root().get_string("model"), "loom-test");

    for (int i = 0; i < 4; ++i) {
        auto response = message_route->handler({
            {"session_id", session_id},
            {"content", "follow up " + std::to_string(i)}
        });
        auto parsed = loom::utils::json::parse(response);
        ASSERT_TRUE(parsed.has_value()) << response;
        EXPECT_EQ(parsed->root().get_string("session_id"), session_id);
        EXPECT_EQ(parsed->root().get_string("response"), "ok");
    }

    auto request_bodies = server.wait_for_bodies(5);
    ASSERT_TRUE(request_bodies.has_value());
    auto last_request_json = loom::utils::json::parse(request_bodies->back());
    ASSERT_TRUE(last_request_json.has_value()) << request_bodies->back();
    auto request_messages = last_request_json->root().get("messages");
    ASSERT_TRUE(request_messages.is_arr()) << request_bodies->back();
    ASSERT_EQ(request_messages.size(), 9u) << request_bodies->back();
    std::string request_history_text;
    request_messages.iter([&](loom::utils::json::JsonVal message) {
        auto content = message.get("content");
        if (content.is_str()) {
            request_history_text += std::string(content.as_str()) + "\n";
        } else if (content.is_arr()) {
            content.iter([&](loom::utils::json::JsonVal block) {
                auto text = block.get("text");
                if (text.valid() && text.is_str()) {
                    request_history_text += std::string(text.as_str()) + "\n";
                }
            });
        }
    });
    EXPECT_NE(request_history_text.find("hello server route"), std::string::npos);
    EXPECT_NE(request_history_text.find("follow up 0"), std::string::npos);
    EXPECT_NE(request_history_text.find("follow up 3"), std::string::npos);
    EXPECT_NE(request_history_text.find("ok"), std::string::npos);

    auto sessions_response = sessions_route->handler({{"limit", "5"}});
    auto sessions_json = loom::utils::json::parse(sessions_response);
    ASSERT_TRUE(sessions_json.has_value()) << sessions_response;
    EXPECT_EQ(sessions_json->root().get("total").as_int(), 1);
    auto sessions = sessions_json->root().get("sessions");
    ASSERT_TRUE(sessions.is_arr());
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.at(0).get_string("session_id"), session_id);
    EXPECT_EQ(sessions.at(0).get("message_count").as_int(), 10);

    auto compact_response = compact_route->handler({{"session_id", session_id}});
    auto compact_json = loom::utils::json::parse(compact_response);
    ASSERT_TRUE(compact_json.has_value()) << compact_response;
    EXPECT_EQ(compact_json->root().get_string("status"), "compacted");
    EXPECT_EQ(compact_json->root().get("messages_before").as_int(), 10);
    EXPECT_EQ(compact_json->root().get("messages_after").as_int(), 7);
    EXPECT_EQ(compact_json->root().get("messages_removed").as_int(), 3);
    EXPECT_EQ(compact_json->root().get("messages_summarized").as_int(), 4);
    ASSERT_TRUE(compact_json->root().get("compact_boundary_id").is_str());

    auto metadata = loom::session::load_session_metadata(sessions_dir, session_id);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->message_count, 7);

    std::ifstream messages_file(loom::session::get_messages_path(sessions_dir, session_id));
    ASSERT_TRUE(messages_file.is_open());
    std::vector<std::string> compacted_lines;
    std::string line;
    while (std::getline(messages_file, line)) {
        if (!line.empty()) compacted_lines.push_back(line);
    }
    ASSERT_EQ(compacted_lines.size(), 7u);
    auto boundary_json = loom::utils::json::parse(compacted_lines.front());
    ASSERT_TRUE(boundary_json.has_value()) << compacted_lines.front();
    EXPECT_EQ(boundary_json->root().get_string("role"), "system");
    EXPECT_EQ(boundary_json->root().get_string("subtype"), "compact_boundary");
    EXPECT_EQ(
        boundary_json->root().get_string("id"),
        compact_json->root().get_string("compact_boundary_id"));
    auto boundary_metadata = boundary_json->root().get("compact_metadata");
    ASSERT_TRUE(boundary_metadata.is_obj());
    EXPECT_EQ(boundary_metadata.get("messages_before").as_int(), 10);
    EXPECT_EQ(boundary_metadata.get("messages_after").as_int(), 7);
    EXPECT_EQ(boundary_metadata.get("messages_removed").as_int(), 3);
    EXPECT_EQ(boundary_metadata.get("messages_summarized").as_int(), 4);
    EXPECT_EQ(boundary_metadata.get("preserved_recent_messages").as_int(), 6);
    auto boundary_content = boundary_json->root().get_string("content");
    EXPECT_NE(boundary_content.find("hello server route"), std::string::npos);
    EXPECT_NE(boundary_content.find("follow up 0"), std::string::npos);
    EXPECT_NE(boundary_content.find("Preserve these details"), std::string::npos);

	loom::server::reset_route_state_for_testing();
	fs::remove_all(root);
}

TEST(ServerRoutes, MessageRoutePublishesAssistantAndResultIngressEvents) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_server_routes_ingress_test_" + std::to_string(suffix));
    const auto sessions_dir = root / "sessions";
    fs::remove_all(root);
    fs::create_directories(sessions_dir);

    LocalMessagesServer server;
    ASSERT_NE(server.port(), 0);
    LocalCcrHttpServer ccr;
    ASSERT_TRUE(ccr.ready());

    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "direct-ingress-test-model");
    CurrentPathGuard cwd_guard(root);

    const auto now = std::chrono::system_clock::now();
    ASSERT_TRUE(loom::session::save_session_metadata(
        sessions_dir,
        loom::session::SessionMetadata{
            .session_id = "session_1",
            .model = "direct-ingress-test-model",
            .cwd = root,
            .created_at = now,
            .last_active = now,
            .message_count = 0,
            .title = std::string("Ingress route test"),
            .is_archived = false,
        }));

    loom::services::api::close_ingress();
    auto created = loom::services::api::create_ingress(loom::services::api::IngressConfig{
        .endpoint = ccr.base_url(),
        .session_id = "session_1",
        .auth_token = "session-route-token",
        .organization_uuid = std::nullopt,
    });
    ASSERT_TRUE(created.has_value()) << created.error();

    loom::server::reset_route_state_for_testing();
    loom::server::set_sessions_dir_for_testing(sessions_dir);
    auto routes = loom::server::get_default_routes();
    auto it = std::ranges::find_if(routes, [](const auto& route) {
        return route.method == "POST" && route.path == "/message";
    });
    ASSERT_NE(it, routes.end());

    auto response = it->handler({
        {"session_id", "session_1"},
        {"content", "hello ingress route"}
    });
    auto parsed = loom::utils::json::parse(response);
    ASSERT_TRUE(parsed.has_value()) << response;
    EXPECT_EQ(parsed->root().get_string("status"), "completed");
    EXPECT_EQ(parsed->root().get_string("session_id"), "session_1");
    EXPECT_EQ(parsed->root().get_string("response"), "ok");

    auto requests = ccr.wait_for_requests(2, std::chrono::seconds(5));
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 2u);
    EXPECT_EQ((*requests)[0].method, "POST");
    EXPECT_EQ((*requests)[0].path, "/v1/sessions/session_1/events");
    EXPECT_NE((*requests)[0].headers.find("Authorization: Bearer session-route-token"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("events":[)"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("type":"assistant")"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("role":"assistant")"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("session_id":"session_1")"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("text":"ok")"), std::string::npos);

    EXPECT_EQ((*requests)[1].method, "POST");
    EXPECT_EQ((*requests)[1].path, "/v1/sessions/session_1/events");
    EXPECT_NE((*requests)[1].headers.find("Authorization: Bearer session-route-token"), std::string::npos);
    EXPECT_NE((*requests)[1].body.find(R"("type":"result")"), std::string::npos);
    EXPECT_NE((*requests)[1].body.find(R"("subtype":"success")"), std::string::npos);
    EXPECT_NE((*requests)[1].body.find(R"("result":"ok")"), std::string::npos);
    EXPECT_NE((*requests)[1].body.find(R"("session_id":"session_1")"), std::string::npos);

    auto metadata = loom::session::load_session_metadata(sessions_dir, "session_1");
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->message_count, 2);

    std::ifstream messages_file(loom::session::get_messages_path(sessions_dir, "session_1"));
    ASSERT_TRUE(messages_file.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(messages_file, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_NE(lines[0].find(R"("role":"user")"), std::string::npos);
    EXPECT_NE(lines[1].find(R"("role":"assistant")"), std::string::npos);

    loom::services::api::close_ingress();
    loom::server::reset_route_state_for_testing();
    fs::remove_all(root);
}

TEST(ServerMain, DirectConnectSessionsAndWebSocketUsePersistentRoutes) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_server_main_test_" + std::to_string(suffix));
    const auto sessions_dir = root / "sessions";
    fs::remove_all(root);
    fs::create_directories(sessions_dir);

    LocalMessagesServer server;
    ASSERT_NE(server.port(), 0);
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "direct-server-test-model");
    EnvironmentGuard sessions_guard("LOOM_SERVER_SESSIONS_DIR", sessions_dir.string());
    CurrentPathGuard cwd_guard(root);
    loom::server::reset_route_state_for_testing();

    loom::server::HttpServer direct_server;
    auto started = direct_server.start(loom::server::ServerConfig{
        .port = 0,
        .host = "127.0.0.1",
        .cors = false,
        .auth_token = std::nullopt,
    });
    ASSERT_TRUE(started.has_value()) << started.error();
    const auto server_port = direct_server.get_config().port;
    ASSERT_NE(server_port, 0);

    const auto create_body = std::format(R"({{"cwd":"{}"}})", root.string());
    auto create_response = direct_connect_http_request(server_port, "POST", "/sessions", create_body);
    ASSERT_TRUE(create_response.has_value());
    ASSERT_EQ(create_response->status, 200) << create_response->body;
    auto create_json = loom::utils::json::parse(create_response->body);
    ASSERT_TRUE(create_json.has_value()) << create_response->body;
    const auto session_id = std::string(create_json->root().get_string("session_id"));
    ASSERT_FALSE(session_id.empty());
    EXPECT_NE(
        std::string(create_json->root().get_string("ws_url")).find("/sessions/ws/" + session_id),
        std::string::npos);
    EXPECT_EQ(
        std::string(create_json->root().get_string("work_dir")),
        fs::weakly_canonical(root).string());

    auto initial_metadata = loom::session::load_session_metadata(sessions_dir, session_id);
    ASSERT_TRUE(initial_metadata.has_value());
    EXPECT_EQ(initial_metadata->message_count, 0);

    auto ws_fd = direct_connect_open_websocket(server_port, "/sessions/ws/" + session_id);
    ASSERT_TRUE(ws_fd.has_value());

    const std::vector<std::string> prompts{
        "hello direct connect",
        "follow up direct connect 1",
        "follow up direct connect 2",
        "follow up direct connect 3",
        "follow up direct connect 4",
    };
    for (const auto& prompt : prompts) {
        const auto user_payload = std::format(
            R"({{"type":"user","message":{{"role":"user","content":"{}"}},"parent_tool_use_id":null,"session_id":""}})",
            prompt);
        ASSERT_TRUE(direct_connect_send_client_text_frame(*ws_fd, user_payload));

        auto assistant_frame = direct_connect_read_ws_frame(*ws_fd);
        ASSERT_TRUE(assistant_frame.has_value());
        ASSERT_EQ(assistant_frame->opcode, 0x1);
        auto assistant_json = loom::utils::json::parse(direct_connect_trim_json_line(assistant_frame->payload));
        ASSERT_TRUE(assistant_json.has_value()) << assistant_frame->payload;
        EXPECT_EQ(assistant_json->root().get_string("type"), "assistant");
        EXPECT_EQ(assistant_json->root().get_string("session_id"), session_id);
        auto assistant_content = assistant_json->root().get("message").get("content");
        ASSERT_TRUE(assistant_content.is_arr()) << assistant_frame->payload;
        ASSERT_GE(assistant_content.size(), 1u);
        EXPECT_EQ(assistant_content.at(0).get_string("text"), "ok");

        auto result_frame = direct_connect_read_ws_frame(*ws_fd);
        ASSERT_TRUE(result_frame.has_value());
        ASSERT_EQ(result_frame->opcode, 0x1);
        auto result_json = loom::utils::json::parse(direct_connect_trim_json_line(result_frame->payload));
        ASSERT_TRUE(result_json.has_value()) << result_frame->payload;
        EXPECT_EQ(result_json->root().get_string("type"), "result");
        EXPECT_EQ(result_json->root().get_string("subtype"), "success");
        EXPECT_EQ(result_json->root().get_string("result"), "ok");
        EXPECT_EQ(result_json->root().get_string("session_id"), session_id);
    }

    const std::string interrupt_payload =
        R"({"type":"control_request","request_id":"interrupt-test","request":{"subtype":"interrupt"}})";
    ASSERT_TRUE(direct_connect_send_client_text_frame(*ws_fd, interrupt_payload));
    auto control_frame = direct_connect_read_ws_frame(*ws_fd);
    ASSERT_TRUE(control_frame.has_value());
    auto control_json = loom::utils::json::parse(direct_connect_trim_json_line(control_frame->payload));
    ASSERT_TRUE(control_json.has_value()) << control_frame->payload;
    EXPECT_EQ(control_json->root().get_string("type"), "control_response");
    auto control_response = control_json->root().get("response");
    EXPECT_EQ(control_response.get_string("subtype"), "success");
    EXPECT_EQ(control_response.get_string("request_id"), "interrupt-test");

    ::shutdown(*ws_fd, SHUT_RDWR);
    ::close(*ws_fd);

    auto request_bodies = server.wait_for_bodies(prompts.size());
    ASSERT_TRUE(request_bodies.has_value());
    auto request_json = loom::utils::json::parse(request_bodies->back());
    ASSERT_TRUE(request_json.has_value()) << request_bodies->back();
    auto request_messages = request_json->root().get("messages");
    ASSERT_TRUE(request_messages.is_arr()) << request_bodies->back();
    ASSERT_EQ(request_messages.size(), 9u);
    EXPECT_NE(
        std::string(request_messages.at(0).get_string("content")).find("hello direct connect"),
        std::string::npos);
    EXPECT_NE(
        std::string(request_messages.at(8).get_string("content")).find("follow up direct connect 4"),
        std::string::npos);

    auto sessions_response = direct_connect_http_request(server_port, "GET", "/sessions?limit=5");
    ASSERT_TRUE(sessions_response.has_value());
    ASSERT_EQ(sessions_response->status, 200) << sessions_response->body;
    auto sessions_json = loom::utils::json::parse(sessions_response->body);
    ASSERT_TRUE(sessions_json.has_value()) << sessions_response->body;
    EXPECT_EQ(sessions_json->root().get("total").as_int(), 1);
    auto sessions = sessions_json->root().get("sessions");
    ASSERT_TRUE(sessions.is_arr());
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.at(0).get_string("session_id"), session_id);
    EXPECT_EQ(sessions.at(0).get("message_count").as_int(), 10);

    const auto compact_body = std::format(R"({{"session_id":"{}"}})", session_id);
    auto compact_response = direct_connect_http_request(server_port, "POST", "/compact", compact_body);
    ASSERT_TRUE(compact_response.has_value());
    ASSERT_EQ(compact_response->status, 200) << compact_response->body;
    auto compact_json = loom::utils::json::parse(compact_response->body);
    ASSERT_TRUE(compact_json.has_value()) << compact_response->body;
    EXPECT_EQ(compact_json->root().get_string("status"), "compacted");
    EXPECT_EQ(compact_json->root().get_string("session_id"), session_id);
    EXPECT_EQ(compact_json->root().get("messages_before").as_int(), 10);
    EXPECT_EQ(compact_json->root().get("messages_after").as_int(), 7);
    EXPECT_EQ(compact_json->root().get("messages_removed").as_int(), 3);
    EXPECT_EQ(compact_json->root().get("messages_summarized").as_int(), 4);
    ASSERT_TRUE(compact_json->root().get("compact_boundary_id").is_str());

    auto metadata = loom::session::load_session_metadata(sessions_dir, session_id);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->message_count, 7);

    std::ifstream messages_file(loom::session::get_messages_path(sessions_dir, session_id));
    ASSERT_TRUE(messages_file.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(messages_file, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    ASSERT_EQ(lines.size(), 7u);
    auto boundary_json = loom::utils::json::parse(lines.front());
    ASSERT_TRUE(boundary_json.has_value()) << lines.front();
    EXPECT_EQ(boundary_json->root().get_string("role"), "system");
    EXPECT_EQ(boundary_json->root().get_string("subtype"), "compact_boundary");
    EXPECT_EQ(
        boundary_json->root().get_string("id"),
        compact_json->root().get_string("compact_boundary_id"));
    auto boundary_metadata = boundary_json->root().get("compact_metadata");
    ASSERT_TRUE(boundary_metadata.is_obj());
    EXPECT_EQ(boundary_metadata.get("messages_before").as_int(), 10);
    EXPECT_EQ(boundary_metadata.get("messages_after").as_int(), 7);
    EXPECT_EQ(boundary_metadata.get("messages_removed").as_int(), 3);
    EXPECT_EQ(boundary_metadata.get("messages_summarized").as_int(), 4);
    EXPECT_EQ(boundary_metadata.get("preserved_recent_messages").as_int(), 6);
    auto boundary_content = boundary_json->root().get_string("content");
    EXPECT_NE(boundary_content.find("hello direct connect"), std::string::npos);
    EXPECT_NE(boundary_content.find("follow up direct connect 1"), std::string::npos);
    EXPECT_NE(boundary_content.find("Preserve these details"), std::string::npos);
    EXPECT_NE(lines.back().find("ok"), std::string::npos);

    direct_server.stop();
	loom::server::reset_route_state_for_testing();
	fs::remove_all(root);
}

TEST(ServerMain, DirectConnectInterruptCancelsActiveMessageRoute) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_server_interrupt_cancel_test_" + std::to_string(suffix));
    const auto sessions_dir = root / "sessions";
    fs::remove_all(root);
    fs::create_directories(sessions_dir);

    EnvironmentGuard sessions_guard("LOOM_SERVER_SESSIONS_DIR", sessions_dir.string());
    CurrentPathGuard cwd_guard(root);
    loom::server::reset_route_state_for_testing();

    std::mutex executor_mutex;
    std::condition_variable executor_cv;
    bool executor_started = false;
    std::atomic<bool> executor_saw_cancel{false};
    loom::server::set_query_executor_for_testing(
        [&](const loom::server::detail::DirectQueryRequest& request)
            -> std::expected<loom::server::detail::DirectQueryResult, std::string> {
            {
                std::lock_guard lock(executor_mutex);
                executor_started = true;
            }
            executor_cv.notify_all();

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (std::chrono::steady_clock::now() < deadline) {
                if (request.cancel_flag && request.cancel_flag->load()) {
                    executor_saw_cancel.store(true);
                    return std::unexpected(std::string("Query interrupted"));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return loom::server::detail::DirectQueryResult{
                .assistant_id = "msg_not_cancelled",
                .content = "not cancelled",
                .model = "test-model",
                .input_tokens = 1,
                .output_tokens = 1,
                .tool_rounds = 0,
                .elapsed_ms = 3000,
            };
        });

    loom::server::HttpServer direct_server;
    auto started = direct_server.start(loom::server::ServerConfig{
        .port = 0,
        .host = "127.0.0.1",
        .cors = false,
        .auth_token = std::nullopt,
    });
    ASSERT_TRUE(started.has_value()) << started.error();
    const auto server_port = direct_server.get_config().port;
    ASSERT_NE(server_port, 0);

    const auto create_body = std::format(R"({{"cwd":"{}"}})", root.string());
    auto create_response = direct_connect_http_request(server_port, "POST", "/sessions", create_body);
    ASSERT_TRUE(create_response.has_value());
    ASSERT_EQ(create_response->status, 200) << create_response->body;
    auto create_json = loom::utils::json::parse(create_response->body);
    ASSERT_TRUE(create_json.has_value()) << create_response->body;
    const auto session_id = std::string(create_json->root().get_string("session_id"));
    ASSERT_FALSE(session_id.empty());

    auto ws_fd = direct_connect_open_websocket(server_port, "/sessions/ws/" + session_id);
    ASSERT_TRUE(ws_fd.has_value());
    const auto user_payload = R"({"type":"user","message":{"role":"user","content":"start long direct route"},"parent_tool_use_id":null,"session_id":""})";
    ASSERT_TRUE(direct_connect_send_client_text_frame(*ws_fd, user_payload));

    {
        std::unique_lock lock(executor_mutex);
        ASSERT_TRUE(executor_cv.wait_for(lock, std::chrono::seconds(1), [&] {
            return executor_started;
        }));
    }

    const std::string interrupt_payload =
        R"({"type":"control_request","request_id":"active-interrupt","request":{"subtype":"interrupt"}})";
    ASSERT_TRUE(direct_connect_send_client_text_frame(*ws_fd, interrupt_payload));

    auto control_frame = direct_connect_read_ws_frame(*ws_fd);
    ASSERT_TRUE(control_frame.has_value());
    auto control_json = loom::utils::json::parse(direct_connect_trim_json_line(control_frame->payload));
    ASSERT_TRUE(control_json.has_value()) << control_frame->payload;
    EXPECT_EQ(control_json->root().get_string("type"), "control_response");
    auto control_response = control_json->root().get("response");
    EXPECT_EQ(control_response.get_string("subtype"), "success");
    EXPECT_EQ(control_response.get_string("request_id"), "active-interrupt");
    EXPECT_TRUE(control_response.get("response").get("interrupted").as_bool());

    auto error_frame = direct_connect_read_ws_frame(*ws_fd);
    ASSERT_TRUE(error_frame.has_value());
    auto error_json = loom::utils::json::parse(direct_connect_trim_json_line(error_frame->payload));
    ASSERT_TRUE(error_json.has_value()) << error_frame->payload;
    EXPECT_EQ(error_json->root().get_string("type"), "result");
    EXPECT_EQ(error_json->root().get_string("subtype"), "error_during_execution");
    EXPECT_TRUE(error_json->root().get("is_error").as_bool());
    auto errors = error_json->root().get("errors");
    ASSERT_TRUE(errors.is_arr());
    ASSERT_GE(errors.size(), 1u);
    EXPECT_NE(std::string(errors.at(0).as_str()).find("Query interrupted"), std::string::npos);
    EXPECT_TRUE(executor_saw_cancel.load());

    ::shutdown(*ws_fd, SHUT_RDWR);
    ::close(*ws_fd);
    direct_server.stop();
	loom::server::reset_route_state_for_testing();
	fs::remove_all(root);
}

TEST(ServerMain, DirectConnectPermissionControlCanAllowAndDenyToolUse) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_direct_permission_test_" + std::to_string(suffix));
    const auto sessions_dir = root / "sessions";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto allowed_file = root / "allowed.txt";
    {
        std::ofstream output(allowed_file);
        output << "permission bridge content\n";
    }
    const auto original_file = root / "original.txt";
    {
        std::ofstream output(original_file);
        output << "original input content\n";
    }
    const auto updated_file = root / "updated.txt";
    {
        std::ofstream output(updated_file);
        output << "updated permission content\n";
    }
    const auto denied_file = root / "denied.txt";
    {
        std::ofstream output(denied_file);
        output << "denied permission content\n";
    }
    const auto errored_file = root / "errored.txt";
    {
        std::ofstream output(errored_file);
        output << "errored permission content\n";
    }
    const auto allowed_dir = root / "allowed-dir";
    fs::create_directories(allowed_dir);
    const auto directory_file = allowed_dir / "directory.txt";
    {
        std::ofstream output(directory_file);
        output << "directory permission content\n";
    }

    LocalMessagesServer server({
        std::format(
            R"({{"id":"msg_read_allow_tool","type":"message","role":"assistant","model":"loom-test","content":[{{"type":"tool_use","id":"toolu_read_allow","name":"Read","input":{{"file_path":"{}"}}}}],"stop_reason":"tool_use","usage":{{"input_tokens":1,"output_tokens":1}}}})",
            allowed_file.string()),
        R"({"id":"msg_read_allow_done","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"read allowed"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})",
        std::format(
            R"({{"id":"msg_read_update_tool","type":"message","role":"assistant","model":"loom-test","content":[{{"type":"tool_use","id":"toolu_read_update","name":"Read","input":{{"file_path":"{}"}}}}],"stop_reason":"tool_use","usage":{{"input_tokens":1,"output_tokens":1}}}})",
            original_file.string()),
        R"({"id":"msg_read_update_done","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"read updated"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})",
        std::format(
            R"({{"id":"msg_read_cached_tool","type":"message","role":"assistant","model":"loom-test","content":[{{"type":"tool_use","id":"toolu_read_cached","name":"Read","input":{{"file_path":"{}"}}}}],"stop_reason":"tool_use","usage":{{"input_tokens":1,"output_tokens":1}}}})",
            original_file.string()),
        R"({"id":"msg_read_cached_done","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"read cached"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})",
        std::format(
            R"({{"id":"msg_read_directory_tool","type":"message","role":"assistant","model":"loom-test","content":[{{"type":"tool_use","id":"toolu_read_directory","name":"Read","input":{{"file_path":"{}"}}}}],"stop_reason":"tool_use","usage":{{"input_tokens":1,"output_tokens":1}}}})",
            directory_file.string()),
        R"({"id":"msg_read_directory_done","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"read directory cached"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})",
        std::format(
            R"({{"id":"msg_read_deny_tool","type":"message","role":"assistant","model":"loom-test","content":[{{"type":"tool_use","id":"toolu_read_deny","name":"Read","input":{{"file_path":"{}"}}}}],"stop_reason":"tool_use","usage":{{"input_tokens":1,"output_tokens":1}}}})",
            denied_file.string()),
        R"({"id":"msg_read_deny_done","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"read denied"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})",
        std::format(
            R"({{"id":"msg_read_error_tool","type":"message","role":"assistant","model":"loom-test","content":[{{"type":"tool_use","id":"toolu_read_error","name":"Read","input":{{"file_path":"{}"}}}}],"stop_reason":"tool_use","usage":{{"input_tokens":1,"output_tokens":1}}}})",
            errored_file.string()),
        R"({"id":"msg_read_error_done","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"read permission error"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})",
    });
    ASSERT_NE(server.port(), 0);

    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard sessions_guard("LOOM_SERVER_SESSIONS_DIR", sessions_dir.string());
    CurrentPathGuard cwd_guard(root);
    loom::server::reset_route_state_for_testing();

    loom::server::HttpServer direct_server;
    auto started = direct_server.start(loom::server::ServerConfig{
        .port = 0,
        .host = "127.0.0.1",
        .cors = false,
        .auth_token = std::nullopt,
    });
    ASSERT_TRUE(started.has_value()) << started.error();
    const auto server_port = direct_server.get_config().port;
    ASSERT_NE(server_port, 0);

    const auto create_body = std::format(R"({{"cwd":"{}"}})", root.string());
    auto create_response = direct_connect_http_request(server_port, "POST", "/sessions", create_body);
    ASSERT_TRUE(create_response.has_value());
    ASSERT_EQ(create_response->status, 200) << create_response->body;
    auto create_json = loom::utils::json::parse(create_response->body);
    ASSERT_TRUE(create_json.has_value()) << create_response->body;
    const auto session_id = std::string(create_json->root().get_string("session_id"));
    ASSERT_FALSE(session_id.empty());

    auto ws_fd = direct_connect_open_websocket(server_port, "/sessions/ws/" + session_id);
    ASSERT_TRUE(ws_fd.has_value());

    auto send_prompt_with_permission = [&](std::string_view prompt,
                                           std::string_view behavior,
                                           std::string_view expected_tool_use_id,
                                           std::string_view expected_file_path,
                                           std::string_view expected_text,
                                           std::string_view extra_response_fields_or_error_message = {}) {
        const auto user_payload = std::format(
            R"({{"type":"user","message":{{"role":"user","content":"{}"}},"parent_tool_use_id":null,"session_id":""}})",
            prompt);
        ASSERT_TRUE(direct_connect_send_client_text_frame(*ws_fd, user_payload));

        std::optional<loom::utils::json::JsonDoc> permission_json;
        for (int attempt = 0; attempt < 4; ++attempt) {
            auto permission_frame = direct_connect_read_ws_frame(*ws_fd);
            ASSERT_TRUE(permission_frame.has_value());
            auto parsed = loom::utils::json::parse(direct_connect_trim_json_line(permission_frame->payload));
            ASSERT_TRUE(parsed.has_value()) << permission_frame->payload;
            if (parsed->root().get_string("type") == "control_request") {
                permission_json.emplace(std::move(*parsed));
                break;
            }
        }
        ASSERT_TRUE(permission_json.has_value());
        EXPECT_EQ(permission_json->root().get_string("type"), "control_request");
        ASSERT_TRUE(permission_json->root().get("request_id").is_str());
        const auto request_id = std::string(permission_json->root().get_string("request_id"));
        auto request = permission_json->root().get("request");
        ASSERT_TRUE(request.is_obj());
        EXPECT_EQ(request.get_string("subtype"), "can_use_tool");
        EXPECT_EQ(request.get_string("tool_name"), "Read");
        EXPECT_EQ(request.get_string("tool_use_id"), expected_tool_use_id);
        EXPECT_EQ(request.get("input").get_string("file_path"), expected_file_path);
        if (behavior == std::string_view("error")) {
            ASSERT_TRUE(direct_connect_send_permission_error_response(
                *ws_fd,
                request_id,
                extra_response_fields_or_error_message));
        } else {
            ASSERT_TRUE(direct_connect_send_permission_response(
                *ws_fd,
                request_id,
                behavior,
                extra_response_fields_or_error_message));
        }

        auto assistant_frame = direct_connect_read_ws_frame(*ws_fd);
        ASSERT_TRUE(assistant_frame.has_value());
        auto assistant_json = loom::utils::json::parse(direct_connect_trim_json_line(assistant_frame->payload));
        ASSERT_TRUE(assistant_json.has_value()) << assistant_frame->payload;
        EXPECT_EQ(assistant_json->root().get_string("type"), "assistant");
        auto assistant_content = assistant_json->root().get("message").get("content");
        ASSERT_TRUE(assistant_content.is_arr()) << assistant_frame->payload;
        ASSERT_GE(assistant_content.size(), 1u);
        EXPECT_EQ(assistant_content.at(0).get_string("text"), expected_text);

        auto result_frame = direct_connect_read_ws_frame(*ws_fd);
        ASSERT_TRUE(result_frame.has_value());
        auto result_json = loom::utils::json::parse(direct_connect_trim_json_line(result_frame->payload));
        ASSERT_TRUE(result_json.has_value()) << result_frame->payload;
        EXPECT_EQ(result_json->root().get_string("type"), "result");
        EXPECT_EQ(result_json->root().get_string("subtype"), "success");
	        EXPECT_EQ(result_json->root().get_string("result"), expected_text);
	    };

    auto send_prompt_without_permission = [&](std::string_view prompt,
                                              std::string_view expected_text) {
        const auto user_payload = std::format(
            R"({{"type":"user","message":{{"role":"user","content":"{}"}},"parent_tool_use_id":null,"session_id":""}})",
            prompt);
        ASSERT_TRUE(direct_connect_send_client_text_frame(*ws_fd, user_payload));

        auto assistant_frame = direct_connect_read_ws_frame(*ws_fd);
        ASSERT_TRUE(assistant_frame.has_value());
        auto assistant_json = loom::utils::json::parse(direct_connect_trim_json_line(assistant_frame->payload));
        ASSERT_TRUE(assistant_json.has_value()) << assistant_frame->payload;
        EXPECT_EQ(assistant_json->root().get_string("type"), "assistant");
        auto assistant_content = assistant_json->root().get("message").get("content");
        ASSERT_TRUE(assistant_content.is_arr()) << assistant_frame->payload;
        ASSERT_GE(assistant_content.size(), 1u);
        EXPECT_EQ(assistant_content.at(0).get_string("text"), expected_text);

        auto result_frame = direct_connect_read_ws_frame(*ws_fd);
        ASSERT_TRUE(result_frame.has_value());
        auto result_json = loom::utils::json::parse(direct_connect_trim_json_line(result_frame->payload));
        ASSERT_TRUE(result_json.has_value()) << result_frame->payload;
        EXPECT_EQ(result_json->root().get_string("type"), "result");
        EXPECT_EQ(result_json->root().get_string("subtype"), "success");
        EXPECT_EQ(result_json->root().get_string("result"), expected_text);
    };

    send_prompt_with_permission(
        "read with permission allow",
        "allow",
        "toolu_read_allow",
        allowed_file.string(),
        "read allowed");
    send_prompt_with_permission(
        "read with permission updated input",
        "allow",
        "toolu_read_update",
        original_file.string(),
	        "read updated",
	        std::format(
	            R"(,"updatedInput":{{"file_path":"{}"}},"updatedPermissions":[{{"type":"addRules","rules":[{{"toolName":"Read","ruleContent":"{}"}}],"behavior":"allow","destination":"session"}},{{"type":"addDirectories","directories":["{}"],"destination":"session"}}])",
	            updated_file.string(),
	            original_file.string(),
	            allowed_dir.string()));
	    send_prompt_without_permission("read with cached permission update", "read cached");
    send_prompt_without_permission("read with cached directory permission", "read directory cached");
	    send_prompt_with_permission(
	        "read with permission deny",
        "deny",
        "toolu_read_deny",
        denied_file.string(),
        "read denied",
        R"(,"message":"denied by direct permission test")");
    send_prompt_with_permission(
        "read with permission error response",
        "error",
        "toolu_read_error",
        errored_file.string(),
        "read permission error",
        "client callback failed direct permission test");

    auto request_bodies = server.wait_for_bodies(12);
    ASSERT_TRUE(request_bodies.has_value());
    ASSERT_EQ(request_bodies->size(), 12u);
    EXPECT_NE((*request_bodies)[1].find("permission bridge content"), std::string::npos);
    EXPECT_NE((*request_bodies)[3].find("updated permission content"), std::string::npos);
    EXPECT_EQ((*request_bodies)[3].find("original input content"), std::string::npos);
    EXPECT_NE((*request_bodies)[5].find("original input content"), std::string::npos);
    EXPECT_NE((*request_bodies)[7].find("directory permission content"), std::string::npos);
    EXPECT_NE((*request_bodies)[9].find("denied by direct permission test"), std::string::npos);
    EXPECT_EQ((*request_bodies)[9].find("Permission denied for tool: Read"), std::string::npos);
    EXPECT_NE((*request_bodies)[11].find("client callback failed direct permission test"), std::string::npos);
    EXPECT_EQ((*request_bodies)[11].find("Permission denied for tool: Read"), std::string::npos);

    ::shutdown(*ws_fd, SHUT_RDWR);
    ::close(*ws_fd);
    direct_server.stop();
    loom::server::reset_route_state_for_testing();
    fs::remove_all(root);
}

TEST(ServerMain, DirectConnectToolLoopPersistsTeamCreateAndSendMessage) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_direct_team_tool_test_" + std::to_string(suffix));
    const auto sessions_dir = root / "sessions";
    fs::remove_all(root);
    fs::create_directories(root);

    LocalMessagesServer server({
        R"({"id":"msg_team_create_tool","type":"message","role":"assistant","model":"loom-test","content":[{"type":"tool_use","id":"toolu_team_create","name":"team_create","input":{"team_id":"direct-team-id","team_name":"Direct Team","members":[{"agent_id":"reviewer-one","role":"reviewer"},{"agent_id":"researcher-one","role":"worker"}],"task_list":[{"id":"direct-task","description":"Inspect direct connect team migration","assigned_to":"reviewer-one"}]}}],"stop_reason":"tool_use","usage":{"input_tokens":1,"output_tokens":1}})",
        R"({"id":"msg_team_create_done","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"direct team created"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})",
        R"({"id":"msg_send_message_tool","type":"message","role":"assistant","model":"loom-test","content":[{"type":"tool_use","id":"toolu_send_message","name":"send_message","input":{"target_agent":"reviewer-one","team_name":"Direct Team","content":"Please review direct connect team output","summary":"direct team follow-up"}}],"stop_reason":"tool_use","usage":{"input_tokens":1,"output_tokens":1}})",
        R"({"id":"msg_send_message_done","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"direct team message delivered"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})",
    });
    ASSERT_NE(server.port(), 0);

    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard sessions_guard("LOOM_SERVER_SESSIONS_DIR", sessions_dir.string());
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    CurrentPathGuard cwd_guard(root);
    loom::server::reset_route_state_for_testing();
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::server::HttpServer direct_server;
    auto started = direct_server.start(loom::server::ServerConfig{
        .port = 0,
        .host = "127.0.0.1",
        .cors = false,
        .auth_token = std::nullopt,
    });
    ASSERT_TRUE(started.has_value()) << started.error();
    const auto server_port = direct_server.get_config().port;
    ASSERT_NE(server_port, 0);

    const auto create_body = std::format(R"({{"cwd":"{}"}})", root.string());
    auto create_response = direct_connect_http_request(server_port, "POST", "/sessions", create_body);
    ASSERT_TRUE(create_response.has_value());
    ASSERT_EQ(create_response->status, 200) << create_response->body;
    auto create_json = loom::utils::json::parse(create_response->body);
    ASSERT_TRUE(create_json.has_value()) << create_response->body;
    const auto session_id = std::string(create_json->root().get_string("session_id"));
    ASSERT_FALSE(session_id.empty());

    auto ws_fd = direct_connect_open_websocket(server_port, "/sessions/ws/" + session_id);
    ASSERT_TRUE(ws_fd.has_value());

    auto read_non_permission_frame = [&]() -> std::optional<DirectConnectWsFrame> {
        while (true) {
            auto frame = direct_connect_read_ws_frame(*ws_fd);
            if (!frame) return std::nullopt;
            auto parsed = loom::utils::json::parse(direct_connect_trim_json_line(frame->payload));
            if (!parsed || !parsed->root().is_obj()) return frame;
            if (parsed->root().get_string("type") == "control_request") {
                auto request_id_value = parsed->root().get("request_id");
                auto request = parsed->root().get("request");
                if (request_id_value.is_str() &&
                    request.is_obj() &&
                    request.get_string("subtype") == "can_use_tool") {
                    EXPECT_TRUE(direct_connect_send_permission_response(
                        *ws_fd,
                        std::string(request_id_value.as_str()),
                        "allow"));
                    continue;
                }
            }
            return frame;
        }
    };

    auto send_user_prompt = [&](std::string_view prompt, std::string_view expected_text) {
        const auto user_payload = std::format(
            R"({{"type":"user","message":{{"role":"user","content":"{}"}},"parent_tool_use_id":null,"session_id":""}})",
            prompt);
        ASSERT_TRUE(direct_connect_send_client_text_frame(*ws_fd, user_payload));

        auto assistant_frame = read_non_permission_frame();
        ASSERT_TRUE(assistant_frame.has_value());
        auto assistant_json = loom::utils::json::parse(direct_connect_trim_json_line(assistant_frame->payload));
        ASSERT_TRUE(assistant_json.has_value()) << assistant_frame->payload;
        EXPECT_EQ(assistant_json->root().get_string("type"), "assistant");
        EXPECT_EQ(assistant_json->root().get_string("session_id"), session_id);
        auto assistant_content = assistant_json->root().get("message").get("content");
        ASSERT_TRUE(assistant_content.is_arr()) << assistant_frame->payload;
        ASSERT_GE(assistant_content.size(), 1u);
        EXPECT_EQ(assistant_content.at(0).get_string("text"), expected_text);

        auto result_frame = read_non_permission_frame();
        ASSERT_TRUE(result_frame.has_value());
        auto result_json = loom::utils::json::parse(direct_connect_trim_json_line(result_frame->payload));
        ASSERT_TRUE(result_json.has_value()) << result_frame->payload;
        EXPECT_EQ(result_json->root().get_string("type"), "result");
        EXPECT_EQ(result_json->root().get_string("subtype"), "success");
        EXPECT_EQ(result_json->root().get_string("result"), expected_text);
    };

    send_user_prompt("create a direct connect team", "direct team created");
    send_user_prompt("message reviewer-one on the direct team", "direct team message delivered");

    ::shutdown(*ws_fd, SHUT_RDWR);
    ::close(*ws_fd);

    auto request_bodies = server.wait_for_bodies(4);
    ASSERT_TRUE(request_bodies.has_value());
    ASSERT_EQ(request_bodies->size(), 4u);
    auto first_request = loom::utils::json::parse(request_bodies->front());
    ASSERT_TRUE(first_request.has_value()) << request_bodies->front();
    auto tools = first_request->root().get("tools");
    ASSERT_TRUE(tools.is_arr()) << request_bodies->front();
    bool exposed_team_create = false;
    bool exposed_send_message = false;
    tools.iter([&](loom::utils::json::JsonVal tool) {
        const auto name = std::string(tool.get_string("name"));
        if (name == "team_create") exposed_team_create = true;
        if (name == "send_message") exposed_send_message = true;
    });
    EXPECT_TRUE(exposed_team_create);
    EXPECT_TRUE(exposed_send_message);

    auto team = loom::tools::global_team_store().get("direct-team-id");
    ASSERT_TRUE(team.has_value()) << std::string(loom::tools::format_error(team.error()));
    EXPECT_EQ((*team)->name, "Direct Team");
    ASSERT_EQ((*team)->members.size(), 2u);
    ASSERT_EQ((*team)->task_list.size(), 1u);
    EXPECT_EQ((*team)->task_list.front().id, "direct-task");
    ASSERT_TRUE((*team)->task_list.front().assigned_to.has_value());
    EXPECT_EQ(*(*team)->task_list.front().assigned_to, "reviewer-one");

    const auto team_dir = root / "teams" / "direct-team";
    EXPECT_TRUE(fs::exists(root / "teams" / "direct-team-id.json"));
    EXPECT_TRUE(fs::exists(team_dir / "config.json"));
    EXPECT_TRUE(fs::exists(team_dir / "tasks.json"));
    EXPECT_TRUE(fs::exists(team_dir / "inboxes" / "reviewer-one.json"));

    auto reviewer_record = loom::tools::agent_runtime::native_agent_store().get("reviewer-one");
    ASSERT_TRUE(reviewer_record.has_value());
    ASSERT_TRUE(reviewer_record->team_name.has_value());
    EXPECT_EQ(*reviewer_record->team_name, "Direct Team");
    ASSERT_EQ(reviewer_record->pending_messages.size(), 2u);
    EXPECT_NE(reviewer_record->pending_messages.front().find("Inspect direct connect team migration"), std::string::npos);
    EXPECT_NE(reviewer_record->pending_messages.back().find("Please review direct connect team output"), std::string::npos);

    auto inbox = loom::utils::read_inbox("reviewer-one", std::optional<std::string_view>{"Direct Team"});
    ASSERT_TRUE(inbox.has_value()) << inbox.error();
    ASSERT_EQ(inbox->size(), 1u);
    EXPECT_EQ(inbox->front().from, "team-lead");
    EXPECT_EQ(inbox->front().text, "Please review direct connect team output");
    ASSERT_TRUE(inbox->front().summary.has_value());
    EXPECT_EQ(*inbox->front().summary, "direct team follow-up");

    auto metadata = loom::session::load_session_metadata(sessions_dir, session_id);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->message_count, 4);

    direct_server.stop();
    loom::server::reset_route_state_for_testing();
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(ServerTypes, RoundtripSerde) {
    loom::server::ServerSession s;
    s.id = "session-1";
    s.token = "tok-abcdef";
    s.role = loom::server::Role::Admin;
    s.user_id = "u-42";
    s.user_agent = "test-agent/1.0";
    s.client_ip = "127.0.0.1";
    s.scopes = {"read", "write", "query"};
    s.created_ms = 1'000'000;
    s.expires_ms = 2'000'000;
    s.last_active_ms = 1'500'000;
    s.request_count = 17;
    s.revoked = false;

    const std::string json = loom::server::to_json(s);
    auto parsed = loom::server::ServerSession_from_json(json);
    ASSERT_TRUE(parsed.has_value()) << "parse error: " << (parsed.has_value() ? std::string{} : parsed.error());
    const auto& p = *parsed;
    EXPECT_EQ(p.id, s.id);
    EXPECT_EQ(p.token, s.token);
    EXPECT_EQ(p.role, s.role);
    EXPECT_EQ(p.user_id, s.user_id);
    EXPECT_EQ(p.user_agent, s.user_agent);
    EXPECT_EQ(p.client_ip, s.client_ip);
    EXPECT_EQ(p.scopes, s.scopes);
    EXPECT_EQ(p.created_ms, s.created_ms);
    EXPECT_EQ(p.expires_ms, s.expires_ms);
    EXPECT_EQ(p.last_active_ms, s.last_active_ms);
    EXPECT_EQ(p.request_count, s.request_count);
    EXPECT_EQ(p.revoked, s.revoked);
}

TEST(ServerTypes, RolesScopes) {
    loom::server::ServerSession s;
    s.role = loom::server::Role::Admin;
    s.scopes = {"read", "write"};
    s.expires_ms = 0;   // never expires
    s.revoked = false;

    EXPECT_TRUE(s.has_scope("read"));
    EXPECT_FALSE(s.has_scope("delete"));
    // Trivially satisfied empty scope query.
    EXPECT_TRUE(s.has_scope(""));
    // Not revoked and no expiry wall clock → not expired.
    EXPECT_FALSE(s.is_expired());
    // Manually inject a "now" that is in the future after a hypothetical expire_ms.
    s.expires_ms = 1000;
    EXPECT_TRUE(s.is_expired(2000));
    EXPECT_FALSE(s.is_expired(500));
    // Revoked always-expired semantics.
    s.revoked = true;
    EXPECT_TRUE(s.is_expired());
}

// End of file
