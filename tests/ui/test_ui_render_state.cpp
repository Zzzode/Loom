/// @file test_ui_runtime.cpp
/// @brief Split from test_ui.cpp - AppRuntime, E2E_Gate, FullscreenLayout, Logo, PromptInput,
/// ReplScreen (SLOC budget fix)

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>
#include <httplib.h>

#include <cstdlib>

#include "test_ui_helpers.h"

import std;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.screens.messages_store;
import loom.ui.prompt.prompt_input;
import loom.ui.prompt.prompt_input_footer;
import loom.ui.chrome.fullscreen_layout;
import loom.ui.chrome.panels;
import loom.ui.messages.message_image;
import loom.ui.messages.virtual_list;
import loom.constants.constants;
import loom.ui.foundation.design_tokens;
import loom.ui.foundation.design_figures;
import loom.ui.foundation.theme_provider;
import loom.ui.widgets.components;
import loom.ui.widgets.all_components;
import loom.ui.messages.message_pipeline;
import loom.ui.messages.messages_list;
import loom.ui.messages.message_row;
import loom.ui.messages.message_tool_result;
import loom.ui.messages.message_bash_io;
import loom.ui.messages.system_text_message;
import loom.ui.messages.user_text_message;
import loom.ui.messages.assistant_text_message;
import loom.ui.foundation.declared_cursor;
import loom.ui.prompt.autocomplete_sources;
import loom.ui.features.teams.live_teammates;

namespace {
namespace fs = std::filesystem;
}

TEST(ReplScreen, SubmitsUtf8PromptOnReturn) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    std::optional<std::string> submitted;

    repl::ReplScreenCallbacks callbacks;
    callbacks.on_submit = [&](const std::string& text, repl::InputMode mode) {
        submitted = text;
        EXPECT_EQ(mode, repl::InputMode::Normal);
    };

    auto component = repl::ReplScreen(state, std::move(callbacks));
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character("你")));
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character("好")));
    EXPECT_EQ(state->input_text, "你好");

    EXPECT_TRUE(component->OnEvent(ftxui::Event::Return));
    ASSERT_TRUE(submitted.has_value());
    EXPECT_EQ(*submitted, "你好");
    EXPECT_TRUE(state->input_text.empty());
}

TEST(ReplScreen, TabAcceptsSelectedSlashSuggestion) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->input_text = "/a";
    state->autocomplete_suggestions = {
        {.display_text = "/add-dir",
         .description = "Add a working directory",
         .insert_text = "/add-dir ",
         .replacement_start = 0,
         .replacement_end = 2,
         .submit_on_return = true},
        {.display_text = "/agents",
         .description = "Manage agent configurations",
         .insert_text = "/agents ",
         .replacement_start = 0,
         .replacement_end = 2,
         .submit_on_return = true},
    };
    state->autocomplete_index = 1;

    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Tab));

    EXPECT_EQ(state->input_text, "/agents ");
    EXPECT_TRUE(state->autocomplete_suggestions.empty());
    EXPECT_EQ(state->autocomplete_index, -1);
}

TEST(ReplScreen, ReturnSubmitsSelectedSlashSuggestion) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->input_text = "/a";
    state->autocomplete_suggestions = {
        {.display_text = "/add-dir",
         .description = "Add a working directory",
         .insert_text = "/add-dir ",
         .replacement_start = 0,
         .replacement_end = 2,
         .submit_on_return = true},
        {.display_text = "/agents",
         .description = "Manage agent configurations",
         .insert_text = "/agents ",
         .replacement_start = 0,
         .replacement_end = 2,
         .submit_on_return = true},
    };
    state->autocomplete_index = 1;

    std::optional<std::string> submitted;
    repl::ReplScreenCallbacks callbacks;
    callbacks.on_submit = [&](const std::string& text, repl::InputMode mode) {
        submitted = text;
        EXPECT_EQ(mode, repl::InputMode::Normal);
    };

    auto component = repl::ReplScreen(state, std::move(callbacks));
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Return));

    ASSERT_TRUE(submitted.has_value());
    EXPECT_EQ(*submitted, "/agents ");
    EXPECT_TRUE(state->input_text.empty());
    EXPECT_TRUE(state->autocomplete_suggestions.empty());
    EXPECT_EQ(state->autocomplete_index, -1);
}

TEST(ReplScreen, CtrlNCtrlPNavigateAutocompleteWithWrapping) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->chrome_store.model_display_name = "GLM-5.2";
    state->cwd = "/tmp/cpp_migration";
    state->input_text = "/a";
    state->autocomplete_suggestions = {
        {.display_text = "/alpha",
         .description = "alpha",
         .insert_text = "/alpha",
         .replacement_start = 0,
         .replacement_end = 2},
        {.display_text = "/bravo",
         .description = "bravo",
         .insert_text = "/bravo",
         .replacement_start = 0,
         .replacement_end = 2},
        {.display_text = "/charlie",
         .description = "charlie",
         .insert_text = "/charlie",
         .replacement_start = 0,
         .replacement_end = 2},
    };
    state->autocomplete_index = -1;

    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});

    // Ctrl+P (\x10) from -1 wraps to the last suggestion (length-1 = 2),
    // mirroring handleAutocompletePrevious (useTypeahead.tsx:1242-1247).
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character("\x10")));
    EXPECT_EQ(state->autocomplete_index, 2);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character("\x10")));
    EXPECT_EQ(state->autocomplete_index, 1);
    // Ctrl+N (\x0e) moves forward.
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character("\x0e")));
    EXPECT_EQ(state->autocomplete_index, 2);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character("\x0e")));
    EXPECT_EQ(state->autocomplete_index, 0) << "Ctrl+N wraps from last suggestion back to 0";
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character("\x0e")));
    EXPECT_EQ(state->autocomplete_index, 1);

    // With zero suggestions Ctrl+N/Ctrl+P fall through unhandled and must
    // not touch input_text (TS useTypeahead.tsx:1341 early return).
    auto empty = std::make_shared<repl::ReplScreenState>();
    empty->input_text = "keep me";
    auto empty_component = repl::ReplScreen(empty, repl::ReplScreenCallbacks{});
    EXPECT_FALSE(empty_component->OnEvent(ftxui::Event::Character("\x0e")));
    EXPECT_FALSE(empty_component->OnEvent(ftxui::Event::Character("\x10")));
    EXPECT_EQ(empty->input_text, "keep me");
}

TEST(ReplScreen, EscapeDoublePressOnWhitespaceOnlyClearsWithoutHistory) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->chrome_store.model_display_name = "GLM-5.2";
    state->cwd = "/tmp/cpp_migration";

    std::string saved = "untouched";
    repl::ReplScreenCallbacks callbacks;
    callbacks.on_save_to_history = [&](const std::string& text) { saved = text; };
    auto component = repl::ReplScreen(state, std::move(callbacks));

    for (char c : std::string("   ")) {
        ASSERT_TRUE(component->OnEvent(ftxui::Event::Character(c)));
    }
    ASSERT_EQ(state->input_text, "   ");

    // TS addToHistory guard (useTextInput.ts:145): whitespace-only input is
    // cleared on double-Esc but NOT appended to history.
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Escape));
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Escape));
    EXPECT_TRUE(state->input_text.empty());
    EXPECT_EQ(saved, "untouched") << "whitespace-only text must not be saved to history";
}

TEST(ReplScreen, EscapeDismissesPopupThenArmsThenClears) {
    namespace repl = loom::ui::repl_screen;
    namespace pif = loom::ui::prompt::footer;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->chrome_store.model_display_name = "GLM-5.2";
    state->cwd = "/tmp/cpp_migration";
    state->input_text = "query";
    state->autocomplete_suggestions = {
        {.display_text = "query one", .description = "one", .insert_text = "query one"},
        {.display_text = "query two", .description = "two", .insert_text = "query two"},
    };

    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});

    // TS PromptInput.tsx disableEscapeDoublePress = suggestions.length>0:
    // Esc #1 dismisses the popup without arming.
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Escape));
    EXPECT_TRUE(state->autocomplete_suggestions.empty());
    EXPECT_EQ(state->autocomplete_index, -1);
    EXPECT_EQ(state->input_text, "query");
    // Esc #2 arms the clear hint.
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Escape));
    EXPECT_EQ(state->input_text, "query");
    // Esc #3 clears.
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Escape));
    EXPECT_TRUE(state->input_text.empty());

    // With empty input and no popup, a single Esc enqueues no hint.
    auto idle = std::make_shared<repl::ReplScreenState>();
    auto idle_component = repl::ReplScreen(idle, repl::ReplScreenCallbacks{});
    (void)idle_component->OnEvent(ftxui::Event::Escape);
    bool has_escape_hint = false;
    if (idle->footer_notification_queue.current) {
        has_escape_hint = idle->footer_notification_queue.current->key == "escape-again-to-clear";
    }
    EXPECT_FALSE(has_escape_hint);
    (void)pif::NotificationPriority::Immediate;
}

// Bug: "输入感叹号之后就没法退出这个 bash mode" — after typing '!' to enter bash
// mode (empty input), Backspace/Escape/Delete/Ctrl+U at cursor position 0 must
// exit back to Prompt mode.
// TS REF: src/components/PromptInput/PromptInput.tsx:1904-1908 —
//   `if (cursorOffset === 0 && (key.escape || key.backspace || key.delete ||
//        (key.ctrl && char === 'u'))) { onModeChange('prompt'); }`
TEST(ReplScreen, BashModeExitsOnBackspaceAtStart) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});

    // Type '!' into empty input → swallowed, flips to Bash mode (TS parity).
    ASSERT_TRUE(component->OnEvent(ftxui::Event::Character("!")));
    EXPECT_EQ(state->prompt_store.input_mode, repl::InputMode::Bash);
    EXPECT_TRUE(state->input_text.empty());  // '!' is a mode trigger, not stored

    // Backspace at cursor 0 must exit bash mode back to Prompt.
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Backspace));
    EXPECT_EQ(state->prompt_store.input_mode, repl::InputMode::Normal);
}

TEST(ReplScreen, BashModeExitsOnEscapeAndDeleteAndCtrlUAtStart) {
    namespace repl = loom::ui::repl_screen;

    // Escape exits bash mode.
    {
        auto state = std::make_shared<repl::ReplScreenState>();
        auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
        ASSERT_TRUE(component->OnEvent(ftxui::Event::Character("!")));
        ASSERT_EQ(state->prompt_store.input_mode, repl::InputMode::Bash);
        component->OnEvent(ftxui::Event::Escape);
        EXPECT_EQ(state->prompt_store.input_mode, repl::InputMode::Normal);
    }
    // Delete exits bash mode.
    {
        auto state = std::make_shared<repl::ReplScreenState>();
        auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
        ASSERT_TRUE(component->OnEvent(ftxui::Event::Character("!")));
        ASSERT_EQ(state->prompt_store.input_mode, repl::InputMode::Bash);
        component->OnEvent(ftxui::Event::Delete);
        EXPECT_EQ(state->prompt_store.input_mode, repl::InputMode::Normal);
    }
    // Ctrl+U (\x15) exits bash mode.
    {
        auto state = std::make_shared<repl::ReplScreenState>();
        auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
        ASSERT_TRUE(component->OnEvent(ftxui::Event::Character("!")));
        ASSERT_EQ(state->prompt_store.input_mode, repl::InputMode::Bash);
        component->OnEvent(ftxui::Event::Character("\x15"));
        EXPECT_EQ(state->prompt_store.input_mode, repl::InputMode::Normal);
    }
}

TEST(ReplScreen, BashModeBackspaceMidTextDoesNotExitMode) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});

    // Enter bash mode, then type a command so cursor is NOT at 0.
    ASSERT_TRUE(component->OnEvent(ftxui::Event::Character("!")));
    ASSERT_EQ(state->prompt_store.input_mode, repl::InputMode::Bash);
    component->OnEvent(ftxui::Event::Character("l"));
    component->OnEvent(ftxui::Event::Character("s"));
    ASSERT_EQ(state->input_text, "ls");

    // Backspace mid-text deletes a char and stays in bash mode (cursor != 0).
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Backspace));
    EXPECT_EQ(state->input_text, "l");
    EXPECT_EQ(state->prompt_store.input_mode, repl::InputMode::Bash);
}

TEST(ReplScreen, MouseWheelScrollsTranscript) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->messages_store.viewport_height_lines = 8;

    repl::MessageDisplayEntry message;
    message.is_local_command_output = true;
    for (int i = 0; i < 40; ++i) {
        message.content_preview += std::format("line-{:02}", i);
        if (i != 39) message.content_preview += '\n';
    }
    state->messages_store.messages.push_back(std::move(message));

    // Fresh state: scroll_pinned_to_bottom=true, scroll_offset=0.
    // 40 lines + 1 margin = 41 rows; viewport=8; max_offset=33.
    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
    ftxui::Mouse wheel;

    // WheelUp from pinned-to-bottom must scroll UP (away from bottom).
    // Before the fix, scroll_offset stayed 0 and the view never moved.
    wheel.button = ftxui::Mouse::WheelUp;
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
    EXPECT_GT(state->messages_store.scroll_offset, 0);
    EXPECT_FALSE(state->messages_store.scroll_pinned_to_bottom);

    // WheelDown scrolls back toward the bottom.
    const int after_up = state->messages_store.scroll_offset;
    wheel.button = ftxui::Mouse::WheelDown;
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
    EXPECT_GT(state->messages_store.scroll_offset, after_up);
    EXPECT_TRUE(state->messages_store.scroll_pinned_to_bottom);
}

TEST(ReplScreen, MouseWheelScrollsVirtualTranscript) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    // >80 entries triggers the virtual renderer (RFC 0008 hysteresis).
    // transcript renderer used by resumed histories.
    for (int i = 0; i < 100; ++i) {
        repl::MessageDisplayEntry message;
        message.id = std::format("virtual-wheel-{:03}", i);
        message.role = "assistant";
        message.content_preview = std::format("virtual message {:03}", i);
        state->messages_store.messages.push_back(std::move(message));
    }

    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, component->Render());
    ASSERT_TRUE(state->messages_store.virtual_list_active);
    ASSERT_TRUE(state->messages_store.scroll_pinned_to_bottom);

    ftxui::Mouse wheel;
    wheel.button = ftxui::Mouse::WheelUp;
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
    EXPECT_FALSE(state->messages_store.scroll_pinned_to_bottom);

    wheel.button = ftxui::Mouse::WheelDown;
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
    EXPECT_TRUE(state->messages_store.scroll_pinned_to_bottom);
}

TEST(ReplScreen, NewMessagesPillClickRePinsTranscript) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->messages_store.scroll_pinned_to_bottom = false;
    state->messages_store.scroll_offset = 0;
    state->messages_store.divider_index = 0;
    state->messages_store.pill_visible = true;
    state->messages_store.unseen_message_count = 7;
    // RFC 0008: pre-set Virtual mode so the hysteresis decision doesn't
    // trigger a mode-switch reset_to_bottom (which would clear the pill
    // before the test can click it).
    state->messages_store.virtual_renderer_mode =
        loom::ui::messages_list::VirtualRendererMode::Virtual;

    // >80 entries exercises the virtualized transcript
    // path used by resumed long sessions.
    for (int i = 0; i < 100; ++i) {
        repl::MessageDisplayEntry message;
        message.id = std::format("pill-click-{:02}", i);
        message.role = "assistant";
        message.content_preview = std::string(80, 'x');
        state->messages_store.messages.push_back(std::move(message));
    }

    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, component->Render());

    ASSERT_TRUE(state->messages_store.virtual_list_active);
    const auto& box = state->messages_store.new_messages_pill_box;
    ASSERT_LE(box.x_min, box.x_max);
    ASSERT_LE(box.y_min, box.y_max);

    ftxui::Mouse click;
    click.button = ftxui::Mouse::Left;
    click.motion = ftxui::Mouse::Released;
    click.x = (box.x_min + box.x_max) / 2;
    click.y = (box.y_min + box.y_max) / 2;
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Mouse("", click)));

    EXPECT_TRUE(state->messages_store.scroll_pinned_to_bottom);
    EXPECT_FALSE(state->messages_store.pill_visible);
    EXPECT_EQ(state->messages_store.unseen_message_count, 0);
    EXPECT_FALSE(state->messages_store.divider_index.has_value());
    EXPECT_FALSE(state->messages_store.unseen_divider.has_value());
}

TEST(ReplScreen, NewMessagesPillKeyboardShortcutRePinsTranscript) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->messages_store.scroll_pinned_to_bottom = false;
    state->messages_store.scroll_offset = 0;
    state->messages_store.divider_index = 0;
    state->messages_store.pill_visible = true;
    state->messages_store.unseen_message_count = 7;
    // RFC 0008: pre-set Virtual mode so the hysteresis decision doesn't
    // trigger a mode-switch reset_to_bottom (which would clear the pill
    // before the test can use the keyboard shortcut).
    state->messages_store.virtual_renderer_mode =
        loom::ui::messages_list::VirtualRendererMode::Virtual;

    for (int i = 0; i < 100; ++i) {
        repl::MessageDisplayEntry message;
        message.id = std::format("pill-kbd-{:02}", i);
        message.role = "assistant";
        message.content_preview = std::string(80, 'x');
        state->messages_store.messages.push_back(std::move(message));
    }

    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, component->Render());
    ASSERT_TRUE(state->messages_store.virtual_list_active);

    // Ctrl+Down is the keyboard equivalent of clicking the pill.
    EXPECT_TRUE(component->OnEvent(ftxui::Event::ArrowDownCtrl));

    EXPECT_TRUE(state->messages_store.scroll_pinned_to_bottom);
    EXPECT_FALSE(state->messages_store.pill_visible);
    EXPECT_EQ(state->messages_store.unseen_message_count, 0);
    EXPECT_FALSE(state->messages_store.divider_index.has_value());
    EXPECT_FALSE(state->messages_store.unseen_divider.has_value());
}

TEST(ReplScreen, CtrlDownNoOpWhenPinnedToBottom) {
    namespace repl = loom::ui::repl_screen;

    auto state = std::make_shared<repl::ReplScreenState>();
    // Already pinned to bottom — Ctrl+Down should not consume the event.
    state->messages_store.scroll_pinned_to_bottom = true;

    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
    EXPECT_FALSE(component->OnEvent(ftxui::Event::ArrowDownCtrl));
    EXPECT_TRUE(state->messages_store.scroll_pinned_to_bottom);
}

// ── CommonMark soft/hard break normalization tests ──────────────────────
// These verify that CountWordWrappedLines matches the Markdown renderer's
// split_on_hard_breaks semantics: soft breaks join with space, hard breaks
// force a new line, blank lines are paragraph boundaries, and continuation
// indentation is stripped.

TEST(ReplScreen, WordWrapSoftBreakJoinsWithSpace) {
    namespace repl = loom::ui::repl_screen;
    // "a\nb" — soft break → joined with space → 1 line at 80 cols
    EXPECT_EQ(repl::CountWordWrappedLines("a\nb", 80), 1);
}

TEST(ReplScreen, WordWrapHardBreakSpacesForcesNewLine) {
    namespace repl = loom::ui::repl_screen;
    // "a  \nb" — hard break (two trailing spaces) → 2 lines
    EXPECT_EQ(repl::CountWordWrappedLines("a  \nb", 80), 2);
}

TEST(ReplScreen, WordWrapHardBreakBackslashForcesNewLine) {
    namespace repl = loom::ui::repl_screen;
    // "a\\\nb" — hard break (trailing backslash) → 2 lines
    EXPECT_EQ(repl::CountWordWrappedLines("a\\\nb", 80), 2);
}

TEST(ReplScreen, WordWrapParagraphBoundaryCountsBlankLine) {
    namespace repl = loom::ui::repl_screen;
    // "a\n\nb" — paragraph boundary → 3 lines (a, blank, b)
    EXPECT_EQ(repl::CountWordWrappedLines("a\n\nb", 80), 3);
}

TEST(ReplScreen, WordWrapContinuationIndentStripped) {
    namespace repl = loom::ui::repl_screen;
    // "a\n   b" — leading spaces on continuation line stripped → 1 line
    EXPECT_EQ(repl::CountWordWrappedLines("a\n   b", 80), 1);
}

TEST(ReplScreen, WordWrapMultipleParagraphs) {
    namespace repl = loom::ui::repl_screen;
    // Two paragraphs with soft breaks within each
    const std::string text = "first para line one\nfirst para line two\n\n"
                             "second para line one\nsecond para line two";
    // At 80 cols: para1 = 1 line (joined), blank = 1, para2 = 1 line → 3
    EXPECT_EQ(repl::CountWordWrappedLines(text, 80), 3);
}

TEST(ReplScreen, WordWrapParagraphWithHardBreak) {
    namespace repl = loom::ui::repl_screen;
    // Paragraph with a hard break inside, then a new paragraph
    const std::string text = "line one  \nline two\n\nsecond paragraph";
    // line one (hard break) = 1, line two = 1, blank = 1, second = 1 → 4
    EXPECT_EQ(repl::CountWordWrappedLines(text, 80), 4);
}

TEST(ReplScreen, WordWrapConsecutiveBlankLinesCollapsed) {
    namespace repl = loom::ui::repl_screen;
    // "a\n\n\nb" — consecutive blanks collapse to one paragraph boundary
    // → 3 lines (a, blank, b), not 4
    EXPECT_EQ(repl::CountWordWrappedLines("a\n\n\nb", 80), 3);
}

TEST(ReplScreen, WordWrapLeadingBlankStripped) {
    namespace repl = loom::ui::repl_screen;
    // "\na" — leading blank run is stripped by the lexer
    EXPECT_EQ(repl::CountWordWrappedLines("\na", 80), 1);
}

TEST(ReplScreen, WordWrapBlankOnlyReturnsOne) {
    namespace repl = loom::ui::repl_screen;
    // "\n" — only a blank line, lexer produces no paragraphs
    EXPECT_EQ(repl::CountWordWrappedLines("\n", 80), 1);
}

TEST(ReplScreen, WordWrapTrailingBlankStripped) {
    namespace repl = loom::ui::repl_screen;
    // "a\n\n" — trailing blank run is stripped by the lexer
    EXPECT_EQ(repl::CountWordWrappedLines("a\n\n", 80), 1);
}

TEST(ReplScreen, WordWrapHardBreakAtEofNoExtraLine) {
    namespace repl = loom::ui::repl_screen;
    // "a  \n" — hard break at EOF: renderer ends the row but does not
    // create an extra empty row.  Estimator must not add a phantom line.
    EXPECT_EQ(repl::CountWordWrappedLines("a  \n", 80), 1);
    // "a\\\n" — same for backslash hard break.
    EXPECT_EQ(repl::CountWordWrappedLines("a\\\n", 80), 1);
}

TEST(ReplScreen, WordWrapLongParagraphLastLineReachable) {
    namespace repl = loom::ui::repl_screen;
    // 200 "word " at 40 cols — the last line must be counted so the
    // scroll offset can reach it (regression: hard-wrap underestimated).
    std::string text;
    for (int i = 0; i < 200; ++i)
        text += "word ";
    text += "THEEND";
    const int lines = repl::CountWordWrappedLines(text, 40);
    // 200 "word " = 1000 chars + "THEEND" = 1006 chars
    // At 40 cols, word-wrap gives ~29 lines (each line fits ~8 "word " tokens)
    EXPECT_GE(lines, 25);
    EXPECT_LE(lines, 35);
}

TEST(ReplScreen, EstimateTranscriptRowsAssistantParagraphs) {
    namespace repl = loom::ui::repl_screen;
    // Assistant message with two paragraphs — the blank line must count
    // as a separator so max_offset is large enough to reach the end.
    std::vector<repl::MessageDisplayEntry> entries;
    repl::MessageDisplayEntry msg;
    msg.role = "assistant";
    msg.content_preview = "First paragraph with enough text to wrap at 40 cols.\n\n"
                          "Second paragraph also with enough text to wrap at 40 cols.";
    entries.push_back(std::move(msg));

    const int estimate = repl::EstimateTranscriptRows(entries, 40);
    // At 40 cols: para1 wraps to ~2 lines, blank = 1, para2 wraps to ~2 lines
    // + 1 separator = ~6 rows.  Must be >= 5 (not collapsed to 2-3).
    EXPECT_GE(estimate, 5);
}

// ── Virtual path term_cols threading tests ──────────────────────────────
// These verify that the virtual-path height estimator respects the real
// terminal width: a long paragraph must produce a taller estimate at 40 cols
// than at 120 cols.  Regression: the virtual path hardcoded term_cols=120,
// causing scroll-bounds underestimation on narrow terminals.

TEST(ReplScreen, VirtualEstimateRowHeightRespectsTermCols) {
    namespace ml = loom::ui::messages_list;

    // A paragraph long enough to wrap at both 40 and 120 cols.
    std::string text;
    for (int i = 0; i < 30; ++i)
        text += "hello world ";
    // ~360 chars → at 120 cols wraps to ~3 lines, at 40 cols wraps to ~9.

    ml::MessagesListInput input;
    ml::AssistantTextMessageData data;
    data.content = text;
    input.rows.push_back(ml::MessageRowPayload{std::move(data)});
    input.shapes.push_back(ml::MessageShape::AssistantText);

    auto visible = ml::build_visible_rows(input);
    ASSERT_EQ(visible.size(), 1u);

    const int est_40 = ml::detail::estimate_row_height(visible[0], input, 40);
    const int est_120 = ml::detail::estimate_row_height(visible[0], input, 120);

    // Narrow terminal must produce a taller (or equal) estimate.
    EXPECT_GT(est_40, est_120) << "40-col estimate should exceed 120-col for wrapping text";
}

TEST(ReplScreen, UserPromptHeightEstimatesMatchCellsWidthAndExplicitNewlines) {
    namespace ml = loom::ui::messages_list;
    namespace repl = loom::ui::repl_screen;
    namespace msg = loom::ui::messages;
    const std::vector<std::pair<std::string, int>> cases{
        {"站在全球顶尖 UI/UX 设计师的视角看，你觉得我们现在的 Terminal UI 设计有什么问题吗？", 120},
        {"中文abc中文abc", 10},
        {"first\n\n第二行\n", 120},
        {std::string(96, 'x'), 40},
    };
    for (const auto& [content, width] : cases) {
        SCOPED_TRACE(std::to_string(width) + ": " + content);
        ml::MessagesListInput input;
        msg::UserTextMessageData data;
        data.content = content;
        input.rows.push_back(ml::MessageRowPayload{data});
        input.shapes.push_back(ml::MessageShape::UserPrompt);
        const auto visible = ml::build_visible_rows(input);
        ASSERT_EQ(visible.size(), 1u);
        auto screen =
            ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(12));
        ftxui::Render(screen,
                      ftxui::vbox({msg::RenderUserPromptMessage(data), ftxui::text("AFTER")}));
        int rendered_rows = -1;
        for (int y = 0; y < screen.dimy(); ++y) {
            if (screen.PixelAt(0, y).character == "A") rendered_rows = y;
        }
        ASSERT_GE(rendered_rows, 2);
        EXPECT_EQ(ml::detail::estimate_row_height(visible[0], input, width), rendered_rows);
        repl::MessageDisplayEntry entry;
        entry.role = "user";
        entry.content_preview = content;
        EXPECT_EQ(repl::EstimateTranscriptRows({entry}, width), rendered_rows);
    }
}

TEST(ReplScreen, VirtualEstimateRowHeightSameForShortText) {
    namespace ml = loom::ui::messages_list;

    // Short text that fits on one line at any width ≥ 20.
    ml::MessagesListInput input;
    ml::AssistantTextMessageData data;
    data.content = "short";
    input.rows.push_back(ml::MessageRowPayload{std::move(data)});
    input.shapes.push_back(ml::MessageShape::AssistantText);

    auto visible = ml::build_visible_rows(input);
    ASSERT_EQ(visible.size(), 1u);

    const int est_40 = ml::detail::estimate_row_height(visible[0], input, 40);
    const int est_120 = ml::detail::estimate_row_height(visible[0], input, 120);

    // Both should be equal (1 content line + envelope header).
    EXPECT_EQ(est_40, est_120);
}

TEST(ReplScreen, VirtualVisibleRowsToVirtualRespectsTermCols) {
    namespace ml = loom::ui::messages_list;

    // A paragraph long enough to wrap differently at 40 vs 120 cols.
    std::string text;
    for (int i = 0; i < 30; ++i)
        text += "hello world ";

    ml::MessagesListInput input;
    ml::AssistantTextMessageData data;
    data.content = text;
    input.rows.push_back(ml::MessageRowPayload{std::move(data)});
    input.shapes.push_back(ml::MessageShape::AssistantText);

    auto visible = ml::build_visible_rows(input);
    ASSERT_EQ(visible.size(), 1u);

    auto virt_40 = ml::visible_rows_to_virtual(visible, input, 40);
    auto virt_120 = ml::visible_rows_to_virtual(visible, input, 120);
    ASSERT_EQ(virt_40.size(), 1u);
    ASSERT_EQ(virt_120.size(), 1u);

    // The virtual row's estimated height must reflect the terminal width.
    EXPECT_GT(virt_40[0].estimated_height_lines, virt_120[0].estimated_height_lines)
        << "40-col virtual estimate should exceed 120-col";
}

// ── Code block height estimation tests ──────────────────────────────────
// These verify that the normalizer preserves line breaks in fenced and
// indented code blocks so the estimator counts each code line as a separate
// row — matching the renderer's render_code_block (one vbox row per line).

TEST(ReplScreen, WordWrapFencedCodeBlockPreservesLines) {
    namespace repl = loom::ui::repl_screen;
    // Fenced code block: each line must count as a separate row, not be
    // joined with spaces (soft-break).
    const std::string text = "```python\n"
                             "def hello():\n"
                             "    print(\"hello\")\n"
                             "```";
    const int lines = repl::CountWordWrappedLines(text, 80);
    // 2 code lines → 2 rows (fence delimiters not counted).
    EXPECT_EQ(lines, 2);
}

TEST(ReplScreen, WordWrapIndentedCodeBlockPreservesLines) {
    namespace repl = loom::ui::repl_screen;
    // Indented code block after blank line: each line must count as a
    // separate row, not be joined with spaces.
    const std::string text = "Some text.\n"
                             "\n"
                             "    code line 1\n"
                             "    code line 2\n"
                             "    code line 3\n"
                             "\n"
                             "More text.";
    const int lines = repl::CountWordWrappedLines(text, 80);
    // "Some text." (1) + blank (1) + 3 code lines + blank (1) + "More text." (1)
    // = 7 lines minimum.  Without the fix, code lines would be joined to ~3.
    EXPECT_GE(lines, 6);
}

TEST(ReplScreen, WordWrapFencedCodeBlockWithWrappingContent) {
    namespace repl = loom::ui::repl_screen;
    // Long code lines should still wrap at the content width.
    std::string text = "```\n";
    for (int i = 0; i < 20; ++i)
        text += "word ";
    text += "\n```";
    const int lines = repl::CountWordWrappedLines(text, 40);
    // 20 "word " = 100 chars → at 40 cols wraps to ~3 lines.
    EXPECT_GE(lines, 2);
    EXPECT_LE(lines, 5);
}

TEST(ReplScreen, WordWrapIndentedCodeBlockNoFalsePositiveInParagraph) {
    namespace repl = loom::ui::repl_screen;
    // 4-space indent WITHOUT a preceding blank line is paragraph continuation,
    // NOT a code block (CommonMark: indented code blocks cannot interrupt
    // a paragraph).  The soft-break join with space still applies.
    const std::string text = "First line\n"
                             "    continued with indent";
    const int lines = repl::CountWordWrappedLines(text, 80);
    // Soft break → joined with space → 1 line at 80 cols.
    EXPECT_EQ(lines, 1);
}

// ── Code block boundary tests (P0 from Dev-0 review of b255155) ─────────
// The lexer flushes the paragraph before a fenced opener and emits code
// blocks as separate tokens; the renderer inserts a blank separator row
// between consecutive blocks.  The normalizer must replicate this so the
// height estimate counts the separator row — otherwise the last line is
// unreachable.

TEST(ReplScreen, WordWrapFencedCodeBlockOpenerSeparator) {
    namespace repl = loom::ui::repl_screen;
    // Prose before a fenced opener must produce a block separator row.
    // Without the opener boundary, "before" and "code" are joined by
    // soft-break → 2 lines instead of 3.
    const std::string text = "before\n"
                             "```\n"
                             "code\n"
                             "```";
    const int lines = repl::CountWordWrappedLines(text, 80);
    // "before" (1) + separator (1) + "code" (1) = 3.
    EXPECT_EQ(lines, 3);
}

TEST(ReplScreen, WordWrapFencedCodeBlockClosingSeparator) {
    namespace repl = loom::ui::repl_screen;
    // Prose after a closing fence must produce a block separator row.
    // Without the closing boundary, "code" and "after" are joined on
    // the same line → 1 line, making the last line unreachable.
    const std::string text = "```\n"
                             "code\n"
                             "```\n"
                             "after";
    const int lines = repl::CountWordWrappedLines(text, 80);
    // "code" (1) + separator (1) + "after" (1) = 3.
    EXPECT_EQ(lines, 3);
}

TEST(ReplScreen, WordWrapIndentedCodeBlockEndsAtProse) {
    namespace repl = loom::ui::repl_screen;
    // An indented code block followed by non-indented prose must produce
    // a block separator.  Without it, "code" and "after" are joined.
    const std::string text = "\n"
                             "    code\n"
                             "after";
    const int lines = repl::CountWordWrappedLines(text, 80);
    // "code" (1) + separator (1) + "after" (1) = 3.
    EXPECT_EQ(lines, 3);
}

TEST(ReplScreen, WordWrapFencedCodeBlockClosingFenceWithTail) {
    namespace repl = loom::ui::repl_screen;
    // A closing fence with non-whitespace tail is NOT a closing fence
    // (CommonMark §4.5: "the closing code fence may be followed only by
    // spaces").  The line and everything after it stay in the code block.
    // Without the tail check, the fence closes early, the remaining lines
    // are soft-break joined → 3 lines instead of 4.
    const std::string text = "```\n"
                             "code\n"
                             "```not a close\n"
                             "line two\n"
                             "line three\n"
                             "```";
    const int lines = repl::CountWordWrappedLines(text, 80);
    // 3 code lines: "code", "```not a close", "line two", "line three"
    // — wait, that's 4.  Without the tail check the fake close ends the
    // block and "line two line three" joins → 3.
    EXPECT_EQ(lines, 4);
}

TEST(ReplScreen, WordWrapIndentedCodeBlockAtDocumentStart) {
    namespace repl = loom::ui::repl_screen;
    // Indented code block at the very start of the document (no preceding
    // blank line, no prose) must be detected: CommonMark allows indented
    // code blocks to begin the document when no paragraph is open.
    // Without the first_content check, the 4-space indent is stripped and
    // the lines are soft-joined → 1 line instead of 2.
    const std::string text = "    code one\n"
                             "    code two";
    const int lines = repl::CountWordWrappedLines(text, 80);
    // 2 code lines → 2 rows.
    EXPECT_EQ(lines, 2);
}

// ── Adaptive wheel stride tests ─────────────────────────────────────────

TEST(ReplScreen, WheelStrideScalesWithViewport) {
    namespace repl = loom::ui::repl_screen;
    // 10% of viewport height, clamped to [3, 15].
    EXPECT_EQ(repl::WheelStride(8), 3);     // 8/10=0 → min 3
    EXPECT_EQ(repl::WheelStride(30), 3);    // 30/10=3 → min 3
    EXPECT_EQ(repl::WheelStride(40), 4);    // 40/10=4
    EXPECT_EQ(repl::WheelStride(60), 6);    // 60/10=6
    EXPECT_EQ(repl::WheelStride(100), 10);  // 100/10=10
    EXPECT_EQ(repl::WheelStride(150), 15);  // 150/10=15 → max 15
    EXPECT_EQ(repl::WheelStride(200), 15);  // 200/10=20 → clamped to 15
}

TEST(ReplScreen, WheelStrideHandlesZero) {
    namespace repl = loom::ui::repl_screen;
    // viewport_height_lines=0 before first render → clamp to 1 → stride 3.
    EXPECT_EQ(repl::WheelStride(0), 3);
    EXPECT_EQ(repl::WheelStride(-1), 3);
}

// ── RFC 0008: Virtual renderer hysteresis lifecycle tests ──────────────
// These tests exercise the full ReplScreen::Render() → RenderMessages →
// render_messages_list_view path, asserting on persistent MessagesStore
// state (mode, counter, force_bottom flag) AND rendered screen content
// after each frame.

namespace {

namespace rs = loom::ui::repl_screen;

/// Add `count` assistant messages with unique IDs to the store.
void AddTestMessages(rs::MessagesStore& store, int count, std::string_view prefix) {
    for (int i = 0; i < count; ++i) {
        rs::MessageDisplayEntry message;
        message.id = std::format("{}-{:03}", prefix, i);
        message.role = "assistant";
        message.content_preview = std::format("{} message {:03}", prefix, i);
        store.messages.push_back(std::move(message));
    }
}

/// Render one frame through the full ReplScreen pipeline and return the
/// stripped screen text for visual assertions.
std::string RenderFrame(const std::shared_ptr<rs::ReplScreenState>& state) {
    auto component = rs::ReplScreen(state, rs::ReplScreenCallbacks{});
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, component->Render());
    return strip_ansi(screen.ToString());
}

/// Create a ReplScreen component for event-driven tests (wheel, click).
[[nodiscard]] ftxui::Component MakeComponent(const std::shared_ptr<rs::ReplScreenState>& state) {
    return rs::ReplScreen(state, rs::ReplScreenCallbacks{});
}

/// Render through an existing component and return stripped screen text.
std::string RenderThrough(const ftxui::Component& component) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, component->Render());
    return strip_ansi(screen.ToString());
}

/// Build a single tall local-command-output message with `lines` lines.
/// The last line contains `tail_marker` for visual bottom-anchor assertions.
rs::MessageDisplayEntry MakeTallMessage(int lines, std::string_view id,
                                        std::string_view tail_marker) {
    rs::MessageDisplayEntry message;
    message.id = id;
    message.role = "user";
    message.is_local_command_output = true;
    std::string content;
    for (int i = 0; i < lines - 1; ++i) {
        content += std::format("output line {:03}\n", i);
    }
    content += tail_marker;
    message.content_preview = std::move(content);
    return message;
}

}  // namespace

TEST(ReplScreen, VirtualModeSwitchStaticToVirtualThroughRender) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    auto state = std::make_shared<repl::ReplScreenState>();
    AddTestMessages(state->messages_store, 81, "enter");

    RenderFrame(state);

    // Mode switched Static→Virtual, counter incremented.
    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Virtual);
    EXPECT_EQ(state->messages_store.virtual_mode_switch_count, 1u);
    EXPECT_TRUE(state->messages_store.virtual_list_active);
    // Pinned (default) → no scroll reset → force_bottom NOT set.
    EXPECT_FALSE(state->messages_store.force_bottom_after_mode_switch);
}

TEST(ReplScreen, VirtualModeSwitchVirtualToStaticThroughRender) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->messages_store.virtual_renderer_mode = VirtualRendererMode::Virtual;
    AddTestMessages(state->messages_store, 59, "exit");

    RenderFrame(state);

    // Mode switched Virtual→Static, counter incremented.
    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Static);
    EXPECT_EQ(state->messages_store.virtual_mode_switch_count, 1u);
    EXPECT_FALSE(state->messages_store.virtual_list_active);
}

TEST(ReplScreen, VirtualHysteresisBandThroughRender) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    auto state = std::make_shared<repl::ReplScreenState>();

    // Frame 1: 81 messages → Static→Virtual.
    AddTestMessages(state->messages_store, 81, "band");
    RenderFrame(state);
    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Virtual);
    EXPECT_EQ(state->messages_store.virtual_mode_switch_count, 1u);

    // Frame 2: reduce to 79 (inside band [60,80]) → stays Virtual.
    state->messages_store.messages.resize(79);
    RenderFrame(state);
    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Virtual);
    EXPECT_EQ(state->messages_store.virtual_mode_switch_count, 1u);

    // Frame 3: reduce to 59 (< 60) → Virtual→Static.
    state->messages_store.messages.resize(59);
    RenderFrame(state);
    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Static);
    EXPECT_EQ(state->messages_store.virtual_mode_switch_count, 2u);
}

TEST(ReplScreen, VirtualModeSwitchNotPinnedResetsScrollToBottom) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    auto state = std::make_shared<repl::ReplScreenState>();
    // User scrolled up → not pinned, pill visible.
    state->messages_store.scroll_pinned_to_bottom = false;
    state->messages_store.scroll_offset = 5;
    state->messages_store.pill_visible = true;
    state->messages_store.unseen_message_count = 3;
    state->messages_store.divider_index = 10;
    AddTestMessages(state->messages_store, 81, "reset");

    RenderFrame(state);

    // Mode switched.
    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Virtual);
    // Scroll was reset to bottom (same-frame reset).
    EXPECT_TRUE(state->messages_store.scroll_pinned_to_bottom);
    // Pill / divider state cleared by ClearRepinState.
    EXPECT_FALSE(state->messages_store.pill_visible);
    EXPECT_EQ(state->messages_store.unseen_message_count, 0);
    EXPECT_FALSE(state->messages_store.divider_index.has_value());
    // Force-bottom flag set for subsequent frames.
    EXPECT_TRUE(state->messages_store.force_bottom_after_mode_switch);
}

TEST(ReplScreen, VirtualModeSwitchPinnedDoesNotSetForceBottom) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    auto state = std::make_shared<repl::ReplScreenState>();
    // Pinned (default) — mode switch should NOT set force_bottom.
    AddTestMessages(state->messages_store, 81, "pinned");

    RenderFrame(state);

    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Virtual);
    EXPECT_FALSE(state->messages_store.force_bottom_after_mode_switch);
}

TEST(ReplScreen, ForceBottomFlagClearedOnWheelScrollAway) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->messages_store.virtual_renderer_mode = VirtualRendererMode::Virtual;
    state->messages_store.force_bottom_after_mode_switch = true;
    AddTestMessages(state->messages_store, 100, "wheel");

    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
    ftxui::Render(screen, component->Render());
    ASSERT_TRUE(state->messages_store.scroll_pinned_to_bottom);
    ASSERT_TRUE(state->messages_store.force_bottom_after_mode_switch);

    // Wheel up → scroll away → flag cleared.
    ftxui::Mouse wheel;
    wheel.button = ftxui::Mouse::WheelUp;
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
    EXPECT_FALSE(state->messages_store.scroll_pinned_to_bottom);
    EXPECT_FALSE(state->messages_store.force_bottom_after_mode_switch);
}

TEST(ReplScreen, ForceBottomFlagSurvivesNewContent) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->messages_store.virtual_renderer_mode = VirtualRendererMode::Virtual;
    state->messages_store.force_bottom_after_mode_switch = true;
    AddTestMessages(state->messages_store, 81, "survive");

    // Render with flag set.
    RenderFrame(state);
    EXPECT_TRUE(state->messages_store.force_bottom_after_mode_switch);

    // Add more content (simulating streaming) and render again.
    AddTestMessages(state->messages_store, 10, "survive-new");
    RenderFrame(state);
    // Flag should survive — it's only cleared by explicit pin=false events.
    EXPECT_TRUE(state->messages_store.force_bottom_after_mode_switch);
    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Virtual);
}

TEST(ReplScreen, VirtualModeSwitchCountTracksMultipleTransitions) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    auto state = std::make_shared<repl::ReplScreenState>();

    // Static→Virtual (81 messages).
    AddTestMessages(state->messages_store, 81, "count");
    RenderFrame(state);
    EXPECT_EQ(state->messages_store.virtual_mode_switch_count, 1u);

    // Virtual→Static (59 messages).
    state->messages_store.messages.resize(59);
    RenderFrame(state);
    EXPECT_EQ(state->messages_store.virtual_mode_switch_count, 2u);

    // Static→Virtual again (81 messages).
    AddTestMessages(state->messages_store, 22, "count-more");
    RenderFrame(state);
    EXPECT_EQ(state->messages_store.virtual_mode_switch_count, 3u);
    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Virtual);
}

// ── Visual bottom-anchor assertions (Dev-0/Dev-2 P1: store state alone
//     cannot prove the screen is scrolled to bottom) ───────────────────

TEST(ReplScreen, VirtualToStaticSingleTallMessageShowsTail) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->messages_store.virtual_renderer_mode = VirtualRendererMode::Virtual;
    state->messages_store.scroll_pinned_to_bottom = false;
    // Single tall message: 200 lines of local command output.
    state->messages_store.messages.push_back(MakeTallMessage(200, "tall-001", "TAIL_MARKER"));

    // Frame 1: Virtual→Static switch (visible=1 < 60). Unpinned →
    // scroll_was_reset_to_bottom → force_bottom_after_mode_switch = true.
    auto text = RenderFrame(state);
    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Static);
    EXPECT_EQ(state->messages_store.virtual_mode_switch_count, 1u);
    EXPECT_TRUE(state->messages_store.force_bottom_after_mode_switch);
    // The tail must be visible on the switch frame.
    EXPECT_NE(text.find("TAIL_MARKER"), std::string::npos);

    // Frame 2: no input, force_bottom still active → tail stays visible.
    text = RenderFrame(state);
    EXPECT_NE(text.find("TAIL_MARKER"), std::string::npos);

    // Frame 3: still anchored.
    text = RenderFrame(state);
    EXPECT_NE(text.find("TAIL_MARKER"), std::string::npos);
}

TEST(ReplScreen, StaticToVirtualSwitchShowsTail) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    auto state = std::make_shared<repl::ReplScreenState>();
    AddTestMessages(state->messages_store, 81, "tail-visible");

    // Frame 1: Static→Virtual switch.
    auto text = RenderFrame(state);
    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Virtual);
    // Last message must be visible (pinned to bottom by default).
    EXPECT_NE(text.find("tail-visible message 080"), std::string::npos);

    // Frame 2: still virtual, still pinned → tail visible.
    text = RenderFrame(state);
    EXPECT_NE(text.find("tail-visible message 080"), std::string::npos);
}

TEST(ReplScreen, StaticToVirtualUnpinnedSwitchResetsToBottom) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    auto state = std::make_shared<repl::ReplScreenState>();
    AddTestMessages(state->messages_store, 81, "unpinned-tail");
    // User has scrolled up (not pinned to bottom).
    state->messages_store.scroll_pinned_to_bottom = false;

    // Frame 1: Static→Virtual switch. Unpinned → same-frame scroll reset
    // to bottom → force_bottom_after_mode_switch = true.
    auto text = RenderFrame(state);
    EXPECT_EQ(state->messages_store.virtual_renderer_mode, VirtualRendererMode::Virtual);
    EXPECT_TRUE(state->messages_store.force_bottom_after_mode_switch);
    // Last message must be visible despite being unpinned before switch.
    EXPECT_NE(text.find("unpinned-tail message 080"), std::string::npos);

    // Frame 2: force_bottom still active → tail stays visible.
    text = RenderFrame(state);
    EXPECT_NE(text.find("unpinned-tail message 080"), std::string::npos);
}

TEST(ReplScreen, WheelUpMovesScreenAwayFromBottom) {
    namespace repl = loom::ui::repl_screen;
    using loom::ui::messages_list::VirtualRendererMode;

    // Exercise the virtual path separately from static transcript geometry.
    auto state = std::make_shared<repl::ReplScreenState>();
    state->messages_store.virtual_renderer_mode = VirtualRendererMode::Virtual;
    AddTestMessages(state->messages_store, 100, "wheel-away");

    auto component = MakeComponent(state);
    auto text = RenderThrough(component);
    ASSERT_TRUE(state->messages_store.virtual_list_active);
    ASSERT_TRUE(state->messages_store.scroll_pinned_to_bottom);
    // Last message visible when pinned.
    ASSERT_NE(text.find("wheel-away message 099"), std::string::npos);

    // Wheel up repeatedly → scroll away from bottom.
    ftxui::Mouse wheel;
    wheel.button = ftxui::Mouse::WheelUp;
    for (int i = 0; i < 20; ++i) {
        component->OnEvent(ftxui::Event::Mouse("", wheel));
    }
    EXPECT_FALSE(state->messages_store.scroll_pinned_to_bottom);

    // Next frame: last message must NOT be visible (user scrolled up).
    text = RenderThrough(component);
    EXPECT_EQ(text.find("wheel-away message 099"), std::string::npos);

    // One more frame: position preserved (no snap-back to bottom).
    text = RenderThrough(component);
    EXPECT_EQ(text.find("wheel-away message 099"), std::string::npos);
}

TEST(ReplScreen, FirstWheelUpMovesStaticChineseMarkdownFromBottom) {
    auto state = std::make_shared<rs::ReplScreenState>();
    rs::MessageDisplayEntry prompt;
    prompt.id = "scroll-prompt";
    prompt.role = "user";
    prompt.content_preview = "Review this interface";
    state->messages_store.messages.push_back(std::move(prompt));
    rs::MessageDisplayEntry answer;
    answer.id = "scroll-answer";
    answer.role = "assistant";
    for (int i = 0; i < 60; ++i) {
        answer.content_preview += std::format("段落 {:02}：", i);
        for (int j = 0; j < 25; ++j)
            answer.content_preview += "中文";
        answer.content_preview += "\n\n";
    }
    answer.content_preview += "SCROLL_TAIL_MARKER";
    state->messages_store.messages.push_back(std::move(answer));
    auto component = MakeComponent(state);
    auto before = RenderThrough(component);
    before = RenderThrough(component);
    ASSERT_FALSE(state->messages_store.virtual_list_active);
    ASSERT_NE(before.find("SCROLL_TAIL_MARKER"), std::string::npos);
    ftxui::Mouse wheel;
    wheel.button = ftxui::Mouse::WheelUp;
    ASSERT_TRUE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
    auto after = RenderThrough(component);
    EXPECT_NE(before, after);
    EXPECT_FALSE(state->messages_store.scroll_pinned_to_bottom);
    EXPECT_EQ(after.find("SCROLL_TAIL_MARKER"), std::string::npos);
    EXPECT_EQ(RenderThrough(component), after);
    wheel.button = ftxui::Mouse::WheelDown;
    ASSERT_TRUE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
    EXPECT_NE(RenderThrough(component).find("SCROLL_TAIL_MARKER"), std::string::npos);
    EXPECT_TRUE(state->messages_store.scroll_pinned_to_bottom);
}

TEST(ReplScreen, StaticScrollBoundsIgnoreCollapsedToolOutput) {
    auto state = std::make_shared<rs::ReplScreenState>();
    for (int i = 0; i < 12; ++i) {
        rs::MessageDisplayEntry call;
        call.id = std::format("scroll-tool-{}", i);
        call.role = "assistant";
        call.is_tool_use = true;
        call.tool_name = "Bash";
        call.tool_status = "success";
        call.tool_use_id = call.id;
        call.tool_input_json = R"({"command":"printf test"})";
        state->messages_store.messages.push_back(call);
        rs::MessageDisplayEntry result;
        result.id = std::format("scroll-result-{}", i);
        result.role = "tool";
        result.tool_name = "Bash";
        result.tool_use_id = call.id;
        for (int line = 0; line < 100; ++line)
            result.content_preview += "Long output hidden by the completed tool chain\n";
        state->messages_store.messages.push_back(std::move(result));
    }
    rs::MessageDisplayEntry answer;
    answer.id = "tool-scroll-answer";
    answer.role = "assistant";
    answer.content_preview = "```text\n";
    for (int i = 0; i < 60; ++i)
        answer.content_preview += std::format("review line {:02}\n", i);
    answer.content_preview += "TOOL_SCROLL_TAIL\n```";
    state->messages_store.messages.push_back(std::move(answer));
    auto component = MakeComponent(state);
    auto before = RenderThrough(component);
    before = RenderThrough(component);
    ASSERT_FALSE(state->messages_store.virtual_list_active);
    ASSERT_NE(before.find("TOOL_SCROLL_TAIL"), std::string::npos);
    ftxui::Mouse wheel;
    wheel.button = ftxui::Mouse::WheelUp;
    ASSERT_TRUE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
    auto after = RenderThrough(component);
    EXPECT_NE(before, after);
    EXPECT_EQ(after.find("TOOL_SCROLL_TAIL"), std::string::npos);
    EXPECT_EQ(RenderThrough(component), after);
    ASSERT_TRUE(component->OnEvent(ftxui::Event::ArrowDownCtrl));
    EXPECT_NE(RenderThrough(component).find("TOOL_SCROLL_TAIL"), std::string::npos);
    EXPECT_TRUE(state->messages_store.scroll_pinned_to_bottom);
}

TEST(ReplScreen, StaticPausedTranscriptScrollsAfterResize) {
    auto state = std::make_shared<rs::ReplScreenState>();
    rs::MessageDisplayEntry answer;
    answer.id = "resizing-answer";
    answer.role = "assistant";
    for (int i = 0; i < 60; ++i) {
        answer.content_preview += std::format("review {:02}: ", i);
        for (int j = 0; j < 30; ++j)
            answer.content_preview += "中文 ";
        answer.content_preview += "\n\n";
    }
    answer.content_preview += "RESIZE_SCROLL_TAIL";
    state->messages_store.messages.push_back(std::move(answer));
    rs::MessageDisplayEntry pause;
    pause.id = "resizing-pause";
    pause.role = "system";
    pause.content_preview = "Tool-round budget reached (1). Progress saved.";
    pause.can_continue = true;
    state->messages_store.messages.push_back(std::move(pause));
    rs::ReplScreenCallbacks callbacks;
    callbacks.on_continue = [] {};
    auto component = rs::ReplScreen(state, std::move(callbacks));
    const auto render = [&](int width, int height) {
        return strip_ansi(render_to_plain_text(component->Render(), width, height));
    };
    std::optional<int> previous_height;
    for (const auto [width, height] : {std::pair{120, 40}, {72, 24}, {140, 42}}) {
        SCOPED_TRACE(std::format("{}x{}", width, height));
        auto before = render(width, height);
        before = render(width, height);
        ASSERT_FALSE(state->messages_store.virtual_list_active);
        ASSERT_NE(before.find("RESIZE_SCROLL_TAIL"), std::string::npos);
        ASSERT_NE(before.find("Continue from saved progress"), std::string::npos);
        ASSERT_TRUE(state->messages_store.rendered_content_height_lines);
        if (previous_height)
            EXPECT_NE(*state->messages_store.rendered_content_height_lines, *previous_height);
        previous_height = state->messages_store.rendered_content_height_lines;
        const auto viewport = state->messages_store.viewport_height_lines;
        EXPECT_LT(viewport, state->messages_store.scrollable_box.y_max -
                                state->messages_store.scrollable_box.y_min + 1);
        ftxui::Mouse wheel;
        wheel.button = ftxui::Mouse::WheelUp;
        ASSERT_TRUE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
        auto after = render(width, height);
        EXPECT_NE(before, after);
        EXPECT_EQ(state->messages_store.viewport_height_lines, viewport);
        EXPECT_EQ(render(width, height), after);
        wheel.button = ftxui::Mouse::WheelDown;
        ASSERT_TRUE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
        EXPECT_NE(render(width, height).find("RESIZE_SCROLL_TAIL"), std::string::npos);
        EXPECT_TRUE(state->messages_store.scroll_pinned_to_bottom);
    }
    state->messages_store.messages.clear();
    rs::reset_virtual_renderer_state(state->messages_store);
    EXPECT_FALSE(state->messages_store.rendered_content_height_lines);
    render(120, 40);
    render(120, 40);
    ftxui::Mouse wheel;
    wheel.button = ftxui::Mouse::WheelUp;
    EXPECT_FALSE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
}

TEST(ReplScreen, VirtualAssistantSpacingMatchesStaticMessagesAndUserContinuations) {
    namespace ml = loom::ui::messages_list;
    for (const int columns : {40, 60, 120}) {
        for (const int scenario : {0, 1, 2, 3, 4, 5}) {
            SCOPED_TRACE(columns);
            SCOPED_TRACE(scenario);
            ml::MessagesListInput input;
            input.term_cols = columns;
            input.viewport_rows = 24;
            input.disable_chain_compression = true;
            if (scenario != 0) {
                for (const auto content : {"USER-FIRST", "USER-CONTINUATION"}) {
                    ml::UserTextMessageData user;
                    user.content = content;
                    input.rows.push_back(user);
                    input.shapes.push_back(ml::MessageShape::UserPrompt);
                }
            }
            if (scenario == 2) {
                loom::ui::messages::image::ImageMessageData image;
                image.source = "fixture.png";
                image.file_name = "fixture.png";
                image.media_type = "image/png";
                input.rows.insert(input.rows.begin() + 1, image);
                input.shapes.insert(input.shapes.begin() + 1, ml::MessageShape::UserImage);
            } else if (scenario == 3 || scenario == 4) {
                if (scenario == 3) {
                    ml::ToolResultOptions result;
                    result.tool_name = "Bash";
                    result.output = "TOOL-RESULT";
                    input.rows.insert(input.rows.begin() + 1, result);
                    input.shapes.insert(input.shapes.begin() + 1, ml::MessageShape::UserToolResult);
                } else {
                    ml::BashIOEntry result;
                    result.content = "TOOL-RESULT";
                    input.rows.insert(input.rows.begin() + 1, result);
                    input.shapes.insert(input.shapes.begin() + 1, ml::MessageShape::UserBashOutput);
                }
            } else if (scenario == 5) {
                ml::AssistantTextMessageData hidden;
                hidden.content = "COMPACTED";
                input.rows.insert(input.rows.begin() + 1, hidden);
                input.shapes.insert(input.shapes.begin() + 1, ml::MessageShape::AssistantText);
                input.compact_boundary_groups.emplace_back(1, 2);
            }
            for (const auto content : {"ASSISTANT-FIRST", "ASSISTANT-NEXT", "ASSISTANT-LAST"}) {
                ml::AssistantTextMessageData assistant;
                assistant.content = content;
                input.rows.push_back(assistant);
                input.shapes.push_back(ml::MessageShape::AssistantText);
            }
            auto draw = [&](bool use_virtual) {
                auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(columns),
                                                    ftxui::Dimension::Fixed(24));
                auto element = use_virtual ? ml::render_messages_list_virtual(input, 0, 24, 0)
                                           : ml::render_messages_list_view(input, 0);
                ftxui::Render(screen, std::move(element));
                return strip_ansi(screen.ToString());
            };
            const auto ordinary = draw(false);
            const auto row_of = [&](std::string_view marker) {
                const auto at = ordinary.find(marker);
                EXPECT_NE(at, std::string::npos) << ordinary;
                if (at == std::string::npos) return -1;
                return static_cast<int>(std::count(ordinary.begin(), ordinary.begin() + at, '\n'));
            };
            // Independent fixed coordinates preserve the pre-existing static
            // contract even if both callers accidentally change together.
            constexpr std::array first_assistant{1, 4, 6, 6, 9, 6};
            EXPECT_EQ(row_of("ASSISTANT-FIRST"), first_assistant[scenario]);
            EXPECT_EQ(row_of("ASSISTANT-NEXT"), first_assistant[scenario] + 2);
            EXPECT_EQ(row_of("ASSISTANT-LAST"), first_assistant[scenario] + 4);
            if (scenario != 0) {
                EXPECT_EQ(row_of("USER-FIRST"), 1);
                constexpr std::array continuation{0, 2, 4, 4, 7, 4};
                EXPECT_EQ(row_of("USER-CONTINUATION"), continuation[scenario]);
            }
            if (scenario == 2) {
                EXPECT_EQ(row_of("[Image]"), 2);
                EXPECT_EQ(row_of("fixture.png"), 3);
            }
            if (scenario == 3 || scenario == 4)
                EXPECT_EQ(row_of("TOOL-RESULT"), scenario == 3 ? 2 : 5);
            if (scenario == 5) EXPECT_EQ(row_of("messages collapsed"), 3);
            EXPECT_EQ(ordinary.find("bad payload"), std::string::npos);
            EXPECT_EQ(draw(true), ordinary);
            EXPECT_EQ(draw(true), ordinary);
        }
        // A short list can still overflow through wrapped prompt content.
        // Exercise both sides of the conditional inner-gutter boundary.
        for (const int viewport : {3, 24}) {
            ml::MessagesListInput input;
            input.term_cols = columns;
            input.pin_to_bottom = true;
            input.disable_chain_compression = true;
            std::vector<std::shared_ptr<ml::SelectionRowGeometry>> geometry;
            input.selection_rows = &geometry;
            ml::UserTextMessageData user;
            user.content = std::string(columns - 4, 'x') + " end";
            input.rows.push_back(user);
            input.shapes.push_back(ml::MessageShape::UserPrompt);
            ml::AssistantTextMessageData assistant;
            assistant.content = "BOUNDARY-TAIL";
            input.rows.push_back(assistant);
            input.shapes.push_back(ml::MessageShape::AssistantText);
            auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(columns),
                                                ftxui::Dimension::Fixed(viewport));
            ftxui::Render(screen, ml::render_messages_list_virtual(input, 0, viewport, 0));
            ASSERT_FALSE(geometry.empty());
            const auto& box = geometry.back()->box;
            EXPECT_EQ(box.x_max - box.x_min + 1, columns - (viewport == 3 ? 2 : 1));
            EXPECT_NE(strip_ansi(screen.ToString()).find("BOUNDARY-TAIL"), std::string::npos);
        }
    }
}

TEST(ReplScreen, ConsecutiveAssistantVirtualHistoryKeepsTailAndScrollsOnFirstEventAcrossResize) {
    namespace repl = loom::ui::repl_screen;
    auto state = std::make_shared<repl::ReplScreenState>();
    AddTestMessages(state->messages_store, 120, "HISTORY");
    repl::MessageDisplayEntry notice;
    notice.id = "history-completion";
    notice.role = "system";
    notice.system_subtype = "background_task";
    notice.content_preview = "Agent completed · history-check";
    state->messages_store.messages.push_back(std::move(notice));
    auto component = MakeComponent(state);
    auto draw = [&](int columns) {
        ScopedEnvVar terminal_columns("COLUMNS");
        ScopedEnvVar terminal_rows("LINES");
        terminal_columns.set(std::to_string(columns));
        terminal_rows.set("40");
        auto screen =
            ftxui::Screen::Create(ftxui::Dimension::Fixed(columns), ftxui::Dimension::Fixed(40));
        ftxui::Render(screen, component->Render());
        return strip_ansi(screen.ToString());
    };
    for (const int columns : {120, 60, 40, 120}) {
        SCOPED_TRACE(columns);
        // Seed the measured viewport, as the App's startup render does.
        (void)draw(columns);
        const auto pinned = draw(columns);
        ASSERT_TRUE(state->messages_store.virtual_list_active);
        EXPECT_EQ(state->messages_store.virtual_jh.total(), 242);
        ASSERT_FALSE(state->messages_store.selection_rows.empty());
        const auto& body_box = state->messages_store.selection_rows.back()->box;
        // The native scroll indicator and the virtual ASCII gutter each
        // reserve one column; the transcript owns every remaining cell.
        EXPECT_EQ(body_box.x_max - body_box.x_min + 1, columns - 2);
        EXPECT_NE(pinned.find("HISTORY message 119"), std::string::npos);
        EXPECT_NE(pinned.find("Agent completed"), std::string::npos);
        EXPECT_EQ(draw(columns), pinned);
        ftxui::Mouse wheel;
        wheel.button = ftxui::Mouse::WheelUp;
        wheel.x = state->messages_store.scrollable_box.x_min + 1;
        wheel.y = state->messages_store.scrollable_box.y_min + 1;
        EXPECT_TRUE(component->OnEvent(ftxui::Event::Mouse("", wheel)));
        EXPECT_FALSE(state->messages_store.scroll_pinned_to_bottom);
        EXPECT_NE(draw(columns), pinned);
        EXPECT_TRUE(component->OnEvent(ftxui::Event::PageUp));
        EXPECT_FALSE(state->messages_store.scroll_pinned_to_bottom);
        EXPECT_NE(draw(columns), pinned);
        EXPECT_TRUE(component->OnEvent(ftxui::Event::Special("\x1b[1;5B")));
        EXPECT_TRUE(state->messages_store.scroll_pinned_to_bottom);
        EXPECT_EQ(draw(columns), pinned);
    }
}

TEST(ReplScreen, SystemEventVirtualHeightMatchesFaithfulSingleLineSummaryForEverySubtype) {
    namespace msg = loom::ui::messages;
    namespace ml = loom::ui::messages_list;
    using ST = msg::SystemMessageSubtype;
    for (const auto subtype :
         {ST::Plain, ST::TurnDuration, ST::MemorySaved, ST::BridgeStatus, ST::ThinkingSummary,
          ST::HookSummary, ST::ModelSwitch, ST::AwaySummary, ST::BackgroundTask, ST::Other}) {
        for (const int columns : {40, 60, 120}) {
            SCOPED_TRACE(static_cast<int>(subtype));
            SCOPED_TRACE(columns);
            msg::SystemTextMessageData event;
            event.subtype = subtype;
            event.summary = "Resuming session: history";
            ml::MessagesListInput input;
            input.rows.push_back(event);
            input.shapes.push_back(ml::MessageShape::SystemText);
            const auto visible = ml::build_visible_rows(input);
            ASSERT_EQ(visible.size(), 1u);
            auto screen =
                ftxui::Screen::Create(ftxui::Dimension::Fixed(columns), ftxui::Dimension::Fixed(8));
            ftxui::Render(screen, ftxui::vbox({msg::RenderSystemTextMessageFaithful(event),
                                               ftxui::text("AFTER-EVENT")}));
            int after_row = -1;
            for (int y = 0; y < screen.dimy(); ++y)
                if (screen.PixelAt(0, y).character == "A") after_row = y;
            EXPECT_EQ(after_row, 2);
            EXPECT_EQ(ml::detail::estimate_row_height(visible[0], input, columns), after_row);
        }
    }
}

TEST(ReplScreen, RestoredAssistantHistorySystemEventsKeepLatestReplyVisible) {
    namespace repl = loom::ui::repl_screen;
    for (const int columns : {40, 60, 120}) {
        SCOPED_TRACE(columns);
        ScopedEnvVar terminal_columns("COLUMNS");
        ScopedEnvVar terminal_rows("LINES");
        terminal_columns.set(std::to_string(columns));
        terminal_rows.set("30");
        auto state = std::make_shared<repl::ReplScreenState>();
        AddTestMessages(state->messages_store, 120, "HISTORY");
        auto append = [&](std::string id, std::string role, std::string text,
                          std::optional<std::string> subtype = std::nullopt) {
            repl::MessageDisplayEntry entry;
            entry.id = std::move(id);
            entry.role = std::move(role);
            entry.content_preview = std::move(text);
            entry.system_subtype = std::move(subtype);
            state->messages_store.messages.push_back(std::move(entry));
        };
        append("completed", "system", "Agent completed · history-check", "background_task");
        append("resume", "system", "Resuming session: history");
        append("followup", "user",
               "真实端点验收：上面的120条Assistant消息和完成通知是明确标注的UI测试数据。"
               "不要调用工具，只回复长历史已确认。");
        append("thinking", "assistant", "Confirm the history fixture.");
        state->messages_store.messages.back().is_thinking = true;
        state->messages_store.messages.back().full_content = "Confirm the history fixture.";
        append("reply", "assistant", "长历史已确认");
        auto component = MakeComponent(state);
        for (int frame = 0; frame < 4; ++frame) {
            auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(columns),
                                                ftxui::Dimension::Fixed(30));
            ftxui::Render(screen, component->Render());
            const auto text = strip_ansi(screen.ToString());
            EXPECT_TRUE(state->messages_store.virtual_list_active);
            // The initial native frame establishes the viewport geometry.
            if (frame != 0) EXPECT_NE(text.find("● 长历史已确认"), std::string::npos) << text;
        }
    }
}
