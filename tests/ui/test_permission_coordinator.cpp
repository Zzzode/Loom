// Request ownership and shutdown regressions for concurrent Agent permission UI.
#include "test_ui_helpers.h"

import std;
import loom.ui.app.permission_coordinator;
import loom.tools.tool;
import loom.ui.dialogs.system;
import loom.ui.app.app;
import loom.ui.screens.repl_state;
import loom.query.query_engine;
import loom.commands.registry;
import loom.services.mcp.elicitation_handler;
import loom.tools.agent_runtime;
import loom.tools.ask_user;

namespace {
using namespace std::chrono_literals;
using loom::ui::PermissionCoordinator;
namespace dsys = loom::ui::dialogs::system;
struct AbortOnExit {
    PermissionCoordinator& coordinator;
    ~AbortOnExit() { coordinator.abort_all(); }
};
}

TEST(PermissionCoordinator, ToolRequestsSerializeThroughCleanupAndRejectLateDecisions) {
    PermissionCoordinator coordinator;
    const auto first = coordinator.begin_permission_request();
    ASSERT_NE(first, 0);
    std::promise<void> entered;
    auto entering = entered.get_future();
    auto queued = std::async(std::launch::async, [&] {
        entered.set_value();
        return coordinator.begin_permission_request();
    });
    AbortOnExit cleanup{coordinator};
    ASSERT_EQ(entering.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(queued.wait_for(20ms), std::future_status::timeout);
    EXPECT_TRUE(coordinator.resolve_permission(first, false));
    EXPECT_FALSE(coordinator.wait_permission_response(first));
    // A decision alone does not permit B to replace A's visible request.
    EXPECT_EQ(queued.wait_for(20ms), std::future_status::timeout);
    coordinator.finish_permission_request(first);
    ASSERT_EQ(queued.wait_for(2s), std::future_status::ready);
    const auto second = queued.get();
    ASSERT_NE(second, first);
    EXPECT_FALSE(coordinator.resolve_permission(first, true, "Bash"));
    EXPECT_FALSE(coordinator.is_always_allowed("Bash"));
    coordinator.finish_permission_request(first);
    EXPECT_TRUE(coordinator.permission_request_current(second));
    EXPECT_TRUE(coordinator.resolve_permission(second, true, "Read"));
    EXPECT_FALSE(coordinator.resolve_permission(second, false));
    EXPECT_TRUE(coordinator.wait_permission_response(second));
    EXPECT_TRUE(coordinator.is_always_allowed("Read"));
    coordinator.finish_permission_request(second);
    EXPECT_FALSE(coordinator.permission_request_current(second));
}

TEST(PermissionCoordinator, ElicitationRequestsSerializeAndOppositeResponsesStayBound) {
    PermissionCoordinator coordinator;
    const auto first = coordinator.begin_elicitation_request();
    std::promise<void> entered;
    auto entering = entered.get_future();
    auto queued = std::async(std::launch::async, [&] {
        entered.set_value();
        return coordinator.begin_elicitation_request();
    });
    AbortOnExit cleanup{coordinator};
    ASSERT_EQ(entering.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(queued.wait_for(20ms), std::future_status::timeout);
    EXPECT_TRUE(coordinator.resolve_elicitation(first, true));
    EXPECT_TRUE(coordinator.wait_elicitation_response(first));
    EXPECT_EQ(queued.wait_for(20ms), std::future_status::timeout);
    coordinator.finish_elicitation_request(first);
    ASSERT_EQ(queued.wait_for(2s), std::future_status::ready);
    const auto second = queued.get();
    EXPECT_NE(second, first);
    EXPECT_FALSE(coordinator.resolve_elicitation(first, true));
    coordinator.finish_elicitation_request(first);
    EXPECT_TRUE(coordinator.elicitation_request_current(second));
    EXPECT_TRUE(coordinator.resolve_elicitation(second, false));
    EXPECT_FALSE(coordinator.wait_elicitation_response(second));
    coordinator.finish_elicitation_request(second);
}

TEST(PermissionCoordinator, ShutdownReleasesActiveAndQueuedWithoutUiCleanup) {
    PermissionCoordinator coordinator;
    const auto tool = coordinator.begin_permission_request();
    const auto elicitation = coordinator.begin_elicitation_request();
    std::latch entered{4};
    auto active_tool = std::async(std::launch::async, [&] {
        entered.count_down();
        return coordinator.wait_permission_response(tool);
    });
    auto active_elicitation = std::async(std::launch::async, [&] {
        entered.count_down();
        return coordinator.wait_elicitation_response(elicitation);
    });
    auto queued_tool = std::async(std::launch::async, [&] {
        entered.count_down();
        return coordinator.begin_permission_request();
    });
    auto queued_elicitation = std::async(std::launch::async, [&] {
        entered.count_down();
        return coordinator.begin_elicitation_request();
    });
    AbortOnExit cleanup{coordinator};
    entered.wait();
    coordinator.abort_all();
    EXPECT_EQ(active_tool.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(active_elicitation.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(queued_tool.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(queued_elicitation.wait_for(2s), std::future_status::ready);
    EXPECT_FALSE(active_tool.get());
    EXPECT_FALSE(active_elicitation.get());
    EXPECT_EQ(queued_tool.get(), 0);
    EXPECT_EQ(queued_elicitation.get(), 0);
    EXPECT_FALSE(coordinator.permission_request_current(tool));
    EXPECT_FALSE(coordinator.elicitation_request_current(elicitation));
    EXPECT_FALSE(coordinator.resolve_permission(tool, true));
    EXPECT_FALSE(coordinator.resolve_elicitation(elicitation, true));
    EXPECT_EQ(coordinator.begin_permission_request(), 0);
    EXPECT_EQ(coordinator.begin_elicitation_request(), 0);
}

TEST(PermissionCoordinator, DelayedShowAndExactCleanupCannotAffectNewRequestOrOtherDialog) {
    PermissionCoordinator coordinator;
    dsys::DialogQueue queue;
    const auto first = coordinator.begin_permission_request();
    const auto first_id = std::format("tool-permission-{}", first);
    auto delayed_show = [&, first, first_id] {
        if (!coordinator.permission_request_current(first)) return;
        dsys::ToolPermissionPayload payload;
        payload.id = first_id;
        queue.push(std::move(payload));
    };
    auto delayed_cleanup = [&, first, first_id] {
        if (!coordinator.permission_request_current(first)) return;
        queue.remove(first_id);
        coordinator.finish_permission_request(first);
    };
    delayed_show();
    EXPECT_TRUE(coordinator.resolve_permission(first, false));
    delayed_cleanup();
    const auto second = coordinator.begin_permission_request();
    dsys::ToolPermissionPayload second_payload;
    second_payload.id = std::format("tool-permission-{}", second);
    queue.push(std::move(second_payload));
    dsys::ElicitationPayload unrelated;
    unrelated.id = "unrelated-server-request";
    queue.push(std::move(unrelated));
    delayed_show();
    delayed_cleanup();
    ASSERT_TRUE(queue.peek_overlay());
    EXPECT_EQ(dsys::id_of(queue.peek_overlay()->get()),
              std::format("tool-permission-{}", second));
    EXPECT_TRUE(queue.has_any_bottom());
    EXPECT_TRUE(coordinator.permission_request_current(second));
    coordinator.abort_all();
    delayed_show();
    EXPECT_FALSE(coordinator.permission_request_current(first));
}

TEST(PermissionCoordinator, ElicitationCleanupDoesNotPopUnrelatedHigherPriorityDialog) {
    PermissionCoordinator coordinator;
    dsys::DialogQueue queue;
    const auto token = coordinator.begin_elicitation_request();
    const auto id = std::format("elicitation-{}", token);
    dsys::ElicitationPayload request;
    request.id = id;
    queue.push(std::move(request));
    dsys::CostThresholdPayload cost;
    cost.id = "cost-threshold-owned-by-another-request";
    queue.push(std::move(cost));
    EXPECT_TRUE(coordinator.resolve_elicitation(token, false));
    EXPECT_FALSE(coordinator.wait_elicitation_response(token));
    if (coordinator.elicitation_request_current(token)) {
        queue.remove(id);
        coordinator.finish_elicitation_request(token);
    }
    ASSERT_TRUE(queue.peek_bottom(false));
    EXPECT_EQ(dsys::id_of(queue.peek_bottom(false)->get()),
              "cost-threshold-owned-by-another-request");
}

TEST(PermissionCoordinator, DistinctRequestKindsRemainIndependentUntilGlobalShutdown) {
    PermissionCoordinator coordinator;
    const auto tool = coordinator.begin_permission_request();
    const auto elicitation = coordinator.begin_elicitation_request();
    EXPECT_TRUE(coordinator.resolve_permission(tool, false));
    coordinator.finish_permission_request(tool);
    EXPECT_TRUE(coordinator.elicitation_request_current(elicitation));
    EXPECT_TRUE(coordinator.resolve_elicitation(elicitation, true));
    EXPECT_TRUE(coordinator.wait_elicitation_response(elicitation));
    coordinator.finish_elicitation_request(elicitation);
}

namespace {
class AppPermissionRequests : public testing::Test {
protected:
    ScopedEnvVar home_{"HOME"};
    ScopedEnvVar config_dir_{"LOOM_CONFIG_DIR"};
    std::filesystem::path root_;
    loom::core::ToolRegistry tools_;
    std::unique_ptr<loom::core::QueryEngine> engine_;
    loom::commands::AppCommandRegistry commands_;
    ftxui::Component app_;
    std::vector<std::string> status_reports_;

    void SetUp() override {
        root_ = std::filesystem::temp_directory_path() /
            std::format("loom_permission_requests_{}", std::chrono::steady_clock::now()
                .time_since_epoch().count());
        std::filesystem::create_directories(root_);
        home_.set(root_.string());
        config_dir_.set((root_ / "config").string());
        loom::core::QueryEngineConfig config;
        config.context_window.auto_compact = false;
        config.cwd = root_.string();
        engine_ = std::make_unique<loom::core::QueryEngine>(std::move(config), tools_);
        app_ = ftxui::Make<loom::ui::AppAdapter>(
            engine_.get(), nullptr, &commands_, root_ / "sessions", [] {});
    }
    void TearDown() override {
        app_.reset();
        engine_.reset();
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }
    bool AwaitToolRequest(std::string_view tool, std::string_view description) {
        return wait_until([&] {
            // The queue also carries settings/skill/observer work. A nonempty
            // queue and a single drain do not establish that this request is
            // visible; wait on the actual typed payload owned by this caller.
            (void)app_->Render();
            const auto state = loom::ui::test_seams(app_).screen_state_for_testing();
            const auto overlay = state->dialog_store.dialog_queue.peek_overlay();
            if (!overlay) return false;
            const auto* payload = std::get_if<dsys::ToolPermissionPayload>(&overlay->get());
            return payload && payload->tool_name == tool &&
                   payload->description == description &&
                   payload->id.starts_with("tool-permission-");
        }, 2s);
    }
    bool AwaitElicitation(std::string_view server, std::string_view description) {
        return wait_until([&] {
            (void)app_->Render();
            const auto state = loom::ui::test_seams(app_).screen_state_for_testing();
            const auto bottom = state->dialog_store.dialog_queue.peek_bottom(false);
            if (!bottom) return false;
            const auto* payload = std::get_if<dsys::ElicitationPayload>(&bottom->get());
            return payload && payload->server_name == server &&
                   payload->request_description == description &&
                   payload->id.starts_with("elicitation-");
        }, 2s);
    }
    loom::ui::AppAdapter* Adapter() {
        return static_cast<loom::ui::AppAdapter*>(app_.get());
    }
};
struct StopAppOnExit {
    ftxui::Component& app;
    ~StopAppOnExit() {
        if (app) (void)loom::ui::test_seams(app).stop_background_workers_for_testing();
    }
};
}

TEST_F(AppPermissionRequests, ConcurrentToolCallbacksKeepLateResponseAndAbortBoundToOriginal) {
    const auto callback = Adapter()->get_permission_callback();
    auto first = std::async(std::launch::async, [callback] {
        return callback("Write", "first child request");
    });
    StopAppOnExit cleanup{app_};
    ASSERT_TRUE(AwaitToolRequest("Write", "first child request"));
    (void)app_->Render();
    const auto state = loom::ui::test_seams(app_).screen_state_for_testing();
    ASSERT_TRUE(state->dialog_store.dialog_queue.peek_overlay());
    const auto* payload = std::get_if<dsys::ToolPermissionPayload>(
        &state->dialog_store.dialog_queue.peek_overlay()->get());
    ASSERT_NE(payload, nullptr);
    const auto old_id = payload->id;
    const auto late_response = payload->on_response;
    const auto late_abort = payload->on_abort;
    (void)app_->OnEvent(ftxui::Event::Escape);
    // A user decision removes its exact overlay in this same UI event. The
    // worker's later dispatched finish still gates the next request kind.
    EXPECT_FALSE(state->dialog_store.dialog_queue.has_overlay());
    ASSERT_EQ(first.wait_for(2s), std::future_status::ready);
    EXPECT_FALSE(first.get());

    auto second = std::async(std::launch::async, [callback] {
        return callback("Bash", "second child request");
    });
    // This guard must outlive second even if an assertion exits early.
    StopAppOnExit cleanup_second{app_};
    EXPECT_EQ(second.wait_for(20ms), std::future_status::timeout);
    EXPECT_FALSE(state->dialog_store.dialog_queue.has_overlay());
    (void)app_->Render();  // Exact-ID cleanup releases the next caller.
    ASSERT_TRUE(AwaitToolRequest("Bash", "second child request"));
    (void)app_->Render();
    ASSERT_TRUE(state->dialog_store.dialog_queue.peek_overlay());
    const auto* current = std::get_if<dsys::ToolPermissionPayload>(
        &state->dialog_store.dialog_queue.peek_overlay()->get());
    ASSERT_NE(current, nullptr);
    EXPECT_NE(current->id, old_id);
    EXPECT_EQ(current->tool_name, "Bash");
    late_response(dsys::ToolPermissionPayload::Decision::AlwaysAllow, false);
    late_abort();
    EXPECT_EQ(second.wait_for(20ms), std::future_status::timeout);
    ASSERT_TRUE(state->dialog_store.dialog_queue.peek_overlay());
    EXPECT_EQ(dsys::id_of(state->dialog_store.dialog_queue.peek_overlay()->get()), current->id);
    current->on_response(dsys::ToolPermissionPayload::Decision::AllowOnce, false);
    ASSERT_EQ(second.wait_for(2s), std::future_status::ready);
    EXPECT_TRUE(second.get());
    (void)app_->Render();
    EXPECT_FALSE(state->dialog_store.dialog_queue.has_overlay());
}

TEST_F(AppPermissionRequests, ConcurrentElicitationsKeepLateDecisionBoundAndCleanupExact) {
    auto first = std::async(std::launch::async, [] {
        return loom::services::mcp::handle_elicitation({
            .server_name = "first-server", .message = "first request", .schema = {}});
    });
    StopAppOnExit cleanup{app_};
    ASSERT_TRUE(AwaitElicitation("first-server", "first request"));
    (void)app_->Render();
    const auto state = loom::ui::test_seams(app_).screen_state_for_testing();
    const auto first_bottom = state->dialog_store.dialog_queue.peek_bottom(false);
    ASSERT_TRUE(first_bottom);
    const auto* payload = std::get_if<dsys::ElicitationPayload>(&first_bottom->get());
    ASSERT_NE(payload, nullptr);
    const auto old_id = payload->id;
    const auto late_response = payload->on_response;
    const auto late_cancel = payload->on_cancel;
    late_response(true);
    ASSERT_EQ(first.wait_for(2s), std::future_status::ready);
    EXPECT_TRUE(first.get().has_value());
    auto second = std::async(std::launch::async, [] {
        return loom::services::mcp::handle_elicitation({
            .server_name = "second-server", .message = "second request", .schema = {}});
    });
    StopAppOnExit cleanup_second{app_};
    (void)app_->Render();
    ASSERT_TRUE(AwaitElicitation("second-server", "second request"));
    (void)app_->Render();
    const auto second_bottom = state->dialog_store.dialog_queue.peek_bottom(false);
    ASSERT_TRUE(second_bottom);
    const auto* current = std::get_if<dsys::ElicitationPayload>(&second_bottom->get());
    ASSERT_NE(current, nullptr);
    EXPECT_NE(current->id, old_id);
    EXPECT_EQ(current->server_name, "second-server");
    late_response(true);
    late_cancel();
    EXPECT_EQ(second.wait_for(20ms), std::future_status::timeout);
    EXPECT_EQ(dsys::id_of(state->dialog_store.dialog_queue.peek_bottom(false)->get()), current->id);
    current->on_response(false);
    ASSERT_EQ(second.wait_for(2s), std::future_status::ready);
    EXPECT_FALSE(second.get().has_value());
    (void)app_->Render();
    EXPECT_FALSE(state->dialog_store.dialog_queue.has_any_bottom());
}

TEST_F(AppPermissionRequests, LegacyPanelDecisionCannotAnswerNativeRequestFromAnotherView) {
    const auto callback = Adapter()->get_permission_callback();
    auto response = std::async(std::launch::async, [callback] {
        return callback("Write", "native request owned by original caller");
    });
    StopAppOnExit cleanup{app_};
    ASSERT_TRUE(AwaitToolRequest("Write", "native request owned by original caller"));
    (void)app_->Render();
    const auto state = std::const_pointer_cast<loom::ui::repl_screen::ReplScreenState>(
        loom::ui::test_seams(app_).screen_state_for_testing());
    const auto overlay = state->dialog_store.dialog_queue.peek_overlay();
    ASSERT_TRUE(overlay);
    const auto* payload = std::get_if<dsys::ToolPermissionPayload>(&overlay->get());
    ASSERT_NE(payload, nullptr);
    const auto original_response = payload->on_response;
    // Isolate the old mode callback: without its token it must not answer the
    // native waiter, regardless of which view/panel currently handles input.
    const auto original_id = payload->id;
    state->dialog_store.RemoveDialog(original_id);
    state->mode = loom::ui::repl_screen::ReplMode::SandboxPermission;
    (void)app_->OnEvent(ftxui::Event::Character('y'));
    EXPECT_EQ(response.wait_for(20ms), std::future_status::timeout);
    original_response(dsys::ToolPermissionPayload::Decision::Deny, false);
    ASSERT_EQ(response.wait_for(2s), std::future_status::ready);
    EXPECT_FALSE(response.get());
    (void)app_->Render();
}


TEST_F(AppPermissionRequests, OwnedStopLeavesUnansweredElicitationPendingAndSiblingIndependent) {
    namespace runtime = loom::tools::agent_runtime;
    ScopedEnvVar runtime_dir{"LOOM_AGENT_RUNTIME_DIR"};
    runtime_dir.set((root_ / "runtime").string());
    // Captured synchronization objects outlive Store worker joins even when an
    // assertion exits the test before its explicit successful-path drain.
    std::promise<void> target_finished;
    auto target_done = target_finished.get_future();
    std::atomic<bool> target_approved{true};
    std::promise<void> sibling_entered;
    auto sibling_started = sibling_entered.get_future();
    std::promise<void> sibling_finished;
    auto sibling_done = sibling_finished.get_future();
    std::atomic<bool> sibling_approved{false};
    runtime::NativeAgentStore store;
    const auto owner = tools_.background_stop_token();
    constexpr std::string_view session = "elicitation-owner-session";
    auto target = runtime::make_native_agent_record("elicitation-target", "runtime");
    target.parent_session_id = session;
    target.background = true;
    store.upsert(std::move(target));
    auto sibling = runtime::make_native_agent_record("elicitation-sibling", "runtime");
    sibling.parent_session_id = session;
    sibling.background = true;
    store.upsert(std::move(sibling));

    store.run_background_agent("elicitation-target", [&] {
        const auto response = loom::services::mcp::handle_elicitation({
            .server_name = "target-server", .message = "target approval pending", .schema = {}});
        target_approved = response.has_value();
        store.finish_agent("elicitation-target", response
            ? std::expected<std::string, std::string>{"target approved"}
            : std::expected<std::string, std::string>{std::unexpected(std::string{"target request declined"})});
        target_finished.set_value();
    }, owner);
    // Abort the application responder before Store destruction if an assertion
    // exits early, so its owned workers never depend on a closed UI for joins.
    StopAppOnExit cleanup{app_};
    ASSERT_TRUE(AwaitElicitation("target-server", "target approval pending"));
    (void)app_->Render();
    const auto state = loom::ui::test_seams(app_).screen_state_for_testing();
    const auto original_bottom = state->dialog_store.dialog_queue.peek_bottom(false);
    ASSERT_TRUE(original_bottom);
    const auto* original = std::get_if<dsys::ElicitationPayload>(&original_bottom->get());
    ASSERT_NE(original, nullptr);
    ASSERT_EQ(original->server_name, "target-server");
    const auto target_dialog_id = original->id;
    const auto target_response = original->on_response;

    store.run_background_agent("elicitation-sibling", [&] {
        sibling_entered.set_value();
        const auto response = loom::services::mcp::handle_elicitation({
            .server_name = "sibling-server", .message = "sibling approval pending", .schema = {}});
        sibling_approved = response.has_value();
        store.finish_agent("elicitation-sibling", response
            ? std::expected<std::string, std::string>{"sibling approved"}
            : std::expected<std::string, std::string>{std::unexpected(std::string{"sibling request declined"})});
        sibling_finished.set_value();
    }, owner);
    ASSERT_EQ(sibling_started.wait_for(2s), std::future_status::ready);
    ASSERT_TRUE(store.request_owned_cancel("elicitation-target", session, owner));
    const auto pending_target = store.get("elicitation-target");
    ASSERT_TRUE(pending_target);
    EXPECT_TRUE(pending_target->cancel_requested);
    EXPECT_EQ(pending_target->status, runtime::NativeAgentStatus::Running);
    EXPECT_EQ(target_done.wait_for(20ms), std::future_status::timeout);
    EXPECT_EQ(sibling_done.wait_for(20ms), std::future_status::timeout);
    EXPECT_FALSE(owner.stop_requested());
    ASSERT_TRUE(state->dialog_store.dialog_queue.peek_bottom(false));
    EXPECT_EQ(dsys::id_of(state->dialog_store.dialog_queue.peek_bottom(false)->get()),
              target_dialog_id);
    const auto pending_sibling = store.get("elicitation-sibling");
    ASSERT_TRUE(pending_sibling);
    EXPECT_FALSE(pending_sibling->cancel_requested);
    EXPECT_EQ(pending_sibling->status, runtime::NativeAgentStatus::Running);

    // The still-reachable original prompt resolves only its original worker.
    target_response(false);
    ASSERT_EQ(target_done.wait_for(2s), std::future_status::ready);
    EXPECT_FALSE(target_approved.load());
    EXPECT_TRUE(wait_until([&] {
        return store.resume_unavailable_reason("elicitation-target").empty();
    }, 2s));
    const auto cancelled_target = store.get("elicitation-target");
    ASSERT_TRUE(cancelled_target);
    EXPECT_EQ(cancelled_target->status, runtime::NativeAgentStatus::Cancelled);
    EXPECT_EQ(cancelled_target->error, "target request declined");
    EXPECT_EQ(sibling_done.wait_for(20ms), std::future_status::timeout);
    (void)app_->Render();  // Target's exact-ID cleanup admits the queued sibling.
    ASSERT_TRUE(AwaitElicitation("sibling-server", "sibling approval pending"));
    (void)app_->Render();
    const auto sibling_bottom = state->dialog_store.dialog_queue.peek_bottom(false);
    ASSERT_TRUE(sibling_bottom);
    const auto* current = std::get_if<dsys::ElicitationPayload>(&sibling_bottom->get());
    ASSERT_NE(current, nullptr);
    EXPECT_EQ(current->server_name, "sibling-server");
    EXPECT_NE(current->id, target_dialog_id);
    target_response(true);  // Late approval cannot leak to the sibling.
    EXPECT_EQ(sibling_done.wait_for(20ms), std::future_status::timeout);
    current->on_response(true);
    ASSERT_EQ(sibling_done.wait_for(2s), std::future_status::ready);
    store.wait_for_background_agents();
    EXPECT_TRUE(sibling_approved.load());
    const auto completed_sibling = store.get("elicitation-sibling");
    ASSERT_TRUE(completed_sibling);
    EXPECT_EQ(completed_sibling->status, runtime::NativeAgentStatus::Completed);
    EXPECT_FALSE(completed_sibling->cancel_requested);
    EXPECT_FALSE(owner.stop_requested());
    (void)app_->Render();
    EXPECT_FALSE(state->dialog_store.dialog_queue.has_any_bottom());
}


TEST_F(AppPermissionRequests, ApprovalEmitsWorkingBeforeImmediatelyFollowingRootQuestion) {
    loom::ui::test_seams(app_).start_program_status_for_testing([this](std::string_view bytes) {
        status_reports_.emplace_back(bytes);
        return true;
    });
    (void)app_->OnEvent(ftxui::Event::Special("\x1b]7501;?\x1b\\"));
    loom::ui::test_seams(app_).set_query_running_for_testing(true);
    (void)app_->OnEvent(ftxui::Event::Custom);
    ASSERT_FALSE(status_reports_.empty());
    ASSERT_NE(status_reports_.back().find("state=working"), std::string::npos);
    const auto permission = Adapter()->get_permission_callback();
    const auto ask_user = loom::tools::get_global_ask_user_responder();
    ASSERT_TRUE(ask_user);
    auto response = std::async(std::launch::async, [permission, ask_user] {
        if (!permission("AskUser", "approve following root question"))
            return std::optional<std::string>{};
        // No intervening barrier: reproduce the real worker advancing from
        // permission directly into the root-only question responder.
        return ask_user("Question immediately after approval", std::nullopt);
    });
    StopAppOnExit cleanup{app_};
    ASSERT_TRUE(AwaitToolRequest("AskUser", "approve following root question"));
    (void)app_->OnEvent(ftxui::Event::Custom);
    ASSERT_NE(status_reports_.back().find("state=blocked"), std::string::npos);
    ASSERT_NE(status_reports_.back().find("kind=permission"), std::string::npos);
    const auto permission_report = status_reports_.size() - 1;
    (void)app_->OnEvent(ftxui::Event::Character('y'));
    const auto state = loom::ui::test_seams(app_).screen_state_for_testing();
    EXPECT_FALSE(state->dialog_store.dialog_queue.has_overlay());
    ASSERT_EQ(status_reports_.size(), permission_report + 2);
    EXPECT_NE(status_reports_.back().find("state=working"), std::string::npos);
    ASSERT_TRUE(wait_until([&] {
        (void)app_->Render();
        const auto bottom = state->dialog_store.dialog_queue.peek_bottom(false);
        if (!bottom) return false;
        const auto* prompt = std::get_if<dsys::PromptDialogPayload>(&bottom->get());
        return prompt && prompt->prompt_text == "Question immediately after approval";
    }, 2s));
    (void)app_->OnEvent(ftxui::Event::Custom);
    ASSERT_EQ(status_reports_.size(), permission_report + 3);
    EXPECT_NE(status_reports_.back().find("state=blocked"), std::string::npos);
    EXPECT_NE(status_reports_.back().find("kind=question"), std::string::npos);
    const auto bottom = state->dialog_store.dialog_queue.peek_bottom(false);
    ASSERT_TRUE(bottom);
    const auto* prompt = std::get_if<dsys::PromptDialogPayload>(&bottom->get());
    ASSERT_NE(prompt, nullptr);
    prompt->on_response(std::string{"answered"});
    ASSERT_EQ(response.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(response.get(), "answered");
    (void)app_->Render();
    loom::ui::test_seams(app_).set_query_running_for_testing(false);
}
