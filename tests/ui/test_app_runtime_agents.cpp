// RFC 0021: session isolation, native change projection and callback lifetime.
#include <cstdlib>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

import std;
import loom.ui.app.app;
import loom.ui.screens.repl_state;
import loom.ui.screens.task_view_store;
import loom.ui.dialogs.system;
import loom.ui.dialogs.triggers;
import loom.ui.screens.repl_screen;
import loom.ui.features.agents.runtime_agents;
import loom.query.query_engine;
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
    AgentUiEnv(std::string name, const std::string& value) : key(std::move(name)) {
        if (const auto* old = std::getenv(key.c_str())) previous = old;
        setenv(key.c_str(), value.c_str(), 1);
    }
    ~AgentUiEnv() {
        if (previous) setenv(key.c_str(), previous->c_str(), 1);
        else unsetenv(key.c_str());
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
        runtime_dir = std::make_unique<AgentUiEnv>("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
        config_dir = std::make_unique<AgentUiEnv>("LOOM_CONFIG_DIR", (root / "config").string());
        native::native_agent_store().clear_for_testing();
        loom::core::QueryEngineConfig config;
        config.cwd = root.string();
        config.base_url = "http://127.0.0.1:1"; // No test submits a model request.
        config.context_window.auto_compact = false;
        engine = std::make_unique<loom::core::QueryEngine>(std::move(config), tools);
        app = ftxui::Make<ui::AppAdapter>(engine.get(), nullptr, &commands, root / "sessions", [] {});
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

} // namespace

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
    for (const auto status : {native::NativeAgentStatus::Running, native::NativeAgentStatus::Completed,
             native::NativeAgentStatus::Queued, native::NativeAgentStatus::Cancelled,
             native::NativeAgentStatus::Failed}) {
        SCOPED_TRACE(static_cast<int>(status));
        switch (status) {
            case native::NativeAgentStatus::Running: store.upsert(member); break;
            case native::NativeAgentStatus::Completed: store.mark_completed(member.agent_id, "done"); break;
            case native::NativeAgentStatus::Queued: store.enqueue_resume_message(member.agent_id, "continue"); break;
            case native::NativeAgentStatus::Cancelled: store.mark_cancelled(member.agent_id, "cancelled"); break;
            case native::NativeAgentStatus::Failed: store.mark_failed(member.agent_id, "fixture failure"); break;
        }
        EXPECT_TRUE(ui::test_seams(app).runtime_agent_dirty_for_testing()->load());
        drain();
        EXPECT_TRUE(ui::test_seams(app).is_query_running_for_testing());
        EXPECT_FALSE(ui::test_seams(app).runtime_agent_dirty_for_testing()->load());
        EXPECT_TRUE(instances().empty()) << "Team Agents must not also appear as Subagents";
        const auto& teammates = ui::test_seams(app).screen_state_for_testing()->task_view_store.live_teammates;
        EXPECT_EQ(teammates.size(), 1u);
        EXPECT_EQ(ui::test_seams(app).screen_state_for_testing()->task_view_store.teammate_count, 1);
        if (teammates.size() == 1) {
            EXPECT_EQ(teammates.front().agent_id, member.agent_id);
            const auto expected = status == native::NativeAgentStatus::Running || status == native::NativeAgentStatus::Queued
                ? "running" : status == native::NativeAgentStatus::Failed ? "unknown" : "idle";
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
    for (const auto& item : instances()) ids.push_back(item.agent_id);
    EXPECT_EQ(ids, (std::vector<std::string>{"active-earlier", "a-active", "z-active", "terminal-old"}));
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
    EXPECT_FALSE(ui::test_seams(app).resume_session_for_testing(root / "sessions", "missing-session"));
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
    auto other_app = ftxui::Make<ui::AppAdapter>(&other_engine, nullptr, &commands, root / "other-sessions", [] {});
    auto& store = native::native_agent_store();
    store.upsert(record("first-app", native::NativeAgentStatus::Running));
    auto other = record("second-app", native::NativeAgentStatus::Cancelled);
    other.parent_session_id = other_engine.session_id().value;
    store.upsert(other);
    drain();
    other_app->OnEvent(ftxui::Event::Custom);
    ASSERT_EQ(instances().size(), 1u);
    EXPECT_EQ(instances().front().agent_id, "first-app");
    const auto& other_instances = ui::test_seams(other_app).screen_state_for_testing()->task_view_store.runtime_agents;
    ASSERT_EQ(other_instances.size(), 1u);
    EXPECT_EQ(other_instances.front().agent_id, "second-app");
}

TEST_F(AppRuntimeAgents, LateAdmittedCallbackPassesDirtyGuardButClosedSharedQueueDropsDispatch) {
    auto callback = ui::test_seams(app).runtime_agent_change_callback_for_testing();
    auto dirty = ui::test_seams(app).runtime_agent_dirty_for_testing();
    auto dispatch = ui::test_seams(app).ui_dispatcher_for_testing();
    std::promise<void> entered, proceed;
    auto gate = proceed.get_future();
    auto late = std::async(std::launch::async, [callback, &entered, gate = std::move(gate)]() mutable {
        entered.set_value();
        gate.wait();
        callback();
    });
    entered.get_future().wait();
    app.reset(); // Production destructor closes the queue before unsubscribe.
    dirty->store(false); // Prove the late callback reaches dispatch, rather than coalescing away.
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
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(ui::test_seams(app).screen_state_for_testing());
    auto modal = state->dialog_store.dialog_queue.peek_modal_mut();
    ASSERT_TRUE(modal);
    EXPECT_EQ(loom::ui::dialogs::system::type_of(modal->get()), loom::ui::dialogs::system::DialogType::AgentInstances);
    auto& payload = std::get<loom::ui::dialogs::system::AgentInstancesPayload>(modal->get());
    payload.on_resize(60, 22);
    auto component = *std::static_pointer_cast<ftxui::Component>(payload.component);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(60), ftxui::Dimension::Fixed(22));
    ftxui::Render(screen, component->Render());
    EXPECT_NE(screen.ToString().find("owned-ended"), std::string::npos);
    EXPECT_EQ(screen.ToString().find("foreign-ended"), std::string::npos);
    component->OnEvent(ftxui::Event::Return);
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
    EXPECT_FALSE(ui::test_seams(app).screen_state_for_testing()->dialog_store.dialog_queue.has_modal());
}

TEST_F(AppRuntimeAgents, InstanceBrowserYieldsToPendingAndNewPermission) {
    namespace ds = loom::ui::dialogs::system;
    namespace dr = loom::ui::repl_screen::dialog_queue_render;
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(ui::test_seams(app).screen_state_for_testing());
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
    native::native_agent_store().upsert(record("ordinary-only", native::NativeAgentStatus::Running));
    drain();
    ui::test_seams(app).handle_submit_for_testing("/subagents team");
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(ui::test_seams(app).screen_state_for_testing());
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
    ASSERT_TRUE(ui::test_seams(app).screen_state_for_testing()->dialog_store.dialog_queue.has_modal());
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
    EXPECT_EQ(ui::test_seams(app).screen_state_for_testing()->task_view_store.spinner_tip, "Cancelling...");
    EXPECT_TRUE(ui::test_seams(app).screen_state_for_testing()->dialog_store.dialog_queue.has_modal());
    ui::test_seams(app).set_query_running_for_testing(false);
}

TEST_F(AppRuntimeAgents, InstanceBrowserYieldsToBothSandboxPermissionKinds) {
    namespace ds = loom::ui::dialogs::system;
    namespace dr = loom::ui::repl_screen::dialog_queue_render;
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(ui::test_seams(app).screen_state_for_testing());
    ds::SandboxPermissionPayload sandbox;
    sandbox.id = "sandbox-original";
    sandbox.host_pattern = "localhost";
    sandbox.on_response = [](bool, bool) {};
    // This legacy payload has no default App renderer. Exercise preemption
    // only after a client makes it actionable, matching the production guard.
    state->dialog_store.dialog_renderers.register_dialog(ds::DialogType::WorkerSandboxPermission,
        [](ds::DialogPayloadVariant&, const ds::DialogRenderContext&) { return ftxui::text("Worker permission"); },
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
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(ui::test_seams(app).screen_state_for_testing());
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
    store.begin_execution(record("stop-ack", native::NativeAgentStatus::Running), tools.background_stop_token());
    ui::test_seams(app).handle_submit_for_testing("/subagents stop-ack");
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(ui::test_seams(app).screen_state_for_testing());
    auto modal = state->dialog_store.dialog_queue.peek_modal_mut();
    ASSERT_TRUE(modal);
    auto& payload = std::get<loom::ui::dialogs::system::AgentInstancesPayload>(modal->get());
    auto component = *std::static_pointer_cast<ftxui::Component>(payload.component);
    component->OnEvent(ftxui::Event::Character('s'));
    EXPECT_EQ(state->footer_dynamic_text, "Cancellation requested");
    EXPECT_TRUE(store.get("stop-ack")->cancel_requested);
    EXPECT_EQ(store.get("stop-ack")->status, native::NativeAgentStatus::Running);
    store.end_execution("stop-ack");
}


TEST_F(AppRuntimeAgents, InstanceModalCoversTranscriptFragmentsAtBothHorizontalEdges) {
    namespace dr = loom::ui::repl_screen::dialog_queue_render;
    ui::test_seams(app).handle_submit_for_testing("/subagents");
    auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(ui::test_seams(app).screen_state_for_testing());
    for (const int columns : {40, 60, 120}) {
        auto prepared = dr::PrepareDialogLayers(*state, false, true, columns, 28);
        ftxui::Elements background;
        for (int row = 0; row < 28; ++row) background.push_back(ftxui::text(std::string(columns, 'Z')));
        auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(columns), ftxui::Dimension::Fixed(28));
        ftxui::Render(screen, dr::LayerAllDialogs(ftxui::vbox(std::move(background)), std::move(prepared)));
        for (int x = 0; x < columns; ++x)
            EXPECT_NE(screen.PixelAt(x, 4).character, "Z") << "columns=" << columns << " x=" << x;
    }
}
