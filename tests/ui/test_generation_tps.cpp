// RFC 0018: deterministic foreground-generation measurement and layout tests.
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <gtest/gtest.h>

#include "test_ui_helpers.h"

import std;
import loom.ui.app.app;
import loom.ui.screens.repl_state;
import loom.ui.screens.repl_screen;
import loom.ui.screens.task_view_store;
import loom.ui.foundation.clock;
import loom.ui.foundation.theme_provider;
import loom.query.query_engine;
import loom.tools.tool;
import loom.commands.registry;

namespace {
using namespace std::chrono_literals;
namespace ui = loom::ui;
namespace core = loom::core;
namespace repl = loom::ui::repl_screen;
namespace test_clock = loom::ui::clock;

struct EnvironmentGuard {
    std::string name;
    std::optional<std::string> previous;

    EnvironmentGuard(std::string key, const std::string& value)
        : name(std::move(key)) {
        if (const auto* current = std::getenv(name.c_str())) previous = current;
        ::setenv(name.c_str(), value.c_str(), 1);
    }

    ~EnvironmentGuard() {
        if (previous)
            ::setenv(name.c_str(), previous->c_str(), 1);
        else
            ::unsetenv(name.c_str());
    }
};

class GenerationTps : public ::testing::Test {
protected:
    void SetUp() override {
        storage = std::filesystem::temp_directory_path() /
                  ("loom-tps-" +
                   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        core::QueryEngineConfig config;
        config.cwd = std::filesystem::temp_directory_path().string();
        config.context_window.auto_compact = false;
        engine = std::make_unique<core::QueryEngine>(std::move(config), tools);
        app = ftxui::Make<ui::AppAdapter>(engine.get(), nullptr, &commands, storage, [] {});
        At(0ms);
        ui::test_seams(app).set_query_running_for_testing(true);
    }

    void TearDown() override {
        app.reset();
        engine.reset();
        test_clock::set_steady_now_for_testing(std::nullopt);
        std::error_code ignored;
        std::filesystem::remove_all(storage, ignored);
    }

    void At(std::chrono::milliseconds offset) {
        test_clock::set_steady_now_for_testing(origin + offset);
    }

    void Inject(core::StreamEvent event) {
        ui::test_seams(app).inject_stream_event_for_testing(event);
    }

    void Begin(std::uint32_t index = 0, core::ContentBlock block = core::TextBlock{""}) {
        Inject(core::ContentBlockStart{index, std::move(block)});
    }

    void Delta(std::string text, std::uint32_t index = 0) {
        Inject(core::ContentBlockDelta{index, std::move(text)});
    }

    void Drain() { (void)app->Render(); }

    std::optional<double> Rate() const {
        return ui::test_seams(app)
            .screen_state_for_testing()
            ->task_view_store.estimated_generation_tps;
    }

    void Live() {
        Begin();
        Delta("seed");
        Drain();
        At(200ms);
        Delta(std::string(40, 'x'));
        Drain();
        ASSERT_TRUE(Rate());
    }

    const std::chrono::steady_clock::time_point origin{100s};
    core::ToolRegistry tools;
    loom::commands::AppCommandRegistry commands;
    std::unique_ptr<core::QueryEngine> engine;
    std::shared_ptr<ui::AppAdapter> app;
    std::filesystem::path storage;
};

TEST_F(GenerationTps, WarmupMeasuresOnlyTimedAccumulatedGrowth) {
    EXPECT_FALSE(Rate());
    Begin();
    Delta("seed");
    Drain();
    EXPECT_FALSE(Rate());
    At(50ms);
    Delta(std::string(20, 'x'));
    Drain();
    EXPECT_FALSE(Rate());
    At(200ms);
    Delta(std::string(20, 'x'));
    Drain();
    ASSERT_TRUE(Rate());
    EXPECT_DOUBLE_EQ(*Rate(), 50.0);
}

TEST_F(GenerationTps, ChunkGroupingDoesNotCountFragmentsOrUtf8BytesAsTokens) {
    for (const bool fragmented : {false, true}) {
        for (const bool unicode : {false, true}) {
            At(0ms);
            Inject(core::StreamStart{core::MessageId{"round"}, "test"});
            Begin();
            Delta("seed");
            Drain();
            std::string output;
            if (unicode)
                for (int i = 0; i < 20; ++i)
                    output += "中";
            else
                output = std::string(40, 'x');
            At(200ms);
            if (fragmented)
                for (char ch : output)
                    Delta(std::string(1, ch));
            else
                Delta(output);
            Drain();
            ASSERT_TRUE(Rate());
            EXPECT_DOUBLE_EQ(*Rate(), unicode ? 100.0 : 50.0);
        }
    }
}

TEST_F(GenerationTps, WorkerChangesRateOnlyWhenUiDrains) {
    Begin();
    Delta("seed");
    Drain();
    At(200ms);
    std::jthread worker([&] { Delta(std::string(40, 'x')); });
    worker.join();
    EXPECT_FALSE(Rate());
    Drain();
    ASSERT_TRUE(Rate());
    EXPECT_DOUBLE_EQ(*Rate(), 50.0);
    std::jthread stop([&] { Inject(core::ContentBlockStop{0}); });
    stop.join();
    EXPECT_TRUE(Rate());
    Drain();
    EXPECT_FALSE(Rate());
}

TEST_F(GenerationTps, ThinkingAndToolArgumentsStopBeforeToolExecution) {
    for (const bool thinking : {true, false}) {
        At(0ms);
        Inject(core::StreamStart{core::MessageId{"round"}, "test"});
        if (thinking)
            Begin(0, core::ThinkingBlock{"", ""});
        else
            Begin(0, core::ToolUseBlock{core::ToolUseId{"tool-args"}, "Bash", "{}"});
        Delta("seed");
        Drain();
        At(200ms);
        Delta(std::string(40, 'x'));
        Drain();
        ASSERT_TRUE(Rate());
        EXPECT_DOUBLE_EQ(*Rate(), 50.0);
        Inject(core::ToolExecutionStart{thinking ? "thinking-tool" : "tool-args", "Bash", "{}"});
        Drain();
        EXPECT_FALSE(Rate());
        Inject(core::ToolExecutionProgress{"tool", std::string(1000, 'r')});
        Drain();
        EXPECT_FALSE(Rate());
    }
}

TEST_F(GenerationTps, StopsErrorsCompletionAndIdleClearRate) {
    for (int ending = 0; ending < 4; ++ending) {
        At(0ms);
        ui::test_seams(app).set_query_running_for_testing(false);
        ui::test_seams(app).set_query_running_for_testing(true);
        Inject(core::StreamStart{core::MessageId{"round"}, "test"});
        Live();
        if (ending == 0) Inject(core::ContentBlockStop{0});
        if (ending == 1) Inject(core::StreamError{"overloaded_error", "retry later"});
        if (ending == 2) Inject(core::StreamEnd{.stop_reason = "end_turn", .usage = {}});
        if (ending == 3) ui::test_seams(app).set_query_running_for_testing(false);
        Drain();
        EXPECT_FALSE(Rate()) << ending;
    }
}

TEST_F(GenerationTps, ToolAndStreamRoundsRestartWithoutReusingHistory) {
    Live();
    Inject(core::ContentBlockStop{99});
    Drain();
    EXPECT_TRUE(Rate());
    // A rejected replay for an unrelated stopped block must not clear rate.
    Begin(99);
    Drain();
    EXPECT_TRUE(Rate());
    Inject(core::ToolExecutionStart{"tool", "Bash", "{}"});
    Drain();
    EXPECT_FALSE(Rate());
    At(400ms);
    Inject(core::StreamStart{core::MessageId{"next"}, "test"});
    Begin();
    Delta("seed");
    Drain();
    EXPECT_FALSE(Rate());
    At(600ms);
    Delta(std::string(8, 'x'));
    Drain();
    ASSERT_TRUE(Rate());
    EXPECT_NEAR(*Rate(), 10.0, 1e-9);
}

TEST_F(GenerationTps, ExpiryAndLateDeltaNeverBridgeSilentInterval) {
    Live();
    At(2200ms);
    Drain();
    EXPECT_FALSE(Rate());
    At(2400ms);
    Delta("seed");
    Drain();
    EXPECT_FALSE(Rate());
    At(2600ms);
    Delta(std::string(8, 'x'));
    Drain();
    ASSERT_TRUE(Rate());
    EXPECT_DOUBLE_EQ(*Rate(), 10.0);
    // No UI drain at expiry: the worker must reset before replacing last_delta.
    At(5000ms);
    Delta(std::string(400, 'x'));
    Drain();
    EXPECT_FALSE(Rate());
    At(5200ms);
    Delta(std::string(8, 'x'));
    Drain();
    ASSERT_TRUE(Rate());
    EXPECT_DOUBLE_EQ(*Rate(), 10.0);
}

TEST_F(GenerationTps, CancellationSuppressesLateStartAndDeltasUntilNewQuery) {
    Live();
    (void)app->OnEvent(ftxui::Event::Special("\x03"));
    Drain();
    EXPECT_FALSE(Rate());
    Inject(core::StreamStart{core::MessageId{"late"}, "test"});
    Begin();
    Delta("late seed");
    Drain();
    At(400ms);
    Delta(std::string(80, 'x'));
    Drain();
    EXPECT_FALSE(Rate());
    ui::test_seams(app).set_query_running_for_testing(false);
    ui::test_seams(app).set_query_running_for_testing(true);
    At(500ms);
    Inject(core::StreamStart{core::MessageId{"new"}, "test"});
    Begin();
    Delta("seed");
    Drain();
    At(700ms);
    Delta(std::string(8, 'x'));
    Drain();
    ASSERT_TRUE(Rate());
    EXPECT_DOUBLE_EQ(*Rate(), 10.0);
}

TEST_F(GenerationTps, RollingWindowDropsOldBurstAndClockRollbackInvalidates) {
    Begin();
    Delta("seed");
    Drain();
    for (int i = 1; i <= 22; ++i) {
        At(i * 100ms);
        Delta(std::string(i == 1 ? 400 : 4, 'x'));
        Drain();
    }
    ASSERT_TRUE(Rate());
    EXPECT_NEAR(*Rate(), 10.0, 1e-9);
    At(2100ms);
    Drain();
    EXPECT_FALSE(Rate());
    Delta("seed");
    Drain();
    At(2300ms);
    Delta(std::string(8, 'x'));
    Drain();
    ASSERT_TRUE(Rate());
    EXPECT_DOUBLE_EQ(*Rate(), 10.0);
}

TEST_F(GenerationTps, EmptyDeltasAndZeroGrowthStayHidden) {
    Begin();
    Delta("");
    Drain();
    EXPECT_FALSE(Rate());
    Delta("a");
    Drain();
    At(200ms);
    Delta("b");
    Drain();
    EXPECT_FALSE(Rate());
}

TEST(GenerationTpsLayout, RateDoesNotMoveTasksTeamsOrEditor) {
    for (int width : {40, 80, 120})
        for (int height : {24, 40}) {
            for (const bool multiline : {false, true}) {
                EnvironmentGuard columns("COLUMNS", std::to_string(width));
                EnvironmentGuard rows("LINES", std::to_string(height));
                repl::ReplScreenState state;
                state.cwd = "/tmp/tps-layout";
                state.query_running = true;
                state.task_view_store.spinner_mode = repl::SpinnerMode::Responding;
                state.task_view_store.spinner_verb = "Responding";
                state.task_view_store.live_teammates.push_back({.agent_id = "worker",
                                                                .name = "Worker",
                                                                .color = "cyan",
                                                                .status = "running",
                                                                .last_output_tail = "team progress",
                                                                .pane_id = ""});
                state.todo_store.items.push_back({.content = "task progress"});
                state.input_text = multiline ? "draft\nsecond\nthird" : "draft";
                const auto capture = [&] {
                    return strip_ansi(
                        render_to_plain_text(repl::RenderReplScreen(state), width, height));
                };
                const auto before = capture();
                state.task_view_store.estimated_generation_tps = 42.1;
                const auto after = capture();
                EXPECT_NE(after.find("~42.1 tok/s"), std::string::npos);
                const auto lines = [](const std::string& text) {
                    std::vector<std::string> result;
                    std::istringstream input(text);
                    for (std::string line; std::getline(input, line);)
                        result.push_back(line);
                    return result;
                };
                const auto a = lines(before), b = lines(after);
                ASSERT_EQ(a.size(), b.size());
                int changed = 0;
                for (std::size_t i = 0; i < a.size(); ++i) {
                    if (a[i] == b[i]) continue;
                    ++changed;
                    EXPECT_NE(b[i].find("tok/s"), std::string::npos);
                }
                EXPECT_EQ(changed, 1) << width << "x" << height;
                EXPECT_NE(after.find("Tasks"), std::string::npos);
                EXPECT_NE(after.find("draft"), std::string::npos);
            }
        }
}

TEST(GenerationTpsLayout, IndicatorUsesMutedThemeTokenAndRejectsInvalidRate) {
    namespace theme = ui::design::theme;

    struct RestoreTheme {
        theme::Theme previous = theme::current_theme();

        ~RestoreTheme() { theme::set_theme(previous); }
    } restore;

    for (const auto variant :
         {theme::ThemeVariant::Dark, theme::ThemeVariant::Light,
          theme::ThemeVariant::DarkDaltonized, theme::ThemeVariant::LightDaltonized,
          theme::ThemeVariant::Monochrome, theme::ThemeVariant::DarkAnsi,
          theme::ThemeVariant::LightAnsi}) {
        theme::set_theme(variant);
        auto screen =
            ftxui::Screen::Create(ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(1));
        ftxui::Render(screen, repl::RenderSpinner(repl::SpinnerMode::Responding, "Responding",
                                                  std::nullopt, 0, 42.1));
        const auto text = strip_ansi(screen.ToString());
        const auto position = text.find('~');
        ASSERT_NE(position, std::string::npos);
        EXPECT_EQ(screen.PixelAt(ftxui::string_width(text.substr(0, position)), 0).foreground_color,
                  theme::current_theme().palette->muted);
        for (const double invalid : {-1.0, 0.0, std::numeric_limits<double>::infinity(),
                                     std::numeric_limits<double>::quiet_NaN()}) {
            const auto rendered = strip_ansi(render_to_plain_text(repl::RenderSpinner(
                repl::SpinnerMode::Responding, "Responding", std::nullopt, 0, invalid)));
            EXPECT_EQ(rendered.find("tok/s"), std::string::npos);
        }
    }
}
}  // namespace
