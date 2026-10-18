// App ingress/output boundaries exercised through real components and workers.
#include <gtest/gtest.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include "test_ui_helpers.h"

import std;
import loom.ui.app.app;
import loom.ui.screens.repl_state;
import loom.ui.screens.repl_screen;
import loom.ui.dialogs.system;
import loom.query.query_engine;
import loom.tools.tool;
import loom.commands.registry;

namespace {
using namespace std::chrono_literals;
namespace ui = loom::ui;
namespace repl = loom::ui::repl_screen;

class AppProgramStatus : public ::testing::Test {
protected:
    void SetUp() override {
        storage = std::filesystem::temp_directory_path() /
            ("loom-status-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        loom::core::QueryEngineConfig config;
        config.cwd = std::filesystem::temp_directory_path().string();
        config.context_window.auto_compact = false;
        engine = std::make_unique<loom::core::QueryEngine>(std::move(config), tools);
        app = ftxui::Make<ui::AppAdapter>(engine.get(), nullptr, &commands, storage,
                                        [this] { exited = true; });
        ui::test_seams(app).start_program_status_for_testing([this](std::string_view bytes) {
            reports.emplace_back(bytes);
            return true;
        });
        ASSERT_EQ(reports.size(), 1U);
        app->OnEvent(ftxui::Event::Special("\x1b]7501;?\x1b\\"));
        ASSERT_EQ(reports.size(), 2U);
    }
    void TearDown() override {
        app.reset();
        engine.reset();
        std::error_code ignored;
        std::filesystem::remove_all(storage, ignored);
    }
    void Complete(bool error) {
        ui::test_seams(app).enqueue_program_status_completion_for_testing(error);
        app->OnEvent(ftxui::Event::Custom);
    }
    std::string Last() const { return reports.back(); }
    loom::core::ToolRegistry tools;
    loom::commands::AppCommandRegistry commands;
    std::unique_ptr<loom::core::QueryEngine> engine;
    std::shared_ptr<ui::AppAdapter> app;
    std::filesystem::path storage;
    std::vector<std::string> reports;
    bool exited = false;
};
}

TEST_F(AppProgramStatus, RenderMayConsumeCompletionButNeverEmits) {
    const auto count = reports.size();
    ui::test_seams(app).enqueue_program_status_completion_for_testing(false);
    (void)app->Render();
    EXPECT_EQ(reports.size(), count);
    app->OnEvent(ftxui::Event::Custom);
    ASSERT_EQ(reports.size(), count + 1);
    EXPECT_NE(Last().find("state=done"), std::string::npos);
    (void)app->Render();
    app->OnEvent(ftxui::Event::Custom);
    EXPECT_EQ(reports.size(), count + 1);
}

TEST_F(AppProgramStatus, ControlFocusAndMotionDoNotAcknowledgeButTypingDoes) {
    Complete(true);
    ASSERT_NE(Last().find("state=error"), std::string::npos);
    const auto count = reports.size();
    app->OnEvent(ftxui::Event::Special("\x1b]7501;?:states=idle\a"));
    app->OnEvent(ftxui::Event::Special("\x1b[I"));
    ftxui::Mouse motion;
    motion.button = ftxui::Mouse::None;
    app->OnEvent(ftxui::Event::Mouse("\x1b[<35;1;1M", motion));
    for (const auto button : {ftxui::Mouse::Left, ftxui::Mouse::Right}) {
        motion.button = button;
        motion.motion = ftxui::Mouse::Pressed;
        app->OnEvent(ftxui::Event::Mouse(button == ftxui::Mouse::Left ?
            "\x1b[<32;1;1M" : "\x1b[<34;1;1M", motion));
    }
    EXPECT_EQ(reports.size(), count);
    app->OnEvent(ftxui::Event::Character('x'));
    EXPECT_NE(Last().find("state=idle"), std::string::npos);
    EXPECT_EQ(ui::test_seams(app).input_text_for_testing(), "x");
    Complete(false);
    motion.button = ftxui::Mouse::Left;
    motion.motion = ftxui::Mouse::Pressed;
    app->OnEvent(ftxui::Event::Mouse("\x1b[<0;1;1M", motion));
    EXPECT_NE(Last().find("state=idle"), std::string::npos);
    Complete(true);
    motion.button = ftxui::Mouse::WheelUp;
    app->OnEvent(ftxui::Event::Mouse("\x1b[<64;1;1M", motion));
    EXPECT_NE(Last().find("state=idle"), std::string::npos);
}

TEST_F(AppProgramStatus, ExitTypingAcknowledgesButReturnPreservesNewOutcome) {
    Complete(false);
    for (const char ch : std::string_view("/exit")) app->OnEvent(ftxui::Event::Character(ch));
    EXPECT_NE(Last().find("state=idle"), std::string::npos);
    ASSERT_EQ(ui::test_seams(app).input_text_for_testing(), "/exit");
    Complete(true);
    const auto count = reports.size();
    app->OnEvent(ftxui::Event::Return);
    EXPECT_TRUE(exited);
    EXPECT_EQ(reports.size(), count);
    EXPECT_NE(Last().find("state=error"), std::string::npos);
}

TEST_F(AppProgramStatus, HiddenPermissionResolutionRestoresRetainedForegroundError) {
    Complete(true);
    app->HandleCommand("/settings");
    auto permission = app->get_permission_callback();
    std::promise<bool> answer;
    auto answered = answer.get_future();
    std::thread worker([&] {
        answer.set_value(permission("Write", R"({"file_path":"/tmp/status-only","content":"unused"})"));
    });
    struct StopBeforeJoin {
        std::shared_ptr<ui::AppAdapter>& app;
        std::thread& worker;
        ~StopBeforeJoin() {
            (void)ui::test_seams(app).stop_background_workers_for_testing();
            worker.join();
        }
    } cleanup{app, worker};
    std::string permission_id;
    ASSERT_TRUE(wait_until([&] {
        (void)app->Render();
        const auto state = ui::test_seams(app).screen_state_for_testing();
        const auto overlay = state->dialog_store.dialog_queue.peek_overlay();
        if (!overlay) return false;
        const auto* request = std::get_if<loom::ui::dialogs::system::ToolPermissionPayload>(&overlay->get());
        if (!request || request->tool_name != "Write" ||
            request->description.find("/tmp/status-only") == std::string::npos) return false;
        permission_id = request->id;
        return true;
    }, 2s));
    app->OnEvent(ftxui::Event::Custom);
    EXPECT_NE(Last().find("state=blocked"), std::string::npos);
    EXPECT_NE(Last().find("kind=permission"), std::string::npos);
    app->OnEvent(ftxui::Event::Character('y'));
    EXPECT_EQ(answered.wait_for(0ms), std::future_status::timeout);
    app->OnEvent(ftxui::Event::Escape);
    app->OnEvent(ftxui::Event::Character('y'));
    ASSERT_EQ(answered.wait_for(2s), std::future_status::ready);
    EXPECT_TRUE(answered.get());
    // The worker stages exact-request cleanup before returning. Consume that
    // UI action before asserting the foreground status restored by the event.
    app->OnEvent(ftxui::Event::Custom);
    const auto state = ui::test_seams(app).screen_state_for_testing();
    const auto overlay = state->dialog_store.dialog_queue.peek_overlay();
    EXPECT_TRUE(!overlay || std::visit([&](const auto& request) {
        if constexpr (requires { request.id; }) return request.id != permission_id;
        return true;
    }, overlay->get()));
    EXPECT_NE(Last().find("state=error"), std::string::npos);
}

TEST_F(AppProgramStatus, LocalShellExitStatusUsesExistingSuccessAndFailureSemantics) {
    for (const int code : {0, 7}) {
        app->HandleSubmit("exit " + std::to_string(code), repl::InputMode::Bash);
        ui::test_seams(app).wait_for_local_bash_for_testing();
        app->OnEvent(ftxui::Event::Custom);
        EXPECT_NE(Last().find(code == 0 ? "state=done" : "state=error"), std::string::npos);
    }
}
