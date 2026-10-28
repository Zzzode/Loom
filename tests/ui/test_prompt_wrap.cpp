#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <gtest/gtest.h>

#include "test_ui_helpers.h"

import std;
import loom.ui.widgets.text_input;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.foundation.theme_provider;
import loom.ui.prompt.prompt_input_footer;

namespace {
namespace input = ::ui::components;
namespace repl = loom::ui::repl_screen;

input::TextInputOptions options(int columns = 10) {
    input::TextInputOptions out;
    out.columns = columns;
    out.show_line_numbers = false;
    out.cursor_blink_ms = 0;
    out.placeholder.clear();
    return out;
}

ftxui::Screen paint(input::TextInputImpl& value, int columns, int rows) {
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(columns), ftxui::Dimension::Fixed(rows));
    ftxui::Render(screen, value.RenderInputAreaPub());
    return screen;
}

std::string row_text(const ftxui::Screen& screen, int columns, int row) {
    std::string out;
    for (int x = 0; x < columns; ++x)
        out += screen.PixelAt(x, row).character;
    return out;
}
}  // namespace

TEST(PromptWrap, LongAsciiWrapsWithoutChangingSubmitBytes) {
    auto opts = options();
    std::string submitted;
    opts.on_submit = [&](const auto& text, const auto&) { submitted = text; };
    input::TextInputImpl value(opts);
    value.set_text("abcdefghijklmnopqrstuvwxyz");
    auto screen = paint(value, 10, 4);
    EXPECT_EQ(row_text(screen, 10, 0), "❯ abcdefgh");
    EXPECT_EQ(row_text(screen, 10, 1), "  ijklmnop");
    EXPECT_EQ(row_text(screen, 10, 2), "  qrstuvwx");
    EXPECT_EQ(row_text(screen, 10, 3), "  yz      ");
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{4, 3}));
    EXPECT_TRUE(value.HandleEvent(ftxui::Event::Return));
    EXPECT_EQ(submitted, "abcdefghijklmnopqrstuvwxyz");
}

TEST(PromptWrap, FullWidthEndCaretHasVisibleContinuationCell) {
    input::TextInputImpl value(options());
    value.set_text("abcdefgh");
    auto screen = paint(value, 10, 2);
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{2, 1}));
    EXPECT_TRUE(screen.PixelAt(2, 1).inverted);
    EXPECT_EQ(value.text(), "abcdefgh");
}

TEST(PromptWrap, ChineseEmojiAndCombiningMarksUseDisplayCells) {
    input::TextInputImpl value(options());
    value.set_text("中文😀ab中e\xcc\x81");
    auto screen = paint(value, 10, 2);
    EXPECT_NE(row_text(screen, 10, 0).find("中文😀ab"), std::string::npos);
    EXPECT_NE(row_text(screen, 10, 1).find("中e\xcc\x81"), std::string::npos);
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{5, 1}));
}

TEST(PromptWrap, ExplicitEmptyLinesRemainAndMiddleCaretUsesWrappedRow) {
    input::TextInputImpl value(options());
    value.set_text("abcdefghij\n\nklm");
    value.move_cursor(9 - value.cursor(), false);
    auto screen = paint(value, 10, 5);
    EXPECT_TRUE(screen.PixelAt(3, 1).inverted);
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{3, 1}));
    EXPECT_NE(row_text(screen, 10, 3).find("klm"), std::string::npos);
    EXPECT_EQ(value.cursor_line(), 0);
}

TEST(PromptWrap, ResizeReflowsHeldWidgetWithoutChangingSelectionOrBuffer) {
    input::TextInputImpl value(options());
    value.set_text("abcdefghijklmnop");
    value.move_cursor(-3, true);
    const auto selection = value.selection();
    input::RenderOptions update;
    update.columns = 8;
    value.update_render_options(update);
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{3, 2}));
    EXPECT_EQ(value.selection(), selection);
    EXPECT_EQ(value.text(), "abcdefghijklmnop");
    update.columns = 20;
    value.update_render_options(update);
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{15, 0}));
}

TEST(PromptWrap, SelectionAcrossWrapUsesSourceByteOffsets) {
    input::TextInputImpl value(options());
    value.set_text("abcdefghij");
    value.move_cursor(-4, true);
    auto screen = paint(value, 10, 2);
    const auto selection_color = loom::ui::design::theme::current_theme().palette->selection_bg;
    EXPECT_EQ(screen.PixelAt(8, 0).background_color, selection_color);
    EXPECT_EQ(screen.PixelAt(2, 1).background_color, selection_color);
    EXPECT_EQ(screen.PixelAt(3, 1).background_color, selection_color);
    EXPECT_NE(screen.PixelAt(7, 0).background_color, selection_color);
}

TEST(PromptWrap, MaskedUnicodeWrapAndCaretUseDisplayedMaskCells) {
    auto opts = options(6);
    opts.mask_input = true;
    input::TextInputImpl value(opts);
    value.set_text("中文😀abc");
    auto screen = paint(value, 6, 2);
    EXPECT_EQ(row_text(screen, 6, 0), "❯ ****");
    EXPECT_EQ(row_text(screen, 6, 1), "  **  ");
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{4, 1}));
}

TEST(PromptWrap, ImageChipInversionSurvivesSoftWrap) {
    input::TextInputImpl value(options());
    value.set_text("xx[Image #1]z");
    value.move_cursor(2 - value.cursor(), false);
    auto screen = paint(value, 10, 2);
    EXPECT_TRUE(screen.PixelAt(4, 0).inverted);
    EXPECT_TRUE(screen.PixelAt(2, 1).inverted);
    EXPECT_TRUE(screen.PixelAt(4, 1).inverted);
    EXPECT_TRUE(screen.PixelAt(5, 1).inverted);
    EXPECT_FALSE(screen.PixelAt(6, 1).inverted);
}

TEST(PromptWrap, BoundedViewportFollowsCaretAndAllowsDisplayRowNavigation) {
    auto opts = options();
    opts.max_visible_rows = 2;
    input::TextInputImpl value(opts);
    value.set_text("abcdefghijklmnopqrstuvwxyz");
    auto screen = paint(value, 10, 2);
    EXPECT_EQ(row_text(screen, 10, 0), "  qrstuvwx");
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{4, 1}));
    EXPECT_TRUE(value.move_visual_row(-1));
    EXPECT_EQ(value.cursor(), 18);
    EXPECT_TRUE(value.move_visual_row(-1));
    EXPECT_EQ(value.cursor(), 10);
    EXPECT_TRUE(value.move_visual_row(1));
    EXPECT_EQ(value.cursor(), 18);
    EXPECT_EQ(value.text(), "abcdefghijklmnopqrstuvwxyz");
}

TEST(PromptWrap, PrefixAndLineNumbersAreDeductedFromWidth) {
    auto opts = options(12);
    opts.prefix = "😀 ";
    opts.show_line_numbers = true;
    input::TextInputImpl value(opts);
    value.set_text("abcdefgh\n中");
    auto screen = paint(value, 12, 3);
    EXPECT_NE(row_text(screen, 12, 0).find("1 abcdef"), std::string::npos);
    EXPECT_NE(row_text(screen, 12, 1).find("gh"), std::string::npos);
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{8, 2}));
}

TEST(PromptWrap, UnconstrainedDefaultPreservesStandaloneExplicitLines) {
    input::TextInputImpl value(options(0));
    value.set_text("abcdefghijklmnop\n中文");
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{6, 1}));
    auto screen = paint(value, 30, 2);
    EXPECT_NE(row_text(screen, 30, 0).find("abcdefghijklmnop"), std::string::npos);
}

TEST(PromptWrap, RealReplCaretAndArrowNavigationShareGeometryAndHeldState) {
    auto state = std::make_shared<repl::ReplScreenState>();
    state->input_text = "abcdefghijklmnopqrstuvwxyz";
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(11), ftxui::Dimension::Fixed(4));
    ftxui::Render(screen, repl::RenderPromptInput(*state, 11, 2));
    const auto held = state->prompt_render_input;
    EXPECT_EQ(screen.cursor().x, 5);
    EXPECT_EQ(screen.cursor().y, 2);
    auto component = repl::ReplScreen(state, {});
    EXPECT_TRUE(component->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(state->input_cursor, 18u);
    ftxui::Render(screen, repl::RenderPromptInput(*state, 11, 2));
    EXPECT_EQ(state->prompt_render_input, held);
    EXPECT_EQ(state->input_text, "abcdefghijklmnopqrstuvwxyz");
    EXPECT_EQ(screen.cursor().x, 5);
    EXPECT_EQ(screen.cursor().y, 2);
}

TEST(PromptWrap, GhostAndArgumentHintsFitRemainingCellsWithoutMovingCaret) {
    auto opts = options(12);
    opts.inline_ghost_text = "中文abcdef";
    opts.argument_hint = "long argument hint";
    input::TextInputImpl value(opts);
    value.set_text("/go");
    auto screen = paint(value, 12, 1);
    EXPECT_EQ(row_text(screen, 12, 0), "❯ /go 中文ab");
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{5, 0}));
    EXPECT_TRUE(screen.PixelAt(5, 0).inverted);
    EXPECT_EQ(value.text(), "/go");
    opts.inline_ghost_text.clear();
    input::TextInputImpl hint(opts);
    hint.set_text("/go");
    screen = paint(hint, 12, 1);
    EXPECT_NE(row_text(screen, 12, 0).find("long"), std::string::npos);
    EXPECT_EQ(hint.text(), "/go");
}

TEST(PromptWrap, PlaceholderUsesBoundedCellsAndNeverEntersBuffer) {
    auto opts = options(9);
    opts.placeholder = "中文😀abcdefgh";
    input::TextInputImpl value(opts);
    auto screen = paint(value, 9, 1);
    EXPECT_NE(row_text(screen, 9, 0).find("中文😀a"), std::string::npos);
    EXPECT_EQ(value.text(), "");
    EXPECT_EQ(value.visual_cursor_position(), (std::pair{2, 0}));
}

TEST(PromptWrap, ReplHistoryAndAutocompleteKeepArrowPrecedence) {
    auto state = std::make_shared<repl::ReplScreenState>();
    state->input_history = {"old", "older"};
    auto component = repl::ReplScreen(state, {});
    EXPECT_TRUE(component->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(state->input_text, "older");
    EXPECT_TRUE(component->OnEvent(ftxui::Event::ArrowDown));
    EXPECT_TRUE(state->input_text.empty());
    state->input_text = "abcdefghijklmnopqrstuvwxyz";
    expect_element(repl::RenderPromptInput(*state, 11, 2));
    state->autocomplete_suggestions.push_back({});
    state->autocomplete_index = -1;
    EXPECT_TRUE(component->OnEvent(ftxui::Event::ArrowUp));
    EXPECT_EQ(state->autocomplete_index, 0);
    EXPECT_EQ(state->input_cursor, std::string::npos);
}

TEST(PromptWrap, ReplNativeCaretIncludesVimAndStashRows) {
    repl::ReplScreenState state;
    state.input_text = "abcdefghijklmnop";
    state.prompt_store.input_mode = repl::InputMode::VimInsert;
    state.prompt_store.stashed_prompt.emplace();
    state.prompt_store.stashed_prompt->text = "saved draft";
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(11), ftxui::Dimension::Fixed(7));
    ftxui::Render(screen, repl::RenderPromptInput(state, 11, 3));
    EXPECT_EQ(screen.cursor().x, 3);
    EXPECT_EQ(screen.cursor().y, 5);
    EXPECT_TRUE(screen.PixelAt(3, 5).inverted);
    EXPECT_EQ(state.input_text, "abcdefghijklmnop");
    EXPECT_EQ(state.prompt_store.stashed_prompt->text, "saved draft");
}

TEST(StatusLine, ContextWindowMetadataSupportsFullEngineRange) {
    namespace footer = loom::ui::prompt::footer;
    repl::ReplScreenState state;
    state.chrome_store.status_bar.context_window_size = 4000000000LL;
    footer::BuiltinStatusLineData data;
    data.context_window_size = state.chrome_store.status_bar.context_window_size;
    data.context_token_count = 200000000;
    const auto text =
        strip_ansi(render_to_plain_text(footer::RenderBuiltinStatusLine(data), 120, 1));
    EXPECT_NE(text.find("5% 200000.0K/4000000.0K"), std::string::npos);
    EXPECT_EQ(state.chrome_store.status_bar.context_window_size, 4000000000LL);
}
