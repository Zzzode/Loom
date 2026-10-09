/// @file test_mcp_client.cpp
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

class LocalReconnectSseStreamServer {
public:
    LocalReconnectSseStreamServer() {
        server_.Get("/sse", [this](const httplib::Request& req, httplib::Response& res) {
            {
                std::lock_guard lock(mutex_);
                stream_last_event_ids_.push_back(req.get_header_value("Last-Event-ID"));
            }
            cv_.notify_all();
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "close");
            res.set_content(
                "id: 1\n"
                "event: endpoint\n"
                "data: /messages\n\n",
                "text/event-stream");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalReconnectSseStreamServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool ready() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string url() const {
        return std::format("http://127.0.0.1:{}/sse", port_);
    }

    [[nodiscard]] std::vector<std::string> stream_last_event_ids() const {
        std::lock_guard lock(mutex_);
        return stream_last_event_ids_;
    }

    [[nodiscard]] bool wait_for_stream_requests(
        std::size_t count,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this, count] {
            return stream_last_event_ids_.size() >= count;
        });
    }

private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::string> stream_last_event_ids_;
};

class LocalStreamableHttpMcpServer {
public:
    LocalStreamableHttpMcpServer() {
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

    ~LocalStreamableHttpMcpServer() {
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
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    [[nodiscard]] bool ready() const {
        return listen_fd_ >= 0 && port_ != 0;
    }

    [[nodiscard]] std::string url() const {
        return std::format("http://127.0.0.1:{}/mcp", port_);
    }

    [[nodiscard]] std::vector<std::string> requests() const {
        std::lock_guard lock(requests_mutex_);
        return requests_;
    }

    [[nodiscard]] std::vector<std::string> post_bodies() const {
        std::lock_guard lock(requests_mutex_);
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
        if (request.starts_with("POST /mcp ")) {
            handle_post(fd, request);
            return;
        }
        send_all(fd, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        ::close(fd);
    }

    void handle_post(int fd, const std::string& request) {
        const auto body_start = request.find("\r\n\r\n");
        const auto body = body_start == std::string::npos ? std::string{} : request.substr(body_start + 4);
        {
            std::lock_guard lock(requests_mutex_);
            requests_.push_back(request);
            post_bodies_.push_back(body);
        }

        auto parsed = loom::utils::json::parse(body);
        if (!parsed) {
            send_empty(fd, "400 Bad Request");
            return;
        }

        auto root = parsed->root();
        const auto method = std::string(root.get("method").as_str());
        if (method == "notifications/initialized" || method == "notifications/cancelled") {
            send_empty(fd, "202 Accepted");
            return;
        }

        const auto id = json_id_literal(root.get("id"));
        if (method == "initialize") {
            send_json(fd, std::format(
                R"({{"jsonrpc":"2.0","id":{},"result":{{"protocolVersion":"2024-11-05","capabilities":{{"tools":{{}}}},"serverInfo":{{"name":"http-fixture","version":"1.0.0"}}}}}})",
                id));
            return;
        }
        if (method == "tools/list") {
            send_json(fd, std::format(
                R"({{"jsonrpc":"2.0","id":{},"result":{{"tools":[{{"name":"http_lookup","description":"Lookup through HTTP","inputSchema":{{"type":"object"}}}}]}}}})",
                id));
            return;
        }

        send_empty(fd, "404 Not Found");
    }

    static void send_empty(int fd, std::string_view status) {
        send_all(fd, std::format(
            "HTTP/1.1 {}\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
            status));
        ::close(fd);
    }

    static void send_json(int fd, const std::string& body) {
        send_all(fd, std::format(
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
            body.size(),
            body));
        ::close(fd);
    }

    int listen_fd_ = -1;
    uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::jthread accept_thread_;
    std::vector<std::jthread> workers_;
    mutable std::mutex requests_mutex_;
    std::vector<std::string> requests_;
    std::vector<std::string> post_bodies_;
};

class LocalUnauthorizedStreamableHttpMcpServer {
public:
    LocalUnauthorizedStreamableHttpMcpServer() {
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

    ~LocalUnauthorizedStreamableHttpMcpServer() {
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
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    [[nodiscard]] bool ready() const {
        return listen_fd_ >= 0 && port_ != 0;
    }

    [[nodiscard]] std::string url() const {
        return std::format("http://127.0.0.1:{}/mcp", port_);
    }

    [[nodiscard]] std::vector<std::string> requests() const {
        std::lock_guard lock(requests_mutex_);
        return requests_;
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
        const auto value_end = request.find("\r\n", value_start);
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
        {
            std::lock_guard lock(requests_mutex_);
            requests_.push_back(std::move(request));
        }
        send_all(fd,
            "HTTP/1.1 401 Unauthorized\r\n"
            "WWW-Authenticate: Bearer realm=\"mcp\"\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n");
        ::close(fd);
    }

    int listen_fd_ = -1;
    uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::jthread accept_thread_;
    std::vector<std::jthread> workers_;
    mutable std::mutex requests_mutex_;
    std::vector<std::string> requests_;
};

class LocalRefreshingStreamableHttpMcpServer {
public:
    explicit LocalRefreshingStreamableHttpMcpServer(bool fail_refresh = false)
        : fail_refresh_(fail_refresh) {
        server_.Get("/metadata", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(std::format(R"({{
              "authorization_endpoint": "{0}/authorize",
              "token_endpoint": "{0}/token",
              "scope": "tools"
            }})", base_url()), "application/json");
        });

        server_.Post("/token", [this](const httplib::Request& req, httplib::Response& res) {
            {
                std::lock_guard lock(mutex_);
                token_request_body_ = req.body;
                ++token_request_count_;
            }
            cv_.notify_all();
            if (req.body.find("grant_type=refresh_token") == std::string::npos ||
                req.body.find("refresh_token=old-refresh") == std::string::npos ||
                req.body.find("client_id=client-1") == std::string::npos) {
                res.status = 400;
                res.set_content(R"({"error":"invalid_request"})", "application/json");
                return;
            }
            if (fail_refresh_) {
                res.status = 400;
                res.set_content(R"({"error":"invalid_grant"})", "application/json");
                return;
            }
            res.set_content(R"({
              "access_token": "fresh-access",
              "refresh_token": "fresh-refresh",
              "expires_in": 3600,
              "scope": "tools"
            })", "application/json");
        });

        server_.Post("/mcp", [this](const httplib::Request& req, httplib::Response& res) {
            {
                std::lock_guard lock(mutex_);
                mcp_authorization_headers_.push_back(req.get_header_value("Authorization"));
                mcp_request_bodies_.push_back(req.body);
            }
            cv_.notify_all();

            if (req.get_header_value("Authorization") != "Bearer fresh-access") {
                res.status = 401;
                res.set_content("", "text/plain");
                return;
            }

            auto parsed = loom::utils::json::parse(req.body);
            if (!parsed) {
                res.status = 400;
                res.set_content("", "text/plain");
                return;
            }

            auto root = parsed->root();
            const auto method = std::string(root.get("method").as_str());
            if (method == "notifications/initialized" || method == "notifications/cancelled") {
                res.status = 202;
                res.set_content("", "text/plain");
                return;
            }

            const auto id = json_id_literal(root.get("id"));
            if (method == "initialize") {
                res.set_content(std::format(
                    R"({{"jsonrpc":"2.0","id":{},"result":{{"protocolVersion":"2024-11-05","capabilities":{{"tools":{{}}}},"serverInfo":{{"name":"refresh-fixture","version":"1.0.0"}}}}}})",
                    id), "application/json");
                return;
            }
            if (method == "tools/list") {
                res.set_content(std::format(
                    R"({{"jsonrpc":"2.0","id":{},"result":{{"tools":[{{"name":"refresh_lookup","description":"Lookup after refresh","inputSchema":{{"type":"object"}}}}]}}}})",
                    id), "application/json");
                return;
            }

            res.status = 404;
            res.set_content("", "text/plain");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalRefreshingStreamableHttpMcpServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool ready() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    [[nodiscard]] std::string mcp_url() const {
        return base_url() + "/mcp";
    }

    [[nodiscard]] std::string metadata_url() const {
        return base_url() + "/metadata";
    }

    [[nodiscard]] std::string token_request_body() const {
        std::lock_guard lock(mutex_);
        return token_request_body_;
    }

    [[nodiscard]] std::vector<std::string> mcp_authorization_headers() const {
        std::lock_guard lock(mutex_);
        return mcp_authorization_headers_;
    }

    [[nodiscard]] bool wait_for_token_request(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return token_request_count_ > 0; });
    }

private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    int token_request_count_{0};
    std::string token_request_body_;
    std::vector<std::string> mcp_authorization_headers_;
    std::vector<std::string> mcp_request_bodies_;
    bool fail_refresh_{false};
};

} // namespace

TEST(McpElicitationHandler, UsesRegisteredResponderAndPolicy) {
    loom::services::mcp::clear_elicitation_policy();
    loom::services::mcp::clear_elicitation_responder();

    auto missing = loom::services::mcp::handle_elicitation(loom::services::mcp::ElicitationRequest{
        .server_name = "linear",
        .message = "Pick a workspace",
        .schema = {{"workspace", "string"}},
    });
    ASSERT_FALSE(missing.has_value());
    EXPECT_NE(missing.error().find("No MCP elicitation responder"), std::string::npos);

    std::optional<loom::services::mcp::ElicitationRequest> captured;
    loom::services::mcp::set_elicitation_responder([&](const loom::services::mcp::ElicitationRequest& request)
        -> std::expected<std::map<std::string, std::string>, std::string> {
        captured = request;
        return std::map<std::string, std::string>{{"workspace", "eng"}};
    });

    auto response = loom::services::mcp::handle_elicitation(loom::services::mcp::ElicitationRequest{
        .server_name = "linear",
        .message = "Pick a workspace",
        .schema = {{"workspace", "string"}},
    });
    ASSERT_TRUE(response.has_value()) << response.error();
    EXPECT_EQ(response->at("workspace"), "eng");
    ASSERT_TRUE(captured.has_value());
    EXPECT_EQ(captured->server_name, "linear");
    EXPECT_EQ(captured->message, "Pick a workspace");
    EXPECT_EQ(captured->schema.at("workspace"), "string");

    loom::services::mcp::set_elicitation_allowed("linear", false);
    auto denied = loom::services::mcp::handle_elicitation(loom::services::mcp::ElicitationRequest{
        .server_name = "linear",
        .message = "Pick a workspace",
        .schema = {},
    });
    ASSERT_FALSE(denied.has_value());
    EXPECT_NE(denied.error().find("not allowed"), std::string::npos);

    loom::services::mcp::clear_elicitation_policy();
    loom::services::mcp::clear_elicitation_responder();
}

TEST(McpHeadersHelper, ParsesAndMergesDynamicHeaders) {
    const auto parsed = loom::services::mcp::parse_header_helper_json(R"JSON({
      "Authorization": "Bearer dynamic",
      "X-Helper": "present"
    })JSON");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->at("Authorization"), "Bearer dynamic");
    EXPECT_EQ(parsed->at("X-Helper"), "present");

    const auto invalid = loom::services::mcp::parse_header_helper_json(R"JSON({
      "X-Bad": 7
    })JSON");
    EXPECT_FALSE(invalid.has_value());

    const auto merged = loom::services::mcp::get_mcp_server_headers(
        "server",
        {
            {"Authorization", "Bearer static"},
            {"X-Keep", "static"},
        },
        "http://127.0.0.1:8123/mcp",
        ""
    );
    EXPECT_EQ(merged.at("Authorization"), "Bearer static");
    EXPECT_EQ(merged.at("X-Keep"), "static");
}

TEST(McpClient, SendsSseRequestsViaDiscoveredPostEndpoint) {
    LocalSseMcpServer server;
    ASSERT_TRUE(server.ready());

    loom::services::mcp::McpClient::Config config;
    config.name = "sse-fixture";
    config.request_timeout = std::chrono::milliseconds{2000};
    config.init_timeout = std::chrono::milliseconds{2000};

    loom::services::mcp::McpClient client(std::move(config));
    auto connected = client.connect_sse(server.url(), {{"X-Test-Header", "present"}});
    ASSERT_TRUE(connected.has_value());

    auto tools = client.list_tools();
    ASSERT_TRUE(tools.has_value());
    ASSERT_EQ(tools->tools.size(), 1u);
    EXPECT_EQ(tools->tools.front().name, "sse_lookup");
    EXPECT_EQ(tools->tools.front().description, "Lookup through SSE");

    const auto posts = server.post_bodies();
    std::string joined;
    for (const auto& body : posts) joined += body + "\n";
    EXPECT_NE(joined.find(R"("method":"initialize")"), std::string::npos) << joined;
    EXPECT_NE(joined.find(R"("method":"notifications/initialized")"), std::string::npos) << joined;
    EXPECT_NE(joined.find(R"("method":"tools/list")"), std::string::npos) << joined;

    client.shutdown();
}

TEST(McpClient, MapsSseUnauthorizedToUnauthorizedError) {
    LocalUnauthorizedStreamableHttpMcpServer server;
    ASSERT_TRUE(server.ready());

    loom::services::mcp::McpClient::Config config;
    config.name = "sse-auth-fixture";
    config.request_timeout = std::chrono::milliseconds{500};
    config.init_timeout = std::chrono::milliseconds{500};

    loom::services::mcp::McpClient client(std::move(config));
    auto connected = client.connect_sse(server.url());
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error(), loom::services::mcp::McpClientError::Unauthorized);

    const auto requests = server.requests();
    ASSERT_FALSE(requests.empty());
    EXPECT_NE(requests.front().find("GET /mcp HTTP/1.1"), std::string::npos) << requests.front();

    client.shutdown();
}

TEST(McpClient, SseReconnectResumesWithLastEventId) {
    LocalReconnectSseStreamServer server;
    ASSERT_TRUE(server.ready());

    loom::services::mcp::SseTransport::ReconnectPolicy policy{
        .initial_delay = std::chrono::milliseconds{10},
        .max_delay = std::chrono::milliseconds{20},
        .backoff_multiplier = 2.0,
        .jitter_factor = 0.0,
        .max_retries = 5,
        .liveness_timeout = std::chrono::seconds{1},
    };
    loom::services::mcp::SseTransport transport(server.url(), {}, policy);

    auto started = transport.start();
    ASSERT_TRUE(started.has_value()) << static_cast<int>(started.error());
    ASSERT_TRUE(server.wait_for_stream_requests(2));
    transport.close();

    const auto last_event_ids = server.stream_last_event_ids();
    ASSERT_GE(last_event_ids.size(), 2u);
    EXPECT_TRUE(last_event_ids.front().empty());
    EXPECT_EQ(last_event_ids[1], "1");
}

TEST(McpConnectionManager, ConnectsStreamableHttpServerWithDirectPostTransport) {
    LocalStreamableHttpMcpServer server;
    ASSERT_TRUE(server.ready());

    loom::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = fs::temp_directory_path() / "loom_streamable_http_mcp_config";
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    loom::services::mcp::McpConnectionManager manager(std::move(manager_config));

    loom::services::mcp::ServerConfig server_config;
    server_config.name = "http-fixture";
    server_config.transport = loom::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.url();
    server_config.headers = {{"X-Test-Header", "present"}};
    server_config.enabled = true;
    server_config.auto_start = true;

    loom::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto connected = manager.connect_server("http-fixture");
    ASSERT_TRUE(connected.has_value()) << static_cast<int>(connected.error());

    auto snapshot = manager.snapshot_server("http-fixture");
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->status, loom::services::mcp::ConnectionStatus::Connected);
    ASSERT_EQ(snapshot->tools.size(), 1u);
    EXPECT_EQ(snapshot->tools.front().name, "http_lookup");
    EXPECT_EQ(snapshot->tools.front().description, "Lookup through HTTP");

    const auto posts = server.post_bodies();
    std::string joined_bodies;
    for (const auto& body : posts) joined_bodies += body + "\n";
    EXPECT_NE(joined_bodies.find(R"("method":"initialize")"), std::string::npos) << joined_bodies;
    EXPECT_NE(joined_bodies.find(R"("method":"notifications/initialized")"), std::string::npos) << joined_bodies;
    EXPECT_NE(joined_bodies.find(R"("method":"tools/list")"), std::string::npos) << joined_bodies;

    const auto requests = server.requests();
    std::string joined_requests;
    for (const auto& request : requests) joined_requests += request + "\n";
    EXPECT_NE(joined_requests.find("POST /mcp HTTP/1.1"), std::string::npos) << joined_requests;
    EXPECT_NE(joined_requests.find("Accept: application/json, text/event-stream"), std::string::npos) << joined_requests;
    EXPECT_NE(joined_requests.find("X-Test-Header: present"), std::string::npos) << joined_requests;

    manager.shutdown();
}

TEST(McpConnectionManager, MarksRemoteHttpUnauthorizedAsNeedsAuth) {
    LocalUnauthorizedStreamableHttpMcpServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    loom::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = fs::temp_directory_path() / ("loom_mcp_unauthorized_" + std::to_string(suffix));
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    loom::services::mcp::McpConnectionManager manager(std::move(manager_config));

    loom::services::mcp::ServerConfig server_config;
    server_config.name = "auth-fixture";
    server_config.transport = loom::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.url();
    server_config.enabled = true;
    server_config.auto_start = true;

    loom::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto connected = manager.connect_server("auth-fixture");
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error(), loom::services::mcp::McpClientError::Unauthorized);

    auto snapshot = manager.snapshot_server("auth-fixture");
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->status, loom::services::mcp::ConnectionStatus::NeedsAuth);
    ASSERT_TRUE(snapshot->last_error.has_value());
    EXPECT_EQ(*snapshot->last_error, "authentication required");

    const auto requests = server.requests();
    ASSERT_FALSE(requests.empty());
    EXPECT_NE(requests.front().find(R"("method":"initialize")"), std::string::npos) << requests.front();

    manager.shutdown();
}

TEST(McpConnectionManager, AppliesHeadersHelperBeforeRemoteConnection) {
    LocalStreamableHttpMcpServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_headers_helper_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto helper_path = root / "headers-helper.sh";
    {
        std::ofstream helper(helper_path);
        helper << R"SH(
printf '{"X-Test-Header":"dynamic","X-Helper-Server":"%s","X-Helper-Url":"%s"}\n' "$LOOM_MCP_SERVER_NAME" "$LOOM_MCP_SERVER_URL"
)SH";
    }

    loom::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = root;
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    loom::services::mcp::McpConnectionManager manager(std::move(manager_config));

    loom::services::mcp::ServerConfig server_config;
    server_config.name = "helper-fixture";
    server_config.transport = loom::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.url();
    server_config.headers = {
        {"X-Test-Header", "static"},
        {"X-Static", "present"},
    };
    server_config.headers_helper = "sh '" + helper_path.string() + "'";

    loom::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto connected = manager.connect_server("helper-fixture");
    ASSERT_TRUE(connected.has_value()) << static_cast<int>(connected.error());

    const auto requests = server.requests();
    std::string joined_requests;
    for (const auto& request : requests) joined_requests += request + "\n";
    EXPECT_NE(joined_requests.find("X-Test-Header: dynamic"), std::string::npos) << joined_requests;
    EXPECT_EQ(joined_requests.find("X-Test-Header: static"), std::string::npos) << joined_requests;
    EXPECT_NE(joined_requests.find("X-Static: present"), std::string::npos) << joined_requests;
    EXPECT_NE(joined_requests.find("X-Helper-Server: helper-fixture"), std::string::npos) << joined_requests;
    EXPECT_NE(joined_requests.find("X-Helper-Url: " + server.url()), std::string::npos) << joined_requests;

    manager.shutdown();
    fs::remove_all(root);
}

TEST(McpConnectionManager, RefreshesExpiredOAuthTokenBeforeRemoteConnection) {
    LocalRefreshingStreamableHttpMcpServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_refresh_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());

    loom::services::mcp::ServerConfig server_config;
    server_config.name = "refresh-fixture";
    server_config.transport = loom::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.mcp_url();
    server_config.enabled = true;
    server_config.auto_start = true;
    server_config.oauth = loom::services::mcp::McpOAuthConfig{
        .auth_server_metadata_url = server.metadata_url(),
        .client_id = "client-1",
    };

    loom::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = server_config.url;
    auth_config.oauth = server_config.oauth;
    const auto server_key = loom::services::mcp::get_server_key(server_config.name, auth_config);
    auto sanitize_key = [](std::string_view key) {
        std::string sanitized;
        sanitized.reserve(key.size());
        for (unsigned char ch : key) {
            sanitized.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
        }
        return sanitized;
    };
    const auto token_path = root / "loom" / "mcp" / (sanitize_key(server_key) + ".json");
    fs::create_directories(token_path.parent_path());
    const auto expired_at = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() - 60;
    {
        std::ofstream token_file(token_path);
        token_file << std::format(R"({{
          "server_name": "refresh-fixture",
          "server_url": "{}",
          "access_token": "old-access",
          "refresh_token": "old-refresh",
          "expires_at": {},
          "scope": "tools",
          "client_id": "client-1",
          "client_secret": "",
          "discovery_state": {{}}
        }})", server.mcp_url(), expired_at);
    }

    loom::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = root;
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    loom::services::mcp::McpConnectionManager manager(std::move(manager_config));

    loom::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto connected = manager.connect_server("refresh-fixture");
    ASSERT_TRUE(connected.has_value()) << static_cast<int>(connected.error());
    ASSERT_TRUE(server.wait_for_token_request());
    EXPECT_NE(server.token_request_body().find("grant_type=refresh_token"), std::string::npos);
    EXPECT_NE(server.token_request_body().find("refresh_token=old-refresh"), std::string::npos);

    auto snapshot = manager.snapshot_server("refresh-fixture");
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->status, loom::services::mcp::ConnectionStatus::Connected);
    ASSERT_EQ(snapshot->tools.size(), 1u);
    EXPECT_EQ(snapshot->tools.front().name, "refresh_lookup");

    const auto auth_headers = server.mcp_authorization_headers();
    ASSERT_FALSE(auth_headers.empty());
    EXPECT_TRUE(std::ranges::all_of(auth_headers, [](const auto& header) {
        return header == "Bearer fresh-access";
    }));

    auto persisted = loom::utils::json::parse_file(token_path);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_EQ(persisted->root().get_string("access_token"), "fresh-access");
    EXPECT_EQ(persisted->root().get_string("refresh_token"), "fresh-refresh");

    manager.shutdown();
    fs::remove_all(root);
}

TEST(McpConnectionManager, MarksRefreshFailureAsNeedsAuthWithoutRemoteConnect) {
    LocalRefreshingStreamableHttpMcpServer server(/*fail_refresh=*/true);
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_refresh_failure_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());

    loom::services::mcp::ServerConfig server_config;
    server_config.name = "refresh-failure-fixture";
    server_config.transport = loom::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.mcp_url();
    server_config.enabled = true;
    server_config.auto_start = true;
    server_config.oauth = loom::services::mcp::McpOAuthConfig{
        .auth_server_metadata_url = server.metadata_url(),
        .client_id = "client-1",
    };

    loom::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = server_config.url;
    auth_config.oauth = server_config.oauth;
    const auto server_key = loom::services::mcp::get_server_key(server_config.name, auth_config);
    auto sanitize_key = [](std::string_view key) {
        std::string sanitized;
        sanitized.reserve(key.size());
        for (unsigned char ch : key) {
            sanitized.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
        }
        return sanitized;
    };
    const auto token_path = root / "loom" / "mcp" / (sanitize_key(server_key) + ".json");
    fs::create_directories(token_path.parent_path());
    const auto expired_at = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() - 60;
    {
        std::ofstream token_file(token_path);
        token_file << std::format(R"({{
          "server_name": "refresh-failure-fixture",
          "server_url": "{}",
          "access_token": "old-access",
          "refresh_token": "old-refresh",
          "expires_at": {},
          "scope": "tools",
          "client_id": "client-1",
          "client_secret": "",
          "discovery_state": {{}}
        }})", server.mcp_url(), expired_at);
    }

    loom::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = root;
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    loom::services::mcp::McpConnectionManager manager(std::move(manager_config));

    loom::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto connected = manager.connect_server("refresh-failure-fixture");
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error(), loom::services::mcp::McpClientError::Unauthorized);
    ASSERT_TRUE(server.wait_for_token_request());
    EXPECT_NE(server.token_request_body().find("grant_type=refresh_token"), std::string::npos);
    EXPECT_NE(server.token_request_body().find("refresh_token=old-refresh"), std::string::npos);

    auto snapshot = manager.snapshot_server("refresh-failure-fixture");
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->status, loom::services::mcp::ConnectionStatus::NeedsAuth);
    ASSERT_TRUE(snapshot->last_error.has_value());
    EXPECT_NE(snapshot->last_error->find("OAuth token refresh failed"), std::string::npos);
    EXPECT_TRUE(server.mcp_authorization_headers().empty());

    auto persisted = loom::utils::json::parse_file(token_path);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_EQ(persisted->root().get_string("access_token"), "old-access");
    EXPECT_EQ(persisted->root().get_string("refresh_token"), "old-refresh");

    manager.shutdown();
    fs::remove_all(root);
}

TEST(McpConnectionManager, MarksDiscoveryServerWithoutTokenAsNeedsAuthThenReconnectsWithStoredToken) {
    LocalRefreshingStreamableHttpMcpServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_auth_needed_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());

    loom::services::mcp::ServerConfig server_config;
    server_config.name = "auth-needed-fixture";
    server_config.transport = loom::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.mcp_url();
    server_config.enabled = true;
    server_config.auto_start = true;
    server_config.oauth = loom::services::mcp::McpOAuthConfig{
        .auth_server_metadata_url = server.metadata_url(),
        .client_id = "client-1",
    };

    loom::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = server_config.url;
    auth_config.oauth = server_config.oauth;
    const auto server_key = loom::services::mcp::get_server_key(server_config.name, auth_config);
    auto sanitize_key = [](std::string_view key) {
        std::string sanitized;
        sanitized.reserve(key.size());
        for (unsigned char ch : key) {
            sanitized.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
        }
        return sanitized;
    };
    const auto token_path = root / "loom" / "mcp" / (sanitize_key(server_key) + ".json");

    loom::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = root;
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    loom::services::mcp::McpConnectionManager manager(std::move(manager_config));

    loom::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto auth_needed = manager.connect_server("auth-needed-fixture");
    ASSERT_FALSE(auth_needed.has_value());
    EXPECT_EQ(auth_needed.error(), loom::services::mcp::McpClientError::Unauthorized);
    EXPECT_TRUE(server.mcp_authorization_headers().empty());
    EXPECT_TRUE(server.token_request_body().empty());

    auto snapshot = manager.snapshot_server("auth-needed-fixture");
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->status, loom::services::mcp::ConnectionStatus::NeedsAuth);
    ASSERT_TRUE(snapshot->last_error.has_value());
    EXPECT_NE(snapshot->last_error->find("MCP OAuth authentication required"), std::string::npos);

    fs::create_directories(token_path.parent_path());
    const auto expires_at = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 3600;
    {
        std::ofstream token_file(token_path);
        token_file << std::format(R"({{
          "server_name": "auth-needed-fixture",
          "server_url": "{}",
          "access_token": "fresh-access",
          "refresh_token": "",
          "expires_at": {},
          "scope": "tools",
          "client_id": "client-1",
          "client_secret": "",
          "discovery_state": {{}}
        }})", server.mcp_url(), expires_at);
    }

    auto connected = manager.connect_server("auth-needed-fixture");
    ASSERT_TRUE(connected.has_value()) << static_cast<int>(connected.error());

    snapshot = manager.snapshot_server("auth-needed-fixture");
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->status, loom::services::mcp::ConnectionStatus::Connected);
    ASSERT_EQ(snapshot->tools.size(), 1u);
    EXPECT_EQ(snapshot->tools.front().name, "refresh_lookup");

    const auto auth_headers = server.mcp_authorization_headers();
    ASSERT_FALSE(auth_headers.empty());
    EXPECT_TRUE(std::ranges::all_of(auth_headers, [](const auto& header) {
        return header == "Bearer fresh-access";
    }));

    manager.shutdown();
    fs::remove_all(root);
}

TEST(McpConnectionManager, RefreshesCachedListsAfterListChangedNotifications) {
    auto root = fs::temp_directory_path() / "loom_mcp_list_changed_test";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto server_path = root / "server.js";
    {
        std::ofstream server(server_path);
        server << R"JS(
const readline = require('node:readline');
const rl = readline.createInterface({ input: process.stdin });

let toolListCount = 0;
let resourceListCount = 0;
let promptListCount = 0;
let notificationsSent = false;

function send(message) {
  process.stdout.write(JSON.stringify(message) + '\n');
}

function maybeSendListChangedNotifications() {
  if (notificationsSent || promptListCount === 0) return;
  notificationsSent = true;
  setTimeout(() => {
    send({ jsonrpc: '2.0', method: 'notifications/tools/list_changed', params: {} });
    send({ jsonrpc: '2.0', method: 'notifications/resources/list_changed', params: {} });
    send({ jsonrpc: '2.0', method: 'notifications/prompts/list_changed', params: {} });
  }, 25);
}

rl.on('line', line => {
  const message = JSON.parse(line);
  if (message.method === 'initialize') {
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: {
        protocolVersion: '2024-11-05',
        capabilities: {
          tools: { listChanged: true },
          resources: { listChanged: true },
          prompts: { listChanged: true }
        },
        serverInfo: { name: 'list-changed-fixture', version: '1.0.0' }
      }
    });
    return;
  }
  if (message.method === 'tools/list') {
    toolListCount += 1;
    const suffix = toolListCount === 1 ? 'initial' : 'updated';
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: { tools: [{ name: `${suffix}_tool`, description: `${suffix} tool`, inputSchema: { type: 'object' } }] }
    });
    maybeSendListChangedNotifications();
    return;
  }
  if (message.method === 'resources/list') {
    resourceListCount += 1;
    const suffix = resourceListCount === 1 ? 'initial' : 'updated';
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: { resources: [{ uri: `file:///${suffix}.txt`, name: `${suffix} resource`, description: `${suffix} resource`, mimeType: 'text/plain' }] }
    });
    maybeSendListChangedNotifications();
    return;
  }
  if (message.method === 'prompts/list') {
    promptListCount += 1;
    const suffix = promptListCount === 1 ? 'initial' : 'updated';
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: { prompts: [{ name: `${suffix}_prompt`, description: `${suffix} prompt`, arguments: [] }] }
    });
    maybeSendListChangedNotifications();
  }
});
)JS";
    }

    loom::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = root;
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    loom::services::mcp::McpConnectionManager manager(std::move(manager_config));

    loom::services::mcp::ServerConfig server_config;
    server_config.name = "list-changed-fixture";
    server_config.transport = loom::services::mcp::TransportType::Stdio;
    server_config.command = "node";
    server_config.args = {server_path.string()};

    loom::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto connected = manager.connect_server("list-changed-fixture");
    ASSERT_TRUE(connected.has_value()) << static_cast<int>(connected.error());

    auto initial = manager.snapshot_server("list-changed-fixture");
    ASSERT_TRUE(initial.has_value());
    ASSERT_EQ(initial->tools.size(), 1u);
    ASSERT_EQ(initial->resources.size(), 1u);
    ASSERT_EQ(initial->prompts.size(), 1u);
    EXPECT_EQ(initial->tools.front().name, "initial_tool");
    EXPECT_EQ(initial->resources.front().uri, "file:///initial.txt");
    EXPECT_EQ(initial->prompts.front().name, "initial_prompt");

    bool refreshed = false;
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto snapshot = manager.snapshot_server("list-changed-fixture");
        if (snapshot &&
            snapshot->tools.size() == 1 &&
            snapshot->resources.size() == 1 &&
            snapshot->prompts.size() == 1 &&
            snapshot->tools.front().name == "updated_tool" &&
            snapshot->resources.front().uri == "file:///updated.txt" &&
            snapshot->prompts.front().name == "updated_prompt") {
            refreshed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
    EXPECT_TRUE(refreshed);

    manager.shutdown();
    fs::remove_all(root);
}

TEST(McpClient, HandlesServerRootsRequestsAndNotificationParams) {
    auto root = fs::temp_directory_path() / "loom_mcp_client_requests_test";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto server_path = root / "server.js";
    const auto log_path = root / "mcp-log.jsonl";
    {
        std::ofstream server(server_path);
        server << R"JS(
const fs = require('node:fs');
const readline = require('node:readline');
const rl = readline.createInterface({ input: process.stdin });
const logPath = process.env.MCP_LOG;

function send(message) {
  process.stdout.write(JSON.stringify(message) + '\n');
}

function log(message) {
  fs.appendFileSync(logPath, `${JSON.stringify(message)}\n`);
}

rl.on('line', line => {
  const message = JSON.parse(line);
  if (message.method === 'initialize') {
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: {
        protocolVersion: '2024-11-05',
        capabilities: { tools: {} },
        serverInfo: { name: 'roots-fixture', version: '1.0.0' }
      }
    });
    return;
  }
  if (message.method === 'notifications/initialized') {
    send({ jsonrpc: '2.0', id: 'roots-1', method: 'roots/list', params: {} });
    send({
      jsonrpc: '2.0',
      method: 'notifications/progress',
      params: { progressToken: 'tok', progress: 0.5 }
    });
    return;
  }
  if (message.id === 'roots-1') {
    log(message);
    return;
  }
  if (message.method === 'tools/list') {
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: { tools: [{ name: 'noop', description: 'Noop', inputSchema: { type: 'object' } }] }
    });
  }
});
)JS";
    }

    loom::services::mcp::McpClient::Config config;
    config.name = "roots-fixture";
    config.request_timeout = std::chrono::milliseconds{2000};
    config.init_timeout = std::chrono::milliseconds{2000};

    loom::services::mcp::McpClient client(std::move(config));
    client.set_roots_handler([] {
        return std::vector<loom::services::mcp::Root>{
            loom::services::mcp::Root{
                .uri = "file:///workspace",
                .name = std::string{"workspace"},
            },
        };
    });

    std::mutex notification_mutex;
    std::optional<loom::services::mcp::JsonRpcNotification> notification;
    client.set_notification_callback([&](const loom::services::mcp::JsonRpcNotification& value) {
        std::lock_guard lock(notification_mutex);
        notification = value;
    });

    auto connected = client.connect_stdio(
        "node",
        {server_path.string()},
        std::map<std::string, std::string>{{"MCP_LOG", log_path.string()}});
    ASSERT_TRUE(connected.has_value());

    auto tools = client.list_tools();
    ASSERT_TRUE(tools.has_value());
    ASSERT_EQ(tools->tools.size(), 1u);
    EXPECT_EQ(tools->tools.front().name, "noop");

    std::string log;
    for (int attempt = 0; attempt < 50; ++attempt) {
        {
            std::ifstream input(log_path);
            if (input) {
                std::stringstream buffer;
                buffer << input.rdbuf();
                log = buffer.str();
            }
        }
        bool has_notification = false;
        {
            std::lock_guard lock(notification_mutex);
            has_notification = notification.has_value();
        }
        if (log.find("\"id\":\"roots-1\"") != std::string::npos && has_notification) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }

    EXPECT_NE(log.find("\"id\":\"roots-1\""), std::string::npos) << log;
    EXPECT_NE(log.find("\"roots\":[{\"uri\":\"file:///workspace\",\"name\":\"workspace\"}]"), std::string::npos) << log;

    std::optional<loom::services::mcp::JsonRpcNotification> captured;
    {
        std::lock_guard lock(notification_mutex);
        captured = notification;
    }
    ASSERT_TRUE(captured.has_value());
    EXPECT_EQ(captured->method, "notifications/progress");
    ASSERT_TRUE(captured->params_json.has_value());
    auto params = loom::utils::json::parse(*captured->params_json);
    ASSERT_TRUE(params.has_value()) << params.error().message();
    EXPECT_EQ(params->root().get("progressToken").as_str(), "tok");
    EXPECT_EQ(params->root().get("progress").as_double(), 0.5);
    EXPECT_FALSE(params->root().has("jsonrpc"));
    EXPECT_FALSE(params->root().has("method"));

    client.shutdown();
    fs::remove_all(root);
}

TEST(McpClient, ParsesPromptMessageContentObjects) {
    auto root = fs::temp_directory_path() / "loom_mcp_prompt_content_test";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto server_path = root / "server.js";
    {
        std::ofstream server(server_path);
        server << R"JS(
const readline = require('node:readline');
const rl = readline.createInterface({ input: process.stdin });

function send(message) {
  process.stdout.write(JSON.stringify(message) + '\n');
}

rl.on('line', line => {
  const message = JSON.parse(line);
  if (message.method === 'initialize') {
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: {
        protocolVersion: '2024-11-05',
        capabilities: { prompts: {} },
        serverInfo: { name: 'prompt-fixture', version: '1.0.0' }
      }
    });
    return;
  }
  if (message.method === 'prompts/list') {
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: {
        prompts: [{
          name: 'review',
          description: 'Review a topic',
          arguments: [{ name: 'topic', description: 'Topic to review', required: true }]
        }]
      }
    });
    return;
  }
  if (message.method === 'prompts/get') {
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: {
        description: 'Prompt with rich content',
        messages: [
          {
            role: 'user',
            content: { type: 'text', text: 'Review ' + message.params.arguments.topic }
          },
          {
            role: 'assistant',
            content: {
              type: 'resource',
              resource: { uri: 'file:///notes.md', mimeType: 'text/markdown', text: 'notes body' }
            }
          },
          {
            role: 'user',
            content: {
              type: 'resource_link',
              name: 'notes',
              uri: 'file:///notes.md',
              description: 'Reference notes'
            }
          },
          {
            role: 'assistant',
            content: { type: 'image', mimeType: 'image/png', data: 'iVBORw0KGgo=' }
          }
        ]
      }
    });
  }
});
)JS";
    }

    loom::services::mcp::McpClient::Config config;
    config.name = "prompt-fixture";
    config.request_timeout = std::chrono::milliseconds{2000};
    config.init_timeout = std::chrono::milliseconds{2000};

    loom::services::mcp::McpClient client(std::move(config));
    auto connected = client.connect_stdio("node", {server_path.string()}, {});
    ASSERT_TRUE(connected.has_value());

    auto prompts = client.list_prompts();
    ASSERT_TRUE(prompts.has_value());
    ASSERT_EQ(prompts->prompts.size(), 1u);
    EXPECT_EQ(prompts->prompts.front().name, "review");
    ASSERT_EQ(prompts->prompts.front().arguments.size(), 1u);
    EXPECT_EQ(prompts->prompts.front().arguments.front().name, "topic");
    EXPECT_TRUE(prompts->prompts.front().arguments.front().required);

    auto prompt = client.get_prompt("review", {{"topic", "migration"}});
    ASSERT_TRUE(prompt.has_value());
    ASSERT_EQ(prompt->messages.size(), 4u);
    EXPECT_EQ(prompt->messages[0].role, loom::services::mcp::PromptRole::User);
    EXPECT_EQ(prompt->messages[0].content, "Review migration");
    EXPECT_EQ(prompt->messages[1].role, loom::services::mcp::PromptRole::Assistant);
    EXPECT_EQ(prompt->messages[1].content, "[Resource from prompt-fixture at file:///notes.md] notes body");
    EXPECT_EQ(prompt->messages[2].content, "[Resource link: notes] file:///notes.md (Reference notes)");
    EXPECT_NE(prompt->messages[3].content.find("[Image from prompt-fixture] Binary content (image/png"), std::string::npos);

    client.shutdown();
    fs::remove_all(root);
}

// End of file
