/// @file test_session.cpp
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

bool send_all(int fd, std::string_view data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        auto n = ::send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
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

} // namespace

TEST(SessionIngress, PostsSessionEventsWithBearerAuth) {
    LocalCcrHttpServer server;
    ASSERT_TRUE(server.ready());

    auto created = loom::services::api::create_ingress(loom::services::api::IngressConfig{
        .endpoint = server.base_url(),
        .session_id = "session_1",
        .auth_token = "session-jwt-token",
        .organization_uuid = std::nullopt,
    });
    ASSERT_TRUE(created.has_value()) << created.error();

    auto sent = loom::services::api::send_ingress_message(
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

    loom::services::api::close_ingress();
}

TEST(SessionIngress, PostsSessionEventsWithSessionCookieAuth) {
    LocalCcrHttpServer server;
    ASSERT_TRUE(server.ready());

    auto created = loom::services::api::create_ingress(loom::services::api::IngressConfig{
        .endpoint = server.base_url(),
        .session_id = "session_1",
        .auth_token = "sk-ant-sid01-test",
        .organization_uuid = "org-uuid-1",
    });
    ASSERT_TRUE(created.has_value()) << created.error();

    auto sent = loom::services::api::send_ingress_message(R"({"type":"progress","value":0.5})");
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

    loom::services::api::close_ingress();
}

TEST(SessionIngress, CreatesIngressFromDaemonEnvironmentAndSendsLifecycleEvent) {
    EnvironmentGuard endpoint("LOOM_REMOTE_API_BASE_URL", "placeholder");
    EnvironmentGuard session("LOOM_REMOTE_SESSION_ID", "session_1");
    EnvironmentGuard token("LOOM_SESSION_ACCESS_TOKEN", "env-session-token");
    EnvironmentUnsetGuard compat_endpoint("LOOM_REMOTE_API_BASE_URL");
    EnvironmentUnsetGuard ingress_endpoint("LOOM_SESSION_INGRESS_URL");
    EnvironmentUnsetGuard compat_ingress_endpoint("LOOM_SESSION_INGRESS_URL");

    LocalCcrHttpServer server;
    ASSERT_TRUE(server.ready());
    setenv("LOOM_REMOTE_API_BASE_URL", server.base_url().c_str(), 1);

    loom::services::api::close_ingress();
    auto created = loom::services::api::create_ingress_from_environment();
    ASSERT_TRUE(created.has_value()) << created.error();
    EXPECT_TRUE(*created);
    EXPECT_TRUE(loom::services::api::is_ingress_active());

    auto sent = loom::services::api::send_ingress_lifecycle_event("started", std::string_view{"work_1"});
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

    loom::services::api::close_ingress();
}

TEST(SessionMemoryService, StoresSearchesAndDeletesMemoryItems) {
    loom::services::memory::SessionMemoryService service;
    const auto now = std::chrono::system_clock::now();
    loom::services::memory::MemoryItem item{
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

TEST(MemoryExtraction, LlmPromptContainsDirInstructionsAndTranscript) {
    namespace em = loom::services::extract_memories;
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
    namespace em = loom::services::extract_memories;
    EXPECT_GE(em::kExtractionMinNewMessages, 1u);
}

// End of file
