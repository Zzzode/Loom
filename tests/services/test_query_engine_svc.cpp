/// @file test_query_engine_svc.cpp
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

class DefinitionOnlyTool final : public loom::core::ITool {
public:
    explicit DefinitionOnlyTool(loom::core::ToolDefinition definition)
        : definition_(std::move(definition)) {}

    [[nodiscard]] const loom::core::ToolDefinition& definition() const override {
        return definition_;
    }

    [[nodiscard]] loom::core::Result<loom::core::ToolResult> execute(
        const loom::core::ToolInput& /*input*/) override {
        return loom::core::ToolResult::success("unused");
    }

    [[nodiscard]] bool check_permission(const loom::core::ToolInput& /*input*/) const override {
        return true;
    }

private:
    loom::core::ToolDefinition definition_;
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

} // namespace

TEST(QueryEngine, AppliesPerQueryEnabledToolsToMessagesRequest) {
    LocalMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-engine-tool-filter-test";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Disabled;
    config.tools = {
        loom::core::ToolDefinition{
            .name = "Read",
            .description = "Read a file",
            .input_schema = loom::core::InputSchema{
                .properties = {
                    loom::core::SchemaProperty{
                        .name = "file_path",
                        .type = "string",
                        .description = "File path",
                        .required = true,
                    },
                },
            },
            .permission = loom::core::ToolPermission::ReadOnly,
        },
        loom::core::ToolDefinition{
            .name = "Write",
            .description = "Write a file",
            .input_schema = loom::core::InputSchema{
                .properties = {
                    loom::core::SchemaProperty{
                        .name = "file_path",
                        .type = "string",
                        .description = "File path",
                        .required = true,
                    },
                },
            },
            .permission = loom::core::ToolPermission::Write,
        },
    };

    loom::core::QueryEngine engine(std::move(config), registry);
    loom::core::QueryOptions options;
    options.enabled_tools = {"Read"};

    auto response = engine.query("hello", options);
    ASSERT_TRUE(response.has_value()) << response.error().message;
    EXPECT_EQ(response->message.model, "loom-test");

    auto request_body = server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    auto parsed = loom::utils::json::parse(*request_body);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();

    auto tools = parsed->root().get("tools");
    ASSERT_TRUE(tools.valid());
    ASSERT_TRUE(tools.is_arr());

    std::vector<std::string> tool_names;
    tools.iter([&](loom::utils::json::JsonVal tool) {
        tool_names.emplace_back(tool.get("name").as_str());
    });

    ASSERT_EQ(tool_names.size(), 1u) << *request_body;
    EXPECT_EQ(tool_names.front(), "Read");
    EXPECT_FALSE(parsed->root().get("stream").as_bool());

    fs::remove_all(root);
}

TEST(QueryEngine, SnipMetadataProjectsRemovedMessagesFromMessagesRequest) {
    LocalMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-engine-snip-projection-test";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Disabled;
    loom::core::QueryEngine engine(std::move(config), registry);

    loom::core::UserMessage old_user{};
    old_user.id.value = "snipped-user-id";
    old_user.timestamp = std::chrono::system_clock::now();
    old_user.content.push_back(loom::core::TextBlock{"SNIPPED_USER_PAYLOAD_DO_NOT_SEND"});
    engine.append_message_for_testing(loom::core::Message{std::move(old_user)});

    loom::core::AssistantMessage old_assistant{};
    old_assistant.id.value = "snipped-assistant-id";
    old_assistant.timestamp = std::chrono::system_clock::now();
    old_assistant.content.push_back(loom::core::TextBlock{"SNIPPED_ASSISTANT_PAYLOAD_DO_NOT_SEND"});
    engine.append_message_for_testing(loom::core::Message{std::move(old_assistant)});

    loom::core::SystemMessage snip_boundary{};
    snip_boundary.id.value = "snip-boundary-id";
    snip_boundary.timestamp = std::chrono::system_clock::now();
    snip_boundary.subtype = "snip_boundary";
    snip_boundary.content.push_back(loom::core::TextBlock{"Conversation snipped."});
    snip_boundary.snip_metadata = loom::core::SnipMetadata{
        .removed_uuids = {"snipped-user-id", "snipped-assistant-id"},
    };
    engine.append_message_for_testing(loom::core::Message{std::move(snip_boundary)});

    loom::core::UserMessage survivor{};
    survivor.id.value = "survivor-user-id";
    survivor.timestamp = std::chrono::system_clock::now();
    survivor.content.push_back(loom::core::TextBlock{"SURVIVOR_PAYLOAD_SHOULD_SEND"});
    engine.append_message_for_testing(loom::core::Message{std::move(survivor)});

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
        return std::ranges::any_of(conversation, [&](const loom::core::Message& msg) {
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

TEST(QueryEngine, SerializesTaskBudgetAndApiContextManagementRequestConfig) {
    EnvironmentUnsetGuard disable_thinking_guard("LOOM_DISABLE_THINKING");
    EnvironmentGuard clear_results_guard("USE_API_CLEAR_TOOL_RESULTS", "1");
    EnvironmentGuard clear_uses_guard("USE_API_CLEAR_TOOL_USES", "1");
    EnvironmentGuard max_tokens_guard("API_MAX_INPUT_TOKENS", "1000");
    EnvironmentGuard target_tokens_guard("API_TARGET_INPUT_TOKENS", "250");

    LocalMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-engine-task-budget-context-management-test";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Adaptive;
    config.task_budget = loom::core::QueryEngineConfig::TaskBudget{
        .total = 12'000,
        .remaining = 6'000,
    };

    loom::core::QueryEngine engine(std::move(config), registry);
    auto response = engine.query("hello");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_body = server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    auto parsed = loom::utils::json::parse(*request_body);
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
    EnvironmentUnsetGuard clear_results_guard("USE_API_CLEAR_TOOL_RESULTS");
    EnvironmentUnsetGuard clear_uses_guard("USE_API_CLEAR_TOOL_USES");

    LocalMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-engine-disable-thinking-test";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Adaptive;

    loom::core::QueryEngine engine(std::move(config), registry);
    auto response = engine.query("hello");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_body = server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    auto parsed = loom::utils::json::parse(*request_body);
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
    LocalMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-engine-agent-notification-test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "query-notify-agent",
        .agent_type = "general-purpose",
        .description = "Query notify agent",
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Completed,
        .output = std::string("native agent completed with useful context"),
    });

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Disabled;

    loom::core::QueryEngine engine(std::move(config), registry);
    auto first_response = engine.query("hello");
    ASSERT_TRUE(first_response.has_value()) << first_response.error().message;

    auto first_body = server.wait_for_body();
    ASSERT_TRUE(first_body.has_value());
    auto first_json = loom::utils::json::parse(*first_body);
    ASSERT_TRUE(first_json.has_value()) << first_json.error().message();

    auto message_text = [](loom::utils::json::JsonVal message) {
        auto content = message.get("content");
        if (content.is_str()) return std::string(content.as_str());
        std::string text;
        if (content.is_arr()) {
            content.iter([&](loom::utils::json::JsonVal block) {
                auto block_text = block.get("text");
                if (block_text.is_str()) text += block_text.as_str();
            });
        }
        return text;
    };
    auto notification_count = [&](loom::utils::json::JsonVal messages) {
        std::size_t count = 0;
        messages.iter([&](loom::utils::json::JsonVal message) {
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
    first_messages.iter([&](loom::utils::json::JsonVal message) {
        const auto text = message_text(message);
        if (text.find("<task_notification>") == std::string::npos) return;
        EXPECT_EQ(std::string(message.get("role").as_str()), "user");
        EXPECT_NE(text.find("<task_id>query-notify-agent</task_id>"), std::string::npos);
        EXPECT_NE(text.find("<status>completed</status>"), std::string::npos);
        EXPECT_NE(text.find("native agent completed with useful context"), std::string::npos);
        saw_user_notification = true;
    });
    EXPECT_TRUE(saw_user_notification);

    auto delivered_record = loom::tools::agent_runtime::native_agent_store().get("query-notify-agent");
    ASSERT_TRUE(delivered_record.has_value());
    EXPECT_TRUE(delivered_record->notification_delivered);
    EXPECT_TRUE(loom::tools::agent_runtime::native_agent_store().take_pending_task_notifications().empty());

    auto second_response = engine.query("follow up");
    ASSERT_TRUE(second_response.has_value()) << second_response.error().message;
    auto request_bodies = server.wait_for_bodies(2);
    ASSERT_TRUE(request_bodies.has_value());
    ASSERT_EQ(request_bodies->size(), 2u);

    auto second_json = loom::utils::json::parse(request_bodies->back());
    ASSERT_TRUE(second_json.has_value()) << second_json.error().message();
    auto second_messages = second_json->root().get("messages");
    ASSERT_TRUE(second_messages.is_arr()) << request_bodies->back();
    EXPECT_EQ(notification_count(second_messages), 1u) << request_bodies->back();

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(QueryEngine, PersistsTranscriptToSessionStorage) {
    auto dir = fs::temp_directory_path() / "loom_qe_session_persist_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    config.model_params.model = "test-model";
    loom::core::QueryEngine engine(std::move(config), registry);
    engine.set_session_storage(dir);

    auto make_user = [](std::string text) {
        loom::core::UserMessage msg{};
        msg.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(msg)};
    };
    auto make_assistant = [](std::string text) {
        loom::core::AssistantMessage msg{};
        msg.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(msg)};
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
    auto sessions = loom::session::list_recent_sessions(dir);
    EXPECT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.front().session_id, engine.session_id().str());
    EXPECT_EQ(sessions.front().model, "test-model");

    // flush_session refreshes metadata message_count.
    engine.flush_session();
    auto sessions2 = loom::session::list_recent_sessions(dir);
    ASSERT_EQ(sessions2.size(), 1u);
    EXPECT_EQ(sessions2.front().message_count, 2);

    fs::remove_all(dir);
}

TEST(QueryEngine, StructuredOutputInjectsResponseSchema) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.response_schema = loom::core::QueryEngineConfig::ResponseSchema{
        .name = "result",
        .schema_json = R"({"type":"object","properties":{"answer":{"type":"string"}},"required":["answer"]})",
    };
    loom::core::QueryEngine engine(std::move(config), registry);

    const auto out = engine.build_output_config_json_for_testing();
    EXPECT_NE(out.find("output_config"), std::string::npos);
    EXPECT_NE(out.find("json_schema"), std::string::npos);
    EXPECT_NE(out.find("\"result\""), std::string::npos);
    EXPECT_NE(out.find("answer"), std::string::npos);

    // Without a schema and without a budget, output_config is omitted.
    loom::core::QueryEngineConfig bare;
    bare.context_window.auto_compact = false;
    loom::core::QueryEngine bare_engine(std::move(bare), registry);
    EXPECT_EQ(bare_engine.build_output_config_json_for_testing(), "{}");
}

TEST(QueryEngine, TracksInvokedSkillsInLoop) {
    // In-loop skill dispatch: when the skill tool is invoked, the engine
    // records the skill name in discovered_skills_ (previously a dead field).
    struct StubSkillTool final : loom::core::ITool {
        loom::core::ToolDefinition definition_{};
        StubSkillTool() {
            definition_.name = "skill";
            definition_.permission = loom::core::ToolPermission::ReadOnly;
        }
        [[nodiscard]] const loom::core::ToolDefinition& definition() const override { return definition_; }
        [[nodiscard]] loom::core::Result<loom::core::ToolResult> execute(const loom::core::ToolInput&) override {
            return loom::core::ToolResult::success("ok");
        }
        [[nodiscard]] bool check_permission(const loom::core::ToolInput&) const override { return true; }
    };

    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<StubSkillTool>());
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), registry);

    loom::core::ToolUseBlock tu{
        .id = loom::core::ToolUseId{.value = "tu-1"},
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

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.base_url = "http://127.0.0.1:1";  // never contacted
    config.context_window.auto_compact = false;
    config.cwd = (root / "work").string();
    config.session_id_override = "resume-session-id";
    fs::create_directories(root / "work");

    const auto summary_path = loom::memdir::get_session_memory_path(
        root / "work", "resume-session-id");

    {
        loom::core::QueryEngine engine(config, registry);
        auto make_user = [](std::string text) {
            loom::core::UserMessage msg{};
            msg.id.value = "user-" + text;
            msg.timestamp = std::chrono::system_clock::now();
            msg.content.push_back(loom::core::TextBlock{std::move(text)});
            return loom::core::Message{std::move(msg)};
        };
        auto make_assistant = [](std::string text) {
            loom::core::AssistantMessage msg{};
            msg.id.value = "assistant-" + text;
            msg.timestamp = std::chrono::system_clock::now();
            msg.content.push_back(loom::core::TextBlock{std::move(text)});
            return loom::core::Message{std::move(msg)};
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
    loom::core::QueryEngine resumed(config, registry);
    const std::string body = resumed.build_request_body_for_testing();
    EXPECT_NE(body.find("session-memory"), std::string::npos) << body;
    EXPECT_NE(body.find("legacy requirement alpha"), std::string::npos) << body;

    fs::remove_all(root);
}

TEST(QueryEngine, CompactConversationPreservesSummarizedHistoryDetails) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();

    loom::core::QueryEngine engine(std::move(config), registry);

    auto make_user = [](std::string text) {
        loom::core::UserMessage msg{};
        msg.id.value = "user-" + text;
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(msg)};
    };
    auto make_assistant = [](std::string text) {
        loom::core::AssistantMessage msg{};
        msg.id.value = "assistant-" + text;
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(msg)};
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

    const auto* boundary = std::get_if<loom::core::SystemMessage>(&conversation[1]);
    ASSERT_NE(boundary, nullptr);
    ASSERT_TRUE(boundary->subtype.has_value());
    EXPECT_EQ(*boundary->subtype, "compact_boundary");
    ASSERT_TRUE(boundary->compact_metadata.has_value());
    EXPECT_EQ(boundary->compact_metadata->trigger, "manual");
    EXPECT_GT(boundary->compact_metadata->pre_tokens, 0u);
    ASSERT_TRUE(boundary->compact_metadata->preserved_segment.has_value());
    EXPECT_EQ(boundary->compact_metadata->preserved_segment->head_uuid, "user-recent one");
    EXPECT_EQ(boundary->compact_metadata->preserved_segment->tail_uuid, "assistant-recent six");

    const auto* marker = std::get_if<loom::core::UserMessage>(&conversation[2]);
    ASSERT_NE(marker, nullptr);
    ASSERT_EQ(marker->content.size(), 1u);
    const auto* summary = std::get_if<loom::core::TextBlock>(&marker->content.front());
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(boundary->compact_metadata->preserved_segment->anchor_uuid, marker->id.value);

    EXPECT_NE(summary->text.find("legacy requirement alpha"), std::string::npos);
    EXPECT_NE(summary->text.find("assistant decision beta"), std::string::npos);
    EXPECT_NE(summary->text.find("Preserve these details"), std::string::npos);

    const auto* last = std::get_if<loom::core::AssistantMessage>(&conversation.back());
    ASSERT_NE(last, nullptr);
    const auto* last_text = std::get_if<loom::core::TextBlock>(&last->content.front());
    ASSERT_NE(last_text, nullptr);
    EXPECT_EQ(last_text->text, "recent six");
}

TEST(QueryEngine, CompactConversationCarriesTaskBudgetRemainingIntoNextRequest) {
    LocalMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-task-budget-compact-carry-test";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Disabled;
    config.task_budget = loom::core::QueryEngineConfig::TaskBudget{
        .total = 10'000,
        .remaining = std::nullopt,
    };

    loom::core::QueryEngine engine(std::move(config), registry);

    auto make_user = [](std::string text) {
        loom::core::UserMessage msg{};
        msg.id.value = "user-" + text.substr(0, 12);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(msg)};
    };
    auto make_assistant = [](std::string text) {
        loom::core::AssistantMessage msg{};
        msg.id.value = "assistant-" + text.substr(0, 12);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(msg)};
    };

    for (int i = 0; i < 5; ++i) {
        engine.append_message_for_testing(make_user("legacy user context " + std::to_string(i) + std::string(160, 'u')));
        engine.append_message_for_testing(make_assistant("legacy assistant context " + std::to_string(i) + std::string(160, 'a')));
    }

    auto compacted = engine.compact_conversation("auto");
    ASSERT_TRUE(compacted.has_value());

    auto conversation = engine.get_conversation();
    auto boundary_it = std::ranges::find_if(conversation, [](const loom::core::Message& message) {
        const auto* system = std::get_if<loom::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    ASSERT_NE(boundary_it, conversation.end());
    const auto* boundary = std::get_if<loom::core::SystemMessage>(&*boundary_it);
    ASSERT_NE(boundary, nullptr);
    ASSERT_TRUE(boundary->compact_metadata.has_value());
    const auto pre_tokens = boundary->compact_metadata->pre_tokens;
    ASSERT_GT(pre_tokens, 0u);
    ASSERT_LT(pre_tokens, 10'000u);

    auto response = engine.query("continue after compact");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_body = server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    auto parsed = loom::utils::json::parse(*request_body);
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

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Disabled;
    config.task_budget = loom::core::QueryEngineConfig::TaskBudget{
        .total = 10'000,
        .remaining = std::nullopt,
    };

    loom::core::QueryEngine engine(std::move(config), registry);

    auto make_user = [](std::string text) {
        loom::core::UserMessage msg{};
        msg.id.value = "restore-user-" + text.substr(0, 12);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(msg)};
    };
    auto make_assistant = [](std::string text) {
        loom::core::AssistantMessage msg{};
        msg.id.value = "restore-assistant-" + text.substr(0, 12);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(msg)};
    };

    for (int i = 0; i < 5; ++i) {
        engine.append_message_for_testing(make_user("restored legacy user context " + std::to_string(i) + std::string(160, 'u')));
        engine.append_message_for_testing(make_assistant("restored legacy assistant context " + std::to_string(i) + std::string(160, 'a')));
    }

    auto compacted = engine.compact_conversation("auto");
    ASSERT_TRUE(compacted.has_value());

    auto restored_messages = engine.get_conversation();
    auto boundary_it = std::ranges::find_if(restored_messages, [](const loom::core::Message& message) {
        const auto* system = std::get_if<loom::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    ASSERT_NE(boundary_it, restored_messages.end());
    const auto* boundary = std::get_if<loom::core::SystemMessage>(&*boundary_it);
    ASSERT_NE(boundary, nullptr);
    ASSERT_TRUE(boundary->compact_metadata.has_value());
    const auto pre_tokens = boundary->compact_metadata->pre_tokens;
    ASSERT_GT(pre_tokens, 0u);
    ASSERT_LT(pre_tokens, 10'000u);

    LocalMessagesServer resumed_server;
    ASSERT_NE(resumed_server.port(), 0);
    loom::core::ToolRegistry restored_registry;
    loom::core::QueryEngineConfig restored_config;
    restored_config.api_key = "test-key";
    restored_config.base_url = resumed_server.base_url();
    restored_config.context_window.auto_compact = false;
    restored_config.cwd = root.string();
    restored_config.retry_policy.max_retries = 0;
    restored_config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Disabled;
    restored_config.task_budget = loom::core::QueryEngineConfig::TaskBudget{
        .total = 10'000,
        .remaining = std::nullopt,
    };

    loom::core::QueryEngine restored_engine(std::move(restored_config), restored_registry);
    restored_engine.restore_conversation(std::move(restored_messages));

    auto response = restored_engine.query("continue after restore");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_body = resumed_server.wait_for_body();
    ASSERT_TRUE(request_body.has_value());
    auto parsed = loom::utils::json::parse(*request_body);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    auto task_budget = parsed->root().get("output_config").get("task_budget");
    ASSERT_TRUE(task_budget.is_obj()) << *request_body;
    EXPECT_EQ(task_budget.get("total").as_int(), 10'000);
    EXPECT_EQ(task_budget.get("remaining").as_int(), 10'000 - pre_tokens);

    fs::remove_all(root);
}

TEST(QueryEngine, RepeatedCompactDoesNotSummarizePriorCompactBoundaries) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();

    loom::core::QueryEngine engine(std::move(config), registry);

    auto make_user = [](std::string text) {
        loom::core::UserMessage msg{};
        msg.id.value = "repeat-user-" + text;
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(msg)};
    };
    auto make_assistant = [](std::string text) {
        loom::core::AssistantMessage msg{};
        msg.id.value = "repeat-assistant-" + text;
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(msg)};
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
    auto boundary_count = std::ranges::count_if(conversation, [](const loom::core::Message& message) {
        const auto* system = std::get_if<loom::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    EXPECT_EQ(boundary_count, 1);

    const auto* marker = std::get_if<loom::core::UserMessage>(&conversation.at(2));
    ASSERT_NE(marker, nullptr);
    ASSERT_EQ(marker->content.size(), 1u);
    const auto* summary = std::get_if<loom::core::TextBlock>(&marker->content.front());
    ASSERT_NE(summary, nullptr);
    EXPECT_EQ(summary->text.find("Conversation compacted by manual compact."), std::string::npos);
    EXPECT_NE(summary->text.find("initial 0"), std::string::npos);

    bool kept_second_wave = false;
    for (const auto& message : conversation) {
        const auto* user = std::get_if<loom::core::UserMessage>(&message);
        if (!user || user->content.empty()) continue;
        const auto* text = std::get_if<loom::core::TextBlock>(&user->content.front());
        if (text && text->text == "second wave 0") {
            kept_second_wave = true;
            break;
        }
    }
    EXPECT_TRUE(kept_second_wave);
}

TEST(QueryEngine, AutoCompactWritesBoundaryMetadataAndKeepsRecentTail) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = true;
    config.context_window.max_context_tokens = 2000;
    config.context_window.compaction_threshold = 0.05;
    config.cwd = fs::temp_directory_path().string();

    loom::core::QueryEngine engine(std::move(config), registry);

    auto make_user = [](int index) {
        loom::core::UserMessage msg{};
        msg.id.value = "auto-user-" + std::to_string(index);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(loom::core::TextBlock{
            "auto compact payload " + std::to_string(index) + " " + std::string(900, 'x')});
        return loom::core::Message{std::move(msg)};
    };

    for (int i = 0; i < 12; ++i) {
        engine.append_message_for_testing(make_user(i));
    }

    auto conversation = engine.get_conversation();
    auto boundary_count = std::ranges::count_if(conversation, [](const loom::core::Message& message) {
        const auto* system = std::get_if<loom::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    ASSERT_EQ(boundary_count, 1);
    ASSERT_LE(conversation.size(), 9u);

    auto boundary_it = std::ranges::find_if(conversation, [](const loom::core::Message& message) {
        const auto* system = std::get_if<loom::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    ASSERT_NE(boundary_it, conversation.end());
    const auto boundary_index = static_cast<std::size_t>(std::distance(conversation.begin(), boundary_it));

    const auto* boundary = std::get_if<loom::core::SystemMessage>(&*boundary_it);
    ASSERT_NE(boundary, nullptr);
    ASSERT_TRUE(boundary->compact_metadata.has_value());
    EXPECT_EQ(boundary->compact_metadata->trigger, "auto");
    EXPECT_GT(boundary->compact_metadata->pre_tokens, 0u);
    ASSERT_TRUE(boundary->compact_metadata->preserved_segment.has_value());

    ASSERT_LT(boundary_index + 1, conversation.size());
    const auto* marker = std::get_if<loom::core::UserMessage>(&conversation[boundary_index + 1]);
    ASSERT_NE(marker, nullptr);
    EXPECT_EQ(boundary->compact_metadata->preserved_segment->anchor_uuid, marker->id.value);

    const auto last_id = std::visit([](const auto& msg) { return msg.id.value; }, conversation.back());
    EXPECT_EQ(boundary->compact_metadata->preserved_segment->tail_uuid, last_id);

    ASSERT_EQ(marker->content.size(), 1u);
    const auto* summary = std::get_if<loom::core::TextBlock>(&marker->content.front());
    ASSERT_NE(summary, nullptr);
    EXPECT_NE(summary->text.find("Preserve these details"), std::string::npos);
    EXPECT_NE(summary->text.find("auto compact payload"), std::string::npos);
}

TEST(QueryEngine, ReactiveCompactRetriesPromptTooLongAfterWritingBoundary) {
    LocalMessagesServer server(
        {
            R"({"error":{"type":"invalid_request_error","message":"prompt_too_long"}})",
            R"({"id":"msg_reactive","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"reactive-ok"}],"stop_reason":"end_turn","usage":{"input_tokens":2,"output_tokens":3}})",
        },
        {413, 200});
    ASSERT_NE(server.port(), 0);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Disabled;

    loom::core::QueryEngine engine(std::move(config), registry);

    for (int i = 0; i < 10; ++i) {
        loom::core::UserMessage msg{};
        msg.id.value = "reactive-user-" + std::to_string(i);
        msg.timestamp = std::chrono::system_clock::now();
        msg.content.push_back(loom::core::TextBlock{
            "reactive legacy " + std::to_string(i) + " " + std::string(600, 'r')});
        engine.append_message_for_testing(loom::core::Message{std::move(msg)});
    }

    auto response = engine.query("trigger reactive compact");
    ASSERT_TRUE(response.has_value()) << response.error().message;
    ASSERT_FALSE(response->message.content.empty());
    const auto* response_text = std::get_if<loom::core::TextBlock>(&response->message.content.front());
    ASSERT_NE(response_text, nullptr);
    EXPECT_EQ(response_text->text, "reactive-ok");

    auto request_bodies = server.wait_for_bodies(2);
    ASSERT_TRUE(request_bodies.has_value());
    ASSERT_EQ(request_bodies->size(), 2u);

    auto count_messages = [](std::string_view body) {
        auto parsed = loom::utils::json::parse(body);
        if (!parsed) return std::size_t{0};
        std::size_t count = 0;
        auto messages = parsed->root().get("messages");
        if (messages.is_arr()) {
            messages.iter([&](loom::utils::json::JsonVal) { ++count; });
        }
        return count;
    };

    const auto first_count = count_messages(request_bodies->front());
    const auto second_count = count_messages(request_bodies->back());
    EXPECT_GT(first_count, second_count);
    EXPECT_NE(request_bodies->back().find("Preserve these details"), std::string::npos);
    EXPECT_EQ(request_bodies->back().find("compact_boundary"), std::string::npos);

    auto conversation = engine.get_conversation();
    auto boundary_it = std::ranges::find_if(conversation, [](const loom::core::Message& message) {
        const auto* system = std::get_if<loom::core::SystemMessage>(&message);
        return system && system->subtype && *system->subtype == "compact_boundary";
    });
    ASSERT_NE(boundary_it, conversation.end());
    const auto* boundary = std::get_if<loom::core::SystemMessage>(&*boundary_it);
    ASSERT_NE(boundary, nullptr);
    ASSERT_TRUE(boundary->compact_metadata.has_value());
    EXPECT_EQ(boundary->compact_metadata->trigger, "reactive");
}

TEST(QueryEngine, AppliesMainThreadToolResultBudgetBeforeModelRequest) {
    LocalMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-tool-result-budget-test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());

    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<DefinitionOnlyTool>(loom::core::ToolDefinition{
        .name = "Bash",
        .description = "Execute shell",
        .input_schema = {},
        .permission = loom::core::ToolPermission::Execute,
        .max_result_size_chars = 30'000,
    }));
    registry.register_tool(std::make_unique<DefinitionOnlyTool>(loom::core::ToolDefinition{
        .name = "Read",
        .description = "Read file",
        .input_schema = {},
        .permission = loom::core::ToolPermission::ReadOnly,
        .max_result_size_chars = 0,
        .max_result_size_unbounded = true,
    }));

    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Disabled;

    loom::core::QueryEngine engine(std::move(config), registry);

    loom::core::AssistantMessage assistant{};
    assistant.id.value = "assistant-tool-uses";
    assistant.timestamp = std::chrono::system_clock::now();
    assistant.content.push_back(loom::core::ToolUseBlock{
        .id = loom::core::ToolUseId{"bash-large-1"},
        .name = "Bash",
        .input_json = R"({"command":"one"})",
    });
    assistant.content.push_back(loom::core::ToolUseBlock{
        .id = loom::core::ToolUseId{"bash-large-2"},
        .name = "Bash",
        .input_json = R"({"command":"two"})",
    });
    assistant.content.push_back(loom::core::ToolUseBlock{
        .id = loom::core::ToolUseId{"read-unbounded"},
        .name = "Read",
        .input_json = R"({"file_path":"huge.txt"})",
    });
    engine.append_message_for_testing(loom::core::Message{std::move(assistant)});

    auto append_tool_result = [&](std::string id, std::string text) {
        loom::core::ToolResultMessage result{};
        result.id.value = "result-" + id;
        result.timestamp = std::chrono::system_clock::now();
        result.tool_use_id = loom::core::ToolUseId{std::move(id)};
        result.content.push_back(loom::core::TextBlock{std::move(text)});
        engine.append_message_for_testing(loom::core::Message{std::move(result)});
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
    auto replacement_for = [](const std::vector<loom::core::Message>& messages, std::string_view tool_use_id) {
        for (const auto& message : messages) {
            const auto* tool_result = std::get_if<loom::core::ToolResultMessage>(&message);
            if (!tool_result || tool_result->tool_use_id.value != tool_use_id) continue;
            if (tool_result->content.empty()) return std::optional<std::string>{};
            const auto* text = std::get_if<loom::core::TextBlock>(&tool_result->content.front());
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
        loom::core::ConversationStore store(storage_path.string());
        auto* stored = store.create_conversation();
        for (const auto& message : conversation) {
            stored->add_message(message);
        }
        ASSERT_TRUE(store.save_all().has_value());
    }

    loom::core::ConversationStore loaded(storage_path.string());
    ASSERT_TRUE(loaded.load_all().has_value());
    auto restored_messages = loaded.get_active_conversation()->get_messages();
    auto restored_replacement = replacement_for(restored_messages, "bash-large-1");
    ASSERT_TRUE(restored_replacement.has_value());
    EXPECT_EQ(*restored_replacement, *original_replacement);

    LocalMessagesServer resumed_server;
    ASSERT_NE(resumed_server.port(), 0);
    loom::core::ToolRegistry restored_registry;
    restored_registry.register_tool(std::make_unique<DefinitionOnlyTool>(loom::core::ToolDefinition{
        .name = "Bash",
        .description = "Execute shell",
        .input_schema = {},
        .permission = loom::core::ToolPermission::Execute,
        .max_result_size_chars = 30'000,
    }));
    restored_registry.register_tool(std::make_unique<DefinitionOnlyTool>(loom::core::ToolDefinition{
        .name = "Read",
        .description = "Read file",
        .input_schema = {},
        .permission = loom::core::ToolPermission::ReadOnly,
        .max_result_size_chars = 0,
        .max_result_size_unbounded = true,
    }));

    loom::core::QueryEngineConfig restored_config;
    restored_config.api_key = "test-key";
    restored_config.base_url = resumed_server.base_url();
    restored_config.context_window.auto_compact = false;
    restored_config.cwd = root.string();
    restored_config.retry_policy.max_retries = 0;
    restored_config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Disabled;

    loom::core::QueryEngine restored_engine(std::move(restored_config), restored_registry);
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

    LocalMessagesServer server;
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom-query-time-based-microcompact-test";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    config.retry_policy.max_retries = 0;
    config.thinking_config.mode = loom::core::ThinkingConfig::Mode::Disabled;

    loom::core::QueryEngine engine(std::move(config), registry);

    const auto old_time = std::chrono::system_clock::now() - std::chrono::hours(2);
    loom::core::AssistantMessage assistant{};
    assistant.id.value = "assistant-old-tool-uses";
    assistant.timestamp = old_time;
    assistant.content.push_back(loom::core::ToolUseBlock{
        .id = loom::core::ToolUseId{"read-old"},
        .name = "Read",
        .input_json = R"({"file_path":"old.txt"})",
    });
    assistant.content.push_back(loom::core::ToolUseBlock{
        .id = loom::core::ToolUseId{"bash-old"},
        .name = "Bash",
        .input_json = R"({"command":"old"})",
    });
    assistant.content.push_back(loom::core::ToolUseBlock{
        .id = loom::core::ToolUseId{"task-noncompact"},
        .name = "Task",
        .input_json = R"({"description":"noncompact"})",
    });
    assistant.content.push_back(loom::core::ToolUseBlock{
        .id = loom::core::ToolUseId{"edit-recent"},
        .name = "Edit",
        .input_json = R"({"file_path":"recent.txt"})",
    });
    engine.append_message_for_testing(loom::core::Message{std::move(assistant)});

    auto append_tool_result = [&](std::string id, std::string text) {
        loom::core::ToolResultMessage result{};
        result.id.value = "result-" + id;
        result.timestamp = old_time;
        result.tool_use_id = loom::core::ToolUseId{std::move(id)};
        result.content.push_back(loom::core::TextBlock{std::move(text)});
        engine.append_message_for_testing(loom::core::Message{std::move(result)});
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
            const auto* result = std::get_if<loom::core::ToolResultMessage>(&message);
            if (!result || result->tool_use_id.value != tool_use_id) continue;
            if (result->content.empty()) return std::nullopt;
            const auto* text = std::get_if<loom::core::TextBlock>(&result->content.front());
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

// End of file
