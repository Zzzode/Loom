/// @file test_fix_query_engine.cpp
/// @brief Dedicated QueryEngine test target closing audit finding B1.
///
/// The audit reports query_engine.cppm at 0.86% coverage with no test target
/// referencing it directly. The loom_core aggregator pulls loom_query transitively,
/// but the existing QueryEngine coverage lives inside test_services.cpp (a
/// 280k-line mega-target) which muddies per-file attribution. This file is a
/// focused, standalone QueryEngine test target.
///
/// What is covered here (gaps not exercised elsewhere, plus core seams):
///   (1) add_output_config_to_json via build_output_config_json_for_testing() —
///       both the response_schema branch and the empty/omitted branch.
///   (2) discovered_skills_ tracking across tool calls
///       (execute_single_tool_for_testing → execute_single_tool).
///   (3) Fallback-model selection on OverloadedError (HTTP 529) and
///       RateLimited (HTTP 429) — drives query() through call_api() and
///       observes model_params().model flipping to the fallback entry.
///   (4) User-memory (~/.loom/LOOM.md) + project LOOM.md injection into
///       the initial system prompt at construction time
///       (build_and_add_system_prompt via get_conversation()).
///
///   (5) Streaming tool-error recovery and resuming a failed model request,
///       using scripted HTTP/SSE responses through the public stream_query API.
///
/// All tests use exported/public surfaces only (QueryEngine public methods,
/// build_output_config_json_for_testing, execute_single_tool_for_testing,
/// discovered_skills, get_conversation, model_params).

#include <cstdlib>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

import std;
import loom.query.query_engine;
import loom.tools.tool;
import loom.types.types;
import loom.serdes.json;
import loom.memdir.paths;

namespace fs = std::filesystem;

namespace {

/// RAII guard for an environment variable. Restores the previous value (or
/// unsets it) on destruction. Mirrors the helper duplicated across the
/// existing test files so this translation unit is self-contained.
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

/// Minimal raw-socket HTTP server that returns a scripted sequence of
/// (status, body) responses, one per incoming request, and records the bodies
/// it received. Used to drive QueryEngine.query() through call_api()'s
/// fallback-model branch. Intentionally tiny (no httplib dep) so the test
/// target links only against loom_core.
class ScriptedHttpServer {
public:
    struct Response {
        int status = 200;
        std::string body =
            R"({"id":"msg_test","type":"message","role":"assistant","model":"loom-test","content":[{"type":"text","text":"ok"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})";
    };

    explicit ScriptedHttpServer(std::vector<Response> responses)
        : responses_(std::move(responses)) {
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
        thread_ = std::thread([this] { accept_loop(); });
    }

    ~ScriptedHttpServer() {
        running_.store(false);
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }
    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    /// Wait until at least `count` request bodies have been captured.
    [[nodiscard]] std::optional<std::vector<std::string>> wait_for_bodies(
        std::size_t count,
        std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this, count] { return bodies_.size() >= count; })) {
            return std::nullopt;
        }
        return bodies_;
    }

    [[nodiscard]] std::optional<std::vector<std::string>> wait_for_request_lines(
        std::size_t count,
        std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this, count] { return request_lines_.size() >= count; })) {
            return std::nullopt;
        }
        return request_lines_;
    }

private:
    struct RawHttpRequest {
        std::string request_line;
        std::string body;
    };

    void accept_loop() {
        while (running_.load()) {
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

    static RawHttpRequest read_request(int fd) {
        std::string request;
        char buffer[4096];
        std::size_t header_end = std::string::npos;
        while ((header_end = request.find("\r\n\r\n")) == std::string::npos) {
            auto n = ::recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0) return {};
            request.append(buffer, buffer + n);
        }
        const auto first_line_end = request.find("\r\n");
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
                try {
                    content_length = static_cast<std::size_t>(
                        std::stoul(header.substr(value_start + 1, value_end - value_start - 1)));
                } catch (...) {}
            }
        }
        const std::size_t body_start = header_end + 4;
        while (request.size() - body_start < content_length) {
            auto n = ::recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0) break;
            request.append(buffer, buffer + n);
        }
        return {
            .request_line = first_line_end == std::string::npos
                ? std::string{}
                : request.substr(0, first_line_end),
            .body = request.substr(body_start, content_length),
        };
    }

    void handle_client(int fd) {
        auto request = read_request(fd);
        if (request.request_line.empty()) return;
        std::size_t idx = 0;
        {
            std::lock_guard lock(mutex_);
            request_lines_.push_back(std::move(request.request_line));
            bodies_.push_back(std::move(request.body));
            idx = bodies_.size() - 1;
        }
        cv_.notify_all();

        Response resp;
        if (!responses_.empty()) {
            resp = responses_[std::min(idx, responses_.size() - 1)];
        }

        std::string reason = "OK";
        if (resp.status == 429) reason = "Too Many Requests";
        else if (resp.status == 529) reason = "Overloaded";
        else if (resp.status >= 400) reason = "Error";

        const auto response = std::format(
            "HTTP/1.1 {} {}\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
            resp.status, reason, resp.body.size(), resp.body);
        send_all(fd, response);
    }

    static void send_all(int fd, std::string_view data) {
        std::size_t sent = 0;
        while (sent < data.size()) {
            auto n = ::send(fd, data.data() + sent, data.size() - sent, 0);
            if (n <= 0) break;
            sent += static_cast<std::size_t>(n);
        }
    }

    int listen_fd_ = -1;
    std::uint16_t port_ = 0;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::string> request_lines_;
    std::vector<std::string> bodies_;
    std::vector<Response> responses_;
};

/// Extract the text of the first system message in a conversation, if any.
std::optional<std::string> first_system_prompt_text(const std::vector<loom::core::Message>& conv) {
    for (const auto& msg : conv) {
        if (const auto* sys = std::get_if<loom::core::SystemMessage>(&msg)) {
            for (const auto& block : sys->content) {
                if (const auto* text = std::get_if<loom::core::TextBlock>(&block)) {
                    return text->text;
                }
            }
        }
    }
    return std::nullopt;
}

} // namespace

// ===========================================================================
// (2 already-covered seams, re-asserted here for a standalone target)
// ===========================================================================

TEST(QueryEngineFix, AbortBeforeCallbackRegistrationIsReplayed) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), registry);

    int calls = 0;
    engine.abort();
    engine.set_abort_callback([&] { ++calls; });
    EXPECT_EQ(calls, 1);
    engine.set_abort_callback(nullptr);
}

TEST(QueryEngineFix, AbortAfterCallbackRegistrationInvokesCallback) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), registry);

    int calls = 0;
    engine.set_abort_callback([&] { ++calls; });
    EXPECT_EQ(calls, 0);
    engine.abort();
    EXPECT_EQ(calls, 1);
    engine.set_abort_callback(nullptr);
}

TEST(QueryEngineFix, ExternalAbortBeforeCallbackRegistrationIsReplayed) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), registry);

    int calls = 0;
    engine.set_external_abort_callback([&] {
        // The host predicate can reenter registration as well as the callback.
        engine.set_abort_callback(nullptr);
        return true;
    });
    engine.set_abort_callback([&] { ++calls; });
    EXPECT_EQ(calls, 1);
    engine.set_abort_callback(nullptr);
}

TEST(QueryEngineFix, ReplayedAbortCallbackCanClearItself) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), registry);

    int calls = 0;
    engine.abort();
    engine.set_abort_callback([&] {
        ++calls;
        engine.set_abort_callback(nullptr);
    });
    EXPECT_EQ(calls, 1);
    engine.abort();
    EXPECT_EQ(calls, 1);
}

TEST(QueryEngineFix, ClearingAbortCallbackDoesNotReplayIt) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), registry);

    int calls = 0;
    engine.set_abort_callback([&] { ++calls; });
    engine.abort();
    ASSERT_EQ(calls, 1);
    engine.set_abort_callback(nullptr);
    engine.abort();
    EXPECT_EQ(calls, 1);
}

TEST(QueryEngineFix, OutputConfigInjectsResponseSchemaAndTaskBudget) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    config.response_schema = loom::core::QueryEngineConfig::ResponseSchema{
        .name = "result",
        .schema_json = R"({"type":"object","properties":{"answer":{"type":"string"}},"required":["answer"]})",
    };
    config.task_budget = loom::core::QueryEngineConfig::TaskBudget{
        .total = 5'000,
        .remaining = 2'500,
    };
    loom::core::QueryEngine engine(std::move(config), registry);

    const auto out = engine.build_output_config_json_for_testing();
    // response_schema → format.json_schema{ name, schema }
    EXPECT_NE(out.find("output_config"), std::string::npos);
    EXPECT_NE(out.find("json_schema"), std::string::npos);
    EXPECT_NE(out.find("\"result\""), std::string::npos);
    EXPECT_NE(out.find("answer"), std::string::npos);
    // task_budget → {type:"tokens", total, remaining}
    EXPECT_NE(out.find("task_budget"), std::string::npos);
    EXPECT_NE(out.find("tokens"), std::string::npos);
    EXPECT_NE(out.find("5000"), std::string::npos);
    EXPECT_NE(out.find("2500"), std::string::npos);
}

TEST(QueryEngineFix, OutputConfigOmittedWhenNeitherBudgetNorSchema) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), registry);

    // Matches the contract documented on build_output_config_json_for_testing.
    EXPECT_EQ(engine.build_output_config_json_for_testing(), "{}");
}

// ===========================================================================
// (4) discovered_skills_ tracking via execute_single_tool_for_testing
// ===========================================================================

namespace {
struct ModelContextProbe final : loom::core::ITool {
    loom::core::ToolDefinition definition_{};
    std::vector<std::optional<std::string>> models;
    std::vector<std::string> inputs;
    ModelContextProbe() {
        definition_.name = "model_probe";
        definition_.permission = loom::core::ToolPermission::ReadOnly;
    }
    const loom::core::ToolDefinition& definition() const override { return definition_; }
    loom::core::Result<loom::core::ToolResult> execute(const loom::core::ToolInput& input) override {
        models.push_back(input.parent_model);
        inputs.emplace_back(input.json());
        return loom::core::ToolResult::success("observed");
    }
    bool check_permission(const loom::core::ToolInput&) const override { return true; }
};
}

TEST(QueryEngineFix, ToolModelContextTracksRuntimeModelChangesWithoutChangingJson) {
    loom::core::ToolRegistry registry;
    auto probe = std::make_unique<ModelContextProbe>();
    auto* observed = probe.get();
    registry.register_tool(std::move(probe));
    loom::core::QueryEngineConfig config;
    config.model_params.model = "configured-model";
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), registry);
    loom::core::ToolUseBlock call{
        .id = loom::core::ToolUseId{.value = "model-context"},
        .name = "model_probe",
        .input_json = R"({"model":"explicit-child-model"})",
    };
    (void)engine.execute_single_tool_for_testing(call);
    auto params = engine.model_params();
    params.model = "switched-model";
    engine.set_model_params(std::move(params));
    (void)engine.execute_single_tool_for_testing(call);
    ASSERT_EQ(observed->models.size(), 2u);
    EXPECT_EQ(observed->models[0], "configured-model");
    EXPECT_EQ(observed->models[1], "switched-model");
    EXPECT_EQ(observed->inputs, (std::vector<std::string>{call.input_json, call.input_json}));
}

TEST(QueryEngineFix, ToolModelContextUsesActualFallbackModel) {
    ScriptedHttpServer server({
        {.status = 529, .body = R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})"},
        {.status = 200, .body = R"({"id":"msg_tool","type":"message","role":"assistant","model":"fallback-model","content":[{"type":"tool_use","id":"tool_probe","name":"model_probe","input":{}}],"stop_reason":"tool_use","usage":{"input_tokens":1,"output_tokens":1}})"},
        {},
    });
    ASSERT_NE(server.port(), 0);
    const auto root = fs::temp_directory_path() / "loom_query_engine_fallback_context_test";
    fs::remove_all(root);
    fs::create_directories(root);
    loom::core::ToolRegistry registry;
    auto probe = std::make_unique<ModelContextProbe>();
    auto* observed = probe.get();
    registry.register_tool(std::move(probe));
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 1;
    config.model_params.model = "primary-model";
    config.fallback_models = {"fallback-model"};
    config.context_window.auto_compact = false;
    config.tools = registry.get_visible_definitions();
    loom::core::QueryEngine engine(std::move(config), registry);
    auto response = engine.query("probe fallback model");
    ASSERT_TRUE(response.has_value()) << response.error().message;
    ASSERT_EQ(observed->models.size(), 1u);
    EXPECT_EQ(observed->models[0], "fallback-model");
    fs::remove_all(root);
}

TEST(QueryEngineFix, DiscoveredSkillsTracksSkillToolInvocations) {
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

    // Initially empty.
    EXPECT_TRUE(engine.discovered_skills().empty());

    // First skill invocation → recorded.
    loom::core::ToolUseBlock first{
        .id = loom::core::ToolUseId{.value = "tu-1"},
        .name = "skill",
        .input_json = R"({"name":"alpha-skill"})",
    };
    (void)engine.execute_single_tool_for_testing(first);
    auto after_first = engine.discovered_skills();
    EXPECT_NE(std::find(after_first.begin(), after_first.end(), "alpha-skill"), after_first.end());

    // Second, distinct skill → appended (de-dup set semantics).
    loom::core::ToolUseBlock second{
        .id = loom::core::ToolUseId{.value = "tu-2"},
        .name = "skill",
        .input_json = R"({"name":"beta-skill"})",
    };
    (void)engine.execute_single_tool_for_testing(second);
    auto after_second = engine.discovered_skills();
    EXPECT_NE(std::find(after_second.begin(), after_second.end(), "beta-skill"), after_second.end());

    // Re-invoking alpha should not duplicate (set, not vector).
    const auto alpha_count = std::count(after_second.begin(), after_second.end(), "alpha-skill");
    EXPECT_EQ(alpha_count, 1);

    // A non-skill tool must NOT contribute to discovered_skills_.
    struct StubReadTool final : loom::core::ITool {
        loom::core::ToolDefinition definition_{};
        StubReadTool() {
            definition_.name = "Read";
            definition_.permission = loom::core::ToolPermission::ReadOnly;
        }
        [[nodiscard]] const loom::core::ToolDefinition& definition() const override { return definition_; }
        [[nodiscard]] loom::core::Result<loom::core::ToolResult> execute(const loom::core::ToolInput&) override {
            return loom::core::ToolResult::success("data");
        }
        [[nodiscard]] bool check_permission(const loom::core::ToolInput&) const override { return true; }
    };
    loom::core::ToolRegistry registry2;
    registry2.register_tool(std::make_unique<StubReadTool>());
    loom::core::QueryEngineConfig config2;
    config2.context_window.auto_compact = false;
    config2.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine2(std::move(config2), registry2);
    loom::core::ToolUseBlock read_call{
        .id = loom::core::ToolUseId{.value = "tu-r"},
        .name = "Read",
        .input_json = R"({"file_path":"/tmp/x"})",
    };
    (void)engine2.execute_single_tool_for_testing(read_call);
    EXPECT_TRUE(engine2.discovered_skills().empty())
        << "non-skill tools must not populate discovered_skills_";
}

// ===========================================================================
// (5) User-memory + project LOOM.md loading into the system prompt
// ===========================================================================

TEST(QueryEngineFix, LoadsProjectLoomMdAndUserMemoryIntoSystemPrompt) {
    // Isolate HOME so get_user_memory_path() (~/.loom/LOOM.md) resolves
    // inside our temp dir and we do not read the developer's real user memory.
    auto root = fs::weakly_canonical(fs::temp_directory_path()) /
                "loom_query_engine_memory_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard home_guard("HOME", root.string());

    // Project LOOM.md at cwd root.
    {
        std::ofstream project_md(root / "LOOM.md");
        project_md << "Project-specific guidance: always write tests in English.";
    }
    // User-level memory (~/.loom/LOOM.md).
    fs::create_directories(root / ".loom");
    {
        std::ofstream user_md(root / ".loom" / "LOOM.md");
        user_md << "Global user preference: respond concisely.";
    }

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    loom::core::QueryEngine engine(std::move(config), registry);

    const auto prompt = first_system_prompt_text(engine.get_conversation());
    ASSERT_TRUE(prompt.has_value()) << "system prompt not present in conversation";

    // Project memory injected under <context name="LOOM.md">.
    EXPECT_NE(prompt->find("<context name=\"LOOM.md\">"), std::string::npos);
    EXPECT_NE(prompt->find("Project-specific guidance"), std::string::npos);

    // User memory injected under <context name="UserMemory">.
    EXPECT_NE(prompt->find("<context name=\"UserMemory\">"), std::string::npos);
    EXPECT_NE(prompt->find("Global user preference"), std::string::npos);

    fs::remove_all(root);
}

TEST(QueryEngineFix, OmitsMemoryContextsWhenFilesAbsent) {
    auto root = fs::weakly_canonical(fs::temp_directory_path()) /
                "loom_query_engine_no_memory_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard home_guard("HOME", root.string());
    // Ensure no user memory exists.
    fs::remove(root / ".loom" / "LOOM.md");

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    loom::core::QueryEngine engine(std::move(config), registry);

    const auto prompt = first_system_prompt_text(engine.get_conversation());
    ASSERT_TRUE(prompt.has_value());
    // Neither context block should appear when the files are missing.
    EXPECT_EQ(prompt->find("<context name=\"LOOM.md\">"), std::string::npos);
    EXPECT_EQ(prompt->find("<context name=\"UserMemory\">"), std::string::npos);

    fs::remove_all(root);
}

// ===========================================================================
// (3) Fallback-model selection on OverloadedError (529) / RateLimited (429)
//
// call_api() in query_engine.cppm (~L1514): on OverloadedError or RateLimited
// with a non-empty fallback_models list, it sets
// config_.model_params.model = fallback_models[idx++] and retries immediately
// (no backoff sleep). We observe the model flip via the public model_params()
// getter after query() returns.
// ===========================================================================

TEST(QueryEngineFix, FallsBackToSecondaryModelOnOverloadedError) {
    // Request 0 → 529 (OverloadedError); Request 1 → 200 (success).
    // After request 0, the engine must switch to the fallback model, so the
    // model field in the captured request-1 body should be the fallback.
    ScriptedHttpServer server({
        ScriptedHttpServer::Response{.status = 529, .body = R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})"},
        ScriptedHttpServer::Response{.status = 200, .body = R"({"id":"msg_ok","type":"message","role":"assistant","model":"fallback-model","content":[{"type":"text","text":"done"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})"},
    });
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom_query_engine_fallback_529_test";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 3;       // allow the immediate fallback retry
    config.retry_policy.initial_delay = std::chrono::milliseconds(1);
    config.model_params.model = "primary-model";
    config.fallback_models = {"fallback-model"};
    // Auto-compact off so a 529 body can't accidentally trip compaction paths.
    config.context_window.auto_compact = false;

    loom::core::QueryEngine engine(std::move(config), registry);
    auto response = engine.query("ping");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    // The engine must have flipped its active model to the fallback.
    EXPECT_EQ(engine.model_params().model, "fallback-model")
        << "expected model to switch to fallback after 529 OverloadedError";

    // Both requests must have been made (primary attempt + fallback retry).
    auto bodies = server.wait_for_bodies(2);
    ASSERT_TRUE(bodies.has_value()) << "server did not receive 2 requests";
    ASSERT_EQ(bodies->size(), 2u);

    // The second request should carry the fallback model in its JSON body.
    auto parsed = loom::utils::json::parse((*bodies)[1]);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    EXPECT_EQ(parsed->root().get("model").as_str(), "fallback-model")
        << "fallback retry request should use the fallback model";

    fs::remove_all(root);
}

TEST(QueryEngineFix, PreservesPathPrefixInMessagesBaseUrl) {
    ScriptedHttpServer server({
        ScriptedHttpServer::Response{.status = 200, .body = R"({"id":"msg_ok","type":"message","role":"assistant","model":"primary-model","content":[{"type":"text","text":"done"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})"},
    });
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom_query_engine_base_path_test";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url() + "/api/anthropic";
    config.cwd = root.string();
    config.retry_policy.max_retries = 1;
    config.model_params.model = "primary-model";
    config.context_window.auto_compact = false;

    loom::core::QueryEngine engine(std::move(config), registry);
    auto response = engine.query("ping");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    auto request_lines = server.wait_for_request_lines(1);
    ASSERT_TRUE(request_lines.has_value());
    ASSERT_EQ(request_lines->size(), 1u);
    EXPECT_EQ((*request_lines)[0], "POST /api/anthropic/v1/messages HTTP/1.1");

    fs::remove_all(root);
}

TEST(QueryEngineFix, FallsBackToSecondaryModelOnRateLimited) {
    // Request 0 → 429 (RateLimited); Request 1 → 200 (success).
    ScriptedHttpServer server({
        ScriptedHttpServer::Response{.status = 429, .body = R"({"type":"error","error":{"type":"rate_limit_error","message":"Too many requests"}})"},
        ScriptedHttpServer::Response{.status = 200, .body = R"({"id":"msg_ok","type":"message","role":"assistant","model":"fallback-model","content":[{"type":"text","text":"done"}],"stop_reason":"end_turn","usage":{"input_tokens":1,"output_tokens":1}})"},
    });
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom_query_engine_fallback_429_test";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 3;
    config.retry_policy.initial_delay = std::chrono::milliseconds(1);
    config.model_params.model = "primary-model";
    config.fallback_models = {"fallback-model"};
    config.context_window.auto_compact = false;

    loom::core::QueryEngine engine(std::move(config), registry);
    auto response = engine.query("ping");
    ASSERT_TRUE(response.has_value()) << response.error().message;

    EXPECT_EQ(engine.model_params().model, "fallback-model")
        << "expected model to switch to fallback after 429 RateLimited";

    fs::remove_all(root);
}

TEST(QueryEngineFix, DoesNotFallBackWhenNoFallbackModelsConfigured) {
    // 529 with no fallback_models → call_api returns the error; query()
    // propagates it. The model must remain unchanged.
    ScriptedHttpServer server({
        ScriptedHttpServer::Response{.status = 529, .body = R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})"},
    });
    ASSERT_NE(server.port(), 0);

    auto root = fs::temp_directory_path() / "loom_query_engine_no_fallback_test";
    fs::remove_all(root);
    fs::create_directories(root);

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = root.string();
    config.retry_policy.max_retries = 1;
    config.retry_policy.initial_delay = std::chrono::milliseconds(1);
    config.retry_policy.max_delay = std::chrono::milliseconds(5);
    config.model_params.model = "primary-model";
    // fallback_models intentionally left empty.
    config.context_window.auto_compact = false;

    loom::core::QueryEngine engine(std::move(config), registry);
    auto response = engine.query("ping");
    ASSERT_FALSE(response.has_value())
        << "query should fail when overloaded with no fallback models";

    // Model untouched — no fallback occurred.
    EXPECT_EQ(engine.model_params().model, "primary-model");

    fs::remove_all(root);
}

// ===========================================================================
// SseEventDecoder — the pure SSE framing parser extracted from
// QueryEngine::stream_single_api_call. These tests exercise the previously
// unreachable parser directly: partial chunks, all event types' framing,
// event-type persistence, multi-line data, and edge cases. No HTTP server.
// ===========================================================================

TEST(SseEventDecoder, DecodesSingleCompleteEvent) {
    loom::core::SseEventDecoder dec;
    auto events = dec.feed("event: message_start\n"
                           "data: {\"type\":\"message_start\"}\n\n");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, "message_start");
    EXPECT_EQ(events[0].data, "{\"type\":\"message_start\"}");
}

TEST(SseEventDecoder, DecodesAllEventTypesInOneStream) {
    loom::core::SseEventDecoder dec;
    std::string stream =
        "event: message_start\ndata: {\"a\":1}\n\n"
        "event: content_block_start\ndata: {\"b\":2}\n\n"
        "event: content_block_delta\ndata: {\"c\":3}\n\n"
        "event: content_block_stop\ndata: {\"d\":4}\n\n"
        "event: message_delta\ndata: {\"e\":5}\n\n"
        "event: message_stop\ndata: {\"f\":6}\n\n"
        "event: error\ndata: {\"g\":7}\n\n";
    auto events = dec.feed(stream);
    ASSERT_EQ(events.size(), 7u);
    EXPECT_EQ(events[0].type, "message_start");
    EXPECT_EQ(events[1].type, "content_block_start");
    EXPECT_EQ(events[2].type, "content_block_delta");
    EXPECT_EQ(events[3].type, "content_block_stop");
    EXPECT_EQ(events[4].type, "message_delta");
    EXPECT_EQ(events[5].type, "message_stop");
    EXPECT_EQ(events[6].type, "error");
    EXPECT_EQ(events[6].data, "{\"g\":7}");
}

TEST(SseEventDecoder, EventCompletesOnlyOnBlankLine) {
    loom::core::SseEventDecoder dec;
    // A single trailing newline is not a terminator.
    EXPECT_TRUE(dec.feed("event: x\ndata: y\n").empty());
    // The second newline completes the \n\n terminator.
    auto events = dec.feed("\n");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, "x");
    EXPECT_EQ(events[0].data, "y");
}

TEST(SseEventDecoder, ByteByByteFeedYieldsEventOnlyAtTerminator) {
    loom::core::SseEventDecoder dec;
    const std::string full = "event: content_block_delta\ndata: {\"delta\":1}\n\n";
    std::vector<loom::core::SseEvent> all;
    for (std::size_t i = 0; i < full.size(); ++i) {
        auto ev = dec.feed(full.substr(i, 1));
        if (i + 1 < full.size()) {
            ASSERT_TRUE(ev.empty()) << "decoder emitted before terminator at byte " << i;
        }
        for (auto& e : ev) all.push_back(std::move(e));
    }
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].type, "content_block_delta");
    EXPECT_EQ(all[0].data, "{\"delta\":1}");
}

TEST(SseEventDecoder, EventTypePersistsAcrossBlocksWithoutEventLine) {
    loom::core::SseEventDecoder dec;
    // Block 1 sets the type; block 2 carries no `event:` line -> inherits.
    auto events = dec.feed(
        "event: content_block_delta\ndata: {\"i\":1}\n\n"
        "data: {\"i\":2}\n\n");
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].type, "content_block_delta");
    EXPECT_EQ(events[1].type, "content_block_delta");
    EXPECT_EQ(events[1].data, "{\"i\":2}");
}

TEST(SseEventDecoder, MultiLineDataIsJoinedWithNewline) {
    loom::core::SseEventDecoder dec;
    auto events = dec.feed("event: x\ndata: line1\ndata: line2\ndata: line3\n\n");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].data, "line1\nline2\nline3");
}

TEST(SseEventDecoder, EmptyDataLinePreservedAsGap) {
    // "data:" with no value still contributes a (joined) empty segment,
    // matching the original inline parser exactly.
    loom::core::SseEventDecoder dec;
    auto events = dec.feed("event: x\ndata: a\ndata:\ndata: b\n\n");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].data, "a\n\nb");
}

TEST(SseEventDecoder, BlockWithNoDataLineYieldsNoEvent) {
    loom::core::SseEventDecoder dec;
    auto events = dec.feed("event: ping\n\n");
    EXPECT_TRUE(events.empty());
}

TEST(SseEventDecoder, EmitsDoneMarkerForDispatcherToFilter) {
    // The decoder intentionally does NOT filter [DONE]; parse_sse_event does.
    // The decoder must surface it so the dispatcher can ignore it.
    loom::core::SseEventDecoder dec;
    auto events = dec.feed("event: message_stop\ndata: [DONE]\n\n");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].data, "[DONE]");
}

TEST(SseEventDecoder, TrailingPartialBufferHeldAcrossFeeds) {
    loom::core::SseEventDecoder dec;
    EXPECT_TRUE(dec.feed("event: x\ndata: partial").empty());  // no terminator yet
    auto events = dec.feed("_continued\n\n");                   // completes the event
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].data, "partial_continued");
}

TEST(SseEventDecoder, MultipleEventsAcrossFeedsWithPartialTail) {
    loom::core::SseEventDecoder dec;
    auto e1 = dec.feed("event: a\ndata: 1\n\nevent: b\ndata: 2\n\nevent: c\ndata: 3");
    ASSERT_EQ(e1.size(), 2u);  // a and b complete; c's payload is partial
    EXPECT_EQ(e1[0].type, "a");
    EXPECT_EQ(e1[1].type, "b");
    auto e2 = dec.feed("\n\n");  // completes c
    ASSERT_EQ(e2.size(), 1u);
    EXPECT_EQ(e2[0].type, "c");
    EXPECT_EQ(e2[0].data, "3");
}

TEST(SseEventDecoder, RealisticMessagesStreamFramesCorrectly) {
    loom::core::SseEventDecoder dec;
    std::string stream =
        "event: message_start\n"
        "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_1\",\"model\":\"loom-x\"}}\n\n"
        "event: content_block_start\n"
        "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\"}}\n\n"
        "event: content_block_delta\n"
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"Hello\"}}\n\n"
        "event: content_block_delta\n"
        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\" world\"}}\n\n"
        "event: content_block_stop\n"
        "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
        "event: message_delta\n"
        "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\"},\"usage\":{\"output_tokens\":5}}\n\n"
        "event: message_stop\n"
        "data: {\"type\":\"message_stop\"}\n\n";
    auto events = dec.feed(stream);
    ASSERT_EQ(events.size(), 7u);
    EXPECT_EQ(events[0].type, "message_start");
    EXPECT_NE(events[2].data.find("Hello"), std::string::npos);
    EXPECT_NE(events[3].data.find(" world"), std::string::npos);
    EXPECT_EQ(events[5].type, "message_delta");
    EXPECT_NE(events[5].data.find("end_turn"), std::string::npos);
}

// Auto-memory guidance + MEMORY.md index must be injected into the system
// prompt, and the directory created, mirroring TS loadMemoryPrompt() +
// the AutoMem loommd injection.
TEST(QueryEngineFix, InjectsAutoMemoryGuidanceAndMemoryIndex) {
    auto root = fs::weakly_canonical(fs::temp_directory_path()) /
                "loom_auto_memory_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard home_guard("HOME", root.string());

    // Pin the auto-memory dir deterministically via the SDK override.
    const auto mem_dir = root / "custom-memory";
    EnvironmentGuard mem_guard(
        "LOOM_COWORK_MEMORY_PATH_OVERRIDE", mem_dir.string());
    EnvironmentGuard disable_guard(
        "LOOM_DISABLE_AUTO_MEMORY", "0");

    // Pre-existing MEMORY.md index content.
    fs::create_directories(mem_dir);
    {
        std::ofstream idx(mem_dir / "MEMORY.md");
        idx << "- [User role](user_role.md) — user is a platform engineer\n";
    }

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    loom::core::QueryEngine engine(std::move(config), registry);

    const auto prompt = first_system_prompt_text(engine.get_conversation());
    ASSERT_TRUE(prompt.has_value());

    // Guidance teaches the model how to save.
    EXPECT_NE(prompt->find("<context name=\"memory\">"), std::string::npos);
    EXPECT_NE(prompt->find("persistent, file-based memory system"),
              std::string::npos);
    EXPECT_NE(prompt->find(mem_dir.string()), std::string::npos);

    // MEMORY.md index content injected separately.
    EXPECT_NE(prompt->find("<context name=\"MEMORY.md\">"), std::string::npos);
    EXPECT_NE(prompt->find("platform engineer"), std::string::npos);

    fs::remove_all(root);
}

// When auto-memory is disabled, neither the guidance nor the index is
// injected and no memory directory is required.
TEST(QueryEngineFix, AutoMemoryDisabledOmitsGuidance) {
    auto root = fs::weakly_canonical(fs::temp_directory_path()) /
                "loom_auto_memory_disabled_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard disable_guard(
        "LOOM_DISABLE_AUTO_MEMORY", "1");

    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = root.string();
    loom::core::QueryEngine engine(std::move(config), registry);

    const auto prompt = first_system_prompt_text(engine.get_conversation());
    ASSERT_TRUE(prompt.has_value());
    EXPECT_EQ(prompt->find("<context name=\"memory\">"), std::string::npos);
    EXPECT_EQ(prompt->find("<context name=\"MEMORY.md\">"), std::string::npos);

    fs::remove_all(root);
}

// Canonical auto-memory path resolution (TS paths.ts):
// <LOOM_CONFIG_DIR>/projects/<sanitized-root>/memory, with override support.
TEST(QueryEngineFix, AutoMemPathResolvesCanonicalLayoutAndOverride) {
    auto root = fs::weakly_canonical(fs::temp_directory_path()) /
                "loom_mem_path_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", (root / "cfg").string());
    unsetenv("LOOM_COWORK_MEMORY_PATH_OVERRIDE");

    const auto project = root / "my project";
    fs::create_directories(project);
    auto p = loom::memdir::get_auto_mem_path(project);
    ASSERT_TRUE(p.has_value());
    const std::string key =
        loom::memdir::sanitize_memory_key(fs::weakly_canonical(project).string());
    EXPECT_EQ(p->string(), (root / "cfg" / "projects" / key / "memory").string());
    EXPECT_EQ(key.find(' '), std::string::npos); // spaces sanitized to '-'
    EXPECT_EQ(*loom::memdir::get_auto_mem_entrypoint(project), *p / "MEMORY.md");

    EnvironmentGuard ov_set(
        "LOOM_COWORK_MEMORY_PATH_OVERRIDE", (root / "ov").string());
    auto po = loom::memdir::get_auto_mem_path(project);
    ASSERT_TRUE(po.has_value());
    EXPECT_EQ(po->string(), (root / "ov").string());

    EnvironmentGuard dis("LOOM_DISABLE_AUTO_MEMORY", "1");
    EXPECT_FALSE(loom::memdir::get_auto_mem_path(project).has_value());

    fs::remove_all(root);
}


namespace {

std::string recovery_sse_event(std::string_view type, std::string_view payload) {
    return std::format("event: {}\ndata: {}\n\n", type, payload);
}

std::string recovery_json_string(std::string_view value) {
    loom::utils::json::JsonMutDoc doc;
    doc.set_root(doc.string(value));
    return doc.to_string();
}

std::string recovery_message_start() {
    return recovery_sse_event("message_start",
        R"({"type":"message_start","message":{"id":"recovery-message","type":"message","role":"assistant","model":"recovery-model","content":[],"usage":{"input_tokens":1}}})");
}

std::string recovery_message_end(std::string_view stop_reason) {
    return recovery_sse_event("content_block_stop", R"({"index":0})") +
        recovery_sse_event("message_delta", std::format(
            R"({{"delta":{{"stop_reason":{}}},"usage":{{"output_tokens":1}}}})",
            recovery_json_string(stop_reason))) +
        recovery_sse_event("message_stop", R"({"type":"message_stop"})");
}

ScriptedHttpServer::Response recovery_tool_call(
    std::string_view id, std::string_view name, std::string_view input) {
    return {.status = 200, .body = recovery_message_start() +
        recovery_sse_event("content_block_start", std::format(
            R"({{"index":0,"content_block":{{"type":"tool_use","id":{},"name":{},"input":{{}}}}}})",
            recovery_json_string(id), recovery_json_string(name))) +
        recovery_sse_event("content_block_delta", std::format(
            R"({{"index":0,"delta":{{"type":"input_json_delta","partial_json":{}}}}})",
            recovery_json_string(input))) + recovery_message_end("tool_use")};
}

ScriptedHttpServer::Response recovery_final_reply() {
    return {.status = 200, .body = recovery_message_start() +
        recovery_sse_event("content_block_start",
            R"({"index":0,"content_block":{"type":"text","text":""}})") +
        recovery_sse_event("content_block_delta",
            R"({"index":0,"delta":{"type":"text_delta","text":"Recovered successfully"}})") +
        recovery_message_end("end_turn")};
}

struct RecoveryTool final : loom::core::ITool {
    loom::core::ToolDefinition definition_{};
    bool permitted = true;
    mutable int permission_checks = 0;
    std::vector<std::string> executed_inputs;

    explicit RecoveryTool(std::string name) {
        definition_.name = std::move(name);
        definition_.permission = loom::core::ToolPermission::ReadOnly;
    }
    const loom::core::ToolDefinition& definition() const override { return definition_; }
    loom::core::Result<loom::core::ToolResult> execute(const loom::core::ToolInput& input) override {
        executed_inputs.emplace_back(input.json());
        if (input.json().find("broken") != std::string_view::npos) {
            return loom::core::ToolResult::error("bash: syntax error near unexpected token");
        }
        return loom::core::ToolResult::success("tool completed");
    }
    bool check_permission(const loom::core::ToolInput&) const override {
        ++permission_checks;
        return permitted;
    }
};

loom::core::QueryEngineConfig recovery_config(
    const ScriptedHttpServer& server, const loom::core::ToolRegistry& registry) {
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.cwd = fs::temp_directory_path().string();
    config.retry_policy.max_retries = 0;
    config.model_params.model = "recovery-model";
    config.context_window.auto_compact = false;
    config.tools = registry.get_visible_definitions();
    return config;
}

std::vector<loom::core::ToolResultMessage> recovery_tool_results(
    const std::vector<loom::core::Message>& messages) {
    std::vector<loom::core::ToolResultMessage> results;
    for (const auto& message : messages) {
        if (const auto* result = std::get_if<loom::core::ToolResultMessage>(&message)) {
            results.push_back(*result);
        }
    }
    return results;
}

void expect_one_recovery_prompt(const std::vector<loom::core::Message>& messages) {
    EXPECT_EQ(std::count_if(messages.begin(), messages.end(), [](const auto& message) {
        return std::holds_alternative<loom::core::UserMessage>(message);
    }), 1);
}

void expect_recovery_completed(const std::vector<loom::core::Message>& messages) {
    ASSERT_FALSE(messages.empty());
    const auto* assistant = std::get_if<loom::core::AssistantMessage>(&messages.back());
    ASSERT_NE(assistant, nullptr);
    ASSERT_EQ(assistant->content.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<loom::core::TextBlock>(assistant->content[0]));
    EXPECT_EQ(std::get<loom::core::TextBlock>(assistant->content[0]).text,
              "Recovered successfully");
}

void expect_wire_tool_result(std::string_view body, std::string_view id, bool error) {
    auto parsed = loom::utils::json::parse(body);
    ASSERT_TRUE(parsed.has_value());
    bool found = false;
    parsed->root().get("messages").iter([&](auto message) {
        message.get("content").iter([&](auto block) {
            if (block.get("type").as_str() == "tool_result" &&
                block.get("tool_use_id").as_str() == id) {
                found = true;
                EXPECT_EQ(block.get("is_error").as_bool(), error);
            }
        });
    });
    EXPECT_TRUE(found) << "Missing tool result " << id;
}

} // namespace

TEST(QueryEngineFix, LargeToolOutputPreservesUtf8AtWireBudgetBoundary) {
    struct LargeReadTool final : loom::core::ITool {
        loom::core::ToolDefinition definition_{};
        std::string output;
        explicit LargeReadTool(std::string value) : output(std::move(value)) {
            definition_.name = "Read";
            definition_.permission = loom::core::ToolPermission::ReadOnly;
        }
        const loom::core::ToolDefinition& definition() const override { return definition_; }
        loom::core::Result<loom::core::ToolResult> execute(const loom::core::ToolInput&) override {
            return loom::core::ToolResult::success(output);
        }
        bool check_permission(const loom::core::ToolInput&) const override { return true; }
    };

    // Exercise every cut position within two-, three-, and four-byte scalars,
    // including a complete scalar exactly at the budget and ASCII boundaries.
    std::vector<std::pair<std::string, std::string>> cases;
    for (const std::string& scalar : {std::string{"é"}, std::string{"─"}, std::string{"😀"}}) {
        for (std::size_t retained = 1; retained <= scalar.size(); ++retained) {
            const std::string prefix(30000 - retained, 'x');
            cases.emplace_back(prefix + scalar + "tail",
                               prefix + (retained == scalar.size() ? scalar : ""));
        }
    }
    cases.emplace_back(std::string(30000, 'x'), std::string(30000, 'x'));
    cases.emplace_back(std::string(30001, 'x'), std::string(30000, 'x'));
    for (const auto& [output, prefix] : cases) {
        SCOPED_TRACE(std::format("output bytes: {}, retained bytes: {}", output.size(), prefix.size()));
        ScriptedHttpServer server({recovery_tool_call("large-read", "Read", "{}"),
                                   recovery_final_reply()});
        ASSERT_NE(server.port(), 0);
        loom::core::ToolRegistry registry;
        registry.register_tool(std::make_unique<LargeReadTool>(output));
        loom::core::QueryEngine engine(recovery_config(server, registry), registry);
        engine.stream_query("read the source");
        const auto results = recovery_tool_results(engine.get_conversation());
        ASSERT_EQ(results.size(), 1u);
        ASSERT_EQ(results[0].content.size(), 1u);
        const auto& text = std::get<loom::core::TextBlock>(results[0].content[0]).text;
        EXPECT_EQ(text, prefix + (output.size() > 30000
            ? "\n\n[Output truncated at 30000 chars. Full output omitted.]" : ""));
        const auto bodies = server.wait_for_bodies(2);
        ASSERT_TRUE(bodies.has_value());
        ASSERT_EQ(bodies->size(), 2u);
        expect_wire_tool_result((*bodies)[1], "large-read", false);
        expect_recovery_completed(engine.get_conversation());
    }
}

TEST(QueryEngineFix, StreamingBashFailureRecoversWithinOriginalTurn) {
    ScriptedHttpServer server({
        recovery_tool_call("bash-bad", "Bash", R"({"command":"broken"})"),
        recovery_tool_call("bash-corrected", "Bash", R"({"command":"printf fixed"})"),
        recovery_final_reply(),
    });
    ASSERT_NE(server.port(), 0);
    loom::core::ToolRegistry registry;
    auto tool = std::make_unique<RecoveryTool>("Bash");
    auto* observed = tool.get();
    registry.register_tool(std::move(tool));
    loom::core::QueryEngine engine(recovery_config(server, registry), registry);
    std::vector<loom::core::StreamEvent> events;
    loom::core::QueryOptions options;
    options.on_event = [&](const auto& event) { events.push_back(event); };
    engine.stream_query("run the command", options);

    EXPECT_EQ(observed->executed_inputs, (std::vector<std::string>{
        R"({"command":"broken"})", R"({"command":"printf fixed"})"}));
    const auto conversation = engine.get_conversation();
    expect_one_recovery_prompt(conversation);
    expect_recovery_completed(conversation);
    const auto results = recovery_tool_results(conversation);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].tool_use_id.value, "bash-bad");
    EXPECT_TRUE(results[0].is_error);
    EXPECT_EQ(results[1].tool_use_id.value, "bash-corrected");
    EXPECT_FALSE(results[1].is_error);
    EXPECT_EQ(std::count_if(events.begin(), events.end(), [](const auto& event) {
        return std::holds_alternative<loom::core::StreamError>(event);
    }), 0);
    std::vector<bool> execution_errors;
    for (const auto& event : events) {
        if (const auto* ended = std::get_if<loom::core::ToolExecutionEnd>(&event)) {
            execution_errors.push_back(ended->is_error);
        }
    }
    EXPECT_EQ(execution_errors, (std::vector<bool>{true, false}));
    const auto bodies = server.wait_for_bodies(3);
    ASSERT_TRUE(bodies.has_value());
    ASSERT_EQ(bodies->size(), 3u);
    expect_wire_tool_result((*bodies)[1], "bash-bad", true);
    expect_wire_tool_result((*bodies)[2], "bash-bad", true);
    expect_wire_tool_result((*bodies)[2], "bash-corrected", false);
}

TEST(QueryEngineFix, StreamingPermissionDenialContinuesWithoutReexecutingDeniedBash) {
    ScriptedHttpServer server({
        recovery_tool_call("bash-denied", "Bash", R"({"command":"read sources"})"),
        recovery_tool_call("grep-allowed", "Grep", R"({"pattern":"sources"})"),
        recovery_final_reply(),
    });
    ASSERT_NE(server.port(), 0);
    loom::core::ToolRegistry registry;
    auto bash = std::make_unique<RecoveryTool>("Bash");
    auto* denied = bash.get();
    bash->permitted = false;
    registry.register_tool(std::move(bash));
    auto grep = std::make_unique<RecoveryTool>("Grep");
    auto* allowed = grep.get();
    registry.register_tool(std::move(grep));
    loom::core::QueryEngine engine(recovery_config(server, registry), registry);
    std::vector<loom::core::StreamEvent> events;
    loom::core::QueryOptions options;
    options.on_event = [&](const auto& event) { events.push_back(event); };
    engine.stream_query("inspect sources", options);

    EXPECT_EQ(denied->permission_checks, 1);
    EXPECT_TRUE(denied->executed_inputs.empty());
    EXPECT_FALSE(denied->permitted);
    EXPECT_EQ(allowed->executed_inputs.size(), 1u);
    const auto conversation = engine.get_conversation();
    expect_one_recovery_prompt(conversation);
    expect_recovery_completed(conversation);
    const auto results = recovery_tool_results(conversation);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_TRUE(results[0].is_error);
    ASSERT_FALSE(results[0].content.empty());
    ASSERT_TRUE(std::holds_alternative<loom::core::TextBlock>(results[0].content[0]));
    EXPECT_NE(std::get<loom::core::TextBlock>(results[0].content[0]).text.find("Permission denied"),
              std::string::npos);
    EXPECT_FALSE(results[1].is_error);
    EXPECT_EQ(std::count_if(events.begin(), events.end(), [](const auto& event) {
        return std::holds_alternative<loom::core::StreamError>(event);
    }), 0);
    const auto bodies = server.wait_for_bodies(3);
    ASSERT_TRUE(bodies.has_value());
    ASSERT_EQ(bodies->size(), 3u);
    expect_wire_tool_result((*bodies)[1], "bash-denied", true);
    expect_wire_tool_result((*bodies)[2], "bash-denied", true);
    expect_wire_tool_result((*bodies)[2], "grep-allowed", false);
}

TEST(QueryEngineFix, StreamingApiRetryResumesCommittedConversationWithoutRepeatingBash) {
    const ScriptedHttpServer::Response overloaded{
        .status = 529,
        .body = R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})",
    };
    ScriptedHttpServer server({
        recovery_tool_call("bash-once", "Bash", R"({"command":"printf once"})"),
        overloaded,
        overloaded,
        recovery_final_reply(),
    });
    ASSERT_NE(server.port(), 0);
    loom::core::ToolRegistry registry;
    auto tool = std::make_unique<RecoveryTool>("Bash");
    auto* observed = tool.get();
    registry.register_tool(std::move(tool));
    loom::core::QueryEngine engine(recovery_config(server, registry), registry);
    std::vector<loom::core::StreamEvent> events;
    loom::core::QueryOptions options;
    options.attachments = {loom::core::TextBlock{"attached source contents"}};
    options.on_event = [&](const auto& event) { events.push_back(event); };
    engine.stream_query("inspect @source", options);
    ASSERT_EQ(std::count_if(events.begin(), events.end(), [](const auto& event) {
        return std::holds_alternative<loom::core::StreamError>(event);
    }), 1);
    const auto prior_results = recovery_tool_results(engine.get_conversation());
    ASSERT_EQ(prior_results.size(), 1u);
    EXPECT_FALSE(prior_results[0].is_error);
    ASSERT_EQ(observed->executed_inputs.size(), 1u);

    events.clear();
    options.resume_from_conversation = true;
    engine.stream_query("inspect @source", options);

    EXPECT_EQ(observed->executed_inputs.size(), 1u);
    const auto conversation = engine.get_conversation();
    expect_one_recovery_prompt(conversation);
    expect_recovery_completed(conversation);
    const auto results = recovery_tool_results(conversation);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].tool_use_id.value, prior_results[0].tool_use_id.value);
    EXPECT_EQ(results[0].is_error, prior_results[0].is_error);
    EXPECT_EQ(std::get<loom::core::TextBlock>(results[0].content[0]).text,
              std::get<loom::core::TextBlock>(prior_results[0].content[0]).text);
    for (const auto& message : conversation) {
        if (const auto* user = std::get_if<loom::core::UserMessage>(&message)) {
            ASSERT_EQ(user->content.size(), 2u);
            EXPECT_EQ(std::get<loom::core::TextBlock>(user->content[0]).text, "inspect @source");
            EXPECT_EQ(std::get<loom::core::TextBlock>(user->content[1]).text,
                      "attached source contents");
        }
    }
    EXPECT_EQ(std::count_if(events.begin(), events.end(), [](const auto& event) {
        return std::holds_alternative<loom::core::StreamError>(event);
    }), 0);
    const auto bodies = server.wait_for_bodies(4);
    ASSERT_TRUE(bodies.has_value());
    ASSERT_EQ(bodies->size(), 4u);
    const auto failed_request = loom::utils::json::parse((*bodies)[1]);
    const auto fallback_request = loom::utils::json::parse((*bodies)[2]);
    const auto resumed_request = loom::utils::json::parse((*bodies)[3]);
    ASSERT_TRUE(failed_request.has_value());
    ASSERT_TRUE(fallback_request.has_value());
    ASSERT_TRUE(resumed_request.has_value());
    EXPECT_EQ(failed_request->root().get("messages").to_string(),
              fallback_request->root().get("messages").to_string());
    EXPECT_EQ(failed_request->root().get("messages").to_string(),
              resumed_request->root().get("messages").to_string());
    expect_wire_tool_result((*bodies)[3], "bash-once", false);
}

TEST(QueryEngineFix, StreamingRetryRejectsEmptyAndCompletedConversation) {
    loom::core::ToolRegistry registry;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    loom::core::QueryEngine engine(std::move(config), registry);
    std::vector<std::string> errors;
    loom::core::QueryOptions options;
    options.resume_from_conversation = true;
    options.on_event = [&](const auto& event) {
        if (const auto* error = std::get_if<loom::core::StreamError>(&event))
            errors.push_back(error->error_type);
    };
    const auto initial_size = engine.get_conversation().size();
    engine.stream_query("must not append", options);
    EXPECT_EQ(engine.get_conversation().size(), initial_size);
    loom::core::AssistantMessage completed;
    completed.content.push_back(loom::core::TextBlock{"Completed answer"});
    engine.append_message_for_testing(completed);
    engine.stream_query("must not append", options);
    EXPECT_EQ(engine.get_conversation().size(), initial_size + 1);
    EXPECT_EQ(errors, (std::vector<std::string>{"invalid_retry", "invalid_retry"}));
}

TEST(QueryEngineFix, StreamingRecoveryStopsAtExistingToolRoundLimit) {
    ScriptedHttpServer server({
        recovery_tool_call("bash-bad", "Bash", R"({"command":"broken"})"),
    });
    ASSERT_NE(server.port(), 0);
    loom::core::ToolRegistry registry;
    auto tool = std::make_unique<RecoveryTool>("Bash");
    auto* observed = tool.get();
    registry.register_tool(std::move(tool));
    loom::core::QueryEngine engine(recovery_config(server, registry), registry);
    std::vector<std::string> errors;
    loom::core::QueryOptions options;
    options.max_tool_rounds = 1;
    options.on_event = [&](const auto& event) {
        if (const auto* error = std::get_if<loom::core::StreamError>(&event))
            errors.push_back(error->error_type);
    };
    engine.stream_query("run the command", options);
    EXPECT_EQ(observed->executed_inputs.size(), 1u);
    const auto results = recovery_tool_results(engine.get_conversation());
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].is_error);
    EXPECT_EQ(errors, (std::vector<std::string>{"tool_round_limit"}));
}

TEST(QueryEngineFix, StreamingUnlimitedToolRoundBudgetCompletesBeyondTwentyRounds) {
    std::vector<ScriptedHttpServer::Response> responses;
    for (int round = 0; round < 21; ++round) {
        responses.push_back(recovery_tool_call(std::format("bash-{}", round), "Bash",
            std::format(R"({{"command":"printf round-{}"}})", round)));
    }
    responses.push_back(recovery_final_reply());
    ScriptedHttpServer server(std::move(responses));
    ASSERT_NE(server.port(), 0);
    loom::core::ToolRegistry registry;
    auto tool = std::make_unique<RecoveryTool>("Bash");
    auto* observed = tool.get();
    registry.register_tool(std::move(tool));
    loom::core::QueryEngine engine(recovery_config(server, registry), registry);
    std::vector<std::string> errors;
    loom::core::QueryOptions options;
    options.max_tool_rounds = std::nullopt;
    options.on_event = [&](const auto& event) {
        if (const auto* error = std::get_if<loom::core::StreamError>(&event))
            errors.push_back(error->error_type);
    };
    engine.stream_query("complete every round", options);

    ASSERT_EQ(observed->executed_inputs.size(), 21u);
    EXPECT_EQ(observed->executed_inputs.back(), R"({"command":"printf round-20"})");
    const auto conversation = engine.get_conversation();
    expect_one_recovery_prompt(conversation);
    expect_recovery_completed(conversation);
    const auto results = recovery_tool_results(conversation);
    ASSERT_EQ(results.size(), 21u);
    EXPECT_EQ(results.back().tool_use_id.value, "bash-20");
    EXPECT_TRUE(errors.empty());
    const auto bodies = server.wait_for_bodies(22);
    ASSERT_TRUE(bodies.has_value());
    ASSERT_EQ(bodies->size(), 22u);
    expect_wire_tool_result(bodies->back(), "bash-0", false);
    expect_wire_tool_result(bodies->back(), "bash-20", false);
    EXPECT_EQ(engine.get_usage().input_tokens, 22u);
    EXPECT_EQ(engine.get_usage().output_tokens, 22u);
}

TEST(QueryEngineFix, StreamingDefaultToolRoundBudgetPreservesNoninteractiveLimit) {
    std::vector<ScriptedHttpServer::Response> responses;
    for (int round = 0; round < 21; ++round) {
        responses.push_back(recovery_tool_call(std::format("bash-{}", round), "Bash",
            std::format(R"({{"command":"printf round-{}"}})", round)));
    }
    ScriptedHttpServer server(std::move(responses));
    ASSERT_NE(server.port(), 0);
    loom::core::ToolRegistry registry;
    auto tool = std::make_unique<RecoveryTool>("Bash");
    auto* observed = tool.get();
    registry.register_tool(std::move(tool));
    loom::core::QueryEngine engine(recovery_config(server, registry), registry);
    std::vector<loom::core::StreamError> errors;
    loom::core::QueryOptions options;
    options.on_event = [&](const auto& event) {
        if (const auto* error = std::get_if<loom::core::StreamError>(&event))
            errors.push_back(*error);
    };
    engine.stream_query("bounded unattended work", options);

    EXPECT_EQ(observed->executed_inputs.size(), 20u);
    const auto conversation = engine.get_conversation();
    expect_one_recovery_prompt(conversation);
    ASSERT_TRUE(std::holds_alternative<loom::core::ToolResultMessage>(conversation.back()));
    const auto results = recovery_tool_results(conversation);
    ASSERT_EQ(results.size(), 20u);
    EXPECT_EQ(results.back().tool_use_id.value, "bash-19");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].error_type, "tool_round_limit");
    EXPECT_EQ(errors[0].message, "Tool-round budget reached (20). Progress saved.");
    const auto bodies = server.wait_for_bodies(20);
    ASSERT_TRUE(bodies.has_value());
    EXPECT_EQ(bodies->size(), 20u);
}

TEST(QueryEngineFix, StreamingToolRoundBudgetContinuesWithoutRepeatingPromptAttachmentsOrTools) {
    ScriptedHttpServer server({
        recovery_tool_call("bash-first", "Bash", R"({"command":"printf first"})"),
        recovery_tool_call("bash-second", "Bash", R"({"command":"printf second"})"),
        recovery_final_reply(),
    });
    ASSERT_NE(server.port(), 0);
    loom::core::ToolRegistry registry;
    auto tool = std::make_unique<RecoveryTool>("Bash");
    auto* observed = tool.get();
    registry.register_tool(std::move(tool));
    loom::core::QueryEngine engine(recovery_config(server, registry), registry);
    std::vector<loom::core::StreamError> errors;
    std::vector<loom::core::Message> committed;
    loom::core::QueryOptions options;
    options.max_tool_rounds = 1;
    options.attachments = {loom::core::TextBlock{"attached source contents"}};
    options.on_commit = [&](const auto& message) { committed.push_back(message); };
    options.on_event = [&](const auto& event) {
        if (const auto* error = std::get_if<loom::core::StreamError>(&event))
            errors.push_back(*error);
    };
    engine.stream_query("inspect @source", options);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors.back().error_type, "tool_round_limit");
    EXPECT_EQ(errors.back().message, "Tool-round budget reached (1). Progress saved.");
    ASSERT_EQ(recovery_tool_results(engine.get_conversation()).size(), 1u);

    options.resume_from_conversation = true;
    engine.stream_query("must not append another prompt", options);
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_EQ(errors.back().error_type, "tool_round_limit");
    ASSERT_EQ(recovery_tool_results(engine.get_conversation()).size(), 2u);
    engine.stream_query("must not append another prompt", options);

    EXPECT_EQ(errors.size(), 2u);
    EXPECT_EQ(observed->executed_inputs, (std::vector<std::string>{
        R"({"command":"printf first"})", R"({"command":"printf second"})"}));
    const auto conversation = engine.get_conversation();
    expect_one_recovery_prompt(conversation);
    expect_recovery_completed(conversation);
    for (const auto& message : conversation) {
        if (const auto* user = std::get_if<loom::core::UserMessage>(&message)) {
            ASSERT_EQ(user->content.size(), 2u);
            EXPECT_EQ(std::get<loom::core::TextBlock>(user->content[0]).text, "inspect @source");
            EXPECT_EQ(std::get<loom::core::TextBlock>(user->content[1]).text,
                      "attached source contents");
        }
    }
    expect_one_recovery_prompt(committed);
    ASSERT_EQ(committed.size(), 6u);
    EXPECT_EQ(recovery_tool_results(committed).size(), 2u);
    const auto bodies = server.wait_for_bodies(3);
    ASSERT_TRUE(bodies.has_value());
    ASSERT_EQ(bodies->size(), 3u);
    expect_wire_tool_result((*bodies)[1], "bash-first", false);
    expect_wire_tool_result((*bodies)[2], "bash-first", false);
    expect_wire_tool_result((*bodies)[2], "bash-second", false);
    EXPECT_EQ(engine.get_usage().input_tokens, 3u);
    EXPECT_EQ(engine.get_usage().output_tokens, 3u);
}

TEST(QueryEngineFix, StreamingToolRoundContinueApiFailureRetryPreservesCommittedHistory) {
    const ScriptedHttpServer::Response overloaded{
        .status = 529,
        .body = R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})",
    };
    ScriptedHttpServer server({
        recovery_tool_call("bash-first", "Bash", R"({"command":"printf first"})"),
        overloaded,
        overloaded,
        recovery_tool_call("bash-second", "Bash", R"({"command":"printf second"})"),
        recovery_final_reply(),
    });
    ASSERT_NE(server.port(), 0);
    loom::core::ToolRegistry registry;
    auto tool = std::make_unique<RecoveryTool>("Bash");
    auto* observed = tool.get();
    registry.register_tool(std::move(tool));
    loom::core::QueryEngine engine(recovery_config(server, registry), registry);
    std::vector<std::string> errors;
    loom::core::QueryOptions options;
    options.max_tool_rounds = 1;
    options.on_event = [&](const auto& event) {
        if (const auto* error = std::get_if<loom::core::StreamError>(&event))
            errors.push_back(error->error_type);
    };
    engine.stream_query("perform both commands", options);
    ASSERT_EQ(errors, (std::vector<std::string>{"tool_round_limit"}));
    const auto paused_size = engine.get_conversation().size();
    options.resume_from_conversation = true;
    engine.stream_query({}, options);
    ASSERT_EQ(errors, (std::vector<std::string>{"tool_round_limit", "overloaded_error"}));
    EXPECT_EQ(engine.get_conversation().size(), paused_size);
    ASSERT_EQ(observed->executed_inputs.size(), 1u);

    engine.stream_query({}, options);
    ASSERT_EQ(errors, (std::vector<std::string>{
        "tool_round_limit", "overloaded_error", "tool_round_limit"}));
    engine.stream_query({}, options);
    EXPECT_EQ(errors.size(), 3u);
    EXPECT_EQ(observed->executed_inputs, (std::vector<std::string>{
        R"({"command":"printf first"})", R"({"command":"printf second"})"}));
    expect_one_recovery_prompt(engine.get_conversation());
    expect_recovery_completed(engine.get_conversation());
    ASSERT_EQ(recovery_tool_results(engine.get_conversation()).size(), 2u);
    const auto bodies = server.wait_for_bodies(5);
    ASSERT_TRUE(bodies.has_value());
    ASSERT_EQ(bodies->size(), 5u);
    const auto failed = loom::utils::json::parse((*bodies)[1]);
    const auto fallback = loom::utils::json::parse((*bodies)[2]);
    const auto retried = loom::utils::json::parse((*bodies)[3]);
    ASSERT_TRUE(failed.has_value());
    ASSERT_TRUE(fallback.has_value());
    ASSERT_TRUE(retried.has_value());
    EXPECT_EQ(failed->root().get("messages").to_string(),
              fallback->root().get("messages").to_string());
    EXPECT_EQ(failed->root().get("messages").to_string(),
              retried->root().get("messages").to_string());
    expect_wire_tool_result((*bodies)[4], "bash-first", false);
    expect_wire_tool_result((*bodies)[4], "bash-second", false);
}

TEST(QueryEngineFix, StreamingCancellationAtToolRoundBoundaryDoesNotReportBudgetPause) {
    ScriptedHttpServer server({
        recovery_tool_call("bash-once", "Bash", R"({"command":"printf once"})"),
        recovery_final_reply(),
    });
    ASSERT_NE(server.port(), 0);
    loom::core::ToolRegistry registry;
    auto tool = std::make_unique<RecoveryTool>("Bash");
    auto* observed = tool.get();
    registry.register_tool(std::move(tool));
    loom::core::QueryEngine engine(recovery_config(server, registry), registry);
    std::vector<std::string> errors;
    loom::core::QueryOptions options;
    options.max_tool_rounds = 1;
    options.on_commit = [&](const auto& message) {
        if (std::holds_alternative<loom::core::ToolResultMessage>(message)) engine.abort();
    };
    options.on_event = [&](const auto& event) {
        if (const auto* error = std::get_if<loom::core::StreamError>(&event))
            errors.push_back(error->error_type);
    };
    engine.stream_query("run once", options);
    EXPECT_TRUE(errors.empty());
    ASSERT_EQ(recovery_tool_results(engine.get_conversation()).size(), 1u);
    ASSERT_EQ(observed->executed_inputs.size(), 1u);
    options.resume_from_conversation = true;
    engine.stream_query({}, options);
    EXPECT_TRUE(errors.empty());
    EXPECT_EQ(observed->executed_inputs.size(), 1u);
    expect_recovery_completed(engine.get_conversation());
}

TEST(QueryEngineFix, StreamingUnlimitedRoundsDoNotBypassDollarBudgetOnContinue) {
    ScriptedHttpServer server({
        recovery_tool_call("bash-once", "Bash", R"({"command":"printf once"})"),
        recovery_final_reply(),
    });
    ASSERT_NE(server.port(), 0);
    loom::core::ToolRegistry registry;
    auto tool = std::make_unique<RecoveryTool>("Bash");
    auto* observed = tool.get();
    registry.register_tool(std::move(tool));
    auto config = recovery_config(server, registry);
    config.max_budget_usd = 0.000000001;
    loom::core::QueryEngine engine(std::move(config), registry);
    std::vector<std::string> errors;
    loom::core::QueryOptions options;
    options.max_tool_rounds = std::nullopt;
    options.on_event = [&](const auto& event) {
        if (const auto* error = std::get_if<loom::core::StreamError>(&event))
            errors.push_back(error->error_type);
    };
    engine.stream_query("respect the cost limit", options);
    ASSERT_EQ(errors, (std::vector<std::string>{"budget_exceeded"}));
    EXPECT_TRUE(engine.budget_tracker().budget_exceeded);
    EXPECT_GE(engine.budget_tracker().current_spend_usd,
              engine.budget_tracker().max_budget_usd);
    const auto paused_size = engine.get_conversation().size();
    options.resume_from_conversation = true;
    engine.stream_query({}, options);
    EXPECT_EQ(errors, (std::vector<std::string>{"budget_exceeded", "budget_exceeded"}));
    EXPECT_EQ(engine.get_conversation().size(), paused_size);
    EXPECT_EQ(observed->executed_inputs.size(), 1u);
    const auto bodies = server.wait_for_bodies(1);
    ASSERT_TRUE(bodies.has_value());
    EXPECT_EQ(bodies->size(), 1u);
}

TEST(QueryEngineFix, StreamingHttpFailuresPreservePermanentAndRetryableCategories) {
    for (const auto& [status, expected] : std::vector<std::pair<int, std::string>>{
             {401, "authentication_error"}, {403, "authentication_error"},
             {400, "invalid_request_error"}, {429, "rate_limit_error"},
             {529, "overloaded_error"}}) {
        const ScriptedHttpServer::Response failure{.status = status,
            .body = R"({"type":"error","error":{"message":"request failed"}})"};
        ScriptedHttpServer server({failure, failure});
        ASSERT_NE(server.port(), 0);
        loom::core::ToolRegistry registry;
        loom::core::QueryEngine engine(recovery_config(server, registry), registry);
        std::vector<std::string> errors;
        loom::core::QueryOptions options;
        options.on_event = [&](const auto& event) {
            if (const auto* error = std::get_if<loom::core::StreamError>(&event))
                errors.push_back(error->error_type);
        };
        engine.stream_query("hello", options);
        EXPECT_EQ(errors, (std::vector<std::string>{expected})) << status;
    }
}
