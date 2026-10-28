// RFC 0021: session isolation, native change projection and callback lifetime.
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <cstdlib>

import std;
import loom.ui.app.app;
import loom.ui.screens.repl_state;
import loom.ui.screens.task_view_store;
import loom.ui.dialogs.system;
import loom.ui.dialogs.triggers;
import loom.ui.screens.repl_screen;
import loom.ui.features.agents.runtime_agents;
import loom.query.query_engine;
import loom.query.assembly;
import loom.serdes.json;
import loom.types.types;
import loom.tools.tool;
import loom.tools.agent_runtime;
import loom.commands.registry;
import loom.session.storage;

namespace {
namespace fs = std::filesystem;
namespace ui = loom::ui;
namespace native = loom::tools::agent_runtime;
namespace display = loom::ui::agents::runtime;

struct AgentUiEnv {
    std::string key;
    std::optional<std::string> previous;

    AgentUiEnv(std::string name, const std::string& value)
        : key(std::move(name)) {
        if (const auto* old = std::getenv(key.c_str())) previous = old;
        setenv(key.c_str(), value.c_str(), 1);
    }

    ~AgentUiEnv() {
        if (previous)
            setenv(key.c_str(), previous->c_str(), 1);
        else
            unsetenv(key.c_str());
    }
};

class AppRuntimeAgents : public testing::Test {
protected:
    void SetUp() override {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (std::uint64_t attempt = 0;; ++attempt) {
            root = fs::temp_directory_path() / std::format("loom-ui-agents-{}-{}", stamp, attempt);
            if (fs::create_directory(root)) break;
        }
        runtime_dir =
            std::make_unique<AgentUiEnv>("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
        config_dir = std::make_unique<AgentUiEnv>("LOOM_CONFIG_DIR", (root / "config").string());
        native::native_agent_store().clear_for_testing();
        loom::core::QueryEngineConfig config;
        config.cwd = root.string();
        config.base_url = "http://127.0.0.1:1";  // No test submits a model request.
        config.context_window.auto_compact = false;
        engine = std::make_unique<loom::core::QueryEngine>(std::move(config), tools);
        app =
            ftxui::Make<ui::AppAdapter>(engine.get(), nullptr, &commands, root / "sessions", [] {});
    }

    void TearDown() override {
        app.reset();
        engine.reset();
        native::native_agent_store().clear_for_testing();
        runtime_dir.reset();
        config_dir.reset();
        fs::remove_all(root);
    }

    native::NativeAgentRecord record(std::string id, native::NativeAgentStatus status,
                                     bool background = false, int created = 0) const {
        auto item = native::make_native_agent_record(std::move(id), "general-purpose");
        item.parent_session_id = engine->session_id().value;
        item.cwd = root.string();
        item.status = status;
        item.background = background;
        item.created_at = std::chrono::system_clock::time_point{std::chrono::seconds{created}};
        item.name = item.agent_id;
        item.description = "Runtime scope fixture";
        return item;
    }

    std::vector<display::Instance> instances() const {
        return ui::test_seams(app).screen_state_for_testing()->task_view_store.runtime_agents;
    }

    void drain() { app->OnEvent(ftxui::Event::Custom); }

    fs::path root;
    std::unique_ptr<AgentUiEnv> runtime_dir, config_dir;
    loom::core::ToolRegistry tools;
    loom::commands::AppCommandRegistry commands;
    std::unique_ptr<loom::core::QueryEngine> engine;
    std::shared_ptr<ui::AppAdapter> app;
};

}  // namespace

TEST_F(AppRuntimeAgents, ForegroundAndBackgroundProjectWhileParentIsIdle) {
    EXPECT_TRUE(instances().empty());
    auto& store = native::native_agent_store();
    store.upsert(record("foreground", native::NativeAgentStatus::Queued));
    store.upsert(record("background", native::NativeAgentStatus::Running, true, 1));
    auto dirty = ui::test_seams(app).runtime_agent_dirty_for_testing();
    EXPECT_TRUE(dirty->load());
    EXPECT_TRUE(instances().empty()) << "Native observers must not mutate UI-thread state";
    EXPECT_FALSE(ui::test_seams(app).is_query_running_for_testing());
    drain();
    ASSERT_EQ(instances().size(), 2u);
    EXPECT_EQ(instances()[0].status, display::Status::Queued);
    EXPECT_FALSE(instances()[0].background);
    EXPECT_TRUE(instances()[1].background);
    EXPECT_FALSE(dirty->load());
    store.mark_completed("background", "completed while parent idle");
    EXPECT_TRUE(dirty->load());
    drain();
    ASSERT_EQ(instances().size(), 2u);
    EXPECT_EQ(instances()[1].status, display::Status::Completed);
    EXPECT_EQ(instances()[1].agent_id, "background");
}

TEST_F(AppRuntimeAgents, NativeTeamChangesRefreshTeamRowsWhileParentIsBusy) {
    AgentUiEnv team_root("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    AgentUiEnv team_name("LOOM_TEAM_NAME", "active-team");
    AgentUiEnv backend("LOOM_TEAMMATE_BACKEND", "in-process");
    app.reset();
    app = ftxui::Make<ui::AppAdapter>(engine.get(), nullptr, &commands, root / "sessions", [] {});
    auto& store = native::native_agent_store();
    auto member = record("worker@active-team", native::NativeAgentStatus::Running, true);
    member.name = "worker";
    member.team_name = "active-team";
    member.teammate_backend = "in-process";
    auto unrelated = member;
    unrelated.agent_id = "other@another-team";
    unrelated.team_name = "another-team";
    store.upsert(unrelated);
    ui::test_seams(app).set_query_running_for_testing(true);
    for (const auto status :
         {native::NativeAgentStatus::Running, native::NativeAgentStatus::Completed,
          native::NativeAgentStatus::Queued, native::NativeAgentStatus::Cancelled,
          native::NativeAgentStatus::Failed}) {
        SCOPED_TRACE(static_cast<int>(status));
        switch (status) {
        case native::NativeAgentStatus::Running:
            store.upsert(member);
            break;
        case native::NativeAgentStatus::Completed:
            store.mark_completed(member.agent_id, "done");
            break;
        case native::NativeAgentStatus::Queued:
            store.enqueue_resume_message(member.agent_id, "continue");
            break;
        case native::NativeAgentStatus::Cancelled:
            store.mark_cancelled(member.agent_id, "cancelled");
            break;
        case native::NativeAgentStatus::Failed:
            store.mark_failed(member.agent_id, "fixture failure");
            break;
        }
        EXPECT_TRUE(ui::test_seams(app).runtime_agent_dirty_for_testing()->load());
        drain();
        EXPECT_TRUE(ui::test_seams(app).is_query_running_for_testing());
        EXPECT_FALSE(ui::test_seams(app).runtime_agent_dirty_for_testing()->load());
        EXPECT_TRUE(instances().empty()) << "Team Agents must not also appear as Subagents";
        const auto& teammates =
            ui::test_seams(app).screen_state_for_testing()->task_view_store.live_teammates;
        EXPECT_EQ(teammates.size(), 1u);
        EXPECT_EQ(ui::test_seams(app).screen_state_for_testing()->task_view_store.teammate_count,
                  1);
        if (teammates.size() == 1) {
            EXPECT_EQ(teammates.front().agent_id, member.agent_id);
            const auto expected = status == native::NativeAgentStatus::Running ||
                                          status == native::NativeAgentStatus::Queued
                                      ? "running"
                                  : status == native::NativeAgentStatus::Failed ? "unknown"
                                                                                : "idle";
            EXPECT_EQ(teammates.front().status, expected);
        }
    }
    ui::test_seams(app).set_query_running_for_testing(false);
}

TEST_F(AppRuntimeAgents, ExcludesOtherSessionsLegacyAndEveryTeammateIdentity) {
    auto& store = native::native_agent_store();
    store.upsert(record("owned", native::NativeAgentStatus::Running));
    auto other = record("other", native::NativeAgentStatus::Running);
    other.parent_session_id = "unrelated-session";
    store.upsert(other);
    auto legacy = record("legacy", native::NativeAgentStatus::Running);
    legacy.parent_session_id.reset();
    store.upsert(legacy);
    auto team = record("team", native::NativeAgentStatus::Running);
    team.team_name = "another-team";
    store.upsert(team);
    auto backend = record("backend", native::NativeAgentStatus::Running);
    backend.teammate_backend = "in-process";
    store.upsert(backend);
    auto task = record("teammate-task", native::NativeAgentStatus::Running);
    task.teammate_task_id = "task";
    store.upsert(task);
    auto pane = record("pane", native::NativeAgentStatus::Running);
    pane.teammate_pane_id = "pane";
    store.upsert(pane);
    drain();
    ASSERT_EQ(instances().size(), 1u);
    EXPECT_EQ(instances().front().agent_id, "owned");
}

TEST_F(AppRuntimeAgents, ActiveRowsSortByCreationAndIdAndResumeMovesTerminalBackToActive) {
    auto& store = native::native_agent_store();
    store.upsert(record("terminal-old", native::NativeAgentStatus::Completed, true, 1));
    store.upsert(record("z-active", native::NativeAgentStatus::Running, false, 3));
    store.upsert(record("a-active", native::NativeAgentStatus::Queued, true, 3));
    store.upsert(record("active-earlier", native::NativeAgentStatus::Running, false, 2));
    drain();
    std::vector<std::string> ids;
    for (const auto& item : instances())
        ids.push_back(item.agent_id);
    EXPECT_EQ(ids,
              (std::vector<std::string>{"active-earlier", "a-active", "z-active", "terminal-old"}));
    store.enqueue_resume_message("terminal-old", "Continue the previous task");
    drain();
    ASSERT_EQ(instances().size(), 4u);
    EXPECT_EQ(instances().front().agent_id, "terminal-old");
    EXPECT_EQ(instances().front().status, display::Status::Queued);
}

TEST_F(AppRuntimeAgents, SuccessfulSessionResumeChangesScopeFailedResumeAndClearPreserveIt) {
    auto& store = native::native_agent_store();
    const auto original_session = engine->session_id().value;
    store.upsert(record("original-active", native::NativeAgentStatus::Running, true));
    drain();
    app->HandleCommand("/clear");
    EXPECT_EQ(engine->session_id().value, original_session);
    ASSERT_EQ(instances().size(), 1u);
    EXPECT_EQ(instances().front().agent_id, "original-active");
    EXPECT_FALSE(
        ui::test_seams(app).resume_session_for_testing(root / "sessions", "missing-session"));
    EXPECT_EQ(engine->session_id().value, original_session);
    ASSERT_EQ(instances().size(), 1u);
    EXPECT_EQ(instances().front().agent_id, "original-active");
    loom::session::SessionMetadata metadata;
    metadata.session_id = "saved-session";
    metadata.cwd = root;
    ASSERT_TRUE(loom::session::save_session_metadata(root / "sessions", metadata));
    auto saved = record("resumed-scope", native::NativeAgentStatus::Failed, true);
    saved.parent_session_id = "saved-session";
    store.upsert(saved);
    ASSERT_TRUE(ui::test_seams(app).resume_session_for_testing(root / "sessions", "saved-session"));
    EXPECT_EQ(engine->session_id().value, "saved-session");
    ASSERT_EQ(instances().size(), 1u);
    EXPECT_EQ(instances().front().agent_id, "resumed-scope");
    EXPECT_EQ(instances().front().status, display::Status::Failed);
}

TEST_F(AppRuntimeAgents, TwoAppSubscriptionsKeepSnapshotsSeparate) {
    loom::core::ToolRegistry other_tools;
    loom::core::QueryEngineConfig config;
    config.cwd = root.string();
    config.context_window.auto_compact = false;
    loom::core::QueryEngine other_engine(config, other_tools);
    auto other_app = ftxui::Make<ui::AppAdapter>(&other_engine, nullptr, &commands,
                                                 root / "other-sessions", [] {});
    auto& store = native::native_agent_store();
    store.upsert(record("first-app", native::NativeAgentStatus::Running));
    auto other = record("second-app", native::NativeAgentStatus::Cancelled);
    other.parent_session_id = other_engine.session_id().value;
    store.upsert(other);
    drain();
    other_app->OnEvent(ftxui::Event::Custom);
    ASSERT_EQ(instances().size(), 1u);
    EXPECT_EQ(instances().front().agent_id, "first-app");
    const auto& other_instances =
        ui::test_seams(other_app).screen_state_for_testing()->task_view_store.runtime_agents;
    ASSERT_EQ(other_instances.size(), 1u);
    EXPECT_EQ(other_instances.front().agent_id, "second-app");
}

TEST_F(AppRuntimeAgents, LateAdmittedCallbackPassesDirtyGuardButClosedSharedQueueDropsDispatch) {
    auto callback = ui::test_seams(app).runtime_agent_change_callback_for_testing();
    auto dirty = ui::test_seams(app).runtime_agent_dirty_for_testing();
    auto dispatch = ui::test_seams(app).ui_dispatcher_for_testing();
    std::promise<void> entered, proceed;
    auto gate = proceed.get_future();
    auto late =
        std::async(std::launch::async, [callback, &entered, gate = std::move(gate)]() mutable {
            entered.set_value();
            gate.wait();
            callback();
        });
    entered.get_future().wait();
    app.reset();          // Production destructor closes the queue before unsubscribe.
    dirty->store(false);  // Prove the late callback reaches dispatch, rather than coalescing away.
    proceed.set_value();
    late.get();
    EXPECT_TRUE(dirty->load());
    auto captured = std::make_shared<int>(0);
    dispatch([captured] { ++*captured; });
    EXPECT_EQ(captured.use_count(), 1) << "Post-close dispatch must not retain a queued action";
    EXPECT_EQ(*captured, 0);
}

TEST_F(AppRuntimeAgents, InstanceBrowserOpensWhileBusyAndNeverConsumesNotification) {
    auto owned = record("owned-ended", native::NativeAgentStatus::Failed);
    owned.error = "Visible failure reason";
    owned.transcript = {"Completed tool evidence"};
    auto foreign = owned;
    foreign.agent_id = "foreign-ended";
    foreign.error = "Foreign private reason";
    foreign.parent_session_id = "another-session";
    auto& store = native::native_agent_store();
    store.upsert(owned);
    store.upsert(foreign);
    drain();
    const auto before = engine->get_conversation().size();
    ui::test_seams(app).set_query_running_for_testing(true);
    ui::test_seams(app).handle_submit_for_testing("/subagents history");
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        ui::test_seams(app).screen_state_for_testing());
    auto modal = state->dialog_store.dialog_queue.peek_modal_mut();
    ASSERT_TRUE(modal);
    EXPECT_EQ(loom::ui::dialogs::system::type_of(modal->get()),
              loom::ui::dialogs::system::DialogType::AgentInstances);
    auto& payload = std::get<loom::ui::dialogs::system::AgentInstancesPayload>(modal->get());
    payload.on_resize(60, 22);
    auto component = *std::static_pointer_cast<ftxui::Component>(payload.component);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(60), ftxui::Dimension::Fixed(22));
    ftxui::Render(screen, component->Render());
    EXPECT_NE(screen.ToString().find("owned-ended"), std::string::npos);
    EXPECT_EQ(screen.ToString().find("foreign-ended"), std::string::npos);
    component->OnEvent(ftxui::Event::Character('i'));
    screen.Clear();
    ftxui::Render(screen, component->Render());
    EXPECT_NE(screen.ToString().find("Visible failure reason"), std::string::npos);
    component->OnEvent(ftxui::Event::Escape);
    component->OnEvent(ftxui::Event::Escape);
    EXPECT_FALSE(state->dialog_store.dialog_queue.has_modal());
    EXPECT_FALSE(store.get(owned.agent_id)->notification_delivered);
    EXPECT_EQ(engine->get_conversation().size(), before);
    ui::test_seams(app).set_query_running_for_testing(false);
}

TEST_F(AppRuntimeAgents, InstanceBrowserRefusesOtherSessionId) {
    auto item = record("foreign-detail", native::NativeAgentStatus::Failed);
    item.parent_session_id = "different-owner";
    native::native_agent_store().upsert(item);
    ui::test_seams(app).handle_submit_for_testing("/subagents foreign-detail");
    EXPECT_FALSE(
        ui::test_seams(app).screen_state_for_testing()->dialog_store.dialog_queue.has_modal());
}

TEST_F(AppRuntimeAgents, InstanceBrowserYieldsToPendingAndNewPermission) {
    namespace ds = loom::ui::dialogs::system;
    namespace dr = loom::ui::repl_screen::dialog_queue_render;
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        ui::test_seams(app).screen_state_for_testing());
    int responses = 0;
    auto permission = [&] {
        ds::ToolPermissionPayload p;
        p.id = "original-parent-permission";
        p.tool_name = "Bash";
        p.description = "Original parent permission";
        p.on_response = [&](ds::ToolPermissionPayload::Decision, bool) { ++responses; };
        return p;
    };
    auto permission_id = state->dialog_store.dialog_queue.push(permission());
    ui::test_seams(app).handle_submit_for_testing("/subagents");
    EXPECT_FALSE(state->dialog_store.dialog_queue.has_modal());
    state->dialog_store.dialog_queue.remove(permission_id);
    ui::test_seams(app).handle_submit_for_testing("/subagents");
    ASSERT_TRUE(state->dialog_store.dialog_queue.has_modal());
    permission_id = state->dialog_store.dialog_queue.push(permission());
    auto prepared = dr::PrepareDialogLayers(*state, false, true, 60, 24);
    EXPECT_FALSE(state->dialog_store.dialog_queue.has_modal());
    EXPECT_TRUE(prepared.awaiting_permission);
    EXPECT_TRUE(dr::DispatchDialogQueueEvents(*state, ftxui::Event::Character('y'), false, true));
    EXPECT_EQ(responses, 1);
    state->dialog_store.dialog_queue.remove(permission_id);
    ui::test_seams(app).handle_submit_for_testing("/subagents");
    EXPECT_TRUE(state->dialog_store.dialog_queue.has_modal());
}

TEST_F(AppRuntimeAgents, InstanceBrowserTeamCategoryDoesNotDuplicateOrdinaryOrPaneRecords) {
    auto team = record("native-team", native::NativeAgentStatus::Running, true);
    team.team_name = "research";
    team.teammate_backend = "in-process";
    native::native_agent_store().upsert(team);
    native::native_agent_store().upsert(
        record("ordinary-only", native::NativeAgentStatus::Running));
    drain();
    ui::test_seams(app).handle_submit_for_testing("/subagents team");
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        ui::test_seams(app).screen_state_for_testing());
    auto modal = state->dialog_store.dialog_queue.peek_modal_mut();
    ASSERT_TRUE(modal);
    auto& payload = std::get<loom::ui::dialogs::system::AgentInstancesPayload>(modal->get());
    payload.on_resize(60, 22);
    auto component = *std::static_pointer_cast<ftxui::Component>(payload.component);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(60), ftxui::Dimension::Fixed(22));
    ftxui::Render(screen, component->Render());
    EXPECT_NE(screen.ToString().find("native-team"), std::string::npos);
    EXPECT_EQ(screen.ToString().find("ordinary-only"), std::string::npos);
    ASSERT_EQ(instances().size(), 1u);
    EXPECT_EQ(instances().front().agent_id, "ordinary-only");
}

TEST_F(AppRuntimeAgents, InstanceModalConsumesTypingAndEmptyEnterWithoutSubmittingDraft) {
    const auto before = engine->get_conversation().size();
    ui::test_seams(app).set_input_text_for_testing("Keep this private draft");
    ui::test_seams(app).handle_submit_for_testing("/subagents");
    ASSERT_TRUE(
        ui::test_seams(app).screen_state_for_testing()->dialog_store.dialog_queue.has_modal());
    app->OnEvent(ftxui::Event::Character('x'));
    app->OnEvent(ftxui::Event::Return);
    EXPECT_EQ(ui::test_seams(app).input_text_for_testing(), "Keep this private draft");
    EXPECT_FALSE(ui::test_seams(app).is_query_running_for_testing());
    EXPECT_EQ(engine->get_conversation().size(), before);
}

TEST_F(AppRuntimeAgents, InstanceModalPreservesGlobalInterruptWhileParentIsBusy) {
    ui::test_seams(app).set_query_running_for_testing(true);
    ui::test_seams(app).handle_submit_for_testing("/subagents");
    app->OnEvent(ftxui::Event::Character('\x03'));
    EXPECT_EQ(ui::test_seams(app).screen_state_for_testing()->task_view_store.spinner_tip,
              "Cancelling...");
    EXPECT_TRUE(
        ui::test_seams(app).screen_state_for_testing()->dialog_store.dialog_queue.has_modal());
    ui::test_seams(app).set_query_running_for_testing(false);
}

TEST_F(AppRuntimeAgents, InstanceBrowserYieldsToBothSandboxPermissionKinds) {
    namespace ds = loom::ui::dialogs::system;
    namespace dr = loom::ui::repl_screen::dialog_queue_render;
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        ui::test_seams(app).screen_state_for_testing());
    ds::SandboxPermissionPayload sandbox;
    sandbox.id = "sandbox-original";
    sandbox.host_pattern = "localhost";
    sandbox.on_response = [](bool, bool) {};
    // This legacy payload has no default App renderer. Exercise preemption
    // only after a client makes it actionable, matching the production guard.
    state->dialog_store.dialog_renderers.register_dialog(
        ds::DialogType::WorkerSandboxPermission,
        [](ds::DialogPayloadVariant&, const ds::DialogRenderContext&) {
            return ftxui::text("Worker permission");
        },
        [](ds::DialogPayloadVariant& payload, const ftxui::Event& event) {
            auto* request = std::get_if<ds::WorkerSandboxPermissionPayload>(&payload);
            if (request && request->on_response && event == ftxui::Event::Character('y')) {
                request->on_response(true, false);
                return true;
            }
            return false;
        });
    ds::WorkerSandboxPermissionPayload worker;
    worker.id = "worker-sandbox-original";
    worker.worker_id = "owned";
    worker.on_response = [](bool, bool) {};
    for (const auto& payload : std::vector<ds::DialogPayloadVariant>{sandbox, worker}) {
        const auto permission = state->dialog_store.dialog_queue.push(payload);
        ui::test_seams(app).handle_submit_for_testing("/subagents");
        EXPECT_FALSE(state->dialog_store.dialog_queue.has_modal());
        state->dialog_store.dialog_queue.remove(permission);
        ui::test_seams(app).handle_submit_for_testing("/subagents");
        ASSERT_TRUE(state->dialog_store.dialog_queue.has_modal());
        const auto arriving = state->dialog_store.dialog_queue.push(payload);
        auto prepared = dr::PrepareDialogLayers(*state, false, true, 60, 24);
        EXPECT_FALSE(state->dialog_store.dialog_queue.has_modal());
        EXPECT_TRUE(prepared.awaiting_permission);
        state->dialog_store.dialog_queue.remove(arriving);
    }
}

TEST_F(AppRuntimeAgents, ExplicitInstanceEntryOpensActiveAfterPreviouslyViewingHistory) {
    native::native_agent_store().upsert(record("active-view", native::NativeAgentStatus::Running));
    native::native_agent_store().upsert(record("ended-view", native::NativeAgentStatus::Failed));
    drain();
    ui::test_seams(app).handle_submit_for_testing("/subagents history");
    ui::test_seams(app).handle_submit_for_testing("/subagents");
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        ui::test_seams(app).screen_state_for_testing());
    auto modal = state->dialog_store.dialog_queue.peek_modal_mut();
    ASSERT_TRUE(modal);
    auto& payload = std::get<loom::ui::dialogs::system::AgentInstancesPayload>(modal->get());
    payload.on_resize(60, 22);
    auto component = *std::static_pointer_cast<ftxui::Component>(payload.component);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(60), ftxui::Dimension::Fixed(22));
    ftxui::Render(screen, component->Render());
    EXPECT_NE(screen.ToString().find("Subagents · Active"), std::string::npos);
    EXPECT_NE(screen.ToString().find("active-view"), std::string::npos);
    EXPECT_EQ(screen.ToString().find("ended-view"), std::string::npos);
}

TEST_F(AppRuntimeAgents, StopAcknowledgementDoesNotPromiseCleanupIsStillRunning) {
    auto& store = native::native_agent_store();
    store.begin_execution(record("stop-ack", native::NativeAgentStatus::Running),
                          tools.background_stop_token());
    ui::test_seams(app).handle_submit_for_testing("/subagents stop-ack");
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        ui::test_seams(app).screen_state_for_testing());
    app->OnEvent(ftxui::Event::Character('\x03'));
    EXPECT_EQ(state->footer_dynamic_text, "Cancellation requested");
    EXPECT_TRUE(store.get("stop-ack")->cancel_requested);
    EXPECT_EQ(store.get("stop-ack")->status, native::NativeAgentStatus::Running);
    store.end_execution("stop-ack");
}

TEST_F(AppRuntimeAgents, InstanceModalCoversTranscriptFragmentsAtBothHorizontalEdges) {
    namespace dr = loom::ui::repl_screen::dialog_queue_render;
    ui::test_seams(app).handle_submit_for_testing("/subagents");
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        ui::test_seams(app).screen_state_for_testing());
    for (const int columns : {40, 60, 120}) {
        auto prepared = dr::PrepareDialogLayers(*state, false, true, columns, 28);
        ftxui::Elements background;
        for (int row = 0; row < 28; ++row)
            background.push_back(ftxui::text(std::string(columns, 'Z')));
        auto screen =
            ftxui::Screen::Create(ftxui::Dimension::Fixed(columns), ftxui::Dimension::Fixed(28));
        ftxui::Render(screen,
                      dr::LayerAllDialogs(ftxui::vbox(std::move(background)), std::move(prepared)));
        for (int x = 0; x < columns; ++x)
            EXPECT_NE(screen.PixelAt(x, 4).character, "Z") << "columns=" << columns << " x=" << x;
    }
}

TEST_F(AppRuntimeAgents, InteractiveConversationRetainsIndependentRootAndChildDraftAndScroll) {
    auto item = record("interactive-one", native::NativeAgentStatus::Failed);
    item.sidechain_entries = {
        R"({"uuid":"interactive-one-source-1","message":{"role":"assistant","content":[{"type":"text","text":"Child answer"}]}})"};
    native::native_agent_store().upsert(item);
    auto root_state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        ui::test_seams(app).screen_state_for_testing());
    ui::test_seams(app).set_input_text_for_testing("Root private draft");
    root_state->messages_store.scroll_pinned_to_bottom = false;
    root_state->messages_store.scroll_offset = 7;
    const auto root_id = engine->session_id().value;
    const auto root_history = engine->get_conversation().size();
    ui::test_seams(app).handle_submit_for_testing("/subagents interactive-one");
    auto child = ui::test_seams(app).agent_conversation_state_for_testing();
    ASSERT_TRUE(child);
    EXPECT_NE(child.get(), root_state.get());
    app->OnEvent(ftxui::Event::Character("Child private draft"));
    EXPECT_EQ(child->input_text, "Child private draft");
    child->messages_store.scroll_pinned_to_bottom = false;
    child->messages_store.scroll_offset = 3;
    app->OnEvent(ftxui::Event::Special("\x1b[1;3D"));
    EXPECT_TRUE(ui::test_seams(app).active_agent_conversation_for_testing().empty());
    EXPECT_EQ(root_state->input_text, "Root private draft");
    EXPECT_FALSE(root_state->messages_store.scroll_pinned_to_bottom);
    EXPECT_EQ(root_state->messages_store.scroll_offset, 7);
    ui::test_seams(app).handle_submit_for_testing("/subagents interactive-one");
    EXPECT_EQ(ui::test_seams(app).agent_conversation_state_for_testing(), child);
    EXPECT_EQ(child->input_text, "Child private draft");
    EXPECT_EQ(child->messages_store.scroll_offset, 3);
    EXPECT_EQ(engine->session_id().value, root_id);
    EXPECT_EQ(engine->get_conversation().size(), root_history);
    EXPECT_FALSE(native::native_agent_store().get(item.agent_id)->notification_delivered);
}

TEST_F(AppRuntimeAgents, InteractiveUnsupportedCommandsAndShellKeepDraftAndNeverReachRoot) {
    native::native_agent_store().upsert(
        record("command-target", native::NativeAgentStatus::Failed));
    ui::test_seams(app).handle_submit_for_testing("/subagents command-target");
    auto child = ui::test_seams(app).agent_conversation_state_for_testing();
    ASSERT_TRUE(child);
    const auto root_history = engine->get_conversation().size();
    for (const auto& text : {"/clear", "/model anything", "/resume other", "!printf unsafe"}) {
        loom::ui::repl_screen::set_prompt_input_text(child, text, std::string_view(text).size());
        app->OnEvent(ftxui::Event::Return);
        EXPECT_EQ(child->input_text, text);
        ASSERT_TRUE(child->footer_dynamic_text.has_value());
        EXPECT_FALSE(child->footer_dynamic_text->empty());
        EXPECT_EQ(engine->get_conversation().size(), root_history);
        EXPECT_FALSE(ui::test_seams(app).is_query_running_for_testing());
    }
    child->prompt_store.input_mode = loom::ui::repl_screen::InputMode::Normal;
    loom::ui::repl_screen::set_prompt_input_text(child, "/back", 5);
    app->OnEvent(ftxui::Event::Return);
    EXPECT_TRUE(ui::test_seams(app).active_agent_conversation_for_testing().empty());
}

TEST_F(AppRuntimeAgents,
       InteractiveParentBusyRejectionRetainsTextAndTargetedInterruptLeavesParentAlone) {
    auto& store = native::native_agent_store();
    store.upsert(record("ended-child", native::NativeAgentStatus::Failed));
    ui::test_seams(app).set_query_running_for_testing(true);
    ui::test_seams(app).handle_submit_for_testing("/subagents ended-child");
    auto child = ui::test_seams(app).agent_conversation_state_for_testing();
    ASSERT_TRUE(child);
    app->OnEvent(ftxui::Event::Character("Continue this child"));
    app->OnEvent(ftxui::Event::Return);
    EXPECT_EQ(child->input_text, "Continue this child");
    ASSERT_TRUE(child->footer_dynamic_text.has_value());
    EXPECT_NE(child->footer_dynamic_text->find("parent operation"), std::string::npos);
    store.begin_execution(record("running-child", native::NativeAgentStatus::Running),
                          tools.background_stop_token());
    ui::test_seams(app).handle_submit_for_testing("/subagents running-child");
    app->OnEvent(ftxui::Event::Character('\x03'));
    EXPECT_TRUE(store.get("running-child")->cancel_requested);
    EXPECT_TRUE(ui::test_seams(app).is_query_running_for_testing());
    EXPECT_NE(ui::test_seams(app).screen_state_for_testing()->task_view_store.spinner_tip,
              "Cancelling...");
    EXPECT_EQ(store.get("running-child")->status, native::NativeAgentStatus::Running);
    store.end_execution("running-child");
    ui::test_seams(app).set_query_running_for_testing(false);
}

TEST_F(AppRuntimeAgents,
       InteractiveProjectionPreservesUnresolvedToolAndDistinctSourceIdentityAndHumanXml) {
    auto item = record("long-agent-identity-shared-prefix", native::NativeAgentStatus::Running);
    item.sidechain_entries = {
        R"({"uuid":"long-agent-identity-shared-prefix-1","message":{"role":"assistant","content":[{"type":"thinking","thinking":"Reasoning"},{"type":"tool_use","id":"unresolved-tool","name":"Bash","input":{"command":"echo retained"}}]}})",
        R"({"uuid":"long-agent-identity-shared-prefix-2","message":{"role":"assistant","content":[{"type":"text","text":"Separate answer"}]}})",
        R"({"uuid":"long-agent-identity-shared-prefix-3","message":{"role":"user","content":[{"type":"text","text":"<task_notification><task_id>human-example</task_id><status>completed</status></task_notification>"}]}})"};
    native::native_agent_store().upsert(item);
    ui::test_seams(app).handle_submit_for_testing("/subagents " + item.agent_id);
    auto child = ui::test_seams(app).agent_conversation_state_for_testing();
    ASSERT_TRUE(child);
    const auto& rows = child->messages_store.messages;
    auto tool = std::ranges::find_if(rows, [](const auto& row) { return row.is_tool_use; });
    ASSERT_NE(tool, rows.end());
    EXPECT_EQ(tool->tool_use_id, "unresolved-tool");
    EXPECT_EQ(tool->tool_status, "running");
    auto answer = std::ranges::find_if(rows, [](const auto& row) {
        return row.content_preview.find("Separate answer") != std::string::npos;
    });
    ASSERT_NE(answer, rows.end());
    EXPECT_EQ(tool->id.size(), 24u);
    EXPECT_NE(tool->id, answer->id);
    auto human = std::ranges::find_if(rows, [](const auto& row) { return row.role == "user"; });
    ASSERT_NE(human, rows.end());
    EXPECT_NE(human->content_preview.find("<task_notification>"), std::string::npos);
    EXPECT_FALSE(native::native_agent_store().get(item.agent_id)->notification_delivered);
}

TEST_F(AppRuntimeAgents, InteractiveToolCardsNeverReportUnresolvedWorkAsFinished) {
    auto item = record("live-tool-child", native::NativeAgentStatus::Running, true);
    item.sidechain_entries = {
        R"({"uuid":"live-prompt","message":{"role":"user","content":[{"type":"text","text":"Run the retained tool"}]}})",
        R"({"uuid":"live-call","message":{"role":"assistant","content":[{"type":"tool_use","id":"live-tool","name":"Bash","input":{"command":"sleep 120"}}]}})"};
    auto& store = native::native_agent_store();
    store.upsert(item);
    ui::test_seams(app).handle_submit_for_testing("/subagents live-tool-child");
    auto child = ui::test_seams(app).agent_conversation_state_for_testing();
    ASSERT_TRUE(child);
    app->OnEvent(ftxui::Event::Character("Editable child draft"));
    for (const bool queued : {false, true}) {
        item.pending_messages =
            queued ? std::vector<std::string>{"Queued guidance"} : std::vector<std::string>{};
        store.upsert(item);
        drain();
        for (const int columns : {40, 60, 112}) {
            AgentUiEnv term_columns("COLUMNS", std::to_string(columns));
            AgentUiEnv term_lines("LINES", "28");
            AgentUiEnv fullscreen("LOOM_NO_FLICKER", "1");
            auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(columns),
                                                ftxui::Dimension::Fixed(28));
            ftxui::Render(screen, app->Render());
            const auto text = screen.ToString();
            EXPECT_NE(text.find("Bash"), std::string::npos);
            EXPECT_EQ(text.find("Worked"), std::string::npos);
            if (queued) EXPECT_NE(text.find("[Queued]"), std::string::npos);
            EXPECT_EQ(child->input_text, "Editable child draft");
            EXPECT_FALSE(child->query_running);
            const auto& messages = child->messages_store.messages;
            const auto tool =
                std::ranges::find_if(messages, [](const auto& row) { return row.is_tool_use; });
            ASSERT_NE(tool, messages.end());
            EXPECT_EQ(tool->tool_status, "running");
        }
    }
    item.pending_messages.clear();
    item.status = native::NativeAgentStatus::Completed;
    item.sidechain_entries.push_back(
        R"({"uuid":"live-result","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":"live-tool","content":"Completed output"}]}})");
    store.upsert(item);
    drain();
    const auto& messages = child->messages_store.messages;
    const auto tool =
        std::ranges::find_if(messages, [](const auto& row) { return row.is_tool_use; });
    ASSERT_NE(tool, messages.end());
    EXPECT_EQ(tool->tool_status, "success");
    auto completed =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(112), ftxui::Dimension::Fixed(28));
    ftxui::Render(completed, app->Render());
    EXPECT_NE(completed.ToString().find("completed"), std::string::npos);
    EXPECT_NE(completed.ToString().find("Bash"), std::string::npos);
    app->OnEvent(ftxui::Event::Special("\x1b[1;3D"));
    EXPECT_TRUE(ui::test_seams(app).active_agent_conversation_for_testing().empty());
    EXPECT_FALSE(ui::test_seams(app).screen_state_for_testing()->disable_chain_compression);
}

TEST_F(AppRuntimeAgents, InteractivePermissionAndQuestionPreemptWithoutLosingChildDraft) {
    namespace ds = loom::ui::dialogs::system;
    auto item = record("permission-child", native::NativeAgentStatus::Failed);
    native::native_agent_store().upsert(item);
    ui::test_seams(app).handle_submit_for_testing("/subagents permission-child");
    auto child = ui::test_seams(app).agent_conversation_state_for_testing();
    ASSERT_TRUE(child);
    app->OnEvent(ftxui::Event::Character("Retain child draft"));
    auto root_state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        ui::test_seams(app).screen_state_for_testing());
    int original_responses = 0;
    ds::ToolPermissionPayload permission;
    permission.id = "immutable-original-request";
    permission.tool_name = "Bash";
    permission.on_response = [&](ds::ToolPermissionPayload::Decision, bool) {
        ++original_responses;
    };
    root_state->dialog_store.dialog_queue.push(permission);
    app->OnEvent(ftxui::Event::Character('y'));
    EXPECT_EQ(original_responses, 1);
    EXPECT_EQ(child->input_text, "Retain child draft");
    EXPECT_EQ(ui::test_seams(app).active_agent_conversation_for_testing(), item.agent_id);
    root_state->dialog_store.dialog_queue.remove(permission.id);
    ds::PromptDialogPayload question;
    question.id = "original-question";
    question.on_response = [](std::optional<std::string>) {};
    root_state->dialog_store.dialog_queue.push(question);
    app->OnEvent(ftxui::Event::Character('x'));
    EXPECT_EQ(child->input_text, "Retain child draft");
    root_state->dialog_store.dialog_queue.remove(question.id);
    app->OnEvent(ftxui::Event::Character('z'));
    EXPECT_EQ(child->input_text, "Retain child draftz");
}

TEST_F(AppRuntimeAgents, InteractiveOrdinaryAndTeamViewsRenderAt40And60And112Columns) {
    for (const bool team : {false, true}) {
        auto item = record(team ? "team-conversation" : "ordinary-conversation",
                           native::NativeAgentStatus::Failed);
        if (team) {
            item.team_name = "research";
            item.teammate_backend = "in-process";
        }
        item.sidechain_entries = {
            R"({"uuid":"width-source","message":{"role":"assistant","content":[{"type":"text","text":"Actual retained conversation answer"}]}})"};
        native::native_agent_store().upsert(item);
        ui::test_seams(app).handle_submit_for_testing("/subagents " + item.agent_id);
        auto child = ui::test_seams(app).agent_conversation_state_for_testing();
        ASSERT_TRUE(child);
        app->OnEvent(ftxui::Event::Character("Child editor text"));
        for (const int columns : {40, 60, 112})
            for (const int rows : {14, 28}) {
                AgentUiEnv term_columns("COLUMNS", std::to_string(columns));
                AgentUiEnv term_lines("LINES", std::to_string(rows));
                AgentUiEnv fullscreen("LOOM_NO_FLICKER", "1");
                auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(columns),
                                                    ftxui::Dimension::Fixed(rows));
                ftxui::Render(screen, app->Render());
                EXPECT_NE(screen.ToString().find(team ? "Team Agent" : "Subagent"),
                          std::string::npos);
                EXPECT_NE(screen.ToString().find("failed"), std::string::npos);
                EXPECT_NE(screen.ToString().find("Child editor text"), std::string::npos);
                EXPECT_NE(screen.ToString().find("Ctrl-C Stop"), std::string::npos);
                EXPECT_EQ(child->input_text, "Child editor text");
                EXPECT_EQ(child->chrome_store.status_bar.cost_usd, std::nullopt);
                const auto caret = screen.cursor();
                ASSERT_GE(caret.x, 0);
                ASSERT_LT(caret.x, columns);
                ASSERT_GE(caret.y, 0);
                ASSERT_LT(caret.y, rows);
                EXPECT_TRUE(screen.PixelAt(caret.x, caret.y).inverted);
            }
        app->OnEvent(ftxui::Event::Special("\x1b[1;3D"));
    }
}

TEST_F(AppRuntimeAgents, InteractiveResultReconciliationStaysWithinTheSelectedChild) {
    auto first = record("result-child-first", native::NativeAgentStatus::Running);
    first.sidechain_entries = {
        R"({"uuid":"first-call","message":{"role":"assistant","content":[{"type":"tool_use","id":"shared-call-id","name":"Bash","input":{"command":"first"}}]}})",
        R"({"uuid":"first-result","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":"shared-call-id","is_error":true,"content":"failed first"}]}})"};
    auto second = record("result-child-second", native::NativeAgentStatus::Running);
    second.sidechain_entries = {
        R"({"uuid":"second-call","message":{"role":"assistant","content":[{"type":"tool_use","id":"shared-call-id","name":"Bash","input":{"command":"second"}}]}})"};
    native::native_agent_store().upsert(first);
    native::native_agent_store().upsert(second);
    ui::test_seams(app).handle_submit_for_testing("/subagents result-child-first");
    auto first_state = ui::test_seams(app).agent_conversation_state_for_testing();
    ASSERT_TRUE(first_state);
    auto call = std::ranges::find_if(first_state->messages_store.messages,
                                     [](const auto& row) { return row.is_tool_use; });
    ASSERT_NE(call, first_state->messages_store.messages.end());
    EXPECT_EQ(call->tool_status, "error");
    ui::test_seams(app).handle_submit_for_testing("/subagents result-child-second");
    auto second_state = ui::test_seams(app).agent_conversation_state_for_testing();
    ASSERT_TRUE(second_state);
    call = std::ranges::find_if(second_state->messages_store.messages,
                                [](const auto& row) { return row.is_tool_use; });
    ASSERT_NE(call, second_state->messages_store.messages.end());
    EXPECT_EQ(call->tool_status, "running");
    EXPECT_EQ(first_state->messages_store.messages.front().tool_status, "error");
}

TEST_F(AppRuntimeAgents,
       RootCommitsWhileChildVisibleNeverReplaceChildProjectionOrConsumeNotification) {
    auto item = record("notification-child", native::NativeAgentStatus::Failed);
    item.sidechain_entries = {
        R"({"uuid":"child-only-row","message":{"role":"assistant","content":[{"type":"text","text":"Private child row"}]}})"};
    native::native_agent_store().upsert(item);
    ui::test_seams(app).handle_submit_for_testing("/subagents notification-child");
    auto child = ui::test_seams(app).agent_conversation_state_for_testing();
    ASSERT_TRUE(child);
    loom::core::AssistantMessage root_message;
    root_message.id.value = "root-new-commit";
    root_message.timestamp = std::chrono::system_clock::now();
    root_message.content.push_back(loom::core::TextBlock{"Root-only result while child visible"});
    engine->append_message_for_testing(loom::core::Message{root_message});
    ui::test_seams(app).set_query_running_for_testing(true);
    ui::test_seams(app).notify_conversation_changed_for_testing();
    (void)app->Render();
    EXPECT_TRUE(std::ranges::any_of(child->messages_store.messages, [](const auto& row) {
        return row.content_preview == "Private child row";
    }));
    EXPECT_FALSE(std::ranges::any_of(child->messages_store.messages, [](const auto& row) {
        return row.content_preview.find("Root-only result") != std::string::npos;
    }));
    const auto root_state = ui::test_seams(app).screen_state_for_testing();
    EXPECT_TRUE(std::ranges::any_of(root_state->messages_store.messages, [](const auto& row) {
        return row.content_preview.find("Root-only result") != std::string::npos;
    }));
    EXPECT_FALSE(native::native_agent_store().get(item.agent_id)->notification_delivered);
    ui::test_seams(app).set_query_running_for_testing(false);
}

TEST_F(AppRuntimeAgents, ChildForegroundApprovalsOverrideOnlySuppressionAndRetainBothActiveDrafts) {
    namespace ds = loom::ui::dialogs::system;
    native::native_agent_store().upsert(
        record("active-draft-child", native::NativeAgentStatus::Failed));
    auto root_state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        ui::test_seams(app).screen_state_for_testing());
    loom::ui::repl_screen::set_prompt_input_text(root_state, "Main draft must stay intact", 7);
    root_state->is_prompt_input_active = true;
    root_state->is_tool_animation_active = true;
    ui::test_seams(app).handle_submit_for_testing("/subagents active-draft-child");
    auto child = ui::test_seams(app).agent_conversation_state_for_testing();
    ASSERT_TRUE(child);
    app->OnEvent(ftxui::Event::Character("Child draft must stay intact"));
    const auto child_cursor = child->input_cursor;
    AgentUiEnv columns("COLUMNS", "60");
    AgentUiEnv lines("LINES", "28");
    AgentUiEnv fullscreen("LOOM_NO_FLICKER", "1");
    int permission_responses = 0;
    int elicitation_responses = 0;
    std::optional<std::string> question_answer;
    for (const int kind : {0, 1, 2}) {
        const std::string marker = kind == 0   ? "TARGET-APPROVAL"
                                   : kind == 1 ? "TARGET-ELICITATION"
                                               : "TARGET-QUESTION";
        const auto id = "original-actionable-" + std::to_string(kind);
        if (kind == 0) {
            ds::ToolPermissionPayload permission;
            permission.id = id;
            permission.tool_name = "Bash";
            permission.description = marker;
            permission.on_response = [&](ds::ToolPermissionPayload::Decision decision, bool) {
                if (decision == ds::ToolPermissionPayload::Decision::AllowOnce)
                    ++permission_responses;
            };
            root_state->dialog_store.dialog_queue.push(std::move(permission));
        } else if (kind == 1) {
            ds::ElicitationPayload elicitation;
            elicitation.id = id;
            elicitation.server_name = "original-server";
            elicitation.request_description = marker;
            elicitation.on_response = [&](bool allowed) {
                if (allowed) ++elicitation_responses;
            };
            root_state->dialog_store.dialog_queue.push(std::move(elicitation));
        } else {
            ds::PromptDialogPayload question;
            question.id = id;
            question.title = "Original question";
            question.prompt_text = marker;
            question.on_response = [&](std::optional<std::string> answer) {
                question_answer = std::move(answer);
            };
            root_state->dialog_store.dialog_queue.push(std::move(question));
        }
        auto screen =
            ftxui::Screen::Create(ftxui::Dimension::Fixed(60), ftxui::Dimension::Fixed(28));
        ftxui::Render(screen, app->Render());
        EXPECT_NE(screen.ToString().find(marker), std::string::npos) << "kind=" << kind;
        EXPECT_TRUE(root_state->prioritize_actionable_dialogs);
        if (kind != 2) {
            // Ignored keys belong to the foreground request, never Main's editor.
            app->OnEvent(ftxui::Event::Character('x'));
            app->OnEvent(ftxui::Event::Character('y'));
        } else {
            app->OnEvent(ftxui::Event::Character("original answer"));
            app->OnEvent(ftxui::Event::Return);
        }
        EXPECT_EQ(root_state->input_text, "Main draft must stay intact");
        EXPECT_EQ(root_state->input_cursor, 7u);
        EXPECT_TRUE(root_state->is_prompt_input_active);
        EXPECT_TRUE(root_state->is_tool_animation_active);
        EXPECT_EQ(child->input_text, "Child draft must stay intact");
        EXPECT_EQ(child->input_cursor, child_cursor);
        EXPECT_EQ(ui::test_seams(app).active_agent_conversation_for_testing(),
                  "active-draft-child");
        root_state->dialog_store.dialog_queue.remove(id);
        app->OnEvent(ftxui::Event::Custom);
        EXPECT_FALSE(root_state->prioritize_actionable_dialogs);
    }
    EXPECT_EQ(permission_responses, 1);
    EXPECT_EQ(elicitation_responses, 1);
    EXPECT_EQ(question_answer, "original answer");
}

TEST_F(AppRuntimeAgents, MainOnlyApprovalSuppressionRemainsUnchangedAfterReturningFromChild) {
    namespace ds = loom::ui::dialogs::system;
    native::native_agent_store().upsert(
        record("return-before-main-prompt", native::NativeAgentStatus::Failed));
    ui::test_seams(app).handle_submit_for_testing("/subagents return-before-main-prompt");
    app->OnEvent(ftxui::Event::Special("\x1b[1;3D"));
    auto root_state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        ui::test_seams(app).screen_state_for_testing());
    loom::ui::repl_screen::set_prompt_input_text(root_state, "Main active draft", 17);
    root_state->is_prompt_input_active = true;
    int responses = 0;
    ds::ElicitationPayload elicitation;
    elicitation.id = "ordinary-main-elicitation";
    elicitation.server_name = "server";
    elicitation.request_description = "MAIN-SUPPRESSED-REQUEST";
    elicitation.on_response = [&](bool) { ++responses; };
    root_state->dialog_store.dialog_queue.push(std::move(elicitation));
    AgentUiEnv columns("COLUMNS", "60");
    AgentUiEnv lines("LINES", "28");
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(60), ftxui::Dimension::Fixed(28));
    ftxui::Render(screen, app->Render());
    EXPECT_FALSE(root_state->prioritize_actionable_dialogs);
    EXPECT_EQ(screen.ToString().find("MAIN-SUPPRESSED-REQUEST"), std::string::npos);
    app->OnEvent(ftxui::Event::Character('x'));
    EXPECT_EQ(root_state->input_text, "Main active draftx");
    EXPECT_EQ(responses, 0);
    root_state->dialog_store.dialog_queue.remove("ordinary-main-elicitation");
}

TEST_F(AppRuntimeAgents, CancelledBatchKeepsPriorSuccessAndPerCallErrorsWithMixedContent) {
    auto item = record("mixed-cancelled-batch", native::NativeAgentStatus::Cancelled);
    item.sidechain_entries = {
        R"({"uuid":"batch-calls","message":{"role":"assistant","content":[{"type":"tool_use","id":"completed-call","name":"Bash","input":{"command":"echo done"}},{"type":"tool_use","id":"cancelled-call","name":"Bash","input":{"command":"sleep 60"}},{"type":"tool_use","id":"unexecuted-call","name":"Bash","input":{"command":"echo never"}}]}})",
        R"({"uuid":"batch-results","message":{"role":"user","content":[{"type":"text","text":"Before result batch"},{"type":"tool_result","tool_use_id":"completed-call","content":[{"type":"text","text":"completed output"},{"type":"image","source":{"type":"base64","media_type":"image/png","data":"aW1hZ2U="}}]},{"type":"text","text":"Between results"},{"type":"tool_result","tool_use_id":"cancelled-call","is_error":true,"content":"cancelled during execution"},{"type":"tool_result","tool_use_id":"unexecuted-call","is_error":true,"content":"not executed"},{"type":"image","source":{"type":"base64","media_type":"image/png","data":"dGFpbA=="}},{"type":"text","text":"After result batch"}]}})"};
    native::native_agent_store().upsert(item);
    ui::test_seams(app).handle_submit_for_testing("/subagents mixed-cancelled-batch");
    const auto child = ui::test_seams(app).agent_conversation_state_for_testing();
    ASSERT_TRUE(child);
    const auto& rows = child->messages_store.messages;
    for (const auto& [id, status] : std::array<std::pair<std::string_view, std::string_view>, 3>{
             {{"completed-call", "success"},
              {"cancelled-call", "error"},
              {"unexecuted-call", "error"}}}) {
        const auto call = std::ranges::find_if(
            rows, [&](const auto& row) { return row.is_tool_use && row.tool_use_id == id; });
        ASSERT_NE(call, rows.end()) << id;
        EXPECT_EQ(call->tool_status, status) << id;
        const auto result = std::ranges::find_if(
            rows, [&](const auto& row) { return row.role == "tool" && row.tool_use_id == id; });
        ASSERT_NE(result, rows.end()) << id;
        EXPECT_EQ(result->tool_status, status) << id;
        EXPECT_EQ(result->is_error, status == "error") << id;
    }
    const auto completed = std::ranges::find_if(rows, [](const auto& row) {
        return row.role == "tool" && row.tool_use_id == "completed-call";
    });
    ASSERT_NE(completed, rows.end());
    ASSERT_TRUE(completed->tool_result_content_items);
    ASSERT_EQ(completed->tool_result_content_items->size(), 2u);
    EXPECT_EQ(completed->tool_result_content_items->back().media_type, "image/png");
    EXPECT_EQ(completed->tool_result_content_items->back().data, "aW1hZ2U=");
    std::vector<std::string> user_text;
    for (const auto& row : rows)
        if (row.role == "user" && !row.is_image) user_text.push_back(row.content_preview);
    EXPECT_EQ(user_text, (std::vector<std::string>{"Before result batch", "Between results",
                                                   "After result batch"}));
    EXPECT_TRUE(std::ranges::any_of(rows, [](const auto& row) {
        return row.role == "user" && row.is_image && row.image_block &&
               row.image_block->data == "dGFpbA==";
    }));
}

TEST(AppAgentResultCodec, SingleAndBatchedResultsPreserveTypedOutcomesAndDocumentContent) {
    auto parsed = loom::utils::json::parse(
        R"({"role":"user","content":[{"type":"tool_result","tool_use_id":"one","is_error":true,"content":[{"type":"text","text":"failed output"},{"type":"image","source":{"type":"base64","media_type":"image/png","data":"aW1hZ2U="}}]},{"type":"document","source":{"type":"base64","media_type":"application/pdf","data":"cGRm"}}]})");
    ASSERT_TRUE(parsed);
    const auto restored = loom::query::parse_session_message_value(parsed->root());
    ASSERT_TRUE(restored);
    const auto* single = std::get_if<loom::core::ToolResultMessage>(&*restored);
    ASSERT_NE(single, nullptr);
    EXPECT_TRUE(single->is_error);
    ASSERT_EQ(single->content.size(), 3u);
    EXPECT_EQ(std::get<loom::core::TextBlock>(single->content[0]).text, "failed output");
    EXPECT_EQ(std::get<loom::core::ImageBlock>(single->content[1]).data, "aW1hZ2U=");
    EXPECT_EQ(std::get<loom::core::DocumentBlock>(single->content[2]).data, "cGRm");
    parsed = loom::utils::json::parse(
        R"({"role":"user","content":[{"type":"tool_result","tool_use_id":"legacy","content":"old successful result"}]})");
    ASSERT_TRUE(parsed);
    const auto legacy = loom::query::parse_session_message_value(parsed->root());
    ASSERT_TRUE(legacy);
    ASSERT_TRUE(std::holds_alternative<loom::core::ToolResultMessage>(*legacy));
    EXPECT_FALSE(std::get<loom::core::ToolResultMessage>(*legacy).is_error);
    parsed = loom::utils::json::parse(
        R"({"role":"user","content":[{"type":"tool_result","tool_use_id":"first-success","content":[{"type":"text","text":"kept success"},{"type":"image","source":{"type":"base64","media_type":"image/png","data":"cGlj"}}]},{"type":"document","source":{"type":"base64","media_type":"application/pdf","data":"YmF0Y2g="}},{"type":"tool_result","tool_use_id":"second-error","is_error":true,"content":"kept error"}]})");
    ASSERT_TRUE(parsed);
    const auto batch = loom::query::parse_session_message_value(parsed->root());
    ASSERT_TRUE(batch);
    const auto* user = std::get_if<loom::core::UserMessage>(&*batch);
    ASSERT_NE(user, nullptr);
    ASSERT_EQ(user->content.size(), 3u);
    const auto& first = std::get<loom::core::ToolResultBlock>(user->content[0]);
    EXPECT_EQ(first.tool_use_id.value, "first-success");
    EXPECT_FALSE(first.is_error);
    const auto& items = std::get<std::vector<loom::core::ToolResultContentItem>>(first.content);
    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items.front().text, "kept success");
    EXPECT_EQ(items.back().data, "cGlj");
    EXPECT_EQ(std::get<loom::core::DocumentBlock>(user->content[1]).data, "YmF0Y2g=");
    const auto& second = std::get<loom::core::ToolResultBlock>(user->content[2]);
    EXPECT_EQ(second.tool_use_id.value, "second-error");
    EXPECT_TRUE(second.is_error);
    EXPECT_EQ(std::get<std::string>(second.content), "kept error");
}
