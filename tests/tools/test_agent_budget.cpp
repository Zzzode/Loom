/// @file test_agent_budget.cpp
/// @brief Regression coverage for sub-agent turn-budget completion semantics.

#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"

#include <gtest/gtest.h>
#include <httplib.h>

#include <cstdlib>

import std;
import loom.tools.tool;
import loom.tools.registry;
import loom.orchestration.agent;
import loom.orchestration.agent.utils;
import loom.tools.agent_runtime;

namespace {

namespace fs = std::filesystem;

struct BudgetEnvironmentGuard {
    std::string name;
    std::optional<std::string> previous;

    BudgetEnvironmentGuard(std::string key, const std::string& value)
        : name(std::move(key)) {
        if (const char* current = std::getenv(name.c_str())) previous = current;
        setenv(name.c_str(), value.c_str(), 1);
    }

    ~BudgetEnvironmentGuard() {
        if (previous)
            setenv(name.c_str(), previous->c_str(), 1);
        else
            unsetenv(name.c_str());
    }
};

std::string budget_event(std::string_view type, std::string_view data) {
    return std::format("event: {}\ndata: {}\n\n", type, data);
}

class BudgetMessagesServer {
public:
    BudgetMessagesServer(int tool_turns, std::string partial_text,
                         std::optional<std::string> stop_reason_override = std::nullopt)
        : tool_turns_(tool_turns)
        , partial_text_(std::move(partial_text))
        , stop_reason_override_(std::move(stop_reason_override)) {
        server_.new_task_queue = [] { return new httplib::ThreadPool(2); };
        server_.Post("/v1/messages", [this](const httplib::Request& request,
                                            httplib::Response& response) {
            {
                std::lock_guard lock(mutex_);
                last_request_body_ = request.body;
            }
            const auto turn = ++request_count_;
            const bool tools = turn <= tool_turns_;
            const auto text = tools || stop_reason_override_ ? partial_text_
                                                             : std::string{"budget task complete"};
            auto stream = budget_event(
                "message_start",
                R"({"type":"message_start","message":{"id":"msg_budget","type":"message","role":"assistant","model":"budget-test","content":[]}})");
            int index = 0;
            if (!text.empty()) {
                stream += budget_event(
                    "content_block_start",
                    R"({"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}})");
                stream += budget_event(
                    "content_block_delta",
                    std::format(
                        R"({{"type":"content_block_delta","index":0,"delta":{{"type":"text_delta","text":"{}"}}}})",
                        loom::tools::agent::utils::json_escape_string(text)));
                stream += budget_event("content_block_stop",
                                       R"({"type":"content_block_stop","index":0})");
                ++index;
            }
            if (tools) {
                stream += budget_event(
                    "content_block_start",
                    std::format(
                        R"({{"type":"content_block_start","index":{},"content_block":{{"type":"tool_use","id":"budget_probe_{}","name":"BudgetProbe","input":{{}}}}}})",
                        index, turn));
                stream += budget_event(
                    "content_block_delta",
                    std::format(
                        R"({{"type":"content_block_delta","index":{},"delta":{{"type":"input_json_delta","partial_json":"{{}}"}}}})",
                        index));
                stream += budget_event(
                    "content_block_stop",
                    std::format(R"({{"type":"content_block_stop","index":{}}})", index));
            }
            const auto stop_reason =
                stop_reason_override_.value_or(tools ? "tool_use" : "end_turn");
            stream += budget_event(
                "message_delta",
                stop_reason.empty()
                    ? R"({"type":"message_delta","delta":{},"usage":{"output_tokens":5}})"
                    : std::format(
                          R"({{"type":"message_delta","delta":{{"stop_reason":"{}"}},"usage":{{"output_tokens":5}}}})",
                          stop_reason));
            stream += budget_event("message_stop", R"({"type":"message_stop"})");
            response.set_content(stream, "text/event-stream");
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ > 0) {
            thread_ = std::thread([this] { server_.listen_after_bind(); });
            server_.wait_until_ready();
        }
    }

    ~BudgetMessagesServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    bool valid() const { return port_ > 0; }

    std::string base_url() const { return std::format("http://127.0.0.1:{}", port_); }

    int request_count() const { return request_count_.load(); }

    std::string last_request_body() const {
        std::lock_guard lock(mutex_);
        return last_request_body_;
    }

private:
    int tool_turns_;
    std::string partial_text_;
    std::optional<std::string> stop_reason_override_;
    httplib::Server server_;
    int port_ = 0;
    std::thread thread_;
    std::atomic<int> request_count_{0};
    mutable std::mutex mutex_;
    std::string last_request_body_;
};

class BudgetProbe final : public loom::core::ITool {
public:
    explicit BudgetProbe(std::atomic<int>& calls)
        : calls_(calls) {}

    const loom::core::ToolDefinition& definition() const override { return definition_; }

    loom::core::Result<loom::core::ToolResult> execute(const loom::core::ToolInput&) override {
        ++calls_;
        return loom::core::ToolResult::success("budget probe result");
    }

    bool check_permission(const loom::core::ToolInput&) const override { return true; }

private:
    std::atomic<int>& calls_;
    loom::core::ToolDefinition definition_{.name = "BudgetProbe",
                                           .permission = loom::core::ToolPermission::ReadOnly};
};

class AgentTurnBudget : public testing::Test {
protected:
    static auto& store() { return loom::tools::agent_runtime::native_agent_store(); }

    void SetUp() override {
        previous_cwd_ = fs::current_path();
        root_ = fs::temp_directory_path() /
                std::format("loom_agent_budget_{}_{}",
                            std::chrono::steady_clock::now().time_since_epoch().count(),
                            std::random_device{}());
        fs::create_directories(root_ / ".loom" / "agents");
        fs::current_path(root_);
        environment_.push_back(std::make_unique<BudgetEnvironmentGuard>(
            "LOOM_CONFIG_DIR", (root_ / "config").string()));
        environment_.push_back(std::make_unique<BudgetEnvironmentGuard>(
            "LOOM_AGENT_RUNTIME_DIR", (root_ / "runtime").string()));
        environment_.push_back(
            std::make_unique<BudgetEnvironmentGuard>("LOOM_API_KEY", "test-key"));
        environment_.push_back(
            std::make_unique<BudgetEnvironmentGuard>("LOOM_MODEL", "budget-test"));
        environment_.push_back(
            std::make_unique<BudgetEnvironmentGuard>("LOOM_WIRE_API", "messages"));
        store().clear_for_testing();
    }

    void TearDown() override {
        store().wait_for_background_agents();
        store().clear_for_testing();
        fs::current_path(previous_cwd_);
        environment_.clear();
        fs::remove_all(root_);
    }

    void write_definition(int turns, bool stop_after_tool = false) {
        std::ofstream definition(root_ / ".loom" / "agents" / "budget.md");
        definition << "---\nname: budget-worker\ndescription: Turn budget regression\nmaxTurns: "
                   << turns << "\nhooks:\n  SubagentStop:\n    - command: 'printf x >> "
                   << (root_ / "stop-count").string() << "'\n";
        if (stop_after_tool) {
            std::ofstream hook(root_ / "post-hook.json");
            hook << R"({"continue":false,"stopReason":"intentional budget hook stop"})";
            definition << "  PostToolUse:\n    BudgetProbe:\n      - command: 'cat "
                       << (root_ / "post-hook.json").string() << "'\n";
        }
        definition << "---\nComplete the budget test.\n";
    }

    auto run(BudgetMessagesServer& server, loom::core::ToolRegistry& registry,
             bool background = false) {
        BudgetEnvironmentGuard base_url("LOOM_BASE_URL", server.base_url());
        loom::tools::AgentTool tool({.parent_session_id = std::string("notification-session")}, 0,
                                    &registry);
        auto result = tool.execute(loom::core::ToolInput::from_json(std::format(
            R"({{"description":"Budget regression","prompt":"Use BudgetProbe and finish","subagent_type":"budget-worker","name":"budget-agent","run_in_background":{}}})",
            background)));
        store().wait_for_background_agents();
        return result;
    }

    void expect_stop_hook_once() {
        std::ifstream marker(root_ / "stop-count");
        std::string contents((std::istreambuf_iterator<char>(marker)),
                             std::istreambuf_iterator<char>());
        EXPECT_EQ(contents, "x");
    }

    void expect_incomplete_stop_failure(const std::string& reason, bool emit_tool = false) {
        write_definition(2);
        BudgetMessagesServer server(emit_tool ? 1 : 0, "unfinished investigation", reason);
        ASSERT_TRUE(server.valid());
        std::atomic<int> calls{0};
        loom::core::ToolRegistry registry;
        registry.register_tool(std::make_unique<BudgetProbe>(calls));
        auto result = run(server, registry);
        ASSERT_TRUE(result.has_value()) << result.error().format();
        ASSERT_TRUE(result->is_error);
        ASSERT_FALSE(result->content.empty());
        const auto& error = result->content.front().text;
        EXPECT_NE(error.find("stopped before completing the task"), std::string::npos);
        EXPECT_NE(error.find("stop_reason=" + (reason.empty() ? std::string{"[missing]"} : reason)),
                  std::string::npos);
        EXPECT_NE(error.find("Partial output:\nunfinished investigation"), std::string::npos);
        EXPECT_EQ(server.request_count(), 1);
        EXPECT_EQ(calls.load(), 0);
        const auto record = store().get("budget-agent");
        ASSERT_TRUE(record.has_value());
        EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Failed);
        EXPECT_FALSE(record->output.has_value());
        EXPECT_TRUE(std::ranges::any_of(record->sidechain_entries, [](const auto& entry) {
            return entry.find("unfinished investigation") != std::string::npos;
        }));
        if (emit_tool) {
            EXPECT_TRUE(std::ranges::any_of(record->sidechain_entries, [](const auto& entry) {
                return entry.find("budget_probe_1") != std::string::npos;
            }));
        }
        expect_stop_hook_once();
    }

    fs::path root_;
    fs::path previous_cwd_;
    std::vector<std::unique_ptr<BudgetEnvironmentGuard>> environment_;
};

TEST_F(AgentTurnBudget, OutputTokenLimitPreservesFailureAndPartialText) {
    expect_incomplete_stop_failure("max_tokens");
}

TEST_F(AgentTurnBudget, UnknownStopReasonPreservesFailureAndPartialText) {
    expect_incomplete_stop_failure("unexpected_stop");
}

TEST_F(AgentTurnBudget, MissingStopReasonPreservesFailureAndPartialText) {
    expect_incomplete_stop_failure("");
}

TEST_F(AgentTurnBudget, EndTurnWithPendingToolsFailsWithoutExecutingThem) {
    expect_incomplete_stop_failure("end_turn", true);
}

TEST_F(AgentTurnBudget, OutputTokenLimitWithPendingToolsFailsWithoutExecutingThem) {
    expect_incomplete_stop_failure("max_tokens", true);
}

TEST_F(AgentTurnBudget, ExhaustionReportsFailureAndPreservesPartialOutputAndToolHistory) {
    write_definition(1);
    BudgetMessagesServer server(1, "partial investigation");
    ASSERT_TRUE(server.valid());
    std::atomic<int> calls{0};
    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<BudgetProbe>(calls));
    auto result = run(server, registry);
    ASSERT_TRUE(result.has_value()) << result.error().format();
    ASSERT_TRUE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("maximum of 1 turns before completing"),
              std::string::npos);
    EXPECT_NE(result->content.front().text.find("Partial output:\npartial investigation"),
              std::string::npos);
    EXPECT_EQ(server.request_count(), 1);
    EXPECT_EQ(calls.load(), 1);
    const auto record = store().get("budget-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Failed);
    EXPECT_FALSE(record->output.has_value());
    ASSERT_TRUE(record->error.has_value());
    EXPECT_NE(record->error->find("partial investigation"), std::string::npos);
    EXPECT_TRUE(std::ranges::any_of(record->sidechain_entries, [](const auto& entry) {
        return entry.find("budget probe result") != std::string::npos;
    }));
    expect_stop_hook_once();
}

TEST_F(AgentTurnBudget, ExhaustionWithoutTextDoesNotClaimEmptySuccess) {
    write_definition(1);
    BudgetMessagesServer server(1, "");
    ASSERT_TRUE(server.valid());
    std::atomic<int> calls{0};
    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<BudgetProbe>(calls));
    auto result = run(server, registry);
    ASSERT_TRUE(result.has_value()) << result.error().format();
    ASSERT_TRUE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_EQ(result->content.front().text.find("completed without producing output"),
              std::string::npos);
    EXPECT_EQ(calls.load(), 1);
    expect_stop_hook_once();
}

TEST_F(AgentTurnBudget, NaturalCompletionOnFinalAllowedTurnSucceeds) {
    write_definition(2);
    BudgetMessagesServer server(1, "partial investigation");
    ASSERT_TRUE(server.valid());
    std::atomic<int> calls{0};
    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<BudgetProbe>(calls));
    auto result = run(server, registry);
    ASSERT_TRUE(result.has_value()) << result.error().format();
    ASSERT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_EQ(result->content.front().text, "budget task complete");
    EXPECT_EQ(server.request_count(), 2);
    EXPECT_EQ(calls.load(), 1);
    const auto record = store().get("budget-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Completed);
    expect_stop_hook_once();
}

TEST_F(AgentTurnBudget, HookStopOnFinalAllowedTurnPreservesIntentionalCompletion) {
    write_definition(1, true);
    BudgetMessagesServer server(1, "partial investigation");
    ASSERT_TRUE(server.valid());
    std::atomic<int> calls{0};
    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<BudgetProbe>(calls));
    auto result = run(server, registry);
    ASSERT_TRUE(result.has_value()) << result.error().format();
    ASSERT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("intentional budget hook stop"), std::string::npos);
    EXPECT_EQ(server.request_count(), 1);
    EXPECT_EQ(calls.load(), 1);
    const auto record = store().get("budget-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Completed);
    expect_stop_hook_once();
}

TEST_F(AgentTurnBudget, BackgroundExhaustionPublishesFailedNotification) {
    write_definition(1);
    BudgetMessagesServer server(1, "partial background investigation");
    ASSERT_TRUE(server.valid());
    std::atomic<int> calls{0};
    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<BudgetProbe>(calls));
    auto started = run(server, registry, true);
    ASSERT_TRUE(started.has_value()) << started.error().format();
    ASSERT_FALSE(started->is_error);
    const auto record = store().get("budget-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Failed);
    const auto notifications = store().take_pending_task_events("notification-session");
    ASSERT_EQ(notifications.size(), 1u);
    EXPECT_NE(notifications.front().model_context.find("<status>failed</status>"),
              std::string::npos);
    EXPECT_NE(notifications.front().model_context.find("partial background investigation"),
              std::string::npos);
    EXPECT_EQ(server.request_count(), 1);
    expect_stop_hook_once();
}

TEST_F(AgentTurnBudget, ExhaustedAgentCanResumeWithCompletedToolResultHistory) {
    write_definition(1);
    BudgetMessagesServer server(1, "partial investigation");
    ASSERT_TRUE(server.valid());
    std::atomic<int> calls{0};
    loom::core::ToolRegistry registry;
    registry.register_tool(std::make_unique<BudgetProbe>(calls));
    auto first = run(server, registry);
    ASSERT_TRUE(first.has_value()) << first.error().format();
    ASSERT_TRUE(first->is_error);
    expect_stop_hook_once();
    BudgetEnvironmentGuard base_url("LOOM_BASE_URL", server.base_url());
    loom::tools::AgentTool tool({.parent_session_id = std::string("notification-session")}, 0,
                                &registry);
    auto resumed = tool.execute(loom::core::ToolInput::from_json(
        R"({"description":"Resume budget regression","prompt":"Continue the unfinished task","subagent_type":"budget-worker","agent_id":"budget-agent","resume_existing":true})"));
    ASSERT_TRUE(resumed.has_value()) << resumed.error().format();
    ASSERT_FALSE(resumed->is_error);
    ASSERT_FALSE(resumed->content.empty());
    EXPECT_EQ(resumed->content.front().text, "budget task complete");
    EXPECT_EQ(server.request_count(), 2);
    EXPECT_EQ(calls.load(), 1);
    const auto body = server.last_request_body();
    EXPECT_NE(body.find("budget_probe_1"), std::string::npos);
    EXPECT_NE(body.find("budget probe result"), std::string::npos);
    const auto record = store().get("budget-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Completed);
    EXPECT_FALSE(record->error.has_value());
    std::ifstream marker(root_ / "stop-count");
    std::string contents((std::istreambuf_iterator<char>(marker)),
                         std::istreambuf_iterator<char>());
    EXPECT_EQ(contents, "xx");
}

}  // namespace
