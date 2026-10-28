// Host agent actions must pass the same permissions and hooks as model tools.
#include <gtest/gtest.h>
#include <httplib.h>

#include <cstdlib>

import std;
import loom.query.query_engine;
import loom.tools.tool;
import loom.tools.agent_runtime;
import loom.tools.team;
import loom.hooks.tool_permissions;
import loom.hooks.execution;
import loom.hooks.registry;
import loom.serdes.json;

namespace {
namespace runtime = loom::tools::agent_runtime;
namespace json = loom::utils::json;
namespace hr = loom::utils::hooks_registry;

struct HostAgentProbe final : loom::core::ITool {
    loom::core::ToolDefinition definition_{};
    int calls = 0;
    bool cancelled = false;
    std::optional<std::string> session;

    HostAgentProbe() { definition_.name = "Agent"; }

    const loom::core::ToolDefinition& definition() const override { return definition_; }

    loom::core::Result<loom::core::ToolResult>
    execute(const loom::core::ToolInput& input) override {
        ++calls;
        session = input.parent_session_id;
        cancelled = input.is_cancelled && input.is_cancelled();
        return loom::core::ToolResult::success("Started background agent owned");
    }

    bool check_permission(const loom::core::ToolInput&) const override { return true; }
};

class AgentHostControls : public ::testing::Test {
protected:
    std::filesystem::path root;
    std::optional<std::string> previous_runtime;
    std::optional<std::string> previous_teams;
    loom::core::ToolRegistry registry;
    std::unique_ptr<loom::core::QueryEngine> engine;
    HostAgentProbe* probe = nullptr;

    void SetUp() override {
        root = std::filesystem::temp_directory_path() /
               std::format("loom-host-agent-{}",
                           std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(root);
        if (const char* old = std::getenv("LOOM_AGENT_RUNTIME_DIR")) previous_runtime = old;
        ::setenv("LOOM_AGENT_RUNTIME_DIR", root.c_str(), 1);
        if (const char* old = std::getenv("LOOM_TEAM_RUNTIME_DIR")) previous_teams = old;
        ::setenv("LOOM_TEAM_RUNTIME_DIR", (root / "teams").c_str(), 1);
        loom::tools::global_team_store().clear_for_testing();
        runtime::native_agent_store().clear_for_testing();
        auto tool = std::make_unique<HostAgentProbe>();
        probe = tool.get();
        registry.register_tool(std::move(tool));
        loom::core::QueryEngineConfig config;
        config.cwd = root.string();
        config.context_window.auto_compact = false;
        engine = std::make_unique<loom::core::QueryEngine>(config, registry);
        engine->set_session_id("owner");
        auto record = runtime::make_native_agent_record("owned", "general-purpose");
        record.status = runtime::NativeAgentStatus::Failed;
        record.parent_session_id = "owner";
        record.transcript = {"user: original work", "assistant: retained output"};
        record.error = "original failure";
        record.notification_delivered = true;
        runtime::native_agent_store().upsert(std::move(record));
    }

    void TearDown() override {
        engine.reset();
        runtime::native_agent_store().clear_for_testing();
        if (previous_runtime)
            ::setenv("LOOM_AGENT_RUNTIME_DIR", previous_runtime->c_str(), 1);
        else
            ::unsetenv("LOOM_AGENT_RUNTIME_DIR");
        loom::tools::global_team_store().clear_for_testing();
        if (previous_teams)
            ::setenv("LOOM_TEAM_RUNTIME_DIR", previous_teams->c_str(), 1);
        else
            ::unsetenv("LOOM_TEAM_RUNTIME_DIR");
        std::filesystem::remove_all(root);
    }
};
}  // namespace

TEST_F(AgentHostControls, ContinueUsesCurrentContextAndDoesNotFabricateRootToolHistory) {
    engine->abort();
    const auto before = engine->get_conversation().size();
    auto result = engine->continue_agent_instance("owned", "Continue previous work");
    ASSERT_TRUE(result) << result.error().format();
    EXPECT_EQ(*result, "owned");
    EXPECT_EQ(probe->calls, 1);
    EXPECT_EQ(probe->session, "owner");
    EXPECT_FALSE(probe->cancelled);
    EXPECT_EQ(engine->get_conversation().size(), before);
    EXPECT_TRUE(runtime::native_agent_store().get("owned")->notification_delivered);
}

TEST_F(AgentHostControls, PermissionDenialDoesNotDispatchOrMutateRecord) {
    loom::hooks::ToolPermissionHook permission;
    permission.set_ask_user_response_fn([](const auto&) {
        loom::hooks::PermissionResponse response;
        response.decision = loom::hooks::PermissionDecision::deny;
        response.message = "explicit denial";
        return response;
    });
    engine->set_permission_hook(&permission);
    EXPECT_FALSE(engine->continue_agent_instance("owned", "Continue"));
    EXPECT_EQ(probe->calls, 0);
    EXPECT_EQ(runtime::native_agent_store().get("owned")->error, "original failure");
    EXPECT_TRUE(runtime::native_agent_store().get("owned")->notification_delivered);
}

TEST_F(AgentHostControls, PermissionPayloadCannotRedirectTargetOrSpawnFreshAgent) {
    loom::hooks::ToolPermissionHook permission;
    std::string rewritten;
    permission.set_ask_user_response_fn([&](const auto&) {
        loom::hooks::PermissionResponse response;
        response.decision = loom::hooks::PermissionDecision::allow;
        response.updated_input_json = rewritten;
        return response;
    });
    engine->set_permission_hook(&permission);
    for (
        const auto* payload :
        {R"({"agent_id":"foreign","subagent_type":"general-purpose","prompt":"redirect","resume_existing":true,"run_in_background":true})",
         R"({"agent_id":"owned","subagent_type":"general-purpose","prompt":"redirect","resume_existing":false,"run_in_background":true})",
         R"({"agent_id":"owned","subagent_type":"general-purpose","prompt":"redirect","resume_existing":true,"run_in_background":false})",
         R"({"agent_id":"owned","subagent_type":"Explore","prompt":"redirect","resume_existing":true,"run_in_background":true})",
         R"({"agent_id":"owned","subagent_type":"general-purpose","prompt":"redirect","resume_existing":true,"run_in_background":true,"team_name":"foreign-team"})",
         R"({"agent_id":"owned","subagent_type":"general-purpose","prompt":"redirect","resume_existing":true,"run_in_background":true,"cwd":"/foreign-directory"})"}) {
        rewritten = payload;
        auto result = engine->continue_agent_instance("owned", "Continue");
        EXPECT_FALSE(result) << rewritten;
        EXPECT_EQ(probe->calls, 0);
        EXPECT_FALSE(runtime::native_agent_store().get("foreign"));
        EXPECT_EQ(runtime::native_agent_store().get("owned")->error, "original failure");
    }
}

TEST_F(AgentHostControls, PreToolUsePayloadCannotRedirectTarget) {
    json::JsonMutDoc doc;
    auto object = doc.object();
    object.add("action", doc.string("retry"));
    object.add(
        "modified_payload",
        doc.string(
            R"({"agent_id":"foreign","subagent_type":"general-purpose","prompt":"redirect","resume_existing":true,"run_in_background":true})"));
    doc.set_root(object);
    httplib::Server server;
    std::atomic<int> hook_calls{0};
    server.Post("/hook", [&](const auto&, auto& response) {
        ++hook_calls;
        response.set_content(doc.to_string(), "application/json");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    ASSERT_GT(port, 0);
    std::jthread serving([&] { server.listen_after_bind(); });

    struct StopServer {
        httplib::Server& server;

        ~StopServer() { server.stop(); }
    } stop_server{server};

    server.wait_until_ready();
    hr::HttpHookConfig command;
    command.url = std::format("http://127.0.0.1:{}/hook", port);
    hr::IndividualHookConfig hook;
    hook.event = hr::HookEventType::PreToolUse;
    hook.config = command;
    hook.source = hr::HookSource::UserSettings;
    engine->set_user_hooks({hook}, {});
    auto result = engine->continue_agent_instance("owned", "Continue");
    EXPECT_EQ(hook_calls.load(), 1);
    EXPECT_FALSE(result);
    EXPECT_EQ(probe->calls, 0);
    EXPECT_FALSE(runtime::native_agent_store().get("foreign"));
    EXPECT_EQ(runtime::native_agent_store().get("owned")->error, "original failure");
}

TEST_F(AgentHostControls, RejectsForeignNestedTeamAndActiveInstances) {
    auto record = *runtime::native_agent_store().get("owned");
    record.parent_session_id = "foreign";
    runtime::native_agent_store().upsert(record);
    EXPECT_FALSE(engine->continue_agent_instance("owned", "Continue"));
    record.parent_session_id = "owner";
    record.parent_agent_id = "parent";
    runtime::native_agent_store().upsert(record);
    EXPECT_FALSE(engine->continue_agent_instance("owned", "Continue"));
    record.parent_agent_id.reset();
    record.teammate_backend = "";
    runtime::native_agent_store().upsert(record);
    EXPECT_FALSE(engine->continue_agent_instance("owned", "Continue"));
    record.teammate_backend.reset();
    runtime::native_agent_store().begin_execution(record, registry.background_stop_token());
    EXPECT_FALSE(engine->continue_agent_instance("owned", "Continue"));
    runtime::native_agent_store().end_execution("owned");
    EXPECT_EQ(probe->calls, 0);
}

TEST_F(AgentHostControls, RunningMessageQueuesWithoutPolicyDispatchOrRootMutation) {
    auto& store = runtime::native_agent_store();
    auto record = *store.get("owned");
    record.status = runtime::NativeAgentStatus::Queued;
    store.begin_execution(record, registry.background_stop_token());
    EXPECT_FALSE(engine->send_agent_instance_message("owned", "startup rejected"));
    store.mark_running("owned");
    ASSERT_TRUE(store.open_input("owned"));
    const auto root_before = engine->get_conversation().size();
    const auto sent = engine->send_agent_instance_message("owned", "target guidance");
    ASSERT_TRUE(sent) << sent.error().format();
    EXPECT_NE(sent->find("queued"), std::string::npos);
    EXPECT_EQ(probe->calls, 0);
    EXPECT_EQ(engine->get_conversation().size(), root_before);
    EXPECT_EQ(store.get("owned")->pending_messages, (std::vector<std::string>{"target guidance"}));
    EXPECT_TRUE(store.get("owned")->notification_delivered);
    store.close_input("owned");
    EXPECT_FALSE(engine->send_agent_instance_message("owned", "finalization rejected"));
    store.end_execution("owned");
}

TEST_F(AgentHostControls, EndedMessageUsesExistingPolicyContinuationAndReportsStarted) {
    const auto sent = engine->send_agent_instance_message("owned", "explicit continuation");
    ASSERT_TRUE(sent) << sent.error().format();
    EXPECT_NE(sent->find("started"), std::string::npos);
    EXPECT_EQ(probe->calls, 1);
}

TEST_F(AgentHostControls, NativeTeamContinuationRequiresTheOriginalExistingMembership) {
    auto& teams = loom::tools::global_team_store();
    ASSERT_TRUE(teams.create("team", "team", {loom::tools::TeamMember{.agent_id = "owned"}}));
    auto record = *runtime::native_agent_store().get("owned");
    record.team_name = "team";
    record.name = "member";
    record.teammate_backend = "in-process";
    runtime::native_agent_store().upsert(record);
    const auto accepted = engine->send_agent_instance_message("owned", "continue same member");
    ASSERT_TRUE(accepted) << accepted.error().format();
    EXPECT_EQ(probe->calls, 1);
    auto team = teams.get("team");
    ASSERT_TRUE(team);
    ASSERT_TRUE(teams.remove("team"));
    ASSERT_TRUE(teams.create("team", "team", {}));
    const auto denied = engine->send_agent_instance_message("owned", "must not recreate member");
    ASSERT_FALSE(denied);
    EXPECT_NE(denied.error().format().find("no longer a member"), std::string::npos);
    EXPECT_EQ(probe->calls, 1);
    EXPECT_TRUE(teams.get("team")->members.empty());
    ASSERT_TRUE(teams.remove("team"));
    EXPECT_NE(engine->agent_message_unavailable_reason("owned").find("no longer available"),
              std::string::npos);
}

TEST_F(AgentHostControls, TeamIdentityMutationDuringPermissionCannotRedirectContinuation) {
    auto& teams = loom::tools::global_team_store();
    ASSERT_TRUE(teams.create("team", "team", {loom::tools::TeamMember{.agent_id = "owned"}}));
    auto record = *runtime::native_agent_store().get("owned");
    record.team_name = "team";
    record.teammate_backend = "in-process";
    runtime::native_agent_store().upsert(record);
    loom::hooks::ToolPermissionHook permission;
    permission.set_ask_user_response_fn([&](const auto&) {
        record.teammate_backend = "tmux";
        runtime::native_agent_store().upsert(record);
        loom::hooks::PermissionResponse response;
        response.decision = loom::hooks::PermissionDecision::allow;
        return response;
    });
    engine->set_permission_hook(&permission);
    EXPECT_FALSE(engine->send_agent_instance_message("owned", "continue"));
    EXPECT_EQ(probe->calls, 0);
    EXPECT_EQ(runtime::native_agent_store().get("owned")->teammate_backend, "tmux");
}
