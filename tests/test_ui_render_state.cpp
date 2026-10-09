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
        {.display_text = "/add-dir", .description = "Add a working directory",
         .insert_text = "/add-dir ", .replacement_start = 0, .replacement_end = 2,
         .submit_on_return = true},
        {.display_text = "/agents", .description = "Manage agent configurations",
         .insert_text = "/agents ", .replacement_start = 0, .replacement_end = 2,
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
        {.display_text = "/add-dir", .description = "Add a working directory",
         .insert_text = "/add-dir ", .replacement_start = 0, .replacement_end = 2,
         .submit_on_return = true},
        {.display_text = "/agents", .description = "Manage agent configurations",
         .insert_text = "/agents ", .replacement_start = 0, .replacement_end = 2,
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
        {.display_text = "/alpha", .description = "alpha",
         .insert_text = "/alpha",
         .replacement_start = 0, .replacement_end = 2},
        {.display_text = "/bravo", .description = "bravo",
         .insert_text = "/bravo",
         .replacement_start = 0, .replacement_end = 2},
        {.display_text = "/charlie", .description = "charlie",
         .insert_text = "/charlie",
         .replacement_start = 0, .replacement_end = 2},
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
    EXPECT_EQ(state->autocomplete_index, 0)
        << "Ctrl+N wraps from last suggestion back to 0";
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character("\x0e")));
    EXPECT_EQ(state->autocomplete_index, 1);

    // With zero suggestions Ctrl+N/Ctrl+P fall through unhandled and must
    // not touch input_text (TS useTypeahead.tsx:1341 early return).
    auto empty = std::make_shared<repl::ReplScreenState>();
    empty->input_text = "keep me";
    auto empty_component =
        repl::ReplScreen(empty, repl::ReplScreenCallbacks{});
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
    EXPECT_EQ(saved, "untouched")
        << "whitespace-only text must not be saved to history";
}



TEST(ReplScreen, EscapeDismissesPopupThenArmsThenClears) {
    namespace repl = loom::ui::repl_screen;
    namespace pif = loom::ui::prompt::footer;

    auto state = std::make_shared<repl::ReplScreenState>();
    state->chrome_store.model_display_name = "GLM-5.2";
    state->cwd = "/tmp/cpp_migration";
    state->input_text = "query";
    state->autocomplete_suggestions = {
        {.display_text = "query one", .description = "one",
         .insert_text = "query one"},
        {.display_text = "query two", .description = "two",
         .insert_text = "query two"},
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
        has_escape_hint =
            idle->footer_notification_queue.current->key ==
            "escape-again-to-clear";
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
    // More than kBigChatThreshold entries selects the long-session virtual
    // transcript renderer used by resumed histories.
    for (int i = 0; i < 100; ++i) {
        repl::MessageDisplayEntry message;
        message.id = std::format("virtual-wheel-{:03}", i);
        message.role = "assistant";
        message.content_preview = std::format("virtual message {:03}", i);
        state->messages_store.messages.push_back(std::move(message));
    }

    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120),
                                        ftxui::Dimension::Fixed(40));
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

    // More than kBigChatThreshold entries exercises the virtualized transcript
    // path used by resumed long sessions.
    for (int i = 0; i < 100; ++i) {
        repl::MessageDisplayEntry message;
        message.id = std::format("pill-click-{:02}", i);
        message.role = "assistant";
        message.content_preview = std::string(80, 'x');
        state->messages_store.messages.push_back(std::move(message));
    }

    auto component = repl::ReplScreen(state, repl::ReplScreenCallbacks{});
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120),
                                        ftxui::Dimension::Fixed(40));
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
    for (int i = 0; i < 200; ++i) text += "word ";
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
    for (int i = 0; i < 30; ++i) text += "hello world ";
    // ~360 chars → at 120 cols wraps to ~3 lines, at 40 cols wraps to ~9.

    ml::MessagesListInput input;
    ml::AssistantTextMessageData data;
    data.content = text;
    input.rows.push_back(ml::MessageRowPayload{std::move(data)});
    input.shapes.push_back(ml::MessageShape::AssistantText);

    auto visible = ml::build_visible_rows(input);
    ASSERT_EQ(visible.size(), 1u);

    const int est_40  = ml::detail::estimate_row_height(visible[0], input, 40);
    const int est_120 = ml::detail::estimate_row_height(visible[0], input, 120);

    // Narrow terminal must produce a taller (or equal) estimate.
    EXPECT_GT(est_40, est_120)
        << "40-col estimate should exceed 120-col for wrapping text";
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

    const int est_40  = ml::detail::estimate_row_height(visible[0], input, 40);
    const int est_120 = ml::detail::estimate_row_height(visible[0], input, 120);

    // Both should be equal (1 content line + envelope header).
    EXPECT_EQ(est_40, est_120);
}

TEST(ReplScreen, VirtualVisibleRowsToVirtualRespectsTermCols) {
    namespace ml = loom::ui::messages_list;

    // A paragraph long enough to wrap differently at 40 vs 120 cols.
    std::string text;
    for (int i = 0; i < 30; ++i) text += "hello world ";

    ml::MessagesListInput input;
    ml::AssistantTextMessageData data;
    data.content = text;
    input.rows.push_back(ml::MessageRowPayload{std::move(data)});
    input.shapes.push_back(ml::MessageShape::AssistantText);

    auto visible = ml::build_visible_rows(input);
    ASSERT_EQ(visible.size(), 1u);

    auto virt_40  = ml::visible_rows_to_virtual(visible, input, 40);
    auto virt_120 = ml::visible_rows_to_virtual(visible, input, 120);
    ASSERT_EQ(virt_40.size(), 1u);
    ASSERT_EQ(virt_120.size(), 1u);

    // The virtual row's estimated height must reflect the terminal width.
    EXPECT_GT(virt_40[0].estimated_height_lines, virt_120[0].estimated_height_lines)
        << "40-col virtual estimate should exceed 120-col";
}
