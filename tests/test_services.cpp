/// @file test_services.cpp
/// @brief Service layer smoke tests aligned with current C++ module APIs.

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

import std;
import cc.cli.ccr_client;
import cc.cli.sse_transport;
import cc.bridge.core;
import cc.config.config;
import cc.constants.paths;
import cc.services.api.client;
import cc.services.api.errors;
import cc.services.api.session_ingress;
import cc.services.api.streaming;
import cc.services.compact.api_microcompact;
import cc.services.lsp.LSPServerManager;
import cc.services.lsp.client;
import cc.services.mcp.client;
import cc.services.mcp.auth;
import cc.services.mcp.channel_permissions;
import cc.services.mcp.config;
import cc.services.mcp.connection_manager;
import cc.services.mcp.elicitation_handler;
import cc.services.mcp.headers_helper;
import cc.services.mcp.vscode_sdk_mcp;
import cc.services.memory.sessionMemory;
import cc.services.extract_memories;
import cc.services.mcp.types;
import cc.services.rate_limit;
import cc.services.token_estimation;
import cc.services.prompt_suggestion;
import cc.server.server_routes;
import cc.server.server_main;
import cc.session.storage;
import cc.session.history;
import cc.query.query_engine;
import cc.memdir.paths;
import cc.tools.agent_runtime;
import cc.tools.team;
import cc.tools.tool;
import cc.types.types;
import cc.utils.error;
import cc.services.ide_integration;
import cc.utils.json;
import cc.utils.team_helpers;
import cc.utils.atomic_replace;

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

/// True when walking up from `base` finds NO `.git` work-tree marker
/// (directory for a normal repo, file for a worktree/submodule).
[[nodiscard]] bool c13_ancestry_clean(const fs::path& base) {
    std::error_code ec;
    for (auto d = base; ; d = d.parent_path()) {
        if (fs::exists(d / ".git", ec)) return false;
        if (d.parent_path() == d || d.parent_path().empty()) return true;
    }
}

/// Pick a temp base directory whose ancestry contains no git work tree
/// (this dev box carries a stray /tmp/.git; CI /tmp is clean). Tries
/// XDG_RUNTIME_DIR, TMPDIR, /dev/shm, then the system temp dir. Returns
/// nullopt only if every candidate is inside a work tree. Env-supplied
/// paths are probed with the error_code overload so EACCES never throws.
[[nodiscard]] std::optional<fs::path> c13_clean_temp_base() {
    std::error_code ec;
    for (const char* var : {"XDG_RUNTIME_DIR", "TMPDIR"}) {
        if (const char* v = std::getenv(var);
            v != nullptr && fs::is_directory(v, ec) && !ec &&
            c13_ancestry_clean(v)) {
            return fs::path(v);
        }
        ec.clear();
    }
    if (fs::is_directory("/dev/shm", ec) && !ec &&
        c13_ancestry_clean("/dev/shm")) {
        return fs::path("/dev/shm");
    }
    if (c13_ancestry_clean(fs::temp_directory_path())) {
        return fs::temp_directory_path();
    }
    return std::nullopt;
}

/// Snapshot of every existing `.gitignore` on the walk-up chain from
/// `root` through `base`. Comparing before/after writes detects a NEW
/// ignore file created by the appender without false-failing on unrelated
/// pre-existing .gitignore files in a shared temp base.
[[nodiscard]] std::set<fs::path>
c13_gitignores_on_chain(const fs::path& root, const fs::path& base) {
    std::set<fs::path> found;
    std::error_code ec;
    for (auto d = root; ; d = d.parent_path()) {
        if (fs::exists(d / ".gitignore", ec)) found.insert(d / ".gitignore");
        ec.clear();
        if (d == base || d.parent_path() == d) break;
    }
    return found;
}

/// Temp root for a write test: anchored under a git-ancestry-clean base
/// so a .gitignore appender walking up from cwd can never reach a real
/// work tree such as a stray /tmp/.git. The unique name carries pid + an
/// in-process counter + timestamp, and create is retried on collision or
/// transient failure.
[[nodiscard]] fs::path c13_make_temp_root(std::string_view name) {
    static std::atomic<unsigned> counter{0};
    const auto base = c13_clean_temp_base();
    const fs::path base_dir = base.value_or(fs::temp_directory_path());
    constexpr int kMaxAttempts = 64;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const fs::path root =
            base_dir /
            (std::string(name) + std::to_string(::getpid()) + "_" +
             std::to_string(counter.fetch_add(1,
                 std::memory_order_relaxed)) + "_" +
             std::to_string(std::chrono::system_clock::now()
                                 .time_since_epoch().count()));
        std::error_code create_ec;
        fs::create_directories(root, create_ec);
        std::error_code probe_ec;
        if (fs::is_directory(root, probe_ec) && !probe_ec) {
            return root;  // created, or a unique name that now exists
        }
        // Collision / transient: take the next unique name.
    }
    // Exhausted (effectively impossible with unique names): return the
    // system temp dir so a failure surfaces as a clear test error rather
    // than a dangling empty path.
    return fs::temp_directory_path();
}

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

struct EnvironmentUnsetGuard {
    std::string name;
    std::optional<std::string> previous;

    explicit EnvironmentUnsetGuard(std::string key) : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        unsetenv(name.c_str());
    }

    ~EnvironmentUnsetGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

class DefinitionOnlyTool final : public cc::core::ITool {
public:
    explicit DefinitionOnlyTool(cc::core::ToolDefinition definition)
        : definition_(std::move(definition)) {}

    [[nodiscard]] const cc::core::ToolDefinition& definition() const override {
        return definition_;
    }

    [[nodiscard]] cc::core::Result<cc::core::ToolResult> execute(
        const cc::core::ToolInput& /*input*/) override {
        return cc::core::ToolResult::success("unused");
    }

    [[nodiscard]] bool check_permission(const cc::core::ToolInput& /*input*/) const override {
        return true;
    }

private:
    cc::core::ToolDefinition definition_;
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

std::string json_id_literal(cc::utils::json::JsonVal id) {
    if (id.is_num()) return std::to_string(id.as_int());
    if (id.is_str()) return "\"" + std::string(id.as_str()) + "\"";
    return "null";
}

int test_hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
    return -1;
}

std::string test_url_decode(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') {
            out.push_back(' ');
        } else if (value[i] == '%' && i + 2 < value.size()) {
            const auto hi = test_hex_value(value[i + 1]);
            const auto lo = test_hex_value(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(value[i]);
            }
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

std::optional<std::string> test_query_param(std::string_view url, std::string_view key) {
    auto query_start = url.find('?');
    if (query_start == std::string_view::npos) return std::nullopt;
    auto fragment_start = url.find('#', query_start);
    auto query = url.substr(
        query_start + 1,
        fragment_start == std::string_view::npos ? std::string_view::npos : fragment_start - query_start - 1);
    std::string pattern(key);
    pattern.push_back('=');
    std::size_t pos = 0;
    while (pos < query.size()) {
        auto next = query.find('&', pos);
        auto part = query.substr(pos, next == std::string_view::npos ? std::string_view::npos : next - pos);
        if (part.starts_with(pattern)) return test_url_decode(part.substr(pattern.size()));
        if (next == std::string_view::npos) break;
        pos = next + 1;
    }
    return std::nullopt;
}

std::optional<int> localhost_url_port(std::string_view url) {
    constexpr std::string_view prefix = "http://localhost:";
    if (!url.starts_with(prefix)) return std::nullopt;
    auto port_start = prefix.size();
    auto path_start = url.find('/', port_start);
    if (path_start == std::string_view::npos) return std::nullopt;
    try {
        return std::stoi(std::string(url.substr(port_start, path_start - port_start)));
    } catch (...) {
        return std::nullopt;
    }
}

class LocalAnthropicMessagesServer {
public:
    explicit LocalAnthropicMessagesServer(
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

    ~LocalAnthropicMessagesServer() {
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

            auto doc = cc::utils::json::parse(frame->payload);
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

        auto parsed = cc::utils::json::parse(body);
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

        auto parsed = cc::utils::json::parse(body);
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

            auto parsed = cc::utils::json::parse(req.body);
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

class LocalOAuthRevocationServer {
public:
    LocalOAuthRevocationServer() {
        server_.Get("/metadata", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(std::format(R"({{
              "authorization_endpoint": "{0}/authorize",
              "token_endpoint": "{0}/token",
              "revocation_endpoint": "{0}/revoke",
              "revocation_endpoint_auth_methods_supported": ["client_secret_post"],
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
            if (req.body.find("grant_type=authorization_code") == std::string::npos ||
                req.body.find("code=callback-code") == std::string::npos ||
                req.body.find("client_id=client-1") == std::string::npos ||
                req.body.find("code_verifier=") == std::string::npos ||
                req.body.find("redirect_uri=http%3A%2F%2Flocalhost%3A") == std::string::npos) {
                res.status = 400;
                res.set_content(R"({"error":"invalid_request"})", "application/json");
                return;
            }
            res.set_content(R"({
              "access_token": "callback-access",
              "refresh_token": "callback-refresh",
              "expires_in": 3600,
              "scope": "tools"
            })", "application/json");
        });

        server_.Post("/revoke", [this](const httplib::Request& req, httplib::Response& res) {
            {
                std::lock_guard lock(mutex_);
                revoke_request_bodies_.push_back(req.body);
                revoke_authorization_headers_.push_back(req.get_header_value("Authorization"));
            }
            cv_.notify_all();
            res.status = 200;
            res.set_content(R"({"ok":true})", "application/json");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalOAuthRevocationServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool ready() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    [[nodiscard]] std::string metadata_url() const {
        return base_url() + "/metadata";
    }

    [[nodiscard]] std::string token_request_body() const {
        std::lock_guard lock(mutex_);
        return token_request_body_;
    }

    [[nodiscard]] std::vector<std::string> revoke_request_bodies() const {
        std::lock_guard lock(mutex_);
        return revoke_request_bodies_;
    }

    [[nodiscard]] std::vector<std::string> revoke_authorization_headers() const {
        std::lock_guard lock(mutex_);
        return revoke_authorization_headers_;
    }

    [[nodiscard]] bool wait_for_revoke_requests(
        std::size_t count,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this, count] {
            return revoke_request_bodies_.size() >= count;
        });
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
    std::vector<std::string> revoke_request_bodies_;
    std::vector<std::string> revoke_authorization_headers_;
};

class LocalXaaIdpServer {
public:
    LocalXaaIdpServer() {
        // PRM discovery (RFC 9728): GET /mcp/.well-known/oauth-protected-resource
        server_.Get("/mcp/.well-known/oauth-protected-resource",
            [this](const httplib::Request& req, httplib::Response& res) {
            (void)req;
            {
                std::lock_guard lock(mutex_);
                ++prm_request_count_;
            }
            cv_.notify_all();
            auto base = base_url();
            auto body = std::format(R"({{
                "resource": "{}/mcp",
                "authorization_servers": ["{}"]
            }})", base, base);
            res.set_content(body, "application/json");
        });

        // AS metadata (RFC 8414): GET /.well-known/oauth-authorization-server
        server_.Get("/.well-known/oauth-authorization-server",
            [this](const httplib::Request& req, httplib::Response& res) {
            (void)req;
            {
                std::lock_guard lock(mutex_);
                ++as_metadata_request_count_;
            }
            cv_.notify_all();
            auto base = base_url();
            auto body = std::format(R"({{
                "issuer": "{}",
                "token_endpoint": "{}/token",
                "grant_types_supported": [
                    "urn:ietf:params:oauth:grant-type:jwt-bearer",
                    "urn:ietf:params:oauth:grant-type:token-exchange"
                ],
                "token_endpoint_auth_methods_supported": ["client_secret_basic"]
            }})", base, base);
            res.set_content(body, "application/json");
        });

        // Token endpoint: handles both RFC 8693 (token-exchange) and RFC 7523 (jwt-bearer)
        server_.Post("/token",
            [this](const httplib::Request& req, httplib::Response& res) {
            auto body = req.body;
            {
                std::lock_guard lock(mutex_);
                if (body.find("grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Atoken-exchange")
                    != std::string::npos) {
                    // RFC 8693 Token Exchange (id_token → ID-JAG)
                    ++token_exchange_count_;
                    token_exchange_body_ = body;
                    cv_.notify_all();

                    // Verify expected fields
                    if (body.find("subject_token=fake-id-token-for-testing") == std::string::npos ||
                        body.find("client_id=idp-client-1") == std::string::npos) {
                        res.status = 400;
                        res.set_content(R"({"error":"invalid_request"})", "application/json");
                        return;
                    }

                    res.set_content(R"({
                        "access_token": "fake-id-jag-for-testing",
                        "issued_token_type": "urn:ietf:params:oauth:token-type:id-jag",
                        "expires_in": 3600,
                        "scope": "openid profile mcp"
                    })", "application/json");
                } else if (body.find("grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer")
                    != std::string::npos) {
                    // RFC 7523 JWT Bearer Grant (ID-JAG → access_token)
                    ++jwt_bearer_count_;
                    jwt_bearer_body_ = body;
                    jwt_bearer_auth_header_ = req.get_header_value("Authorization");
                    cv_.notify_all();

                    res.set_content(R"({
                        "access_token": "xaa-access",
                        "refresh_token": "xaa-refresh",
                        "token_type": "Bearer",
                        "expires_in": 3600,
                        "scope": "openid profile mcp"
                    })", "application/json");
                } else {
                    res.status = 400;
                    res.set_content(R"({"error":"unsupported_grant_type"})", "application/json");
                }
            }
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalXaaIdpServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool ready() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    // ── Request counters and bodies for test assertions ──────────────────

    [[nodiscard]] int prm_request_count() const {
        std::lock_guard lock(mutex_);
        return prm_request_count_;
    }

    [[nodiscard]] int as_metadata_request_count() const {
        std::lock_guard lock(mutex_);
        return as_metadata_request_count_;
    }

    [[nodiscard]] int token_exchange_count() const {
        std::lock_guard lock(mutex_);
        return token_exchange_count_;
    }

    [[nodiscard]] int jwt_bearer_count() const {
        std::lock_guard lock(mutex_);
        return jwt_bearer_count_;
    }

    [[nodiscard]] std::string token_exchange_body() const {
        std::lock_guard lock(mutex_);
        return token_exchange_body_;
    }

    [[nodiscard]] std::string jwt_bearer_body() const {
        std::lock_guard lock(mutex_);
        return jwt_bearer_body_;
    }

    [[nodiscard]] std::string jwt_bearer_auth_header() const {
        std::lock_guard lock(mutex_);
        return jwt_bearer_auth_header_;
    }

    [[nodiscard]] bool wait_for_jwt_bearer(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] {
            return jwt_bearer_count_ > 0;
        });
    }

private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    int prm_request_count_{0};
    int as_metadata_request_count_{0};
    int token_exchange_count_{0};
    int jwt_bearer_count_{0};
    std::string token_exchange_body_;
    std::string jwt_bearer_body_;
    std::string jwt_bearer_auth_header_;
};

} // namespace

TEST(LspConfig, LoadsPluginLspServersFromManifestAndRoutesExtension) {
    auto root = fs::weakly_canonical(fs::temp_directory_path()) / "loom_plugin_lsp_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom");
    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard plugin_cache_guard(
        "LOOM_PLUGIN_CACHE_DIR",
        (root / ".loom" / "plugins").string()
    );

    const auto plugin_root = root / ".loom" / "plugins" / "lsp-fixture";
    fs::create_directories(plugin_root / "workspace");
    const auto log_path = root / "lsp-log.jsonl";
    const auto server_path = plugin_root / "server.js";
    {
        std::ofstream server(server_path);
        server << R"JS(
const fs = require('node:fs');
const logPath = process.env.PLUGIN_LSP_LOG;
let buffer = Buffer.alloc(0);

function send(message) {
  const body = JSON.stringify(message);
  process.stdout.write(`Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`);
}

function handle(message) {
  if (logPath) {
    fs.appendFileSync(logPath, `${JSON.stringify(message)}\n`);
  }
  if (message.method === 'initialize') {
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: { capabilities: { textDocumentSync: 1 } },
    });
  }
  if (message.method === 'exit') {
    process.exit(0);
  }
}

process.stdin.on('data', chunk => {
  buffer = Buffer.concat([buffer, chunk]);
  while (true) {
    const headerEnd = buffer.indexOf('\r\n\r\n');
    if (headerEnd === -1) return;
    const header = buffer.subarray(0, headerEnd).toString();
    const match = /Content-Length:\s*(\d+)/i.exec(header);
    if (!match) process.exit(2);
    const length = Number(match[1]);
    const bodyStart = headerEnd + 4;
    if (buffer.length < bodyStart + length) return;
    const body = buffer.subarray(bodyStart, bodyStart + length).toString();
    buffer = buffer.subarray(bodyStart + length);
    handle(JSON.parse(body));
  }
});
process.stdin.resume();
)JS";
    }
    {
        std::ofstream settings(root / ".loom" / "settings.json");
        settings << R"JSON({
  "pluginConfigs": {
    "lsp-fixture": {
      "options": {
        "mode": "configured"
      }
    }
  }
})JSON";
    }
    {
        std::ofstream defaults(plugin_root / ".lsp.json");
        defaults << R"JSON({
  "fixture": {
    "command": "missing-lsp-command",
    "extensionToLanguage": {".foo": "foo-default"}
  }
})JSON";
    }
    {
        std::ofstream manifest(plugin_root / "plugin.json");
        manifest << R"JSON({
  "name": "lsp-fixture",
  "version": "1.0.0",
  "entry_point": "plugin.js",
  "lspServers": {
    "fixture": {
      "command": "node",
      "args": [
        "${LOOM_PLUGIN_ROOT}/server.js",
        "${user_config.mode}",
        "${PLUGIN_LSP_MISSING:-fallback}"
      ],
      "extensionToLanguage": {".foo": "foo-plugin"},
      "env": {
        "PLUGIN_LSP_MODE": "${user_config.mode}",
        "PLUGIN_LSP_ROOT": "${LOOM_PLUGIN_ROOT}",
        "PLUGIN_LSP_LOG": ")JSON" << log_path.string() << R"JSON("
      },
      "workspaceFolder": "${LOOM_PLUGIN_ROOT}/workspace",
      "initializationOptions": {"mode": "configured", "feature": true}
    }
  }
})JSON";
    }

    {
        CurrentPathGuard cwd(root);
        auto servers = cc::services::lsp::discover_plugin_lsp_servers();
        auto it = std::ranges::find_if(servers, [](const auto& server) {
            return server.name == "plugin:lsp-fixture:fixture";
        });
        ASSERT_NE(it, servers.end());
        EXPECT_EQ(it->config.command, "node");
        ASSERT_EQ(it->config.args.size(), 3u);
        EXPECT_EQ(it->config.args[0], server_path.string());
        EXPECT_EQ(it->config.args[1], "configured");
        EXPECT_EQ(it->config.args[2], "fallback");
        EXPECT_EQ(it->config.env.at("PLUGIN_LSP_MODE"), "configured");
        EXPECT_EQ(it->config.env.at("PLUGIN_LSP_ROOT"), plugin_root.string());
        EXPECT_EQ(it->config.env.at("PLUGIN_LSP_LOG"), log_path.string());
        EXPECT_EQ(it->config.env.at("LOOM_PLUGIN_ROOT"), plugin_root.string());
        EXPECT_EQ(
            it->config.env.at("LOOM_PLUGIN_DATA"),
            (root / ".loom" / "plugins" / "data" / "lsp-fixture").string()
        );
        ASSERT_TRUE(it->config.workspace_folder.has_value());
        EXPECT_EQ(*it->config.workspace_folder, (plugin_root / "workspace").string());
        EXPECT_NE(it->config.initialization_options_json.find("\"mode\":\"configured\""), std::string::npos);
        EXPECT_NE(it->config.initialization_options_json.find("\"feature\":true"), std::string::npos);
        EXPECT_EQ(it->config.extension_to_language.at("foo"), "foo-plugin");

        auto manager = cc::services::lsp::create_lsp_server_manager();
        auto initialized = manager->initialize();
        ASSERT_TRUE(initialized.has_value()) << initialized.error().message();
        auto* routed = manager->get_server_for_file((root / "sample.foo").string());
        ASSERT_NE(routed, nullptr);
        EXPECT_EQ(routed->name, "plugin:lsp-fixture:fixture");
        EXPECT_EQ(routed->config.extension_to_language.at("foo"), "foo-plugin");

        auto opened = manager->open_file((root / "sample.foo").string(), "let x = 1;");
        ASSERT_TRUE(opened.has_value()) << opened.error().message();

        std::string log;
        for (int attempt = 0; attempt < 50; ++attempt) {
            std::ifstream input(log_path);
            if (input) {
                std::stringstream buffer;
                buffer << input.rdbuf();
                log = buffer.str();
                if (log.find("\"method\":\"textDocument/didOpen\"") != std::string::npos) break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }

        const auto initialize_pos = log.find("\"method\":\"initialize\"");
        const auto initialized_pos = log.find("\"method\":\"initialized\"");
        const auto did_open_pos = log.find("\"method\":\"textDocument/didOpen\"");
        EXPECT_NE(initialize_pos, std::string::npos) << log;
        EXPECT_NE(initialized_pos, std::string::npos) << log;
        EXPECT_NE(did_open_pos, std::string::npos) << log;
        if (initialize_pos != std::string::npos &&
            initialized_pos != std::string::npos &&
            did_open_pos != std::string::npos) {
            EXPECT_LT(initialize_pos, initialized_pos);
            EXPECT_LT(initialized_pos, did_open_pos);
        }
        EXPECT_NE(log.find("\"initializationOptions\":{\"mode\":\"configured\",\"feature\":true}"), std::string::npos) << log;
        EXPECT_NE(log.find("\"rootPath\":\"" + (plugin_root / "workspace").string() + "\""), std::string::npos) << log;
        EXPECT_NE(log.find("\"languageId\":\"foo-plugin\""), std::string::npos) << log;

        auto shutdown = manager->shutdown();
        EXPECT_TRUE(shutdown.has_value());
    }

    fs::remove_all(root);
}

TEST(CcrClient, UsesDefaultHttpTransportForRemoteSessionLifecycle) {
    LocalCcrHttpServer server;
    ASSERT_TRUE(server.ready());

    cc::cli::CcrClient client;
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

TEST(SessionIngress, PostsSessionEventsWithBearerAuth) {
    LocalCcrHttpServer server;
    ASSERT_TRUE(server.ready());

    auto created = cc::services::api::create_ingress(cc::services::api::IngressConfig{
        .endpoint = server.base_url(),
        .session_id = "session_1",
        .auth_token = "session-jwt-token",
        .organization_uuid = std::nullopt,
    });
    ASSERT_TRUE(created.has_value()) << created.error();

    auto sent = cc::services::api::send_ingress_message(
        R"({"type":"control_response","response":{"request_id":"permission-1","subtype":"success"}})");
    ASSERT_TRUE(sent.has_value()) << sent.error();

    auto requests = server.wait_for_requests(1);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 1u);
    EXPECT_EQ((*requests)[0].method, "POST");
    EXPECT_EQ((*requests)[0].path, "/v1/sessions/session_1/events");
    EXPECT_NE((*requests)[0].headers.find("Authorization: Bearer session-jwt-token"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("events":[)"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("type":"control_response")"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("request_id":"permission-1")"), std::string::npos);

    cc::services::api::close_ingress();
}

TEST(SessionIngress, PostsSessionEventsWithSessionCookieAuth) {
    LocalCcrHttpServer server;
    ASSERT_TRUE(server.ready());

    auto created = cc::services::api::create_ingress(cc::services::api::IngressConfig{
        .endpoint = server.base_url(),
        .session_id = "session_1",
        .auth_token = "sk-ant-sid01-test",
        .organization_uuid = "org-uuid-1",
    });
    ASSERT_TRUE(created.has_value()) << created.error();

    auto sent = cc::services::api::send_ingress_message(R"({"type":"progress","value":0.5})");
    ASSERT_TRUE(sent.has_value()) << sent.error();

    auto requests = server.wait_for_requests(1);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 1u);
    EXPECT_EQ((*requests)[0].method, "POST");
    EXPECT_EQ((*requests)[0].path, "/v1/sessions/session_1/events");
    EXPECT_NE((*requests)[0].headers.find("Cookie: sessionKey=sk-ant-sid01-test"), std::string::npos);
    EXPECT_NE((*requests)[0].headers.find("X-Organization-Uuid: org-uuid-1"), std::string::npos);
    EXPECT_EQ((*requests)[0].headers.find("Authorization:"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("type":"progress")"), std::string::npos);

    cc::services::api::close_ingress();
}

TEST(SessionIngress, CreatesIngressFromDaemonEnvironmentAndSendsLifecycleEvent) {
    EnvironmentGuard endpoint("LOOM_REMOTE_API_BASE_URL", "placeholder");
    EnvironmentGuard session("CC_REMOTE_SESSION_ID", "session_1");
    EnvironmentGuard token("LOOM_SESSION_ACCESS_TOKEN", "env-session-token");
    EnvironmentUnsetGuard compat_endpoint("LOOM_REMOTE_API_BASE_URL");
    EnvironmentUnsetGuard ingress_endpoint("LOOM_SESSION_INGRESS_URL");
    EnvironmentUnsetGuard compat_ingress_endpoint("LOOM_SESSION_INGRESS_URL");
    EnvironmentUnsetGuard compat_session("LOOM_REMOTE_SESSION_ID");

    LocalCcrHttpServer server;
    ASSERT_TRUE(server.ready());
    setenv("LOOM_REMOTE_API_BASE_URL", server.base_url().c_str(), 1);

    cc::services::api::close_ingress();
    auto created = cc::services::api::create_ingress_from_environment();
    ASSERT_TRUE(created.has_value()) << created.error();
    EXPECT_TRUE(*created);
    EXPECT_TRUE(cc::services::api::is_ingress_active());

    auto sent = cc::services::api::send_ingress_lifecycle_event("started", std::string_view{"work_1"});
    ASSERT_TRUE(sent.has_value()) << sent.error();

    auto requests = server.wait_for_requests(1);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 1u);
    EXPECT_EQ((*requests)[0].method, "POST");
    EXPECT_EQ((*requests)[0].path, "/v1/sessions/session_1/events");
    EXPECT_NE((*requests)[0].headers.find("Authorization: Bearer env-session-token"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("type":"session_lifecycle")"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("status":"started")"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("bridge_work_id":"work_1")"), std::string::npos);

    cc::services::api::close_ingress();
}
TEST(CcrClient, StreamsMessagesThroughDefaultHttpTransport) {
    LocalCcrHttpServer server;
    ASSERT_TRUE(server.ready());

    cc::cli::CcrClient client;
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
    cc::cli::CcrClient client;
    client.set_http_transport([](const cc::cli::CcrHttpRequest&)
        -> std::expected<cc::cli::CcrHttpResponse, std::string> {
        return std::unexpected("network down");
    });

    auto connected = client.connect("https://remote.example/api", "ccr-token");
    ASSERT_FALSE(connected.has_value());
    EXPECT_NE(connected.error().find("Remote session handshake failed: network down"), std::string::npos);
    EXPECT_FALSE(client.is_connected());
}

TEST(ApiErrors, ClassifiesHttpStatusCodes) {
    using cc::services::api::errors::ApiErrorCategory;
    using cc::services::api::errors::ErrorClassifier;

    EXPECT_EQ(ErrorClassifier::classify_status(401), ApiErrorCategory::Authentication);
    EXPECT_EQ(ErrorClassifier::classify_status(429), ApiErrorCategory::RateLimited);
    EXPECT_EQ(ErrorClassifier::classify_status(529), ApiErrorCategory::Overloaded);
    EXPECT_EQ(ErrorClassifier::classify_status(500), ApiErrorCategory::ServerError);
    EXPECT_EQ(ErrorClassifier::classify_status(400), ApiErrorCategory::InvalidRequest);
}

TEST(ApiErrors, RetryDecisionUsesRetryableCategories) {
    using cc::services::api::errors::ApiErrorCategory;
    using cc::services::api::errors::ApiErrorDetails;
    using cc::services::api::errors::ErrorClassifier;

    ApiErrorDetails rate_limited{};
    rate_limited.category = ApiErrorCategory::RateLimited;
    rate_limited.http_status = 429;
    ApiErrorDetails bad_request{};
    bad_request.category = ApiErrorCategory::InvalidRequest;
    bad_request.http_status = 400;

    EXPECT_TRUE(ErrorClassifier::is_retryable(rate_limited));
    EXPECT_FALSE(ErrorClassifier::is_retryable(bad_request));
}

TEST(ApiErrors, ClientMapsJsonHttpErrorsToStructuredMessages) {
    auto error = cc::services::api::AnthropicClient::error_from_http_response(
        400,
        R"({"type":"error","error":{"type":"invalid_request_error","message":"prompt is too long"}})",
        std::optional<std::string>{"req_123"});

    EXPECT_EQ(error.code(), cc::utils::ErrorCode::invalid_argument);
    EXPECT_NE(error.message().find("HTTP 400 invalid_request_error: prompt is too long"), std::string::npos);
    EXPECT_NE(error.message().find("req_123"), std::string::npos);
}

TEST(ApiErrors, ClientPreservesRetryAfterFromJsonHttpErrors) {
    auto error = cc::services::api::AnthropicClient::error_from_http_response(
        429,
        R"({"error":{"type":"rate_limit_error","message":"too many requests","retry_after_seconds":7}})");

    EXPECT_EQ(error.code(), cc::utils::ErrorCode::resource_exhausted);
    EXPECT_NE(error.message().find("rate_limit_error: too many requests"), std::string::npos);
    EXPECT_NE(error.message().find("retry after: 7s"), std::string::npos);
}

TEST(ApiErrors, ClientErrorDetailsDriveRetryClassification) {
    using cc::services::api::errors::ApiErrorCategory;
    using cc::services::api::errors::ErrorClassifier;

    auto invalid_error = cc::services::api::AnthropicClient::error_from_http_response(
        400,
        R"({"error":{"type":"invalid_request_error","message":"bad tool schema"}})");
    auto invalid_details = cc::services::api::AnthropicClient::error_details_from_error(invalid_error);

    EXPECT_EQ(invalid_details.category, ApiErrorCategory::InvalidRequest);
    EXPECT_EQ(invalid_details.http_status, 400);
    EXPECT_EQ(invalid_details.error_type, "invalid_request_error");
    EXPECT_FALSE(ErrorClassifier::is_retryable(invalid_details));

    auto rate_limit_error = cc::services::api::AnthropicClient::error_from_http_response(
        429,
        R"({"error":{"type":"rate_limit_error","message":"too many requests","retry_after_seconds":7}})");
    auto rate_limit_details = cc::services::api::AnthropicClient::error_details_from_error(rate_limit_error);

    EXPECT_EQ(rate_limit_details.category, ApiErrorCategory::RateLimited);
    EXPECT_EQ(rate_limit_details.retry_after_seconds, std::optional<int>{7});
    EXPECT_TRUE(ErrorClassifier::is_retryable(rate_limit_details));
}

TEST(QueryEngine, AppliesPerQueryEnabledToolsToAnthropicRequest) {
    LocalAnthropicMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-engine-tool-filter-test";
    fs::remove_all(root);
    fs::create_directories(root);

    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Disabled;
    config.tools = {
        cc::core::ToolDefinition{
            .name = "Read",
            .description = "Read a file",
            .input_schema = cc::core::InputSchema{
                .properties = {
                    cc::core::SchemaProperty{
                        .name = "file_path",
                        .type = "string",
                        .description = "File path",
                        .required = true,
                    },
                },
            },
            .permission = cc::core::ToolPermission::ReadOnly,
        },
        cc::core::ToolDefinition{
            .name = "Write",
            .description = "Write a file",
            .input_schema = cc::core::InputSchema{
                .properties = {
                    cc::core::SchemaProperty{
                        .name = "file_path",
                        .type = "string",
                        .description = "File path",
                        .required = true,
                    },
                },
            },
            .permission = cc::core::ToolPermission::Write,
        },
    };

    cc::core::QueryEngine engine(std::move(config), registry);
    cc::core::QueryOptions options;
    options.enabled_tools = {"Read"};

    auto response = engine.query("hello", options);
    ASSERT_TRUE(response.has_value()) << response.error().message;
    EXPECT_EQ(response->message.model, "loom-test");

    auto request_body = server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    auto parsed = cc::utils::json::parse(*request_body);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();

    auto tools = parsed->root().get("tools");
    ASSERT_TRUE(tools.valid());
    ASSERT_TRUE(tools.is_arr());

    std::vector<std::string> tool_names;
    tools.iter([&](cc::utils::json::JsonVal tool) {
        tool_names.emplace_back(tool.get("name").as_str());
    });

    ASSERT_EQ(tool_names.size(), 1u) << *request_body;
    EXPECT_EQ(tool_names.front(), "Read");
    EXPECT_FALSE(parsed->root().get("stream").as_bool());

    fs::remove_all(root);
}

TEST(QueryEngine, SnipMetadataProjectsRemovedMessagesFromAnthropicRequest) {
    LocalAnthropicMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-engine-snip-projection-test";
    fs::remove_all(root);
    fs::create_directories(root);

    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Disabled;
    cc::core::QueryEngine engine(std::move(config), registry);

    cc::core::UserMessage old_user{};
    old_user.id.value = "snipped-user-id";
    old_user.timestamp = std::chrono::system_clock::now();
    old_user.content.push_back(cc::core::TextBlock{"SNIPPED_USER_PAYLOAD_DO_NOT_SEND"});
    engine.append_message_for_testing(cc::core::Message{std::move(old_user)});

    cc::core::AssistantMessage old_assistant{};
    old_assistant.id.value = "snipped-assistant-id";
    old_assistant.timestamp = std::chrono::system_clock::now();
    old_assistant.content.push_back(cc::core::TextBlock{"SNIPPED_ASSISTANT_PAYLOAD_DO_NOT_SEND"});
    engine.append_message_for_testing(cc::core::Message{std::move(old_assistant)});

    cc::core::SystemMessage snip_boundary{};
    snip_boundary.id.value = "snip-boundary-id";
    snip_boundary.timestamp = std::chrono::system_clock::now();
    snip_boundary.subtype = "snip_boundary";
    snip_boundary.content.push_back(cc::core::TextBlock{"Conversation snipped."});
    snip_boundary.snip_metadata = cc::core::SnipMetadata{
        .removed_uuids = {"snipped-user-id", "snipped-assistant-id"},
    };
    engine.append_message_for_testing(cc::core::Message{std::move(snip_boundary)});

    cc::core::UserMessage survivor{};
    survivor.id.value = "survivor-user-id";
    survivor.timestamp = std::chrono::system_clock::now();
    survivor.content.push_back(cc::core::TextBlock{"SURVIVOR_PAYLOAD_SHOULD_SEND"});
    engine.append_message_for_testing(cc::core::Message{std::move(survivor)});

    auto response = engine.query("fresh prompt after snip");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_body = server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    EXPECT_EQ(request_body->find("SNIPPED_USER_PAYLOAD_DO_NOT_SEND"), std::string::npos)
        << *request_body;
    EXPECT_EQ(request_body->find("SNIPPED_ASSISTANT_PAYLOAD_DO_NOT_SEND"), std::string::npos)
        << *request_body;
    EXPECT_EQ(request_body->find("snipped-user-id"), std::string::npos) << *request_body;
    EXPECT_EQ(request_body->find("snipped-assistant-id"), std::string::npos) << *request_body;
    EXPECT_NE(request_body->find("SURVIVOR_PAYLOAD_SHOULD_SEND"), std::string::npos)
        << *request_body;
    EXPECT_NE(request_body->find("fresh prompt after snip"), std::string::npos)
        << *request_body;

    auto conversation = engine.get_conversation();
    auto contains_message_id = [&](std::string_view id) {
        return std::ranges::any_of(conversation, [&](const cc::core::Message& msg) {
            return std::visit([&](const auto& value) {
                return value.id.value == id;
            }, msg);
        });
    };
    EXPECT_FALSE(contains_message_id("snipped-user-id"));
    EXPECT_FALSE(contains_message_id("snipped-assistant-id"));
    EXPECT_TRUE(contains_message_id("snip-boundary-id"));
    EXPECT_TRUE(contains_message_id("survivor-user-id"));

    fs::remove_all(root);
}

TEST(ApiMicrocompact, BuildsThinkingAndToolContextManagementStrategies) {
    EnvironmentGuard user_type_guard("USER_TYPE", "ant");
    EnvironmentGuard clear_results_guard("USE_API_CLEAR_TOOL_RESULTS", "1");
    EnvironmentGuard clear_uses_guard("USE_API_CLEAR_TOOL_USES", "true");
    EnvironmentGuard max_tokens_guard("API_MAX_INPUT_TOKENS", "1000");
    EnvironmentGuard target_tokens_guard("API_TARGET_INPUT_TOKENS", "250");

    auto context = cc::services::compact::get_api_context_management({
        .has_thinking = true,
        .is_redact_thinking_active = false,
        .clear_all_thinking = true,
    });

    ASSERT_TRUE(context.has_value());
    ASSERT_EQ(context->edits.size(), 3u);

    EXPECT_EQ(context->edits[0].type, "clear_thinking_20251015");
    EXPECT_TRUE(context->edits[0].has_thinking_keep);
    ASSERT_TRUE(context->edits[0].keep_thinking_turns.has_value());
    EXPECT_EQ(*context->edits[0].keep_thinking_turns, 1u);

    EXPECT_EQ(context->edits[1].type, "clear_tool_uses_20250919");
    ASSERT_TRUE(context->edits[1].trigger_input_tokens.has_value());
    ASSERT_TRUE(context->edits[1].clear_at_least_input_tokens.has_value());
    EXPECT_EQ(*context->edits[1].trigger_input_tokens, 1000u);
    EXPECT_EQ(*context->edits[1].clear_at_least_input_tokens, 750u);
    EXPECT_TRUE(std::ranges::contains(context->edits[1].clear_tool_inputs, std::string{"Bash"}));
    EXPECT_TRUE(std::ranges::contains(context->edits[1].clear_tool_inputs, std::string{"Read"}));

    EXPECT_EQ(context->edits[2].type, "clear_tool_uses_20250919");
    EXPECT_TRUE(std::ranges::contains(context->edits[2].exclude_tools, std::string{"Edit"}));
    EXPECT_TRUE(std::ranges::contains(context->edits[2].exclude_tools, std::string{"Write"}));
    EXPECT_TRUE(std::ranges::contains(context->edits[2].exclude_tools, std::string{"NotebookEdit"}));
}

TEST(QueryEngine, SerializesTaskBudgetAndApiContextManagementRequestConfig) {
    EnvironmentUnsetGuard disable_thinking_guard("LOOM_DISABLE_THINKING");
    EnvironmentGuard user_type_guard("USER_TYPE", "ant");
    EnvironmentGuard clear_results_guard("USE_API_CLEAR_TOOL_RESULTS", "1");
    EnvironmentGuard clear_uses_guard("USE_API_CLEAR_TOOL_USES", "1");
    EnvironmentGuard max_tokens_guard("API_MAX_INPUT_TOKENS", "1000");
    EnvironmentGuard target_tokens_guard("API_TARGET_INPUT_TOKENS", "250");

    LocalAnthropicMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-engine-task-budget-context-management-test";
    fs::remove_all(root);
    fs::create_directories(root);

    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Adaptive;
    config.task_budget = cc::core::QueryEngineConfig::TaskBudget{
        .total = 12'000,
        .remaining = 6'000,
    };

    cc::core::QueryEngine engine(std::move(config), registry);
    auto response = engine.query("hello");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_body = server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    auto parsed = cc::utils::json::parse(*request_body);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();

    auto output_config = parsed->root().get("output_config");
    ASSERT_TRUE(output_config.is_obj()) << *request_body;
    auto task_budget = output_config.get("task_budget");
    ASSERT_TRUE(task_budget.is_obj()) << *request_body;
    EXPECT_EQ(task_budget.get("type").as_str(), "tokens");
    EXPECT_EQ(task_budget.get("total").as_int(), 12'000);
    EXPECT_EQ(task_budget.get("remaining").as_int(), 6'000);

    auto context_management = parsed->root().get("context_management");
    ASSERT_TRUE(context_management.is_obj()) << *request_body;
    auto edits = context_management.get("edits");
    ASSERT_TRUE(edits.is_arr()) << *request_body;
    ASSERT_EQ(edits.size(), 3u) << *request_body;

    EXPECT_EQ(edits.at(0).get("type").as_str(), "clear_thinking_20251015");
    EXPECT_EQ(edits.at(0).get("keep").as_str(), "all");
    EXPECT_EQ(edits.at(1).get("trigger").get("value").as_int(), 1000);
    EXPECT_EQ(edits.at(1).get("clear_at_least").get("value").as_int(), 750);
    EXPECT_TRUE(edits.at(1).get("clear_tool_inputs").is_arr());
    EXPECT_TRUE(edits.at(2).get("exclude_tools").is_arr());

    auto headers = server.wait_for_headers();
    ASSERT_TRUE(headers.has_value());
    EXPECT_NE(headers->find("task-budgets-2026-03-13"), std::string::npos) << *headers;
    EXPECT_NE(headers->find("context-management-2025-06-27"), std::string::npos) << *headers;

    fs::remove_all(root);
}

TEST(QueryEngine, DisableThinkingEnvSuppressesThinkingAndClearThinkingContextManagement) {
    EnvironmentGuard disable_thinking_guard("LOOM_DISABLE_THINKING", "1");
    EnvironmentUnsetGuard user_type_guard("USER_TYPE");
    EnvironmentUnsetGuard clear_results_guard("USE_API_CLEAR_TOOL_RESULTS");
    EnvironmentUnsetGuard clear_uses_guard("USE_API_CLEAR_TOOL_USES");

    LocalAnthropicMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-engine-disable-thinking-test";
    fs::remove_all(root);
    fs::create_directories(root);

    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Adaptive;

    cc::core::QueryEngine engine(std::move(config), registry);
    auto response = engine.query("hello");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_body = server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    auto parsed = cc::utils::json::parse(*request_body);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    EXPECT_FALSE(parsed->root().get("thinking").valid()) << *request_body;
    EXPECT_FALSE(parsed->root().get("context_management").valid()) << *request_body;

    auto headers = server.wait_for_headers();
    ASSERT_TRUE(headers.has_value());
    EXPECT_EQ(headers->find("context-management-2025-06-27"), std::string::npos)
        << *headers;

    fs::remove_all(root);
}

TEST(QueryEngine, InjectsPendingNativeAgentTaskNotificationsIntoRequest) {
    LocalAnthropicMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-engine-agent-notification-test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    cc::tools::agent_runtime::native_agent_store().clear_for_testing();

    cc::tools::agent_runtime::native_agent_store().upsert(cc::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "query-notify-agent",
        .agent_type = "general-purpose",
        .description = "Query notify agent",
        .background = true,
        .status = cc::tools::agent_runtime::NativeAgentStatus::Completed,
        .output = std::string("native agent completed with useful context"),
    });

    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Disabled;

    cc::core::QueryEngine engine(std::move(config), registry);
    auto first_response = engine.query("hello");
    ASSERT_TRUE(first_response.has_value()) << first_response.error().message;

    auto first_body = server.wait_for_body();
    ASSERT_TRUE(first_body.has_value());
    auto first_json = cc::utils::json::parse(*first_body);
    ASSERT_TRUE(first_json.has_value()) << first_json.error().message();

    auto message_text = [](cc::utils::json::JsonVal message) {
        auto content = message.get("content");
        if (content.is_str()) return std::string(content.as_str());
        std::string text;
        if (content.is_arr()) {
            content.iter([&](cc::utils::json::JsonVal block) {
                auto block_text = block.get("text");
                if (block_text.is_str()) text += block_text.as_str();
            });
        }
        return text;
    };
    auto notification_count = [&](cc::utils::json::JsonVal messages) {
        std::size_t count = 0;
        messages.iter([&](cc::utils::json::JsonVal message) {
            if (message_text(message).find("<task_notification>") != std::string::npos) {
                ++count;
            }
        });
        return count;
    };

    auto first_messages = first_json->root().get("messages");
    ASSERT_TRUE(first_messages.is_arr()) << *first_body;
    ASSERT_EQ(notification_count(first_messages), 1u) << *first_body;

    bool saw_user_notification = false;
    first_messages.iter([&](cc::utils::json::JsonVal message) {
        const auto text = message_text(message);
        if (text.find("<task_notification>") == std::string::npos) return;
        EXPECT_EQ(std::string(message.get("role").as_str()), "user");
        EXPECT_NE(text.find("<task_id>query-notify-agent</task_id>"), std::string::npos);
        EXPECT_NE(text.find("<status>completed</status>"), std::string::npos);
        EXPECT_NE(text.find("native agent completed with useful context"), std::string::npos);
        saw_user_notification = true;
    });
    EXPECT_TRUE(saw_user_notification);

    auto delivered_record = cc::tools::agent_runtime::native_agent_store().get("query-notify-agent");
    ASSERT_TRUE(delivered_record.has_value());
    EXPECT_TRUE(delivered_record->notification_delivered);
    EXPECT_TRUE(cc::tools::agent_runtime::native_agent_store().take_pending_task_notifications().empty());

    auto second_response = engine.query("follow up");
    ASSERT_TRUE(second_response.has_value()) << second_response.error().message;
    auto request_bodies = server.wait_for_bodies(2);
    ASSERT_TRUE(request_bodies.has_value());
    ASSERT_EQ(request_bodies->size(), 2u);

    auto second_json = cc::utils::json::parse(request_bodies->back());
    ASSERT_TRUE(second_json.has_value()) << second_json.error().message();
    auto second_messages = second_json->root().get("messages");
    ASSERT_TRUE(second_messages.is_arr()) << request_bodies->back();
    EXPECT_EQ(notification_count(second_messages), 1u) << request_bodies->back();

    cc::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(QueryEngine, PersistsTranscriptToSessionStorage) {
    auto dir = fs::temp_directory_path() / "loom_qe_session_persist_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    config.model_params.model = "test-model";
    cc::core::QueryEngine engine(std::move(config), registry);
    engine.set_session_storage(dir);

    auto make_user = [](std::string text) {
        cc::core::UserMessage msg{};
        msg.content.push_back(cc::core::TextBlock{std::move(text)});
        return cc::core::Message{std::move(msg)};
    };
    auto make_assistant = [](std::string text) {
        cc::core::AssistantMessage msg{};
        msg.content.push_back(cc::core::TextBlock{std::move(text)});
        return cc::core::Message{std::move(msg)};
    };

    engine.append_message_for_testing(make_user("hello world"));
    engine.append_message_for_testing(make_assistant("hi there"));

    const auto msgs_path = dir / engine.session_id().str() / "messages.jsonl";
    ASSERT_TRUE(fs::exists(msgs_path)) << msgs_path;

    std::vector<std::string> lines;
    {
        std::ifstream ifs(msgs_path);
        std::string line;
        while (std::getline(ifs, line)) if (!line.empty()) lines.push_back(line);
    }
    // Two non-system messages -> two transcript lines.
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_NE(lines[0].find("hello world"), std::string::npos);
    EXPECT_NE(lines[0].find("user"), std::string::npos);
    EXPECT_NE(lines[1].find("hi there"), std::string::npos);
    EXPECT_NE(lines[1].find("assistant"), std::string::npos);

    // Metadata discoverable via list_recent_sessions.
    auto sessions = cc::session::list_recent_sessions(dir);
    EXPECT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.front().session_id, engine.session_id().str());
    EXPECT_EQ(sessions.front().model, "test-model");

    // flush_session refreshes metadata message_count.
    engine.flush_session();
    auto sessions2 = cc::session::list_recent_sessions(dir);
    ASSERT_EQ(sessions2.size(), 1u);
    EXPECT_EQ(sessions2.front().message_count, 2);

    fs::remove_all(dir);
}

TEST(QueryEngine, StructuredOutputInjectsResponseSchema) {
    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.response_schema = cc::core::QueryEngineConfig::ResponseSchema{
        .name = "result",
        .schema_json = R"({"type":"object","properties":{"answer":{"type":"string"}},"required":["answer"]})",
    };
    cc::core::QueryEngine engine(std::move(config), registry);

    const auto out = engine.build_output_config_json_for_testing();
    EXPECT_NE(out.find("output_config"), std::string::npos);
    EXPECT_NE(out.find("json_schema"), std::string::npos);
    EXPECT_NE(out.find("\"result\""), std::string::npos);
    EXPECT_NE(out.find("answer"), std::string::npos);

    // Without a schema and without a budget, output_config is omitted.
    cc::core::QueryEngineConfig bare;
    bare.context_window.auto_compact = false;
    cc::core::QueryEngine bare_engine(std::move(bare), registry);
    EXPECT_EQ(bare_engine.build_output_config_json_for_testing(), "{}");
}

TEST(QueryEngine, TracksInvokedSkillsInLoop) {
    // In-loop skill dispatch: when the skill tool is invoked, the engine
    // records the skill name in discovered_skills_ (previously a dead field).
    struct StubSkillTool final : cc::core::ITool {
        cc::core::ToolDefinition definition_{};
        StubSkillTool() {
            definition_.name = "skill";
            definition_.permission = cc::core::ToolPermission::ReadOnly;
        }
        [[nodiscard]] const cc::core::ToolDefinition& definition() const override { return definition_; }
        [[nodiscard]] cc::core::Result<cc::core::ToolResult> execute(const cc::core::ToolInput&) override {
            return cc::core::ToolResult::success("ok");
        }
        [[nodiscard]] bool check_permission(const cc::core::ToolInput&) const override { return true; }
    };

    cc::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<StubSkillTool>());
    cc::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    cc::core::QueryEngine engine(std::move(config), registry);

    cc::core::ToolUseBlock tu{
        .id = cc::core::ToolUseId{.value = "tu-1"},
        .name = "skill",
        .input_json = R"({"name":"my-test-skill"})",
    };
    (void)engine.execute_single_tool_for_testing(tu);

    const auto skills = engine.discovered_skills();
    EXPECT_NE(std::find(skills.begin(), skills.end(), "my-test-skill"), skills.end())
        << "skill invocation should be tracked in discovered_skills_";
}

TEST(QueryEngine, CompactionPersistsSessionSummaryForResumedSession) {
    auto root = fs::temp_directory_path() /
        ("loom-session-summary-" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard home_guard("HOME", root.string());

    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = (root / "work").string();
    config.session_id_override = "resume-session-id";
    fs::create_directories(root / "work");

    const auto summary_path = cc::memdir::get_session_memory_path(
        root / "work", "resume-session-id");

    {
        cc::core::QueryEngine engine(config, registry);
        auto make_user = [](std::string text) {
            cc::core::UserMessage msg{};
            msg.id.value = "user-" + text;
            msg.timestamp = std::chrono::system_clock::now();
            msg.content.push_back(cc::core::TextBlock{std::move(text)});
            return cc::core::Message{std::move(msg)};
        };
        auto make_assistant = [](std::string text) {
            cc::core::AssistantMessage msg{};
            msg.id.value = "assistant-" + text;
            msg.timestamp = std::chrono::system_clock::now();
            msg.content.push_back(cc::core::TextBlock{std::move(text)});
            return cc::core::Message{std::move(msg)};
        };
        // > keep_recent(6) + system messages so compaction actually runs.
        engine.append_message_for_testing(make_user("legacy requirement alpha"));
        engine.append_message_for_testing(make_assistant("assistant decision beta"));
        engine.append_message_for_testing(make_user("design constraint gamma"));
        engine.append_message_for_testing(make_assistant("assistant decision delta"));
        engine.append_message_for_testing(make_user("recent one"));
        engine.append_message_for_testing(make_assistant("recent two"));
        engine.append_message_for_testing(make_user("recent three"));
        engine.append_message_for_testing(make_assistant("recent four"));
        engine.append_message_for_testing(make_user("recent five"));
        engine.append_message_for_testing(make_assistant("recent six"));

        ASSERT_TRUE(engine.compact_conversation().has_value());
    }

    ASSERT_TRUE(std::filesystem::exists(summary_path));
    std::ifstream ifs(summary_path);
    std::string persisted((std::istreambuf_iterator<char>(ifs)), {});
    EXPECT_NE(persisted.find("legacy requirement alpha"), std::string::npos);
    EXPECT_NE(persisted.find("Compaction"), std::string::npos);

    // A fresh engine with the same session id injects the persisted summary
    // into its system prompt (resumed session does not start blind).
    cc::core::QueryEngine resumed(config, registry);
    const std::string body = resumed.build_request_body_for_testing();
    EXPECT_NE(body.find("session-memory"), std::string::npos) << body;
    EXPECT_NE(body.find("legacy requirement alpha"), std::string::npos) << body;

    fs::remove_all(root);
}

TEST(QueryEngine, CompactConversationPreservesSummarizedHistoryDetails) {
    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();

    cc::core::QueryEngine engine(std::move(config), registry);

    auto make_user = [](std::string text) {
        cc::core::UserMessage msg{};
        msg.id.value = "user-" + text;
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(cc::core::TextBlock{std::move(text)});
        return cc::core::Message{std::move(msg)};
    };
    auto make_assistant = [](std::string text) {
        cc::core::AssistantMessage msg{};
        msg.id.value = "assistant-" + text;
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(cc::core::TextBlock{std::move(text)});
        return cc::core::Message{std::move(msg)};
    };

    engine.append_message_for_testing(make_user("legacy requirement alpha"));
    engine.append_message_for_testing(make_assistant("assistant decision beta"));
    engine.append_message_for_testing(make_user("tool context gamma"));
    engine.append_message_for_testing(make_assistant("design constraint delta"));
    engine.append_message_for_testing(make_user("recent one"));
    engine.append_message_for_testing(make_assistant("recent two"));
    engine.append_message_for_testing(make_user("recent three"));
    engine.append_message_for_testing(make_assistant("recent four"));
    engine.append_message_for_testing(make_user("recent five"));
    engine.append_message_for_testing(make_assistant("recent six"));

    auto compacted = engine.compact_conversation();
    ASSERT_TRUE(compacted.has_value());

    auto conversation = engine.get_conversation();
    ASSERT_EQ(conversation.size(), 9u);

    const auto* boundary = std::get_if<cc::core::SystemMessage>(&conversation[1]);
    ASSERT_NE(boundary, nullptr);
    ASSERT_TRUE(boundary->subtype.has_value());
    EXPECT_EQ(*boundary->subtype, "compact_boundary");
    ASSERT_TRUE(boundary->compact_metadata.has_value());
    EXPECT_EQ(boundary->compact_metadata->trigger, "manual");
    EXPECT_GT(boundary->compact_metadata->pre_tokens, 0u);
    ASSERT_TRUE(boundary->compact_metadata->preserved_segment.has_value());
    EXPECT_EQ(boundary->compact_metadata->preserved_segment->head_uuid, "user-recent one");
    EXPECT_EQ(boundary->compact_metadata->preserved_segment->tail_uuid, "assistant-recent six");

    const auto* marker = std::get_if<cc::core::UserMessage>(&conversation[2]);
    ASSERT_NE(marker, nullptr);
    ASSERT_EQ(marker->content.size(), 1u);
    const auto* summary = std::get_if<cc::core::TextBlock>(&marker->content.front());
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(boundary->compact_metadata->preserved_segment->anchor_uuid, marker->id.value);

    EXPECT_NE(summary->text.find("legacy requirement alpha"), std::string::npos);
    EXPECT_NE(summary->text.find("assistant decision beta"), std::string::npos);
    EXPECT_NE(summary->text.find("Preserve these details"), std::string::npos);

    const auto* last = std::get_if<cc::core::AssistantMessage>(&conversation.back());
    ASSERT_NE(last, nullptr);
    const auto* last_text = std::get_if<cc::core::TextBlock>(&last->content.front());
    ASSERT_NE(last_text, nullptr);
    EXPECT_EQ(last_text->text, "recent six");
}

TEST(QueryEngine, CompactConversationCarriesTaskBudgetRemainingIntoNextRequest) {
    LocalAnthropicMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-task-budget-compact-carry-test";
    fs::remove_all(root);
    fs::create_directories(root);

    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Disabled;
    config.task_budget = cc::core::QueryEngineConfig::TaskBudget{
        .total = 10'000,
        .remaining = std::nullopt,
    };

    cc::core::QueryEngine engine(std::move(config), registry);

    auto make_user = [](std::string text) {
        cc::core::UserMessage msg{};
        msg.id.value = "user-" + text.substr(0, 12);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(cc::core::TextBlock{std::move(text)});
        return cc::core::Message{std::move(msg)};
    };
    auto make_assistant = [](std::string text) {
        cc::core::AssistantMessage msg{};
        msg.id.value = "assistant-" + text.substr(0, 12);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(cc::core::TextBlock{std::move(text)});
        return cc::core::Message{std::move(msg)};
    };

    for (int i = 0; i < 5; ++i) {
        engine.append_message_for_testing(make_user("legacy user context " + std::to_string(i) + std::string(160, 'u')));
        engine.append_message_for_testing(make_assistant("legacy assistant context " + std::to_string(i) + std::string(160, 'a')));
    }

    auto compacted = engine.compact_conversation("auto");
    ASSERT_TRUE(compacted.has_value());

    auto conversation = engine.get_conversation();
    auto boundary_it = std::ranges::find_if(conversation, [](const cc::core::Message& message) {
        const auto* system = std::get_if<cc::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    ASSERT_NE(boundary_it, conversation.end());
    const auto* boundary = std::get_if<cc::core::SystemMessage>(&*boundary_it);
    ASSERT_NE(boundary, nullptr);
    ASSERT_TRUE(boundary->compact_metadata.has_value());
    const auto pre_tokens = boundary->compact_metadata->pre_tokens;
    ASSERT_GT(pre_tokens, 0u);
    ASSERT_LT(pre_tokens, 10'000u);

    auto response = engine.query("continue after compact");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_body = server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    auto parsed = cc::utils::json::parse(*request_body);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    auto task_budget = parsed->root().get("output_config").get("task_budget");
    ASSERT_TRUE(task_budget.is_obj()) << *request_body;
    EXPECT_EQ(task_budget.get("total").as_int(), 10'000);
    EXPECT_EQ(task_budget.get("remaining").as_int(), 10'000 - pre_tokens);

    fs::remove_all(root);
}

TEST(QueryEngine, RestoreConversationDerivesTaskBudgetRemainingFromCompactBoundaryMetadata) {
    auto root = fs::temp_directory_path() / "loom-query-task-budget-restore-test";
    fs::remove_all(root);
    fs::create_directories(root);

    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Disabled;
    config.task_budget = cc::core::QueryEngineConfig::TaskBudget{
        .total = 10'000,
        .remaining = std::nullopt,
    };

    cc::core::QueryEngine engine(std::move(config), registry);

    auto make_user = [](std::string text) {
        cc::core::UserMessage msg{};
        msg.id.value = "restore-user-" + text.substr(0, 12);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(cc::core::TextBlock{std::move(text)});
        return cc::core::Message{std::move(msg)};
    };
    auto make_assistant = [](std::string text) {
        cc::core::AssistantMessage msg{};
        msg.id.value = "restore-assistant-" + text.substr(0, 12);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(cc::core::TextBlock{std::move(text)});
        return cc::core::Message{std::move(msg)};
    };

    for (int i = 0; i < 5; ++i) {
        engine.append_message_for_testing(make_user("restored legacy user context " + std::to_string(i) + std::string(160, 'u')));
        engine.append_message_for_testing(make_assistant("restored legacy assistant context " + std::to_string(i) + std::string(160, 'a')));
    }

    auto compacted = engine.compact_conversation("auto");
    ASSERT_TRUE(compacted.has_value());

    auto restored_messages = engine.get_conversation();
    auto boundary_it = std::ranges::find_if(restored_messages, [](const cc::core::Message& message) {
        const auto* system = std::get_if<cc::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    ASSERT_NE(boundary_it, restored_messages.end());
    const auto* boundary = std::get_if<cc::core::SystemMessage>(&*boundary_it);
    ASSERT_NE(boundary, nullptr);
    ASSERT_TRUE(boundary->compact_metadata.has_value());
    const auto pre_tokens = boundary->compact_metadata->pre_tokens;
    ASSERT_GT(pre_tokens, 0u);
    ASSERT_LT(pre_tokens, 10'000u);

    LocalAnthropicMessagesServer resumed_server;
    ASSERT_NE(resumed_server.port(), 0);
    cc::core::ToolRegistry restored_registry;
    cc::core::QueryEngineConfig restored_config;
    restored_config.api_key = "test-key";
    restored_config.base_url = resumed_server.base_url();
    restored_config.context_window.auto_compact = false;
    restored_config.cwd = root.string();
    restored_config.retry_policy.max_retries = 0;
    restored_config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Disabled;
    restored_config.task_budget = cc::core::QueryEngineConfig::TaskBudget{
        .total = 10'000,
        .remaining = std::nullopt,
    };

    cc::core::QueryEngine restored_engine(std::move(restored_config), restored_registry);
    restored_engine.restore_conversation(std::move(restored_messages));

    auto response = restored_engine.query("continue after restore");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_body = resumed_server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    auto parsed = cc::utils::json::parse(*request_body);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    auto task_budget = parsed->root().get("output_config").get("task_budget");
    ASSERT_TRUE(task_budget.is_obj()) << *request_body;
    EXPECT_EQ(task_budget.get("total").as_int(), 10'000);
    EXPECT_EQ(task_budget.get("remaining").as_int(), 10'000 - pre_tokens);

    fs::remove_all(root);
}

TEST(QueryEngine, RepeatedCompactDoesNotSummarizePriorCompactBoundaries) {
    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();

    cc::core::QueryEngine engine(std::move(config), registry);

    auto make_user = [](std::string text) {
        cc::core::UserMessage msg{};
        msg.id.value = "repeat-user-" + text;
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(cc::core::TextBlock{std::move(text)});
        return cc::core::Message{std::move(msg)};
    };
    auto make_assistant = [](std::string text) {
        cc::core::AssistantMessage msg{};
        msg.id.value = "repeat-assistant-" + text;
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(cc::core::TextBlock{std::move(text)});
        return cc::core::Message{std::move(msg)};
    };

    for (int i = 0; i < 10; ++i) {
        engine.append_message_for_testing(i % 2 == 0
            ? make_user("initial " + std::to_string(i))
            : make_assistant("initial " + std::to_string(i)));
    }

    auto first = engine.compact_conversation();
    ASSERT_TRUE(first.has_value());

    for (int i = 0; i < 6; ++i) {
        engine.append_message_for_testing(i % 2 == 0
            ? make_user("second wave " + std::to_string(i))
            : make_assistant("second wave " + std::to_string(i)));
    }

    auto second = engine.compact_conversation();
    ASSERT_TRUE(second.has_value());

    auto conversation = engine.get_conversation();
    auto boundary_count = std::ranges::count_if(conversation, [](const cc::core::Message& message) {
        const auto* system = std::get_if<cc::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    EXPECT_EQ(boundary_count, 1);

    const auto* marker = std::get_if<cc::core::UserMessage>(&conversation.at(2));
    ASSERT_NE(marker, nullptr);
    ASSERT_EQ(marker->content.size(), 1u);
    const auto* summary = std::get_if<cc::core::TextBlock>(&marker->content.front());
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text.find("Conversation compacted by manual compact."), std::string::npos);
    EXPECT_NE(summary->text.find("initial 0"), std::string::npos);

    bool kept_second_wave = false;
    for (const auto& message : conversation) {
        const auto* user = std::get_if<cc::core::UserMessage>(&message);
        if (!user || user->content.empty()) continue;
        const auto* text = std::get_if<cc::core::TextBlock>(&user->content.front());
        if (text && text->text == "second wave 0") {
            kept_second_wave = true;
            break;
        }
    }
    EXPECT_TRUE(kept_second_wave);
}

TEST(QueryEngine, AutoCompactWritesBoundaryMetadataAndKeepsRecentTail) {
    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.context_window.auto_compact = true;
    config.context_window.max_context_tokens = 2000;
    config.context_window.compaction_threshold = 0.05;
    config.cwd = fs::temp_directory_path().string();

    cc::core::QueryEngine engine(std::move(config), registry);

    auto make_user = [](int index) {
        cc::core::UserMessage msg{};
        msg.id.value = "auto-user-" + std::to_string(index);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(cc::core::TextBlock{
            "auto compact payload " + std::to_string(index) + " " + std::string(900, 'x')});
        return cc::core::Message{std::move(msg)};
    };

    for (int i = 0; i < 12; ++i) {
        engine.append_message_for_testing(make_user(i));
    }

    auto conversation = engine.get_conversation();
    auto boundary_count = std::ranges::count_if(conversation, [](const cc::core::Message& message) {
        const auto* system = std::get_if<cc::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    ASSERT_EQ(boundary_count, 1);
    ASSERT_LE(conversation.size(), 9u);

    auto boundary_it = std::ranges::find_if(conversation, [](const cc::core::Message& message) {
        const auto* system = std::get_if<cc::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    ASSERT_NE(boundary_it, conversation.end());
    const auto boundary_index = static_cast<std::size_t>(std::distance(conversation.begin(), boundary_it));

    const auto* boundary = std::get_if<cc::core::SystemMessage>(&*boundary_it);
    ASSERT_NE(boundary, nullptr);
    ASSERT_TRUE(boundary->compact_metadata.has_value());
    EXPECT_EQ(boundary->compact_metadata->trigger, "auto");
    EXPECT_GT(boundary->compact_metadata->pre_tokens, 0u);
    ASSERT_TRUE(boundary->compact_metadata->preserved_segment.has_value());

    ASSERT_LT(boundary_index + 1, conversation.size());
    const auto* marker = std::get_if<cc::core::UserMessage>(&conversation[boundary_index + 1]);
    ASSERT_NE(marker, nullptr);
    EXPECT_EQ(boundary->compact_metadata->preserved_segment->anchor_uuid, marker->id.value);

    const auto last_id = std::visit([](const auto& msg) { return msg.id.value; }, conversation.back());
    EXPECT_EQ(boundary->compact_metadata->preserved_segment->tail_uuid, last_id);

    ASSERT_EQ(marker->content.size(), 1u);
    const auto* summary = std::get_if<cc::core::TextBlock>(&marker->content.front());
    ASSERT_NE(summary, nullptr);
    EXPECT_NE(summary->text.find("Preserve these details"), std::string::npos);
    EXPECT_NE(summary->text.find("auto compact payload"), std::string::npos);
}

TEST(QueryEngine, ReactiveCompactRetriesPromptTooLongAfterWritingBoundary) {
    LocalAnthropicMessagesServer server(
        {
            R"({"error":{"type":"invalid_request_error","message":"prompt_too_long"}})",
            R"({"id":"msg_reactive","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"reactive-ok"}],"stop_reason":"end_turn","usage":{"input_tokens":2,"output_tokens":3}})",
        },
        {413, 200});
    ASSERT_NE(server.port(), 0);

    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Disabled;

    cc::core::QueryEngine engine(std::move(config), registry);

    for (int i = 0; i < 10; ++i) {
        cc::core::UserMessage msg{};
        msg.id.value = "reactive-user-" + std::to_string(i);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(cc::core::TextBlock{
            "reactive legacy " + std::to_string(i) + " " + std::string(600, 'r')});
        engine.append_message_for_testing(cc::core::Message{std::move(msg)});
    }

    auto response = engine.query("trigger reactive compact");
    ASSERT_TRUE(response.has_value()) << response.error().message;
    ASSERT_FALSE(response->message.content.empty());
    const auto* response_text = std::get_if<cc::core::TextBlock>(&response->message.content.front());
    ASSERT_NE(response_text, nullptr);
    EXPECT_EQ(response_text->text, "reactive-ok");

    auto request_bodies = server.wait_for_bodies(2);
    ASSERT_TRUE(request_bodies.has_value());
    ASSERT_EQ(request_bodies->size(), 2u);

    auto count_messages = [](std::string_view body) {
        auto parsed = cc::utils::json::parse(body);
        if (!parsed) return std::size_t{0};
        std::size_t count = 0;
        auto messages = parsed->root().get("messages");
        if (messages.is_arr()) {
            messages.iter([&](cc::utils::json::JsonVal) { ++count; });
        }
        return count;
    };

    const auto first_count = count_messages(request_bodies->front());
    const auto second_count = count_messages(request_bodies->back());
    EXPECT_GT(first_count, second_count);
    EXPECT_NE(request_bodies->back().find("Preserve these details"), std::string::npos);
    EXPECT_EQ(request_bodies->back().find("compact_boundary"), std::string::npos);

    auto conversation = engine.get_conversation();
    auto boundary_it = std::ranges::find_if(conversation, [](const cc::core::Message& message) {
        const auto* system = std::get_if<cc::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    ASSERT_NE(boundary_it, conversation.end());
    const auto* boundary = std::get_if<cc::core::SystemMessage>(&*boundary_it);
    ASSERT_NE(boundary, nullptr);
    ASSERT_TRUE(boundary->compact_metadata.has_value());
    EXPECT_EQ(boundary->compact_metadata->trigger, "reactive");
}

TEST(QueryEngine, AppliesMainThreadToolResultBudgetBeforeModelRequest) {
    LocalAnthropicMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-tool-result-budget-test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());

    cc::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<DefinitionOnlyTool>(cc::core::ToolDefinition{
        .name = "Bash",
        .description = "Execute shell",
        .input_schema = {},
        .permission = cc::core::ToolPermission::Execute,
        .max_result_size_chars = 30'000,
    }));
    registry.register_tool(std::make_unique<DefinitionOnlyTool>(cc::core::ToolDefinition{
        .name = "Read",
        .description = "Read file",
        .input_schema = {},
        .permission = cc::core::ToolPermission::ReadOnly,
        .max_result_size_chars = 0,
        .max_result_size_unbounded = true,
    }));

    cc::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Disabled;

    cc::core::QueryEngine engine(std::move(config), registry);

    cc::core::AssistantMessage assistant{};
    assistant.id.value = "assistant-tool-uses";
    assistant.timestamp = std::chrono::system_clock::now();
    assistant.content.push_back(cc::core::ToolUseBlock{
        .id = cc::core::ToolUseId{"bash-large-1"},
        .name = "Bash",
        .input_json = R"({"command":"one"})",
    });
    assistant.content.push_back(cc::core::ToolUseBlock{
        .id = cc::core::ToolUseId{"bash-large-2"},
        .name = "Bash",
        .input_json = R"({"command":"two"})",
    });
    assistant.content.push_back(cc::core::ToolUseBlock{
        .id = cc::core::ToolUseId{"read-unbounded"},
        .name = "Read",
        .input_json = R"({"file_path":"huge.txt"})",
    });
    engine.append_message_for_testing(cc::core::Message{std::move(assistant)});

    auto append_tool_result = [&](std::string id, std::string text) {
        cc::core::ToolResultMessage result{};
        result.id.value = "result-" + id;
        result.timestamp = std::chrono::system_clock::now();
        result.tool_use_id = cc::core::ToolUseId{std::move(id)};
        result.content.push_back(cc::core::TextBlock{std::move(text)});
        engine.append_message_for_testing(cc::core::Message{std::move(result)});
    };

    append_tool_result("bash-large-1", std::string(170'000, 'b'));
    append_tool_result("bash-large-2", std::string(160'000, 'c'));
    append_tool_result("read-unbounded", std::string(250'000, 'r'));

    auto response = engine.query("continue after tools");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_body = server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    EXPECT_NE(request_body->find("<persisted-output>"), std::string::npos);
    EXPECT_NE(request_body->find("Output too large (170000 bytes)"), std::string::npos);
    EXPECT_EQ(request_body->find(std::string(2'500, 'b')), std::string::npos);
    EXPECT_NE(request_body->find(std::string(2'500, 'c')), std::string::npos);
    EXPECT_NE(request_body->find(std::string(2'500, 'r')), std::string::npos);

    auto persisted_dir = root / "runtime" / "tool-results";
    ASSERT_TRUE(fs::exists(persisted_dir));
    bool saw_persisted_file = false;
    for (const auto& entry : fs::directory_iterator(persisted_dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().filename().string().find("bash-large-1") != std::string::npos) {
            saw_persisted_file = true;
            EXPECT_EQ(fs::file_size(entry.path()), 170'000u);
        }
    }
    EXPECT_TRUE(saw_persisted_file);

    const auto conversation = engine.get_conversation();
    auto replacement_for = [](const std::vector<cc::core::Message>& messages, std::string_view tool_use_id) {
        for (const auto& message : messages) {
            const auto* tool_result = std::get_if<cc::core::ToolResultMessage>(&message);
            if (!tool_result || tool_result->tool_use_id.value != tool_use_id) continue;
            if (tool_result->content.empty()) return std::optional<std::string>{};
            const auto* text = std::get_if<cc::core::TextBlock>(&tool_result->content.front());
            if (!text) return std::optional<std::string>{};
            return std::optional<std::string>{text->text};
        }
        return std::optional<std::string>{};
    };
    auto original_replacement = replacement_for(conversation, "bash-large-1");
    ASSERT_TRUE(original_replacement.has_value());
    ASSERT_NE(original_replacement->find("<persisted-output>"), std::string::npos);

    auto storage_path = root / "history.json";
    {
        cc::core::ConversationStore store(storage_path.string());
        auto* stored = store.create_conversation();
        for (const auto& message : conversation) {
            stored->add_message(message);
        }
        ASSERT_TRUE(store.save_all().has_value());
    }

    cc::core::ConversationStore loaded(storage_path.string());
    ASSERT_TRUE(loaded.load_all().has_value());
    auto restored_messages = loaded.get_active_conversation()->get_messages();
    auto restored_replacement = replacement_for(restored_messages, "bash-large-1");
    ASSERT_TRUE(restored_replacement.has_value());
    EXPECT_EQ(*restored_replacement, *original_replacement);

    LocalAnthropicMessagesServer resumed_server;
    ASSERT_NE(resumed_server.port(), 0);
    cc::core::ToolRegistry restored_registry;
    restored_registry.register_tool(std::make_unique<DefinitionOnlyTool>(cc::core::ToolDefinition{
        .name = "Bash",
        .description = "Execute shell",
        .input_schema = {},
        .permission = cc::core::ToolPermission::Execute,
        .max_result_size_chars = 30'000,
    }));
    restored_registry.register_tool(std::make_unique<DefinitionOnlyTool>(cc::core::ToolDefinition{
        .name = "Read",
        .description = "Read file",
        .input_schema = {},
        .permission = cc::core::ToolPermission::ReadOnly,
        .max_result_size_chars = 0,
        .max_result_size_unbounded = true,
    }));

    cc::core::QueryEngineConfig restored_config;
    restored_config.api_key = "test-key";
    restored_config.base_url = resumed_server.base_url();
    restored_config.context_window.auto_compact = false;
    restored_config.cwd = root.string();
    restored_config.retry_policy.max_retries = 0;
    restored_config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Disabled;

    cc::core::QueryEngine restored_engine(std::move(restored_config), restored_registry);
    restored_engine.restore_conversation(std::move(restored_messages));
    auto resumed_response = restored_engine.query("after resume");
    ASSERT_TRUE(resumed_response.has_value()) << resumed_response.error().message;

    auto resumed_body = resumed_server.wait_for_body();
    ASSERT_TRUE(resumed_body.has_value());
    EXPECT_NE(resumed_body->find("<persisted-output>"), std::string::npos);
    EXPECT_NE(resumed_body->find("Output too large (170000 bytes)"), std::string::npos);
    EXPECT_EQ(resumed_body->find(std::string(2'500, 'b')), std::string::npos);
    EXPECT_NE(resumed_body->find(std::string(2'500, 'c')), std::string::npos);
    EXPECT_NE(resumed_body->find(std::string(2'500, 'r')), std::string::npos);

    fs::remove_all(root);
}

TEST(QueryEngine, TimeBasedMicrocompactClearsOldCompactableToolResultsBeforeRequest) {
    EnvironmentGuard enable_guard("LOOM_TIME_BASED_MICROCOMPACT", "1");
    EnvironmentGuard gap_guard("LOOM_TIME_BASED_MICROCOMPACT_GAP_MINUTES", "30");
    EnvironmentGuard keep_guard("LOOM_TIME_BASED_MICROCOMPACT_KEEP_RECENT", "1");

    LocalAnthropicMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-time-based-microcompact-test";
    fs::remove_all(root);
    fs::create_directories(root);

    cc::core::ToolRegistry registry;
    cc::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = cc::core::ThinkingConfig::Mode::Disabled;

    cc::core::QueryEngine engine(std::move(config), registry);

    const auto old_time = std::chrono::system_clock::now() - std::chrono::hours(2);
    cc::core::AssistantMessage assistant{};
    assistant.id.value = "assistant-old-tool-uses";
    assistant.timestamp = old_time;
    assistant.content.push_back(cc::core::ToolUseBlock{
        .id = cc::core::ToolUseId{"read-old"},
        .name = "Read",
        .input_json = R"({"file_path":"old.txt"})",
    });
    assistant.content.push_back(cc::core::ToolUseBlock{
        .id = cc::core::ToolUseId{"bash-old"},
        .name = "Bash",
        .input_json = R"({"command":"old"})",
    });
    assistant.content.push_back(cc::core::ToolUseBlock{
        .id = cc::core::ToolUseId{"task-noncompact"},
        .name = "Task",
        .input_json = R"({"description":"noncompact"})",
    });
    assistant.content.push_back(cc::core::ToolUseBlock{
        .id = cc::core::ToolUseId{"edit-recent"},
        .name = "Edit",
        .input_json = R"({"file_path":"recent.txt"})",
    });
    engine.append_message_for_testing(cc::core::Message{std::move(assistant)});

    auto append_tool_result = [&](std::string id, std::string text) {
        cc::core::ToolResultMessage result{};
        result.id.value = "result-" + id;
        result.timestamp = old_time;
        result.tool_use_id = cc::core::ToolUseId{std::move(id)};
        result.content.push_back(cc::core::TextBlock{std::move(text)});
        engine.append_message_for_testing(cc::core::Message{std::move(result)});
    };

    append_tool_result("read-old", "OLD_READ_RESULT_SHOULD_CLEAR");
    append_tool_result("bash-old", "OLD_BASH_RESULT_SHOULD_CLEAR");
    append_tool_result("task-noncompact", "NONCOMPACT_TASK_RESULT_SHOULD_STAY");
    append_tool_result("edit-recent", "RECENT_EDIT_RESULT_SHOULD_STAY");

    auto response = engine.query("continue after cold cache");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_body = server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    EXPECT_EQ(request_body->find("OLD_READ_RESULT_SHOULD_CLEAR"), std::string::npos)
        << *request_body;
    EXPECT_EQ(request_body->find("OLD_BASH_RESULT_SHOULD_CLEAR"), std::string::npos)
        << *request_body;
    EXPECT_NE(request_body->find("[Old tool result content cleared]"), std::string::npos)
        << *request_body;
    EXPECT_NE(request_body->find("NONCOMPACT_TASK_RESULT_SHOULD_STAY"), std::string::npos)
        << *request_body;
    EXPECT_NE(request_body->find("RECENT_EDIT_RESULT_SHOULD_STAY"), std::string::npos)
        << *request_body;

    auto conversation = engine.get_conversation();
    auto tool_result_text = [&](std::string_view tool_use_id) -> std::optional<std::string> {
        for (const auto& message : conversation) {
            const auto* result = std::get_if<cc::core::ToolResultMessage>(&message);
            if (!result || result->tool_use_id.value != tool_use_id) continue;
            if (result->content.empty()) return std::nullopt;
            const auto* text = std::get_if<cc::core::TextBlock>(&result->content.front());
            if (!text) return std::nullopt;
            return text->text;
        }
        return std::nullopt;
    };
    EXPECT_EQ(tool_result_text("read-old"), std::optional<std::string>{"[Old tool result content cleared]"});
    EXPECT_EQ(tool_result_text("bash-old"), std::optional<std::string>{"[Old tool result content cleared]"});
    EXPECT_EQ(tool_result_text("task-noncompact"), std::optional<std::string>{"NONCOMPACT_TASK_RESULT_SHOULD_STAY"});
    EXPECT_EQ(tool_result_text("edit-recent"), std::optional<std::string>{"RECENT_EDIT_RESULT_SHOULD_STAY"});

    fs::remove_all(root);
}

TEST(ApiClient, MessageFromTextCreatesSingleTextBlock) {
    auto message = cc::services::api::Message::from_text("user", "hello");

    ASSERT_EQ(message.role, "user");
    ASSERT_EQ(message.content.size(), 1u);
    EXPECT_EQ(message.content.front().type, cc::services::api::ContentBlockType::Text);
    EXPECT_EQ(message.content.front().text, "hello");
}

TEST(ApiClient, ResponseCombinesTextContentAndTokenUsage) {
    cc::services::api::CreateMessageResponse response;
    cc::services::api::ContentBlock first;
    first.type = cc::services::api::ContentBlockType::Text;
    first.text = "hello ";
    response.content.push_back(first);
    cc::services::api::ContentBlock second;
    second.type = cc::services::api::ContentBlockType::Text;
    second.text = "world";
    response.content.push_back(second);
    response.usage.input_tokens = 3;
    response.usage.output_tokens = 5;
    response.usage.cache_creation_tokens = 7;
    response.usage.cache_read_tokens = 11;

    EXPECT_EQ(response.get_text_content(), "hello world");
    EXPECT_EQ(response.usage.total(), 8);
    EXPECT_EQ(response.usage.total_with_cache(), 26);
}

TEST(ApiClient, RequestSerializerPreservesToolUseInputJson) {
    cc::services::api::CreateMessageRequest request;
    request.model = "loom-test";
    request.messages.push_back(cc::services::api::Message{
        .role = "assistant",
        .content = {
            cc::services::api::ContentBlock{
                .type = cc::services::api::ContentBlockType::Text,
                .text = "I will read a file."
            },
            cc::services::api::ContentBlock{
                .type = cc::services::api::ContentBlockType::ToolUse,
                .tool_use_id = "toolu_1",
                .tool_name = "Read",
                .tool_input_json = R"({"file_path":"README.md","limit":20})"
            }
        }
    });

    auto serialized = cc::services::api::RequestSerializer::serialize(request);
    auto parsed = cc::utils::json::parse(serialized);

    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    auto content = parsed->root().get("messages").at(0).get("content");
    ASSERT_TRUE(content.is_arr());
    auto tool_use = content.at(1);
    EXPECT_EQ(tool_use.get("type").as_str(), "tool_use");
    EXPECT_EQ(tool_use.get("id").as_str(), "toolu_1");
    EXPECT_EQ(tool_use.get("name").as_str(), "Read");
    auto input = tool_use.get("input");
    ASSERT_TRUE(input.is_obj());
    EXPECT_EQ(input.get("file_path").as_str(), "README.md");
    EXPECT_EQ(input.get("limit").as_int(), 20);
}

TEST(ApiClient, RequestSerializerPreservesImageAndDocumentBlocks) {
    cc::services::api::CreateMessageRequest request;
    request.model = "loom-test";
    request.messages.push_back(cc::services::api::Message{
        .role = "user",
        .content = {
            cc::services::api::ContentBlock{
                .type = cc::services::api::ContentBlockType::Image,
                .media_type = "image/png",
                .image_data = "iVBORw0KGgo="
            },
            cc::services::api::ContentBlock{
                .type = cc::services::api::ContentBlockType::Document,
                .media_type = "application/pdf",
                .image_data = "JVBERi0xLjQ="
            }
        }
    });

    auto serialized = cc::services::api::RequestSerializer::serialize(request);
    auto parsed = cc::utils::json::parse(serialized);

    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    auto content = parsed->root().get("messages").at(0).get("content");
    ASSERT_TRUE(content.is_arr());

    auto image = content.at(0);
    EXPECT_EQ(image.get("type").as_str(), "image");
    EXPECT_EQ(image.get("source").get("type").as_str(), "base64");
    EXPECT_EQ(image.get("source").get("media_type").as_str(), "image/png");
    EXPECT_EQ(image.get("source").get("data").as_str(), "iVBORw0KGgo=");

    auto document = content.at(1);
    EXPECT_EQ(document.get("type").as_str(), "document");
    EXPECT_EQ(document.get("source").get("type").as_str(), "base64");
    EXPECT_EQ(document.get("source").get("media_type").as_str(), "application/pdf");
    EXPECT_EQ(document.get("source").get("data").as_str(), "JVBERi0xLjQ=");
}

TEST(ApiClient, RequestSerializerSerializesEffortConfig) {
    cc::services::api::CreateMessageRequest request;
    request.model = "loom-test";
    request.messages.push_back(cc::services::api::Message::from_text("user", "hello"));
    request.output_effort = "high";
    request.task_budget = cc::services::api::TaskBudget{
        .total = 12000,
        .remaining = 3456,
    };
    request.internal_effort_override = 77;

    auto serialized = cc::services::api::RequestSerializer::serialize(request);
    auto parsed = cc::utils::json::parse(serialized);

    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    auto output_config = parsed->root().get("output_config");
    ASSERT_TRUE(output_config.is_obj());
    EXPECT_EQ(output_config.get("effort").as_str(), "high");
    auto task_budget = output_config.get("task_budget");
    ASSERT_TRUE(task_budget.is_obj());
    EXPECT_EQ(task_budget.get("type").as_str(), "tokens");
    EXPECT_EQ(task_budget.get("total").as_int(), 12000);
    EXPECT_EQ(task_budget.get("remaining").as_int(), 3456);
    auto anthropic_internal = parsed->root().get("anthropic_internal");
    ASSERT_TRUE(anthropic_internal.is_obj());
    EXPECT_EQ(anthropic_internal.get("effort_override").as_int(), 77);
}

TEST(ApiClient, ResponseParserPreservesToolUseInputJson) {
    const auto response = cc::services::api::ResponseParser::parse(R"({
      "id": "msg_1",
      "model": "loom-test",
      "role": "assistant",
      "content": [
        {
          "type": "tool_use",
          "id": "toolu_1",
          "name": "Bash",
          "input": {"command": "pwd", "timeout": 1000}
        }
      ],
      "stop_reason": "tool_use",
      "usage": {"input_tokens": 1, "output_tokens": 2}
    })");

    ASSERT_TRUE(response.has_value()) << response.error().message();
    ASSERT_EQ(response->content.size(), 1u);
    const auto& block = response->content.front();
    EXPECT_EQ(block.type, cc::services::api::ContentBlockType::ToolUse);
    EXPECT_EQ(block.tool_use_id, "toolu_1");
    EXPECT_EQ(block.tool_name, "Bash");

    auto input = cc::utils::json::parse(block.tool_input_json);
    ASSERT_TRUE(input.has_value()) << input.error().message();
    EXPECT_EQ(input->root().get("command").as_str(), "pwd");
    EXPECT_EQ(input->root().get("timeout").as_int(), 1000);
}

TEST(ApiStreaming, SseBufferExtractsCompleteEvents) {
    cc::services::api::SseBuffer buffer;
    buffer.append("event: ping\ndata: {}\n\n");
    auto event = buffer.next_event();
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->first, "ping");
    EXPECT_EQ(event->second, "{}");
}

TEST(ApiStreaming, StreamParserAccumulatesTextDeltas) {
    cc::services::api::StreamParser parser;
    parser.start();
    parser.feed("event: content_block_delta\n");
    parser.feed("data: {\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"hi\"}}\n\n");

    auto event = parser.next_event();
    ASSERT_TRUE(event.has_value());
    ASSERT_TRUE(event->has_value());
    EXPECT_EQ((*event)->type, cc::services::api::StreamEventType::ContentBlockDelta);
    EXPECT_EQ(parser.full_text(), "hi");
    EXPECT_EQ(parser.statistics().total_events, 1);
}

TEST(McpElicitationHandler, UsesRegisteredResponderAndPolicy) {
    cc::services::mcp::clear_elicitation_policy();
    cc::services::mcp::clear_elicitation_responder();

    auto missing = cc::services::mcp::handle_elicitation(cc::services::mcp::ElicitationRequest{
        .server_name = "linear",
        .message = "Pick a workspace",
        .schema = {{"workspace", "string"}},
    });
    ASSERT_FALSE(missing.has_value());
    EXPECT_NE(missing.error().find("No MCP elicitation responder"), std::string::npos);

    std::optional<cc::services::mcp::ElicitationRequest> captured;
    cc::services::mcp::set_elicitation_responder([&](const cc::services::mcp::ElicitationRequest& request)
        -> std::expected<std::map<std::string, std::string>, std::string> {
        captured = request;
        return std::map<std::string, std::string>{{"workspace", "eng"}};
    });

    auto response = cc::services::mcp::handle_elicitation(cc::services::mcp::ElicitationRequest{
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

    cc::services::mcp::set_elicitation_allowed("linear", false);
    auto denied = cc::services::mcp::handle_elicitation(cc::services::mcp::ElicitationRequest{
        .server_name = "linear",
        .message = "Pick a workspace",
        .schema = {},
    });
    ASSERT_FALSE(denied.has_value());
    EXPECT_NE(denied.error().find("not allowed"), std::string::npos);

    cc::services::mcp::clear_elicitation_policy();
    cc::services::mcp::clear_elicitation_responder();
}

TEST(McpConfigParser, ParsesJsonFieldsAndExplicitTransports) {
    const auto parsed = cc::services::mcp::ConfigParser::parse_json(R"JSON({
      "mcpServers": {
        "stdio_fixture": {
          "type": "stdio",
          "command": "node",
          "args": ["server.js", "--flag"],
          "env": {"FOO": "bar"},
          "timeout": 1234,
          "autoStart": false,
          "enabled": false
        },
	"sse_fixture": {
	  "type": "sse",
	  "url": "http://127.0.0.1:8123/events",
	  "headers": {"Authorization": "Bearer token"},
	  "headersHelper": "node helper.js",
	  "oauth": {
	    "authServerMetadataUrl": "https://auth.example.com/.well-known/oauth-authorization-server",
	    "callbackPort": 19485,
	    "clientId": "client-1",
	    "xaa": true
	  }
	},
        "http_fixture": {
          "type": "http",
          "url": "http://127.0.0.1:8124/mcp",
          "headers": {"X-Test": "present"},
          "disabled": true
        },
        "inferred_http": {
          "url": "http://127.0.0.1:8125/mcp"
        },
        "unsupported_ws": {
          "type": "ws",
          "url": "ws://127.0.0.1:8126/mcp"
        },
        "invalid_stdio": {
          "type": "stdio",
          "args": ["missing-command"]
        }
      }
    })JSON", cc::services::mcp::ConfigScope::Project);

    ASSERT_TRUE(parsed.has_value()) << static_cast<int>(parsed.error());
    ASSERT_EQ(parsed->size(), 4u);

    const auto& stdio = parsed->at("stdio_fixture");
    EXPECT_EQ(stdio.transport, cc::services::mcp::TransportType::Stdio);
    EXPECT_EQ(stdio.command, "node");
    ASSERT_EQ(stdio.args.size(), 2u);
    EXPECT_EQ(stdio.args[0], "server.js");
    EXPECT_EQ(stdio.args[1], "--flag");
    EXPECT_EQ(stdio.env.at("FOO"), "bar");
    EXPECT_EQ(stdio.timeout, std::chrono::milliseconds{1234});
    EXPECT_FALSE(stdio.auto_start);
    EXPECT_FALSE(stdio.enabled);
    EXPECT_EQ(stdio.scope, cc::services::mcp::ConfigScope::Project);

	const auto& sse = parsed->at("sse_fixture");
	EXPECT_EQ(sse.transport, cc::services::mcp::TransportType::Sse);
	EXPECT_EQ(sse.url, "http://127.0.0.1:8123/events");
	EXPECT_EQ(sse.headers.at("Authorization"), "Bearer token");
	EXPECT_EQ(sse.headers_helper, "node helper.js");
	ASSERT_TRUE(sse.oauth.has_value());
	ASSERT_TRUE(sse.oauth->auth_server_metadata_url.has_value());
	EXPECT_EQ(*sse.oauth->auth_server_metadata_url, "https://auth.example.com/.well-known/oauth-authorization-server");
	ASSERT_TRUE(sse.oauth->callback_port.has_value());
	EXPECT_EQ(*sse.oauth->callback_port, 19485);
	ASSERT_TRUE(sse.oauth->client_id.has_value());
	EXPECT_EQ(*sse.oauth->client_id, "client-1");
	EXPECT_TRUE(sse.oauth->xaa);

    const auto& http = parsed->at("http_fixture");
    EXPECT_EQ(http.transport, cc::services::mcp::TransportType::StreamableHttp);
    EXPECT_EQ(http.url, "http://127.0.0.1:8124/mcp");
    EXPECT_EQ(http.headers.at("X-Test"), "present");
    EXPECT_FALSE(http.enabled);

    const auto& inferred = parsed->at("inferred_http");
    EXPECT_EQ(inferred.transport, cc::services::mcp::TransportType::StreamableHttp);
    EXPECT_EQ(inferred.url, "http://127.0.0.1:8125/mcp");
    EXPECT_FALSE(parsed->contains("unsupported_ws"));
    EXPECT_FALSE(parsed->contains("invalid_stdio"));
}

TEST(McpConfigParser, SupportsServersAliasAndRejectsInvalidJson) {
    const auto parsed = cc::services::mcp::ConfigParser::parse_json(R"JSON({
      "servers": {
        "alias_fixture": {
          "transport": "streamable-http",
          "url": "http://127.0.0.1:8127/mcp"
        }
      }
    })JSON", cc::services::mcp::ConfigScope::User);

    ASSERT_TRUE(parsed.has_value()) << static_cast<int>(parsed.error());
    ASSERT_EQ(parsed->size(), 1u);
    const auto& alias = parsed->at("alias_fixture");
    EXPECT_EQ(alias.transport, cc::services::mcp::TransportType::StreamableHttp);
    EXPECT_EQ(alias.scope, cc::services::mcp::ConfigScope::User);

    const auto invalid = cc::services::mcp::ConfigParser::parse_json(
        "{",
        cc::services::mcp::ConfigScope::User
    );
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error(), cc::services::mcp::ConfigError::ParseError);
}

TEST(McpHeadersHelper, ParsesAndMergesDynamicHeaders) {
    const auto parsed = cc::services::mcp::parse_header_helper_json(R"JSON({
      "Authorization": "Bearer dynamic",
      "X-Helper": "present"
    })JSON");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->at("Authorization"), "Bearer dynamic");
    EXPECT_EQ(parsed->at("X-Helper"), "present");

    const auto invalid = cc::services::mcp::parse_header_helper_json(R"JSON({
      "X-Bad": 7
    })JSON");
    EXPECT_FALSE(invalid.has_value());

    const auto merged = cc::services::mcp::get_mcp_server_headers(
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

TEST(McpTypes, JsonRpcSerializationIncludesParams) {
    auto request = cc::services::mcp::make_request(
        int64_t{7},
        "tools/call",
        std::optional<std::string>{R"({"name":"echo","arguments":{"value":"hello"}})"});

    const auto serialized = cc::services::mcp::serialize_request(request);

    EXPECT_NE(serialized.find(R"("method":"tools/call")"), std::string::npos);
    EXPECT_NE(serialized.find(R"("params":{"name":"echo","arguments":{"value":"hello"}})"), std::string::npos);

    auto notification = cc::services::mcp::make_notification(
        "notifications/initialized",
        std::optional<std::string>{R"({"ready":true})"});

    const auto serialized_notification = cc::services::mcp::serialize_notification(notification);
    EXPECT_NE(serialized_notification.find(R"("params":{"ready":true})"), std::string::npos);
}

TEST(McpClient, SendsSseRequestsViaDiscoveredPostEndpoint) {
    LocalSseMcpServer server;
    ASSERT_TRUE(server.ready());

    cc::services::mcp::McpClient::Config config;
    config.name = "sse-fixture";
    config.request_timeout = std::chrono::milliseconds{2000};
    config.init_timeout = std::chrono::milliseconds{2000};

    cc::services::mcp::McpClient client(std::move(config));
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

    cc::utils::ide::IdeLockfileScanner scanner;
    auto lockfiles = scanner.scan();
    ASSERT_EQ(lockfiles.size(), 1u);
    EXPECT_EQ(lockfiles.front().port, server.port());
    EXPECT_EQ(lockfiles.front().name, "VS Code");
    ASSERT_EQ(lockfiles.front().workspace_folders.size(), 1u);
    EXPECT_EQ(lockfiles.front().workspace_folders.front(), fs::current_path());

    auto response = cc::utils::ide::callIdeRpc(
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

    auto response = cc::utils::ide::callIdeRpc(
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

    auto servers = cc::services::mcp::discover_vscode_mcp_servers(workspace.string());
    auto find_server = [&servers](std::string_view name) {
        return std::find_if(servers.begin(), servers.end(), [name](const auto& server) {
            return server.name == name;
        });
    };

    auto stdio = find_server("workspace-stdio");
    ASSERT_NE(stdio, servers.end());
    EXPECT_EQ(stdio->transport_type, "stdio");
    EXPECT_EQ(stdio->connection_string, "node");
    EXPECT_TRUE(cc::services::mcp::connect_vscode_mcp(*stdio));

    auto sse = find_server("workspace-sse");
    ASSERT_NE(sse, servers.end());
    EXPECT_EQ(sse->transport_type, "sse");
    EXPECT_EQ(sse->connection_string, "http://127.0.0.1:9012/sse");
    EXPECT_TRUE(cc::services::mcp::connect_vscode_mcp(*sse));

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

    auto servers = cc::services::mcp::discover_vscode_mcp_servers(workspace.string());
    auto find_server = [&servers](std::string_view name) {
        return std::find_if(servers.begin(), servers.end(), [name](const auto& server) {
            return server.name == name;
        });
    };

    auto stdio = find_server("extension-stdio");
    ASSERT_NE(stdio, servers.end());
    EXPECT_EQ(stdio->transport_type, "stdio");
    EXPECT_EQ(stdio->connection_string, "node");
    EXPECT_TRUE(cc::services::mcp::connect_vscode_mcp(*stdio));

    auto ws = find_server("extension-ws");
    ASSERT_NE(ws, servers.end());
    EXPECT_EQ(ws->transport_type, "ws");
    EXPECT_EQ(ws->connection_string, "ws://127.0.0.1:8020/mcp");
    EXPECT_TRUE(cc::services::mcp::connect_vscode_mcp(*ws));

    fs::remove_all(root);
}

TEST(McpClient, MapsSseUnauthorizedToUnauthorizedError) {
    LocalUnauthorizedStreamableHttpMcpServer server;
    ASSERT_TRUE(server.ready());

    cc::services::mcp::McpClient::Config config;
    config.name = "sse-auth-fixture";
    config.request_timeout = std::chrono::milliseconds{500};
    config.init_timeout = std::chrono::milliseconds{500};

    cc::services::mcp::McpClient client(std::move(config));
    auto connected = client.connect_sse(server.url());
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error(), cc::services::mcp::McpClientError::Unauthorized);

    const auto requests = server.requests();
    ASSERT_FALSE(requests.empty());
    EXPECT_NE(requests.front().find("GET /mcp HTTP/1.1"), std::string::npos) << requests.front();

    client.shutdown();
}

TEST(McpClient, SseReconnectResumesWithLastEventId) {
    LocalReconnectSseStreamServer server;
    ASSERT_TRUE(server.ready());

    cc::services::mcp::SseTransport::ReconnectPolicy policy{
        .initial_delay = std::chrono::milliseconds{10},
        .max_delay = std::chrono::milliseconds{20},
        .backoff_multiplier = 2.0,
        .jitter_factor = 0.0,
        .max_retries = 5,
        .liveness_timeout = std::chrono::seconds{1},
    };
    cc::services::mcp::SseTransport transport(server.url(), {}, policy);

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

    cc::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = fs::temp_directory_path() / "loom_streamable_http_mcp_config";
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    cc::services::mcp::McpConnectionManager manager(std::move(manager_config));

    cc::services::mcp::ServerConfig server_config;
    server_config.name = "http-fixture";
    server_config.transport = cc::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.url();
    server_config.headers = {{"X-Test-Header", "present"}};
    server_config.enabled = true;
    server_config.auto_start = true;

    cc::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto connected = manager.connect_server("http-fixture");
    ASSERT_TRUE(connected.has_value()) << static_cast<int>(connected.error());

    auto snapshot = manager.snapshot_server("http-fixture");
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->status, cc::services::mcp::ConnectionStatus::Connected);
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
    cc::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = fs::temp_directory_path() / ("loom_mcp_unauthorized_" + std::to_string(suffix));
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    cc::services::mcp::McpConnectionManager manager(std::move(manager_config));

    cc::services::mcp::ServerConfig server_config;
    server_config.name = "auth-fixture";
    server_config.transport = cc::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.url();
    server_config.enabled = true;
    server_config.auto_start = true;

    cc::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto connected = manager.connect_server("auth-fixture");
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error(), cc::services::mcp::McpClientError::Unauthorized);

    auto snapshot = manager.snapshot_server("auth-fixture");
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->status, cc::services::mcp::ConnectionStatus::NeedsAuth);
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

    cc::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = root;
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    cc::services::mcp::McpConnectionManager manager(std::move(manager_config));

    cc::services::mcp::ServerConfig server_config;
    server_config.name = "helper-fixture";
    server_config.transport = cc::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.url();
    server_config.headers = {
        {"X-Test-Header", "static"},
        {"X-Static", "present"},
    };
    server_config.headers_helper = "sh '" + helper_path.string() + "'";

    cc::services::mcp::McpConfig mcp_config;
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

    cc::services::mcp::ServerConfig server_config;
    server_config.name = "refresh-fixture";
    server_config.transport = cc::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.mcp_url();
    server_config.enabled = true;
    server_config.auto_start = true;
    server_config.oauth = cc::services::mcp::McpOAuthConfig{
        .auth_server_metadata_url = server.metadata_url(),
        .client_id = "client-1",
    };

    cc::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = server_config.url;
    auth_config.oauth = server_config.oauth;
    const auto server_key = cc::services::mcp::get_server_key(server_config.name, auth_config);
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

    cc::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = root;
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    cc::services::mcp::McpConnectionManager manager(std::move(manager_config));

    cc::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto connected = manager.connect_server("refresh-fixture");
    ASSERT_TRUE(connected.has_value()) << static_cast<int>(connected.error());
    ASSERT_TRUE(server.wait_for_token_request());
    EXPECT_NE(server.token_request_body().find("grant_type=refresh_token"), std::string::npos);
    EXPECT_NE(server.token_request_body().find("refresh_token=old-refresh"), std::string::npos);

    auto snapshot = manager.snapshot_server("refresh-fixture");
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->status, cc::services::mcp::ConnectionStatus::Connected);
    ASSERT_EQ(snapshot->tools.size(), 1u);
    EXPECT_EQ(snapshot->tools.front().name, "refresh_lookup");

    const auto auth_headers = server.mcp_authorization_headers();
    ASSERT_FALSE(auth_headers.empty());
    EXPECT_TRUE(std::ranges::all_of(auth_headers, [](const auto& header) {
        return header == "Bearer fresh-access";
    }));

    auto persisted = cc::utils::json::parse_file(token_path);
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

    cc::services::mcp::ServerConfig server_config;
    server_config.name = "refresh-failure-fixture";
    server_config.transport = cc::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.mcp_url();
    server_config.enabled = true;
    server_config.auto_start = true;
    server_config.oauth = cc::services::mcp::McpOAuthConfig{
        .auth_server_metadata_url = server.metadata_url(),
        .client_id = "client-1",
    };

    cc::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = server_config.url;
    auth_config.oauth = server_config.oauth;
    const auto server_key = cc::services::mcp::get_server_key(server_config.name, auth_config);
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

    cc::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = root;
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    cc::services::mcp::McpConnectionManager manager(std::move(manager_config));

    cc::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto connected = manager.connect_server("refresh-failure-fixture");
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error(), cc::services::mcp::McpClientError::Unauthorized);
    ASSERT_TRUE(server.wait_for_token_request());
    EXPECT_NE(server.token_request_body().find("grant_type=refresh_token"), std::string::npos);
    EXPECT_NE(server.token_request_body().find("refresh_token=old-refresh"), std::string::npos);

    auto snapshot = manager.snapshot_server("refresh-failure-fixture");
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->status, cc::services::mcp::ConnectionStatus::NeedsAuth);
    ASSERT_TRUE(snapshot->last_error.has_value());
    EXPECT_NE(snapshot->last_error->find("OAuth token refresh failed"), std::string::npos);
    EXPECT_TRUE(server.mcp_authorization_headers().empty());

    auto persisted = cc::utils::json::parse_file(token_path);
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

    cc::services::mcp::ServerConfig server_config;
    server_config.name = "auth-needed-fixture";
    server_config.transport = cc::services::mcp::TransportType::StreamableHttp;
    server_config.url = server.mcp_url();
    server_config.enabled = true;
    server_config.auto_start = true;
    server_config.oauth = cc::services::mcp::McpOAuthConfig{
        .auth_server_metadata_url = server.metadata_url(),
        .client_id = "client-1",
    };

    cc::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = server_config.url;
    auth_config.oauth = server_config.oauth;
    const auto server_key = cc::services::mcp::get_server_key(server_config.name, auth_config);
    auto sanitize_key = [](std::string_view key) {
        std::string sanitized;
        sanitized.reserve(key.size());
        for (unsigned char ch : key) {
            sanitized.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
        }
        return sanitized;
    };
    const auto token_path = root / "loom" / "mcp" / (sanitize_key(server_key) + ".json");

    cc::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = root;
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    cc::services::mcp::McpConnectionManager manager(std::move(manager_config));

    cc::services::mcp::McpConfig mcp_config;
    mcp_config.servers.emplace(server_config.name, std::move(server_config));
    manager.set_configuration(std::move(mcp_config));

    auto auth_needed = manager.connect_server("auth-needed-fixture");
    ASSERT_FALSE(auth_needed.has_value());
    EXPECT_EQ(auth_needed.error(), cc::services::mcp::McpClientError::Unauthorized);
    EXPECT_TRUE(server.mcp_authorization_headers().empty());
    EXPECT_TRUE(server.token_request_body().empty());

    auto snapshot = manager.snapshot_server("auth-needed-fixture");
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->status, cc::services::mcp::ConnectionStatus::NeedsAuth);
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
    EXPECT_EQ(snapshot->status, cc::services::mcp::ConnectionStatus::Connected);
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

TEST(McpAuth, RevokesOAuthTokensViaMetadataEndpointAndClearsLocalStorage) {
    LocalOAuthRevocationServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_revoke_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());

    cc::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = "https://mcp.example.test/mcp";
    auth_config.oauth = cc::services::mcp::McpOAuthConfig{
        .auth_server_metadata_url = server.metadata_url(),
        .client_id = "client-1",
    };
    const auto server_key = cc::services::mcp::get_server_key("revoke-fixture", auth_config);
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
    const auto expires_at = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 3600;
    {
        std::ofstream token_file(token_path);
        token_file << std::format(R"({{
          "server_name": "revoke-fixture",
          "server_url": "{}",
          "access_token": "old-access",
          "refresh_token": "old-refresh",
          "expires_at": {},
          "scope": "tools",
          "client_id": "client-1",
          "client_secret": "secret-1",
          "discovery_state": {{}}
        }})", *auth_config.url, expires_at);
    }

    auto revoked = cc::services::mcp::revoke_server_tokens("revoke-fixture", auth_config);
    ASSERT_TRUE(revoked.has_value()) << revoked.error().message();
    ASSERT_TRUE(server.wait_for_revoke_requests(2));

    const auto bodies = server.revoke_request_bodies();
    ASSERT_EQ(bodies.size(), 2u);
    EXPECT_NE(bodies[0].find("token=old-refresh"), std::string::npos) << bodies[0];
    EXPECT_NE(bodies[0].find("token_type_hint=refresh_token"), std::string::npos) << bodies[0];
    EXPECT_NE(bodies[0].find("client_id=client-1"), std::string::npos) << bodies[0];
    EXPECT_NE(bodies[0].find("client_secret=secret-1"), std::string::npos) << bodies[0];
    EXPECT_NE(bodies[1].find("token=old-access"), std::string::npos) << bodies[1];
    EXPECT_NE(bodies[1].find("token_type_hint=access_token"), std::string::npos) << bodies[1];

    const auto auth_headers = server.revoke_authorization_headers();
    ASSERT_EQ(auth_headers.size(), 2u);
    EXPECT_TRUE(auth_headers[0].empty());
    EXPECT_TRUE(auth_headers[1].empty());
    EXPECT_FALSE(fs::exists(token_path));

    fs::remove_all(root);
}

TEST(McpAuth, CompletesOAuthBrowserCallbackFlowAndStoresTokens) {
    LocalOAuthRevocationServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_oauth_callback_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());

    cc::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = "https://mcp.example.test/mcp";
    auth_config.oauth = cc::services::mcp::McpOAuthConfig{
        .auth_server_metadata_url = server.metadata_url(),
        .client_id = "client-1",
    };

    std::mutex mutex;
    std::condition_variable cv;
    std::optional<std::string> auth_url;
    std::optional<std::string> flow_error;
    bool flow_done = false;
    std::jthread flow_thread([&](std::stop_token) {
        auto result = cc::services::mcp::perform_mcp_oauth_flow(
            "callback-fixture",
            auth_config,
            [&](const std::string& url) {
                {
                    std::lock_guard lock(mutex);
                    auth_url = url;
                }
                cv.notify_all();
            },
            std::nullopt,
            true);
        {
            std::lock_guard lock(mutex);
            if (!result) flow_error = result.error().message();
            flow_done = true;
        }
        cv.notify_all();
    });

    std::string captured_url;
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return auth_url.has_value() || flow_done;
        }));
        ASSERT_TRUE(auth_url.has_value()) << flow_error.value_or("OAuth flow ended before authorization URL");
        captured_url = *auth_url;
    }

    auto redirect_uri = test_query_param(captured_url, "redirect_uri");
    auto state = test_query_param(captured_url, "state");
    ASSERT_TRUE(redirect_uri.has_value()) << captured_url;
    ASSERT_TRUE(state.has_value()) << captured_url;
    EXPECT_NE(captured_url.find("response_type=code"), std::string::npos) << captured_url;
    EXPECT_NE(captured_url.find("client_id=client-1"), std::string::npos) << captured_url;
    EXPECT_NE(captured_url.find("code_challenge_method=S256"), std::string::npos) << captured_url;
    EXPECT_NE(captured_url.find("scope=tools"), std::string::npos) << captured_url;

    auto callback_port = localhost_url_port(*redirect_uri);
    ASSERT_TRUE(callback_port.has_value()) << *redirect_uri;
    httplib::Client callback_client(std::format("http://localhost:{}", *callback_port));
    auto callback_response = callback_client.Get(
        std::format("/oauth/callback?code=callback-code&state={}", *state));
    ASSERT_TRUE(callback_response);
    EXPECT_EQ(callback_response->status, 200);

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return flow_done; }));
        ASSERT_FALSE(flow_error.has_value()) << *flow_error;
    }
    ASSERT_TRUE(server.wait_for_token_request());
    const auto token_body = server.token_request_body();
    EXPECT_NE(token_body.find("grant_type=authorization_code"), std::string::npos) << token_body;
    EXPECT_NE(token_body.find("code=callback-code"), std::string::npos) << token_body;
    EXPECT_NE(token_body.find("client_id=client-1"), std::string::npos) << token_body;
    EXPECT_NE(token_body.find("code_verifier="), std::string::npos) << token_body;

    const auto server_key = cc::services::mcp::get_server_key("callback-fixture", auth_config);
    auto sanitize_key = [](std::string_view key) {
        std::string sanitized;
        sanitized.reserve(key.size());
        for (unsigned char ch : key) {
            sanitized.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
        }
        return sanitized;
    };
    const auto token_path = root / "loom" / "mcp" / (sanitize_key(server_key) + ".json");
    auto persisted = cc::utils::json::parse_file(token_path);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_EQ(persisted->root().get_string("server_name"), "callback-fixture");
    EXPECT_EQ(persisted->root().get_string("server_url"), *auth_config.url);
    EXPECT_EQ(persisted->root().get_string("access_token"), "callback-access");
    EXPECT_EQ(persisted->root().get_string("refresh_token"), "callback-refresh");
    EXPECT_EQ(persisted->root().get_string("scope"), "tools");
    EXPECT_EQ(persisted->root().get_string("client_id"), "client-1");

    fs::remove_all(root);
}

TEST(McpAuth, PerformsXaaIdpLoginAndStoresTokens) {
    LocalXaaIdpServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_xaa_idp_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root / ".loom");
    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());
    EnvironmentGuard xaa_enabled_guard("LOOM_ENABLE_XAA", "1");
    {
        std::ofstream idp_config(root / ".loom" / "xaa-idp.txt");
        idp_config << "idp_url=" << server.base_url() << "\n";
        idp_config << "idp_issuer=" << server.base_url() << "\n";
        idp_config << "idp_token_endpoint=" << server.base_url() << "/token\n";
        idp_config << "idp_id_token=fake-id-token-for-testing\n";
        idp_config << "client_id=idp-client-1\n";
        idp_config << "client_secret=as-secret-1\n";
        idp_config << "idp_client_id=idp-client-1\n";
        idp_config << "scope=openid profile mcp\n";
    }

    cc::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    // Point to mock server so PRM discovery succeeds
    auth_config.url = server.base_url() + "/mcp";
    auth_config.oauth = cc::services::mcp::McpOAuthConfig{
        .client_id = "as-client-1",
        .xaa = true,
    };

    bool authorization_url_called = false;
    auto result = cc::services::mcp::perform_mcp_oauth_flow(
        "xaa-fixture",
        auth_config,
        [&](const std::string&) {
            authorization_url_called = true;
        },
        std::nullopt,
        true);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    // No browser consent screen should have been shown — id_token was provided
    EXPECT_FALSE(authorization_url_called);

    // Verify the full XAA pipeline executed
    EXPECT_GE(server.prm_request_count(), 1)
        << "PRM discovery should have been called";
    EXPECT_GE(server.as_metadata_request_count(), 1)
        << "AS metadata discovery should have been called";
    EXPECT_GE(server.token_exchange_count(), 1)
        << "IdP token exchange (RFC 8693) should have been called";
    EXPECT_GE(server.jwt_bearer_count(), 1)
        << "AS jwt-bearer grant (RFC 7523) should have been called";

    // Verify the token exchange sent our fake id_token and correct client_id
    const auto exchange_body = server.token_exchange_body();
    EXPECT_NE(exchange_body.find("subject_token=fake-id-token-for-testing"), std::string::npos)
        << exchange_body;
    EXPECT_NE(exchange_body.find("client_id=idp-client-1"), std::string::npos)
        << exchange_body;
    EXPECT_NE(exchange_body.find("requested_token_type=urn%3Aietf%3Aparams%3Aoauth%3Atoken-type%3Aid-jag"),
        std::string::npos) << exchange_body;

    // Verify the jwt-bearer grant used HTTP Basic auth with AS credentials
    const auto auth_header = server.jwt_bearer_auth_header();
    EXPECT_NE(auth_header.find("Basic "), std::string::npos) << auth_header;

    // Verify token persistence
    const auto server_key = cc::services::mcp::get_server_key("xaa-fixture", auth_config);
    auto sanitize_key = [](std::string_view key) {
        std::string sanitized;
        sanitized.reserve(key.size());
        for (unsigned char ch : key) {
            sanitized.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
        }
        return sanitized;
    };
    const auto token_path = root / "loom" / "mcp" / (sanitize_key(server_key) + ".json");
    auto persisted = cc::utils::json::parse_file(token_path);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_EQ(persisted->root().get_string("server_name"), "xaa-fixture");
    EXPECT_EQ(persisted->root().get_string("server_url"), *auth_config.url);
    EXPECT_EQ(persisted->root().get_string("access_token"), "xaa-access");
    EXPECT_EQ(persisted->root().get_string("refresh_token"), "xaa-refresh");
    EXPECT_EQ(persisted->root().get_string("scope"), "openid profile mcp");
    EXPECT_EQ(persisted->root().get_string("client_id"), "idp-client-1");

    fs::remove_all(root);
}

TEST(McpAuth, XaaEnabledServerRequiresConfiguredIdpConnection) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_xaa_missing_idp_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());
    EnvironmentGuard xaa_enabled_guard("LOOM_ENABLE_XAA", "1");

    cc::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = "https://mcp.example.test/mcp";
    auth_config.oauth = cc::services::mcp::McpOAuthConfig{
        .client_id = "as-client-1",
        .xaa = true,
    };

    auto result = cc::services::mcp::perform_mcp_oauth_flow(
        "xaa-missing-idp",
        auth_config,
        [](const std::string&) {},
        std::nullopt,
        true);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find("configured IdP connection"), std::string::npos);

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

    cc::services::mcp::ConnectionManagerConfig manager_config;
    manager_config.config_directory = root;
    manager_config.connection_timeout = std::chrono::milliseconds{2000};
    manager_config.auto_connect_on_start = false;

    cc::services::mcp::McpConnectionManager manager(std::move(manager_config));

    cc::services::mcp::ServerConfig server_config;
    server_config.name = "list-changed-fixture";
    server_config.transport = cc::services::mcp::TransportType::Stdio;
    server_config.command = "node";
    server_config.args = {server_path.string()};

    cc::services::mcp::McpConfig mcp_config;
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

    cc::services::mcp::McpClient::Config config;
    config.name = "roots-fixture";
    config.request_timeout = std::chrono::milliseconds{2000};
    config.init_timeout = std::chrono::milliseconds{2000};

    cc::services::mcp::McpClient client(std::move(config));
    client.set_roots_handler([] {
        return std::vector<cc::services::mcp::Root>{
            cc::services::mcp::Root{
                .uri = "file:///workspace",
                .name = std::string{"workspace"},
            },
        };
    });

    std::mutex notification_mutex;
    std::optional<cc::services::mcp::JsonRpcNotification> notification;
    client.set_notification_callback([&](const cc::services::mcp::JsonRpcNotification& value) {
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

    std::optional<cc::services::mcp::JsonRpcNotification> captured;
    {
        std::lock_guard lock(notification_mutex);
        captured = notification;
    }
    ASSERT_TRUE(captured.has_value());
    EXPECT_EQ(captured->method, "notifications/progress");
    ASSERT_TRUE(captured->params_json.has_value());
    auto params = cc::utils::json::parse(*captured->params_json);
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

    cc::services::mcp::McpClient::Config config;
    config.name = "prompt-fixture";
    config.request_timeout = std::chrono::milliseconds{2000};
    config.init_timeout = std::chrono::milliseconds{2000};

    cc::services::mcp::McpClient client(std::move(config));
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
    EXPECT_EQ(prompt->messages[0].role, cc::services::mcp::PromptRole::User);
    EXPECT_EQ(prompt->messages[0].content, "Review migration");
    EXPECT_EQ(prompt->messages[1].role, cc::services::mcp::PromptRole::Assistant);
    EXPECT_EQ(prompt->messages[1].content, "[Resource from prompt-fixture at file:///notes.md] notes body");
    EXPECT_EQ(prompt->messages[2].content, "[Resource link: notes] file:///notes.md (Reference notes)");
    EXPECT_NE(prompt->messages[3].content.find("[Image from prompt-fixture] Binary content (image/png"), std::string::npos);

    client.shutdown();
    fs::remove_all(root);
}

TEST(ConfigManager, PersistsMcpServerSettings) {
    const auto root = c13_make_temp_root("loom_config_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);

    cc::core::ConfigManager manager(root / "global.json", root / "project.json");
    auto& settings = manager.settings_mut();
    settings.mcp_servers.push_back(cc::core::McpServerConfig{
        .name = "echo",
        .command = "node",
        .args = {"server.js", "--flag"},
        .env = {{"FOO", "bar"}},
    });

    ASSERT_TRUE(manager.save(cc::core::ConfigSource::ProjectConfig).has_value());

    cc::core::ConfigManager loaded(root / "global.json", root / "project.json");
    ASSERT_TRUE(loaded.load().has_value());
    ASSERT_EQ(loaded.settings().mcp_servers.size(), 1u);
    EXPECT_EQ(loaded.settings().mcp_servers.front().name, "echo");
    EXPECT_EQ(loaded.settings().mcp_servers.front().command, "node");
    ASSERT_EQ(loaded.settings().mcp_servers.front().args.size(), 2u);
    EXPECT_EQ(loaded.settings().mcp_servers.front().args[0], "server.js");
    EXPECT_EQ(loaded.settings().mcp_servers.front().args[1], "--flag");
	EXPECT_EQ(loaded.settings().mcp_servers.front().env.at("FOO"), "bar");

	fs::remove_all(root);
}

TEST(ConfigManager, PreservesRemoteMcpServerAuthSettings) {
    const auto root = c13_make_temp_root("loom_remote_mcp_config_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);

    cc::core::ConfigManager manager(root / "global.json", root / "project.json");
    auto& settings = manager.settings_mut();
    settings.mcp_servers.push_back(cc::core::McpServerConfig{
        .name = "remote",
        .command = {},
        .args = {},
        .env = {},
        .transport = "http",
        .url = "https://mcp.example.com/mcp",
        .headers = {{"X-Test", "present"}},
        .headers_helper = "node headers.js",
        .oauth = cc::core::McpOAuthConfig{
            .auth_server_metadata_url = "https://auth.example.com/.well-known/oauth-authorization-server",
            .callback_port = 19485,
            .client_id = "client-1",
            .xaa = true,
        },
    });

    ASSERT_TRUE(manager.save(cc::core::ConfigSource::ProjectConfig).has_value());

    cc::core::ConfigManager loaded(root / "global.json", root / "project.json");
    ASSERT_TRUE(loaded.load().has_value());
    ASSERT_EQ(loaded.settings().mcp_servers.size(), 1u);
    const auto& server = loaded.settings().mcp_servers.front();
    EXPECT_EQ(server.name, "remote");
    EXPECT_EQ(server.transport, "http");
    ASSERT_TRUE(server.url.has_value());
    EXPECT_EQ(*server.url, "https://mcp.example.com/mcp");
    EXPECT_EQ(server.headers.at("X-Test"), "present");
    ASSERT_TRUE(server.headers_helper.has_value());
    EXPECT_EQ(*server.headers_helper, "node headers.js");
    ASSERT_TRUE(server.oauth.has_value());
    ASSERT_TRUE(server.oauth->auth_server_metadata_url.has_value());
    EXPECT_EQ(*server.oauth->auth_server_metadata_url, "https://auth.example.com/.well-known/oauth-authorization-server");
    ASSERT_TRUE(server.oauth->callback_port.has_value());
    EXPECT_EQ(*server.oauth->callback_port, 19485);
    ASSERT_TRUE(server.oauth->client_id.has_value());
    EXPECT_EQ(*server.oauth->client_id, "client-1");
    EXPECT_TRUE(server.oauth->xaa);

    fs::remove_all(root);
}

// RFC-0001 B4: persisted-data round-trip coverage for the canonical
// cc.config.mcp_types settings shape — legacy snake_case reads, project/global
// layering, and environment-layer non-interference.
TEST(McpTypes, ReadsOldShapedSnakeCaseAndRewritesCamelCase) {
    const auto root = c13_make_temp_root("loom_mcp_types_legacy_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto global_path = root / "global.json";
    const auto project_path = root / "project.json";

    {
        std::ofstream file(project_path);
        // Every legacy snake_case key the parser historically accepted, plus
        // "transport" (instead of "type") and the previously dropped keys
        // "disabled" / oauth "issuer"; configScope is intentionally absent
        // so the "project" default path is exercised.
        file << R"JSON({
  "mcpServers": {
    "legacy": {
      "transport": "http",
      "url": "https://mcp.example.com/mcp",
      "headers": {"X-Test": "present"},
      "headers_helper": "node headers.js",
      "disabled": false,
      "oauth": {
        "auth_server_metadata_url": "https://auth.example.com/.well-known/oauth-authorization-server",
        "callback_port": 19485,
        "client_id": "client-1",
        "xaa": true,
        "issuer": "https://issuer.example.com"
      }
    }
  }
})JSON";
    }

    auto assert_legacy_fields = [](const cc::core::ConfigManager& manager) {
        ASSERT_EQ(manager.settings().mcp_servers.size(), 1u);
        const auto& server = manager.settings().mcp_servers.front();
        EXPECT_EQ(server.name, "legacy");
        EXPECT_EQ(server.transport, "http");
        ASSERT_TRUE(server.url.has_value());
        EXPECT_EQ(*server.url, "https://mcp.example.com/mcp");
        EXPECT_EQ(server.headers.at("X-Test"), "present");
        ASSERT_TRUE(server.headers_helper.has_value());
        EXPECT_EQ(*server.headers_helper, "node headers.js");
        ASSERT_TRUE(server.oauth.has_value());
        ASSERT_TRUE(server.oauth->auth_server_metadata_url.has_value());
        EXPECT_EQ(*server.oauth->auth_server_metadata_url,
                  "https://auth.example.com/.well-known/oauth-authorization-server");
        ASSERT_TRUE(server.oauth->callback_port.has_value());
        EXPECT_EQ(*server.oauth->callback_port, 19485);
        ASSERT_TRUE(server.oauth->client_id.has_value());
        EXPECT_EQ(*server.oauth->client_id, "client-1");
        EXPECT_TRUE(server.oauth->xaa);
        // Round-trip guarantee (RFC-0001 B followup c4): the previously
        // dropped fields now survive. "disabled" is optional<bool>, so an
        // explicit false is captured rather than defaulted away; oauth
        // "issuer" is read in canonical spelling; and an absent configScope
        // falls back to the "project" default.
        ASSERT_TRUE(server.disabled.has_value());
        EXPECT_FALSE(*server.disabled);
        ASSERT_TRUE(server.oauth->issuer.has_value());
        EXPECT_EQ(*server.oauth->issuer, "https://issuer.example.com");
        EXPECT_EQ(server.config_scope, "project");
    };

    {
        cc::core::ConfigManager loaded(global_path, project_path);
        ASSERT_TRUE(loaded.load().has_value());
        assert_legacy_fields(loaded);

        // Re-save: the serializer must rewrite every READ field in canonical
        // camelCase with zero loss.
        ASSERT_TRUE(loaded.save(cc::core::ConfigSource::ProjectConfig).has_value());
    }

    std::string rewritten;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        rewritten = buffer.str();
    }
    EXPECT_NE(rewritten.find("\"headersHelper\""), std::string::npos);
    EXPECT_EQ(rewritten.find("headers_helper"), std::string::npos);
    EXPECT_NE(rewritten.find("\"authServerMetadataUrl\""), std::string::npos);
    EXPECT_EQ(rewritten.find("auth_server_metadata_url"), std::string::npos);
    EXPECT_NE(rewritten.find("\"callbackPort\""), std::string::npos);
    EXPECT_EQ(rewritten.find("callback_port"), std::string::npos);
    EXPECT_NE(rewritten.find("\"clientId\""), std::string::npos);
    EXPECT_EQ(rewritten.find("client_id"), std::string::npos);
    EXPECT_NE(rewritten.find("\"type\": \"http\""), std::string::npos);
    EXPECT_EQ(rewritten.find("\"transport\""), std::string::npos);
    // Previously dropped fields are rewritten in canonical form; configScope
    // is always written (defaulting to "project"), never in snake_case.
    EXPECT_NE(rewritten.find("\"disabled\": false"), std::string::npos);
    EXPECT_NE(rewritten.find("\"issuer\""), std::string::npos);
    EXPECT_NE(rewritten.find("\"configScope\": \"project\""), std::string::npos);
    EXPECT_EQ(rewritten.find("config_scope"), std::string::npos);

    {
        cc::core::ConfigManager reloaded(global_path, project_path);
        ASSERT_TRUE(reloaded.load().has_value());
        assert_legacy_fields(reloaded);
    }

    fs::remove_all(root);
}

// RFC-0001 B followup c4: canonical camelCase input with an explicitly
// disabled server and an oauth issuer must survive load -> save -> reload
// byte-for-byte in meaning, and be rewritten in canonical spelling.
TEST(McpTypes, DisabledAndOauthIssuerSurviveConfigRewrite) {
    const auto root = c13_make_temp_root("loom_mcp_types_disabled_issuer_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto global_path = root / "global.json";
    const auto project_path = root / "project.json";

    {
        std::ofstream file(project_path);
        file << R"JSON({
  "mcpServers": {
    "canonical": {
      "type": "http",
      "url": "https://mcp.example.com/mcp",
      "disabled": true,
      "configScope": "user",
      "oauth": {
        "issuer": "https://issuer.example.com"
      }
    }
  }
})JSON";
    }

    {
        cc::core::ConfigManager loaded(global_path, project_path);
        ASSERT_TRUE(loaded.load().has_value());
        ASSERT_EQ(loaded.settings().mcp_servers.size(), 1u);
        const auto& server = loaded.settings().mcp_servers.front();
        ASSERT_TRUE(server.disabled.has_value());
        EXPECT_TRUE(*server.disabled);
        EXPECT_EQ(server.config_scope, "user");
        ASSERT_TRUE(server.oauth.has_value());
        ASSERT_TRUE(server.oauth->issuer.has_value());
        EXPECT_EQ(*server.oauth->issuer, "https://issuer.example.com");

        ASSERT_TRUE(loaded.save(cc::core::ConfigSource::ProjectConfig).has_value());
    }

    std::string rewritten;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        rewritten = buffer.str();
    }
    EXPECT_NE(rewritten.find("\"disabled\": true"), std::string::npos);
    EXPECT_NE(rewritten.find("\"issuer\": \"https://issuer.example.com\""), std::string::npos);
    EXPECT_NE(rewritten.find("\"configScope\": \"user\""), std::string::npos);
    EXPECT_EQ(rewritten.find("config_scope"), std::string::npos);

    cc::core::ConfigManager reloaded(global_path, project_path);
    ASSERT_TRUE(reloaded.load().has_value());
    ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
    const auto& server = reloaded.settings().mcp_servers.front();
    ASSERT_TRUE(server.disabled.has_value());
    EXPECT_TRUE(*server.disabled);
    EXPECT_EQ(server.config_scope, "user");
    ASSERT_TRUE(server.oauth.has_value());
    ASSERT_TRUE(server.oauth->issuer.has_value());
    EXPECT_EQ(*server.oauth->issuer, "https://issuer.example.com");

    fs::remove_all(root);
}

// RFC-0001 B followup c4: configScope reads canonical camelCase and legacy
// snake_case alike, but every rewrite emits only the canonical camelCase
// key, including across a reload.
TEST(McpTypes, ConfigScopeRoundTripsInCanonicalCamelCase) {
    const auto root = c13_make_temp_root("loom_mcp_types_scope_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto global_path = root / "global.json";
    const auto project_path = root / "project.json";

    {
        std::ofstream file(project_path);
        file << R"JSON({
  "mcpServers": {
    "legacy-scope": {
      "command": "node",
      "args": ["legacy.js"],
      "config_scope": "user"
    },
    "canonical-scope": {
      "command": "node",
      "args": ["canonical.js"],
      "configScope": "local"
    },
    "both-scopes": {
      "command": "node",
      "args": ["both.js"],
      "configScope": "local",
      "config_scope": "user"
    }
  }
})JSON";
    }

    {
        cc::core::ConfigManager loaded(global_path, project_path);
        ASSERT_TRUE(loaded.load().has_value());
        ASSERT_EQ(loaded.settings().mcp_servers.size(), 3u);
        EXPECT_EQ(loaded.settings().mcp_servers[0].config_scope, "user");
        EXPECT_EQ(loaded.settings().mcp_servers[1].config_scope, "local");
        // When both spellings are present, canonical camelCase wins
        // (json_string(...).or_else(...) short-circuits on the first hit).
        EXPECT_EQ(loaded.settings().mcp_servers[2].config_scope, "local");

        ASSERT_TRUE(loaded.save(cc::core::ConfigSource::ProjectConfig).has_value());
    }

    std::string rewritten;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        rewritten = buffer.str();
    }
    EXPECT_NE(rewritten.find("\"configScope\": \"user\""), std::string::npos);
    EXPECT_NE(rewritten.find("\"configScope\": \"local\""), std::string::npos);
    EXPECT_EQ(rewritten.find("config_scope"), std::string::npos);

    cc::core::ConfigManager reloaded(global_path, project_path);
    ASSERT_TRUE(reloaded.load().has_value());
    ASSERT_EQ(reloaded.settings().mcp_servers.size(), 3u);
    EXPECT_EQ(reloaded.settings().mcp_servers[0].config_scope, "user");
    EXPECT_EQ(reloaded.settings().mcp_servers[1].config_scope, "local");
    EXPECT_EQ(reloaded.settings().mcp_servers[2].config_scope, "local");

    fs::remove_all(root);
}

// RFC-0001 B followup c4: absent optional keys stay unset. A minimal server
// must round-trip without a "disabled" or oauth "issuer" key in the rewrite,
// while configScope is still written with its "project" default.
TEST(McpTypes, AbsentDisabledAndIssuerKeysStayUnset) {
    const auto root = c13_make_temp_root("loom_mcp_types_absent_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto global_path = root / "global.json";
    const auto project_path = root / "project.json";

    {
        std::ofstream file(project_path);
        file << R"JSON({
  "mcpServers": {
    "minimal": {
      "command": "node"
    }
  }
})JSON";
    }

    {
        cc::core::ConfigManager loaded(global_path, project_path);
        ASSERT_TRUE(loaded.load().has_value());
        ASSERT_EQ(loaded.settings().mcp_servers.size(), 1u);
        const auto& server = loaded.settings().mcp_servers.front();
        EXPECT_FALSE(server.disabled.has_value());
        EXPECT_FALSE(server.oauth.has_value());
        EXPECT_EQ(server.config_scope, "project");

        ASSERT_TRUE(loaded.save(cc::core::ConfigSource::ProjectConfig).has_value());
    }

    std::string rewritten;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        rewritten = buffer.str();
    }
    EXPECT_EQ(rewritten.find("disabled"), std::string::npos);
    EXPECT_EQ(rewritten.find("issuer"), std::string::npos);
    EXPECT_NE(rewritten.find("\"configScope\": \"project\""), std::string::npos);

    cc::core::ConfigManager reloaded(global_path, project_path);
    ASSERT_TRUE(reloaded.load().has_value());
    ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
    const auto& server = reloaded.settings().mcp_servers.front();
    EXPECT_FALSE(server.disabled.has_value());
    EXPECT_FALSE(server.oauth.has_value());
    EXPECT_EQ(server.config_scope, "project");

    fs::remove_all(root);
}

// RFC-0001 B followup c12: the xaaIdp section (issuer/clientId/callbackPort)
// used to be dropped by the hand-rolled full-save serializer. All three
// values must survive an unrelated mutation + rewrite + reload, and the
// rewritten document must carry the canonical camelCase keys exactly once.
// Bad-typed members are tolerated (folded-in second phase below).
TEST(McpTypes, XaaIdpRoundTripsConfigRewrite) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = c13_make_temp_root("loom_mcp_types_xaa_idp_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto global_path = root / "global.json";
    const auto project_path = root / "project.json";

    {
        std::ofstream file(project_path);
        file << R"JSON({
  "model": {
    "default_model": "claude-x"
  },
  "mcpServers": {
    "keep-me": {
      "command": "node",
      "args": ["serve.js"]
    }
  },
  "xaaIdp": {
    "issuer": "https://idp.example.com",
    "clientId": "loom-cli",
    "callbackPort": 8765
  }
})JSON";
    }

    {
        cc::core::ConfigManager loaded(global_path, project_path);
        ASSERT_TRUE(loaded.load().has_value());
        const auto& xaa = loaded.settings().xaa_idp;
        EXPECT_EQ(xaa.issuer, "https://idp.example.com");
        EXPECT_EQ(xaa.client_id, "loom-cli");
        ASSERT_TRUE(xaa.callback_port.has_value());
        EXPECT_EQ(*xaa.callback_port, 8765);
        ASSERT_EQ(loaded.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(loaded.settings().mcp_servers.front().name, "keep-me");
        EXPECT_EQ(loaded.settings().model.default_model, "claude-x");

        // Mutate something unrelated (display theme), then full-save.
        loaded.settings_mut().display.theme = "dark";
        ASSERT_TRUE(loaded.save(cc::core::ConfigSource::ProjectConfig).has_value());
    }

    // Byte-semantic check of the rewritten document.
    std::string rewritten;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        rewritten = buffer.str();
    }
    auto doc = cc::utils::json::parse(rewritten);
    ASSERT_TRUE(doc.has_value());
    const auto xaa_obj = doc->root().get("xaaIdp");
    ASSERT_TRUE(xaa_obj.is_obj());
    EXPECT_EQ(std::string(xaa_obj.get("issuer").as_str()), "https://idp.example.com");
    EXPECT_EQ(std::string(xaa_obj.get("clientId").as_str()), "loom-cli");
    ASSERT_TRUE(xaa_obj.get("callbackPort").is_num());
    EXPECT_EQ(xaa_obj.get("callbackPort").as_int(), 8765);
    // Canonical camelCase only; the section appears exactly once.
    EXPECT_EQ(rewritten.find("client_id"), std::string::npos);
    EXPECT_EQ(rewritten.find("callback_port"), std::string::npos);
    ASSERT_NE(rewritten.find("xaaIdp"), std::string::npos);
    EXPECT_EQ(rewritten.find("xaaIdp"), rewritten.rfind("xaaIdp"));
    // The unrelated mcp server and the mutation survive untouched.
    const auto servers = doc->root().get("mcpServers");
    ASSERT_TRUE(servers.is_obj());
    const auto keep = servers.get("keep-me");
    ASSERT_TRUE(keep.is_obj());
    EXPECT_EQ(std::string(keep.get("command").as_str()), "node");
    EXPECT_EQ(std::string(doc->root().get("display").get("theme").as_str()), "dark");

    // Reload: all three XAA values survive.
    cc::core::ConfigManager reloaded(global_path, project_path);
    ASSERT_TRUE(reloaded.load().has_value());
    const auto& xaa = reloaded.settings().xaa_idp;
    EXPECT_EQ(xaa.issuer, "https://idp.example.com");
    EXPECT_EQ(xaa.client_id, "loom-cli");
    ASSERT_TRUE(xaa.callback_port.has_value());
    EXPECT_EQ(*xaa.callback_port, 8765);
    ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
    EXPECT_EQ(reloaded.settings().mcp_servers.front().name, "keep-me");

    fs::remove_all(root);

    // Bad-type tolerance: a numeric issuer and a string callbackPort are
    // ignored without failing the load, while a well-typed clientId in the
    // same object still parses.
    const auto bad_root = fs::temp_directory_path() /
        ("loom_mcp_types_xaa_idp_bad_" + std::to_string(suffix));
    fs::create_directories(bad_root);
    const auto bad_global = bad_root / "global.json";
    const auto bad_project = bad_root / "project.json";
    {
        std::ofstream file(bad_project);
        file << R"JSON({
  "xaaIdp": {
    "issuer": 42,
    "clientId": "partial-client",
    "callbackPort": "8080"
  }
})JSON";
    }
    cc::core::ConfigManager bad(bad_global, bad_project);
    ASSERT_TRUE(bad.load().has_value());
    const auto& bad_xaa = bad.settings().xaa_idp;
    EXPECT_TRUE(bad_xaa.issuer.empty());
    EXPECT_EQ(bad_xaa.client_id, "partial-client");
    EXPECT_FALSE(bad_xaa.callback_port.has_value());

    fs::remove_all(bad_root);
}

// RFC-0001 B followup c12: default settings never emit an xaaIdp section,
// and a save/load/save cycle with the section absent is byte-stable (this
// is also what makes /mcp xaa clear remove the section on rewrite).
TEST(McpTypes, XaaIdpOmittedWhenUnset) {
    const auto root = c13_make_temp_root("loom_mcp_types_xaa_idp_absent_test_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto global_path = root / "global.json";
    const auto project_path = root / "project.json";

    std::string first;
    {
        cc::core::ConfigManager fresh(global_path, project_path);
        ASSERT_TRUE(fresh.load().has_value());
        EXPECT_TRUE(fresh.settings().xaa_idp.issuer.empty());
        EXPECT_TRUE(fresh.settings().xaa_idp.client_id.empty());
        EXPECT_FALSE(fresh.settings().xaa_idp.callback_port.has_value());
        ASSERT_TRUE(fresh.save(cc::core::ConfigSource::ProjectConfig).has_value());

        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        first = buffer.str();
    }
    EXPECT_EQ(first.find("xaaIdp"), std::string::npos);

    {
        cc::core::ConfigManager reloaded(global_path, project_path);
        ASSERT_TRUE(reloaded.load().has_value());
        EXPECT_TRUE(reloaded.settings().xaa_idp.issuer.empty());
        EXPECT_FALSE(reloaded.settings().xaa_idp.callback_port.has_value());
        ASSERT_TRUE(reloaded.save(cc::core::ConfigSource::ProjectConfig).has_value());
    }
    std::string second;
    {
        std::ifstream file(project_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        second = buffer.str();
    }
    EXPECT_EQ(second.find("xaaIdp"), std::string::npos);
    EXPECT_EQ(first, second);

    fs::remove_all(root);
}

// RFC-0001 B followup c6 (D2): mcpServers now merge with per-entry name
// overlay across global -> user -> project -> local instead of the project
// file replacing the whole global block. Global g1/g2 survive a project file
// that defines p1 (and overrides g1 when it redefines it), and an EMPTY
// project mcpServers object overrides nothing.
TEST(McpTypes, ProjectMcpServersOverlayGlobal) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_types_overlay_test_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto global_path = root / "global.json";
    const auto project_path = root / "project.json";

    {
        std::ofstream global_file(global_path);
        global_file << R"JSON({
  "mcpServers": {
    "g1": {"command": "node", "args": ["g1.js"]},
    "g2": {"command": "node", "args": ["g2.js"]}
  }
})JSON";
        std::ofstream project_file(project_path);
        project_file << R"JSON({
  "mcpServers": {
    "g1": {"command": "node", "args": ["p-g1.js"]},
    "p1": {"command": "node", "args": ["p1.js"]}
  }
})JSON";
    }

    cc::core::ConfigManager manager(global_path, project_path);
    ASSERT_TRUE(manager.load().has_value());
    // Overlay: same-named g1 is replaced in place at the project tier, so the
    // effective order is g2, g1(project), p1.
    const auto& servers = manager.settings().mcp_servers;
    ASSERT_EQ(servers.size(), 3u);
    EXPECT_EQ(servers[0].name, "g2");
    EXPECT_EQ(servers[1].name, "g1");
    EXPECT_EQ(servers[1].args, (std::vector<std::string>{"p-g1.js"}));
    EXPECT_EQ(servers[2].name, "p1");
    ASSERT_TRUE(manager.mcp_server_owner("g1").has_value());
    EXPECT_EQ(*manager.mcp_server_owner("g1"), cc::core::McpStorageScope::Project);
    EXPECT_EQ(*manager.mcp_server_owner("g2"), cc::core::McpStorageScope::Global);

    // An empty project mcpServers object overrides NOTHING: globals survive.
    {
        std::ofstream project_file(project_path, std::ios::trunc);
        project_file << R"JSON({
  "mcpServers": {}
})JSON";
    }
    cc::core::ConfigManager empty_object_manager(global_path, project_path);
    ASSERT_TRUE(empty_object_manager.load().has_value());
    ASSERT_EQ(empty_object_manager.settings().mcp_servers.size(), 2u);
    EXPECT_EQ(empty_object_manager.settings().mcp_servers[0].name, "g1");
    EXPECT_EQ(empty_object_manager.settings().mcp_servers[1].name, "g2");

    fs::remove_all(root);
}

TEST(McpTypes, ProjectWithoutMcpServersKeepsGlobal) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_types_keep_global_test_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto global_path = root / "global.json";
    const auto project_path = root / "project.json";

    {
        std::ofstream global_file(global_path);
        global_file << R"JSON({
  "mcpServers": {
    "g1": {"command": "node", "args": ["g1.js"]},
    "g2": {"command": "node", "args": ["g2.js"]}
  }
})JSON";
        // Project layer has no mcpServers key: globals survive untouched.
        std::ofstream project_file(project_path);
        project_file << R"JSON({
  "systemPrompt": "keep"
})JSON";
    }

    cc::core::ConfigManager manager(global_path, project_path);
    ASSERT_TRUE(manager.load().has_value());
    ASSERT_EQ(manager.settings().mcp_servers.size(), 2u);
    EXPECT_EQ(manager.settings().mcp_servers[0].name, "g1");
    EXPECT_EQ(manager.settings().mcp_servers[1].name, "g2");

    // A missing global file is ConfigNotFound and tolerated.
    cc::core::ConfigManager missing_global(root / "does-not-exist.json", project_path);
    ASSERT_TRUE(missing_global.load().has_value());
    EXPECT_TRUE(missing_global.settings().mcp_servers.empty());

    fs::remove_all(root);
}

TEST(McpTypes, EnvironmentLayerLeavesMcpServersUntouched) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_types_env_test_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto global_path = root / "global.json";
    const auto project_path = root / "project.json";

    EnvironmentGuard api_key_guard("ANTHROPIC_API_KEY", "env-layer-test-key");
    EnvironmentGuard base_url_guard("ANTHROPIC_BASE_URL", "https://env.example.com");

    {
        std::ofstream project_file(project_path);
        project_file << R"JSON({
  "mcpServers": {
    "remote": {
      "type": "http",
      "url": "https://mcp.example.com/mcp",
      "headers": {"X-Test": "present"},
      "headersHelper": "node headers.js"
    }
  }
})JSON";
    }

    cc::core::ConfigManager manager(global_path, project_path);
    ASSERT_TRUE(manager.load().has_value());

    // The environment layer demonstrably ran...
    ASSERT_TRUE(manager.settings().network.api_key.has_value());
    EXPECT_EQ(*manager.settings().network.api_key, "env-layer-test-key");
    ASSERT_TRUE(manager.settings().network.base_url.has_value());
    EXPECT_EQ(*manager.settings().network.base_url, "https://env.example.com");

    // ...and left the file-defined MCP servers byte-for-byte intact.
    ASSERT_EQ(manager.settings().mcp_servers.size(), 1u);
    const auto& server = manager.settings().mcp_servers.front();
    EXPECT_EQ(server.name, "remote");
    EXPECT_EQ(server.transport, "http");
    ASSERT_TRUE(server.url.has_value());
    EXPECT_EQ(*server.url, "https://mcp.example.com/mcp");
    EXPECT_EQ(server.headers.at("X-Test"), "present");
    ASSERT_TRUE(server.headers_helper.has_value());
    EXPECT_EQ(*server.headers_helper, "node headers.js");

    fs::remove_all(root);
}

// ============================================================================
// RFC-0001 B followup c6: per-tier MCP file routing (design doc groups 2-13)
// ============================================================================

namespace {

[[nodiscard]] std::string c6_read_file(const fs::path& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void c6_write_file(const fs::path& path, std::string_view content) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path);
    file << content;
}

[[nodiscard]] std::size_t c6_count_occurrences(std::string_view haystack,
                                               std::string_view needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

[[nodiscard]] std::vector<std::string>
c6_json_keys(cc::utils::json::JsonVal object) {
    std::vector<std::string> keys;
    object.iter_obj([&](auto key, auto) {
        if (key.is_str()) keys.emplace_back(key.as_str());
    });
    return keys;
}

}  // namespace

// Group 2: four physical files, lowest-to-highest precedence; a name in all
// four resolves to the local value; the legacy global tier is still read.
TEST(McpTypes, McpStorageFourFilePrecedence) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_four_file_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    c6_write_file(global_path, R"JSON({
  "mcpServers": {
    "onlyg": {"command": "node", "args": ["g.js"]},
    "shared": {"command": "node", "args": ["global.js"]}
  }
})JSON");
    c6_write_file(user_path, R"JSON({
  "mcpServers": {
    "onlyu": {"command": "node", "args": ["u.js"]},
    "shared": {"command": "node", "args": ["user.js"]}
  }
})JSON");
    c6_write_file(project_path, R"JSON({
  "mcpServers": {
    "onlyp": {"command": "node", "args": ["p.js"]},
    "shared": {"command": "node", "args": ["project.js"]}
  }
})JSON");
    c6_write_file(local_path, R"JSON({
  "mcpServers": {
    "onlyl": {"command": "node", "args": ["l.js"]},
    "shared": {"command": "node", "args": ["local.js"]}
  }
})JSON");

    cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
    const auto paths = manager.mcp_scope_paths();
    ASSERT_EQ(paths.size(), 4u);
    EXPECT_EQ(paths[0].first, cc::core::McpStorageScope::Global);
    EXPECT_EQ(paths[1].first, cc::core::McpStorageScope::User);
    EXPECT_EQ(paths[2].first, cc::core::McpStorageScope::Project);
    EXPECT_EQ(paths[3].first, cc::core::McpStorageScope::Local);

    ASSERT_TRUE(manager.load().has_value());
    std::map<std::string, cc::core::McpServerConfig> by_name;
    for (const auto& server : manager.settings().mcp_servers) {
        by_name[server.name] = server;
    }
    ASSERT_EQ(by_name.size(), 5u);
    EXPECT_EQ(by_name.at("shared").args, (std::vector<std::string>{"local.js"}));
    EXPECT_EQ(by_name.at("onlyg").args, (std::vector<std::string>{"g.js"}));
    EXPECT_EQ(by_name.at("onlyu").args, (std::vector<std::string>{"u.js"}));
    EXPECT_EQ(by_name.at("onlyp").args, (std::vector<std::string>{"p.js"}));
    EXPECT_EQ(by_name.at("onlyl").args, (std::vector<std::string>{"l.js"}));

    EXPECT_EQ(*manager.mcp_server_owner("shared"), cc::core::McpStorageScope::Local);
    EXPECT_EQ(*manager.mcp_server_owner("onlyg"),  cc::core::McpStorageScope::Global);
    EXPECT_EQ(*manager.mcp_server_owner("onlyu"),  cc::core::McpStorageScope::User);
    EXPECT_EQ(*manager.mcp_server_owner("onlyp"),  cc::core::McpStorageScope::Project);
    EXPECT_EQ(*manager.mcp_server_owner("onlyl"),  cc::core::McpStorageScope::Local);

    const auto shared_files = manager.find_mcp_server_files("shared");
    ASSERT_EQ(shared_files.size(), 4u);
    EXPECT_EQ(shared_files.back().first, cc::core::McpStorageScope::Local);
    EXPECT_TRUE(manager.find_mcp_server_files("onlyg").size() == 1u);

    fs::remove_all(root);
}

// Groups 3 + 8: upsert(User) writes ONLY the user file (no model/display/
// features sections, other files untouched), and the patched entry is
// compared STRUCTURALLY with the C4-pinned key set/order preserved.
TEST(McpTypes, McpUpsertUserWritesOnlyUserFile) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_user_upsert_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    cc::core::McpServerConfig cfg;
    cfg.name = "srv";
    cfg.transport = "http";
    cfg.args = {"a1", "a2"};
    cfg.env = {{"K", "V"}};
    cfg.url = "https://mcp.example.com/mcp";
    cfg.headers = {{"Authorization", "Bearer abc"}};
    cfg.headers_helper = "node h.js";
    cfg.disabled = false;
    cfg.config_scope = "user";
    cc::core::McpOAuthConfig oauth;
    oauth.auth_server_metadata_url = "https://auth.example.com/meta";
    oauth.callback_port = 19485;
    oauth.client_id = "client-1";
    oauth.xaa = true;
    oauth.issuer = "https://issuer.example.com";
    cfg.oauth = oauth;

    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        auto upserted = manager.upsert_mcp_server(cc::core::McpStorageScope::User, cfg);
        ASSERT_TRUE(upserted.has_value()) << upserted.error().message;
    }

    EXPECT_FALSE(fs::exists(global_path));
    EXPECT_TRUE(fs::exists(user_path));
    EXPECT_FALSE(fs::exists(project_path));
    EXPECT_FALSE(fs::exists(local_path));

    auto parsed = cc::utils::json::parse_file(user_path);
    ASSERT_TRUE(parsed.has_value());
    // The file contains exactly one top-level section: mcpServers.
    const auto root_keys = c6_json_keys(parsed->root());
    ASSERT_EQ(root_keys.size(), 1u);
    EXPECT_EQ(root_keys[0], "mcpServers");

    const auto servers = parsed->root().get("mcpServers");
    ASSERT_TRUE(servers.is_obj());
    const auto entry = servers.get("srv");
    ASSERT_TRUE(entry.is_obj());

    // Key order is exactly the C4-pinned set (command omitted as empty).
    const auto keys = c6_json_keys(entry);
    EXPECT_EQ(keys, (std::vector<std::string>{
        "type", "args", "env", "url", "headers", "headersHelper",
        "disabled", "configScope", "oauth"}));
    EXPECT_EQ(entry.get("type").as_str(), std::string_view("http"));
    ASSERT_TRUE(entry.get("args").is_arr());
    EXPECT_EQ(entry.get("args").at(0).as_str(), std::string_view("a1"));
    EXPECT_EQ(entry.get("args").at(1).as_str(), std::string_view("a2"));
    EXPECT_EQ(entry.get("env").get("K").as_str(), std::string_view("V"));
    EXPECT_EQ(entry.get("url").as_str(), std::string_view("https://mcp.example.com/mcp"));
    EXPECT_EQ(entry.get("headers").get("Authorization").as_str(),
              std::string_view("Bearer abc"));
    EXPECT_EQ(entry.get("headersHelper").as_str(), std::string_view("node h.js"));
    EXPECT_FALSE(entry.get("disabled").as_bool());
    EXPECT_EQ(entry.get("configScope").as_str(), std::string_view("user"));
    const auto oauth_json = entry.get("oauth");
    ASSERT_TRUE(oauth_json.is_obj());
    EXPECT_EQ(c6_json_keys(oauth_json), (std::vector<std::string>{
        "authServerMetadataUrl", "callbackPort", "clientId", "xaa", "issuer"}));
    EXPECT_EQ(oauth_json.get("callbackPort").as_int(), 19485);
    EXPECT_EQ(oauth_json.get("clientId").as_str(), std::string_view("client-1"));
    EXPECT_TRUE(oauth_json.get("xaa").as_bool());

    // Reload picks the user entry up.
    {
        cc::core::ConfigManager reloaded(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(reloaded.load().has_value());
        ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(reloaded.settings().mcp_servers[0].name, "srv");
        EXPECT_EQ(*reloaded.settings().mcp_servers[0].url, "https://mcp.example.com/mcp");

        // Same-name upsert replaces, never duplicates.
        cfg.url = "https://mcp.example.com/v2";
        ASSERT_TRUE(reloaded.upsert_mcp_server(cc::core::McpStorageScope::User, cfg)
                        .has_value());
    }
    auto reparsed = cc::utils::json::parse_file(user_path);
    ASSERT_TRUE(reparsed.has_value());
    EXPECT_EQ(reparsed->root().get("mcpServers").size(), 1u);
    EXPECT_EQ(reparsed->root().get("mcpServers").get("srv").get("url").as_str(),
              std::string_view("https://mcp.example.com/v2"));

    fs::remove_all(root);
}

// Group 8 (stdio variant): the patched file is whole-file yyjson-PRETTY
// (inline arrays become multiline), the key order survives, and a reload
// round-trips every field structurally.
TEST(McpTypes, McpPatchedEntryStructuralShapeAndKeyOrder) {
    const auto root = c13_make_temp_root("loom_mcp_c6_shape_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    cc::core::McpServerConfig cfg;
    cfg.name = "runner";
    cfg.transport = "stdio";
    cfg.command = "node";
    cfg.args = {"serve.js", "--port", "9000"};
    cfg.env = {{"DEBUG", "1"}};
    cfg.disabled = true;
    cfg.config_scope = "project";

    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::Project, cfg)
                        .has_value());
    }

    const std::string text = c6_read_file(project_path);
    // Pretty reformat: the inline args array is now multiline.
    EXPECT_NE(text.find("\"args\": [\n"), std::string::npos);
    auto parsed = cc::utils::json::parse_file(project_path);
    ASSERT_TRUE(parsed.has_value());
    const auto entry = parsed->root().get("mcpServers").get("runner");
    ASSERT_TRUE(entry.is_obj());
    EXPECT_EQ(c6_json_keys(entry),
              (std::vector<std::string>{"type", "command", "args", "env",
                                        "disabled", "configScope"}));
    EXPECT_EQ(entry.get("type").as_str(), std::string_view("stdio"));
    EXPECT_EQ(entry.get("command").as_str(), std::string_view("node"));
    EXPECT_EQ(entry.get("args").size(), 3u);
    EXPECT_TRUE(entry.get("disabled").as_bool());

    cc::core::ConfigManager reloaded(global_path, user_path, project_path, local_path);
    ASSERT_TRUE(reloaded.load().has_value());
    ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
    const auto& back = reloaded.settings().mcp_servers[0];
    EXPECT_EQ(back.command, "node");
    EXPECT_EQ(back.args, (std::vector<std::string>{"serve.js", "--port", "9000"}));
    EXPECT_EQ(back.env.at("DEBUG"), "1");
    ASSERT_TRUE(back.disabled.has_value());
    EXPECT_TRUE(*back.disabled);
    EXPECT_EQ(back.config_scope, "project");

    fs::remove_all(root);
}

// Group 4: the default Local scope creates config.local.json (no
// config.json) and appends its basename to ./.gitignore exactly once,
// coping with a pre-existing file that has no trailing newline.
TEST(McpTypes, McpUpsertLocalCreatesLocalFileAndGitignore) {
    if (std::system("git --version >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "git not available";
    }
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_local_gitignore_" + std::to_string(suffix));
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    // c13e: .gitignore appends happen only inside a git work tree.
    ASSERT_EQ(std::system("git init -q --initial-branch main"), 0);

    // Existing .gitignore with NO trailing newline.
    c6_write_file(root / ".gitignore", "sentinel");

    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / ".loom" / "config.json";
    const auto local_path   = root / ".loom" / "config.local.json";

    auto make_stdio = [](std::string name) {
        cc::core::McpServerConfig cfg;
        cfg.name = std::move(name);
        cfg.transport = "stdio";
        cfg.command = "node";
        cfg.args = {"s.js"};
        cfg.config_scope = "local";
        return cfg;
    };

    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::Local,
                                              make_stdio("l1")).has_value());
    }
    EXPECT_TRUE(fs::exists(local_path));
    EXPECT_FALSE(fs::exists(project_path));
    // c13d: the local lock sibling is ignored alongside the data file.
    EXPECT_EQ(c6_read_file(root / ".gitignore"),
              "sentinel\nconfig.local.json\nconfig.local.json.lock\n");

    // The local tier is the documented secret holder: owner-only (0600).
    {
        std::error_code ec;
        const auto status = fs::status(local_path, ec);
        ASSERT_FALSE(ec);
        const auto perms = status.permissions();
        const auto none = fs::perms::none;
        EXPECT_EQ(perms & (fs::perms::group_read | fs::perms::group_write |
                           fs::perms::group_exec | fs::perms::others_read |
                           fs::perms::others_write | fs::perms::others_exec),
                  none);
        EXPECT_NE(perms & fs::perms::owner_read, none);
        EXPECT_NE(perms & fs::perms::owner_write, none);
    }

    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::Local,
                                              make_stdio("l2")).has_value());
    }
    const std::string gitignore = c6_read_file(root / ".gitignore");
    // Line-exact idempotency (the lock line contains the data basename as
    // a substring, so count whole lines).
    EXPECT_EQ(c6_count_occurrences(gitignore, "config.local.json\n"), 1u);
    EXPECT_EQ(c6_count_occurrences(gitignore, "config.local.json.lock\n"), 1u);
    auto parsed = cc::utils::json::parse_file(local_path);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->root().get("mcpServers").size(), 2u);

    fs::remove_all(root);
}

// Groups 5 + 13 (headline duplication regression): a Local upsert next to a
// legacy global server leaves the global file byte-identical, creates no
// project config.json, and reloads as the {global, local} overlay; core
// commands also never touch the services-layer mcp_servers.json files.
TEST(McpTypes, McpLocalUpsertDoesNotDuplicateGlobal) {
    const auto root = c13_make_temp_root("loom_mcp_c6_dup_guard_");
    CurrentPathGuard cwd_guard(root);
    EnvironmentGuard home_guard("HOME", root.string());

    // Services-layer files (mcp_servers.json names) must stay untouched.
    const auto svc_global = cc::services::mcp::ConfigPaths::global_config();
    const auto svc_user   = cc::services::mcp::ConfigPaths::user_config();
    const auto svc_local  = cc::services::mcp::ConfigPaths::local_config(root);
    c6_write_file(svc_global, "{\"serviceGlobal\": true}\n");
    c6_write_file(svc_user,   "{\"serviceUser\": true}\n");
    c6_write_file(svc_local,  "{\"serviceLocal\": true}\n");
    const std::string svc_global_before = c6_read_file(svc_global);
    const std::string svc_user_before   = c6_read_file(svc_user);
    const std::string svc_local_before  = c6_read_file(svc_local);

    const auto global_path  = root / ".config" / "loom" / "config.json";
    const auto project_path = root / ".loom" / "config.json";
    c6_write_file(global_path, R"JSON({
  "mcpServers": {
    "g1": {"command": "node", "args": ["g1.js"]}
  }
})JSON");
    const std::string global_before = c6_read_file(global_path);

    cc::core::McpServerConfig local_cfg;
    local_cfg.name = "l1";
    local_cfg.transport = "stdio";
    local_cfg.command = "node";
    local_cfg.args = {"l1.js"};
    local_cfg.config_scope = "local";

    {
        // 2-arg ctor: local path is derived as project.local.json.
        cc::core::ConfigManager manager(global_path, project_path);
        ASSERT_EQ(manager.mcp_scope_paths().size(), 3u);  // user tier absent
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_EQ(manager.settings().mcp_servers.size(), 1u);
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::Local,
                                              local_cfg).has_value());
    }

    EXPECT_EQ(c6_read_file(global_path), global_before);
    EXPECT_FALSE(fs::exists(project_path));
    EXPECT_TRUE(fs::exists(root / ".loom" / "config.local.json"));

    cc::core::ConfigManager reloaded(global_path, project_path);
    ASSERT_TRUE(reloaded.load().has_value());
    std::map<std::string, cc::core::McpServerConfig> by_name;
    for (const auto& server : reloaded.settings().mcp_servers) by_name[server.name] = server;
    EXPECT_EQ(by_name.size(), 2u);
    EXPECT_EQ(by_name.at("g1").args, (std::vector<std::string>{"g1.js"}));
    EXPECT_EQ(by_name.at("l1").args, (std::vector<std::string>{"l1.js"}));

    EXPECT_EQ(c6_read_file(svc_global), svc_global_before);
    EXPECT_EQ(c6_read_file(svc_user),   svc_user_before);
    EXPECT_EQ(c6_read_file(svc_local),  svc_local_before);

    fs::remove_all(root);
}

// Group 6: default remove clears every physical copy and is idempotent;
// --scope touches one file (including the legacy global); unknown names
// yield an empty outcome; removing the last entry drops mcpServers but
// preserves sibling sections.
TEST(McpTypes, McpRemoveAllCopiesScopedAndNotFound) {
    const auto root = c13_make_temp_root("loom_mcp_c6_remove_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    // Default removal touches both polluted copies.
    c6_write_file(global_path, R"JSON({
  "mcpServers": {
    "dup": {"command": "node", "args": ["g.js"]},
    "gonly": {"command": "node", "args": ["go.js"]}
  }
})JSON");
    c6_write_file(project_path, R"JSON({
  "mcpServers": {
    "dup": {"command": "node", "args": ["p.js"]},
    "ponly": {"command": "node", "args": ["po.js"]}
  }
})JSON");
    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        auto outcome = manager.remove_mcp_server("dup");
        ASSERT_TRUE(outcome.has_value());
        EXPECT_EQ(outcome->touched.size(), 2u);
        EXPECT_TRUE(outcome->failed.empty());

        // Idempotent re-run: nothing present anymore.
        auto again = manager.remove_mcp_server("dup");
        ASSERT_TRUE(again.has_value());
        EXPECT_TRUE(again->touched.empty());
        EXPECT_TRUE(again->failed.empty());
    }
    {
        cc::core::ConfigManager reloaded(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(reloaded.load().has_value());
        std::set<std::string> names;
        for (const auto& server : reloaded.settings().mcp_servers) names.insert(server.name);
        EXPECT_EQ(names, (std::set<std::string>{"gonly", "ponly"}));
    }

    // Scoped removal touches only that tier.
    c6_write_file(global_path, R"JSON({"mcpServers": {"s": {"command": "node"}}})JSON");
    c6_write_file(project_path, R"JSON({"mcpServers": {"s": {"command": "node"}}})JSON");
    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        auto outcome = manager.remove_mcp_server(
            "s", cc::core::McpStorageScope::Project);
        ASSERT_TRUE(outcome.has_value());
        EXPECT_EQ(outcome->touched.size(), 1u);
        EXPECT_EQ(outcome->touched[0], project_path);
    }
    {
        cc::core::ConfigManager reloaded(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(reloaded.load().has_value());
        ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(reloaded.settings().mcp_servers[0].name, "s");  // survives in global
    }
    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        auto outcome = manager.remove_mcp_server(
            "s", cc::core::McpStorageScope::Global);
        ASSERT_TRUE(outcome.has_value());
        EXPECT_EQ(outcome->touched.size(), 1u);
        EXPECT_EQ(outcome->touched[0], global_path);
    }

    // Unknown names: empty outcome both unscoped and scoped.
    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        EXPECT_TRUE(manager.remove_mcp_server("ghost")->touched.empty());
        auto scoped = manager.remove_mcp_server("ghost", cc::core::McpStorageScope::Local);
        ASSERT_TRUE(scoped.has_value());
        EXPECT_TRUE(scoped->touched.empty());
    }

    // Removing the last entry drops mcpServers but keeps sibling sections.
    c6_write_file(global_path, R"JSON({
  "systemPrompt": "keep",
  "mcpServers": {"last": {"command": "node"}}
})JSON");
    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.remove_mcp_server(
            "last", cc::core::McpStorageScope::Global).has_value());
        auto parsed = cc::utils::json::parse_file(global_path);
        ASSERT_TRUE(parsed.has_value());
        EXPECT_FALSE(parsed->root().has("mcpServers"));
        EXPECT_EQ(parsed->root().get("systemPrompt").as_str(), std::string_view("keep"));
    }

    fs::remove_all(root);
}

// Group 7: enable/disable patch only "disabled" on the owner file; unknown
// sibling keys survive; lower tiers are untouched; the batch helper patches
// one file per distinct owner; a global-only entry patches the legacy file.
TEST(McpTypes, McpEnableDisablePatchesOwnerFilesAndGlobal) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = c13_make_temp_root("loom_mcp_c6_disable_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    c6_write_file(global_path, R"JSON({
  "mcpServers": {
    "goff": {"command": "node", "disabled": true},
    "gx": {"command": "node", "weird": 123}
  }
})JSON");
    c6_write_file(project_path, R"JSON({
  "mcpServers": {"pon": {"command": "node"}}
})JSON");
    c6_write_file(local_path, R"JSON({
  "mcpServers": {"lon": {"command": "node"}}
})JSON");

    const std::string project_before = c6_read_file(project_path);
    const std::string local_before = c6_read_file(local_path);

    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());

        // Highest-precedence owner is global; enabling flips only that file.
        ASSERT_TRUE(manager.set_mcp_server_disabled("goff", false).has_value());
        auto global_doc = cc::utils::json::parse_file(global_path);
        ASSERT_TRUE(global_doc.has_value());
        EXPECT_FALSE(global_doc->root().get("mcpServers").get("goff").get("disabled").as_bool());

        // Unknown sibling key survives a disable/enable round trip.
        ASSERT_TRUE(manager.set_mcp_server_disabled("gx", true).has_value());
        ASSERT_TRUE(manager.set_mcp_server_disabled("gx", false).has_value());
        global_doc = cc::utils::json::parse_file(global_path);
        ASSERT_TRUE(global_doc.has_value());
        const auto gx = global_doc->root().get("mcpServers").get("gx");
        EXPECT_EQ(gx.get("weird").as_int(), 123);
        EXPECT_FALSE(gx.get("disabled").as_bool());

        // Project owner patches project; global/local bytes untouched.
        ASSERT_TRUE(manager.set_mcp_server_disabled("pon", true).has_value());
        EXPECT_EQ(c6_read_file(global_path).find("\"pon\""), std::string::npos);
        EXPECT_EQ(c6_read_file(local_path), local_before);

        // Unknown name and wrong-scope name are errors.
        EXPECT_FALSE(manager.set_mcp_server_disabled("zzz", true).has_value());
        EXPECT_FALSE(manager.set_mcp_server_disabled(
            "pon", true, cc::core::McpStorageScope::Local).has_value());
    }
    EXPECT_NE(c6_read_file(project_path), project_before);

    // Global-only entry patches the legacy global file, creates nothing else.
    {
        const auto root2 = fs::temp_directory_path() /
            ("loom_mcp_c6_disable_global_only_" + std::to_string(suffix + 1));
        fs::create_directories(root2);
        const auto g2 = root2 / "global.json";
        const auto u2 = root2 / "user.json";
        const auto p2 = root2 / "project.json";
        const auto l2 = root2 / "project.local.json";
        c6_write_file(g2, R"JSON({"mcpServers": {"solo": {"command": "node"}}})JSON");
        cc::core::ConfigManager manager(g2, u2, p2, l2);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.set_mcp_server_disabled("solo", true).has_value());
        EXPECT_TRUE(cc::utils::json::parse_file(g2)->root()
                        .get("mcpServers").get("solo").get("disabled").as_bool());
        EXPECT_FALSE(fs::exists(p2));
        EXPECT_FALSE(fs::exists(l2));
        fs::remove_all(root2);
    }

    // "all" shape: one batch call per distinct owner file.
    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        const std::vector<std::string> global_names{"goff", "gx"};
        const std::vector<std::string> project_names{"pon"};
        const std::vector<std::string> local_names{"lon"};
        EXPECT_TRUE(manager.set_mcp_servers_disabled_in(
            cc::core::McpStorageScope::Global, global_names, true).has_value());
        EXPECT_TRUE(manager.set_mcp_servers_disabled_in(
            cc::core::McpStorageScope::Project, project_names, true).has_value());
        EXPECT_TRUE(manager.set_mcp_servers_disabled_in(
            cc::core::McpStorageScope::Local, local_names, true).has_value());
        for (const auto& [path, names] : {
                 std::pair{global_path, std::vector<std::string>{"goff", "gx"}},
                 std::pair{project_path, std::vector<std::string>{"pon"}},
                 std::pair{local_path, std::vector<std::string>{"lon"}}}) {
            auto doc = cc::utils::json::parse_file(path);
            ASSERT_TRUE(doc.has_value()) << path.string();
            for (const auto& name : names) {
                EXPECT_TRUE(doc->root().get("mcpServers").get(name)
                                .get("disabled").as_bool()) << path.string() << " " << name;
            }
        }
    }

    fs::remove_all(root);
}

// Group 9 (§A): key=value user content and a non-object local root make
// those tiers contribute zero entries with one diagnostic each, while the
// global/project tiers keep loading; upserts against an unparseable tier
// fail with an actionable message and leave bytes untouched.
TEST(McpTypes, McpGarbageUserLocalFilesSkippedAndUpsertRejected) {
    const auto root = c13_make_temp_root("loom_mcp_c6_garbage_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    c6_write_file(global_path, R"JSON({"mcpServers": {"g1": {"command": "node"}}})JSON");
    c6_write_file(user_path, "FOO=bar\nBAZ=qux\n");              // not JSON
    c6_write_file(local_path, "[1, 2]\n");                        // valid JSON, wrong root
    c6_write_file(project_path, R"JSON({"mcpServers": {"p1": {"command": "node"}}})JSON");

    const std::string user_before = c6_read_file(user_path);
    const std::string local_before = c6_read_file(local_path);

    cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
    ASSERT_TRUE(manager.load().has_value());
    std::set<std::string> names;
    for (const auto& server : manager.settings().mcp_servers) names.insert(server.name);
    EXPECT_EQ(names, (std::set<std::string>{"g1", "p1"}));

    cc::core::McpServerConfig cfg;
    cfg.name = "newu";
    cfg.transport = "stdio";
    cfg.command = "node";
    cfg.config_scope = "user";
    auto user_upsert = manager.upsert_mcp_server(cc::core::McpStorageScope::User, cfg);
    ASSERT_FALSE(user_upsert.has_value());
    EXPECT_NE(user_upsert.error().message.find(user_path.string()), std::string::npos);
    EXPECT_NE(user_upsert.error().message.find("not valid JSON"), std::string::npos);
    EXPECT_EQ(c6_read_file(user_path), user_before);

    cfg.name = "newl";
    auto local_upsert = manager.upsert_mcp_server(cc::core::McpStorageScope::Local, cfg);
    ASSERT_FALSE(local_upsert.has_value());
    EXPECT_EQ(c6_read_file(local_path), local_before);

    // Other tiers remain patchable.
    cfg.name = "newp";
    ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::Project, cfg)
                    .has_value());
    cc::core::ConfigManager reloaded(global_path, user_path, project_path, local_path);
    ASSERT_TRUE(reloaded.load().has_value());
    names.clear();
    for (const auto& server : reloaded.settings().mcp_servers) names.insert(server.name);
    EXPECT_EQ(names, (std::set<std::string>{"g1", "newp", "p1"}));

    fs::remove_all(root);
}

// Group 10: $LOOM_CONFIG_DIR routes the user tier; config_home_write() and
// the default ConfigManager constructor both honor it.
TEST(McpTypes, McpUserScopeHonorsConfigDirEnv) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_env_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto fake_home = root / "home";
    const auto config_dir = root / "cfgdir";
    const auto work = root / "work";
    fs::create_directories(fake_home);
    fs::create_directories(config_dir);
    fs::create_directories(work);

    EnvironmentGuard home_guard("HOME", fake_home.string());
    EnvironmentGuard dir_guard("LOOM_CONFIG_DIR", config_dir.string());
    CurrentPathGuard cwd_guard(work);

    EXPECT_EQ(cc::constants::paths::config_home_write(), config_dir);

    cc::core::McpServerConfig cfg;
    cfg.name = "u1";
    cfg.transport = "stdio";
    cfg.command = "node";
    cfg.config_scope = "user";

    {
        cc::core::ConfigManager manager;
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::User, cfg)
                        .has_value());
    }
    EXPECT_TRUE(fs::exists(config_dir / "config.json"));
    EXPECT_FALSE(fs::exists(fake_home / ".loom" / "config.json"));

    {
        cc::core::ConfigManager reloaded;
        ASSERT_TRUE(reloaded.load().has_value());
        ASSERT_EQ(reloaded.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(reloaded.settings().mcp_servers[0].name, "u1");
    }

    fs::remove_all(root);
}

// Group 11: unwritable files aggregate into the failed list without
// aborting writable tiers; all-unwritable-present is an empty touched list
// rather than NotFound. Skipped when running as root (perms are bypassed).
TEST(McpTypes, McpRemoveAggregatesUnwritableFiles) {
    if (::getuid() == 0) {
        GTEST_SKIP() << "read-only permissions are bypassed for root";
    }
    const auto root = c13_make_temp_root("loom_mcp_c6_unwritable_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);

    {
        const auto ro = root / "ro";
        fs::create_directories(ro);
        const auto global_path  = ro / "global.json";
        const auto project_path = root / "project.json";
        c6_write_file(global_path, R"JSON({"mcpServers": {"dup": {"command": "node"}}})JSON");
        c6_write_file(project_path, R"JSON({"mcpServers": {"dup": {"command": "node"}}})JSON");

        fs::permissions(ro, fs::perms::owner_read | fs::perms::owner_exec,
                        fs::perm_options::replace);
        cc::core::ConfigManager manager(global_path, project_path);
        ASSERT_TRUE(manager.load().has_value());
        auto outcome = manager.remove_mcp_server("dup");
        ASSERT_TRUE(outcome.has_value());
        EXPECT_EQ(outcome->touched.size(), 1u);
        EXPECT_EQ(outcome->touched[0], project_path);
        EXPECT_EQ(outcome->failed.size(), 1u);
        EXPECT_EQ(outcome->failed[0], global_path);

        fs::permissions(ro, fs::perms::owner_all, fs::perm_options::replace);
    }

    {
        // Both files present in one read-only directory: present, zero writes.
        const auto ro2 = root / "ro2";
        fs::create_directories(ro2);
        const auto global_path  = ro2 / "global.json";
        const auto project_path = ro2 / "project.json";
        c6_write_file(global_path, R"JSON({"mcpServers": {"dup": {"command": "node"}}})JSON");
        c6_write_file(project_path, R"JSON({"mcpServers": {"dup": {"command": "node"}}})JSON");

        fs::permissions(ro2, fs::perms::owner_read | fs::perms::owner_exec,
                        fs::perm_options::replace);
        cc::core::ConfigManager manager(global_path, project_path);
        ASSERT_TRUE(manager.load().has_value());
        auto outcome = manager.remove_mcp_server("dup");
        ASSERT_TRUE(outcome.has_value());
        EXPECT_TRUE(outcome->touched.empty());
        EXPECT_EQ(outcome->failed.size(), 2u);

        fs::permissions(ro2, fs::perms::owner_all, fs::perm_options::replace);
    }

    fs::remove_all(root);
}

// Group 12 (§B secret boundary): a full save to the PROJECT file omits
// user/local-only entries (which may carry Authorization headers) and
// re-emits the PROJECT FILE'S OWN value for physically-present names, never
// the shadowing higher-tier value; global-owned entries still copy down.
TEST(McpTypes, McpProjectSaveDoesNotLeakUserLocalSecrets) {
    const auto root = c13_make_temp_root("loom_mcp_c6_secret_boundary_");
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    c6_write_file(global_path, R"JSON({
  "mcpServers": {"g1": {"command": "node", "args": ["g.js"]}}
})JSON");
    c6_write_file(project_path, R"JSON({
  "mcpServers": {
    "p1": {"command": "node", "args": ["p.js"]},
    "shared": {
      "type": "http",
      "url": "https://mcp.example.com/mcp",
      "headers": {"X-Public": "1"}
    }
  }
})JSON");
    c6_write_file(user_path, R"JSON({
  "mcpServers": {
    "secret-srv": {
      "type": "http",
      "url": "https://secret.example.com/mcp",
      "headers": {"Authorization": "Bearer user-secret"}
    },
    "shared": {
      "type": "http",
      "url": "https://mcp.example.com/mcp",
      "headers": {"Authorization": "Bearer user-shadow-secret"}
    }
  }
})JSON");
    c6_write_file(local_path, R"JSON({
  "mcpServers": {
    "localsecret": {
      "type": "http",
      "url": "https://local.example.com/mcp",
      "headers": {"Authorization": "Bearer local-secret"}
    }
  }
})JSON");

    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        // The overlay carries the secret-bearing user/local entries.
        ASSERT_TRUE(manager.save(cc::core::ConfigSource::ProjectConfig).has_value());
    }

    const std::string rewritten = c6_read_file(project_path);
    auto doc = cc::utils::json::parse(rewritten);
    ASSERT_TRUE(doc.has_value());
    const auto servers = doc->root().get("mcpServers");
    ASSERT_TRUE(servers.is_obj());
    const auto keys = c6_json_keys(servers);
    EXPECT_EQ(std::set<std::string>(keys.begin(), keys.end()),
              (std::set<std::string>{"g1", "p1", "shared"}));
    // No user/local names and no secret material leak into the tracked file.
    EXPECT_EQ(rewritten.find("secret-srv"), std::string::npos);
    EXPECT_EQ(rewritten.find("localsecret"), std::string::npos);
    EXPECT_EQ(rewritten.find("Bearer"), std::string::npos);
    EXPECT_EQ(rewritten.find("Authorization"), std::string::npos);
    // The physically-present shared entry keeps the PROJECT file's own value.
    const auto shared = servers.get("shared");
    ASSERT_TRUE(shared.is_obj());
    EXPECT_EQ(shared.get("headers").get("X-Public").as_str(), std::string_view("1"));
    EXPECT_FALSE(shared.get("headers").has("Authorization"));
    EXPECT_EQ(servers.get("p1").get("command").as_str(), std::string_view("node"));
    EXPECT_EQ(servers.get("g1").get("command").as_str(), std::string_view("node"));

    fs::remove_all(root);
}

// B1 (review followup): mutators must keep in-memory bookkeeping and the
// merged vector consistent with disk, so a full save on the SAME
// ConfigManager instance (no reload) cannot resurrect a removed entry or
// misapply the §B filter, and an upsert stays coherent through a save.
TEST(McpTypes, McpMutationsStayCoherentForSameInstanceSave) {
    const auto root = c13_make_temp_root("loom_mcp_c6_b1_");
    // Pin CWD to the git-ancestry-clean temp root so the walk-up
    // gitignore appender never reaches a real work tree (e.g. /tmp/.git).
    CurrentPathGuard cwd_guard(root);
    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    c6_write_file(global_path, R"JSON({
  "mcpServers": {"g1": {"command": "node", "args": ["g.js"]}}
})JSON");
    c6_write_file(project_path, R"JSON({
  "mcpServers": {
    "p1": {"command": "node", "args": ["p.js"]},
    "p2": {"command": "node", "args": ["p2.js"]}
  }
})JSON");

    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_EQ(manager.settings().mcp_servers.size(), 3u);

        // Remove a PROJECT entry and immediately full-save on the SAME
        // instance (the old bug rewrote the stale merged entry back).
        auto removed = manager.remove_mcp_server(
            "p1", cc::core::McpStorageScope::Project);
        ASSERT_TRUE(removed.has_value());
        EXPECT_EQ(removed->touched.size(), 1u);
        EXPECT_TRUE(std::ranges::none_of(manager.settings().mcp_servers,
            [](const auto& s) { return s.name == "p1"; }));
        ASSERT_TRUE(manager.save(cc::core::ConfigSource::ProjectConfig).has_value());

        const std::string text = c6_read_file(project_path);
        EXPECT_EQ(text.find("p1"), std::string::npos);
        auto reparsed = cc::utils::json::parse(text);
        ASSERT_TRUE(reparsed.has_value());
        const auto servers = reparsed->root().get("mcpServers");
        EXPECT_FALSE(servers.has("p1"));
        EXPECT_TRUE(servers.has("p2"));
        EXPECT_TRUE(servers.has("g1"));  // global copy-down preserved
    }

    // A FRESH instance sees the same state.
    {
        cc::core::ConfigManager reloaded(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(reloaded.load().has_value());
        std::set<std::string> names;
        for (const auto& s : reloaded.settings().mcp_servers) names.insert(s.name);
        EXPECT_EQ(names, (std::set<std::string>{"g1", "p2"}));
    }

    // Upsert -> same-instance save coherence: new local entry must not be
    // copied down into the project file by the subsequent save (§B uses the
    // updated bookkeeping), and a project upsert round-trips.
    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());

        cc::core::McpServerConfig local_cfg;
        local_cfg.name = "l1";
        local_cfg.transport = "stdio";
        local_cfg.command = "node";
        local_cfg.args = {"l.js"};
        local_cfg.config_scope = "local";
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::Local, local_cfg)
                        .has_value());
        ASSERT_EQ(manager.settings().mcp_servers.size(), 3u);  // g1, p2, l1
        ASSERT_TRUE(manager.mcp_server_owner("l1").has_value());
        EXPECT_EQ(*manager.mcp_server_owner("l1"), cc::core::McpStorageScope::Local);

        cc::core::McpServerConfig project_cfg;
        project_cfg.name = "p3";
        project_cfg.transport = "stdio";
        project_cfg.command = "node";
        project_cfg.args = {"p3.js"};
        project_cfg.config_scope = "project";
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::Project, project_cfg)
                        .has_value());

        ASSERT_TRUE(manager.save(cc::core::ConfigSource::ProjectConfig).has_value());
        const std::string text = c6_read_file(project_path);
        EXPECT_EQ(text.find("l1"), std::string::npos);  // not copied down
        auto reparsed = cc::utils::json::parse(text);
        ASSERT_TRUE(reparsed.has_value());
        const auto servers = reparsed->root().get("mcpServers");
        EXPECT_TRUE(servers.has("p2"));
        EXPECT_TRUE(servers.has("p3"));

        // A lower-tier upsert while a higher tier shadows the name keeps the
        // higher value effective and the higher owner.
        cc::core::McpServerConfig shadow;
        shadow.name = "l1";
        shadow.transport = "stdio";
        shadow.command = "node";
        shadow.args = {"project-shadow.js"};
        shadow.config_scope = "project";
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::Project, shadow)
                        .has_value());
        auto effective = std::ranges::find(manager.settings().mcp_servers, "l1",
                                          [](const auto& s) { return s.name; });
        ASSERT_NE(effective, manager.settings().mcp_servers.end());
        EXPECT_EQ(effective->args, (std::vector<std::string>{"l.js"}));  // local wins
        EXPECT_EQ(*manager.mcp_server_owner("l1"), cc::core::McpStorageScope::Local);
    }

    fs::remove_all(root);
}

// §A followup: a zero-length or all-whitespace user/local file behaves like
// a MISSING file — no warning state, zero entries, and an upsert creates it
// fresh — while genuine garbage keeps the rejection policy.
TEST(McpTypes, McpBlankUserLocalFilesTreatedAsMissing) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_blank_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    // --- §A asymmetry: global/project stay HARD failures even when blank or
    // garbage; only user/local get the soft policy. -----------------------
    { std::ofstream(global_path) << ""; }
    {
        cc::core::ConfigManager m(global_path, user_path, project_path, local_path);
        EXPECT_FALSE(m.load().has_value());
    }
    { std::ofstream(global_path) << "FOO=bar\n"; }  // non-JSON garbage
    {
        cc::core::ConfigManager m(global_path, user_path, project_path, local_path);
        EXPECT_FALSE(m.load().has_value());
    }

    fs::remove(global_path);
    { std::ofstream(project_path) << "   \n"; }  // blank project file
    {
        cc::core::ConfigManager m(global_path, user_path, project_path, local_path);
        EXPECT_FALSE(m.load().has_value());
    }
    c6_write_file(global_path, R"JSON({"mcpServers": {"g1": {"command": "node"}}})JSON");
    { std::ofstream(project_path) << "[1, 2]\n"; }  // valid JSON, wrong root
    {
        cc::core::ConfigManager m(global_path, user_path, project_path, local_path);
        EXPECT_FALSE(m.load().has_value());
    }

    // --- Blank user/local: tolerated like a missing file, NO warning, and a
    // subsequent upsert creates the file fresh. ---------------------------
    fs::remove(project_path);
    { std::ofstream(user_path) << ""; }
    { std::ofstream(local_path) << "  \n\t \n"; }
    {
        testing::internal::CaptureStderr();
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        const std::string warnings = testing::internal::GetCapturedStderr();
        EXPECT_EQ(warnings.find("not valid JSON"), std::string::npos);
        ASSERT_EQ(manager.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(manager.settings().mcp_servers[0].name, "g1");

        cc::core::McpServerConfig cfg;
        cfg.name = "u1";
        cfg.transport = "stdio";
        cfg.command = "node";
        cfg.config_scope = "user";
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::User, cfg).has_value());
        auto user_doc = cc::utils::json::parse_file(user_path);
        ASSERT_TRUE(user_doc.has_value());
        EXPECT_TRUE(user_doc->root().get("mcpServers").has("u1"));
    }

    // --- Genuine garbage in user/local is DISTINCT from blank: the tier is
    // skipped but a warning names each bad path, and lower tiers load. ----
    fs::remove(user_path);
    fs::remove(local_path);
    { std::ofstream(user_path) << "FOO=bar\nBAZ=qux\n"; }
    { std::ofstream(local_path) << "[1, 2]\n"; }
    {
        testing::internal::CaptureStderr();
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        const std::string warnings = testing::internal::GetCapturedStderr();
        EXPECT_NE(warnings.find(user_path.string()), std::string::npos);
        EXPECT_NE(warnings.find(local_path.string()), std::string::npos);
        EXPECT_EQ(c6_count_occurrences(warnings, "not valid JSON"), 2u);
        ASSERT_EQ(manager.settings().mcp_servers.size(), 1u);
        EXPECT_EQ(manager.settings().mcp_servers[0].name, "g1");
    }

    fs::remove_all(root);
}

// Mode/gitignore follow-up: tmp+rename must preserve a pre-existing
// non-local file's mode (pre-C6 in-place ofstream kept the inode mode),
// while every LOCAL patch forces 0600 and gitignores the local basename
// even when the local file pre-existed non-empty.
TEST(McpTypes, McpPatchPreservesFileModesAndProtectsPreExistingLocal) {
    if (::getuid() == 0) {
        GTEST_SKIP() << "file mode restrictions are bypassed for root";
    }
    if (std::system("git --version >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "git not available";
    }
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_c6_modes_" + std::to_string(suffix));
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);
    // c13e: gitignore appends require a work tree.
    ASSERT_EQ(std::system("git init -q --initial-branch main"), 0);
    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / ".loom" / "config.json";
    const auto local_path   = root / ".loom" / "config.local.json";

    auto make_cfg = [](std::string name) {
        cc::core::McpServerConfig cfg;
        cfg.name = std::move(name);
        cfg.transport = "stdio";
        cfg.command = "node";
        cfg.args = {"a.js"};
        return cfg;
    };
    auto mode_of = [](const fs::path& path) {
        std::error_code ec;
        return fs::status(path, ec).permissions() & fs::perms::mask;
    };

    // Pre-existing 0600 project file keeps 0600 across upsert + disable.
    c6_write_file(project_path, R"JSON({
  "mcpServers": {"p1": {"command": "node"}}
})JSON");
    fs::permissions(project_path, fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::Project,
                                              make_cfg("p2")).has_value());
        EXPECT_EQ(mode_of(project_path),
                  fs::perms::owner_read | fs::perms::owner_write);
        ASSERT_TRUE(manager.set_mcp_server_disabled(
            "p1", true, cc::core::McpStorageScope::Project).has_value());
        EXPECT_EQ(mode_of(project_path),
                  fs::perms::owner_read | fs::perms::owner_write);
    }

    // Pre-existing 0640 user file keeps 0640 across an upsert.
    c6_write_file(user_path, R"JSON({
  "mcpServers": {"u1": {"command": "node"}}
})JSON");
    fs::permissions(user_path,
                    fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read,
                    fs::perm_options::replace);
    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::User,
                                              make_cfg("u2")).has_value());
        EXPECT_EQ(mode_of(user_path),
                  fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read);
    }

    // A hand-created, non-empty LOCAL file with a server entry is still
    // gitignored on the next patch, and forced to 0600.
    c6_write_file(local_path, R"JSON({
  "mcpServers": {"l1": {"command": "node"}}
})JSON");
    fs::permissions(local_path,
                    fs::perms::owner_read | fs::perms::owner_write | fs::perms::others_read,
                    fs::perm_options::replace);
    // c13d: the earlier PROJECT-tier writes already ignore the lock
    // sibling, but the local data basename is not ignored yet.
    if (fs::exists(root / ".gitignore")) {
        const auto prior = c6_read_file(root / ".gitignore");
        EXPECT_EQ(prior.find("config.local.json"), std::string::npos);
        EXPECT_NE(prior.find("config.json.lock"), std::string::npos);
    }
    {
        cc::core::ConfigManager manager(global_path, user_path, project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        ASSERT_TRUE(manager.upsert_mcp_server(cc::core::McpStorageScope::Local,
                                              make_cfg("l2")).has_value());
    }
    ASSERT_TRUE(fs::exists(root / ".gitignore"));
    EXPECT_NE(c6_read_file(root / ".gitignore").find("config.local.json"),
              std::string::npos);
    EXPECT_NE(c6_read_file(root / ".gitignore").find("config.local.json.lock"),
              std::string::npos);
    EXPECT_EQ(mode_of(local_path),
              fs::perms::owner_read | fs::perms::owner_write);
    // Both entries survive (pre-existing local content is not overwritten).
    auto local_doc = cc::utils::json::parse_file(local_path);
    ASSERT_TRUE(local_doc.has_value());
    EXPECT_TRUE(local_doc->root().get("mcpServers").has("l1"));
    EXPECT_TRUE(local_doc->root().get("mcpServers").has("l2"));

    fs::remove_all(root);
}

TEST(ServerRoutes, MessageSessionsAndCompactUsePersistentState) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_server_routes_test_" + std::to_string(suffix));
    const auto sessions_dir = root / "sessions";
    fs::remove_all(root);
    fs::create_directories(sessions_dir);

    LocalAnthropicMessagesServer server;
    ASSERT_NE(server.port(), 0);
    EnvironmentGuard api_key_guard("ANTHROPIC_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("ANTHROPIC_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "direct-route-test-model");
    CurrentPathGuard cwd_guard(root);

    cc::server::reset_route_state_for_testing();
    cc::server::set_sessions_dir_for_testing(sessions_dir);
    auto routes = cc::server::get_default_routes();
    auto find_route = [&](std::string_view method, std::string_view path) -> const cc::server::Route* {
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
    auto first_json = cc::utils::json::parse(first_response);
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
        auto parsed = cc::utils::json::parse(response);
        ASSERT_TRUE(parsed.has_value()) << response;
        EXPECT_EQ(parsed->root().get_string("session_id"), session_id);
        EXPECT_EQ(parsed->root().get_string("response"), "ok");
    }

    auto request_bodies = server.wait_for_bodies(5);
    ASSERT_TRUE(request_bodies.has_value());
    auto last_request_json = cc::utils::json::parse(request_bodies->back());
    ASSERT_TRUE(last_request_json.has_value()) << request_bodies->back();
    auto request_messages = last_request_json->root().get("messages");
    ASSERT_TRUE(request_messages.is_arr()) << request_bodies->back();
    ASSERT_EQ(request_messages.size(), 9u) << request_bodies->back();
    std::string request_history_text;
    request_messages.iter([&](cc::utils::json::JsonVal message) {
        auto content = message.get("content");
        if (content.is_str()) {
            request_history_text += std::string(content.as_str()) + "\n";
        } else if (content.is_arr()) {
            content.iter([&](cc::utils::json::JsonVal block) {
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
    auto sessions_json = cc::utils::json::parse(sessions_response);
    ASSERT_TRUE(sessions_json.has_value()) << sessions_response;
    EXPECT_EQ(sessions_json->root().get("total").as_int(), 1);
    auto sessions = sessions_json->root().get("sessions");
    ASSERT_TRUE(sessions.is_arr());
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.at(0).get_string("session_id"), session_id);
    EXPECT_EQ(sessions.at(0).get("message_count").as_int(), 10);

    auto compact_response = compact_route->handler({{"session_id", session_id}});
    auto compact_json = cc::utils::json::parse(compact_response);
    ASSERT_TRUE(compact_json.has_value()) << compact_response;
    EXPECT_EQ(compact_json->root().get_string("status"), "compacted");
    EXPECT_EQ(compact_json->root().get("messages_before").as_int(), 10);
    EXPECT_EQ(compact_json->root().get("messages_after").as_int(), 7);
    EXPECT_EQ(compact_json->root().get("messages_removed").as_int(), 3);
    EXPECT_EQ(compact_json->root().get("messages_summarized").as_int(), 4);
    ASSERT_TRUE(compact_json->root().get("compact_boundary_id").is_str());

    auto metadata = cc::session::load_session_metadata(sessions_dir, session_id);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->message_count, 7);

    std::ifstream messages_file(cc::session::get_messages_path(sessions_dir, session_id));
    ASSERT_TRUE(messages_file.is_open());
    std::vector<std::string> compacted_lines;
    std::string line;
    while (std::getline(messages_file, line)) {
        if (!line.empty()) compacted_lines.push_back(line);
    }
    ASSERT_EQ(compacted_lines.size(), 7u);
    auto boundary_json = cc::utils::json::parse(compacted_lines.front());
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

	cc::server::reset_route_state_for_testing();
	fs::remove_all(root);
}

TEST(ServerRoutes, MessageRoutePublishesAssistantAndResultIngressEvents) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_server_routes_ingress_test_" + std::to_string(suffix));
    const auto sessions_dir = root / "sessions";
    fs::remove_all(root);
    fs::create_directories(sessions_dir);

    LocalAnthropicMessagesServer anthropic;
    ASSERT_NE(anthropic.port(), 0);
    LocalCcrHttpServer ccr;
    ASSERT_TRUE(ccr.ready());

    EnvironmentGuard api_key_guard("ANTHROPIC_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("ANTHROPIC_BASE_URL", anthropic.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "direct-ingress-test-model");
    CurrentPathGuard cwd_guard(root);

    const auto now = std::chrono::system_clock::now();
    ASSERT_TRUE(cc::session::save_session_metadata(
        sessions_dir,
        cc::session::SessionMetadata{
            .session_id = "session_1",
            .model = "direct-ingress-test-model",
            .cwd = root,
            .created_at = now,
            .last_active = now,
            .message_count = 0,
            .title = std::string("Ingress route test"),
            .is_archived = false,
        }));

    cc::services::api::close_ingress();
    auto created = cc::services::api::create_ingress(cc::services::api::IngressConfig{
        .endpoint = ccr.base_url(),
        .session_id = "session_1",
        .auth_token = "session-route-token",
        .organization_uuid = std::nullopt,
    });
    ASSERT_TRUE(created.has_value()) << created.error();

    cc::server::reset_route_state_for_testing();
    cc::server::set_sessions_dir_for_testing(sessions_dir);
    auto routes = cc::server::get_default_routes();
    auto it = std::ranges::find_if(routes, [](const auto& route) {
        return route.method == "POST" && route.path == "/message";
    });
    ASSERT_NE(it, routes.end());

    auto response = it->handler({
        {"session_id", "session_1"},
        {"content", "hello ingress route"}
    });
    auto parsed = cc::utils::json::parse(response);
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

    auto metadata = cc::session::load_session_metadata(sessions_dir, "session_1");
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->message_count, 2);

    std::ifstream messages_file(cc::session::get_messages_path(sessions_dir, "session_1"));
    ASSERT_TRUE(messages_file.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(messages_file, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_NE(lines[0].find(R"("role":"user")"), std::string::npos);
    EXPECT_NE(lines[1].find(R"("role":"assistant")"), std::string::npos);

    cc::services::api::close_ingress();
    cc::server::reset_route_state_for_testing();
    fs::remove_all(root);
}

TEST(ServerMain, DirectConnectSessionsAndWebSocketUsePersistentRoutes) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_server_main_test_" + std::to_string(suffix));
    const auto sessions_dir = root / "sessions";
    fs::remove_all(root);
    fs::create_directories(sessions_dir);

    LocalAnthropicMessagesServer anthropic;
    ASSERT_NE(anthropic.port(), 0);
    EnvironmentGuard api_key_guard("ANTHROPIC_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("ANTHROPIC_BASE_URL", anthropic.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "direct-server-test-model");
    EnvironmentGuard sessions_guard("LOOM_SERVER_SESSIONS_DIR", sessions_dir.string());
    CurrentPathGuard cwd_guard(root);
    cc::server::reset_route_state_for_testing();

    cc::server::HttpServer direct_server;
    auto started = direct_server.start(cc::server::ServerConfig{
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
    auto create_json = cc::utils::json::parse(create_response->body);
    ASSERT_TRUE(create_json.has_value()) << create_response->body;
    const auto session_id = std::string(create_json->root().get_string("session_id"));
    ASSERT_FALSE(session_id.empty());
    EXPECT_NE(
        std::string(create_json->root().get_string("ws_url")).find("/sessions/ws/" + session_id),
        std::string::npos);
    EXPECT_EQ(
        std::string(create_json->root().get_string("work_dir")),
        fs::weakly_canonical(root).string());

    auto initial_metadata = cc::session::load_session_metadata(sessions_dir, session_id);
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
        auto assistant_json = cc::utils::json::parse(direct_connect_trim_json_line(assistant_frame->payload));
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
        auto result_json = cc::utils::json::parse(direct_connect_trim_json_line(result_frame->payload));
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
    auto control_json = cc::utils::json::parse(direct_connect_trim_json_line(control_frame->payload));
    ASSERT_TRUE(control_json.has_value()) << control_frame->payload;
    EXPECT_EQ(control_json->root().get_string("type"), "control_response");
    auto control_response = control_json->root().get("response");
    EXPECT_EQ(control_response.get_string("subtype"), "success");
    EXPECT_EQ(control_response.get_string("request_id"), "interrupt-test");

    ::shutdown(*ws_fd, SHUT_RDWR);
    ::close(*ws_fd);

    auto request_bodies = anthropic.wait_for_bodies(prompts.size());
    ASSERT_TRUE(request_bodies.has_value());
    auto request_json = cc::utils::json::parse(request_bodies->back());
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
    auto sessions_json = cc::utils::json::parse(sessions_response->body);
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
    auto compact_json = cc::utils::json::parse(compact_response->body);
    ASSERT_TRUE(compact_json.has_value()) << compact_response->body;
    EXPECT_EQ(compact_json->root().get_string("status"), "compacted");
    EXPECT_EQ(compact_json->root().get_string("session_id"), session_id);
    EXPECT_EQ(compact_json->root().get("messages_before").as_int(), 10);
    EXPECT_EQ(compact_json->root().get("messages_after").as_int(), 7);
    EXPECT_EQ(compact_json->root().get("messages_removed").as_int(), 3);
    EXPECT_EQ(compact_json->root().get("messages_summarized").as_int(), 4);
    ASSERT_TRUE(compact_json->root().get("compact_boundary_id").is_str());

    auto metadata = cc::session::load_session_metadata(sessions_dir, session_id);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->message_count, 7);

    std::ifstream messages_file(cc::session::get_messages_path(sessions_dir, session_id));
    ASSERT_TRUE(messages_file.is_open());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(messages_file, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    ASSERT_EQ(lines.size(), 7u);
    auto boundary_json = cc::utils::json::parse(lines.front());
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
	cc::server::reset_route_state_for_testing();
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
    cc::server::reset_route_state_for_testing();

    std::mutex executor_mutex;
    std::condition_variable executor_cv;
    bool executor_started = false;
    std::atomic<bool> executor_saw_cancel{false};
    cc::server::set_query_executor_for_testing(
        [&](const cc::server::detail::DirectQueryRequest& request)
            -> std::expected<cc::server::detail::DirectQueryResult, std::string> {
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
            return cc::server::detail::DirectQueryResult{
                .assistant_id = "msg_not_cancelled",
                .content = "not cancelled",
                .model = "test-model",
                .input_tokens = 1,
                .output_tokens = 1,
                .tool_rounds = 0,
                .elapsed_ms = 3000,
            };
        });

    cc::server::HttpServer direct_server;
    auto started = direct_server.start(cc::server::ServerConfig{
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
    auto create_json = cc::utils::json::parse(create_response->body);
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
    auto control_json = cc::utils::json::parse(direct_connect_trim_json_line(control_frame->payload));
    ASSERT_TRUE(control_json.has_value()) << control_frame->payload;
    EXPECT_EQ(control_json->root().get_string("type"), "control_response");
    auto control_response = control_json->root().get("response");
    EXPECT_EQ(control_response.get_string("subtype"), "success");
    EXPECT_EQ(control_response.get_string("request_id"), "active-interrupt");
    EXPECT_TRUE(control_response.get("response").get("interrupted").as_bool());

    auto error_frame = direct_connect_read_ws_frame(*ws_fd);
    ASSERT_TRUE(error_frame.has_value());
    auto error_json = cc::utils::json::parse(direct_connect_trim_json_line(error_frame->payload));
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
	cc::server::reset_route_state_for_testing();
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

    LocalAnthropicMessagesServer anthropic({
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
    ASSERT_NE(anthropic.port(), 0);

    EnvironmentGuard api_key_guard("ANTHROPIC_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("ANTHROPIC_BASE_URL", anthropic.base_url());
    EnvironmentGuard sessions_guard("LOOM_SERVER_SESSIONS_DIR", sessions_dir.string());
    CurrentPathGuard cwd_guard(root);
    cc::server::reset_route_state_for_testing();

    cc::server::HttpServer direct_server;
    auto started = direct_server.start(cc::server::ServerConfig{
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
    auto create_json = cc::utils::json::parse(create_response->body);
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

        std::optional<cc::utils::json::JsonDoc> permission_json;
        for (int attempt = 0; attempt < 4; ++attempt) {
            auto permission_frame = direct_connect_read_ws_frame(*ws_fd);
            ASSERT_TRUE(permission_frame.has_value());
            auto parsed = cc::utils::json::parse(direct_connect_trim_json_line(permission_frame->payload));
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
        auto assistant_json = cc::utils::json::parse(direct_connect_trim_json_line(assistant_frame->payload));
        ASSERT_TRUE(assistant_json.has_value()) << assistant_frame->payload;
        EXPECT_EQ(assistant_json->root().get_string("type"), "assistant");
        auto assistant_content = assistant_json->root().get("message").get("content");
        ASSERT_TRUE(assistant_content.is_arr()) << assistant_frame->payload;
        ASSERT_GE(assistant_content.size(), 1u);
        EXPECT_EQ(assistant_content.at(0).get_string("text"), expected_text);

        auto result_frame = direct_connect_read_ws_frame(*ws_fd);
        ASSERT_TRUE(result_frame.has_value());
        auto result_json = cc::utils::json::parse(direct_connect_trim_json_line(result_frame->payload));
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
        auto assistant_json = cc::utils::json::parse(direct_connect_trim_json_line(assistant_frame->payload));
        ASSERT_TRUE(assistant_json.has_value()) << assistant_frame->payload;
        EXPECT_EQ(assistant_json->root().get_string("type"), "assistant");
        auto assistant_content = assistant_json->root().get("message").get("content");
        ASSERT_TRUE(assistant_content.is_arr()) << assistant_frame->payload;
        ASSERT_GE(assistant_content.size(), 1u);
        EXPECT_EQ(assistant_content.at(0).get_string("text"), expected_text);

        auto result_frame = direct_connect_read_ws_frame(*ws_fd);
        ASSERT_TRUE(result_frame.has_value());
        auto result_json = cc::utils::json::parse(direct_connect_trim_json_line(result_frame->payload));
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

    auto request_bodies = anthropic.wait_for_bodies(12);
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
    cc::server::reset_route_state_for_testing();
    fs::remove_all(root);
}

TEST(ServerMain, DirectConnectToolLoopPersistsTeamCreateAndSendMessage) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_direct_team_tool_test_" + std::to_string(suffix));
    const auto sessions_dir = root / "sessions";
    fs::remove_all(root);
    fs::create_directories(root);

    LocalAnthropicMessagesServer anthropic({
        R"({"id":"msg_team_create_tool","type":"message","role":"assistant","model":"loom-test","content":[{"type":"tool_use","id":"toolu_team_create","name":"team_create","input":{"team_id":"direct-team-id","team_name":"Direct Team","members":[{"agent_id":"reviewer-one","role":"reviewer"},{"agent_id":"researcher-one","role":"worker"}],"task_list":[{"id":"direct-task","description":"Inspect direct connect team migration","assigned_to":"reviewer-one"}]}}],"stop_reason":"tool_use","usage":{"input_tokens":1,"output_tokens":1}})",
        R"({"id":"msg_team_create_done","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"direct team created"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})",
        R"({"id":"msg_send_message_tool","type":"message","role":"assistant","model":"loom-test","content":[{"type":"tool_use","id":"toolu_send_message","name":"send_message","input":{"target_agent":"reviewer-one","team_name":"Direct Team","content":"Please review direct connect team output","summary":"direct team follow-up"}}],"stop_reason":"tool_use","usage":{"input_tokens":1,"output_tokens":1}})",
        R"({"id":"msg_send_message_done","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"direct team message delivered"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})",
    });
    ASSERT_NE(anthropic.port(), 0);

    EnvironmentGuard api_key_guard("ANTHROPIC_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("ANTHROPIC_BASE_URL", anthropic.base_url());
    EnvironmentGuard sessions_guard("LOOM_SERVER_SESSIONS_DIR", sessions_dir.string());
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    CurrentPathGuard cwd_guard(root);
    cc::server::reset_route_state_for_testing();
    cc::tools::global_team_store().clear_for_testing();
    cc::tools::agent_runtime::native_agent_store().clear_for_testing();

    cc::server::HttpServer direct_server;
    auto started = direct_server.start(cc::server::ServerConfig{
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
    auto create_json = cc::utils::json::parse(create_response->body);
    ASSERT_TRUE(create_json.has_value()) << create_response->body;
    const auto session_id = std::string(create_json->root().get_string("session_id"));
    ASSERT_FALSE(session_id.empty());

    auto ws_fd = direct_connect_open_websocket(server_port, "/sessions/ws/" + session_id);
    ASSERT_TRUE(ws_fd.has_value());

    auto read_non_permission_frame = [&]() -> std::optional<DirectConnectWsFrame> {
        while (true) {
            auto frame = direct_connect_read_ws_frame(*ws_fd);
            if (!frame) return std::nullopt;
            auto parsed = cc::utils::json::parse(direct_connect_trim_json_line(frame->payload));
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
        auto assistant_json = cc::utils::json::parse(direct_connect_trim_json_line(assistant_frame->payload));
        ASSERT_TRUE(assistant_json.has_value()) << assistant_frame->payload;
        EXPECT_EQ(assistant_json->root().get_string("type"), "assistant");
        EXPECT_EQ(assistant_json->root().get_string("session_id"), session_id);
        auto assistant_content = assistant_json->root().get("message").get("content");
        ASSERT_TRUE(assistant_content.is_arr()) << assistant_frame->payload;
        ASSERT_GE(assistant_content.size(), 1u);
        EXPECT_EQ(assistant_content.at(0).get_string("text"), expected_text);

        auto result_frame = read_non_permission_frame();
        ASSERT_TRUE(result_frame.has_value());
        auto result_json = cc::utils::json::parse(direct_connect_trim_json_line(result_frame->payload));
        ASSERT_TRUE(result_json.has_value()) << result_frame->payload;
        EXPECT_EQ(result_json->root().get_string("type"), "result");
        EXPECT_EQ(result_json->root().get_string("subtype"), "success");
        EXPECT_EQ(result_json->root().get_string("result"), expected_text);
    };

    send_user_prompt("create a direct connect team", "direct team created");
    send_user_prompt("message reviewer-one on the direct team", "direct team message delivered");

    ::shutdown(*ws_fd, SHUT_RDWR);
    ::close(*ws_fd);

    auto request_bodies = anthropic.wait_for_bodies(4);
    ASSERT_TRUE(request_bodies.has_value());
    ASSERT_EQ(request_bodies->size(), 4u);
    auto first_request = cc::utils::json::parse(request_bodies->front());
    ASSERT_TRUE(first_request.has_value()) << request_bodies->front();
    auto tools = first_request->root().get("tools");
    ASSERT_TRUE(tools.is_arr()) << request_bodies->front();
    bool exposed_team_create = false;
    bool exposed_send_message = false;
    tools.iter([&](cc::utils::json::JsonVal tool) {
        const auto name = std::string(tool.get_string("name"));
        if (name == "team_create") exposed_team_create = true;
        if (name == "send_message") exposed_send_message = true;
    });
    EXPECT_TRUE(exposed_team_create);
    EXPECT_TRUE(exposed_send_message);

    auto team = cc::tools::global_team_store().get("direct-team-id");
    ASSERT_TRUE(team.has_value()) << std::string(cc::tools::format_error(team.error()));
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

    auto reviewer_record = cc::tools::agent_runtime::native_agent_store().get("reviewer-one");
    ASSERT_TRUE(reviewer_record.has_value());
    ASSERT_TRUE(reviewer_record->team_name.has_value());
    EXPECT_EQ(*reviewer_record->team_name, "Direct Team");
    ASSERT_EQ(reviewer_record->pending_messages.size(), 2u);
    EXPECT_NE(reviewer_record->pending_messages.front().find("Inspect direct connect team migration"), std::string::npos);
    EXPECT_NE(reviewer_record->pending_messages.back().find("Please review direct connect team output"), std::string::npos);

    auto inbox = cc::utils::read_inbox("reviewer-one", std::optional<std::string_view>{"Direct Team"});
    ASSERT_TRUE(inbox.has_value()) << inbox.error();
    ASSERT_EQ(inbox->size(), 1u);
    EXPECT_EQ(inbox->front().from, "team-lead");
    EXPECT_EQ(inbox->front().text, "Please review direct connect team output");
    ASSERT_TRUE(inbox->front().summary.has_value());
    EXPECT_EQ(*inbox->front().summary, "direct team follow-up");

    auto metadata = cc::session::load_session_metadata(sessions_dir, session_id);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->message_count, 4);

    direct_server.stop();
    cc::server::reset_route_state_for_testing();
    cc::tools::global_team_store().clear_for_testing();
    cc::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(RateLimitManager, UpdatesStateFromHeadersAndWarnsNearLimits) {
    cc::services::RateLimitManager manager;
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
    cc::services::RateLimitManager manager;
    manager.mock_rate_limit({.simulate_429 = true});
    EXPECT_TRUE(manager.is_rate_limited());

    manager.clear_mock();
    EXPECT_FALSE(manager.is_rate_limited());
}

TEST(SessionMemoryService, StoresSearchesAndDeletesMemoryItems) {
    cc::services::memory::SessionMemoryService service;
    const auto now = std::chrono::system_clock::now();
    cc::services::memory::MemoryItem item{
        .id = "mem-1",
        .content = "remember project migration details",
        .type = "note",
        .created_at = now,
        .updated_at = now,
        .importance = 7,
    };

    ASSERT_TRUE(service.add_memory(item).has_value());

    auto loaded = service.get_memory("mem-1");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->content, item.content);

    auto matches = service.search_memories("migration");
    ASSERT_TRUE(matches.has_value());
    ASSERT_EQ(matches->size(), 1u);
    EXPECT_EQ(matches->front().id, "mem-1");

    ASSERT_TRUE(service.delete_memory("mem-1").has_value());
    EXPECT_FALSE(service.get_memory("mem-1").has_value());
}

TEST(TokenEstimator, EstimatesTextImagesToolsAndModelLimits) {
    using cc::services::ImageDetail;
    using cc::services::TokenEstimator;

    EXPECT_GT(TokenEstimator::estimate_text("Hello world"), 0u);
    EXPECT_EQ(TokenEstimator::estimate_text(""), 1u);
    EXPECT_EQ(TokenEstimator::estimate_image(512, 512, ImageDetail::low), 85u);
    EXPECT_GT(TokenEstimator::estimate_tool_use("Read", R"({"file_path":"main.cpp"})"), 50u);
    EXPECT_EQ(TokenEstimator::get_model_limit("claude-3-5-sonnet"), 200000u);
    EXPECT_TRUE(TokenEstimator::fits_in_context(1000, "claude-3-5-sonnet"));
}

// LSP response parsers — exercised against canned JSON fixtures (no server).
// These cover the previously-stubbed parsers that dropped most response data.
// ---------------------------------------------------------------------------

namespace {
cc::services::lsp::LspClient make_lsp_for_parsing() {
    return cc::services::lsp::LspClient(cc::services::lsp::LspClient::Config{});
}
} // namespace

TEST(LspClientParser, ParseHoverExtractsMarkupStringAndRange) {
    auto client = make_lsp_for_parsing();
    auto hover = client.parse_hover(
        R"({"contents":{"kind":"markdown","value":"fn doc"},"range":{"start":{"line":1,"character":2},"end":{"line":1,"character":4}}})");
    ASSERT_TRUE(hover.has_value()) << static_cast<int>(hover.error());
    EXPECT_EQ(std::get<std::string>(hover->contents), "fn doc");
    ASSERT_TRUE(hover->range.has_value());
    EXPECT_EQ(hover->range->start.line, 1);
    EXPECT_EQ(hover->range->end.character, 4);
}

TEST(LspClientParser, ParseLocationsReadsUriAndRange) {
    auto client = make_lsp_for_parsing();
    auto locs = client.parse_locations(
        R"json([{"uri":"file:///a.cpp","range":{"start":{"line":0,"character":3},"end":{"line":0,"character":7}}}])json");
    ASSERT_TRUE(locs.has_value()) << static_cast<int>(locs.error());
    ASSERT_EQ(locs->size(), 1u);
    EXPECT_EQ((*locs)[0].uri, "file:///a.cpp");
    EXPECT_EQ((*locs)[0].range.start.character, 3);
    EXPECT_EQ((*locs)[0].range.end.character, 7);
}

TEST(LspClientParser, ParseDocumentSymbolsHandlesHierarchy) {
    auto client = make_lsp_for_parsing();
    auto syms = client.parse_document_symbols(
        R"([{"name":"main","kind":12,"range":{"start":{"line":0,"character":0},"end":{"line":2,"character":0}},"selectionRange":{"start":{"line":0,"character":0},"end":{"line":0,"character":4}},"children":[{"name":"x","kind":13,"range":{"start":{"line":1,"character":0},"end":{"line":1,"character":1}},"selectionRange":{"start":{"line":1,"character":0},"end":{"line":1,"character":1}}}]}])");
    ASSERT_TRUE(syms.has_value());
    ASSERT_EQ(syms->size(), 1u);
    EXPECT_EQ((*syms)[0].name, "main");
    ASSERT_TRUE((*syms)[0].children.has_value());
    EXPECT_EQ((*syms)[0].children->size(), 1u);
    EXPECT_EQ((*syms)[0].children->front().name, "x");
}

TEST(LspClientParser, ParseCodeActionsReadsTitleKindPreferred) {
    auto client = make_lsp_for_parsing();
    auto actions = client.parse_code_actions(
        R"([{"title":"Fix me","kind":"quickfix","isPreferred":true}])");
    ASSERT_TRUE(actions.has_value());
    ASSERT_EQ(actions->size(), 1u);
    EXPECT_EQ((*actions)[0].title, "Fix me");
    ASSERT_TRUE((*actions)[0].kind.has_value());
    EXPECT_EQ((*actions)[0].kind.value(), "quickfix");
    EXPECT_TRUE((*actions)[0].is_preferred.value_or(false));
}

TEST(LspClientParser, ParseTextEditsReadsRangeAndNewText) {
    auto client = make_lsp_for_parsing();
    auto edits = client.parse_text_edits(
        R"([{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":5}},"newText":"hello"}])");
    ASSERT_TRUE(edits.has_value());
    ASSERT_EQ(edits->size(), 1u);
    EXPECT_EQ((*edits)[0].new_text, "hello");
    EXPECT_EQ((*edits)[0].range.end.character, 5);
}

TEST(LspClientParser, ParseCompletionListEnrichesItems) {
    auto client = make_lsp_for_parsing();
    auto list = client.parse_completion_list(
        R"json({"isIncomplete":true,"items":[{"label":"foo","kind":3,"detail":"(int)","documentation":"doc","insertText":"foo()","sortText":"a"}]})json");
    ASSERT_TRUE(list.has_value());
    EXPECT_TRUE(list->is_incomplete);
    ASSERT_EQ(list->items.size(), 1u);
    const auto& item = list->items.front();
    EXPECT_EQ(item.label, "foo");
    ASSERT_TRUE(item.detail.has_value());  EXPECT_EQ(*item.detail, "(int)");
    ASSERT_TRUE(item.documentation.has_value()); EXPECT_EQ(*item.documentation, "doc");
    ASSERT_TRUE(item.insert_text.has_value());  EXPECT_EQ(*item.insert_text, "foo()");
}

TEST(LspClientParser, ParseInitializeResultPopulatesCapabilities) {
    auto client = make_lsp_for_parsing();
    auto init = client.parse_initialize_result(
        R"({"capabilities":{"hoverProvider":true,"definitionProvider":true,"completionProvider":{"triggerCharacters":["."]}},"serverInfo":{"name":"clangd","version":"17.0"}})");
    ASSERT_TRUE(init.has_value());
    EXPECT_TRUE(init->capabilities.hover_provider.value_or(false));
    EXPECT_TRUE(init->capabilities.definition_provider.value_or(false));
    ASSERT_TRUE(init->capabilities.completion_provider.has_value());
    ASSERT_TRUE(init->server_info.has_value());
    EXPECT_NE(init->server_info->find("clangd"), std::string::npos);
}

// ─── P2-07: WorkerRegistry + Server types smoke tests ───────────────────────

import cc.daemon.worker_registry;
import cc.server.types;

namespace {

TEST(WorkerRegistry, ExpiresStale) {
    auto& r = cc::daemon::WorkerRegistry::instance();
    r.clear();

    cc::daemon::WorkerInfo w;
    w.kind = cc::daemon::WorkerKind::InProcess;
    w.hostname = "localhost";
    w.capabilities = {"query"};
    w.max_concurrent_tasks = 1;
    auto id_r = r.register_worker(std::move(w));
    ASSERT_TRUE(id_r.has_value());
    const std::string id = *id_r;

    // Advance heartbeat to a known timestamp then manually expire it.
    (void)r.heartbeat(id, 0.0, 0, 0, cc::daemon::WorkerHealth::Healthy);
    // Use a very short TTL (1ms) + a 20ms sleep so the worker is older than TTL.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const size_t expired = r.expire_stale(std::chrono::milliseconds(1));
    EXPECT_EQ(expired, 1u);
    EXPECT_FALSE(r.lookup(id).has_value());
}

TEST(WorkerRegistry, PickBest) {
    auto& r = cc::daemon::WorkerRegistry::instance();
    r.clear();

    auto mk = [&](int cur, int max, uint64_t mem_used, uint64_t mem_limit,
                  std::string suffix) -> std::string {
        cc::daemon::WorkerInfo w;
        w.kind = cc::daemon::WorkerKind::Subprocess;
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

    cc::daemon::WorkerQueryFilters f;
    f.capability_required = "query";
    f.min_free_tasks = 0;          // do not filter on free slots here
    f.require_heartbeat_within_ms = 0;
    auto best = r.pick_best(f);
    ASSERT_TRUE(best.has_value());
    // Lowest load ratio = id_a (0/1).  If tie, most free memory (A still wins here).
    EXPECT_EQ(best->id, id_a);
}

TEST(WorkerRegistry, Cordon) {
    auto& r = cc::daemon::WorkerRegistry::instance();
    r.clear();

    cc::daemon::WorkerInfo w;
    w.kind = cc::daemon::WorkerKind::InProcess;
    w.hostname = "cordon";
    w.capabilities = {"query"};
    w.max_concurrent_tasks = 2;
    const std::string id = *r.register_worker(std::move(w));

    cc::daemon::WorkerQueryFilters f;
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

TEST(ServerTypes, RoundtripSerde) {
    cc::server::ServerSession s;
    s.id = "session-1";
    s.token = "tok-abcdef";
    s.role = cc::server::Role::Admin;
    s.user_id = "u-42";
    s.user_agent = "test-agent/1.0";
    s.client_ip = "127.0.0.1";
    s.scopes = {"read", "write", "query"};
    s.created_ms = 1'000'000;
    s.expires_ms = 2'000'000;
    s.last_active_ms = 1'500'000;
    s.request_count = 17;
    s.revoked = false;

    const std::string json = cc::server::to_json(s);
    auto parsed = cc::server::ServerSession_from_json(json);
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
    cc::server::ServerSession s;
    s.role = cc::server::Role::Admin;
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

// ── Speculation suggestion engine (port of TS speculation.ts) ────────────────
// The deterministic ranker replaces the former single hardcoded suggestion.
// These tests pin the ranker's contract without a live LLM.

TEST(SpeculationSuggestion, EmptyTurnsReturnsNothing) {
    using cc::services::prompt_suggestion::rank_candidate_suggestions;
    using cc::services::prompt_suggestion::SuggestionRequest;
    SuggestionRequest req;
    EXPECT_TRUE(rank_candidate_suggestions(req).empty());
}

TEST(SpeculationSuggestion, NoAssistantTurnReturnsNothing) {
    using namespace cc::services::prompt_suggestion;
    SuggestionRequest req;
    req.recent_turns.push_back({.role = "user", .content = "implement the login flow"});
    // Early gate: needs >= 1 assistant turn before suggesting.
    EXPECT_TRUE(rank_candidate_suggestions(req).empty());
}

TEST(SpeculationSuggestion, AssistantTurnYieldsRankedCandidates) {
    using namespace cc::services::prompt_suggestion;
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
    using namespace cc::services::prompt_suggestion;
    SuggestionRequest req;
    req.recent_turns.push_back({.role = "user", .content = "refactor the module"});
    req.recent_turns.push_back({.role = "assistant", .content = "I refactored it and added tests."});
    const auto r = rank_candidate_suggestions(req);
    for (std::size_t i = 1; i < r.size(); ++i) {
        EXPECT_GE(r[i - 1].confidence, r[i].confidence);
    }
}

TEST(SpeculationSuggestion, RespectsMaxSuggestions) {
    using namespace cc::services::prompt_suggestion;
    SuggestionRequest req;
    req.max_suggestions = 2;
    req.recent_turns.push_back({.role = "user", .content = "ship the feature"});
    req.recent_turns.push_back({.role = "assistant", .content = "Shipped with full test coverage."});
    const auto r = rank_candidate_suggestions(req);
    EXPECT_LE(r.size(), 2u);
}

TEST(SpeculationSuggestion, QualityFilterRejectsEmpty) {
    using cc::services::prompt_suggestion::should_filter_suggestion;
    EXPECT_TRUE(should_filter_suggestion(""));
    EXPECT_TRUE(should_filter_suggestion("   "));
}

// ============================================================================
// ChannelPermission — short request ID generation
// ============================================================================
// TS REF: src/services/mcp/channelPermissions.ts:140-152

TEST(ChannelPermission, ShortRequestIdIsFiveLetters) {
    using namespace cc::services::mcp;
    auto id = short_request_id("toolu_01ABC123def456GHI789jkl");
    EXPECT_EQ(id.size(), 5u);
    for (char c : id) {
        EXPECT_GE(c, 'a');
        EXPECT_LE(c, 'z');
        EXPECT_NE(c, 'l');  // 'l' excluded from alphabet
    }
}

TEST(ChannelPermission, ShortRequestIdIsDeterministic) {
    using namespace cc::services::mcp;
    auto id1 = short_request_id("toolu_01ABC123def456GHI789jkl");
    auto id2 = short_request_id("toolu_01ABC123def456GHI789jkl");
    EXPECT_EQ(id1, id2);
}

TEST(ChannelPermission, ShortRequestIdDifferentInputsDiffer) {
    using namespace cc::services::mcp;
    auto id1 = short_request_id("toolu_01ABC123def456GHI789jkl");
    auto id2 = short_request_id("toolu_99XYZ999xyz999ABC999mno");
    EXPECT_NE(id1, id2);
}

TEST(ChannelPermission, ShortRequestIdAvoidsBlockedSubstrings) {
    using namespace cc::services::mcp;
    // The re-hash with salt should avoid producing IDs containing
    // blocklisted substrings. We test a few inputs that might hash to
    // problematic outputs.
    for (int i = 0; i < 100; ++i) {
        std::string tool_use_id = "toolu_test_" + std::to_string(i);
        auto id = short_request_id(tool_use_id);
        // Verify no blocked substring is present
        constexpr std::array<std::string_view, 24> blocked = {
            "fuck",  "shit",  "cunt",  "cock",  "dick",  "twat",  "piss",
            "crap",  "bitch", "whore", "ass",   "tit",   "cum",   "fag",
            "dyke",  "nig",   "kike",  "rape",  "nazi",  "damn",  "poo",
            "pee",   "wank",  "anus",
        };
        for (auto bad : blocked) {
            EXPECT_EQ(id.find(bad), std::string::npos)
                << "ID '" << id << "' contains blocked substring '" << bad << "'";
        }
    }
}

// ============================================================================
// ChannelPermission — truncate_for_preview
// ============================================================================
// TS REF: src/services/mcp/channelPermissions.ts:160-167

TEST(ChannelPermission, TruncateForPreviewShort) {
    using namespace cc::services::mcp;
    auto result = truncate_for_preview(R"({"cmd":"ls"})");
    EXPECT_EQ(result, R"({"cmd":"ls"})");
}

TEST(ChannelPermission, TruncateForPreviewLong) {
    using namespace cc::services::mcp;
    std::string long_str(300, 'x');
    auto result = truncate_for_preview(long_str);
    EXPECT_EQ(result.size(), 203u);  // 200 chars + "…" (3 UTF-8 bytes: E2 80 A6)
    EXPECT_EQ(static_cast<unsigned char>(result[200]), 0xE2u);  // first byte of UTF-8 ellipsis …
}

TEST(ChannelPermission, TruncateForPreviewEmpty) {
    using namespace cc::services::mcp;
    auto result = truncate_for_preview("");
    EXPECT_EQ(result, "(unserializable)");
}

// ============================================================================
// ChannelPermission — parse_permission_reply
// ============================================================================
// TS REF: src/services/mcp/channelPermissions.ts:75

TEST(ChannelPermission, ParseReplyYesLowercase) {
    using namespace cc::services::mcp;
    auto parsed = parse_permission_reply("yes tbxkq");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->request_id, "tbxkq");
    EXPECT_EQ(parsed->behavior, ChannelPermissionBehavior::Allow);
}

TEST(ChannelPermission, ParseReplyNoLowercase) {
    using namespace cc::services::mcp;
    auto parsed = parse_permission_reply("no tbxkq");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->request_id, "tbxkq");
    EXPECT_EQ(parsed->behavior, ChannelPermissionBehavior::Deny);
}

TEST(ChannelPermission, ParseReplyYShortForm) {
    using namespace cc::services::mcp;
    auto parsed = parse_permission_reply("y tbxkq");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->behavior, ChannelPermissionBehavior::Allow);
}

TEST(ChannelPermission, ParseReplyNShortForm) {
    using namespace cc::services::mcp;
    auto parsed = parse_permission_reply("n tbxkq");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->behavior, ChannelPermissionBehavior::Deny);
}

TEST(ChannelPermission, ParseReplyCaseInsensitive) {
    using namespace cc::services::mcp;
    auto parsed = parse_permission_reply("YES TBXKQ");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->request_id, "tbxkq");  // lowercased
    EXPECT_EQ(parsed->behavior, ChannelPermissionBehavior::Allow);
}

TEST(ChannelPermission, ParseReplyWithWhitespacePadding) {
    using namespace cc::services::mcp;
    auto parsed = parse_permission_reply("  yes   tbxkq  ");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->request_id, "tbxkq");
}

TEST(ChannelPermission, ParseReplyRejectsBareYes) {
    using namespace cc::services::mcp;
    EXPECT_FALSE(parse_permission_reply("yes").has_value());
}

TEST(ChannelPermission, ParseReplyRejectsIdWithL) {
    using namespace cc::services::mcp;
    // 'l' is excluded from the alphabet (looks like 1/I)
    EXPECT_FALSE(parse_permission_reply("yes tblkq").has_value());
}

TEST(ChannelPermission, ParseReplyRejectsExtraText) {
    using namespace cc::services::mcp;
    EXPECT_FALSE(parse_permission_reply("yes tbxkq please").has_value());
}

TEST(ChannelPermission, ParseReplyRejectsWrongIdLength) {
    using namespace cc::services::mcp;
    EXPECT_FALSE(parse_permission_reply("yes tbxk").has_value());   // 4 chars
    EXPECT_FALSE(parse_permission_reply("yes tbxkqq").has_value()); // 6 chars
}

// ============================================================================
// ChannelPermission — ChannelPermissionCallbacks
// ============================================================================
// TS REF: src/services/mcp/channelPermissions.ts:46-61, 209-240

TEST(ChannelPermission, CallbacksResolveAllow) {
    using namespace cc::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    bool called = false;
    ChannelPermissionBehavior received_behavior{};
    std::string received_server;

    auto unsub = cbs->on_response("tbxkq", [&](const ChannelPermissionResponse& resp) {
        called = true;
        received_behavior = resp.behavior;
        received_server = resp.from_server;
    });

    bool resolved = cbs->resolve("tbxkq", ChannelPermissionBehavior::Allow, "plugin:telegram:tg");
    EXPECT_TRUE(resolved);
    EXPECT_TRUE(called);
    EXPECT_EQ(received_behavior, ChannelPermissionBehavior::Allow);
    EXPECT_EQ(received_server, "plugin:telegram:tg");
}

TEST(ChannelPermission, CallbacksResolveDeny) {
    using namespace cc::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    ChannelPermissionBehavior received{};
    cbs->on_response("abcde", [&](const ChannelPermissionResponse& resp) {
        received = resp.behavior;
    });
    cbs->resolve("abcde", ChannelPermissionBehavior::Deny, "test");
    EXPECT_EQ(received, ChannelPermissionBehavior::Deny);
}

TEST(ChannelPermission, CallbacksResolveReturnsFalseForUnknown) {
    using namespace cc::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    EXPECT_FALSE(cbs->resolve("zzzzz", ChannelPermissionBehavior::Allow, "test"));
}

TEST(ChannelPermission, CallbacksUnsubscribePreventsResolve) {
    using namespace cc::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    bool called = false;
    auto unsub = cbs->on_response("tbxkq", [&](const ChannelPermissionResponse&) {
        called = true;
    });
    unsub();  // unsubscribe
    EXPECT_FALSE(cbs->resolve("tbxkq", ChannelPermissionBehavior::Allow, "test"));
    EXPECT_FALSE(called);
}

TEST(ChannelPermission, CallbacksCaseInsensitiveMatching) {
    using namespace cc::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    bool called = false;
    cbs->on_response("TBXKQ", [&](const ChannelPermissionResponse&) {
        called = true;
    });
    // Resolve with different case
    EXPECT_TRUE(cbs->resolve("tbxkq", ChannelPermissionBehavior::Allow, "test"));
    EXPECT_TRUE(called);
}

TEST(ChannelPermission, CallbacksResolveDeletesBeforeCalling) {
    using namespace cc::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    int call_count = 0;
    cbs->on_response("tbxkq", [&](const ChannelPermissionResponse&) {
        ++call_count;
    });
    // First resolve succeeds
    EXPECT_TRUE(cbs->resolve("tbxkq", ChannelPermissionBehavior::Allow, "test"));
    // Second resolve on same ID fails (already consumed)
    EXPECT_FALSE(cbs->resolve("tbxkq", ChannelPermissionBehavior::Allow, "test"));
    EXPECT_EQ(call_count, 1);
}

TEST(ChannelPermission, CallbacksPendingCount) {
    using namespace cc::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    EXPECT_EQ(cbs->pending_count(), 0u);
    auto u1 = cbs->on_response("aaaaa", [](auto){});
    EXPECT_EQ(cbs->pending_count(), 1u);
    auto u2 = cbs->on_response("bbbbb", [](auto){});
    EXPECT_EQ(cbs->pending_count(), 2u);
    u1();
    EXPECT_EQ(cbs->pending_count(), 1u);
    cbs->resolve("bbbbb", ChannelPermissionBehavior::Allow, "test");
    EXPECT_EQ(cbs->pending_count(), 0u);
}

// ============================================================================
// ChannelPermission — filter_permission_relay_clients
// ============================================================================
// TS REF: src/services/mcp/channelPermissions.ts:177-194

namespace {
struct TestMcpClient {
    std::string name;
    cc::services::mcp::ServerState state = cc::services::mcp::ServerState::Ready;
    cc::services::mcp::ServerCapabilities capabilities;
};
} // anonymous namespace

TEST(ChannelPermission, FilterRelayRequiresConnected) {
    using namespace cc::services::mcp;
    std::vector<TestMcpClient> clients = {
        {"telegram", ServerState::Ready, {}},
        {"discord", ServerState::Error, {}},
    };
    clients[0].capabilities.experimental["loom/channel"] = "true";
    clients[0].capabilities.experimental["loom/channel/permission"] = "true";
    clients[1].capabilities.experimental["loom/channel"] = "true";
    clients[1].capabilities.experimental["loom/channel/permission"] = "true";

    auto filtered = filter_permission_relay_clients<TestMcpClient>(
        clients, [](auto) { return true; });
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].name, "telegram");
}

TEST(ChannelPermission, FilterRelayRequiresAllowlist) {
    using namespace cc::services::mcp;
    std::vector<TestMcpClient> clients = {
        {"telegram", ServerState::Ready, {}},
        {"discord", ServerState::Ready, {}},
    };
    for (auto& c : clients) {
        c.capabilities.experimental["loom/channel"] = "true";
        c.capabilities.experimental["loom/channel/permission"] = "true";
    }

    auto filtered = filter_permission_relay_clients<TestMcpClient>(
        clients, [](auto name) { return name == "telegram"; });
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].name, "telegram");
}

TEST(ChannelPermission, FilterRelayRequiresBothCapabilities) {
    using namespace cc::services::mcp;
    std::vector<TestMcpClient> clients = {
        {"both", ServerState::Ready, {}},
        {"channel_only", ServerState::Ready, {}},
        {"permission_only", ServerState::Ready, {}},
        {"neither", ServerState::Ready, {}},
    };
    clients[0].capabilities.experimental["loom/channel"] = "true";
    clients[0].capabilities.experimental["loom/channel/permission"] = "true";
    clients[1].capabilities.experimental["loom/channel"] = "true";
    clients[2].capabilities.experimental["loom/channel/permission"] = "true";

    auto filtered = filter_permission_relay_clients<TestMcpClient>(
        clients, [](auto) { return true; });
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].name, "both");
}

TEST(ChannelPermission, FilterRelayEmptyInput) {
    using namespace cc::services::mcp;
    std::vector<TestMcpClient> clients;
    auto filtered = filter_permission_relay_clients<TestMcpClient>(
        clients, [](auto) { return true; });
    EXPECT_TRUE(filtered.empty());
}

// ============================================================================
// ChannelPermission — ChannelPermissionStore
// ============================================================================
// TS REF: conceptual extension (persistent permission rules)

TEST(ChannelPermission, StoreDefaultIsPrompt) {
    using namespace cc::services::mcp;
    ChannelPermissionStore store;
    // No rules loaded → default to Prompt
    EXPECT_EQ(store.check_permission("any_server", "any_tool"),
              ChannelPermission::Prompt);
}

TEST(ChannelPermission, StoreGlobalRuleApplies) {
    using namespace cc::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_global_rule(
        ChannelPermission::Allowed));
    EXPECT_EQ(store.check_permission("server_a", "tool_x"),
              ChannelPermission::Allowed);
    EXPECT_EQ(store.check_permission("server_b", "tool_y"),
              ChannelPermission::Allowed);
}

TEST(ChannelPermission, StoreServerRuleOverridesGlobal) {
    using namespace cc::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_global_rule(
        ChannelPermission::Prompt));
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "trusted_server", ChannelPermission::Allowed));
    EXPECT_EQ(store.check_permission("trusted_server", "any_tool"),
              ChannelPermission::Allowed);
    EXPECT_EQ(store.check_permission("other_server", "any_tool"),
              ChannelPermission::Prompt);
}

TEST(ChannelPermission, StoreToolRuleOverridesServer) {
    using namespace cc::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "my_server", ChannelPermission::Allowed));
    store.set_permission(ChannelPermissionStore::make_tool_rule(
        "my_server", "dangerous_tool", ChannelPermission::Denied));
    EXPECT_EQ(store.check_permission("my_server", "safe_tool"),
              ChannelPermission::Allowed);
    EXPECT_EQ(store.check_permission("my_server", "dangerous_tool"),
              ChannelPermission::Denied);
}

TEST(ChannelPermission, StoreMostSpecificWins) {
    using namespace cc::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_global_rule(
        ChannelPermission::Prompt));
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "srv", ChannelPermission::Allowed));
    store.set_permission(ChannelPermissionStore::make_tool_rule(
        "srv", "tool", ChannelPermission::Denied));
    // Tool rule (score 3) > Server rule (score 2) > Global rule (score 1)
    EXPECT_EQ(store.check_permission("srv", "tool"),
              ChannelPermission::Denied);
}

TEST(ChannelPermission, StoreRemoveRule) {
    using namespace cc::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "srv", ChannelPermission::Allowed));
    EXPECT_EQ(store.check_permission("srv", "tool"),
              ChannelPermission::Allowed);
    bool removed = store.remove_rule(ChannelPermissionScope::Server, "srv", "");
    EXPECT_TRUE(removed);
    EXPECT_EQ(store.check_permission("srv", "tool"),
              ChannelPermission::Prompt);
}

TEST(ChannelPermission, StoreRemoveNonexistentReturnsFalse) {
    using namespace cc::services::mcp;
    ChannelPermissionStore store;
    EXPECT_FALSE(store.remove_rule(ChannelPermissionScope::Server, "nope", ""));
}

TEST(ChannelPermission, StoreUpsertSameIdentity) {
    using namespace cc::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "srv", ChannelPermission::Allowed));
    // Setting same identity again should update, not duplicate
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "srv", ChannelPermission::Denied));
    EXPECT_EQ(store.rule_count(), 1u);
    EXPECT_EQ(store.check_permission("srv", "tool"),
              ChannelPermission::Denied);
}

TEST(ChannelPermission, StoreGetAllRules) {
    using namespace cc::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_global_rule(
        ChannelPermission::Prompt));
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "srv", ChannelPermission::Allowed));
    auto rules = store.get_all_rules();
    EXPECT_EQ(rules.size(), 2u);
}

TEST(ChannelPermission, StorePersistenceRoundtrip) {
    using namespace cc::services::mcp;
    // Isolate to a unique temp file: the store defaults to a shared
    // ~/.loom path, which races with sibling tests under parallel ctest
    // (and must never touch the real user file).
    const auto tmp_file =
        fs::temp_directory_path() /
        ("loom_chanperm_roundtrip_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".json");
    EnvironmentGuard env_override(
        "LOOM_CHANNEL_PERMISSIONS_FILE", tmp_file.string());
    // Create a store, set rules, save
    {
        ChannelPermissionStore store;
        store.set_permission(ChannelPermissionStore::make_server_rule(
            "test_server", ChannelPermission::Allowed));
        store.set_permission(ChannelPermissionStore::make_tool_rule(
            "test_server", "dangerous", ChannelPermission::Denied));
        store.save();
    }
    // Load into a new store and verify
    {
        ChannelPermissionStore store;
        store.load();
        EXPECT_EQ(store.check_permission("test_server", "safe"),
                  ChannelPermission::Allowed);
        EXPECT_EQ(store.check_permission("test_server", "dangerous"),
                  ChannelPermission::Denied);
    }
    // Cleanup: remove the test file
    std::error_code ec;
    fs::remove(tmp_file, ec);
}

TEST(ChannelPermission, StoreFactoryCreatesLoaded) {
    using namespace cc::services::mcp;
    const auto tmp_file =
        fs::temp_directory_path() /
        ("loom_chanperm_factory_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".json");
    EnvironmentGuard env_override(
        "LOOM_CHANNEL_PERMISSIONS_FILE", tmp_file.string());
    std::error_code ec;
    fs::remove(tmp_file, ec);

    auto store = create_channel_permission_store();
    ASSERT_NE(store, nullptr);
    // Should have loaded from disk (empty rules → default Prompt)
    EXPECT_EQ(store->check_permission("any", "tool"),
              ChannelPermission::Prompt);
    EXPECT_EQ(store->rule_count(), 0u);
    fs::remove(tmp_file, ec);
}

// ============================================================================
// ChannelPermission — string conversion utilities
// ============================================================================

TEST(ChannelPermission, PermissionToString) {
    using namespace cc::services::mcp;
    EXPECT_EQ(channel_permission_to_string(ChannelPermission::Allowed), "Allowed");
    EXPECT_EQ(channel_permission_to_string(ChannelPermission::Denied), "Denied");
    EXPECT_EQ(channel_permission_to_string(ChannelPermission::Prompt), "Prompt");
}

TEST(ChannelPermission, ScopeToString) {
    using namespace cc::services::mcp;
    EXPECT_EQ(channel_permission_scope_to_string(ChannelPermissionScope::Global), "Global");
    EXPECT_EQ(channel_permission_scope_to_string(ChannelPermissionScope::Server), "Server");
    EXPECT_EQ(channel_permission_scope_to_string(ChannelPermissionScope::Tool), "Tool");
}

// ============================================================================
// ChannelPermission — feature gate
// ============================================================================

TEST(ChannelPermission, FeatureGateDefaultsToFalse) {
    using namespace cc::services::mcp;
    // Stub returns false until GrowthBook integration exists
    EXPECT_FALSE(is_channel_permission_relay_enabled());
}

}  // namespace

TEST(MemoryExtraction, LlmPromptContainsDirInstructionsAndTranscript) {
    namespace em = cc::services::extract_memories;
    const std::string prompt = em::build_llm_extraction_prompt(
        "/tmp/x/memory",
        "user: always write tests in English\nassistant: got it",
        std::string_view{"- [Old](old.md) — prior memory"});

    // Instructs the sub-agent WHERE to write.
    EXPECT_NE(prompt.find("/tmp/x/memory"), std::string::npos);
    // Frontmatter + MEMORY.md index contract.
    EXPECT_NE(prompt.find("metadata:"), std::string::npos);
    EXPECT_NE(prompt.find("MEMORY.md"), std::string::npos);
    // Existing manifest is passed so it can avoid duplicates.
    EXPECT_NE(prompt.find("prior memory"), std::string::npos);
    // The recent transcript is embedded.
    EXPECT_NE(prompt.find("always write tests in English"), std::string::npos);
    // Discourages ephemeral/secrets persistence.
    EXPECT_NE(prompt.find("secrets"), std::string::npos);
}

TEST(MemoryExtraction, MinNewMessagesConstantIsPositive) {
    namespace em = cc::services::extract_memories;
    EXPECT_GE(em::kExtractionMinNewMessages, 1u);
}

// ===========================================================================
// TokenRefreshScheduler — proactive refresh must carry a real token.
// Regression guard: the refresh callback used to return void, so on_refresh
// always fired with an empty OAuth token (log-only refresh).
// ===========================================================================
namespace {

cc::bridge::TokenRefreshScheduler::Params fast_refresh_params() {
    cc::bridge::TokenRefreshScheduler::Params p;
    // expires_in=1s with a 0 buffer → 5s floor in the scheduler; keep the
    // test bounded by driving expires_in near zero.
    p.refresh_buffer_ms = std::chrono::milliseconds{0};
    p.label = "test";
    return p;
}

}  // namespace

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

    cc::bridge::TokenRefreshScheduler scheduler(std::move(params));
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

    cc::bridge::TokenRefreshScheduler scheduler(std::move(params));
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

    cc::bridge::TokenRefreshScheduler scheduler(std::move(params));
    scheduler.schedule_from_expires_in("ses_cancel", /*expires_in_s=*/30);
    EXPECT_TRUE(scheduler.is_scheduled());

    scheduler.cancel_all();
    EXPECT_FALSE(scheduler.is_scheduled());

    // Well past the point a refresh would have fired.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(refresh_calls.load(), 0);
}

// ===========================================================================
// RFC-0001 B followup c13b — structured user settings on ConfigManager.
// ===========================================================================
namespace {

struct C13Paths {
    fs::path root;
    fs::path base;  // The actually-selected ancestry-clean base.
    fs::path global_path;
    fs::path user_path;
    fs::path project_path;
    fs::path local_path;

    explicit C13Paths(std::string_view tag) {
        // GTEST_SKIP() cannot run from a constructor (it expands to a
        // `return <void-expr>`), so a ctor without any clean base falls back
        // to the system temp dir rather than skipping; in practice a clean
        // base always exists (/dev/shm on Linux, clean /tmp on mac/CI), and
        // the PlainDirectoryWritesNoGitignore semantic pin performs the
        // explicit skip in void test context.
        static std::atomic<unsigned> counter{0};
        base = c13_clean_temp_base().value_or(fs::temp_directory_path());
        const auto suffix =
            std::chrono::system_clock::now().time_since_epoch().count();
        // Anchored under a git-ancestry-clean base so project/local writes
        // and full saves (whose .gitignore appender walks up from cwd) can
        // never reach a real work tree such as a stray /tmp/.git.
        root = base /
               (std::string("loom_c13_") + std::string(tag) + "_" +
                std::to_string(::getpid()) + "_" +
                std::to_string(counter.fetch_add(1,
                    std::memory_order_relaxed)) + "_" +
                std::to_string(suffix));
        fs::create_directories(root);
        global_path  = root / "global.json";
        user_path    = root / "user.json";
        project_path = root / "project.json";
        local_path   = root / "project.local.json";
    }
    ~C13Paths() { std::error_code ec; fs::remove_all(root, ec); }

    [[nodiscard]] cc::core::ConfigManager manager() const {
        return cc::core::ConfigManager(global_path, user_path,
                                       project_path, local_path);
    }
};

void c13_write_file(const fs::path& path, std::string_view content) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

// Owns the parsed document alongside its root view: the JsonVal points
// into the JsonDoc's storage, so both must live for the same scope.
struct C13Json {
    cc::utils::json::JsonDoc doc;
    cc::utils::json::JsonVal root;

    explicit C13Json(std::string_view text) {
        if (auto parsed = cc::utils::json::parse(text)) {
            doc = std::move(*parsed);
            root = doc.root();
        }
    }
    operator const cc::utils::json::JsonVal&() const noexcept { return root; }
};

[[nodiscard]] C13Json c13_parse(std::string_view text) {
    return C13Json(text);
}

} // namespace

// All 7 writable kinds round-trip through a quiet reload and a fresh
// manager, with the canonical value token and repaired=null on a valid file.
TEST(ConfigManagerUserSettings, SevenWritableKindsRoundTrip) {
    C13Paths p("seven");
    {
        auto m = p.manager();
        auto setv = [&](std::string_view key, std::string_view json) {
            auto value = c13_parse(json);
            auto out = m.set_user_setting(key, value);
            ASSERT_TRUE(out.has_value()) << out.error().message;
            EXPECT_FALSE(out->repaired.has_value());
        };
        setv("model.default_model", R"("  my-model  ")");
        setv("model.max_output_tokens", "4096");
        setv("model.temperature", "0.5");
        setv("model.extended_thinking", "true");
        setv("model.thinking_budget", "2048");
        setv("model.context_window_size", "100000");
        setv("network.max_retries", "0");
    }
    auto fresh = p.manager();
    ASSERT_TRUE(fresh.load().has_value());
    const auto& s = fresh.settings();
    EXPECT_EQ(s.model.default_model, "my-model");
    EXPECT_EQ(s.model.max_output_tokens, 4096u);
    EXPECT_DOUBLE_EQ(*s.model.temperature, 0.5);
    EXPECT_TRUE(s.model.extended_thinking);
    EXPECT_EQ(*s.model.thinking_budget, 2048u);
    EXPECT_EQ(s.model.context_window_size, 100000u);
    EXPECT_EQ(s.network.max_retries, 0u);

    // Native JSON bool/number inputs are accepted and normalized.
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out1 = m.set_user_setting("model.extended_thinking", c13_parse("false"));
        ASSERT_TRUE(out1.has_value());
        auto out2 = m.set_user_setting("model.temperature", c13_parse("0.25"));
        ASSERT_TRUE(out2.has_value());
        auto out3 = m.set_user_setting("network.max_retries", c13_parse("5"));
        ASSERT_TRUE(out3.has_value()) << out3.error().message;
    }
    auto reloaded = p.manager();
    ASSERT_TRUE(reloaded.load().has_value());
    EXPECT_FALSE(reloaded.settings().model.extended_thinking);
    EXPECT_DOUBLE_EQ(*reloaded.settings().model.temperature, 0.25);
    EXPECT_EQ(reloaded.settings().network.max_retries, 5u);
}

// Temperature: [0,1] bounds, text/number inputs, and null-clear (user tier
// only — a lower-tier value still wins the merge).
TEST(ConfigManagerUserSettings, TemperatureBoundsAndClear) {
    C13Paths p("temp");
    c13_write_file(p.global_path, R"JSON({"model":{"temperature":0.25}})JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    for (const auto* bad : {"-0.01", "1.01", "2", "\"hot\"", "true"}) {
        auto out = m.set_user_setting("model.temperature", c13_parse(bad));
        EXPECT_FALSE(out.has_value()) << bad;
    }
    for (const auto* good : {"0", "1", "0.75", "\"0.1\""}) {
        auto out = m.set_user_setting("model.temperature", c13_parse(good));
        EXPECT_TRUE(out.has_value()) << good << ": "
            << (out ? "" : out.error().message);
    }
    // Explicit null clears the USER leaf: the file stores null and the
    // effective value falls through to the global 0.25.
    {
        auto out = m.set_user_setting("model.temperature", c13_parse("null"));
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out->value_token, "null");
    }
    {
        std::ifstream in(p.user_path);
        std::string bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
        EXPECT_NE(bytes.find("\"temperature\": null"), std::string::npos);
    }
    auto after_mgr = p.manager();
    ASSERT_TRUE(after_mgr.load().has_value());
    ASSERT_TRUE(after_mgr.settings().model.temperature.has_value());
    EXPECT_DOUBLE_EQ(*after_mgr.settings().model.temperature, 0.25);
}

// thinking_budget: null/0 clear, otherwise the Anthropic 1024 minimum.
TEST(ConfigManagerUserSettings, ThinkingBudgetBoundsAndClear) {
    C13Paths p("budget");
    c13_write_file(p.global_path, R"JSON({"model":{"thinking_budget":4096}})JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    EXPECT_FALSE(m.set_user_setting("model.thinking_budget", c13_parse("512")).has_value());
    EXPECT_FALSE(m.set_user_setting("model.thinking_budget", c13_parse("\"abc\"")).has_value());
    EXPECT_FALSE(m.set_user_setting("model.thinking_budget", c13_parse("-1")).has_value());
    ASSERT_TRUE(m.set_user_setting("model.thinking_budget", c13_parse("1024")).has_value());
    {
        auto at_min = p.manager();
        ASSERT_TRUE(at_min.load().has_value());
        EXPECT_EQ(*at_min.settings().model.thinking_budget, 1024u);
    }
    // 0 maps to a null clear; lower-tier 4096 wins again.
    ASSERT_TRUE(m.set_user_setting("model.thinking_budget", c13_parse("0")).has_value());
    ASSERT_TRUE(m.set_user_setting("model.thinking_budget", c13_parse("null")).has_value());
    auto after_mgr = p.manager();
    ASSERT_TRUE(after_mgr.load().has_value());
    ASSERT_TRUE(after_mgr.settings().model.thinking_budget.has_value());
    EXPECT_EQ(*after_mgr.settings().model.thinking_budget, 4096u);
}

// Malformed / out-of-range values are rejected before any write.
TEST(ConfigManagerUserSettings, MalformedValuesRejected) {
    C13Paths p("malformed");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    auto rejected = [&](std::string_view key, std::string_view json) {
        auto out = m.set_user_setting(key, c13_parse(json));
        EXPECT_FALSE(out.has_value()) << key << " <- " << json;
    };
    rejected("model.default_model", R"("")");
    rejected("model.default_model", R"("   ")");
    rejected("model.default_model", "42");
    rejected("model.max_output_tokens", "\"abc\"");
    rejected("model.max_output_tokens", "-1");
    rejected("model.max_output_tokens", "1.5");
    rejected("model.max_output_tokens", "4294967296"); // uint32 max + 1
    rejected("model.max_output_tokens", "\"18446744073709551616\"");
    rejected("model.max_output_tokens", "\"-5\"");
    rejected("model.context_window_size", "0");        // positive uint
    rejected("model.extended_thinking", "\"yes\"");    // exact true|false only
    rejected("model.extended_thinking", "1");
    rejected("network.max_retries", "\"1x\"");
    rejected("model.temperature", "{}");

    // No file was created by the failed writes.
    EXPECT_FALSE(fs::exists(p.user_path));

    // max_retries accepts 0; the positive keys do not.
    ASSERT_TRUE(m.set_user_setting("network.max_retries", c13_parse("0"))
                    .has_value());
}

// Read-only / blocked / unknown keys each get their terminal error, and no
// write happens.
TEST(ConfigManagerUserSettings, ReadonlyBlockedUnknownRejected) {
    C13Paths p("blocked");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    auto ro = m.set_user_setting("display.theme", c13_parse(R"("dark")"));
    ASSERT_FALSE(ro.has_value());
    EXPECT_NE(ro.error().message.find("not writable through this tool"),
              std::string::npos);
    EXPECT_NE(ro.error().message.find("no runtime component consumes"),
              std::string::npos);

    const std::array<std::pair<const char*, const char*>, 9> blocked_cases = {{
        {"network.api_key", "ANTHROPIC_API_KEY"},
        {"network.base_url", "ANTHROPIC_BASE_URL"},
        {"network.proxy", "HTTPS_PROXY"},
        {"network.verify_ssl", "TLS"},
        {"permissions.deny_rules", "permission"},
        {"systemPrompt", "session"},
        {"customInstructions", "session"},
        {"mcpServers", "loom mcp"},
        {"xaaIdp", "/mcp xaa"},
    }};
    for (const auto& [key, hint] : blocked_cases) {
        auto out = m.set_user_setting(key, c13_parse(R"("x")"));
        ASSERT_FALSE(out.has_value()) << key;
        EXPECT_NE(out.error().message.find(hint), std::string::npos)
            << key << ": " << out.error().message;
    }

    auto unknown = m.set_user_setting("totally.bogus", c13_parse("1"));
    ASSERT_FALSE(unknown.has_value());
    EXPECT_NE(unknown.error().message.find("Unknown configuration key"),
              std::string::npos);
    EXPECT_NE(unknown.error().message.find("action=list"), std::string::npos);

    EXPECT_FALSE(fs::exists(p.user_path));
}

// Read-only keys are projected with writable=false / consumes=[], and the
// list payload carries the closed sets plus enum values for display.theme.
TEST(ConfigManagerUserSettings, ReadOnlyProjectionAndListShape) {
    C13Paths p("projection");
    c13_write_file(p.user_path, R"JSON({
      "display": {"theme": "dark", "line_width": 120, "compact_mode": true},
      "network": {"timeout_seconds": 45},
      "permissions": {"allow_network": false}
    })JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    auto theme = m.agent_setting_value_json("display.theme");
    ASSERT_TRUE(theme.has_value());
    auto theme_val = c13_parse(*theme);
    EXPECT_EQ(theme_val.root.get("writable").as_bool(), false);
    EXPECT_TRUE(theme_val.root.get("consumes").is_arr());
    EXPECT_EQ(theme_val.root.get("consumes").size(), 0u);
    EXPECT_EQ(std::string(theme_val.root.get("source").as_str()), "file");
    EXPECT_EQ(std::string(theme_val.root.get("value").as_str()), "dark");

    auto settings = c13_parse(m.serialize_agent_settings_json());
    EXPECT_TRUE(settings.root.get("model").get("temperature").get("value").is_null());
    ASSERT_TRUE(settings.root.get("model").get("temperature").is_obj());
    EXPECT_EQ(settings.root.get("display").get("line_width").get("value").as_int(), 120);
    EXPECT_EQ(settings.root.get("network").get("timeout_seconds").get("value").as_int(), 45);
    EXPECT_EQ(settings.root.get("permissions").get("allow_network").get("value").as_bool(), false);

    auto list = c13_parse(cc::core::ConfigManager::serialize_user_setting_specs_json());
    EXPECT_EQ(list.root.get("writable").size(), 7u);
    EXPECT_EQ(list.root.get("read_only").size(), 9u);
    EXPECT_GE(list.root.get("blocked").size(), 9u);
    // Enumeration metadata survives into the list payload.
    bool found_theme = false;
    list.root.get("read_only").iter([&](auto item) {
        if (std::string(item.get("key").as_str()) == "display.theme") {
            found_theme = true;
            EXPECT_EQ(item.get("enum_values").size(), 3u);
        }
    });
    EXPECT_TRUE(found_theme);
    auto null_keys = list.root.get("null_clear_keys");
    ASSERT_EQ(null_keys.size(), 2u);
    EXPECT_EQ(std::string(null_keys.at(0).as_str()), "model.temperature");
    EXPECT_EQ(std::string(null_keys.at(1).as_str()), "model.thinking_budget");
}

// Patching one leaf preserves sibling model/display keys, unknown top-level
// keys, and the mcpServers section byte-for-byte structurally.
TEST(ConfigManagerUserSettings, SiblingUnknownAndMcpPreserved) {
    C13Paths p("preserve");
    c13_write_file(p.user_path, R"JSON({
  "model": {
    "default_model": "kept-model",
    "context_window_size": 123456,
    "unknown_future_flag": true
  },
  "display": {"theme": "light"},
  "mcpServers": {"srv": {"command": "node", "args": ["x.js"]}},
  "totally_unknown_section": {"a": [1, 2, 3]}
})JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    ASSERT_TRUE(m.set_user_setting("network.max_retries", c13_parse("7"))
                    .has_value());

    auto doc = cc::utils::json::parse_file(p.user_path);
    ASSERT_TRUE(doc.has_value());
    const auto root = doc->root();
    EXPECT_EQ(root.get("model").get("default_model").as_str(),
              std::string_view("kept-model"));
    EXPECT_EQ(root.get("model").get("context_window_size").as_int(), 123456);
    EXPECT_EQ(root.get("model").get("unknown_future_flag").as_bool(), true);
    EXPECT_EQ(root.get("display").get("theme").as_str(),
              std::string_view("light"));
    EXPECT_EQ(root.get("mcpServers").get("srv").get("command").as_str(),
              std::string_view("node"));
    EXPECT_EQ(root.get("totally_unknown_section").get("a").at(1).as_int(), 2);
    EXPECT_EQ(root.get("network").get("max_retries").as_int(), 7);
}

// Salvage tri-state: valid object untouched; leading object + trailing junk
// dropped; leading non-object and malformed objects replaced wholesale.
TEST(ConfigManagerUserSettings, SalvageTriState) {
    C13Paths p("salvage");

    // 1. strict-valid object → Untouched (repaired null), keys preserved.
    c13_write_file(p.user_path, R"JSON({"model":{"default_model":"a"},"x":1})JSON");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("network.max_retries", c13_parse("2"));
        ASSERT_TRUE(out.has_value());
        EXPECT_FALSE(out->repaired.has_value());
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("model").get("default_model").as_str(),
                  std::string_view("a"));
        EXPECT_EQ(root.root.get("x").as_int(), 1);
    }

    // 2. complete leading OBJECT + trailing junk → trailing_junk_dropped.
    c13_write_file(p.user_path,
                  R"JSON({"model":{"default_model":"b"}} TRAILING GARBAGE)JSON");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("network.max_retries", c13_parse("3"));
        ASSERT_TRUE(out.has_value());
        ASSERT_TRUE(out->repaired.has_value());
        EXPECT_EQ(*out->repaired, "trailing_junk_dropped");
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("model").get("default_model").as_str(),
                  std::string_view("b"));
        EXPECT_EQ(root.root.get("network").get("max_retries").as_int(), 3);
    }

    // 3. leading complete non-object [1,2] + junk → replaced_unparseable.
    c13_write_file(p.user_path, "[1,2] GARBAGE");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("network.max_retries", c13_parse("4"));
        ASSERT_TRUE(out.has_value());
        ASSERT_TRUE(out->repaired.has_value());
        EXPECT_EQ(*out->repaired, "replaced_unparseable");
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("network").get("max_retries").as_int(), 4);
    }

    // 4. trailing-comma malformed object → replaced_unparseable.
    c13_write_file(p.user_path, R"JSON({"model":{"default_model":"c",}})JSON");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("network.max_retries", c13_parse("5"));
        ASSERT_TRUE(out.has_value());
        ASSERT_TRUE(out->repaired.has_value());
        EXPECT_EQ(*out->repaired, "replaced_unparseable");
    }

    // 5. pure junk → replaced_unparseable.
    c13_write_file(p.user_path, "FOO=bar\nBAZ=qux\n");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("network.max_retries", c13_parse("6"));
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out->repaired.value_or(""), "replaced_unparseable");
    }
}

// Duplicate keys on the patched leaf are canonicalized to one; the write is
// valid JSON with a single key (yyjson obj_put replaces the first twin).
TEST(ConfigManagerUserSettings, DuplicateKeyCanonicalizedOnPatch) {
    C13Paths p("dup");
    c13_write_file(p.user_path,
        R"JSON({"model":{"default_model":"one","default_model":"two"}}junk!)JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    auto out = m.set_user_setting("model.default_model", c13_parse(R"("final")"));
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->repaired.value_or(""), "trailing_junk_dropped");

    std::ifstream f(p.user_path);
    const std::string bytes((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
    // Exactly one canonical leaf spelling survives.
    EXPECT_EQ(c6_count_occurrences(bytes, "\"default_model\""), 1u);
    EXPECT_EQ(bytes.find("\"one\""), std::string::npos);
    EXPECT_EQ(bytes.find("\"two\""), std::string::npos);
    auto root = c13_parse(bytes);
    EXPECT_EQ(root.root.get("model").get("default_model").as_str(),
              std::string_view("final"));
}

// Blank/missing user files start from {} and report repaired=null.
TEST(ConfigManagerUserSettings, BlankAndMissingFileFreshStart) {
    C13Paths p("blank");
    {
        std::ofstream(p.user_path) << "  \n\t \n";
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("network.max_retries", c13_parse("1"));
        ASSERT_TRUE(out.has_value());
        EXPECT_FALSE(out->repaired.has_value());
    }
    fs::remove(p.user_path);
    {
        auto m = p.manager();
        auto out = m.set_user_setting("network.max_retries", c13_parse("2"));
        ASSERT_TRUE(out.has_value());
        EXPECT_FALSE(out->repaired.has_value());
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("network").get("max_retries").as_int(), 2);
    }
}

// A pre-existing 0600 user file keeps 0600 across the tmp+rename patch.
TEST(ConfigManagerUserSettings, Mode0600Preserved) {
    if (::getuid() == 0) GTEST_SKIP() << "modes are bypassed for root";
    C13Paths p("mode");
    c13_write_file(p.user_path, R"JSON({"network":{"max_retries":1}})JSON");
    fs::permissions(p.user_path,
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    ASSERT_TRUE(m.set_user_setting("network.max_retries", c13_parse("9"))
                    .has_value());
    const auto mode = fs::status(p.user_path).permissions() & fs::perms::mask;
    EXPECT_EQ(mode, fs::perms::owner_read | fs::perms::owner_write);
}

// $LOOM_CONFIG_DIR routes the write; only that one file is touched.
TEST(ConfigManagerUserSettings, ConfigDirRoutingOnlyUserTouched) {
    const auto suffix =
        std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() /
                      ("loom_c13_cfgdir_" + std::to_string(suffix));
    fs::create_directories(root);
    const auto fake_home = root / "home";
    const auto cfg_dir = root / "cfg";
    const auto work = root / "work";
    fs::create_directories(fake_home);
    fs::create_directories(cfg_dir);
    fs::create_directories(work);
    EnvironmentGuard home_guard("HOME", fake_home.string());
    EnvironmentGuard dir_guard("LOOM_CONFIG_DIR", cfg_dir.string());
    CurrentPathGuard cwd_guard(work);

    {
        cc::core::ConfigManager m;
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("network.max_retries", c13_parse("4"));
        ASSERT_TRUE(out.has_value());
        EXPECT_EQ(out->path, cfg_dir / "config.json");
    }
    EXPECT_TRUE(fs::exists(cfg_dir / "config.json"));
    EXPECT_FALSE(fs::exists(fake_home / ".loom"));
    EXPECT_FALSE(fs::exists(work / ".loom"));
    std::error_code ec;
    fs::remove_all(root, ec);
}

// Provenance: source env/file/default, the LOOM_MODEL shadow bit, and
// presence-only secret projection (credential bytes never serialized).
TEST(ConfigManagerUserSettings, ProvenanceShadowAndSecretPresence) {
    // Presence/provenance assertions are sensitive to ambient credential
    // env vars (including ANTHROPIC_AUTH_TOKEN, which counts as presence
    // for network.api_key); start hermetic. Inner EnvironmentGuards below
    // restore into the unset state, and these restore the shell at exit.
    EnvironmentUnsetGuard g_api("ANTHROPIC_API_KEY");
    EnvironmentUnsetGuard g_auth("ANTHROPIC_AUTH_TOKEN");
    EnvironmentUnsetGuard g_base("ANTHROPIC_BASE_URL");
    EnvironmentUnsetGuard g_http_proxy("HTTP_PROXY");
    EnvironmentUnsetGuard g_https_proxy("HTTPS_PROXY");

    C13Paths p("provenance");
    c13_write_file(p.user_path, R"JSON({
      "model": {"default_model": "file-model", "max_output_tokens": 111},
      "network": {"api_key": "SECRET-zx9w87-traceable-key",
                  "base_url": "https://SECRET-zx9w87.example.invalid/api"}
    })JSON");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto settings = c13_parse(m.serialize_agent_settings_json());
        EXPECT_EQ(std::string(settings.root.get("model").get("default_model")
                                  .get("source").as_str()), "file");
        EXPECT_EQ(std::string(settings.root.get("network").get("max_retries")
                                  .get("source").as_str()), "default");

        auto presence = m.agent_secret_presence_json("network.api_key");
        ASSERT_TRUE(presence.has_value());
        auto pv = c13_parse(*presence);
        EXPECT_EQ(pv.root.get("set").as_bool(), true);
        EXPECT_EQ(std::string(pv.root.get("source").as_str()), "file");
        EXPECT_EQ(presence->find("SECRET-zx9w87"), std::string::npos);

        auto url = m.agent_secret_presence_json("network.base_url");
        ASSERT_TRUE(url.has_value());
        EXPECT_EQ(url->find("SECRET-zx9w87"), std::string::npos);

        auto none = m.agent_secret_presence_json("network.proxy");
        ASSERT_TRUE(none.has_value());
        auto nv = c13_parse(*none);
        EXPECT_EQ(nv.root.get("set").as_bool(), false);
        EXPECT_EQ(std::string(nv.root.get("source").as_str()), "none");
    }

    // Env engagement flips source + shadow; the file write still happens.
    EnvironmentGuard model_guard("LOOM_MODEL", "env-model");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto token = m.agent_setting_value_json("model.default_model");
        ASSERT_TRUE(token.has_value());
        auto tv = c13_parse(*token);
        EXPECT_EQ(std::string(tv.root.get("source").as_str()), "env");
        EXPECT_EQ(std::string(tv.root.get("env_var").as_str()), "LOOM_MODEL");
        EXPECT_EQ(std::string(tv.root.get("value").as_str()), "env-model");

        auto out = m.set_user_setting("model.default_model",
                                      c13_parse(R"("written-model")"));
        ASSERT_TRUE(out.has_value());
        EXPECT_TRUE(out->shadowed);
        EXPECT_EQ(out->shadowed_by, "LOOM_MODEL");

        // The post-write reload keeps reporting the env value/source.
        auto after = c13_parse(
            *m.agent_setting_value_json("model.default_model"));
        EXPECT_EQ(std::string(after.root.get("source").as_str()), "env");
        EXPECT_EQ(std::string(after.root.get("value").as_str()), "env-model");
        std::ifstream f(p.user_path);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_NE(bytes.find("written-model"), std::string::npos);
    }

    // ANTHROPIC_API_KEY presence is env-sourced and never leaks bytes,
    // neither single-get nor get-all.
    EnvironmentGuard key_guard("ANTHROPIC_API_KEY",
                               "SECRET-zx9w87-traceable-key");
    EnvironmentGuard proxy_guard("HTTP_PROXY", "http://SECRET-zx9w87-proxy:3128");
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto presence = m.agent_secret_presence_json("network.api_key");
        ASSERT_TRUE(presence.has_value());
        auto pv = c13_parse(*presence);
        EXPECT_EQ(pv.root.get("set").as_bool(), true);
        EXPECT_EQ(std::string(pv.root.get("source").as_str()), "env");
        EXPECT_EQ(presence->find("SECRET-zx9w87"), std::string::npos);

        auto proxy = m.agent_secret_presence_json("network.proxy");
        ASSERT_TRUE(proxy.has_value());
        auto px = c13_parse(*proxy);
        EXPECT_EQ(px.root.get("set").as_bool(), true);
        EXPECT_EQ(std::string(px.root.get("source").as_str()), "env");
        EXPECT_EQ(proxy->find("SECRET-zx9w87"), std::string::npos);

        const std::string all = m.serialize_agent_settings_json();
        EXPECT_EQ(all.find("SECRET-zx9w87"), std::string::npos);
    }
}

// ANTHROPIC_MODEL adds the interactive-resolver source note.
TEST(ConfigManagerUserSettings, AnthropicModelSourceNote) {
    C13Paths p("note");
    EnvironmentGuard guard("ANTHROPIC_MODEL", "interactive-model");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    auto token = m.agent_setting_value_json("model.default_model");
    ASSERT_TRUE(token.has_value());
    EXPECT_NE(token->find("ANTHROPIC_DEFAULT_SONNET_MODEL"),
              std::string::npos);
}

// A section holding the wrong JSON type is replaced by an object on write.
TEST(ConfigManagerUserSettings, SectionWrongTypeReplaced) {
    C13Paths p("wrongtype");
    c13_write_file(p.user_path, R"JSON({"model":[1,2,3]})JSON");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    auto out = m.set_user_setting("model.default_model",
                                  c13_parse(R"("rebuilt")"));
    ASSERT_TRUE(out.has_value());
    auto root = c13_parse([&] { std::ifstream f(p.user_path);
        return std::string(std::istreambuf_iterator<char>(f),
                           std::istreambuf_iterator<char>()); }());
    ASSERT_TRUE(root.root.get("model").is_obj());
    EXPECT_EQ(root.root.get("model").get("default_model").as_str(),
              std::string_view("rebuilt"));
}

// Quiet load emits zero stderr bytes; a salvaged write clears the
// unparseable flag within the same instance; a corrupt other tier still
// succeeds with reload_warning.
TEST(ConfigManagerUserSettings, SilentLoadSalvageClearsFlagAndReloadWarning) {
    C13Paths p("silent");
    c13_write_file(p.user_path, "GARBAGE NOT JSON\n");
    {
        auto m = p.manager();
        testing::internal::CaptureStderr();
        ASSERT_TRUE(m.load(cc::core::LoadOptions{.quiet = true}).has_value());
        const std::string captured = testing::internal::GetCapturedStderr();
        EXPECT_TRUE(captured.empty()) << captured;
        EXPECT_TRUE(m.user_tier_unparseable());

        // Audible path (CLI/core loader) keeps the warning.
        auto audible = p.manager();
        testing::internal::CaptureStderr();
        ASSERT_TRUE(audible.load().has_value());
        EXPECT_FALSE(testing::internal::GetCapturedStderr().empty());

        // Salvaging write repairs the file and clears the flag (D4 reload).
        auto out = m.set_user_setting("network.max_retries", c13_parse("1"));
        ASSERT_TRUE(out.has_value());
        ASSERT_TRUE(out->repaired.has_value());
        EXPECT_EQ(*out->repaired, "replaced_unparseable");
        EXPECT_FALSE(m.user_tier_unparseable());
        auto fresh = p.manager();
        ASSERT_TRUE(fresh.load().has_value());
        EXPECT_FALSE(fresh.user_tier_unparseable());
        EXPECT_EQ(fresh.settings().network.max_retries, 1u);
    }

    // Corrupt PROJECT tier: the user write still succeeds; the post-write
    // quiet reload surfaces it as reload_warning without rolling back.
    c13_write_file(p.project_path, "BROKEN PROJECT JSON");
    {
        auto m = p.manager();
        // Pre-write load hard-fails on the project tier, as designed.
        EXPECT_FALSE(m.load(cc::core::LoadOptions{.quiet = true}).has_value());
        auto out = m.set_user_setting("network.max_retries", c13_parse("8"));
        ASSERT_TRUE(out.has_value()) << out.error().message;
        ASSERT_TRUE(out->reload_warning.has_value());
        EXPECT_NE(out->reload_warning->find("parse"), std::string::npos);
        EXPECT_TRUE(fs::exists(p.user_path));
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("network").get("max_retries").as_int(), 8);
    }
}

// The spec table is closed: exactly 16 projected keys, 7 writable, with
// coherent section/leaf/dotted tokens.
TEST(ConfigManagerUserSettings, SpecTableIsClosed) {
    const auto specs = cc::core::ConfigManager::user_setting_specs();
    EXPECT_EQ(specs.size(), 16u);
    int writable = 0;
    for (const auto& spec : specs) {
        EXPECT_EQ(spec.key,
                  std::string(spec.section) + "." +
                      std::string(spec.leaf));
        if (spec.writable) ++writable;
    }
    EXPECT_EQ(writable, 7);
    EXPECT_NE(cc::core::ConfigManager::find_user_setting("model.temperature"),
              nullptr);
    EXPECT_EQ(cc::core::ConfigManager::find_user_setting("nope.nope"),
              nullptr);
    EXPECT_TRUE(cc::core::ConfigManager::blocked_setting_message("xaaIdp")
                    .has_value());
    EXPECT_FALSE(cc::core::ConfigManager::blocked_setting_message("model.temperature")
                     .has_value());
}

// ===========================================================================
// RFC-0001 B followup c13c — hardened atomic writer + coercion/env/BOM nits.
// ===========================================================================

// Pre-placed config.json.tmp symlink must never be followed: the victim is
// untouched, config.json ends up a regular file, and the stale symlink is
// removed rather than left behind.
TEST(ConfigManagerUserSettings, AtomicWriterTmpSymlinkNeverFollowed) {
    C13Paths p("symlink");
    fs::create_directories(p.root);
    const auto victim = p.root / "victim.txt";
    constexpr std::string_view kCanary = "VICTIM-CANARY-c13c-7788";
    { std::ofstream(victim) << kCanary; }
    // The pre-c13c writer used the fixed suffix user_path + ".tmp";
    // pre-place that exact name as a symlink to the victim.
    const auto legacy_tmp = p.user_path.parent_path() /
                            (p.user_path.filename().string() + ".tmp");
    fs::remove(legacy_tmp);
    fs::create_symlink(victim, legacy_tmp);

    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    auto out = m.set_user_setting("network.max_retries", c13_parse("3"));
    ASSERT_TRUE(out.has_value()) << out.error().message;

    // Victim bytes unchanged.
    {
        std::ifstream f(victim);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(bytes, kCanary);
    }
    // config.json is a REGULAR file with the correct JSON.
    std::error_code ec;
    EXPECT_EQ(fs::symlink_status(p.user_path, ec).type(),
              fs::file_type::regular);
    auto root = c13_parse([&] { std::ifstream f(p.user_path);
        return std::string(std::istreambuf_iterator<char>(f),
                           std::istreambuf_iterator<char>()); }());
    EXPECT_EQ(root.root.get("network").get("max_retries").as_int(), 3);
    // No leftover attacker symlink.
    EXPECT_NE(fs::symlink_status(legacy_tmp, ec).type(),
              fs::file_type::symlink);
}

// A pre-created directory at the legacy tmp name is refused cleanly; a
// pre-created FIFO is treated as old crash debris and auto-unlinked so the
// write proceeds (c13d; regular/FIFO debris must not block post-upgrade).
TEST(ConfigManagerUserSettings, AtomicWriterTmpDirRefusedFifoCleared) {
    C13Paths p("tmpblock");
    const auto legacy_tmp = p.root /
                            (p.user_path.filename().string() + ".tmp");

    // Directory: refused, nothing clobbered.
    {
        std::error_code ec;
        fs::remove_all(legacy_tmp, ec);
        fs::create_directories(legacy_tmp, ec);
        ASSERT_FALSE(ec);
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("network.max_retries", c13_parse("3"));
        EXPECT_FALSE(out.has_value());
        EXPECT_EQ(fs::symlink_status(legacy_tmp).type(),
                  fs::file_type::directory);
        fs::remove_all(legacy_tmp, ec);
    }

    // FIFO: treated as stale debris, removed, write succeeds (no blocking
    // open).
    {
        ASSERT_EQ(::mkfifo(legacy_tmp.c_str(), 0600), 0);
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        auto out = m.set_user_setting("network.max_retries", c13_parse("3"));
        EXPECT_TRUE(out.has_value()) << out.error().message;
        EXPECT_FALSE(fs::exists(legacy_tmp));
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("network").get("max_retries").as_int(), 3);
    }
}

// 20 parallel processes hammering distinct settings on one config dir:
// every child either writes parse-valid JSON or fails cleanly, no torn
// files, no fixed-tmp "Failed to replace" collisions; the final file is
// valid and holds the last successful value for each key.
TEST(ConfigManagerUserSettings, ConcurrentSetProcessesNoCollisions) {
    C13Paths p("concurrent");
    constexpr int kProcesses = 20;
    constexpr std::array<std::string_view, 7> keys = {{
        "network.max_retries",
        "model.max_output_tokens",
        "model.context_window_size",
        "model.thinking_budget",
        "model.extended_thinking",
        "model.temperature",
        "model.default_model",
    }};
    std::array<std::string, keys.size()> last_values{{
        "100", "20000", "300000", "4096", "true", "0.5", "concurrent-model"
    }};
    std::vector<pid_t> children;
    for (int i = 0; i < kProcesses; ++i) {
        pid_t pid = ::fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            const auto& key = keys[static_cast<std::size_t>(i % keys.size())];
            auto m = p.manager();
            (void)m.load();
            const std::string value = last_values[i % keys.size()];
            const std::string payload =
                key == "model.default_model"
                    ? std::format("\"{}\"", value)
                    : value;
            auto out = m.set_user_setting(key, c13_parse(payload));
            _exit(out.has_value() ? 0 : 2);
        }
        children.push_back(pid);
    }
    int clean_failures = 0;
    for (pid_t pid : children) {
        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFEXITED(status));
        const int code = WEXITSTATUS(status);
        ASSERT_NE(code, 2) << "child reported a write-level error";
        if (code != 0) ++clean_failures;
    }
    // At least the seven keys' final writes must land (single wave, unique
    // tmp names — expected zero failures).
    EXPECT_EQ(clean_failures, 0);

    auto doc = cc::utils::json::parse_file(p.user_path);
    ASSERT_TRUE(doc.has_value());
    const auto root = doc->root();
    ASSERT_TRUE(root.is_obj());
    EXPECT_EQ(root.get("network").get("max_retries").as_int(), 100);
    EXPECT_EQ(root.get("model").get("max_output_tokens").as_int(), 20000);
    EXPECT_EQ(root.get("model").get("context_window_size").as_int(), 300000);
    EXPECT_EQ(root.get("model").get("thinking_budget").as_int(), 4096);
    EXPECT_EQ(root.get("model").get("extended_thinking").as_bool(), true);
    EXPECT_DOUBLE_EQ(root.get("model").get("temperature").as_double(), 0.5);
    EXPECT_EQ(root.get("model").get("default_model").as_str(),
              std::string_view("concurrent-model"));

    // No stale tmp debris (fixed or unique-named) remains.
    for (const auto& entry : fs::directory_iterator(p.root)) {
        const auto name = entry.path().filename().string();
        EXPECT_EQ(name.find(".tmp"), std::string::npos) << name;
    }
}

// Native JSON numbers that are integral doubles within uint32 range are
// accepted; fractional/negative/overflow values rejected (D7).
TEST(ConfigManagerUserSettings, IntegralDoubleCoercion) {
    C13Paths p("intdouble");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    ASSERT_TRUE(m.set_user_setting("model.max_output_tokens", c13_parse("4096.0"))
                    .has_value());
    ASSERT_TRUE(m.set_user_setting("model.context_window_size", c13_parse("100000.0"))
                    .has_value());
    ASSERT_TRUE(m.set_user_setting("network.max_retries", c13_parse("0.0"))
                    .has_value());
    ASSERT_TRUE(m.set_user_setting("model.thinking_budget", c13_parse("2048.0"))
                    .has_value());

    for (const auto* bad : {"4096.5", "-1.0", "4294967296.0", "1e40"}) {
        EXPECT_FALSE(m.set_user_setting("model.max_output_tokens",
                                        c13_parse(bad)).has_value()) << bad;
    }
    auto reloaded = p.manager();
    ASSERT_TRUE(reloaded.load().has_value());
    EXPECT_EQ(reloaded.settings().model.max_output_tokens, 4096u);
    EXPECT_EQ(reloaded.settings().model.context_window_size, 100000u);
    EXPECT_EQ(reloaded.settings().network.max_retries, 0u);
    EXPECT_EQ(*reloaded.settings().model.thinking_budget, 2048u);
}

// Temperature TEXT parsing must keep std::from_chars(chars_format::general)
// strictness on the portable strtod path: hex floats, NaN/Inf words,
// overflow, and trailing junk are rejected at PARSE time; otherwise-well
// formed out-of-range values are rejected later by the [0,1] check instead.
TEST(ConfigManagerUserSettings, TemperatureTextStrictDoublePortable) {
    C13Paths p("strictdouble");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    auto try_text = [&](const char* text) {
        return m.set_user_setting(
            "model.temperature",
            c13_parse(std::format("\"{}\"", text)));
    };

    // Parser rejects (naive strtod would accept the hex float and the
    // nan/inf words): must fail at the finite-number parse step.
    const char* parse_rejected[] = {
        "0x1p4", "nan", "inf", "infinity", "-inf",
        "NAN", "Inf", "1e309", "-1e309", "0.5x",
        "1.2.3", "e5", "..5", "1e", "+", "-",
    };
    for (const char* bad : parse_rejected) {
        auto out = try_text(bad);
        ASSERT_FALSE(out.has_value()) << bad;
        EXPECT_NE(out.error().message.find("finite number"),
                  std::string::npos)
            << bad << ": " << out.error().message;
    }

    // Well-formed doubles that only fail the [0,1] range prove the parser
    // accepted the spelling (error comes from the range check, not parse).
    const char* range_rejected[] = {
        "-.5", "-0.25", "1e2", "1E2",
    };
    for (const char* text : range_rejected) {
        auto out = try_text(text);
        ASSERT_FALSE(out.has_value()) << text;
        EXPECT_NE(out.error().message.find("between 0 and 1"),
                  std::string::npos)
            << text << ": " << out.error().message;
    }

    // In-range spellings fully accepted and verified by a real reload.
    for (const auto& [text, expected] :
         std::vector<std::pair<const char*, double>>{
             {"  0.5", 0.5}, {"0.5", 0.5}, {"+.5", 0.5}, {".5", 0.5},
             {"0.0", 0.0}, {"1.5e-1", 0.15}, {"1e-2", 0.01},
             {"+1", 1.0}, {"1.", 1.0}}) {
        auto out = try_text(text);
        ASSERT_TRUE(out.has_value())
            << text << ": " << (out ? "" : out.error().message);
        auto check = p.manager();
        ASSERT_TRUE(check.load().has_value());
        EXPECT_DOUBLE_EQ(*check.settings().model.temperature, expected)
            << text;
    }
}

// LOOM_MAX_TOKENS engagement is ONE range-checked predicate for both get
// provenance and set shadow disclosure: engaged iff digits parse into
// [1, uint32_max]. Returns {source_env, shadowed, effective_value}.
TEST(ConfigManagerUserSettings, MaxTokensEnvEngagementConsistent) {
    C13Paths p("envrange");
    c13_write_file(p.user_path,
                   R"JSON({"model":{"max_output_tokens": 777}})JSON");

    struct Observation {
        bool source_env;
        bool shadowed;
        std::uint32_t effective;
    };
    const auto observe = [&](const char* raw) {
        EnvironmentGuard guard("LOOM_MAX_TOKENS", raw);
        auto m = p.manager();
        EXPECT_TRUE(m.load().has_value());
        auto token = c13_parse(
            *m.agent_setting_value_json("model.max_output_tokens"));
        const bool source_env =
            std::string(token.root.get("source").as_str()) == "env";
        auto out = m.set_user_setting("model.max_output_tokens",
                                      c13_parse("888"));
        EXPECT_TRUE(out.has_value());
        // The post-write quiet reload re-applies the same env predicate, so
        // the effective value is read while the guard is still live.
        return Observation{
            source_env, out->shadowed, m.settings().model.max_output_tokens};
    };

    for (const char* bad : {"4294967296", "18446744073709551616", "abc",
                            "0", "-1", "12x"}) {
        auto o = observe(bad);
        EXPECT_FALSE(o.source_env) << bad;
        EXPECT_FALSE(o.shadowed) << bad;
        EXPECT_EQ(o.effective, 888u) << bad;  // file write is effective
    }
    {
        auto o = observe("12345");
        EXPECT_TRUE(o.source_env);
        EXPECT_TRUE(o.shadowed);
        // Effective value comes from the env; the 888 file write is
        // shadowed while the override is engaged.
        EXPECT_EQ(o.effective, 12345u);
    }
    // With the env unset again the last file write is effective.
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        EXPECT_EQ(m.settings().model.max_output_tokens, 888u);
    }
}

// A leading UTF-8 BOM is stripped in both tier load and the patcher:
// a clean BOM object loads normally; BOM+object+junk is salvaged with the
// leading object's keys preserved.
TEST(ConfigManagerUserSettings, BomStrippedOnLoadAndPatch) {
    C13Paths p("bom");
    // BOM + complete leading object (carrying a sibling leaf) + trailing
    // junk. The junk keeps the §A soft-load skip; the salvage must recover
    // the leading object after stripping the BOM.
    {
        std::ofstream f(p.user_path, std::ios::binary);
        f << "\xEF\xBB\xBF"
          << R"({"network":{"max_retries":4},)"
          << R"("model":{"default_model":"bom-kept"}} TRAIL JUNK)";
    }
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());  // soft tier tolerated
        auto out = m.set_user_setting("network.max_retries", c13_parse("5"));
        ASSERT_TRUE(out.has_value());
        ASSERT_TRUE(out->repaired.has_value());
        EXPECT_EQ(*out->repaired, "trailing_junk_dropped");
        auto repaired = cc::utils::json::parse_file(p.user_path);
        ASSERT_TRUE(repaired.has_value());
        const auto root = repaired->root();
        EXPECT_EQ(root.get("network").get("max_retries").as_int(), 5);
        EXPECT_EQ(root.get("model").get("default_model").as_str(),
                  std::string_view("bom-kept"));
    }
    {
        std::ofstream f(p.user_path, std::ios::binary);
        f << "\xEF\xBB\xBF" << R"({"network":{"max_retries":6}})";
    }
    {
        auto m = p.manager();
        ASSERT_TRUE(m.load().has_value());
        EXPECT_EQ(m.settings().network.max_retries, 6u);
        auto out = m.set_user_setting("network.max_retries", c13_parse("7"));
        ASSERT_TRUE(out.has_value());
        EXPECT_FALSE(out->repaired.has_value());
        auto reloaded = p.manager();
        ASSERT_TRUE(reloaded.load().has_value());
        EXPECT_EQ(reloaded.settings().network.max_retries, 7u);
    }
}

// ===========================================================================
// RFC-0001 B followup c13d — lockfile gitignore, bounded lock, full-save
// hardening, legacy-tmp/random-name attacks.
// ===========================================================================
namespace {

// Scratch git repo for gitignore tests; skipped when git is unavailable.
struct C13GitRepo {
    fs::path root;
    fs::path loom;
    fs::path outside;
    std::unique_ptr<CurrentPathGuard> cwd_guard;
    bool available = false;

    explicit C13GitRepo(std::string_view tag)
        : root(fs::temp_directory_path() /
               (std::string("loom_c13_git_") + std::string(tag) + "_" +
                std::to_string(std::chrono::system_clock::now()
                                   .time_since_epoch().count()))) {
        if (std::system("git --version >/dev/null 2>&1") != 0) return;
        fs::create_directories(root);
        loom = root / ".loom";
        outside = root / "outside";
        fs::create_directories(loom);
        fs::create_directories(outside);
        if (std::system(("git -C \"" + root.string() +
                         "\" init -q --initial-branch main")
                            .c_str()) != 0) {
            return;
        }
        std::system(("git -C \"" + root.string() +
                     "\" config user.email t@example.invalid").c_str());
        std::system(("git -C \"" + root.string() +
                     "\" config user.name c13d").c_str());
        // Enter the repo only after it exists.
        cwd_guard = std::make_unique<CurrentPathGuard>(root);
        available = true;
    }
    ~C13GitRepo() {
        // Restore cwd before removing the tree.
        cwd_guard.reset();
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    void git(std::string_view cmd) const {
        ASSERT_EQ(std::system(std::string(cmd).c_str()), 0) << cmd;
    }

    [[nodiscard]] std::string porcelain() const {
        const auto out = root / "porcelain.txt";
        const std::string cmd =
            "git status --porcelain=v1 > " + out.string() + " 2>/dev/null";
        std::system(cmd.c_str());
        std::ifstream f(out);
        return std::string((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    }

    [[nodiscard]] cc::core::ConfigManager manager() const {
        return cc::core::ConfigManager(outside / "global.json",
                                       outside / "user.json",
                                       loom / "config.json",
                                       loom / "config.local.json");
    }
};

bool c13_git_available() {
    return std::system("git --version >/dev/null 2>&1") == 0;
}

} // namespace

// Local-tier write: data file AND lock ignored, .gitignore tracked.
TEST(ConfigManagerC13d, LocalWriteIgnoresDataAndLockTracksGitignore) {
    if (!c13_git_available()) GTEST_SKIP() << "git not available";
    C13GitRepo repo("local");
    ASSERT_TRUE(repo.available);

    cc::core::McpServerConfig cfg;
    cfg.name = "srv";
    cfg.transport = "stdio";
    cfg.command = "node";
    {
        auto m = repo.manager();
        ASSERT_TRUE(m.load().has_value());
        ASSERT_TRUE(m.upsert_mcp_server(cc::core::McpStorageScope::Local, cfg)
                        .has_value());
    }
    EXPECT_TRUE(fs::exists(repo.loom / "config.local.json"));
    EXPECT_TRUE(fs::exists(repo.loom / "config.local.json.lock"));

    repo.git("git add -A");
    const auto status = repo.porcelain();
    EXPECT_NE(status.find(".gitignore"), std::string::npos);
    EXPECT_EQ(status.find("config.local.json"), std::string::npos) << status;
    EXPECT_EQ(status.find("config.local.json.lock"), std::string::npos)
        << status;
}

// Project-tier save: config.json TRACKED, only the lock is ignored.
TEST(ConfigManagerC13d, ProjectSaveTracksDataIgnoresLock) {
    if (!c13_git_available()) GTEST_SKIP() << "git not available";
    C13GitRepo repo("project");
    ASSERT_TRUE(repo.available);

    {
        auto m = repo.manager();
        ASSERT_TRUE(m.load().has_value());
        ASSERT_TRUE(m.save(cc::core::ConfigSource::ProjectConfig).has_value());
    }
    EXPECT_TRUE(fs::exists(repo.loom / "config.json"));
    EXPECT_TRUE(fs::exists(repo.loom / "config.json.lock"));

    repo.git("git add -A");
    const auto status = repo.porcelain();
    EXPECT_NE(status.find(".loom/config.json"), std::string::npos) << status;
    EXPECT_EQ(status.find("config.json.lock"), std::string::npos) << status;

    // The .gitignore rule names the lock, never the data file.
    std::ifstream gi(repo.root / ".gitignore");
    const std::string text((std::istreambuf_iterator<char>(gi)),
                           std::istreambuf_iterator<char>());
    EXPECT_NE(text.find("config.json.lock"), std::string::npos);
    EXPECT_EQ(text.find("\nconfig.json\n"), std::string::npos);
}

// Repeated local/project writes never duplicate ignore lines.
TEST(ConfigManagerC13d, GitignoreLinesAreIdempotent) {
    if (!c13_git_available()) GTEST_SKIP() << "git not available";
    C13GitRepo repo("idem");
    ASSERT_TRUE(repo.available);

    cc::core::McpServerConfig cfg;
    cfg.name = "srv";
    cfg.transport = "stdio";
    cfg.command = "node";
    auto m = repo.manager();
    ASSERT_TRUE(m.load().has_value());
    for (int i = 0; i < 3; ++i) {
        cfg.name = std::string("srv") + std::to_string(i);
        ASSERT_TRUE(m.upsert_mcp_server(cc::core::McpStorageScope::Local, cfg)
                        .has_value());
    }
    ASSERT_TRUE(m.save(cc::core::ConfigSource::ProjectConfig).has_value());
    ASSERT_TRUE(m.save(cc::core::ConfigSource::ProjectConfig).has_value());

    const std::string text = [] {
        std::ifstream f(fs::current_path() / ".gitignore");
        return std::string((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    }();
    EXPECT_EQ(c6_count_occurrences(text, "config.local.json\n"), 1u) << text;
    EXPECT_EQ(c6_count_occurrences(text, "config.local.json.lock\n"), 1u)
        << text;
    EXPECT_EQ(c6_count_occurrences(text, "config.json.lock\n"), 1u) << text;
}

// User/global tier writes create no .gitignore at all.
TEST(ConfigManagerC13d, UserGlobalWritesNoGitignore) {
    if (!c13_git_available()) GTEST_SKIP() << "git not available";
    C13GitRepo repo("user");
    ASSERT_TRUE(repo.available);

    {
        auto m = repo.manager();
        ASSERT_TRUE(m.load().has_value());
        ASSERT_TRUE(m.set_user_setting("network.max_retries", c13_parse("2"))
                        .has_value());
    }
    EXPECT_FALSE(fs::exists(repo.root / ".gitignore"));
    // The user-tier write lives outside the project tree; .loom stays
    // empty (it is a pre-created empty directory from the fixture).
    EXPECT_EQ(std::distance(fs::directory_iterator(repo.loom),
                            fs::directory_iterator()), 0);
    EXPECT_TRUE(fs::exists(repo.outside / "user.json"));
}

// A 2s external lock holder makes the writer WAIT and then succeed; a
// long holder trips the ~10s WALL-CLOCK bound with a clean, specific
// error and leaves the file and directory untouched. Holder readiness is
// a pipe handshake — the child signals only AFTER its flock succeeds — so
// no fixed sleep guesses child readiness. That guess (plus an
// attempt-counted "bound") made the old version acquire at ~13s under a
// loaded 3-vCPU mac runner instead of timing out at 10s.
TEST(ConfigManagerC13d, BoundedLockWaitSuccessThenTimeout) {
    C13Paths p("lockwait");
    const auto lock_path = p.user_path.string() + ".lock";

    struct Holder {
        pid_t pid;
        int read_fd;
    };
    // Kills+reaps a still-held holder on scope exit, so an early ASSERT_*
    // failure (including a failed readiness handshake) can never leak a
    // forked child holding the lock. The normal path reaps explicitly and
    // disarms the guard with release().
    struct HolderReaper {
        pid_t pid = -1;
        HolderReaper() = default;
        explicit HolderReaper(pid_t child) : pid(child) {}
        HolderReaper(const HolderReaper&) = delete;
        HolderReaper& operator=(const HolderReaper&) = delete;
        ~HolderReaper() {
            if (pid > 0) {
                (void)::kill(pid, SIGKILL);
                int status = 0;
                (void)::waitpid(pid, &status, 0);
            }
        }
        void release() { pid = -1; }
    };
    // Fork a child that takes LOCK_EX on lock_path, then writes one byte
    // to the pipe only once the flock is actually held ('R'; 'E' if the
    // open/flock failed), so the parent can block on read() instead of
    // guessing readiness with a sleep. Plain ::pipe() — pipe2() is
    // Linux-only and this test must stay macOS-portable.
    auto spawn_holder = [&](int seconds) -> Holder {
        int pipefd[2];
        if (::pipe(pipefd) != 0) {
            ADD_FAILURE() << "pipe: " << std::strerror(errno);
            return {-1, -1};
        }
        const pid_t pid = ::fork();
        if (pid < 0) {
            ADD_FAILURE() << "fork: " << std::strerror(errno);
            ::close(pipefd[0]);
            ::close(pipefd[1]);
            return {-1, -1};
        }
        if (pid == 0) {
            ::close(pipefd[0]);
            const int fd = ::open(lock_path.c_str(),
                                  O_RDWR | O_CREAT | O_CLOEXEC, 0600);
            const char fail = 'E';
            if (fd < 0) {
                (void)::write(pipefd[1], &fail, 1);
                _exit(10);
            }
            if (::flock(fd, LOCK_EX) != 0) {
                (void)::write(pipefd[1], &fail, 1);
                _exit(11);
            }
            const char ready = 'R';
            if (::write(pipefd[1], &ready, 1) != 1) _exit(12);
            for (int i = 0; i < seconds * 10; ++i) {
                struct timespec ts{0, 100 * 1000 * 1000};
                ::nanosleep(&ts, nullptr);
            }
            _exit(0);
        }
        ::close(pipefd[1]);  // parent's only handle is the read end
        return {pid, pipefd[0]};
    };
    // Block until the holder child has reported its flock. ASSERT (not
    // EXPECT): an 'E'/short read aborts the phase, and the armed
    // HolderReaper reaps the child on the way out.
    auto await_ready = [&](int fd) {
        char b = 0;
        ssize_t n = 0;
        do {
            n = ::read(fd, &b, 1);
        } while (n == -1 && errno == EINTR);
        ASSERT_EQ(n, 1);
        ASSERT_EQ(b, 'R') << "holder child failed to acquire the lock";
        ::close(fd);
    };

    // Short holder: wait then succeed.
    {
        Holder h = spawn_holder(2);
        ASSERT_GT(h.pid, 0);
        HolderReaper reaper(h.pid);
        await_ready(h.read_fd);
        auto m = p.manager();
        const auto start = std::chrono::steady_clock::now();
        auto out = m.set_user_setting("network.max_retries", c13_parse("1"));
        const auto elapsed = std::chrono::duration_cast<
            std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
        ASSERT_TRUE(out.has_value()) << (out ? "" : out.error().message);
        EXPECT_GE(elapsed, 1000) << "writer should have waited on the lock";
        int status = 0;
        ASSERT_EQ(::waitpid(h.pid, &status, 0), h.pid);
        reaper.release();  // holder exited and is reaped
        auto fresh = p.manager();
        ASSERT_TRUE(fresh.load().has_value());
        EXPECT_EQ(fresh.settings().network.max_retries, 1u);
    }

    // Long holder (30s nominal — well past the 10s bound even with
    // scheduler-stalled sleeps): bounded wait fails closed with the
    // specific message.
    {
        Holder h = spawn_holder(30);
        ASSERT_GT(h.pid, 0);
        HolderReaper reaper(h.pid);
        await_ready(h.read_fd);
        c13_write_file(p.user_path, R"JSON({"network":{"max_retries":9}})JSON");
        auto m = p.manager();
        const auto start = std::chrono::steady_clock::now();
        auto out = m.set_user_setting("network.max_retries", c13_parse("7"));
        const auto elapsed = std::chrono::duration_cast<
            std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
        ASSERT_FALSE(out.has_value());
        EXPECT_NE(out.error().message.find("another Loom process is updating"),
                  std::string::npos);
        EXPECT_NE(out.error().message.find(lock_path), std::string::npos);
        EXPECT_GE(elapsed, 9000) << "should wait up to the ~10s bound";
        EXPECT_LE(elapsed, 14000) << "deadline must hold under scheduler load";
        ::kill(h.pid, SIGKILL);
        int status = 0;
        ASSERT_EQ(::waitpid(h.pid, &status, 0), h.pid);
        reaper.release();  // holder killed and reaped

        // File unmodified; no tmp debris of any kind.
        auto doc = cc::utils::json::parse_file(p.user_path);
        ASSERT_TRUE(doc.has_value());
        EXPECT_EQ(doc->root().get("network").get("max_retries").as_int(), 9);
        for (const auto& entry : fs::directory_iterator(p.root)) {
            const auto name = entry.path().filename().string();
            EXPECT_EQ(name.find(".tmp."), std::string::npos) << name;
            EXPECT_NE(name, "user.json.tmp") << name;
        }
    }
}

// ===========================================================================
// RFC-0001 B followup c14 — ScopedInboxLock hard wall-clock deadline and
// O_NOFOLLOW. The old loop advertised "200 attempts ~10s" but called a
// BLOCKING flock(LOCK_EX): while contended it never returns EWOULDBLOCK, so
// the counter advanced only on EINTR and the wait was effectively unbounded.
// These tests pin the same steady_clock deadline pattern/constants as
// ConfigFileLock (c13i) and refusal of a symlinked lock name.
// ===========================================================================
namespace {

// Hermetic team runtime root: every inbox path resolves under it via
// LOOM_TEAM_RUNTIME_DIR, so no test ever touches the repo cwd's .loom tree.
struct C14TeamEnv {
    fs::path root;
    EnvironmentGuard team_dir_guard;

    C14TeamEnv()
        : root([] {
              static std::atomic<unsigned> counter{0};
              const auto suffix =
                  std::chrono::system_clock::now().time_since_epoch().count();
              return fs::temp_directory_path() /
                     (std::string("loom_c14_") +
                      std::to_string(::getpid()) + "_" +
                      std::to_string(counter.fetch_add(
                          1, std::memory_order_relaxed)) +
                      "_" + std::to_string(suffix));
          }()),
          team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string()) {
        fs::create_directories(root);
    }
    ~C14TeamEnv() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
};

[[nodiscard]] fs::path c14_inbox_path(
    std::string_view agent = "worker",
    std::string_view team = "c14team") {
    return fs::path{cc::utils::get_inbox_path(
        agent, std::optional<std::string_view>{team})};
}

[[nodiscard]] std::string c14_read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

struct C14Holder {
    pid_t pid;
    int read_fd;
};

// Kills+reaps a still-held holder on scope exit, so an early ASSERT_* can
// never leak a forked child holding the lock. Mirrors the c13i reaper.
struct C14HolderReaper {
    pid_t pid = -1;
    C14HolderReaper() = default;
    explicit C14HolderReaper(pid_t child) : pid(child) {}
    C14HolderReaper(const C14HolderReaper&) = delete;
    C14HolderReaper& operator=(const C14HolderReaper&) = delete;
    ~C14HolderReaper() {
        if (pid > 0) {
            (void)::kill(pid, SIGKILL);
            int status = 0;
            (void)::waitpid(pid, &status, 0);
        }
    }
    void release() { pid = -1; }
};

// Fork a child that takes LOCK_EX on lock_path, then writes one byte to the
// pipe only once the flock is actually held ('R'; 'E' on open/flock
// failure), so the parent blocks on read() instead of guessing readiness
// with a sleep. Plain ::pipe() for macOS portability (no pipe2).
[[nodiscard]] C14Holder c14_spawn_holder(const std::string& lock_path,
                                         int seconds) {
    int pipefd[2];
    if (::pipe(pipefd) != 0) {
        ADD_FAILURE() << "pipe: " << std::strerror(errno);
        return {-1, -1};
    }
    const pid_t pid = ::fork();
    if (pid < 0) {
        ADD_FAILURE() << "fork: " << std::strerror(errno);
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        return {-1, -1};
    }
    if (pid == 0) {
        ::close(pipefd[0]);
        const int fd = ::open(lock_path.c_str(),
                              O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        const char fail = 'E';
        if (fd < 0) {
            (void)::write(pipefd[1], &fail, 1);
            _exit(10);
        }
        if (::flock(fd, LOCK_EX) != 0) {
            (void)::write(pipefd[1], &fail, 1);
            _exit(11);
        }
        const char ready = 'R';
        if (::write(pipefd[1], &ready, 1) != 1) _exit(12);
        for (int i = 0; i < seconds * 10; ++i) {
            struct timespec ts{0, 100 * 1000 * 1000};
            ::nanosleep(&ts, nullptr);
        }
        _exit(0);
    }
    ::close(pipefd[1]);  // parent's only handle is the read end
    return {pid, pipefd[0]};
}

// Block until the holder child has reported its flock; ASSERT on any
// handshake failure so the armed reaper cleans the child up.
void c14_await_ready(int fd) {
    char b = 0;
    ssize_t n = 0;
    do {
        n = ::read(fd, &b, 1);
    } while (n == -1 && errno == EINTR);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(b, 'R') << "holder child failed to acquire the lock";
    ::close(fd);
}

[[nodiscard]] long long c14_elapsed_ms(
    const std::chrono::steady_clock::time_point& start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start)
        .count();
}

}  // namespace

// A 2s external holder makes acquisition WAIT (blocking-style behavior) and
// then succeed once the holder exits and the kernel releases the flock.
TEST(ScopedInboxLockC14, BoundedWaitSucceedsAfterHolderReleases) {
    C14TeamEnv env;
    const auto inbox = c14_inbox_path();
    fs::create_directories(inbox.parent_path());
    const std::string lock_path = inbox.string() + ".lock";

    C14Holder h = c14_spawn_holder(lock_path, 2);
    ASSERT_GT(h.pid, 0);
    C14HolderReaper reaper(h.pid);
    c14_await_ready(h.read_fd);

    const auto start = std::chrono::steady_clock::now();
    cc::utils::ScopedInboxLock lock(inbox);
    const auto elapsed = c14_elapsed_ms(start);
    ASSERT_TRUE(lock.locked());
    EXPECT_GE(elapsed, 1000) << "acquisition should wait on the holder";
    EXPECT_LE(elapsed, 6000)
        << "lock should be granted shortly after the 2s holder exits";

    int status = 0;
    ASSERT_EQ(::waitpid(h.pid, &status, 0), h.pid);
    reaper.release();  // holder exited and is reaped
}

// A 30s holder trips the ~10s WALL-CLOCK bound: direct construction reports
// locked()==false within [9s, 14s], and the production guarded mailbox
// writer fails closed in the same contention window without touching the
// inbox. Both contenders run concurrently so the suite pays one 10s wait,
// not two; holder readiness is the pipe handshake, never a sleep guess.
TEST(ScopedInboxLockC14, BoundedWaitTimesOutAndMailboxWriteFailsClosed) {
    C14TeamEnv env;
    const auto inbox = c14_inbox_path();
    fs::create_directories(inbox.parent_path());
    const std::string canary =
        R"JSON([{"from":"lead","text":"keep","timestamp":"1","read":true}])JSON";
    {
        std::ofstream out(inbox, std::ios::binary);
        out << canary;
    }
    const std::string lock_path = inbox.string() + ".lock";

    C14Holder h = c14_spawn_holder(lock_path, 30);
    ASSERT_GT(h.pid, 0);
    C14HolderReaper reaper(h.pid);
    c14_await_ready(h.read_fd);

    struct DirectResult {
        bool locked = true;
        long long elapsed_ms = 0;
    } direct;
    std::thread contender([&] {
        const auto t0 = std::chrono::steady_clock::now();
        cc::utils::ScopedInboxLock lock(inbox);
        direct.elapsed_ms = c14_elapsed_ms(t0);
        direct.locked = lock.locked();
    });

    const auto write_start = std::chrono::steady_clock::now();
    auto written = cc::utils::write_to_mailbox(
        "worker",
        cc::utils::TeammateMessage{
            .from = "lead",
            .text = "must-not-land",
            .timestamp = "2",
            .read = false,
            .color = std::nullopt,
            .summary = std::nullopt,
        },
        std::optional<std::string_view>{"c14team"});
    const auto write_elapsed = c14_elapsed_ms(write_start);
    contender.join();

    EXPECT_FALSE(direct.locked);
    EXPECT_GE(direct.elapsed_ms, 9000) << "should wait up to the ~10s bound";
    EXPECT_LE(direct.elapsed_ms, 14000)
        << "deadline must hold under scheduler load";

    ASSERT_FALSE(written.has_value());
    EXPECT_NE(written.error().find("lock"), std::string::npos)
        << written.error();
    EXPECT_GE(write_elapsed, 9000) << "guarded writer should share the bound";
    EXPECT_LE(write_elapsed, 14000) << "guarded writer must hold the deadline";

    ::kill(h.pid, SIGKILL);
    int status = 0;
    ASSERT_EQ(::waitpid(h.pid, &status, 0), h.pid);
    reaper.release();  // holder killed and reaped

    // Inbox bytes unmodified; only the inbox and its lock file exist.
    EXPECT_EQ(c14_read_file(inbox), canary);
    bool saw_lock = false;
    for (const auto& entry :
         fs::directory_iterator(inbox.parent_path())) {
        const auto name = entry.path().filename().string();
        if (name == "worker.json") continue;
        if (name == "worker.json.lock") {
            saw_lock = true;
            continue;
        }
        ADD_FAILURE() << "unexpected inbox-dir debris: " << name;
    }
    EXPECT_TRUE(saw_lock);
}

// A pre-placed symlink at the lock name fails the O_NOFOLLOW open
// immediately: locked()==false, the guarded writer fails closed, and neither
// the symlink nor its target is ever followed or modified.
TEST(ScopedInboxLockC14, SymlinkedLockNameRejectedAndTargetUntouched) {
    C14TeamEnv env;
    const auto inbox = c14_inbox_path();
    fs::create_directories(inbox.parent_path());
    const fs::path lock_path = fs::path{inbox} += ".lock";
    const auto canary_path = env.root / "canary.txt";
    const std::string canary = "symlink-target-canary\n";
    {
        std::ofstream out(canary_path, std::ios::binary);
        out << canary;
    }
    fs::create_symlink(canary_path, lock_path);
    ASSERT_TRUE(fs::is_symlink(lock_path));

    {
        cc::utils::ScopedInboxLock lock(inbox);
        EXPECT_FALSE(lock.locked());
    }
    EXPECT_TRUE(fs::is_symlink(lock_path))
        << "failed open must not unlink or replace the symlink";
    EXPECT_EQ(c14_read_file(canary_path), canary);

    // The production guarded writer fails closed on the same lock name,
    // without a deadline wait and without creating the inbox.
    auto written = cc::utils::write_to_mailbox(
        "worker",
        cc::utils::TeammateMessage{
            .from = "lead",
            .text = "must-not-land",
            .timestamp = "2",
            .read = false,
            .color = std::nullopt,
            .summary = std::nullopt,
        },
        std::optional<std::string_view>{"c14team"});
    ASSERT_FALSE(written.has_value());
    EXPECT_NE(written.error().find("lock"), std::string::npos)
        << written.error();
    EXPECT_TRUE(fs::is_symlink(lock_path));
    EXPECT_EQ(c14_read_file(canary_path), canary);
    EXPECT_FALSE(fs::exists(inbox))
        << "failed lock acquisition must bail before any inbox write";
}

// RFC-0001 B followup c15 — a pre-placed FIFO at the lock name opens
// successfully (O_RDWR on a FIFO needs no reader/writer peer) and flock(2)
// works on any fd type, so without fstat()+S_ISREG the lock would be taken
// on a FIFO instead of rejected. Acquisition must fail FAST (no deadline
// wait), the guarded mailbox writer must fail closed, and the FIFO itself
// must be left in place.
TEST(ScopedInboxLockC15, FifoLockNameRejectedWithoutBlocking) {
    C14TeamEnv env;
    const auto inbox = c14_inbox_path();
    fs::create_directories(inbox.parent_path());
    const fs::path lock_path = fs::path{inbox} += ".lock";
    ASSERT_EQ(::mkfifo(lock_path.c_str(), 0600), 0);

    const auto direct_start = std::chrono::steady_clock::now();
    bool direct_locked = true;
    {
        cc::utils::ScopedInboxLock lock(inbox);
        direct_locked = lock.locked();
    }
    const auto direct_elapsed = c14_elapsed_ms(direct_start);
    EXPECT_FALSE(direct_locked);
    EXPECT_LT(direct_elapsed, 3000)
        << "non-regular lock name must fail fast, never await the deadline";

    const auto write_start = std::chrono::steady_clock::now();
    auto written = cc::utils::write_to_mailbox(
        "worker",
        cc::utils::TeammateMessage{
            .from = "lead",
            .text = "must-not-land",
            .timestamp = "2",
            .read = false,
            .color = std::nullopt,
            .summary = std::nullopt,
        },
        std::optional<std::string_view>{"c14team"});
    const auto write_elapsed = c14_elapsed_ms(write_start);
    ASSERT_FALSE(written.has_value());
    EXPECT_NE(written.error().find("lock"), std::string::npos)
        << written.error();
    EXPECT_LT(write_elapsed, 3000)
        << "guarded writer must fail fast on a non-regular lock";
    EXPECT_FALSE(fs::exists(inbox))
        << "failed lock acquisition must bail before any inbox write";
    std::error_code ec;
    EXPECT_TRUE(fs::is_fifo(lock_path, ec))
        << "the FIFO itself must never be unlinked or replaced";
}

// Same FIFO-at-lock-name defense for ConfigFileLock: set_user_setting must
// surface ConfigWriteError with the existing open-failure shape (not the
// symlink-specific wording), leave the FIFO in place, create no user.json,
// and return without awaiting the 10s bound.
TEST(ConfigManagerC15, FifoLockNameRejectedWithoutBlocking) {
    C13Paths p("fifolock");
    const std::string lock_path = p.user_path.string() + ".lock";
    ASSERT_EQ(::mkfifo(lock_path.c_str(), 0600), 0);

    auto m = p.manager();
    const auto start = std::chrono::steady_clock::now();
    auto out = m.set_user_setting("network.max_retries", c13_parse("1"));
    const auto elapsed = std::chrono::duration_cast<
        std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    ASSERT_FALSE(out.has_value());
    EXPECT_NE(out.error().message.find("Cannot open config file for writing"),
              std::string::npos)
        << out.error().message;
    EXPECT_NE(out.error().message.find("user.json.lock"),
              std::string::npos)
        << out.error().message;
    EXPECT_EQ(out.error().message.find("symbolic link"), std::string::npos)
        << "FIFO rejection must not borrow the symlink-specific wording";
    EXPECT_LT(elapsed, 3000)
        << "non-regular lock name must fail fast, never await the deadline";
    EXPECT_FALSE(fs::exists(p.user_path))
        << "failed lock acquisition must bail before any data-file write";
    std::error_code ec;
    EXPECT_TRUE(fs::is_fifo(lock_path, ec))
        << "the FIFO itself must never be unlinked or replaced";
}

// Stale regular-file/FIFO debris at the reserved legacy tmp name is an old
// crash artifact: auto-unlinked, the write proceeds cleanly.
TEST(ConfigManagerC13d, LegacyTmpRegularAndFifoDebrisAutoRemoved) {
    C13Paths p("legacydebris");
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());

    {
        std::ofstream(p.user_path.string() + ".tmp") << "old crash junk";
        auto out = m.set_user_setting("network.max_retries", c13_parse("1"));
        ASSERT_TRUE(out.has_value()) << out.error().message;
        EXPECT_FALSE(fs::exists(p.user_path.string() + ".tmp"));
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("network").get("max_retries").as_int(), 1);
    }
    {
        ASSERT_EQ(::mkfifo((p.user_path.string() + ".tmp").c_str(), 0600), 0);
        auto out = m.set_user_setting("network.max_retries", c13_parse("2"));
        ASSERT_TRUE(out.has_value()) << out.error().message;
        EXPECT_FALSE(fs::exists(p.user_path.string() + ".tmp"));
        auto root = c13_parse([&] { std::ifstream f(p.user_path);
            return std::string(std::istreambuf_iterator<char>(f),
                               std::istreambuf_iterator<char>()); }());
        EXPECT_EQ(root.root.get("network").get("max_retries").as_int(), 2);
    }
}

// A symlinked config LEAF fails closed with the specific message; a
// symlinked lock path names the lock rather than its target.
TEST(ConfigManagerC13d, SymlinkedLeafAndLockRefusedWithSpecificErrors) {
    C13Paths p("leafsym");
    const auto victim = p.root / "victim.txt";
    constexpr std::string_view kCanary = "VICTIM-c13d-leaf-5521";
    { std::ofstream(victim) << kCanary; }

    // Symlinked data leaf.
    fs::create_symlink(victim, p.user_path);
    {
        auto m = p.manager();
        auto out = m.set_user_setting("network.max_retries", c13_parse("1"));
        ASSERT_FALSE(out.has_value());
        EXPECT_NE(out.error().message.find("symbolic link"),
                  std::string::npos);
        EXPECT_NE(out.error().message.find("symlinked configuration file"),
                  std::string::npos);
    }
    {
        std::ifstream f(victim);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(bytes, kCanary);
        std::error_code ec;
        EXPECT_TRUE(fs::is_symlink(p.user_path, ec));
    }

    // Symlinked lock path.
    fs::remove(p.user_path);
    fs::create_symlink(victim, p.user_path.string() + ".lock");
    {
        auto m = p.manager();
        auto out = m.set_user_setting("network.max_retries", c13_parse("1"));
        ASSERT_FALSE(out.has_value());
        EXPECT_NE(out.error().message.find("symlinked configuration lock"),
                  std::string::npos);
        EXPECT_NE(out.error().message.find("user.json.lock"),
                  std::string::npos);
        std::ifstream f(victim);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(bytes, kCanary);
    }
}

// save() through a pre-placed tmp symlink: victim intact, regular JSON.
TEST(ConfigManagerC13d, FullSaveTmpSymlinkNeverFollowed) {
    C13Paths p("savesym");
    CurrentPathGuard cwd_guard(p.root);
    const auto victim = p.root / "victim.txt";
    constexpr std::string_view kCanary = "VICTIM-c13d-save-8830";
    { std::ofstream(victim) << kCanary; }
    fs::create_symlink(victim, p.project_path.string() + ".tmp");

    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    ASSERT_TRUE(m.save(cc::core::ConfigSource::ProjectConfig).has_value());

    {
        std::ifstream f(victim);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(bytes, kCanary);
    }
    std::error_code ec;
    EXPECT_EQ(fs::symlink_status(p.project_path, ec).type(),
              fs::file_type::regular);
    auto doc = cc::utils::json::parse_file(p.project_path);
    ASSERT_TRUE(doc.has_value());
    EXPECT_TRUE(doc->root().is_obj());
    EXPECT_FALSE(fs::exists(p.project_path.string() + ".tmp"));
}

// save() preserves a pre-existing 0600 mode across the replace.
TEST(ConfigManagerC13d, FullSavePreservesMode) {
    if (::getuid() == 0) GTEST_SKIP() << "modes are bypassed for root";
    C13Paths p("savemode");
    CurrentPathGuard cwd_guard(p.root);
    c13_write_file(p.project_path, R"JSON({"network":{"max_retries":1}})JSON");
    fs::permissions(p.project_path,
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    ASSERT_TRUE(m.save(cc::core::ConfigSource::ProjectConfig).has_value());
    const auto mode = fs::status(p.project_path).permissions() & fs::perms::mask;
    EXPECT_EQ(mode, fs::perms::owner_read | fs::perms::owner_write);
}

// save() through a symlinked leaf refuses with the specific error.
TEST(ConfigManagerC13d, FullSaveSymlinkedLeafRefused) {
    C13Paths p("saveleaf");
    CurrentPathGuard cwd_guard(p.root);
    const auto victim = p.root / "victim.txt";
    { std::ofstream(victim) << "VICTIM-c13d-saveleaf"; }
    fs::create_symlink(victim, p.project_path);
    auto m = p.manager();
    // No load(): a symlinked project leaf already hard-fails the tier read;
    // here we verify save() itself fails closed when reached directly.
    auto out = m.save(cc::core::ConfigSource::ProjectConfig);
    ASSERT_FALSE(out.has_value());
    EXPECT_NE(out.error().message.find("symlinked configuration file"),
              std::string::npos);
}

// One full save() racing ten patched setters: every child succeeds and
// the final file (and no intermediate one observed) is parseable JSON with
// no tmp debris — never torn, last-writer-wins by nature.
TEST(ConfigManagerC13d, ConcurrentSaveAndPatchersAlwaysParseable) {
    C13Paths p("saveconcurrent");
    CurrentPathGuard cwd_guard(p.root);
    c13_write_file(p.user_path, R"JSON({"network":{"max_retries":1}})JSON");

    constexpr int kChildren = 11;
    std::array<int, kChildren> pids{};
    for (int i = 0; i < kChildren; ++i) {
        const pid_t pid = ::fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            auto m = p.manager();
            (void)m.load();
            if (i == kChildren - 1) {
                _exit(m.save(cc::core::ConfigSource::ProjectConfig)
                          ? 0 : 2);
            }
            const char* keys[] = {"network.max_retries",
                                  "model.max_output_tokens",
                                  "model.context_window_size",
                                  "model.thinking_budget",
                                  "model.extended_thinking"};
            const char* vals[] = {"100", "20000", "300000", "4096", "true"};
            const int k = i % 5;
            auto out = m.set_user_setting(
                keys[k], c13_parse(vals[k]));
            _exit(out.has_value() ? 0 : 2);
        }
        pids[static_cast<std::size_t>(i)] = pid;
    }
    for (pid_t pid : pids) {
        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0);
    }

    // User file: every patcher landed distinct keys (project save targets
    // a different path, so it cannot clobber the user file).
    auto doc = cc::utils::json::parse_file(p.user_path);
    ASSERT_TRUE(doc.has_value());
    ASSERT_TRUE(doc->root().is_obj());
    EXPECT_EQ(doc->root().get("model").get("max_output_tokens").as_int(),
              20000);
    EXPECT_EQ(doc->root().get("model").get("extended_thinking").as_bool(),
              true);
    // Project save produced a parseable full document.
    auto project_doc = cc::utils::json::parse_file(p.project_path);
    ASSERT_TRUE(project_doc.has_value());
    EXPECT_TRUE(project_doc->root().is_obj());
    for (const auto& entry : fs::directory_iterator(p.root)) {
        EXPECT_EQ(entry.path().filename().string().find(".tmp."),
                  std::string::npos);
    }
}

// Pre-spraying symlinks at predictable legacy-style tmp names cannot block
// the random-suffixed writer or reach the victim.
TEST(ConfigManagerC13d, PresprayedPredictableTmpNamesDefeated) {
    C13Paths p("prespray");
    const auto victim = p.root / "victim.txt";
    constexpr std::string_view kCanary = "VICTIM-c13d-prespray-2299";
    { std::ofstream(victim) << kCanary; }
    const pid_t pid = ::getpid();
    for (int i = 0; i < 64; ++i) {
        fs::create_symlink(
            victim,
            p.user_path.string() + ".tmp." + std::to_string(pid) + "." +
                std::to_string(i));
    }
    auto m = p.manager();
    ASSERT_TRUE(m.load().has_value());
    auto out = m.set_user_setting("network.max_retries", c13_parse("3"));
    ASSERT_TRUE(out.has_value()) << out.error().message;

    {
        std::ifstream f(victim);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(bytes, kCanary);
    }
    std::error_code ec;
    EXPECT_EQ(fs::symlink_status(p.user_path, ec).type(),
              fs::file_type::regular);
    auto root = c13_parse([&] { std::ifstream f(p.user_path);
        return std::string(std::istreambuf_iterator<char>(f),
                           std::istreambuf_iterator<char>()); }());
    EXPECT_EQ(root.root.get("network").get("max_retries").as_int(), 3);
}

// ===========================================================================
// RFC-0001 B followup c13e — git-worktree-gated ignores + same-path
// save/patch contention.
// ===========================================================================

// Plain non-git directory: local AND project writes must NOT create any
// .gitignore anywhere (the data write itself is unaffected).
TEST(ConfigManagerC13e, PlainDirectoryWritesNoGitignore) {
    // Anchored under the shared git-ancestry-clean temp base (this box
    // carries a stray /tmp/.git, so XDG_RUNTIME_DIR / /dev/shm win).
    const auto base = c13_clean_temp_base();
    if (!base) GTEST_SKIP() << "no .git-free temp base directory available";

    const auto suffix =
        std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = *base / ("loom_c13e_plain_" + std::to_string(suffix));
    fs::create_directories(root);
    CurrentPathGuard cwd_guard(root);

    const auto global_path  = root / "global.json";
    const auto user_path    = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path   = root / "project.local.json";

    // Snapshot any PRE-EXISTING (unrelated) .gitignore on the chain so the
    // post-write assertion is immune to a dirty shared temp base.
    const auto ignores_before = c13_gitignores_on_chain(root, *base);

    cc::core::McpServerConfig local;
    local.name = "ls";
    local.transport = "stdio";
    local.command = "node";
    cc::core::McpServerConfig project = local;
    project.name = "ps";

    {
        cc::core::ConfigManager m(global_path, user_path,
                                  project_path, local_path);
        ASSERT_TRUE(m.load().has_value());
        ASSERT_TRUE(m.upsert_mcp_server(cc::core::McpStorageScope::Local, local)
                        .has_value());
        ASSERT_TRUE(m.upsert_mcp_server(cc::core::McpStorageScope::Project,
                                        project).has_value());
        ASSERT_TRUE(m.save(cc::core::ConfigSource::ProjectConfig).has_value());
    }
    EXPECT_TRUE(fs::exists(local_path));
    EXPECT_TRUE(fs::exists(project_path));
    // No NEW .gitignore appeared anywhere on the walk-up chain to the base.
    const auto ignores_after = c13_gitignores_on_chain(root, *base);
    for (const auto& path : ignores_after) {
        EXPECT_TRUE(ignores_before.count(path) != 0)
            << "unexpected new .gitignore: " << path;
    }
    EXPECT_FALSE(fs::exists(root / ".gitignore"));
    // Data files are still correct.
    auto ldoc = cc::utils::json::parse_file(local_path);
    ASSERT_TRUE(ldoc.has_value());
    EXPECT_TRUE(ldoc->root().get("mcpServers").has("ls"));

    std::error_code ec;
    fs::remove_all(root, ec);
}

// Nested cwd inside a git repo: the ignore lines land in the REPO ROOT
// .gitignore via walk-up, not the nested working directory.
TEST(ConfigManagerC13e, NestedCwdAppendsAtRepoRoot) {
    if (std::system("git --version >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "git not available";
    }
    const auto suffix =
        std::chrono::system_clock::now().time_since_epoch().count();
    const auto base = fs::temp_directory_path() /
                      ("loom_c13e_nested_" + std::to_string(suffix));
    fs::create_directories(base);
    const auto repo = base / "r";
    fs::create_directories(repo);
    {
        CurrentPathGuard init_guard(repo);
        ASSERT_EQ(std::system("git init -q --initial-branch main"), 0);
    }
    const auto nested = repo / "sub" / "deep";
    fs::create_directories(nested);

    const auto global_path  = nested / "global.json";
    const auto user_path    = nested / "user.json";
    const auto project_path = nested / ".loom" / "config.json";
    const auto local_path   = nested / ".loom" / "config.local.json";

    {
        CurrentPathGuard cwd_guard(nested);
        cc::core::ConfigManager m(global_path, user_path,
                                  project_path, local_path);
        ASSERT_TRUE(m.load().has_value());
        cc::core::McpServerConfig cfg;
        cfg.name = "deep-srv";
        cfg.transport = "stdio";
        cfg.command = "node";
        ASSERT_TRUE(m.upsert_mcp_server(cc::core::McpStorageScope::Local, cfg)
                        .has_value());
        ASSERT_TRUE(m.save(cc::core::ConfigSource::ProjectConfig).has_value());

        // Nothing written at the nested cwd level.
        EXPECT_FALSE(fs::exists(nested / ".gitignore"));
        // Lines landed at the walk-up repo root.
        const auto root_gi = repo / ".gitignore";
        ASSERT_TRUE(fs::exists(root_gi));
        std::ifstream f(root_gi);
        const std::string text((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
        EXPECT_NE(text.find("config.local.json.lock"), std::string::npos);
        EXPECT_NE(text.find("config.local.json\n"), std::string::npos);
        EXPECT_NE(text.find("config.json.lock"), std::string::npos);
        EXPECT_EQ(text.find("config.json\n"), std::string::npos);
    }
    fs::remove_all(base);
}

// Full save() and patched set_user_setting writes hit the SAME user file
// concurrently: every child exits 0 (bounded lock/CAS keep failures clean),
// and a sampled sweep plus the final file always parse as JSON objects with
// no tmp debris. save() can only target project_path_, so the manager is
// constructed with project_path_ aliasing the user config location.
TEST(ConfigManagerC13e, SaveAndPatchesContendOnSameFile) {
    // C13Paths anchors p.root under a git-ANCESTRY-CLEAN base (this box
    // carries a stray /tmp/.git, which would otherwise make forked savers
    // write config.json.lock into /tmp/.gitignore), so the claim below is
    // now true rather than merely CI-lucky.
    C13Paths p("samefile");
    // Forked children inherit cwd: keep every ignore-append probe inside
    // the clean temp root, whose walk-up contains no .git marker, so no
    // .gitignore can be created anywhere up the chain.
    CurrentPathGuard cwd_guard(p.root);
    c13_write_file(p.project_path, R"JSON({
      "network": {"max_retries": 1},
      "model": {"default_model": "seed-model"}
    })JSON");

    // Snapshot pre-existing .gitignore files on the exact chain this
    // fixture uses (recorded in p.base) before the fork storm.
    const auto ignores_before =
        c13_gitignores_on_chain(p.root, p.base);

    // Manager whose SAVE target is the same file the patchers write:
    // save(ProjectConfig) writes project_path_; set_user_setting writes
    // user_path_ — point both at one path.
    auto make_aligned = [&] {
        return cc::core::ConfigManager(p.global_path, p.project_path,
                                       p.project_path,
                                       p.project_path.string() + ".local");
    };

    constexpr int kPatchers = 8;
    constexpr int kSavers = 4;
    constexpr int kTotal = kPatchers + kSavers;
    std::array<pid_t, kTotal> pids{};
    int idx = 0;
    for (int s = 0; s < kSavers; ++s) {
        const pid_t pid = ::fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            auto m = make_aligned();
            (void)m.load();
            // Mutate so each full-save document is distinct, then replace
            // the SAME file the patchers update.
            m.settings_mut().network.max_retries =
                static_cast<std::uint32_t>(1000 + s);
            _exit(m.save(cc::core::ConfigSource::ProjectConfig) ? 0 : 2);
        }
        pids[static_cast<std::size_t>(idx++)] = pid;
    }
    const std::array<std::string_view, 5> keys = {{
        "network.max_retries",
        "model.max_output_tokens",
        "model.context_window_size",
        "model.extended_thinking",
        "model.default_model",
    }};
    for (int k = 0; k < kPatchers; ++k) {
        const pid_t pid = ::fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            auto m = make_aligned();
            (void)m.load();
            const auto& key = keys[static_cast<std::size_t>(k % keys.size())];
            const char* payload =
                key == "model.default_model" ? R"("contend-model")"
                : key == "model.extended_thinking" ? "true"
                : key == "model.max_output_tokens" ? "2048"
                : key == "model.context_window_size" ? "100000"
                : "7";
            auto out = m.set_user_setting(key, c13_parse(payload));
            _exit(out.has_value() ? 0 : 2);
        }
        pids[static_cast<std::size_t>(idx++)] = pid;
    }
    for (pid_t pid : pids) {
        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFEXITED(status));
        // Only 0 (success) or a CLEAN lock/CAS-style error (2) are allowed;
        // crash/signal would fail here. With the bounded flock + CAS retry
        // the writer converges, so expect all-success in practice.
        EXPECT_EQ(WEXITSTATUS(status), 0);
    }

    // Final file is a parseable object.
    auto doc = cc::utils::json::parse_file(p.project_path);
    ASSERT_TRUE(doc.has_value());
    EXPECT_TRUE(doc->root().is_obj());
    // No torn tmp leftovers; no stray local file (that path is never
    // written here — local_path_ aliases a different name).
    for (const auto& entry : fs::directory_iterator(p.root)) {
        const auto name = entry.path().filename().string();
        EXPECT_EQ(name.find(".tmp"), std::string::npos) << name;
    }
    // Snapshot-negative assertion: the fork storm created NO new .gitignore
    // anywhere on the exact chain from the temp root to p's recorded base
    // (pre-existing unrelated ignores in a shared base stay allowed).
    const auto ignores_after =
        c13_gitignores_on_chain(p.root, p.base);
    for (const auto& path : ignores_after) {
        EXPECT_TRUE(ignores_before.count(path) != 0)
            << "unexpected new .gitignore: " << path;
    }
}

// ===========================================================================
// RFC-0001 B followup c16 — atomic, symlink/FIFO-safe team-data replaces;
// shared-lock readers; per-path grants mutex shards.
// ===========================================================================
namespace {

// Mode bits of a path as lstat(2) sees them (no symlink follow).
[[nodiscard]] mode_t c16_mode_bits(const fs::path& path) {
    struct ::stat st {};
    EXPECT_EQ(::lstat(path.c_str(), &st), 0) << path.string();
    return st.st_mode & 07777;
}

// True when the directory contains no atomic-replace tmp debris.
[[nodiscard]] bool c16_no_tmp_debris(const fs::path& dir) {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.path().filename().string().find(".tmp.") !=
            std::string::npos) {
            return false;
        }
    }
    return true;
}

// Forked PRE-FIX style writer: truncating open + two non-atomic write()
// halves + close, looped for `duration_ms`. Small deliberate gaps
// reproduce the visible truncate/partial window a slow multi-insert
// std::ofstream (or a contended page cache) presents; reads landing in the
// three windows (post-truncate empty, first half, between halves) fail to
// parse. Exists only to MEASURE the torn window c16 removes.
void c16_raw_truncating_writer(const fs::path& path,
                               std::string half1,
                               std::string half2,
                               int duration_ms) {
    constexpr struct timespec kGap1{0, 300 * 1000};   // 300us
    constexpr struct timespec kGap2{0, 200 * 1000};   // 200us
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(duration_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        const int fd = ::open(path.c_str(),
                              O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) _exit(20);
        ::nanosleep(&kGap1, nullptr);  // post-truncate empty window
        const ssize_t n1 =
            ::write(fd, half1.data(), half1.size());
        ::nanosleep(&kGap2, nullptr);  // partial-content window
        const ssize_t n2 =
            ::write(fd, half2.data(), half2.size());
        if (n1 < 0 || n2 < 0) {
            ::close(fd);
            _exit(21);
        }
        ::close(fd);
    }
    _exit(0);
}

struct C16TornCounters {
    std::atomic<std::uint64_t> attempts{0};
    std::atomic<std::uint64_t> torn{0};
};

// Raw unlocked JSON-array reader (pre-c16 read_inbox shape).
void c16_raw_array_reader(const fs::path& path,
                          C16TornCounters& counters,
                          std::atomic<bool>* stop,
                          std::uint64_t max_attempts) {
    while (!stop->load(std::memory_order_relaxed) &&
           counters.attempts.load(std::memory_order_relaxed) < max_attempts) {
        auto parsed = cc::utils::json::parse_file(path);
        counters.attempts.fetch_add(1, std::memory_order_relaxed);
        if (!parsed || !parsed->root().is_arr()) {
            counters.torn.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

// c16 production-shape storm child: an exclusive RMW rewrite of the inbox
// (lock + atomic replace, rotating payload) followed by a full team
// config.json rewrite, looped until the deadline.
void c16_storm_writer_child(std::string teams_root,
                            std::string team,
                            std::string agent,
                            int duration_ms) {
    ::setenv("LOOM_TEAM_RUNTIME_DIR", teams_root.c_str(), 1);
    const auto inbox =
        fs::path{cc::utils::get_inbox_path(agent,
                    std::optional<std::string_view>{team})};
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(duration_ms);
    std::uint64_t seq = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            cc::utils::ScopedInboxLock flock(inbox);
            if (!flock.locked()) _exit(30);
            std::vector<cc::utils::TeammateMessage> messages;
            for (int m = 0; m < 3; ++m) {
                messages.push_back(cc::utils::TeammateMessage{
                    .from = std::format("w{}", m),
                    .text = std::format("seq-{}-msg-{}", seq, m),
                    .timestamp = std::to_string(seq),
                    .read = (m == 0),
                    .color = std::nullopt,
                    .summary = std::nullopt,
                });
            }
            if (!cc::utils::detail::write_messages(inbox, messages)) {
                _exit(31);
            }
        }
        {
            cc::utils::TeamFileRecord record;
            record.name = team;
            record.lead_agent_id = "team-lead@" + team;
            record.created_at = static_cast<std::int64_t>(seq);
            for (int m = 0; m < 2; ++m) {
                cc::utils::TeamMemberRecord member;
                member.agent_id = std::format("w{}@{}", m, team);
                member.name = std::format("w{}", m);
                member.tmux_pane_id = std::to_string((seq + m) % 7);
                member.cwd = "/tmp";
                member.joined_at = static_cast<std::int64_t>(seq);
                record.members.push_back(std::move(member));
            }
            if (!cc::utils::write_team_file(team, record)) _exit(32);
        }
        ++seq;
    }
    _exit(0);
}

}  // namespace

// The replace primitive itself: new files follow umask, rewrites preserve
// the existing mode, OwnerOnly forces 0600, payload bytes are exact, and no
// tmp debris remains after success.
TEST(AtomicReplaceC16, ModePolicyBytesAndNoDebris) {
    ::umask(0022);
    C14TeamEnv env;
    const auto path = env.root / "data.json";

    ASSERT_TRUE(cc::utils::atomic_replace_file(path, "[]").has_value());
    EXPECT_EQ(c14_read_file(path), "[]");
    EXPECT_EQ(c16_mode_bits(path), 0644u);

    ASSERT_TRUE(cc::utils::atomic_replace_file(path, "[1]").has_value());
    EXPECT_EQ(c14_read_file(path), "[1]");
    EXPECT_EQ(c16_mode_bits(path), 0644u);

    ASSERT_EQ(::chmod(path.string().c_str(), 0600), 0);
    ASSERT_TRUE(cc::utils::atomic_replace_file(path, "[2]").has_value());
    EXPECT_EQ(c16_mode_bits(path), 0600u)
        << "PreserveOrUmask must keep a pre-existing 0600 mode";

    ASSERT_EQ(::chmod(path.string().c_str(), 0640), 0);
    ASSERT_TRUE(cc::utils::atomic_replace_file(path, "[3]").has_value());
    EXPECT_EQ(c16_mode_bits(path), 0640u)
        << "PreserveOrUmask must keep a pre-existing 0640 mode";

    ASSERT_TRUE(cc::utils::atomic_replace_file(
        path, "[4]", cc::utils::AtomicMode::OwnerOnly).has_value());
    EXPECT_EQ(c16_mode_bits(path), 0600u)
        << "OwnerOnly must force 0600 regardless of the prior mode";
    EXPECT_EQ(c14_read_file(path), "[4]");
    EXPECT_TRUE(c16_no_tmp_debris(env.root));
}

// Symlinked data leaves (to a canary, or dangling) and a FIFO at the data
// name are refused SPECIFICALLY and fast: no follow, no clobber, no block.
TEST(AtomicReplaceC16, SymlinkDanglingAndFifoLeavesRefusedFast) {
    C14TeamEnv env;
    const auto path = env.root / "data.json";
    const auto canary = env.root / "canary.txt";
    constexpr std::string_view kCanary = "c16-canary-7741";
    { std::ofstream out(canary, std::ios::binary); out << kCanary; }

    // Symlink to a real victim.
    fs::create_symlink(canary, path);
    auto t0 = std::chrono::steady_clock::now();
    auto result = cc::utils::atomic_replace_file(path, "[]");
    auto elapsed = c14_elapsed_ms(t0);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("symbolic link"), std::string::npos)
        << result.error();
    EXPECT_LT(elapsed, 2000);
    EXPECT_TRUE(fs::is_symlink(path));
    EXPECT_EQ(c14_read_file(canary), std::string(kCanary));

    // Dangling symlink.
    std::error_code ec;
    fs::remove(path, ec);
    fs::create_symlink(env.root / "missing", path);
    result = cc::utils::atomic_replace_file(path, "[]");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("symbolic link"), std::string::npos)
        << result.error();
    EXPECT_TRUE(fs::is_symlink(path));
    EXPECT_FALSE(fs::exists(path, ec)) << "the dangling target stays uncreated";

    // FIFO at the data name: the lstat gate refuses before any open(), so
    // the O_RDWR/FIFO blocking rule never comes into play.
    fs::remove(path, ec);
    ASSERT_EQ(::mkfifo(path.string().c_str(), 0600), 0);
    t0 = std::chrono::steady_clock::now();
    result = cc::utils::atomic_replace_file(path, "[]");
    elapsed = c14_elapsed_ms(t0);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("not a regular file"), std::string::npos)
        << result.error();
    EXPECT_LT(elapsed, 2000) << "a FIFO leaf must never block the writer";
    EXPECT_TRUE(fs::is_fifo(path, ec));
}

// BEFORE/AFTER measurement. The same 8-process rewrite storm, read 4-way:
//   * raw truncating writers + unlocked reads (the pre-c16 hazard) measure
//     a non-zero torn/parse-failure rate and log it;
//   * the production c16 writers (LOCK_EX + unique-tmp atomic replace) with
//     the production LOCK_SH readers must show ZERO torn reads across at
//     least 100 000 production read attempts, and finish with valid data.
TEST(TeamDataC16, StormReadersMeasureZeroTornAcrossInboxAndTeamFile) {
    constexpr int kWriters = 8;
    constexpr std::uint64_t kMinReads = 100'000;

    // ── Baseline: old-style truncating storm (measurement only) ──────────
    {
        C14TeamEnv env;
        const auto path = c14_inbox_path("raw", "c16rawteam");
        fs::create_directories(path.parent_path());
        const std::string whole =
            "[{\"from\":\"a\",\"text\":\"" + std::string(220, 'x') +
            "\",\"timestamp\":\"1\",\"read\":false}]";
        const std::string h1 = whole.substr(0, whole.size() / 2);
        const std::string h2 = whole.substr(whole.size() / 2);
        {
            std::ofstream seed(path, std::ios::binary);
            seed << whole;
        }
        constexpr int kRawMs = 1500;
        pid_t pids[kWriters];
        for (int w = 0; w < kWriters; ++w) {
            const pid_t pid = ::fork();
            if (pid < 0) _exit(2);
            if (pid == 0) {
                c16_raw_truncating_writer(path, h1, h2, kRawMs);
            }
            pids[w] = pid;
        }
        C16TornCounters raw;
        std::atomic<bool> stop{false};
        std::vector<std::thread> readers;
        for (int r = 0; r < 4; ++r) {
            readers.emplace_back([&] {
                c16_raw_array_reader(path, raw, &stop,
                                     std::numeric_limits<std::uint64_t>::max());
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kRawMs));
        stop.store(true, std::memory_order_relaxed);
        for (auto& t : readers) t.join();
        for (pid_t pid : pids) {
            int status = 0;
            ASSERT_EQ(::waitpid(pid, &status, 0), pid);
            ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
        }
        const auto attempts = raw.attempts.load();
        const auto torn = raw.torn.load();
        const double rate = attempts
            ? (100.0 * static_cast<double>(torn) /
               static_cast<double>(attempts))
            : 0.0;
        std::printf(
            "[c16] PRE-FIX raw truncating storm: %llu torn / %llu reads "
            "(%.2f%%) — the transient parse-failure window c16 removes\n",
            static_cast<unsigned long long>(torn),
            static_cast<unsigned long long>(attempts), rate);
        // Informational baseline: never flake the suite if the window was
        // not sampled on a fast scheduler; the post-fix assertion is the
        // load-bearing one below.
        EXPECT_GT(attempts, 1000u);
    }

    // ── Production c16 storm: locked atomic writers, SH readers ──────────
    C14TeamEnv env;
    const std::string team = "c16storm";
    const auto inbox = c14_inbox_path("worker", team);
    // Seed both files so a read never legitimately returns empty/absent.
    {
        cc::utils::TeammateMessage seed{
            .from = "lead", .text = "seed", .timestamp = "0", .read = false, .color = std::nullopt, .summary = std::nullopt};
        ASSERT_TRUE(cc::utils::detail::write_messages(inbox, {seed}));
        cc::utils::TeamFileRecord record;
        record.name = team;
        record.lead_agent_id = "team-lead@" + team;
        ASSERT_TRUE(cc::utils::write_team_file(team, record));
    }

    constexpr int kStormMs = 6000;
    pid_t pids[kWriters];
    for (int w = 0; w < kWriters; ++w) {
        const pid_t pid = ::fork();
        if (pid < 0) _exit(2);
        if (pid == 0) {
            c16_storm_writer_child(
                (env.root / "teams").string(), team, "worker", kStormMs);
        }
        pids[w] = pid;
    }

    C16TornCounters inbox_counts;
    C16TornCounters team_counts;
    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&] {
            while (!stop.load(std::memory_order_relaxed)) {
                auto msgs = cc::utils::read_inbox(
                    "worker", std::optional<std::string_view>{team});
                inbox_counts.attempts.fetch_add(1, std::memory_order_relaxed);
                if (!msgs || msgs->empty()) {
                    inbox_counts.torn.fetch_add(1,
                        std::memory_order_relaxed);
                }
                // Leave EX writers real lock windows (production polls on
                // a 1.5 s cadence); a non-yielding SH spin can starve
                // flock-EX writers up to their bounded wait.
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&] {
            while (!stop.load(std::memory_order_relaxed)) {
                auto file = cc::utils::read_team_file(team);
                team_counts.attempts.fetch_add(1, std::memory_order_relaxed);
                if (!file) team_counts.torn.fetch_add(1,
                    std::memory_order_relaxed);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
    // Reap the storm writers first (they self-exit after kStormMs). The
    // LOCK_SH readers keep running; once the writers are gone the readers
    // are uncontended, so the 100k floor is reached deterministically
    // regardless of machine speed or load (the torn count is what the
    // storm-under-contention phase proves; extra uncontended reads simply
    // keep being torn-free).
    for (pid_t pid : pids) {
        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0)
            << "storm writer failed: " << WEXITSTATUS(status);
    }
    const auto read_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < read_deadline) {
        const std::uint64_t total =
            inbox_counts.attempts.load() + team_counts.attempts.load();
        if (total >= kMinReads) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    stop.store(true, std::memory_order_relaxed);
    for (auto& t : readers) t.join();

    const auto inbox_attempts = inbox_counts.attempts.load();
    const auto team_attempts = team_counts.attempts.load();
    std::printf(
        "[c16] POST-FIX locked atomic storm: inbox %llu torn / %llu reads; "
        "team file %llu torn / %llu reads (0 expected)\n",
        static_cast<unsigned long long>(inbox_counts.torn.load()),
        static_cast<unsigned long long>(inbox_attempts),
        static_cast<unsigned long long>(team_counts.torn.load()),
        static_cast<unsigned long long>(team_attempts));
    EXPECT_GE(inbox_attempts + team_attempts, kMinReads)
        << "storm must exercise at least 100k production reads";
    EXPECT_EQ(inbox_counts.torn.load(), 0u);
    EXPECT_EQ(team_counts.torn.load(), 0u);

    // Final content is valid.
    auto final_inbox = cc::utils::read_inbox(
        "worker", std::optional<std::string_view>{team});
    ASSERT_TRUE(final_inbox.has_value());
    EXPECT_FALSE(final_inbox->empty());
    auto final_team = cc::utils::read_team_file(team);
    ASSERT_TRUE(final_team.has_value());
    EXPECT_EQ(final_team->name, team);
    EXPECT_EQ(final_team->members.size(), 2u);
    EXPECT_TRUE(c16_no_tmp_debris(inbox.parent_path()));
    EXPECT_TRUE(c16_no_tmp_debris(
        fs::path{cc::utils::team_file_path(team)}.parent_path()));
}

// Data-leaf attacks on the live inbox RMW path: a symlink (dangling or to
// a canary) or a FIFO at the inbox name makes write_to_mailbox fail clean,
// fast, and leaves the victim and the leaf untouched — never block, never
// follow, never clobber.
TEST(TeamDataC16, InboxLeafAttacksFailCleanFastWithoutFollowing) {
    struct Case { const char* name; int kind; };  // 0 symlink, 1 dangling, 2 fifo
    for (const auto& tc : std::array<Case, 3>{
            Case{"symlink", 0}, Case{"dangling", 1}, Case{"fifo", 2}}) {
        C14TeamEnv env;
        const auto inbox = c14_inbox_path("worker", "c16attack");
        fs::create_directories(inbox.parent_path());
        const auto canary = env.root / "canary.txt";
        constexpr std::string_view kCanary = "c16-inbox-canary-3180";

        if (tc.kind == 0) {
            { std::ofstream out(canary, std::ios::binary); out << kCanary; }
            fs::create_symlink(canary, inbox);
        } else if (tc.kind == 1) {
            fs::create_symlink(env.root / "gone", inbox);
        } else {
            ASSERT_EQ(::mkfifo(inbox.string().c_str(), 0600), 0);
        }

        const auto t0 = std::chrono::steady_clock::now();
        auto written = cc::utils::write_to_mailbox(
            "worker",
            cc::utils::TeammateMessage{
                .from = "lead",
                .text = "must-not-land",
                .timestamp = "9",
                .read = false,
                .color = std::nullopt,
                .summary = std::nullopt,
            },
            std::optional<std::string_view>{"c16attack"});
        const auto elapsed = c14_elapsed_ms(t0);
        ASSERT_FALSE(written.has_value()) << tc.name;
        EXPECT_LT(elapsed, 3000)
            << tc.name << ": non-regular leaf must fail fast, never block";

        std::error_code ec;
        if (tc.kind == 0) {
            EXPECT_TRUE(fs::is_symlink(inbox, ec)) << tc.name;
            EXPECT_EQ(c14_read_file(canary), std::string(kCanary)) << tc.name;
        } else if (tc.kind == 1) {
            EXPECT_TRUE(fs::is_symlink(inbox, ec)) << tc.name;
        } else {
            EXPECT_TRUE(fs::is_fifo(inbox, ec)) << tc.name;
        }
        // The unlocked/locked reads also fail safe (empty) and never block.
        auto msgs = cc::utils::read_inbox(
            "worker", std::optional<std::string_view>{"c16attack"});
        ASSERT_TRUE(msgs.has_value()) << tc.name;
        EXPECT_TRUE(msgs->empty()) << tc.name;
        EXPECT_TRUE(c16_no_tmp_debris(inbox.parent_path())) << tc.name;
    }
}

// The same leaf gate for the canonical team config.json writer.
TEST(TeamDataC16, TeamFileLeafAttacksFailCleanFast) {
    C14TeamEnv env;
    const fs::path path{cc::utils::team_file_path("c16tattack")};
    fs::create_directories(path.parent_path());
    const auto canary = env.root / "canary.txt";
    constexpr std::string_view kCanary = "c16-team-canary-6204";

    cc::utils::TeamFileRecord record;
    record.name = "c16tattack";
    record.lead_agent_id = "team-lead@c16tattack";

    { std::ofstream out(canary, std::ios::binary); out << kCanary; }
    fs::create_symlink(canary, path);
    auto t0 = std::chrono::steady_clock::now();
    EXPECT_FALSE(cc::utils::write_team_file("c16tattack", record));
    EXPECT_LT(c14_elapsed_ms(t0), 3000);
    EXPECT_TRUE(fs::is_symlink(path));
    EXPECT_EQ(c14_read_file(canary), std::string(kCanary));

    std::error_code ec;
    fs::remove(path, ec);
    ASSERT_EQ(::mkfifo(path.string().c_str(), 0600), 0);
    t0 = std::chrono::steady_clock::now();
    EXPECT_FALSE(cc::utils::write_team_file("c16tattack", record));
    EXPECT_LT(c14_elapsed_ms(t0), 3000)
        << "a FIFO config leaf must never block the writer";
    EXPECT_TRUE(fs::is_fifo(path, ec));
}

// Mode preservation across the live inbox and team-config writers, and the
// 0644-under-022 mode for brand-new files.
TEST(TeamDataC16, InboxAndTeamFileModesPreservedAcrossRewrites) {
    ::umask(0022);
    C14TeamEnv env;
    const auto inbox = c14_inbox_path("worker", "c16mode");

    cc::utils::TeammateMessage m{
        .from = "lead", .text = "one", .timestamp = "1", .read = false, .color = std::nullopt, .summary = std::nullopt};
    ASSERT_TRUE(cc::utils::write_to_mailbox(
        "worker", m, std::optional<std::string_view>{"c16mode"}).has_value());
    EXPECT_EQ(c16_mode_bits(inbox), 0644u);

    ASSERT_EQ(::chmod(inbox.string().c_str(), 0600), 0);
    m.text = "two";
    m.timestamp = "2";
    ASSERT_TRUE(cc::utils::write_to_mailbox(
        "worker", m, std::optional<std::string_view>{"c16mode"}).has_value());
    EXPECT_EQ(c16_mode_bits(inbox), 0600u)
        << "inbox rewrite must preserve a pre-existing 0600 mode";

    ASSERT_EQ(::chmod(inbox.string().c_str(), 0640), 0);
    m.text = "three";
    m.timestamp = "3";
    ASSERT_TRUE(cc::utils::write_to_mailbox(
        "worker", m, std::optional<std::string_view>{"c16mode"}).has_value());
    EXPECT_EQ(c16_mode_bits(inbox), 0640u)
        << "inbox rewrite must preserve a pre-existing 0640 mode";

    const fs::path config{cc::utils::team_file_path("c16mode")};
    cc::utils::TeamFileRecord record;
    record.name = "c16mode";
    record.lead_agent_id = "team-lead@c16mode";
    ASSERT_TRUE(cc::utils::write_team_file("c16mode", record));
    EXPECT_EQ(c16_mode_bits(config), 0644u);
    ASSERT_EQ(::chmod(config.string().c_str(), 0600), 0);
    record.created_at = 7;
    ASSERT_TRUE(cc::utils::write_team_file("c16mode", record));
    EXPECT_EQ(c16_mode_bits(config), 0600u)
        << "team config rewrite must preserve a pre-existing 0600 mode";
}

// Shared-lock reader semantics: while an exclusive holder owns the inbox
// sibling, read_inbox fails closed (empty) at the ~10s bound (it never
// sees partial bytes), and many concurrent LOCK_SH readers on an
// uncontended inbox all succeed simultaneously.
TEST(ScopedInboxLockC16, SharedReaderFailsClosedAndSharedReadersConcur) {
    C14TeamEnv env;
    const auto inbox = c14_inbox_path("worker", "c16sh");
    fs::create_directories(inbox.parent_path());
    const std::string canary =
        R"JSON([{"from":"lead","text":"keep","timestamp":"1","read":true}])JSON";
    { std::ofstream out(inbox, std::ios::binary); out << canary; }

    const std::string lock_path = inbox.string() + ".lock";
    C14Holder h = c14_spawn_holder(lock_path, 30);
    ASSERT_GT(h.pid, 0);
    C14HolderReaper reaper(h.pid);
    c14_await_ready(h.read_fd);

    struct ReadResult {
        bool has_value = false;
        bool empty = true;
        long long elapsed_ms = 0;
    };
    ReadResult reader;
    std::thread contended([&] {
        const auto t0 = std::chrono::steady_clock::now();
        auto msgs = cc::utils::read_inbox(
            "worker", std::optional<std::string_view>{"c16sh"});
        reader.elapsed_ms = c14_elapsed_ms(t0);
        reader.has_value = msgs.has_value();
        reader.empty = msgs && msgs->empty();
    });

    // Meanwhile, 8 concurrent LOCK_SH readers on a DIFFERENT, uncontended
    // inbox must all succeed at once (shared locks do not exclude each
    // other).
    const auto other = c14_inbox_path("worker", "c16sh-other");
    fs::create_directories(other.parent_path());
    {
        cc::utils::TeammateMessage seed{
            .from = "lead", .text = "hi", .timestamp = "1", .read = false, .color = std::nullopt, .summary = std::nullopt};
        ASSERT_TRUE(cc::utils::detail::write_messages(other, {seed}));
    }
    std::atomic<int> shared_ok{0};
    std::vector<std::thread> shared_readers;
    for (int i = 0; i < 8; ++i) {
        shared_readers.emplace_back([&] {
            cc::utils::ScopedInboxLock lock(
                other, cc::utils::LockKind::Shared);
            if (lock.locked()) shared_ok.fetch_add(1);
        });
    }
    for (auto& t : shared_readers) t.join();
    EXPECT_EQ(shared_ok.load(), 8)
        << "concurrent shared locks must all be granted";

    contended.join();
    EXPECT_TRUE(reader.has_value);
    EXPECT_TRUE(reader.empty)
        << "reader must fail closed to empty, never return partial bytes";
    EXPECT_GE(reader.elapsed_ms, 9000)
        << "shared acquisition shares the 10s bounded wait";
    EXPECT_LE(reader.elapsed_ms, 14000);

    // The contended read never parsed partial bytes: the canary is intact.
    EXPECT_EQ(c14_read_file(inbox), canary);

    ::kill(h.pid, SIGKILL);
    int status = 0;
    ASSERT_EQ(::waitpid(h.pid, &status, 0), h.pid);
    reaper.release();
}
