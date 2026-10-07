/// @file test_ui_runtime.cpp
/// @brief Split from test_ui.cpp - AppRuntime, E2E_Gate, FullscreenLayout, LogoV2, PromptInput, ReplScreen (SLOC budget fix)

#include <cstdlib>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <gtest/gtest.h>
#include <httplib.h>

#include "test_ui_helpers.h"

import std;
import loom.ui.app.app;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.messages.message_image;
import loom.commands.registry;
import loom.query.query_engine;
import loom.tools.tool;
import loom.session.app_storage;
import loom.teams.team_helpers;
import loom.teams.swarm.helpers;
import loom.constants.constants;
import loom.ui.prompt.autocomplete_sources;
import loom.ui.features.teams.live_teammates;

namespace {
namespace fs = std::filesystem;
}


// ═══════════════════════════════════════════════════════════════════════════════
// loom.ui.chrome.terminal: FTXUI terminal controller and common widgets
// ═══════════════════════════════════════════════════════════════════════════════






TEST(AppRuntime, FreshWelcomeAnimationTicksWithoutInputEvents) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_welcome_animation_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    const auto initial_ticks = test_seams(app).ui_animation_tick_count_for_testing();
    EXPECT_TRUE(wait_until([&] {
        return test_seams(app).ui_animation_tick_count_for_testing() > initial_ticks;
    }, std::chrono::milliseconds(300)));
    EXPECT_FALSE(test_seams(app).is_query_running_for_testing());

    fs::remove_all(storage_root);
}



TEST(AppRuntime, FreshWelcomeAnimationKeepsTickingAfterStartupWindow) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_welcome_animation_long_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    std::this_thread::sleep_for(std::chrono::milliseconds(3200));
    const auto ticks_after_startup_window =
        test_seams(app).ui_animation_tick_count_for_testing();
    EXPECT_TRUE(wait_until([&] {
        return test_seams(app).ui_animation_tick_count_for_testing() >
               ticks_after_startup_window;
    }, std::chrono::milliseconds(300)));
    EXPECT_FALSE(test_seams(app).is_query_running_for_testing());

    fs::remove_all(storage_root);
}



TEST(AppRuntime, CommandsAndStatusRenderWithoutTerminalLoop) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.model_params.model = "test-model";
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_app_runtime_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    bool exited = false;
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [&] {
            exited = true;
        });

    // --- Initial render: prompt input with the status bar ---
    app->SyncState();
    auto initial = render_to_plain_text(app->Render(), 120, 28);
    EXPECT_EQ(test_seams(app).status_bar_model_for_testing(), "test-model");
    EXPECT_NE(initial.find("test-model"), std::string::npos);
    EXPECT_EQ(initial.find("You are Loom"), std::string::npos);
    EXPECT_NE(initial.find("❯"), std::string::npos);

    // --- /model haiku-runtime: changes model state ---
    app->HandleCommand("/model haiku-runtime");
    EXPECT_EQ(engine.model_params().model, "haiku-runtime");
    EXPECT_EQ(test_seams(app).status_bar_model_for_testing(), "haiku-runtime");

    // --- /cost: sets status tip (visible via testing accessor) ---
    app->HandleCommand("/cost");
    auto status_msg = test_seams(app).status_message_for_testing();
    EXPECT_NE(status_msg.find("Cost: $"), std::string::npos);
    EXPECT_NE(status_msg.find("In:"), std::string::npos);
    EXPECT_NE(status_msg.find("Out:"), std::string::npos);
    EXPECT_NE(status_msg.find("Ctx:"), std::string::npos);

    // --- /clear: clears conversation, status bar retains current model ---
    app->HandleCommand("/clear");
    EXPECT_EQ(test_seams(app).status_bar_model_for_testing(), "haiku-runtime");

    // --- /exit: triggers on_exit callback ---
    app->HandleCommand("/exit");
    EXPECT_TRUE(exited);

    fs::remove_all(storage_root);
}



TEST(AppRuntime, SlashInputShowsRegistrySuggestions) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_slash_suggestions_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("/")));
    EXPECT_GT(test_seams(app).autocomplete_suggestion_count_for_testing(), 0u);
    ASSERT_GT(test_seams(app).autocomplete_suggestion_count_for_testing(), 1u);
    EXPECT_EQ(test_seams(app).autocomplete_index_for_testing(), 0);

    EXPECT_TRUE(app->OnEvent(ftxui::Event::ArrowDown));
    EXPECT_EQ(test_seams(app).autocomplete_index_for_testing(), 1);

    EXPECT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).autocomplete_index_for_testing(), 0);

    auto slash_rendered = strip_ansi(render_to_plain_text(app->Render(), 120, 32));
    EXPECT_NE(slash_rendered.find("❯ /"), std::string::npos);
    EXPECT_EQ(slash_rendered.find("/ /"), std::string::npos);

    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("h")));
    const auto suggestions = test_seams(app).autocomplete_suggestions_for_testing();
    // SL-07: canonical row shows a matched-alias parenthetical (e.g. "/help (h)"
    // when the user typed the alias "h"), so match by substring, not exact element.
    const bool has_help = std::any_of(suggestions.begin(), suggestions.end(),
        [](const std::string& s) { return s.find("/help") != std::string::npos; });
    EXPECT_TRUE(has_help);

    auto help_rendered = strip_ansi(render_to_plain_text(app->Render(), 120, 32));
    EXPECT_NE(help_rendered.find("/help"), std::string::npos);

    fs::remove_all(storage_root);
}



TEST(AppRuntime, SkillsDialogDismissOrderDebug) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_skills_order_dbg_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('/')));
    for (char c : std::string("skills")) {
        EXPECT_TRUE(app->OnEvent(ftxui::Event::Character(c)));
    }
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Return));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Escape));

    const auto msgs = test_seams(app).messages_for_testing();
    EXPECT_FALSE(msgs.empty());
    if (!msgs.empty()) {
        EXPECT_EQ(msgs[0].substr(0, std::string("lc-input").size()), "lc-input")
            << "expected /skills echo first, got: " << msgs[0];
    }

    // Submit a text message ("hello") AFTER /skills dismiss — the local-command
    // rows must stay ABOVE the user text row (chronological order).
    for (char c : std::string("hello")) {
        EXPECT_TRUE(app->OnEvent(ftxui::Event::Character(c)));
    }
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Return));
    for (int i = 0; i < 100 && test_seams(app).is_query_running_for_testing(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    // Trigger a render so Render()/SyncState projects the engine conversation
    // (the hello row lives in engine_->get_conversation(), only reaches
    // screen_state_->messages after a Render pass).
    (void)strip_ansi(render_to_plain_text(app->Render(), 120, 32));
    const auto msgs2 = test_seams(app).messages_for_testing();
    // Find positions of lc-input and the user text row.
    int lc_input_pos = -1, user_pos = -1;
    for (int i = 0; i < static_cast<int>(msgs2.size()); ++i) {
        if (msgs2[i].substr(0, std::string("lc-input").size()) == "lc-input" && lc_input_pos < 0)
            lc_input_pos = i;
        if (msgs2[i].substr(0, std::string("user").size()) == "user" && user_pos < 0)
            user_pos = i;
    }
    EXPECT_GE(lc_input_pos, 0);
    EXPECT_GE(user_pos, 0);
    EXPECT_LT(lc_input_pos, user_pos)
        << "local-command /skills row must render ABOVE the later user text row";

    fs::remove_all(storage_root);
}



TEST(AppRuntime, CommandResultMessagesRenderInTranscript) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_command_result_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    app->HandleCommand("/help");
    auto rendered = strip_ansi(render_to_plain_text(app->Render(), 160, 60));
    EXPECT_NE(rendered.find("Available commands"), std::string::npos);

    app->HandleCommand("/theme list");
    rendered = strip_ansi(render_to_plain_text(app->Render(), 160, 80));
    EXPECT_NE(rendered.find("Available themes"), std::string::npos);

    app->HandleCommand("/clear");
    rendered = strip_ansi(render_to_plain_text(app->Render(), 160, 40));
    EXPECT_EQ(rendered.find("Available commands"), std::string::npos);

    fs::remove_all(storage_root);
}



// TS REF: src/utils/processUserInput/processBashCommand.tsx — a `!`-prefixed
// command runs LOCALLY (BashTool.call, shouldQuery:false) and renders
// <bash-input>/<bash-stdout> local-command rows.  It must NOT be sent to the
// LLM (no Bash tool-use card, no assistant summary).  This is the fix for the
// reported bug where `!ls -la` rendered as an LLM Bash tool call.
TEST(AppRuntime, BangCommandRunsLocallyNotThroughLLM) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_bang_local_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // Type "!" (enters bash mode) then the command, then Enter.
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("!")));
    for (char c : std::string("echo cpp_port_marker")) {
        EXPECT_TRUE(app->OnEvent(ftxui::Event::Character(std::string(1, c))));
    }
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Return));

    // No LLM query must be started for a local bash command.
    EXPECT_FALSE(test_seams(app).is_query_running_for_testing());

    // Wait for the local bash worker and drain its output row.
    test_seams(app).wait_for_local_bash_for_testing();
    (void)strip_ansi(render_to_plain_text(app->Render(), 120, 32));

    const auto msgs = test_seams(app).messages_for_testing();
    int lc_input_pos = -1, lc_output_pos = -1;
    bool saw_assistant_or_tool = false;
    std::string output_row;
    for (int i = 0; i < static_cast<int>(msgs.size()); ++i) {
        if (msgs[i].rfind("lc-input", 0) == 0 && lc_input_pos < 0) lc_input_pos = i;
        if (msgs[i].rfind("lc-output", 0) == 0) {
            if (lc_output_pos < 0) lc_output_pos = i;
            output_row = msgs[i];
        }
        // A real LLM turn would project "assistant:" / tool-use rows.
        if (msgs[i].rfind("assistant", 0) == 0) saw_assistant_or_tool = true;
    }

    ASSERT_GE(lc_input_pos, 0) << "expected an lc-input row for the '!' command";
    ASSERT_GE(lc_output_pos, 0) << "expected an lc-output row with command output";
    EXPECT_LT(lc_input_pos, lc_output_pos) << "input row must precede output row";
    EXPECT_FALSE(saw_assistant_or_tool)
        << "a local '!' command must not produce an assistant/LLM turn";
    // The echoed marker should appear in the output row (preview is truncated
    // to 30 chars, but the marker fits).
    EXPECT_NE(output_row.find("cpp_port_marker"), std::string::npos)
        << "output row: " << output_row;

    fs::remove_all(storage_root);
}



TEST(AppRuntime, SkillsCommandRendersInlineOutputAndRejectsListSubcommand) {
    const auto cwd_root = fs::temp_directory_path() /
        ("loom_ui_skills_menu_cwd_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto home_root = fs::temp_directory_path() /
        ("loom_ui_skills_menu_home_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(home_root);
    ScopedEnvVar home_guard("HOME");
    ScopedEnvVar skills_path_guard("LOOM_SKILLS_PATH");
    home_guard.set(home_root.string());
    const auto skills_dir = cwd_root / ".loom" / "skills" / "cpp-review";
    fs::create_directories(skills_dir);
    {
        std::ofstream out(skills_dir / "SKILL.md");
        out << "---\n"
            << "name: cpp-review\n"
            << "description: Review migrated C++ UI code.\n"
            << "version: 1.0.0\n"
            << "---\n"
            << "Review C++ UI migration changes.\n"
            << std::string(4000, 'x') << "\n";
    }

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = cwd_root.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_skills_menu_storage_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    app->HandleCommand("/skills");
    auto rendered = strip_ansi(render_to_plain_text(app->Render(), 140, 40));
    EXPECT_NE(rendered.find("❯ /skills"), std::string::npos);
    EXPECT_NE(rendered.find("Skills"), std::string::npos);
    EXPECT_NE(rendered.find("1 skill"), std::string::npos);
    EXPECT_NE(rendered.find("Project skills"), std::string::npos);
    EXPECT_NE(
        rendered.find("cpp-review · ~10 description tokens"),
        std::string::npos);
    EXPECT_NE(rendered.find("Esc to close"), std::string::npos);
    EXPECT_EQ(rendered.find("Built-in skills"), std::string::npos);
    EXPECT_EQ(rendered.find("batch ·"), std::string::npos);
    EXPECT_EQ(rendered.find("Try \"write a test\""), std::string::npos);
    EXPECT_EQ(rendered.find("Skills dialog dismissed"), std::string::npos);
    EXPECT_EQ(rendered.find("⎿"), std::string::npos);

    EXPECT_TRUE(app->OnEvent(ftxui::Event::Escape));
    rendered = strip_ansi(render_to_plain_text(app->Render(), 140, 40));
    EXPECT_NE(rendered.find("❯ /skills"), std::string::npos);
    EXPECT_NE(rendered.find("⎿"), std::string::npos);
    EXPECT_NE(rendered.find("Skills dialog dismissed"), std::string::npos);

    app->HandleCommand("/skills list");
    rendered = strip_ansi(render_to_plain_text(app->Render(), 160, 80));
    EXPECT_NE(rendered.find("Usage: /skills"), std::string::npos);
    EXPECT_EQ(rendered.find("Installed skills"), std::string::npos);

    fs::remove_all(storage_root);
    fs::remove_all(home_root);
    fs::remove_all(cwd_root);
}



TEST(AppRuntime, SkillsCommandInlineOutputScrollsWithTranscript) {
    const auto cwd_root = fs::temp_directory_path() /
        ("loom_ui_skills_scroll_cwd_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto home_root = fs::temp_directory_path() /
        ("loom_ui_skills_scroll_home_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(home_root);
    ScopedEnvVar home_guard("HOME");
    ScopedEnvVar skills_path_guard("LOOM_SKILLS_PATH");
    home_guard.set(home_root.string());

    for (int i = 0; i < 36; ++i) {
        const auto skills_dir = cwd_root / ".loom" / "skills" /
            ("scroll-skill-" + std::to_string(i));
        fs::create_directories(skills_dir);
        std::ofstream out(skills_dir / "SKILL.md");
        out << "---\n"
            << "name: scroll-skill-" << i << "\n"
            << "description: Scroll regression fixture " << i << ".\n"
            << "---\n"
            << "Use this skill for scroll regression fixture " << i << ".\n";
    }

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = cwd_root.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_skills_scroll_storage_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    app->HandleCommand("/skills");
    auto rendered = strip_ansi(render_to_plain_text(app->Render(), 120, 14));
    EXPECT_NE(rendered.find("scroll-skill-0"), std::string::npos);
    EXPECT_EQ(rendered.find("scroll-skill-35"), std::string::npos);

    ftxui::Mouse wheel;
    wheel.button = ftxui::Mouse::WheelDown;
    for (int i = 0; i < 6; ++i) {
        EXPECT_TRUE(app->OnEvent(ftxui::Event::Mouse("", wheel)));
    }

    rendered = strip_ansi(render_to_plain_text(app->Render(), 120, 14));
    EXPECT_EQ(rendered.find("scroll-skill-0"), std::string::npos) << rendered;
    EXPECT_NE(rendered.find("scroll-skill-"), std::string::npos) << rendered;

    fs::remove_all(storage_root);
    fs::remove_all(home_root);
    fs::remove_all(cwd_root);
}



TEST(AppRuntime, ReturnSubmitsAgentSlashSubcommandsWhenCompletionIsVisible) {
    const auto cwd_root = fs::temp_directory_path() /
        ("loom_ui_slash_subcommand_return_cwd_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto home_root = fs::temp_directory_path() /
        ("loom_ui_slash_subcommand_return_home_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(home_root);
    ScopedEnvVar home_guard("HOME");
    ScopedEnvVar skills_path_guard("LOOM_SKILLS_PATH");
    home_guard.set(home_root.string());
    const auto skills_dir = cwd_root / ".loom" / "skills" / "cpp-review";
    fs::create_directories(skills_dir);
    {
        std::ofstream out(skills_dir / "SKILL.md");
        out << "---\n"
            << "name: cpp-review\n"
            << "description: Review migrated C++ UI code.\n"
            << "---\n"
            << "Review C++ UI migration changes.\n";
    }

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = cwd_root.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_slash_subcommand_return_storage_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    ASSERT_TRUE(app->OnEvent(ftxui::Event::Character("/agents list")));
    ASSERT_GT(test_seams(app).autocomplete_suggestion_count_for_testing(), 0u);
    ASSERT_TRUE(app->OnEvent(ftxui::Event::Return));
    EXPECT_TRUE(test_seams(app).input_text_for_testing().empty());

    auto rendered = strip_ansi(render_to_plain_text(app->Render(), 180, 110));
    EXPECT_TRUE(rendered.find("Agents") != std::string::npos ||
                rendered.find("Create new agent") != std::string::npos);
    EXPECT_FALSE(same_rendered_line_contains(
        rendered, "Available agents", "loom-guide"));

    fs::remove_all(storage_root);
    fs::remove_all(home_root);
    fs::remove_all(cwd_root);
}



TEST(AppRuntime, DynamicPromptSuggestionsCoverSkillsFilesAndCursorEditing) {
    const auto cwd_root = fs::temp_directory_path() /
        ("loom_ui_dynamic_suggestions_cwd_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto skills_dir = cwd_root / ".loom" / "skills" / "cpp-review";
    fs::create_directories(skills_dir);
    {
        std::ofstream out(skills_dir / "SKILL.md");
        out << "---\n"
            << "name: cpp-review\n"
            << "description: Review migrated C++ UI code.\n"
            << "---\n"
            << "Review C++ UI migration changes.\n";
    }
    {
        std::ofstream out(cwd_root / "src_file.cpp");
        out << "int main() { return 0; }\n";
    }

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = cwd_root.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_dynamic_suggestions_storage_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("/")));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("c")));
    const auto slash_suggestions = test_seams(app).autocomplete_suggestions_for_testing();
    EXPECT_NE(
        std::find(slash_suggestions.begin(), slash_suggestions.end(), "/cpp-review"),
        slash_suggestions.end());

    // TS PromptInput.tsx passes disableEscapeDoublePress only while
    // suggestions are open, so clearing through an open popup takes three
    // presses: Esc (dismiss popup) -> Esc (arm) -> Esc (clear).
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Escape));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Escape));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Escape));
    EXPECT_TRUE(test_seams(app).input_text_for_testing().empty());
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("@")));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("s")));
    const auto at_suggestions = test_seams(app).autocomplete_suggestions_for_testing();
    EXPECT_TRUE(std::any_of(at_suggestions.begin(), at_suggestions.end(), [](const auto& suggestion) {
        return suggestion.find("src_file.cpp") != std::string::npos;
    }));

    // Unguarded dismiss/arm/clear loop: at most three Escapes total, sending
    // the second and third only while input is still non-empty (popup state
    // varies with the "@s" prefix).
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Escape));
    for (int extra_esc = 0;
         extra_esc < 2 && !test_seams(app).input_text_for_testing().empty();
         ++extra_esc) {
        EXPECT_TRUE(app->OnEvent(ftxui::Event::Escape));
    }
    EXPECT_TRUE(test_seams(app).input_text_for_testing().empty());
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("a")));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("b")));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::ArrowLeft));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("你")));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "a你b");
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Backspace));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "ab");

    fs::remove_all(storage_root);
    fs::remove_all(cwd_root);
}



TEST(AppRuntime, ReturnOnSelectedSlashSuggestionOpensAgentsLocalJsx) {
    const auto home_root = fs::temp_directory_path() /
        ("loom_ui_slash_agents_home_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(home_root / ".loom");
    ScopedEnvVar home_guard("HOME");
    home_guard.set(home_root.string());

    const auto cwd_root = fs::temp_directory_path() /
        ("loom_ui_slash_agents_cwd_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(cwd_root);

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = cwd_root.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_slash_agents_accept_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("/")));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("a")));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("g")));
    ASSERT_GT(test_seams(app).autocomplete_suggestion_count_for_testing(), 0u);
    const auto suggestions = test_seams(app).autocomplete_suggestions_for_testing();
    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ(suggestions.front(), "/agents");

    EXPECT_TRUE(app->OnEvent(ftxui::Event::Return));
    EXPECT_TRUE(test_seams(app).is_agents_view_for_testing());
    EXPECT_FALSE(test_seams(app).is_local_jsx_command_for_testing("agents"));
    EXPECT_TRUE(test_seams(app).input_text_for_testing().empty());
    EXPECT_GT(test_seams(app).agent_card_count_for_testing(), 0u);

    auto rendered = strip_ansi(render_to_plain_text(app->Render(), 140, 40));
    EXPECT_NE(rendered.find("Agents"), std::string::npos);
    EXPECT_NE(rendered.find("Create new agent"), std::string::npos);
    EXPECT_NE(rendered.find("Built-in agents"), std::string::npos);
    EXPECT_NE(rendered.find("Press ↑↓ to navigate"), std::string::npos);
    EXPECT_EQ(rendered.find("Esc to close"), std::string::npos);
    EXPECT_EQ(rendered.find("╭"), std::string::npos);
    EXPECT_EQ(rendered.find("╰"), std::string::npos);
    const auto create_pos = rendered.find("› Create new agent");
    ASSERT_NE(create_pos, std::string::npos);
    const auto create_line_start = rendered.rfind('\n', create_pos);
    const auto create_col =
        create_pos - (create_line_start == std::string::npos ? 0 : create_line_start + 1);
    EXPECT_LT(create_col, 10u);
    EXPECT_EQ(rendered.find("Recent activity"), std::string::npos);
    EXPECT_EQ(rendered.find("Try \"write a test\""), std::string::npos);
    EXPECT_EQ(rendered.find("Grid"), std::string::npos);

    fs::remove_all(storage_root);
    fs::remove_all(home_root);
    fs::remove_all(cwd_root);
}



TEST(AppRuntime, AgentsLocalJsxArrowKeysSelectProjectAgentAndReturnActs) {
    const auto home_root = fs::temp_directory_path() /
        ("loom_ui_agents_nav_home_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(home_root / ".loom");
    ScopedEnvVar home_guard("HOME");
    home_guard.set(home_root.string());

    const auto cwd_root = fs::temp_directory_path() /
        ("loom_ui_agents_nav_cwd_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto agents_dir = cwd_root / ".loom" / "agents";
    fs::create_directories(agents_dir);
    {
        std::ofstream out(agents_dir / "cpp-reviewer.md");
        out << "---\n"
            << "name: cpp-reviewer\n"
            << "description: Reviews migrated C++ UI code.\n"
            << "model: inherit\n"
            << "---\n"
            << "Review C++ UI migration changes.\n";
    }

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = cwd_root.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_agents_nav_storage_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("/")));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("a")));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character("g")));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Return));
    ASSERT_TRUE(test_seams(app).is_agents_view_for_testing());

    auto initial = strip_ansi(render_to_plain_text(app->Render(), 120, 36));
    EXPECT_NE(initial.find("› Create new agent"), std::string::npos);
    EXPECT_NE(initial.find("Project agents"), std::string::npos);
    EXPECT_NE(initial.find("cpp-reviewer"), std::string::npos);

    EXPECT_TRUE(app->OnEvent(ftxui::Event::ArrowDown));
    auto selected = strip_ansi(render_to_plain_text(app->Render(), 120, 36));
    EXPECT_EQ(selected.find("› Create new agent"), std::string::npos);
    EXPECT_NE(selected.find("› cpp-reviewer"), std::string::npos);

    EXPECT_TRUE(app->OnEvent(ftxui::Event::Return));
    EXPECT_FALSE(test_seams(app).is_local_jsx_command_for_testing("agents"));
    EXPECT_FALSE(test_seams(app).is_agents_view_for_testing());

    fs::remove_all(storage_root);
    fs::remove_all(home_root);
    fs::remove_all(cwd_root);
}



TEST(AppRuntime, StatusLineRuntimeSettingsOverrideDiskSettings) {
    const auto home_root = fs::temp_directory_path() /
        ("loom_ui_statusline_home_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(home_root / ".loom");

    ScopedEnvVar home_guard("HOME");
    ScopedEnvVar command_guard("LOOM_STATUS_LINE_COMMAND");
    ScopedEnvVar command_compat_guard("LOOM_STATUS_LINE_COMMAND");
    ScopedEnvVar enabled_guard("LOOM_STATUS_LINE_ENABLED");
    ScopedEnvVar enabled_compat_guard("LOOM_STATUS_LINE_ENABLED");
    ScopedEnvVar padding_guard("LOOM_STATUS_LINE_PADDING");
    ScopedEnvVar padding_compat_guard("LOOM_STATUS_LINE_PADDING");

    home_guard.set(home_root.string());
    command_guard.set(":");
    enabled_guard.set("1");
    padding_guard.set("2");

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_statusline_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    EXPECT_TRUE(test_seams(app).status_line_enabled_for_testing());
    EXPECT_EQ(test_seams(app).status_line_command_for_testing(), ":");
    EXPECT_EQ(test_seams(app).status_line_padding_for_testing(), 2);

    fs::remove_all(storage_root);
    fs::remove_all(home_root);
}



TEST(AppRuntime, CtrlCIdleRequiresDoublePressWithinWindow) {
    // TS REF: src/hooks/useTextInput.ts:108-120 handleCtrlC =
    // useDoublePress(..., onExit, onFirstPress) with
    // DOUBLE_PRESS_TIMEOUT_MS = 800 (useDoublePress.ts:6). A single idle
    // Ctrl+C clears non-empty input and shows "Press Ctrl-C again to exit";
    // only the second press inside 800ms exits.

    auto make_app = [](std::function<void()> on_exit) {
        loom::core::ToolRegistry tools;
        loom::core::QueryEngineConfig config;
        config.context_window.auto_compact = false;
        config.cwd = fs::temp_directory_path().string();
        auto engine =
            std::make_unique<loom::core::QueryEngine>(std::move(config), tools);
        auto commands =
            std::make_unique<loom::commands::AppCommandRegistry>();
        const auto storage_root = fs::temp_directory_path() /
            ("loom_ui_interrupt_test_" +
             std::to_string(std::chrono::steady_clock::now()
                                .time_since_epoch().count()));
        auto storage =
            std::make_unique<loom::utils::SessionStorage>(storage_root);
        loom::core::QueryEngine* engine_ptr = engine.get();
        loom::commands::AppCommandRegistry* commands_ptr = commands.get();
        loom::utils::SessionStorage* storage_ptr = storage.get();
        auto app = ftxui::Make<loom::ui::AppAdapter>(
            engine_ptr, nullptr, commands_ptr, storage_ptr,
            std::move(on_exit));
        return std::tuple(std::move(app), std::move(engine),
                          std::move(commands), std::move(storage),
                          storage_root);
    };

    // ── Double press inside the window exits ──────────────────────────
    {
        bool exited = false;
        auto [app, engine, commands, storage, storage_root] =
            make_app([&] { exited = true; });

        app->OnEvent(ftxui::Event::Character("typed text"));
        ASSERT_EQ(test_seams(app).input_text_for_testing(), "typed text");

        EXPECT_TRUE(app->OnEvent(ftxui::Event::Special("\x03")));
        EXPECT_FALSE(exited)
            << "first idle Ctrl+C must request confirmation, not exit";
        // TS onFirstPress clears non-empty input immediately.
        EXPECT_TRUE(test_seams(app).input_text_for_testing().empty());
        const auto armed = strip_ansi(render_to_plain_text(
            app->Render(), 120, 32));
        EXPECT_NE(armed.find("Press Ctrl-C again to exit"),
                  std::string::npos);

        EXPECT_TRUE(app->OnEvent(ftxui::Event::Special("\x03")));
        EXPECT_TRUE(exited)
            << "second Ctrl+C inside the 800ms window must exit";

        fs::remove_all(storage_root);
    }

    // ── Expired first press re-arms instead of exiting ────────────────
    {
        bool exited = false;
        auto [app, engine, commands, storage, storage_root] =
            make_app([&] { exited = true; });

        EXPECT_TRUE(app->OnEvent(ftxui::Event::Special("\x03")));
        EXPECT_FALSE(exited);

        std::this_thread::sleep_for(std::chrono::milliseconds(850));
        EXPECT_TRUE(app->OnEvent(ftxui::Event::Special("\x03")));
        EXPECT_FALSE(exited)
            << "a Ctrl+C after the 800ms window must re-arm, not exit";
        const auto rearmed = strip_ansi(render_to_plain_text(
            app->Render(), 120, 32));
        EXPECT_NE(rearmed.find("Press Ctrl-C again to exit"),
                  std::string::npos);

        fs::remove_all(storage_root);
    }
}



TEST(AppRuntime, StreamFallbackErrorIsRendered) {
    LocalErrorMessagesStreamServer server;
    ASSERT_TRUE(server.valid());

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_stream_error_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});

    app->HandleSubmit("你好");
    ASSERT_TRUE(server.wait_for_requests(2));
    ASSERT_TRUE(wait_until([&] {
        (void)app->Render();
        return !test_seams(app).is_query_running_for_testing();
    }, std::chrono::seconds(2)));

    auto rendered = strip_ansi(render_to_plain_text(app->Render(), 140, 36));
    EXPECT_NE(rendered.find("Error:"), std::string::npos);
    EXPECT_NE(rendered.find("API error (400)"), std::string::npos);
    EXPECT_NE(rendered.find("bad model"), std::string::npos);

    fs::remove_all(storage_root);
}



TEST(AppRuntime, CtrlCWhileStreamingQueryCancelsWithoutExiting) {
    LocalChunkedMessagesStreamServer server;
    ASSERT_TRUE(server.valid());

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_stream_cancel_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    bool exited = false;
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [&] {
            exited = true;
        });
    ReleaseAfterCancelGuard release_guard{server};

    app->HandleSubmit("show streaming cancel behavior");
    ASSERT_TRUE(server.wait_for_first_delta());
    ASSERT_TRUE(wait_until([&] {
        auto rendered = render_to_plain_text(app->Render(), 120, 32);
        return rendered.find("partial UI stream") != std::string::npos;
    }, std::chrono::seconds(2)));
    EXPECT_TRUE(test_seams(app).is_loading_for_testing());
    EXPECT_TRUE(test_seams(app).is_query_running_for_testing());

    EXPECT_TRUE(app->OnEvent(ftxui::Event::Special("\x03")));
    EXPECT_FALSE(exited);
    EXPECT_TRUE(test_seams(app).is_loading_for_testing());
    EXPECT_TRUE(test_seams(app).is_query_running_for_testing());
    EXPECT_EQ(test_seams(app).status_message_for_testing(), "Cancelling...");

    auto cancelling = render_to_plain_text(app->Render(), 120, 32);
    EXPECT_NE(cancelling.find("Cancelling..."), std::string::npos);
    EXPECT_NE(cancelling.find("partial UI stream"), std::string::npos);

    server.release_after_cancel();
    EXPECT_TRUE(wait_until([&] {
        (void)app->Render();
        return !test_seams(app).is_query_running_for_testing();
    }, std::chrono::seconds(3)));
    (void)app->Render();
    EXPECT_FALSE(test_seams(app).is_loading_for_testing());

    fs::remove_all(storage_root);
}



// ESC must interrupt a running streaming query.  The footer advertises
// "esc to interrupt" while loading (is_loading → ModeIndicatorOptions), so
// ESC is the discoverable cancel binding the hint promises.  This is the
// ESC twin of CtrlCWhileStreamingQueryCancelsWithoutExiting: it exercises
// the repl_screen CatchEvent ESC branch (tool_animating → on_interrupt)
// rather than the global Ctrl+C shortcut.
TEST(AppRuntime, EscWhileStreamingQueryInterruptsAndRestoresInput) {
    LocalChunkedMessagesStreamServer server;
    ASSERT_TRUE(server.valid());

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = server.base_url();
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_esc_interrupt_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    bool exited = false;
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [&] {
            exited = true;
        });
    ReleaseAfterCancelGuard release_guard{server};

    app->HandleSubmit("show esc interrupt behavior");
    ASSERT_TRUE(server.wait_for_first_delta());
    ASSERT_TRUE(wait_until([&] {
        auto rendered = render_to_plain_text(app->Render(), 120, 32);
        return rendered.find("partial UI stream") != std::string::npos;
    }, std::chrono::seconds(2)));
    EXPECT_TRUE(test_seams(app).is_loading_for_testing());
    EXPECT_TRUE(test_seams(app).is_query_running_for_testing());

    // ESC interrupts the running query.  Unlike Ctrl+C (which has an idle
    // double-press-to-exit binding), ESC must never exit the app — it is
    // purely the cancel binding.
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Escape));
    EXPECT_FALSE(exited);
    // Abort is asynchronous: engine->abort() sets the flag, but the HTTP
    // stream only disconnects on the next chunk (the server is blocked
    // waiting for release_after_cancel below).  So the query is still
    // "running" and the spinner still shows "Cancelling...".
    EXPECT_TRUE(test_seams(app).is_loading_for_testing());
    EXPECT_TRUE(test_seams(app).is_query_running_for_testing());
    EXPECT_EQ(test_seams(app).status_message_for_testing(), "Cancelling...");

    auto cancelling = render_to_plain_text(app->Render(), 120, 32);
    EXPECT_NE(cancelling.find("Cancelling..."), std::string::npos);
    EXPECT_NE(cancelling.find("partial UI stream"), std::string::npos);

    // Release the server so it sends the "after cancel" chunk; the client's
    // content_receiver sees should_abort() and disconnects, finishing the query.
    server.release_after_cancel();
    EXPECT_TRUE(wait_until([&] {
        (void)app->Render();
        return !test_seams(app).is_query_running_for_testing();
    }, std::chrono::seconds(3)));
    (void)app->Render();
    EXPECT_FALSE(test_seams(app).is_loading_for_testing());

    // The user's core requirement: after ESC interrupts, input must be
    // editable again so they can type the next message.
    (void)app->OnEvent(ftxui::Event::Character('h'));
    (void)app->OnEvent(ftxui::Event::Character('i'));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "hi");

    fs::remove_all(storage_root);
}



// Regression guard: the up arrow must recall persisted prompt history after
// restart.  input_history was in-memory only — populated by Enter presses
// during the current session, empty on startup — so the first ArrowUp after
// relaunching loom did nothing.  The AppAdapter constructor now loads
// persisted history (project-filtered) into input_history.
TEST(AppRuntime, UpArrowRecallsPersistedHistoryAcrossSessions) {
    // Override HOME + LOOM_HISTORY_FILE so history resolves to a temp dir.
    const char* old_home = std::getenv("HOME");
    const auto temp_home = fs::temp_directory_path() /
        ("loom_uparrow_hist_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_home / ".loom");
    setenv("HOME", temp_home.string().c_str(), 1);
    ScopedEnvVar hist_env("LOOM_HISTORY_FILE");
    hist_env.set((temp_home / ".loom" / "history.jsonl").string());

    // The project cwd determines the history filter.
    const auto project_cwd = temp_home / "project";
    fs::create_directories(project_cwd);

    // Write a persisted history entry for this project.
    {
        std::ofstream ofs(temp_home / ".loom" / "history.jsonl");
        ofs << "{\"display\":\"previous prompt\",\"full_text\":\"previous prompt\","
               "\"timestamp\":1791000000000,\"sessionId\":\"sess-1\",\"project\":\""
            << project_cwd.string() << "\"}\n";
    }

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = project_cwd.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = temp_home / "storage";
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // The constructor loads persisted history into input_history.  ArrowUp
    // recalls the most recent entry — this is the "restart loom, press up"
    // scenario that was broken (input_history was empty on startup).
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "previous prompt");

    // Restore HOME and clean up.
    if (old_home) setenv("HOME", old_home, 1);
    else unsetenv("HOME");
    fs::remove_all(temp_home);
}

TEST(AppRuntime, UpArrowNavigatesHistoryPastCommandEntries) {
    // Regression test: after recalling a command entry (e.g. "/resume"),
    // the autocomplete popup shows command suggestions and the next
    // ArrowUp was captured by autocomplete navigation instead of
    // continuing history navigation.  The fix gives history navigation
    // priority when history_index != npos (already navigating history).
    const char* old_home = std::getenv("HOME");
    const auto temp_home = fs::temp_directory_path() /
        ("loom_uparrow_cmd_hist_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_home / ".loom");
    setenv("HOME", temp_home.string().c_str(), 1);
    ScopedEnvVar hist_env("LOOM_HISTORY_FILE");
    hist_env.set((temp_home / ".loom" / "history.jsonl").string());

    const auto project_cwd = temp_home / "project";
    fs::create_directories(project_cwd);

    // Four entries, oldest first in the file.  load_recent_prompt_texts
    // reads newest-first then reverses, so input_history ends up
    // oldest-first: [0]="first prompt", [1]="/clear", [2]="/resume",
    // [3]="last prompt".
    {
        std::ofstream ofs(temp_home / ".loom" / "history.jsonl");
        ofs << "{\"display\":\"first prompt\",\"full_text\":\"first prompt\","
               "\"timestamp\":1791000000000,\"sessionId\":\"sess-1\",\"project\":\""
            << project_cwd.string() << "\"}\n";
        ofs << "{\"display\":\"/clear\",\"full_text\":\"/clear\","
               "\"timestamp\":1791000001000,\"sessionId\":\"sess-1\",\"project\":\""
            << project_cwd.string() << "\"}\n";
        ofs << "{\"display\":\"/resume\",\"full_text\":\"/resume\","
               "\"timestamp\":1791000002000,\"sessionId\":\"sess-1\",\"project\":\""
            << project_cwd.string() << "\"}\n";
        ofs << "{\"display\":\"last prompt\",\"full_text\":\"last prompt\","
               "\"timestamp\":1791000003000,\"sessionId\":\"sess-1\",\"project\":\""
            << project_cwd.string() << "\"}\n";
    }

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = project_cwd.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = temp_home / "storage";
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // ArrowUp recalls the newest entry ("last prompt").
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "last prompt");

    // ArrowUp again — must recall "/resume" even though the autocomplete
    // popup is now showing command suggestions for "/resume".
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "/resume");

    // ArrowUp again — "/clear" is also a command entry.
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "/clear");

    // ArrowUp again — the oldest entry.
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "first prompt");

    // ArrowUp at the oldest entry stays put.
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "first prompt");

    // ArrowDown navigates forward through the same entries.
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowDown));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "/clear");
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowDown));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "/resume");
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowDown));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "last prompt");

    // ArrowDown past the newest entry clears the input.
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowDown));
    EXPECT_TRUE(test_seams(app).input_text_for_testing().empty());

    // Restore HOME and clean up.
    if (old_home) setenv("HOME", old_home, 1);
    else unsetenv("HOME");
    fs::remove_all(temp_home);
}

TEST(AppRuntime, UpArrowHistoryPreservesDuplicateEntries) {
    // Regression test: load_recent_prompt_texts used to dedup by display
    // text, so the same prompt typed in multiple sessions appeared only
    // once in Up-arrow recall.  The dedup was removed — duplicates are
    // now preserved.
    const char* old_home = std::getenv("HOME");
    const auto temp_home = fs::temp_directory_path() /
        ("loom_uparrow_dup_hist_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_home / ".loom");
    setenv("HOME", temp_home.string().c_str(), 1);
    ScopedEnvVar hist_env("LOOM_HISTORY_FILE");
    hist_env.set((temp_home / ".loom" / "history.jsonl").string());

    const auto project_cwd = temp_home / "project";
    fs::create_directories(project_cwd);

    // Three entries: "same prompt" appears twice (sess-1 and sess-3),
    // with "other prompt" in between (sess-2).  Oldest first in the file.
    {
        std::ofstream ofs(temp_home / ".loom" / "history.jsonl");
        ofs << "{\"display\":\"same prompt\",\"full_text\":\"same prompt\","
               "\"timestamp\":1791000000000,\"sessionId\":\"sess-1\",\"project\":\""
            << project_cwd.string() << "\"}\n";
        ofs << "{\"display\":\"other prompt\",\"full_text\":\"other prompt\","
               "\"timestamp\":1791000001000,\"sessionId\":\"sess-2\",\"project\":\""
            << project_cwd.string() << "\"}\n";
        ofs << "{\"display\":\"same prompt\",\"full_text\":\"same prompt\","
               "\"timestamp\":1791000002000,\"sessionId\":\"sess-3\",\"project\":\""
            << project_cwd.string() << "\"}\n";
    }

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = project_cwd.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = temp_home / "storage";
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // ArrowUp recalls the newest entry ("same prompt" from sess-3).
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "same prompt");

    // ArrowUp again — "other prompt" from sess-2.
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "other prompt");

    // ArrowUp again — the OLDER "same prompt" from sess-1 must still be
    // present (previously deduped away).
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "same prompt");

    // Restore HOME and clean up.
    if (old_home) setenv("HOME", old_home, 1);
    else unsetenv("HOME");
    fs::remove_all(temp_home);
}

TEST(AppRuntime, UpArrowHistoryFindsProjectEntriesBeyondOtherProjects) {
    // Regression test: read_history_recent used a small batch (max_entries * 2)
    // which could be entirely filled by newer entries from other projects
    // (e.g. test runs in temp directories), causing the current project's
    // entries to be missed entirely — ArrowUp did nothing.  The fix reads
    // all entries.
    const char* old_home = std::getenv("HOME");
    const auto temp_home = fs::temp_directory_path() /
        ("loom_uparrow_batch_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_home / ".loom");
    setenv("HOME", temp_home.string().c_str(), 1);
    ScopedEnvVar hist_env("LOOM_HISTORY_FILE");
    hist_env.set((temp_home / ".loom" / "history.jsonl").string());

    const auto project_cwd = temp_home / "project";
    fs::create_directories(project_cwd);

    // 3 current-project entries (older) + 300 other-project entries (newer).
    // The 300 other-project entries would fill a 200-entry batch, hiding
    // the current project's entries.
    {
        std::ofstream ofs(temp_home / ".loom" / "history.jsonl");
        ofs << "{\"display\":\"my prompt 1\",\"full_text\":\"my prompt 1\","
               "\"timestamp\":1791000000000,\"sessionId\":\"sess-1\",\"project\":\""
            << project_cwd.string() << "\"}\n";
        ofs << "{\"display\":\"my prompt 2\",\"full_text\":\"my prompt 2\","
               "\"timestamp\":1791000001000,\"sessionId\":\"sess-1\",\"project\":\""
            << project_cwd.string() << "\"}\n";
        ofs << "{\"display\":\"my prompt 3\",\"full_text\":\"my prompt 3\","
               "\"timestamp\":1791000002000,\"sessionId\":\"sess-1\",\"project\":\""
            << project_cwd.string() << "\"}\n";
        for (int i = 0; i < 300; ++i) {
            ofs << "{\"display\":\"other " << i << "\",\"full_text\":\"other " << i << "\","
                   "\"timestamp\":" << (1791000003000 + i)
                << ",\"sessionId\":\"sess-other\",\"project\":\"/tmp/other-project\"}\n";
        }
    }

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = project_cwd.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = temp_home / "storage";
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // ArrowUp must recall the current project's newest entry, not nothing.
    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "my prompt 3");

    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "my prompt 2");

    ASSERT_TRUE(app->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "my prompt 1");

    // Restore HOME and clean up.
    if (old_home) setenv("HOME", old_home, 1);
    else unsetenv("HOME");
    fs::remove_all(temp_home);
}

TEST(AppRuntime, TabAcceptsNextActionSuggestion) {
    // Regression test: the next-action suggestion must be accepted into the
    // input buffer when the user presses Tab (shown as an inline gray
    // placeholder, not as a popup above the input).
    const auto temp_home = fs::temp_directory_path() /
        ("loom_tab_suggest_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_home / ".loom");

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = temp_home.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    loom::utils::SessionStorage storage(temp_home / "storage");

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // Simulate the PromptSuggestionService having produced a suggestion.
    test_seams(app).set_next_action_suggestion_for_testing("explain the error above");
    EXPECT_TRUE(test_seams(app).input_text_for_testing().empty());

    // Tab accepts the suggestion into the input buffer.
    ASSERT_TRUE(app->OnEvent(ftxui::Event::Tab));
    EXPECT_EQ(test_seams(app).input_text_for_testing(), "explain the error above");

    fs::remove_all(temp_home);
}

TEST(AppRuntime, TabDoesNotAcceptSlashSuggestion) {
    // Slash-commands are handled by the autocomplete popup, not the inline
    // next-action suggestion.  Tab must NOT accept a suggestion that starts
    // with '/'.
    const auto temp_home = fs::temp_directory_path() /
        ("loom_tab_slash_suggest_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_home / ".loom");

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = temp_home.string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    loom::utils::SessionStorage storage(temp_home / "storage");

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    test_seams(app).set_next_action_suggestion_for_testing("/commit");
    EXPECT_TRUE(test_seams(app).input_text_for_testing().empty());

    // Tab must NOT accept a slash-command suggestion.
    app->OnEvent(ftxui::Event::Tab);
    EXPECT_TRUE(test_seams(app).input_text_for_testing().empty());

    fs::remove_all(temp_home);
}


TEST(AppRuntime, PermissionCallbackRendersAndResolvesUserChoices) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_permission_dialog_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine,
        nullptr,
        &commands,
        &storage,
        [] {});
    auto permission_callback = app->get_permission_callback();

    std::atomic<bool> allow_done{false};
    std::atomic<bool> allow_result{false};
    std::jthread allow_worker([&] {
        allow_result.store(permission_callback("Bash", "Run npm test"), std::memory_order_release);
        allow_done.store(true, std::memory_order_release);
    });

    const bool allow_prompt_shown = wait_until([&] {
        (void)app->Render();
        return test_seams(app).has_pending_dialog_for_testing();
    }, std::chrono::milliseconds(1000));
    EXPECT_TRUE(allow_prompt_shown);
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('y')));
    EXPECT_TRUE(wait_until([&] { return allow_done.load(std::memory_order_acquire); },
                           std::chrono::milliseconds(1000)));
    EXPECT_TRUE(allow_result.load(std::memory_order_acquire));
    allow_worker.join();

    std::atomic<bool> deny_done{false};
    std::atomic<bool> deny_result{true};
    std::jthread deny_worker([&] {
        deny_result.store(permission_callback("Write", "Modify src/main.cpp"), std::memory_order_release);
        deny_done.store(true, std::memory_order_release);
    });

    const bool deny_prompt_shown = wait_until([&] {
        (void)app->Render();
        return test_seams(app).has_pending_dialog_for_testing();
    }, std::chrono::milliseconds(1000));
    EXPECT_TRUE(deny_prompt_shown);
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('n')));
    EXPECT_TRUE(wait_until([&] { return deny_done.load(std::memory_order_acquire); },
                           std::chrono::milliseconds(1000)));
    EXPECT_FALSE(deny_result.load(std::memory_order_acquire));
    deny_worker.join();

    std::atomic<bool> always_done{false};
    std::atomic<bool> always_result{false};
    std::jthread always_worker([&] {
        always_result.store(permission_callback("Read", "Read package.json"), std::memory_order_release);
        always_done.store(true, std::memory_order_release);
    });

    const bool always_prompt_shown = wait_until([&] {
        (void)app->Render();
        return test_seams(app).has_pending_dialog_for_testing();
    }, std::chrono::milliseconds(1000));
    EXPECT_TRUE(always_prompt_shown);
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('a')));
    EXPECT_TRUE(wait_until([&] { return always_done.load(std::memory_order_acquire); },
                           std::chrono::milliseconds(1000)));
    EXPECT_TRUE(always_result.load(std::memory_order_acquire));
    always_worker.join();

    std::atomic<bool> repeated_done{false};
    std::atomic<bool> repeated_result{false};
    std::jthread repeated_worker([&] {
        repeated_result.store(permission_callback("Read", "Read package-lock.json"), std::memory_order_release);
        repeated_done.store(true, std::memory_order_release);
    });

    const bool completed_without_prompt = wait_until(
        [&] { return repeated_done.load(std::memory_order_acquire); },
        std::chrono::milliseconds(200));
    EXPECT_TRUE(completed_without_prompt);
    if (!completed_without_prompt) {
        EXPECT_TRUE(wait_until([&] {
            auto rendered = render_to_plain_text(app->Render(), 120, 34);
            return rendered.find("Permission Required") != std::string::npos &&
                   rendered.find("Read package-lock.json") != std::string::npos;
        }, std::chrono::milliseconds(1000)));
        EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('y')));
        EXPECT_TRUE(wait_until([&] { return repeated_done.load(std::memory_order_acquire); },
                               std::chrono::milliseconds(1000)));
    }
    EXPECT_TRUE(repeated_result.load(std::memory_order_acquire));
    repeated_worker.join();

    fs::remove_all(storage_root);
}



TEST(AppRuntime, RenderMessageShowsCollapsedThinkingWhenUnselected) {
    loom::core::AssistantMessage assistant;
    assistant.content.push_back(loom::core::ThinkingBlock{
        .thinking = "private reasoning preview",
        .signature = "sig-1",
    });

    // Completed thinking blocks render as a collapsed summary
    // (∴ Thinking + first-line summary + ctrl+o hint) that can be
    // expanded on click.  They are NOT hidden entirely.
    auto rendered = render_to_plain_text(
        loom::ui::RenderMessage(loom::core::Message{std::move(assistant)}), 140, 24);

    // The collapsed "Thinking" label should be present.
    EXPECT_NE(rendered.find("Thinking"), std::string::npos);
    // The summary (first 50 chars of thinking content) should be shown.
    EXPECT_NE(rendered.find("private reasoning preview"), std::string::npos);
}



TEST(AppRuntime, RenderMessageShowsCompletedThinkingWhenExpanded) {
    // Regression safety: transcript mode / explicit expand still renders
    // the collapsed label + summary, but no full content leakage.
    loom::core::AssistantMessage assistant;
    assistant.content.push_back(loom::core::ThinkingBlock{
        .thinking = "first line of reasoning\nsecond line with private details",
        .signature = "sig-2",
    });

    // The project_messages() flow with selected_row_idx pointing at the
    // thinking row is what triggers "selected_or_active=true" in the
    // render_payload_row() Thinking guard.  RenderMessage() hardcodes
    // selected_row_idx=-1, so to cover the selected branch we build the
    // visible list manually via repl_screen::RenderMessages with selected=0.
    auto input = loom::ui::project_messages(
        loom::core::Message{std::move(assistant)});
    // Pass disable_chain_compression=true so the thinking block renders
    // individually (collapsed label) rather than being compressed into a
    // chain summary line.
    auto rendered_selected = render_to_plain_text(
        loom::ui::repl_screen::RenderMessages(input, /*selected=*/0, 40,
            0, true, 0, std::nullopt, {}, false, {}, false, false, true),
        140, 24);

    // The "Thinking" label should be present.
    EXPECT_NE(rendered_selected.find("Thinking"), std::string::npos);
    // The collapsed summary (first line) SHOULD be shown.
    EXPECT_NE(rendered_selected.find("first line of reasoning"),
              std::string::npos);
    // But the full content (second line) must NOT leak.
    EXPECT_EQ(rendered_selected.find("second line with private details"),
              std::string::npos);
}



TEST(AppRuntime, RenderMessageShowsToolUseContent) {
    loom::core::AssistantMessage assistant;
    assistant.content.push_back(loom::core::ToolUseBlock{
        .id = loom::core::ToolUseId{"tool-ui-1"},
        .name = "Bash",
        .input_json = R"({"command":"npm test"})",
    });

    auto rendered = render_to_plain_text(
        loom::ui::RenderMessage(loom::core::Message{std::move(assistant)}), 140, 24);

    EXPECT_NE(rendered.find("Bash"), std::string::npos);
}



TEST(AppRuntime, RenderMessageShowsAssistantText) {
    loom::core::AssistantMessage assistant;
    assistant.content.push_back(loom::core::TextBlock{"visible assistant answer"});

    auto rendered = render_to_plain_text(
        loom::ui::RenderMessage(loom::core::Message{std::move(assistant)}), 140, 24);

    EXPECT_NE(rendered.find("visible assistant answer"), std::string::npos);
}



TEST(AppRuntime, RenderMessageShowsUserMessage) {
    loom::core::UserMessage user;
    user.content.push_back(loom::core::TextBlock{"hello world"});

    auto rendered = render_to_plain_text(
        loom::ui::RenderMessage(loom::core::Message{std::move(user)}), 140, 24);

    EXPECT_NE(rendered.find("hello world"), std::string::npos);
}




TEST(AppRuntime, CollapseBackgroundBashWiredIntoLiveTranscript) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ui_collapse_wire_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    // Append 3 consecutive completed background-bash notifications (CPP wire
    // format: underscored tags) directly to the engine conversation.
    auto make_bash_notif = [](std::string_view name) {
        loom::core::UserMessage m{};
        std::string text =
            "<task_notification><status>completed</status><summary>"
            "Background command " + std::string(name) + " completed"
            "</summary></task_notification>";
        m.content.push_back(loom::core::TextBlock{std::move(text)});
        return loom::core::Message{std::move(m)};
    };
    engine.append_message_for_testing(make_bash_notif("\"a\""));
    engine.append_message_for_testing(make_bash_notif("\"b\""));
    engine.append_message_for_testing(make_bash_notif("\"c\""));

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});
    app->SyncState();

    // Count how many user rows carry a task-notification.  Before the fix this
    // would be 3 (one per notification); wired collapse merges them into 1.
    const auto msgs = test_seams(app).messages_for_testing();
    int user_rows = 0;
    for (const auto& row : msgs) {
        if (row.rfind("user", 0) == 0) ++user_rows;
    }
    EXPECT_EQ(user_rows, 1)
        << "3 consecutive background-bash notifications must collapse to 1 row";

    fs::remove_all(storage_root);
}



// Diagnostic: verify spacing between tool_result and assistant text.
// Completed thinking blocks now render as collapsed summaries (visible between
// the tool result and the assistant text), so the gap includes the thinking
// line plus its margins.
TEST(AppRuntime, ToolResultToAssistantTextSpacingIsOneLine) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    // Match the real flow: user → assistant(thinking+tool) → result → assistant(thinking+text)
    loom::core::UserMessage u;
    u.content.push_back(loom::core::TextBlock{"what day is it"});
    engine.append_message_for_testing(loom::core::Message{std::move(u)});

    loom::core::AssistantMessage a1;
    a1.content.push_back(loom::core::ThinkingBlock{.thinking = "let me check", .signature = ""});
    a1.content.push_back(loom::core::ToolUseBlock{
        .id = loom::core::ToolUseId{"tu1"}, .name = "Edit",
        .input_json = R"({"file_path":"test.txt"})"});
    engine.append_message_for_testing(loom::core::Message{std::move(a1)});

    loom::core::ToolResultMessage tr;
    tr.tool_use_id = loom::core::ToolUseId{"tu1"};
    tr.tool_name = "Edit";
    tr.content.push_back(loom::core::TextBlock{"2026-07-08 Wednesday\n"});
    engine.append_message_for_testing(loom::core::Message{std::move(tr)});

    loom::core::AssistantMessage a2;
    a2.content.push_back(loom::core::ThinkingBlock{.thinking = "got the date", .signature = ""});
    a2.content.push_back(loom::core::TextBlock{"Today is Wednesday."});
    engine.append_message_for_testing(loom::core::Message{std::move(a2)});

    loom::commands::AppCommandRegistry commands;
    const auto storage_root2 = fs::temp_directory_path() /
        ("loom_spacing_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root2);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});
    // Disable chain compression so the thinking block renders individually
    // (collapsed label) — this test verifies spacing around the collapsed
    // thinking row, not chain compression.
    loom::ui::test_seams(app).set_disable_chain_compression_for_testing(true);
    app->SyncState();

    auto rendered = strip_ansi(render_to_plain_text(app->Render(), 120, 40));

    auto lines = std::vector<std::string>{};
    std::size_t pos = 0;
    while (pos <= rendered.size()) {
        auto nl = rendered.find('\n', pos);
        lines.push_back(nl == std::string::npos
            ? rendered.substr(pos) : rendered.substr(pos, nl - pos));
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }

    int result_line = -1, text_line = -1;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        if (lines[i].find("2026-07-08") != std::string::npos) result_line = i;
        if (lines[i].find("Today is Wednesday") != std::string::npos) text_line = i;
    }

    ASSERT_GE(result_line, 0) << "tool result not found\n" << rendered;
    ASSERT_GE(text_line, 0) << "assistant text not found\n" << rendered;
    ASSERT_GT(text_line, result_line);

    int gap = text_line - result_line - 1;
    // Print ALL lines for debugging
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        if (!lines[i].empty() && lines[i].find_first_not_of(' ') != std::string::npos)
            std::cerr << "  L" << i << ": \"" << lines[i].substr(0, 60) << "\"\n";
        else if (i >= result_line - 3 && i <= text_line + 1)
            std::cerr << "  L" << i << ": (blank)\n";
    }
    // With completed thinking blocks now visible as collapsed summaries, the
    // gap between tool_result and assistant text is 3: blank margin, the
    // collapsed "∴ Thinking" line, and another blank margin.
    EXPECT_EQ(gap, 3)
        << "Expected 3 lines (blank + collapsed thinking + blank) between "
           "tool_result and assistant text, got " << gap;

    fs::remove_all(storage_root2);
}



/// E2E Gate #1: Startup screen must show logo, statusline, and prompt.
/// Regression guard for "statusline disappeared" bug.

// ============================================================
// @agent + @history autocomplete sources
// ============================================================

namespace acsrc = loom::ui::autocomplete_sources;

/// Round-trip: append_prompt_history writes a JSONL line, then
/// collect_history_suggestions reads it back (newest-first) and matches
/// by substring query.  Uses LOOM_HISTORY_FILE env var to isolate
/// the test from the real ~/.loom/history.jsonl.
TEST(AutocompleteSources, AppendAndReadHistoryRoundTrip) {
    const auto hist_path = fs::temp_directory_path() /
        ("loom_hist_roundtrip_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".jsonl");
    ScopedEnvVar env("LOOM_HISTORY_FILE");
    env.set(hist_path.string());

    // Empty file => empty results.
    auto empty = acsrc::collect_history_suggestions("", 50);
    EXPECT_TRUE(empty.empty());

    // Append three prompts (chronological order: oldest first).
    acsrc::append_prompt_history("fix the flaky test", "sess-aaa", "/proj");
    acsrc::append_prompt_history("refactor auth module", "sess-bbb", "/proj");
    acsrc::append_prompt_history("write a unit test", "sess-ccc", "/proj");

    // Empty query => all entries, newest first ("write a unit test" first).
    auto all = acsrc::collect_history_suggestions("", 50);
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].prompt_text, "write a unit test");
    EXPECT_EQ(all[1].prompt_text, "refactor auth module");
    EXPECT_EQ(all[2].prompt_text, "fix the flaky test");

    // Substring query "test" matches two entries (newest first).
    auto matched = acsrc::collect_history_suggestions("test", 50);
    ASSERT_EQ(matched.size(), 2u);
    EXPECT_EQ(matched[0].prompt_text, "write a unit test");
    EXPECT_EQ(matched[1].prompt_text, "fix the flaky test");

    // Case-insensitive: "AUTH" should match "refactor auth module".
    auto ci = acsrc::collect_history_suggestions("AUTH", 50);
    ASSERT_EQ(ci.size(), 1u);
    EXPECT_EQ(ci[0].prompt_text, "refactor auth module");

    // Cap at max_entries: request 1, get 1 (newest).
    auto capped = acsrc::collect_history_suggestions("", 1);
    ASSERT_EQ(capped.size(), 1u);
    EXPECT_EQ(capped[0].prompt_text, "write a unit test");

    // Dedup by display: append a duplicate of an existing prompt.
    acsrc::append_prompt_history("write a unit test", "sess-ddd", "/proj");
    auto deduped = acsrc::collect_history_suggestions("test", 50);
    ASSERT_EQ(deduped.size(), 2u) << "duplicate display should be deduped";

    fs::remove(hist_path);
}



/// build_history_suggestions produces FormattedSuggestion entries with
/// truncated display + relative-time description.
TEST(AutocompleteSources, BuildHistorySuggestionsFormatting) {
    const auto hist_path = fs::temp_directory_path() /
        ("loom_hist_fmt_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".jsonl");
    ScopedEnvVar env("LOOM_HISTORY_FILE");
    env.set(hist_path.string());

    // Long prompt (>80 chars) should be truncated in display.
    acsrc::append_prompt_history(
        "this is a very long prompt that exceeds eighty characters in display "
        "length and should be truncated with ellipsis", "sess-xyz", "/proj");

    auto sugs = acsrc::build_history_suggestions("", 0, 5, 50);
    ASSERT_EQ(sugs.size(), 1u);
    EXPECT_LE(sugs[0].display_text.size(), 83u)  // 80 + "..."
        << "display should be truncated to ~80 chars";
    EXPECT_EQ(sugs[0].display_text.back(), '.')
        << "truncated display should end with '...'";
    // insert_text is the FULL prompt (not truncated).
    EXPECT_GT(sugs[0].insert_text.size(), sugs[0].display_text.size());
    EXPECT_EQ(sugs[0].replacement_start, 0u);
    EXPECT_EQ(sugs[0].replacement_end, 5u);
    EXPECT_FALSE(sugs[0].submit_on_return);
    EXPECT_EQ(sugs[0].id.substr(0, 8), "history:");

    fs::remove(hist_path);
}



/// build_agent_suggestions returns agent/teammate suggestions with the
/// color_name field populated from agent.color / record.teammate_color.
/// At minimum the built-in "loom" agent should be present.
TEST(AutocompleteSources, BuildAgentSuggestionsHasColors) {
    auto agents = acsrc::collect_agent_suggestions("");
    ASSERT_FALSE(agents.empty())
        << "expected at least one built-in agent definition";

    // The default "loom" agent should exist (catch-all).
    auto it = std::find_if(agents.begin(), agents.end(),
        [](const auto& a) { return a.name == "loom"; });
    ASSERT_NE(it, agents.end()) << "built-in 'loom' agent not found";

    // build_agent_suggestions with empty query returns all agents (fuzzy
    // match passes for everything when query is empty).
    auto sugs = acsrc::build_agent_suggestions("", "", 0, 7);
    ASSERT_FALSE(sugs.empty());
    bool found_loom = false;
    for (const auto& s : sugs) {
        EXPECT_FALSE(s.display_text.empty());
        EXPECT_TRUE(s.display_text.starts_with("@"))
            << "display should start with @";
        EXPECT_EQ(s.replacement_start, 0u);
        EXPECT_EQ(s.replacement_end, 7u);
        EXPECT_FALSE(s.submit_on_return);
        if (s.display_text == "@loom") {
            found_loom = true;
            EXPECT_FALSE(s.icon.empty()) << "loom agent should have an icon";
            EXPECT_FALSE(s.id.empty());
        }
    }
    EXPECT_TRUE(found_loom) << "@loom suggestion not found in results";

    // Fuzzy filter: query "xyz" should match nothing (no agent named xyz).
    auto filtered = acsrc::build_agent_suggestions("", "xyz_nonexistent", 0, 3);
    EXPECT_TRUE(filtered.empty());
}



/// Typing "@history " in the prompt triggers history suggestions from the
/// persisted history file.  We pre-populate the history file, then type
/// "@history " and verify suggestions appear.
TEST(AppRuntime, AtHistoryShowsPersistedPrompts) {
    const auto hist_path = fs::temp_directory_path() /
        ("loom_app_hist_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".jsonl");
    ScopedEnvVar env("LOOM_HISTORY_FILE");
    env.set(hist_path.string());

    // Pre-populate history.
    acsrc::append_prompt_history("deploy to production", "sess-1", "/proj");
    acsrc::append_prompt_history("review the pull request", "sess-2", "/proj");

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_app_hist_storage_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // Type "@history " — should trigger history suggestions.
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('@')));
    for (char c : std::string("history")) {
        EXPECT_TRUE(app->OnEvent(ftxui::Event::Character(c)));
    }
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character(' ')));

    ASSERT_GT(test_seams(app).autocomplete_suggestion_count_for_testing(), 0u)
        << "@history should show persisted prompt suggestions";

    auto suggestions = test_seams(app).autocomplete_suggestions_for_testing();
    bool found_deploy = std::any_of(suggestions.begin(), suggestions.end(),
        [](const std::string& s) { return s.find("deploy to production") != std::string::npos; });
    bool found_review = std::any_of(suggestions.begin(), suggestions.end(),
        [](const std::string& s) { return s.find("review the pull request") != std::string::npos; });
    EXPECT_TRUE(found_deploy) << "history suggestion 'deploy to production' not found";
    EXPECT_TRUE(found_review) << "history suggestion 'review the pull request' not found";

    fs::remove_all(storage_root);
    fs::remove(hist_path);
}



/// Typing "@history deploy" filters history by the substring "deploy".
TEST(AppRuntime, AtHistoryWithQueryFiltersResults) {
    const auto hist_path = fs::temp_directory_path() /
        ("loom_app_hist_filter_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".jsonl");
    ScopedEnvVar env("LOOM_HISTORY_FILE");
    env.set(hist_path.string());

    acsrc::append_prompt_history("deploy to production", "sess-1", "/proj");
    acsrc::append_prompt_history("review the pull request", "sess-2", "/proj");

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_app_hist_filter_storage_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // Type "@history review" — should only show "review the pull request".
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('@')));
    for (char c : std::string("history")) {
        EXPECT_TRUE(app->OnEvent(ftxui::Event::Character(c)));
    }
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character(' ')));
    for (char c : std::string("review")) {
        EXPECT_TRUE(app->OnEvent(ftxui::Event::Character(c)));
    }

    auto suggestions = test_seams(app).autocomplete_suggestions_for_testing();
    bool found_review = std::any_of(suggestions.begin(), suggestions.end(),
        [](const std::string& s) { return s.find("review the pull request") != std::string::npos; });
    bool found_deploy = std::any_of(suggestions.begin(), suggestions.end(),
        [](const std::string& s) { return s.find("deploy to production") != std::string::npos; });
    EXPECT_TRUE(found_review) << "filtered history should show 'review the pull request'";
    EXPECT_FALSE(found_deploy) << "filtered history should NOT show 'deploy to production'";

    fs::remove_all(storage_root);
    fs::remove(hist_path);
}



/// Ctrl+R (\\x12) injects "@history " into the input, triggering history
/// search mode.  The input text should start with "@history ".
TEST(AppRuntime, CtrlREntersHistorySearchMode) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_app_ctrlr_storage_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // Ctrl+R should inject "@history " into the input.
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('\x12')));
    EXPECT_TRUE(test_seams(app).input_text_for_testing().starts_with("@history"))
        << "Ctrl+R should set input to '@history ' prefix, got: "
        << test_seams(app).input_text_for_testing();

    // Pressing Ctrl+R again should NOT duplicate the prefix.
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('\x12')));
    EXPECT_TRUE(test_seams(app).input_text_for_testing().starts_with("@history"))
        << "second Ctrl+R should keep '@history ' prefix";
    // Count occurrences of "@history" — should be exactly 1.
    const auto input = test_seams(app).input_text_for_testing();
    size_t count = 0;
    size_t pos = 0;
    while ((pos = input.find("@history", pos)) != std::string::npos) {
        ++count;
        pos += 8;  // len("@history") = 8
    }
    EXPECT_EQ(count, 1u) << "@history should appear exactly once after two Ctrl+R presses";

    fs::remove_all(storage_root);
}



/// Submitting a prompt persists it to history, so subsequent @history
/// searches can find it.  Verifies the end-to-end persistence wiring.
TEST(AppRuntime, SubmitPersistsPromptToHistory) {
    const auto hist_path = fs::temp_directory_path() /
        ("loom_app_submit_hist_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".jsonl");
    ScopedEnvVar env("LOOM_HISTORY_FILE");
    env.set(hist_path.string());

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_app_submit_hist_storage_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // Type a unique prompt and submit.
    const std::string unique_prompt = "unique_persist_test_prompt_xyz";
    for (char c : unique_prompt) {
        EXPECT_TRUE(app->OnEvent(ftxui::Event::Character(c)));
    }
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Return));

    // Now verify the prompt appears in history via collect_history_suggestions.
    auto hist = acsrc::collect_history_suggestions(unique_prompt, 50);
    ASSERT_EQ(hist.size(), 1u) << "submitted prompt should appear in history";
    EXPECT_EQ(hist[0].prompt_text, unique_prompt);

    // Tear the app down BEFORE deleting its storage root so background
    // flush threads cannot repopulate the directory mid-remove_all.
    app.reset();

    std::error_code ec;
    fs::remove_all(storage_root, ec);
    if (ec || fs::exists(storage_root, ec)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ec.clear();
        fs::remove_all(storage_root, ec);
    }
    fs::remove(hist_path, ec);
}



/// Typing "@" followed by agent name characters should show agent
/// suggestions.  At minimum "@cl" should match the "loom" agent.
TEST(AppRuntime, AtAgentShowsAgentSuggestions) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_app_at_agent_storage_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // Type "@lo" — should show agent suggestions matching "lo".
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('@')));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('l')));
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('o')));

    auto suggestions = test_seams(app).autocomplete_suggestions_for_testing();
    bool found_loom = std::any_of(suggestions.begin(), suggestions.end(),
        [](const std::string& s) { return s.find("@loom") != std::string::npos; });
    EXPECT_TRUE(found_loom) << "@lo should surface the @loom agent suggestion";

    fs::remove_all(storage_root);
}



// A pane teammate's filesystem inbox poll delivers addressed task messages
// (wrapped in the teammate_message XML tag) to the prompt queue, filters
// control messages, and dedupes across repeated polls.
TEST(AppRuntime, TeammateInboxPollDeliversTasksAndFiltersControl) {
    namespace tu = loom::utils;

    const auto runtime_dir = fs::temp_directory_path() /
        ("loom_teammate_inbox_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::remove_all(runtime_dir);
    ScopedEnvVar runtime_guard("LOOM_TEAM_RUNTIME_DIR");
    runtime_guard.set(runtime_dir.string());
    ScopedEnvVar team_guard("LOOM_TEAM_NAME");
    team_guard.set("alpha");

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    auto engine = std::make_unique<loom::core::QueryEngine>(std::move(config), tools);
    auto commands = std::make_unique<loom::commands::AppCommandRegistry>();
    const auto storage_root = fs::temp_directory_path() /
        ("loom_ti_storage_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto storage = std::make_unique<loom::utils::SessionStorage>(storage_root);
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        engine.get(), nullptr, commands.get(), storage.get(), [] {});

    test_seams(app).configure_teammate_for_testing("worker-a", "alpha");

    // Leader sends one task and one control message to worker-a's inbox.
    ASSERT_TRUE(tu::send_message(
        "worker-a", "please run the build", std::string_view("build it")).has_value());
    // Control message (shutdown) must not become a task prompt.
    ASSERT_TRUE(tu::send_message(
        "worker-a", "loom:shutdown approved").has_value());

    test_seams(app).poll_teammate_inbox_once_for_testing();
    // Only the task is queued.
    ASSERT_EQ(test_seams(app).teammate_pending_count_for_testing(), 1u);
    const auto prompt = test_seams(app).pop_teammate_prompt_for_testing();
    EXPECT_NE(prompt.find("<teammate_message teammate_id=\""), std::string::npos);
    EXPECT_NE(prompt.find("please run the build"), std::string::npos);
    EXPECT_EQ(prompt.find("loom:shutdown"), std::string::npos);

    // A second poll after read-marking delivers nothing (no duplicates).
    test_seams(app).poll_teammate_inbox_once_for_testing();
    EXPECT_EQ(test_seams(app).teammate_pending_count_for_testing(), 0u);

    fs::remove_all(runtime_dir);
    fs::remove_all(storage_root);
}



// ═══════════════════════════════════════════════════════════════════════════
// Live teams UI (stage C): live teammate strip + TeamsView modal + /teams.
// ═══════════════════════════════════════════════════════════════════════════

TEST(LiveTeamsUi, StripRendersNameStatusAndTail) {
    namespace repl = loom::ui::repl_screen;
    namespace live = loom::ui::teams::live;

    repl::ReplScreenState s;
    s.chrome_store.model_display_name = "M";
    s.cwd = "/tmp/x";

    live::LiveTeammate a;
    a.agent_id = "a1";
    a.name = "alice";
    a.color = "cyan";
    a.status = "running";
    a.last_output_tail = "BUILD TARGET widgets OK";
    a.pane_id = "%3";

    live::LiveTeammate b;
    b.agent_id = "a2";
    b.name = "bob";
    b.color = "red";
    b.status = "idle";
    b.last_output_tail = "waiting for review";
    b.pane_id = "%4";

    s.task_view_store.live_teammates = {a, b};
    s.task_view_store.teammate_count = 2;

    const auto txt = strip_ansi(
        render_to_plain_text(repl::RenderReplScreen(s), 140, 40));
    EXPECT_NE(txt.find("alice"), std::string::npos);
    EXPECT_NE(txt.find("running"), std::string::npos);
    EXPECT_NE(txt.find("BUILD TARGET widgets OK"), std::string::npos);
    EXPECT_NE(txt.find("bob"), std::string::npos);
    EXPECT_NE(txt.find("idle"), std::string::npos);
    // Footer pill (prompt_input_footer ModeIndicator label "N teams").
    EXPECT_NE(txt.find("2 teams"), std::string::npos);

    repl::ReplScreenState empty;
    empty.cwd = "/tmp/x";
    const auto none = strip_ansi(
        render_to_plain_text(repl::RenderReplScreen(empty), 140, 40));
    EXPECT_EQ(none.find("@alice"), std::string::npos);
    EXPECT_EQ(none.find("teams"), std::string::npos);
}



TEST(LiveTeamsUi, SlashTeamsOpensOverviewModal) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_teams_modal_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    namespace live = loom::ui::teams::live;
    live::LiveTeammate a;
    a.agent_id = "a1";
    a.name = "alice";
    a.color = "cyan";
    a.status = "running";
    a.last_output_tail = "TAIL-MARKER-42";
    a.pane_id = "%3";
    std::vector<live::LiveTeammate> teammates{a};
    test_seams(app).set_live_teammates_for_testing(&teammates);
    ASSERT_EQ(test_seams(app).teams_overview_count_for_testing(), 1);

    test_seams(app).handle_submit_for_testing("/teams");
    ASSERT_TRUE(test_seams(app).teams_overview_open_for_testing());

    const auto txt = strip_ansi(
        render_to_plain_text(app->Render(), 140, 40));
    EXPECT_NE(txt.find("alice"), std::string::npos);
    EXPECT_NE(txt.find("TAIL-MARKER-42"), std::string::npos);
    EXPECT_NE(txt.find("Esc close"), std::string::npos);

    // Unhandled Escape falls through to DispatchDialogQueueEvents' modal
    // fallback, which pops the stack.
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Escape));
    EXPECT_FALSE(test_seams(app).teams_overview_open_for_testing());

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}



// Leader-side: a queued stage-A permission_request surfaces through the
// existing ToolPermission overlay; approving replies via PermissionSync into
// the worker mailbox (no second dialog path).
TEST(LiveTeamsUi, TeammatePermissionRequestRoutesThroughToolPermission) {
    namespace sh = loom::utils::swarm_helpers;

    const auto runtime_dir = fs::temp_directory_path() /
        ("loom_teams_perm_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    fs::remove_all(runtime_dir);
    ScopedEnvVar runtime_guard("LOOM_TEAM_RUNTIME_DIR");
    runtime_guard.set(runtime_dir.string());
    ScopedEnvVar team_guard("LOOM_TEAM_NAME");
    team_guard.set("alpha");
    ScopedEnvVar agent_guard("LOOM_AGENT_NAME");
    agent_guard.unset();  // this process is the LEADER, not a pane teammate

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    auto engine = std::make_unique<loom::core::QueryEngine>(std::move(config), tools);
    auto commands = std::make_unique<loom::commands::AppCommandRegistry>();
    const auto storage_root = fs::temp_directory_path() /
        ("loom_teams_perm_storage_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto storage = std::make_unique<loom::utils::SessionStorage>(storage_root);
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        engine.get(), nullptr, commands.get(), storage.get(), [] {});

    sh::SwarmPermissionRequestMessage request;
    request.type = "permission_request";
    request.request_id = sh::PermissionSync::generate_request_id();
    request.agent_id = "worker-a";
    request.tool_name = "Bash";
    request.tool_use_id = "toolu_perm_1";
    request.description = R"({"command":"rm -rf build"})";
    request.input_json = R"({"command":"rm -rf build"})";
    test_seams(app).enqueue_teammate_permission_for_testing(&request, "alpha");

    // The queued request drains on the next Custom event into the existing
    // ToolPermission overlay (Band3), and the event is consumed like the
    // pane-teammate prompt drain.
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Custom));
    EXPECT_TRUE(test_seams(app).has_pending_dialog_for_testing());

    // Render surfaces the worker identity + tool name in the dialog.
    const auto txt = strip_ansi(
        render_to_plain_text(app->Render(), 140, 40));
    EXPECT_NE(txt.find("worker-a"), std::string::npos);
    EXPECT_NE(txt.find("Bash"), std::string::npos);

    // Approve ('y'): the stage-A success response is written to the worker's
    // mailbox and the overlay is dismissed.
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('y')));
    EXPECT_EQ(test_seams(app).pending_teammate_permission_count_for_testing(), 0u);

    auto worker_inbox = loom::utils::read_inbox(
        "worker-a", std::optional<std::string_view>{"alpha"});
    ASSERT_TRUE(worker_inbox.has_value()) << worker_inbox.error();
    ASSERT_EQ(worker_inbox->size(), 1u);
    const auto response =
        sh::PermissionSync::parse_response((*worker_inbox)[0].text);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->subtype, "success");
    EXPECT_EQ(response->request_id, request.request_id);

    app.reset();
    std::error_code ec;
    fs::remove_all(runtime_dir, ec);
    fs::remove_all(storage_root, ec);
}
