/// @file test_state.cpp


/// persistence, on_change_app_state, ftxui_integration

#include <cstdlib>

#include <gtest/gtest.h>

import std;
import loom.state.app_state;
import loom.state.store;
import loom.state.selectors;
import loom.state.persistence;
import loom.state.on_change;
import loom.state.ftxui_integration;
import loom.session.history;
import loom.types.types;
import loom.cli.update;
import loom.services.mcp.auth;
import loom.services.mcp.types;
import loom.utils.error;
import loom.constants.prompts;

namespace {

[[nodiscard]] std::unique_ptr<loom::state::AppStore> make_test_store() {
    return std::make_unique<loom::state::AppStore>(
        loom::state::get_default_app_state(),
        &loom::state::app_reducer);
}

[[nodiscard]] std::shared_ptr<loom::state::AppStore> make_shared_test_store() {
    return std::make_shared<loom::state::AppStore>(
        loom::state::get_default_app_state(),
        &loom::state::app_reducer);
}

[[nodiscard]] loom::core::Message make_user_message(std::string id, std::string text) {
    return loom::core::UserMessage{
        loom::core::MessageBase{
            loom::core::MessageId{std::move(id)},
            std::chrono::system_clock::now(),
            {loom::core::TextBlock{std::move(text)}}
        }
    };
}

} // namespace
// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════

TEST(AppState, DefaultStateIsValid) {
    auto state = loom::state::get_default_app_state();
    EXPECT_LE(state.created_at, std::chrono::system_clock::now());
    EXPECT_FALSE(state.verbose);
    EXPECT_FALSE(state.is_loading);
    EXPECT_FALSE(state.is_streaming);
}

TEST(AppState, ObservableState) {
    loom::state::ObservableState obs_state;
    int change_count = 0;
    
    auto sub_id = obs_state.subscribe([&change_count](const auto&, const auto&) {
        change_count++;
    });
    EXPECT_NE(sub_id, 0u);
    
    auto new_state = loom::state::get_default_app_state();
    new_state.verbose = true;
    obs_state.set(new_state);
    
    EXPECT_EQ(change_count, 1);
    EXPECT_TRUE(obs_state.get().verbose);
}

// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════

TEST(StateStore, InitialState) {
    auto store = make_test_store();
    auto state = store->get_state();

    EXPECT_LE(state.created_at, std::chrono::system_clock::now());
}

TEST(StateStore, DispatchSetVerbose) {
    auto store = make_test_store();

    store->dispatch(loom::state::Action{loom::state::ActionType::SetVerbose, true});

    auto state = store->get_state();
    EXPECT_TRUE(state.verbose);
}

TEST(StateStore, DispatchSetLoading) {
    auto store = make_test_store();

    store->dispatch(loom::state::Action{loom::state::ActionType::SetLoading, true});

    auto state = store->get_state();
    EXPECT_TRUE(state.is_loading);
}

TEST(StateStore, SubscribeReceivesNotifications) {
    auto store = make_test_store();
    int notify_count = 0;


    auto sub_id = store->subscribe([&notify_count](const auto& /*prev*/, const auto& /*next*/) {
        notify_count++;
    });

    store->dispatch(loom::state::Action{loom::state::ActionType::SetVerbose, true});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetLoading, true});

    EXPECT_EQ(notify_count, 2);


    store->unsubscribe(sub_id);
    store->dispatch(loom::state::Action{loom::state::ActionType::SetStreaming, true});
    EXPECT_EQ(notify_count, 2);
}

TEST(StateStore, UnknownActionNoOp) {
    auto store = make_test_store();
    auto before = store->get_state();


    store->dispatch(loom::state::Action{loom::state::ActionType::EnableTool});
    auto after = store->get_state();

    EXPECT_EQ(before.verbose, after.verbose);
    EXPECT_EQ(before.is_loading, after.is_loading);
}

TEST(StateStore, DispatchPermissionParityActions) {
    auto store = make_test_store();

    store->dispatch(loom::state::Action{loom::state::ActionType::GrantPermission, std::string{"Bash"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::RevokePermission, std::string{"Bash"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::GrantPermission, std::string{"Read"}});

    auto state = store->get_state();
    EXPECT_FALSE(state.tool_permission_context.allowed_tools.contains("Bash"));
    EXPECT_TRUE(state.tool_permission_context.denied_tools.contains("Bash"));
    EXPECT_TRUE(state.tool_permission_context.allowed_tools.contains("Read"));
    EXPECT_FALSE(state.tool_permission_context.denied_tools.contains("Read"));
}

TEST(StateStore, DispatchMessageAndSettingsParityActions) {
    auto store = make_test_store();

    store->dispatch(loom::state::Action{
        loom::state::ActionType::AddMessage,
        make_user_message("msg-1", "first")});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::UpdateLastMessage,
        make_user_message("msg-2", "replacement")});

    loom::state::Settings settings;
    settings.model = "claude-sonnet-4-6";
    settings.theme = "dark";
    settings.verbose = true;
    store->dispatch(loom::state::Action{loom::state::ActionType::UpdateSettings, settings});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetThinkingEnabled, false});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetPromptSuggestionEnabled, false});

    auto state = store->get_state();
    ASSERT_EQ(state.messages.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<loom::core::UserMessage>(state.messages.front()));
    const auto& updated = std::get<loom::core::UserMessage>(state.messages.front());
    EXPECT_EQ(updated.id.value, "msg-2");
    ASSERT_EQ(updated.content.size(), 1u);
    EXPECT_EQ(std::get<loom::core::TextBlock>(updated.content.front()).text, "replacement");
    EXPECT_EQ(state.settings.model, "claude-sonnet-4-6");
    EXPECT_EQ(state.settings.theme, "dark");
    EXPECT_TRUE(state.settings.verbose);
    EXPECT_FALSE(state.thinking_enabled);
    EXPECT_FALSE(state.prompt_suggestion_enabled);
}

TEST(StateStore, DispatchUiParityActions) {
    auto store = make_test_store();

    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetSlashCommand,
        std::optional<std::string>{"/help"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::AddNotification, std::string{"n1"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::AddNotification, std::string{"n2"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::DismissNotification, std::string{"n1"}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetStatusLineText,
        std::optional<std::string>{"ready"}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetFooterSelection,
        std::optional<loom::state::FooterItem>{loom::state::FooterItem::Tasks}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetSpinnerTip,
        std::optional<std::string>{"working"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetBriefOnly, true});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetShowTeammatePreview, true});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetSelectedAgentIndex, 2});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetCoordinatorTaskIndex, 3});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetViewSelectionMode, std::string{"viewing-agent"}});

    auto state = store->get_state();
    ASSERT_TRUE(state.active_slash_command.has_value());
    EXPECT_EQ(*state.active_slash_command, "/help");
    ASSERT_EQ(state.notifications.size(), 1u);
    EXPECT_EQ(state.notifications.front(), "n2");
    ASSERT_TRUE(state.status_line_text.has_value());
    EXPECT_EQ(*state.status_line_text, "ready");
    ASSERT_TRUE(state.footer_selection.has_value());
    EXPECT_EQ(*state.footer_selection, loom::state::FooterItem::Tasks);
    ASSERT_TRUE(state.spinner_tip.has_value());
    EXPECT_EQ(*state.spinner_tip, "working");
    EXPECT_TRUE(state.is_brief_only);
    EXPECT_TRUE(state.show_teammate_message_preview);
    EXPECT_EQ(state.selected_ip_agent_index, 2);
    EXPECT_EQ(state.coordinator_task_index, 3);
    EXPECT_EQ(state.view_selection_mode, "viewing-agent");
}

TEST(StateStore, DispatchBridgeAndRemoteParityActions) {
    auto store = make_test_store();

    store->dispatch(loom::state::Action{loom::state::ActionType::SetBridgeEnabled, true});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetBridgeExplicit, true});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetBridgeOutboundOnly, true});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetBridgeConnected, true});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetBridgeSessionActive, true});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetBridgeReconnecting, true});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetBridgeConnectUrl,
        std::optional<std::string>{"http://bridge/connect"}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetBridgeSessionUrl,
        std::optional<std::string>{"http://bridge/session"}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetBridgeEnvironmentId,
        std::optional<std::string>{"env-1"}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetBridgeSessionId,
        std::optional<std::string>{"session-1"}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetBridgeError,
        std::optional<std::string>{"bridge error"}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetBridgeInitialName,
        std::optional<std::string>{"local"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetShowRemoteCallout, true});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetRemoteSessionUrl,
        std::optional<std::string>{"http://remote/session"}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetRemoteConnectionStatus,
        loom::state::RemoteConnectionStatus::Connected});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetRemoteBackgroundTaskCount, 7u});

    auto state = store->get_state();
    EXPECT_TRUE(state.repl_bridge_enabled);
    EXPECT_TRUE(state.repl_bridge_explicit);
    EXPECT_TRUE(state.repl_bridge_outbound_only);
    EXPECT_TRUE(state.repl_bridge_connected);
    EXPECT_TRUE(state.repl_bridge_session_active);
    EXPECT_TRUE(state.repl_bridge_reconnecting);
    EXPECT_EQ(state.repl_bridge_connect_url, std::optional<std::string>{"http://bridge/connect"});
    EXPECT_EQ(state.repl_bridge_session_url, std::optional<std::string>{"http://bridge/session"});
    EXPECT_EQ(state.repl_bridge_environment_id, std::optional<std::string>{"env-1"});
    EXPECT_EQ(state.repl_bridge_session_id, std::optional<std::string>{"session-1"});
    EXPECT_EQ(state.repl_bridge_error, std::optional<std::string>{"bridge error"});
    EXPECT_EQ(state.repl_bridge_initial_name, std::optional<std::string>{"local"});
    EXPECT_TRUE(state.show_remote_callout);
    EXPECT_EQ(state.remote_session_url, std::optional<std::string>{"http://remote/session"});
    EXPECT_EQ(state.remote_connection_status, loom::state::RemoteConnectionStatus::Connected);
    EXPECT_EQ(state.remote_background_task_count, 7u);
}

TEST(StateStore, DispatchTasksAgentsAndOverlayParityActions) {
    auto store = make_test_store();

    loom::state::TaskState task{
        .id = "task-1",
        .title = "Investigate",
        .status = "running",
        .messages = {},
        .created_at = std::chrono::system_clock::now(),
    };
    store->dispatch(loom::state::Action{loom::state::ActionType::AddTask, task});
    task.status = "done";
    store->dispatch(loom::state::Action{loom::state::ActionType::UpdateTask, task});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetForegroundedTaskId,
        std::optional<std::string>{"task-1"}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetViewingAgentTaskId,
        std::optional<std::string>{"task-1"}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::RegisterAgentName,
        std::pair<std::string, std::string>{"agent-a", "task-1"}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetAgent,
        std::optional<std::string>{"agent-a"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetKairosEnabled, true});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetCompanionReaction, std::optional<std::string>{"ok"}});
    const auto pet_time = std::chrono::system_clock::now();
    store->dispatch(loom::state::Action{loom::state::ActionType::SetCompanionPetTime, std::optional{pet_time}});
    store->dispatch(loom::state::Action{loom::state::ActionType::AddActiveOverlay, std::string{"help"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::RemoveActiveOverlay, std::string{"help"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::AddActiveOverlay, std::string{"tasks"}});

    auto state = store->get_state();
    ASSERT_TRUE(state.tasks.contains("task-1"));
    EXPECT_EQ(state.tasks.at("task-1").status, "done");
    EXPECT_EQ(state.foregrounded_task_id, std::optional<std::string>{"task-1"});
    EXPECT_EQ(state.viewing_agent_task_id, std::optional<std::string>{"task-1"});
    ASSERT_TRUE(state.agent_name_registry.contains("agent-a"));
    EXPECT_EQ(state.agent_name_registry.at("agent-a"), "task-1");
    EXPECT_EQ(state.agent, std::optional<std::string>{"agent-a"});
    EXPECT_TRUE(state.kairos_enabled);
    EXPECT_EQ(state.companion_reaction, std::optional<std::string>{"ok"});
    EXPECT_EQ(state.companion_pet_at, std::optional{pet_time});
    EXPECT_FALSE(state.active_overlays.contains("help"));
    EXPECT_TRUE(state.active_overlays.contains("tasks"));

    store->dispatch(loom::state::Action{loom::state::ActionType::RemoveTask, std::string{"task-1"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::ClearActiveOverlays});
    state = store->get_state();
    EXPECT_FALSE(state.tasks.contains("task-1"));
    EXPECT_TRUE(state.active_overlays.empty());
}

TEST(StateStore, DispatchFeatureBucketParityActions) {
    auto store = make_test_store();

    loom::state::MCPState mcp;
    mcp.clients.push_back(loom::state::MCPServerConnection{
        .id = "mcp-1",
        .name = "MCP",
        .url = "stdio://mcp",
        .connected = true,
    });
    store->dispatch(loom::state::Action{loom::state::ActionType::UpdateMcpState, mcp});
    store->dispatch(loom::state::Action{loom::state::ActionType::IncrementMcpReconnectKey});

    loom::state::AppState::PluginsState plugins;
    plugins.enabled.push_back(loom::state::LoadedPlugin{
        .id = "plugin-1",
        .name = "Plugin",
        .version = "1.0.0",
        .enabled = true,
        .commands = {},
        .tools = {},
    });
    store->dispatch(loom::state::Action{loom::state::ActionType::UpdatePluginsState, plugins});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetPluginsNeedRefresh, true});

    loom::state::SpeculationState speculation;
    speculation.status = loom::state::SpeculationStatus::Active;
    speculation.id = "spec-1";
    store->dispatch(loom::state::Action{loom::state::ActionType::SetSpeculationState, speculation});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetSpeculationTimeSaved,
                                      std::int64_t{1234}});

    loom::state::AppState::SkillImprovementState::Suggestion suggestion;
    suggestion.skill_name = "state";
    store->dispatch(loom::state::Action{loom::state::ActionType::SetSkillSuggestion, std::optional{suggestion}});
    store->dispatch(loom::state::Action{loom::state::ActionType::IncrementAuthVersion});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetEffortValue, std::optional<std::string>{"high"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetAdvisorModel, std::optional<std::string>{"advisor"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetUltraplanLaunching, true});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetUltraplanSessionUrl,
        std::optional<std::string>{"http://ultraplan/session"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetUltraplanMode, true});

    auto state = store->get_state();
    ASSERT_EQ(state.mcp.clients.size(), 1u);
    EXPECT_EQ(state.mcp.clients.front().id, "mcp-1");
    EXPECT_EQ(state.mcp.plugin_reconnect_key, 1u);
    ASSERT_EQ(state.plugins.enabled.size(), 1u);
    EXPECT_EQ(state.plugins.enabled.front().id, "plugin-1");
    EXPECT_TRUE(state.plugins.needs_refresh);
    EXPECT_EQ(state.speculation.status, loom::state::SpeculationStatus::Active);
    EXPECT_EQ(state.speculation.id, "spec-1");
    EXPECT_EQ(state.speculation_session_time_saved_ms, 1234LL);
    ASSERT_TRUE(state.skill_improvement.suggestion.has_value());
    EXPECT_EQ(state.skill_improvement.suggestion->skill_name, "state");
    EXPECT_EQ(state.auth_version, 1u);
    EXPECT_EQ(state.effort_value, std::optional<std::string>{"high"});
    EXPECT_EQ(state.advisor_model, std::optional<std::string>{"advisor"});
    EXPECT_TRUE(state.ultraplan_launching);
    EXPECT_EQ(state.ultraplan_session_url, std::optional<std::string>{"http://ultraplan/session"});
    EXPECT_TRUE(state.is_ultraplan_mode);
}

TEST(StateStore, DispatchPendingRequestPromptAndInboxParityActions) {
    auto store = make_test_store();

    loom::state::AppState::InitialMessage initial{
        .message = std::get<loom::core::UserMessage>(make_user_message("initial", "start")),
        .clear_context = true,
        .mode = loom::state::PermissionMode::Plan,
        .allowed_prompts = {"plan"},
    };
    store->dispatch(loom::state::Action{loom::state::ActionType::SetInitialMessage, initial});

    loom::state::AppState::WorkerSandboxPermissions::PermissionRequest permission{
        .request_id = "perm-1",
        .worker_id = "worker-1",
        .worker_name = "Worker",
        .worker_color = "blue",
        .host = "localhost",
        .created_at = std::chrono::system_clock::now(),
    };
    store->dispatch(loom::state::Action{loom::state::ActionType::AddSandboxPermissionRequest, permission});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetSelectedSandboxPermissionIndex,
        std::size_t{4}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetPendingWorkerRequest,
        std::optional{loom::state::AppState::PendingWorkerRequest{
            .tool_name = "Bash",
            .tool_use_id = "tool-1",
            .description = "run command",
        }}});
    store->dispatch(loom::state::Action{
        loom::state::ActionType::SetPendingSandboxRequest,
        std::optional{loom::state::AppState::PendingSandboxRequest{
            .request_id = "sandbox-1",
            .host = "localhost",
        }}});

    loom::state::AppState::PromptSuggestionState prompt;
    prompt.text = "try this";
    prompt.prompt_id = "prompt-1";
    prompt.shown_at = std::chrono::system_clock::now();
    store->dispatch(loom::state::Action{loom::state::ActionType::SetPromptSuggestion, prompt});

    loom::state::AppState::InboxState::InboxMessage inbox_message{
        .id = "inbox-1",
        .from = "agent",
        .text = "done",
        .timestamp = "now",
        .status = "unread",
        .color = "green",
        .summary = "summary",
    };
    store->dispatch(loom::state::Action{loom::state::ActionType::AddInboxMessage, inbox_message});

    auto state = store->get_state();
    ASSERT_TRUE(state.initial_message.has_value());
    EXPECT_TRUE(state.initial_message->clear_context);
    ASSERT_EQ(state.worker_sandbox_permissions.queue.size(), 1u);
    EXPECT_EQ(state.worker_sandbox_permissions.queue.front().request_id, "perm-1");
    EXPECT_EQ(state.worker_sandbox_permissions.selected_index, 4u);
    ASSERT_TRUE(state.pending_worker_request.has_value());
    EXPECT_EQ(state.pending_worker_request->tool_name, "Bash");
    ASSERT_TRUE(state.pending_sandbox_request.has_value());
    EXPECT_EQ(state.pending_sandbox_request->request_id, "sandbox-1");
    EXPECT_EQ(state.prompt_suggestion.text, std::optional<std::string>{"try this"});
    ASSERT_EQ(state.inbox.messages.size(), 1u);
    EXPECT_EQ(state.inbox.messages.front().id, "inbox-1");

    store->dispatch(loom::state::Action{loom::state::ActionType::ClearInitialMessage});
    store->dispatch(loom::state::Action{loom::state::ActionType::RemoveSandboxPermissionRequest, std::string{"perm-1"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::ClearPromptSuggestion});
    store->dispatch(loom::state::Action{loom::state::ActionType::RemoveInboxMessage, std::string{"inbox-1"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::AddInboxMessage, inbox_message});
    store->dispatch(loom::state::Action{loom::state::ActionType::ClearInboxMessages});

    state = store->get_state();
    EXPECT_FALSE(state.initial_message.has_value());
    EXPECT_TRUE(state.worker_sandbox_permissions.queue.empty());
    EXPECT_FALSE(state.prompt_suggestion.text.has_value());
    EXPECT_FALSE(state.prompt_suggestion.prompt_id.has_value());
    EXPECT_TRUE(state.inbox.messages.empty());
}

TEST(StateStore, UnsupportedSideEffectActionsRemainNoOps) {
    auto store = make_test_store();
    auto before = store->get_state();

    store->dispatch(loom::state::Action{loom::state::ActionType::EnableTool, std::string{"Bash"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::DisableTool, std::string{"Bash"}});
    store->dispatch(loom::state::Action{loom::state::ActionType::SaveState});
    store->dispatch(loom::state::Action{loom::state::ActionType::LoadState});
    store->dispatch(loom::state::Action{loom::state::ActionType::ClearSavedState});

    auto after = store->get_state();
    EXPECT_EQ(before.tool_permission_context.allowed_tools.size(), after.tool_permission_context.allowed_tools.size());
    for (const auto& tool : before.tool_permission_context.allowed_tools) {
        EXPECT_TRUE(after.tool_permission_context.allowed_tools.contains(tool));
    }
    EXPECT_EQ(before.tool_permission_context.denied_tools.size(), after.tool_permission_context.denied_tools.size());
    for (const auto& tool : before.tool_permission_context.denied_tools) {
        EXPECT_TRUE(after.tool_permission_context.denied_tools.contains(tool));
    }
    EXPECT_EQ(before.settings.model, after.settings.model);
    EXPECT_EQ(before.messages.size(), after.messages.size());
}

TEST(StateStore, MiddlewareSupport) {
    auto store = make_test_store();
    int middleware_count = 0;
    
    store->add_middleware([&middleware_count](loom::state::DispatchFn next) {
        return [next = std::move(next), &middleware_count](const loom::state::Action& action) {
            middleware_count++;
            next(action);
        };
    });
    
    store->dispatch(loom::state::Action{loom::state::ActionType::SetVerbose, true});
    EXPECT_EQ(middleware_count, 1);
}

// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════

TEST(Selectors, IsVerbose) {
    auto state = loom::state::get_default_app_state();
    state.verbose = true;
    
    EXPECT_TRUE(loom::state::selectors::is_verbose(state));
}

TEST(Selectors, IsLoading) {
    auto state = loom::state::get_default_app_state();
    state.is_loading = true;
    
    EXPECT_TRUE(loom::state::selectors::is_loading(state));
}

TEST(Selectors, MemoizedSelector) {
    auto state = loom::state::get_default_app_state();
    int compute_count = 0;
    
    loom::state::selectors::MemoizedSelector<bool, bool> selector(
        [](const auto& s) {
            return s.verbose;
        },
        [&compute_count](const auto& s) {
            compute_count++;
            return s.verbose;
        }
    );
    

    auto result1 = selector.select(state);
    auto result2 = selector.select(state);
    EXPECT_EQ(result1, result2);
    EXPECT_EQ(compute_count, 1);
    

    state.verbose = true;
    auto result3 = selector.select(state);
    EXPECT_TRUE(result3);
    EXPECT_EQ(compute_count, 2);
}

// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════

TEST(OnChangeAppState, StateChangeRegistry) {
    loom::state::on_change::StateChangeRegistry registry;
    int callback_count = 0;
    
    registry.register_callback([&callback_count](const auto&, const auto&) {
        callback_count++;
    });
    
    auto state1 = loom::state::get_default_app_state();
    auto state2 = loom::state::get_default_app_state();
    state2.verbose = true;
    
    registry.run_callbacks(state1, state2);
    EXPECT_EQ(callback_count, 1);
}

// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════

TEST(FTXUIIntegration, VerboseIndicator) {
    loom::state::ftxui::VerboseIndicator indicator;
    
    auto store = make_shared_test_store();
    indicator.connect(store);
    
    EXPECT_EQ(indicator.get_text(), "");
    
    store->dispatch(loom::state::Action{loom::state::ActionType::SetVerbose, true});
    // Note: In real usage, the component would need to process the state change
    // This test verifies basic construction and API
    EXPECT_TRUE(indicator.get_last_state().verbose);
}

TEST(FTXUIIntegration, LoadingIndicator) {
    loom::state::ftxui::LoadingIndicator indicator;
    
    auto store = make_shared_test_store();
    indicator.connect(store);
    
    EXPECT_EQ(indicator.get_text(), "");
}

TEST(FTXUIIntegration, MessageCounter) {
    loom::state::ftxui::MessageCounter counter;
    
    auto store = make_shared_test_store();
    counter.connect(store);
    

    EXPECT_EQ(counter.get_last_state().messages.size(), 0);
}

TEST(FTXUIIntegration, ReactiveScreenManager) {
    auto store = make_shared_test_store();
    auto manager = loom::state::ftxui::make_reactive_screen_manager(store);
    
    auto indicator = std::make_shared<loom::state::ftxui::VerboseIndicator>();
    manager->add_component(indicator);
    
    EXPECT_EQ(manager->get_store(), store);
    manager->clear_components();
}

// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════

TEST(Persistence, StatePersistenceAPI) {

    auto state_file = std::filesystem::temp_directory_path() / "loom_test_state.json";
    loom::state::persistence::StatePersistence persistence(state_file);
    
    auto state = loom::state::get_default_app_state();
    state.verbose = true;
    state.is_loading = false;
    

    auto save_result = persistence.save_state(state);
    ASSERT_TRUE(save_result.has_value());

    auto loaded = persistence.load_state();
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->verbose);
    EXPECT_FALSE(loaded->is_loading);

    (void)persistence.delete_state();
}

TEST(Persistence, RoundTripsAllPersistedFields) {
    auto src = loom::state::get_default_app_state();
    // Flip every persisted field away from its default.
    src.verbose = true;
    src.compact_mode = true;
    src.show_thinking = true;
    src.fast_mode = true;
    src.thinking_enabled = false;             // default is true
    src.prompt_suggestion_enabled = false;     // default is true
    src.kairos_enabled = true;
    src.is_ultraplan_mode = true;
    src.ultraplan_launching = true;
    src.is_brief_only = true;
    src.show_teammate_message_preview = true;
    src.working_directory = "/tmp/cc-roundtrip";
    src.view_selection_mode = "viewing-agent";
    src.selected_ip_agent_index = 7;
    src.coordinator_task_index = 3;
    src.auth_version = 42;
    src.remote_background_task_count = 9;
    src.main_loop_model = "claude-opus-4-8";
    src.advisor_model = "claude-haiku-4-5";
    src.effort_value = "high";
    src.status_line_text = "custom status";

    auto serialized = loom::state::persistence::serialize_state(src);
    ASSERT_TRUE(serialized.has_value()) << serialized.error().format();

    auto parsed = loom::state::persistence::deserialize_state(*serialized);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().format();
    const auto& dst = *parsed;

    EXPECT_EQ(dst.verbose, src.verbose);
    EXPECT_EQ(dst.compact_mode, src.compact_mode);
    EXPECT_EQ(dst.show_thinking, src.show_thinking);
    EXPECT_EQ(dst.fast_mode, src.fast_mode);
    EXPECT_EQ(dst.thinking_enabled, src.thinking_enabled);
    EXPECT_EQ(dst.prompt_suggestion_enabled, src.prompt_suggestion_enabled);
    EXPECT_EQ(dst.kairos_enabled, src.kairos_enabled);
    EXPECT_EQ(dst.is_ultraplan_mode, src.is_ultraplan_mode);
    EXPECT_EQ(dst.ultraplan_launching, src.ultraplan_launching);
    EXPECT_EQ(dst.is_brief_only, src.is_brief_only);
    EXPECT_EQ(dst.show_teammate_message_preview, src.show_teammate_message_preview);
    EXPECT_EQ(dst.working_directory, src.working_directory);
    EXPECT_EQ(dst.view_selection_mode, src.view_selection_mode);
    EXPECT_EQ(dst.selected_ip_agent_index, src.selected_ip_agent_index);
    EXPECT_EQ(dst.coordinator_task_index, src.coordinator_task_index);
    EXPECT_EQ(dst.auth_version, src.auth_version);
    EXPECT_EQ(dst.remote_background_task_count, src.remote_background_task_count);
    ASSERT_TRUE(dst.main_loop_model.has_value());  EXPECT_EQ(*dst.main_loop_model, *src.main_loop_model);
    ASSERT_TRUE(dst.advisor_model.has_value());    EXPECT_EQ(*dst.advisor_model, *src.advisor_model);
    ASSERT_TRUE(dst.effort_value.has_value());     EXPECT_EQ(*dst.effort_value, *src.effort_value);
    ASSERT_TRUE(dst.status_line_text.has_value()); EXPECT_EQ(*dst.status_line_text, *src.status_line_text);
}

TEST(Persistence, AbsentOptionalStringsStayDefault) {
    auto src = loom::state::get_default_app_state();
    auto serialized = loom::state::persistence::serialize_state(src);
    ASSERT_TRUE(serialized.has_value());
    auto parsed = loom::state::persistence::deserialize_state(*serialized);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->main_loop_model.has_value());
    EXPECT_FALSE(parsed->advisor_model.has_value());
    EXPECT_FALSE(parsed->effort_value.has_value());
    EXPECT_FALSE(parsed->status_line_text.has_value());
}

TEST(Persistence, LoadsLegacyV1ShapeWithMissingFields) {
    // Minimal legacy v1 object. thinking_enabled and auth_version were
    // previously written-but-dropped; this proves they now round-trip, while
    // fields absent from the legacy blob keep their defaults.
    std::string legacy = R"({"verbose":true,"thinking_enabled":false,"auth_version":5,"schema_version":1})";
    auto parsed = loom::state::persistence::deserialize_state(legacy);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().format();
    EXPECT_TRUE(parsed->verbose);
    EXPECT_FALSE(parsed->thinking_enabled);
    EXPECT_EQ(parsed->auth_version, 5u);
    EXPECT_EQ(parsed->view_selection_mode, "none");
    EXPECT_FALSE(parsed->main_loop_model.has_value());
}

TEST(Persistence, WritesCurrentSchemaVersion) {
    auto s = loom::state::get_default_app_state();
    auto serialized = loom::state::persistence::serialize_state(s);
    ASSERT_TRUE(serialized.has_value());
    EXPECT_NE(serialized->find("\"schema_version\":2"), std::string::npos);
    EXPECT_EQ(loom::state::persistence::kCurrentStateSchemaVersion, 2);
}

TEST(Persistence, ValidateStateAcceptsDefaultsRejectsBadValues) {
    auto good = loom::state::get_default_app_state();
    EXPECT_TRUE(loom::state::persistence::validate_state(good).has_value());

    auto bad_index = loom::state::get_default_app_state();
    bad_index.selected_ip_agent_index = -5;
    EXPECT_FALSE(loom::state::persistence::validate_state(bad_index).has_value());

    auto bad_cost = loom::state::get_default_app_state();
    bad_cost.total_cost_usd = -1.0;
    EXPECT_FALSE(loom::state::persistence::validate_state(bad_cost).has_value());
}

TEST(Persistence, DeserializeRejectsInvalidIndices) {
    std::string malformed = R"({"selected_ip_agent_index":-5,"schema_version":2})";
    auto parsed = loom::state::persistence::deserialize_state(malformed);
    ASSERT_FALSE(parsed.has_value());
}

TEST(StoreUndoRedo, RoundTripsDispatchedActions) {
    auto store = make_test_store();
    store->enable_undo();
    EXPECT_FALSE(store->can_undo());

    store->dispatch(loom::state::Action{loom::state::ActionType::SetVerbose, true});   // false -> true
    store->dispatch(loom::state::Action{loom::state::ActionType::SetVerbose, false});  // true -> false
    EXPECT_FALSE(store->get_state().verbose);

    ASSERT_TRUE(store->can_undo());
    store->undo();
    EXPECT_TRUE(store->get_state().verbose);
    store->undo();
    EXPECT_FALSE(store->get_state().verbose);
    EXPECT_FALSE(store->can_undo());

    ASSERT_TRUE(store->can_redo());
    store->redo();
    EXPECT_TRUE(store->get_state().verbose);
    store->redo();
    EXPECT_FALSE(store->get_state().verbose);
    EXPECT_FALSE(store->can_redo());
}

TEST(StoreUndoRedo, CapacityBoundsHistoryToOneLevel) {
    auto store = make_test_store();
    store->enable_undo(1);
    store->dispatch(loom::state::Action{loom::state::ActionType::SetVerbose, true});
    store->dispatch(loom::state::Action{loom::state::ActionType::SetVerbose, false});
    store->undo();
    EXPECT_TRUE(store->get_state().verbose); // only the most recent snapshot survives
    EXPECT_FALSE(store->can_undo());
}

TEST(StoreUndoRedo, NewActionClearsRedoStack) {
    auto store = make_test_store();
    store->enable_undo();
    store->dispatch(loom::state::Action{loom::state::ActionType::SetVerbose, true});
    store->undo(); // verbose=false, redo has the true snapshot
    ASSERT_TRUE(store->can_redo());
    store->dispatch(loom::state::Action{loom::state::ActionType::SetVerbose, true});
    EXPECT_FALSE(store->can_redo()); // a new dispatch clears redo
}

// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════

TEST(SessionHistory, SaveAllPersistsCreatedConversationIds) {
    auto storage_path = std::filesystem::temp_directory_path() /
        "loom_history_save_test.json";
    std::filesystem::remove(storage_path);

    loom::core::ConversationStore store(storage_path.string());
    store.create_conversation();
    auto ids = store.get_conversation_ids();
    ASSERT_EQ(ids.size(), 1u);

    auto saved = store.save_all();
    ASSERT_TRUE(saved.has_value()) << saved.error().format();

    std::ifstream input(storage_path);
    ASSERT_TRUE(input.is_open());
    std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_NE(json.find(ids.front()), std::string::npos);
    EXPECT_NE(json.find("active_conversation_id"), std::string::npos);

    std::filesystem::remove(storage_path);
}

TEST(SessionHistory, LoadAllRestoresConversationIdsAndActiveSelection) {
    auto storage_path = std::filesystem::temp_directory_path() /
        "loom_history_load_test.json";
    std::filesystem::remove(storage_path);

    {
        loom::core::ConversationStore store(storage_path.string());
        store.create_conversation();
        auto ids = store.get_conversation_ids();
        ASSERT_EQ(ids.size(), 1u);
        ASSERT_TRUE(store.save_all().has_value());
    }

    loom::core::ConversationStore loaded(storage_path.string());
    auto result = loaded.load_all();
    ASSERT_TRUE(result.has_value()) << result.error().format();

    auto loaded_ids = loaded.get_conversation_ids();
    ASSERT_EQ(loaded_ids.size(), 1u);
    EXPECT_TRUE(loaded.switch_conversation(loaded_ids.front()));

    std::filesystem::remove(storage_path);
}

TEST(SessionHistory, LoadAllRestoresSavedMessages) {
    auto storage_path = std::filesystem::temp_directory_path() /
        "loom_history_messages_test.json";
    std::filesystem::remove(storage_path);

    {
        loom::core::ConversationStore store(storage_path.string());
        auto* conversation = store.create_conversation();
        conversation->add_message(loom::core::UserMessage{
            loom::core::MessageBase{
                loom::core::MessageId{"msg_user_1"},
                std::chrono::system_clock::now(),
                {loom::core::TextBlock{"hello from persisted history"}}
            }
        });
        ASSERT_TRUE(store.save_all().has_value());
    }

    loom::core::ConversationStore loaded(storage_path.string());
    ASSERT_TRUE(loaded.load_all().has_value());
    auto* active = loaded.get_active_conversation();
    auto messages = active->get_messages();

    ASSERT_EQ(messages.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<loom::core::UserMessage>(messages.front()));
    const auto& user = std::get<loom::core::UserMessage>(messages.front());
    ASSERT_EQ(user.content.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<loom::core::TextBlock>(user.content.front()));
    EXPECT_EQ(std::get<loom::core::TextBlock>(user.content.front()).text,
              "hello from persisted history");

    std::filesystem::remove(storage_path);
}

TEST(SessionHistory, LoadAllRestoresCompactBoundaryMetadata) {
    auto storage_path = std::filesystem::temp_directory_path() /
        "loom_history_compact_boundary_test.json";
    std::filesystem::remove(storage_path);

    {
        loom::core::ConversationStore store(storage_path.string());
        auto* conversation = store.create_conversation();
        conversation->add_message(loom::core::SystemMessage{
            loom::core::MessageBase{
                loom::core::MessageId{"compact-boundary-1"},
                std::chrono::system_clock::now(),
                {loom::core::TextBlock{"Conversation compacted."}}
            },
            std::nullopt,
            std::string{"compact_boundary"},
            loom::core::CompactMetadata{
                .trigger = "manual",
                .pre_tokens = 1234,
                .preserved_segment = loom::core::CompactPreservedSegment{
                    .head_uuid = "head-message",
                    .anchor_uuid = "summary-message",
                    .tail_uuid = "tail-message",
                },
            },
            std::nullopt,
        });
        ASSERT_TRUE(store.save_all().has_value());
    }

    loom::core::ConversationStore loaded(storage_path.string());
    ASSERT_TRUE(loaded.load_all().has_value());
    auto* active = loaded.get_active_conversation();
    auto messages = active->get_messages();

    ASSERT_EQ(messages.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<loom::core::SystemMessage>(messages.front()));
    const auto& boundary = std::get<loom::core::SystemMessage>(messages.front());
    ASSERT_TRUE(boundary.subtype.has_value());
    EXPECT_EQ(*boundary.subtype, "compact_boundary");
    ASSERT_TRUE(boundary.compact_metadata.has_value());
    EXPECT_EQ(boundary.compact_metadata->trigger, "manual");
    EXPECT_EQ(boundary.compact_metadata->pre_tokens, 1234u);
    ASSERT_TRUE(boundary.compact_metadata->preserved_segment.has_value());
    EXPECT_EQ(boundary.compact_metadata->preserved_segment->head_uuid, "head-message");
    EXPECT_EQ(boundary.compact_metadata->preserved_segment->anchor_uuid, "summary-message");
    EXPECT_EQ(boundary.compact_metadata->preserved_segment->tail_uuid, "tail-message");

    std::filesystem::remove(storage_path);
}

TEST(SessionHistory, LoadAllRestoresSnipMetadata) {
    auto storage_path = std::filesystem::temp_directory_path() /
        "loom_history_snip_metadata_test.json";
    std::filesystem::remove(storage_path);

    {
        loom::core::ConversationStore store(storage_path.string());
        auto* conversation = store.create_conversation();
        conversation->add_message(loom::core::SystemMessage{
            loom::core::MessageBase{
                loom::core::MessageId{"snip-boundary-1"},
                std::chrono::system_clock::now(),
                {loom::core::TextBlock{"Conversation snipped."}}
            },
            std::nullopt,
            std::string{"snip_boundary"},
            std::nullopt,
            loom::core::SnipMetadata{
                .removed_uuids = {"old-user-1", "old-assistant-1"},
            },
        });
        ASSERT_TRUE(store.save_all().has_value());
    }

    loom::core::ConversationStore loaded(storage_path.string());
    ASSERT_TRUE(loaded.load_all().has_value());
    auto* active = loaded.get_active_conversation();
    auto messages = active->get_messages();

    ASSERT_EQ(messages.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<loom::core::SystemMessage>(messages.front()));
    const auto& boundary = std::get<loom::core::SystemMessage>(messages.front());
    ASSERT_TRUE(boundary.subtype.has_value());
    EXPECT_EQ(*boundary.subtype, "snip_boundary");
    ASSERT_TRUE(boundary.snip_metadata.has_value());
    ASSERT_EQ(boundary.snip_metadata->removed_uuids.size(), 2u);
    EXPECT_EQ(boundary.snip_metadata->removed_uuids[0], "old-user-1");
    EXPECT_EQ(boundary.snip_metadata->removed_uuids[1], "old-assistant-1");

    std::filesystem::remove(storage_path);
}

TEST(SessionHistory, LoadAllRestoresImageAndDocumentBlocks) {
    auto storage_path = std::filesystem::temp_directory_path() /
        "loom_history_rich_content_test.json";
    std::filesystem::remove(storage_path);

    {
        loom::core::ConversationStore store(storage_path.string());
        auto* conversation = store.create_conversation();
        loom::core::ImageBlock ib_rich_png;
        ib_rich_png.media_type = "image/png";
        ib_rich_png.data = "iVBORw0KGgo=";
        conversation->add_message(loom::core::UserMessage{
            loom::core::MessageBase{
                loom::core::MessageId{"msg_user_rich"},
                std::chrono::system_clock::now(),
                {
                    loom::core::TextBlock{"rich content"},
                    std::move(ib_rich_png),
                    loom::core::DocumentBlock{"application/pdf", "JVBERi0xLjQ="},
                }
            }
        });
        ASSERT_TRUE(store.save_all().has_value());
    }

    loom::core::ConversationStore loaded(storage_path.string());
    ASSERT_TRUE(loaded.load_all().has_value());
    auto* active = loaded.get_active_conversation();
    auto messages = active->get_messages();

    ASSERT_EQ(messages.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<loom::core::UserMessage>(messages.front()));
    const auto& user = std::get<loom::core::UserMessage>(messages.front());
    ASSERT_EQ(user.content.size(), 3u);

    ASSERT_TRUE(std::holds_alternative<loom::core::ImageBlock>(user.content[1]));
    const auto& image = std::get<loom::core::ImageBlock>(user.content[1]);
    EXPECT_EQ(image.media_type, "image/png");
    EXPECT_EQ(image.data, "iVBORw0KGgo=");

    ASSERT_TRUE(std::holds_alternative<loom::core::DocumentBlock>(user.content[2]));
    const auto& document = std::get<loom::core::DocumentBlock>(user.content[2]);
    EXPECT_EQ(document.media_type, "application/pdf");
    EXPECT_EQ(document.data, "JVBERi0xLjQ=");

    std::filesystem::remove(storage_path);
}

// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════

TEST(CliUpdate, DownloadUpdateCopiesFileUrlPayload) {
    auto source_path = std::filesystem::temp_directory_path() /
        "loom_update_source.bin";
    std::filesystem::remove(source_path);
    {
        std::ofstream output(source_path, std::ios::binary | std::ios::trunc);
        output << "real update payload";
    }

    auto downloaded = loom::cli::download_update("file://" + source_path.string());
    ASSERT_TRUE(downloaded.has_value()) << downloaded.error();
    ASSERT_TRUE(std::filesystem::exists(*downloaded));

    std::ifstream input(*downloaded, std::ios::binary);
    std::string payload((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    EXPECT_EQ(payload, "real update payload");

    std::filesystem::remove(source_path);
    std::filesystem::remove(*downloaded);
}

// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════

TEST(McpAuth, FetchConfiguredMetadataFromFileUrl) {
    auto metadata_path = std::filesystem::temp_directory_path() /
        "loom_mcp_oauth_metadata.json";
    std::filesystem::remove(metadata_path);
    {
        std::ofstream output(metadata_path, std::ios::trunc);
        output << R"({
            "authorization_endpoint":"https://auth.example.com/authorize",
            "token_endpoint":"https://auth.example.com/token",
            "scope":"openid profile"
        })";
    }

    auto metadata = loom::services::mcp::fetch_auth_server_metadata(
        "test-server",
        "https://mcp.example.com/sse",
        "file://" + metadata_path.string());

    ASSERT_TRUE(metadata.has_value()) << metadata.error().format();
    ASSERT_TRUE(metadata->has_value());
    EXPECT_EQ((*metadata)->authorization_endpoint, "https://auth.example.com/authorize");
    EXPECT_EQ((*metadata)->token_endpoint, "https://auth.example.com/token");
    ASSERT_TRUE((*metadata)->scope.has_value());
    EXPECT_EQ(*(*metadata)->scope, "openid profile");

    std::filesystem::remove(metadata_path);
}

TEST(McpAuth, XaaFlowDoesNotReturnUnimplementedError) {
    unsetenv("LOOM_ENABLE_XAA");

    loom::services::mcp::McpServerConfig server_config{
        .name = "test-server",
        .command = {},
        .args = {},
        .env = {},
        .transport = "http",
        .url = "https://mcp.example.com/mcp",
        .headers = {},
        .oauth = loom::services::mcp::McpOAuthConfig{
            .auth_server_metadata_url = std::nullopt,
            .callback_port = std::nullopt,
            .client_id = std::nullopt,
            .xaa = true}
    };

    auto result = loom::services::mcp::perform_mcp_oauth_flow(
        "test-server",
        server_config,
        [](const std::string&) {});

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().code(), loom::utils::ErrorCode::unimplemented);
    EXPECT_NE(result.error().message().find("XAA is not enabled"), std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════════

TEST(SystemPrompts, ComputeSimpleEnvInfoIncludesDynamicRuntimeDetails) {
    auto env_info = loom::constants::prompts::compute_simple_env_info(
        "claude-sonnet-4-6",
        {"/tmp/loom-extra"});

    EXPECT_NE(env_info.find("# Environment"), std::string::npos);
    EXPECT_NE(env_info.find("Primary working directory:"), std::string::npos);
    EXPECT_NE(env_info.find("Additional working directories:"), std::string::npos);
    EXPECT_NE(env_info.find("/tmp/loom-extra"), std::string::npos);
    EXPECT_NE(env_info.find("Assistant knowledge cutoff is August 2025."), std::string::npos);
}

TEST(SystemPrompts, GetSystemPromptAssemblesStaticAndDynamicSections) {
    loom::constants::prompts::SystemPromptOptions options{
        .model = "claude-opus-4-6",
        .enabled_tools = {"Read", "Write"},
        .additional_working_directories = {},
        .simple = false,
        .use_global_cache_boundary = true,
    };

    auto sections = loom::constants::prompts::get_system_prompt(options);
    ASSERT_GE(sections.size(), 6u);

    auto joined = std::accumulate(std::next(sections.begin()), sections.end(), sections.front(),
        [](std::string acc, const std::string& section) {
            acc += "\n";
            acc += section;
            return acc;
        });

    EXPECT_NE(joined.find("You are Loom"), std::string::npos);
    EXPECT_NE(joined.find("# Tone and style"), std::string::npos);
    EXPECT_NE(joined.find(loom::constants::prompts::system_prompt_dynamic_boundary), std::string::npos);
    EXPECT_NE(joined.find("# Environment"), std::string::npos);
    EXPECT_NE(joined.find("Read"), std::string::npos);
    EXPECT_NE(joined.find("Write"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Store genericity — the Store template must be instantiable with a State
// other than AppState (dispatch/undo/redo/subscribers work generically; the
// AppState-specific persistence/change-registry hooks are compile-time-gated).
// ---------------------------------------------------------------------------

namespace {

struct CounterState {
    int value = 0;
};

[[nodiscard]] inline CounterState counter_reducer(const CounterState& s, const loom::state::Action& a) {
    CounterState next = s;
    if (a.type == loom::state::ActionType::SetLoading) {
        if (auto v = a.get_payload<bool>(); v && *v) next.value += 1;
    }
    return next;
}

} // namespace

TEST(StoreGenericity, IsGenericOverStateAndSupportsUndoRedo) {
    using CounterStore = loom::state::Store<CounterState, decltype(&counter_reducer)>;
    CounterStore store{CounterState{0}, &counter_reducer};
    EXPECT_EQ(store.get_state().value, 0);

    store.enable_undo();
    store.dispatch(loom::state::Action{loom::state::ActionType::SetLoading, true});  // value -> 1
    store.dispatch(loom::state::Action{loom::state::ActionType::SetLoading, true});  // value -> 2
    EXPECT_EQ(store.get_state().value, 2);

    // Generic undo/redo works on a non-AppState State.
    ASSERT_TRUE(store.can_undo());
    store.undo();
    EXPECT_EQ(store.get_state().value, 1);
    store.undo();
    EXPECT_EQ(store.get_state().value, 0);
    EXPECT_FALSE(store.can_undo());

    store.redo();
    EXPECT_EQ(store.get_state().value, 1);

    // Generic subscriber fan-out works.
    int notifications = 0;
    auto sub = store.subscribe([&](const CounterState&, const CounterState&) { ++notifications; });
    store.dispatch(loom::state::Action{loom::state::ActionType::SetLoading, true});
    EXPECT_EQ(notifications, 1);
    store.unsubscribe(sub);
}
