// text_input_render.cpp - impl unit for loom.ui.widgets.text_input
// (RFC 0001 Phase C batch 8). All FTXUI rendering: TextInputImpl::Render and
// the input-area caret/multiline/selection painter (incl. the [Image #N]
// chip inversion that needs loom.text.parse_references), the suggestions
// dropdown, reverse-search panel, paste-preview overlay, the two public
// render primitives (RenderInputAreaPub / RenderSuggestionsFromListPub),
// cursor_display_col, and the free TextInput() component factory.
// suggest_util::category_color is implemented here; category_icon stays inline.
//
// Phase A (#184957): textual FTXUI dom/component headers + `import std;`
// (the textual FTXUI includes keep the reduced-BMI writer happy).
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/screen/string.hpp>  // for string_width

module loom.ui.widgets.text_input;

import std;

import loom.ui.foundation.design_figures;
import loom.ui.foundation.theme_provider;

import loom.text.parse_references;
import loom.ui.prompt.prompt_paste_handler;
import loom.ui.prompt.placeholder_cascade;

namespace ui::components {

Color suggest_util::category_color(SuggestionCategory cat) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    switch (cat) {
        case SuggestionCategory::Command:      return pal.info;
        case SuggestionCategory::File:         return pal.text;
        case SuggestionCategory::Directory:    return pal.text;
        case SuggestionCategory::History:      return pal.muted;
        case SuggestionCategory::Agent:        return pal.primary;
        case SuggestionCategory::MCPResource:  return pal.info;
        case SuggestionCategory::Shell:        return pal.bash_border;
        case SuggestionCategory::SlackChannel: return pal.info;
        case SuggestionCategory::CustomTitle:  return pal.info;
        case SuggestionCategory::None:         return pal.text;
        default:                               return pal.text;
    }
}

// ------------------------------------------------------------
// Cursor display position (for declared cursor / IME support)
// ------------------------------------------------------------
/// Display column (visual width) of the cursor on its line.
/// Unlike `compute_cursor_position` which returns byte offset, this
/// accounts for full-width CJK characters that occupy 2 terminal cells.
int TextInputImpl::cursor_display_col() const {
    int line = 0, byte_col = 0;
    compute_cursor_position(line, byte_col);
    // Find the line's start byte
    int line_start = 0;
    for (int i = 0; i < line; ++i) {
        // Advance past this line and its newline
        while (line_start < (int)text_.size() && text_[line_start] != '\n')
            ++line_start;
        ++line_start;  // skip the '\n'
    }
    int line_end = line_start;
    while (line_end < (int)text_.size() && text_[line_end] != '\n')
        ++line_end;
    const std::string line_str = text_.substr(line_start, byte_col);
    return string_width(line_str);
}

// ------------------------------------------------------------
// Rendering (Element output)
// ------------------------------------------------------------
Element TextInputImpl::Render() {
    using namespace ftxui;
    refresh_blink();

    // --- Search mode (reverse history search) ---
    if (search_mode_) {
        return RenderSearchMode();
    }

    Element input_el = RenderInputArea();

    // --- Paste preview confirmation overlay (GAP 1) ---
    // When a large paste (> 10000 chars) is pending confirmation,
    // show the preview box below the input area.
    if (paste_preview_) {
        Element preview = RenderPastePreviewOverlay();
        return vbox({
            input_el,
            preview,
        });
    }

    // --- Suggestions dropdown overlay ---
    if (showing_suggestions_ && !suggestions_.empty()) {
        Element dropdown = RenderSuggestionsDropdown();
        return vbox({
            input_el,
            dropdown,
        });
    }

    return input_el;
}

/// Render the paste preview confirmation overlay.
/// Shows line count + "(large paste - press Enter to confirm)" + a
/// 200-char snippet of the truncated content.
Element TextInputImpl::RenderPastePreviewOverlay() const {
    if (!paste_preview_) return ftxui::text("");
    return loom::ui::prompt::render_paste_preview(*paste_preview_);
}

/// Render just the input/caret/multiline/selection area (no dropdown).
Element TextInputImpl::RenderInputAreaPub() { return RenderInputArea(); }

/// Render a suggestions dropdown from an externally-supplied list.
/// `selected` is clamped to [0, suggestions.size()-1]; -1 disables.
/// Used by repl_screen to surface autocomplete_suggestions inline.
Element TextInputImpl::RenderSuggestionsFromListPub(const std::vector<Suggestion>& sugs,
                                                     int selected) {
    // Temporarily install the supplied list + selection and reuse the
    // private dropdown renderer.  We do not mutate suggestions_ long
    // term because this is a pure render primitive called per-frame.
    const bool was_showing = showing_suggestions_;
    const auto old_sel = selected_suggestion_;
    const auto old_sugs = suggestions_;
    suggestions_ = sugs;
    showing_suggestions_ = !sugs.empty();
    selected_suggestion_ = sugs.empty() ? 0
        : ((selected < 0) ? 0
           : std::min(selected, static_cast<int>(sugs.size()) - 1));
    Element out = (showing_suggestions_ && !suggestions_.empty())
        ? RenderSuggestionsDropdown()
        : ftxui::text("");
    // Restore prior state so this call is side-effect-free for callers
    // that also use the interactive path.
    suggestions_ = old_sugs;
    selected_suggestion_ = old_sel;
    showing_suggestions_ = was_showing;
    return out;
}

namespace {
struct InputRow {
    int begin;
    int end;
    int line;
    bool continuation;
};
struct InputLayout {
    std::vector<InputRow> rows;
    int prefix_width;
    int number_width;
    int text_width;
    int cursor_row = 0;
    int cursor_col = 0;
    int first_row = 0;
};

// Source-byte ranges are shared by painting, navigation and the native caret.
// Wrapping changes presentation only; explicit newlines stay in the buffer.
int next_display_glyph(const std::string& value, int pos, bool masked) {
    int end = detail::glyph_forward(value, pos);
    if (!masked) {
        while (end < static_cast<int>(value.size()) && value[end] != '\n') {
            const int next = detail::glyph_forward(value, end);
            if (string_width(value.substr(end, next - end)) != 0) break;
            end = next;
        }
    }
    return end;
}
int display_width(const std::string& value, int begin, int end, bool masked) {
    if (!masked) return string_width(value.substr(begin, end - begin));
    int width = 0;
    while (begin < end) { begin = detail::glyph_forward(value, begin); ++width; }
    return width;
}
InputLayout input_layout(const std::string& value, int cursor,
                         const TextInputOptions& options) {
    InputLayout out;
    out.prefix_width = string_width(options.prefix);
    const int line_count = 1 + static_cast<int>(std::count(value.begin(), value.end(), '\n'));
    out.number_width = options.show_line_numbers && line_count > 1
        ? std::max(2, static_cast<int>(std::to_string(line_count).size())) + 1 : 0;
    out.text_width = options.columns > 0
        ? std::max(1, options.columns - out.prefix_width - out.number_width)
        : std::numeric_limits<int>::max();
    int begin = 0;
    for (int line = 0; line < line_count; ++line) {
        int end = begin;
        while (end < static_cast<int>(value.size()) && value[end] != '\n') ++end;
        int row_begin = begin, pos = begin, cells = 0;
        bool continuation = false;
        while (pos < end) {
            const int next = next_display_glyph(value, pos, options.mask_input);
            const int width = display_width(value, pos, next, options.mask_input);
            if (cells > 0 && cells + width > out.text_width) {
                out.rows.push_back({row_begin, pos, line, continuation});
                row_begin = pos;
                cells = 0;
                continuation = true;
            }
            cells += width;
            pos = next;
        }
        out.rows.push_back({row_begin, end, line, continuation});
        // A caret at the end of a full row needs its own visible cell.
        if (cursor == end && cells >= out.text_width) {
            out.rows.push_back({end, end, line, true});
        }
        begin = end + 1;
    }
    for (int i = 0; i < static_cast<int>(out.rows.size()); ++i) {
        const auto& row = out.rows[i];
        if (cursor >= row.begin && cursor <= row.end) {
            out.cursor_row = i;
            out.cursor_col = display_width(value, row.begin, cursor, options.mask_input);
        }
    }
    if (options.max_visible_rows > 0) {
        out.first_row = std::max(0, out.cursor_row - options.max_visible_rows + 1);
    }
    return out;
}
std::string fit_display_text(const std::string& value, int columns) {
    int pos = 0, cells = 0;
    while (pos < static_cast<int>(value.size())) {
        const int next = next_display_glyph(value, pos, false);
        const int width = string_width(value.substr(pos, next - pos));
        if (width > columns - cells) break;
        cells += width;
        pos = next;
    }
    return value.substr(0, pos);
}
} // namespace

std::pair<int, int> TextInputImpl::visual_cursor_position() const {
    const auto layout = input_layout(text_, cursor_, options_);
    return {layout.prefix_width + layout.number_width + layout.cursor_col,
            layout.cursor_row - layout.first_row};
}

bool TextInputImpl::move_visual_row(int direction) {
    const auto layout = input_layout(text_, cursor_, options_);
    const int target = layout.cursor_row + direction;
    if (target < 0 || target >= static_cast<int>(layout.rows.size())) return false;
    const auto& row = layout.rows[target];
    int pos = row.begin, cells = 0;
    while (pos < row.end) {
        const int next = next_display_glyph(text_, pos, options_.mask_input);
        const int width = display_width(text_, pos, next, options_.mask_input);
        if (cells + width > layout.cursor_col) break;
        cells += width;
        pos = next;
    }
    // A shared boundary belongs to the following row, so stop on the last
    // glyph when the target row is followed by a soft continuation.
    if (pos == row.end && target + 1 < static_cast<int>(layout.rows.size()) &&
        layout.rows[target + 1].begin == row.end && pos > row.begin) {
        pos = detail::glyph_back(text_, pos);
    }
    move_cursor(pos - cursor_, false);
    return true;
}

Element TextInputImpl::RenderInputArea() {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    const auto layout = input_layout(text_, cursor_, options_);
    if (text_.empty()) {
        namespace ph = loom::ui::placeholder;
        const auto placeholder = fit_display_text(options_.placeholder, layout.text_width);
        auto rendered = ph::RenderPlaceholder(
            placeholder.empty() ? std::nullopt : std::optional<std::string_view>(placeholder),
            "", blink_visible_, blink_visible_, options_.terminal_focus,
            options_.prefix, options_.prefix_color.value_or(pal.prompt_border));
        if (rendered.element) return *std::move(rendered.element);
        return hbox({ftxui::text(options_.prefix) |
                     color(options_.prefix_color.value_or(pal.prompt_border)),
                     ftxui::text(" ") | inverted | color(pal.text)});
    }
    const int sel_begin = has_selection() ? std::min(sel_start_, sel_end_) : -1;
    const int sel_end = has_selection() ? std::max(sel_start_, sel_end_) : -1;
    int chip_end = cursor_;
    for (const auto& ref : loom::utils::parse_references(text_)) {
        if (ref.match.starts_with("[Image") && static_cast<int>(ref.index) == cursor_) {
            chip_end += static_cast<int>(ref.match.size());
            break;
        }
    }
    Elements rows;
    const int last = options_.max_visible_rows > 0
        ? std::min(static_cast<int>(layout.rows.size()), layout.first_row + options_.max_visible_rows)
        : static_cast<int>(layout.rows.size());
    for (int index = layout.first_row; index < last; ++index) {
        const auto& row = layout.rows[index];
        Elements parts;
        if (index == 0) {
            auto prefix = ftxui::text(options_.prefix) |
                color(options_.prefix_color.value_or(pal.prompt_border));
            if (options_.prefix_bold) prefix = prefix | bold;
            parts.push_back(std::move(prefix));
        } else {
            parts.push_back(ftxui::text(std::string(layout.prefix_width, ' ')));
        }
        if (layout.number_width > 0) {
            parts.push_back(ftxui::text(row.continuation
                ? std::string(layout.number_width, ' ')
                : std::format("{:>{}} ", row.line + 1, layout.number_width - 1)) |
                dim | color(pal.muted));
        }
        enum class Paint { Plain, Selection, Caret };
        Paint previous = Paint::Plain;
        std::string run;
        auto flush = [&] {
            if (run.empty()) return;
            auto el = ftxui::text(std::exchange(run, {}));
            if (previous == Paint::Selection) el = el | bgcolor(pal.selection_bg) | color(pal.text);
            if (previous == Paint::Caret) el = el | inverted | color(pal.text);
            parts.push_back(std::move(el));
        };
        for (int pos = row.begin; pos < row.end;) {
            const int next = next_display_glyph(text_, pos, options_.mask_input);
            Paint paint = Paint::Plain;
            if (pos < sel_end && next > sel_begin && has_selection()) {
                paint = Paint::Selection;
            } else if ((pos >= cursor_ && pos < chip_end) ||
                       (pos == cursor_ && blink_visible_)) {
                paint = Paint::Caret;
            }
            if (paint != previous) { flush(); previous = paint; }
            run += options_.mask_input ? std::string(1, options_.mask_char)
                                       : text_.substr(pos, next - pos);
            pos = next;
        }
        flush();
        int used = display_width(text_, row.begin, row.end, options_.mask_input);
        const bool cursor_at_end = index == layout.cursor_row && cursor_ == row.end;
        if (cursor_at_end && !has_selection() && used < layout.text_width) {
            parts.push_back(ftxui::text(" ") |
                (blink_visible_ ? inverted : nothing) | color(pal.text));
            ++used;
        }
        auto append_hint = [&](const std::string& value, Color hint_color) {
            auto fitted = fit_display_text(value, layout.text_width - used);
            used += string_width(fitted);
            parts.push_back(ftxui::text(fitted) | dim | color(hint_color));
        };
        if (cursor_at_end && !has_selection() && !options_.mask_input) {
            append_hint(options_.inline_ghost_text, pal.muted);
        }
        if (row.line == 0 && !row.continuation && text_.starts_with('/') &&
            cursor_ > 0 && !options_.argument_hint.empty()) {
            append_hint("  " + options_.argument_hint, pal.info);
        }
        rows.push_back(hbox(std::move(parts)));
    }
    return vbox(std::move(rows));
}

Element TextInputImpl::RenderSuggestionsDropdown() const {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    using namespace ftxui;
    const size_t visible = std::min(
        options_.max_visible_suggestions,
        suggestions_.size());

    int start_idx = 0;
    if ((size_t)selected_suggestion_ >= visible) {
        start_idx = selected_suggestion_ - (int)visible + 1;
    }
    int end_idx = std::min(start_idx + (int)visible, (int)suggestions_.size());

    Elements rows;
    rows.reserve(visible + 2);

    // Header
    rows.push_back(hbox({
        ftxui::text(" " + std::string(loom::ui::design::figures::kInfo) + " "),
        ftxui::text("Suggestions") | bold | color(pal.info),
        ftxui::text(std::format(" ({} total)", (int)suggestions_.size())) | dim,
    }));
    rows.push_back(separator() | color(pal.border_default));

    for (int i = start_idx; i < end_idx; ++i) {
        const auto& s = suggestions_[i];
        bool selected = (i == selected_suggestion_);

        Elements row_parts;
        row_parts.push_back(ftxui::text(selected ? " > " : "   "));

        // Icon
        if (s.icon.has_value() && !s.icon->empty()) {
            row_parts.push_back(ftxui::text(*s.icon + " "));
        } else {
            const char* cat_icon = suggest_util::category_icon(s.category);
            if (*cat_icon != '\0') {
                row_parts.push_back(ftxui::text(std::string(cat_icon) + " "));
            }
        }

        // Display text (or text if display_text empty)
        std::string label = s.display_text.empty() ? s.text : s.display_text;
        Element label_el = ftxui::text(label);
        Color accent = s.color_hint.value_or(
            suggest_util::category_color(s.category));
        if (selected) {
            label_el = label_el | bgcolor(pal.selection_bg) | color(pal.text) | bold;
        } else {
            label_el = label_el | color(accent);
        }
        row_parts.push_back(label_el);

        // Tag
        if (s.tag.has_value() && !s.tag->empty()) {
            row_parts.push_back(ftxui::text(" "));
            row_parts.push_back(ftxui::text(*s.tag) | dim | color(pal.warning));
        }

        // Description
        if (!s.description.empty()) {
            row_parts.push_back(ftxui::text("  "));
            row_parts.push_back(ftxui::text(s.description) | dim | color(pal.muted));
        }

        rows.push_back(hbox(std::move(row_parts)));
    }

    // Footer hint
    rows.push_back(separator() | dim);
    rows.push_back(
        ftxui::text("  [↑/↓] navigate    [Tab] cycle    [Enter] accept    [Esc] close")
            | dim | color(pal.muted));

    return vbox(std::move(rows))
         | border
         | color(pal.border_default);
}

Element TextInputImpl::RenderSearchMode() const {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    using namespace ftxui;
    Elements rows;

    // Search prompt line
    {
        Elements parts;
        parts.push_back(ftxui::text("reverse-i-search: ") | color(pal.warning) | bold);
        parts.push_back(ftxui::text("`" + search_query_ + "`") | color(pal.info));
        parts.push_back(ftxui::text("  "));
        if (search_matches_.empty()) {
            parts.push_back(ftxui::text("(no matches)") | dim | color(pal.danger));
        } else {
            parts.push_back(ftxui::text(
                std::format("{} of {}", (int)search_selected_ + 1, (int)search_matches_.size()))
                | dim);
        }
        rows.push_back(hbox(std::move(parts)));
    }

    // Show current match (if any)
    if (!search_matches_.empty()) {
        const std::string& match = search_matches_[search_selected_];
        rows.push_back(separator() | dim);
        rows.push_back(
            ftxui::text(match) | color(pal.success)
        );
    }

    // Hint
    rows.push_back(separator() | dim);
    rows.push_back(
        ftxui::text("[Enter] accept  [Esc] cancel  [Ctrl+R] next  [↑/↓] navigate")
            | dim | color(pal.muted));

    return vbox(std::move(rows)) | border | color(pal.warning);
}

// ============================================================
// FTXUI Component wrapper
// ============================================================
/// Create an ftxui::Component backed by a TextInputImpl.
/// `out_impl` (optional, non-null) is populated with the internal pointer.
/// Default arguments live on the exported declaration in the primary;
/// this out-of-line definition repeats neither.
Component TextInput(const TextInputOptions& options,
                    std::shared_ptr<TextInputImpl>* out_impl) {
    auto impl = std::make_shared<TextInputImpl>(options);
    if (out_impl) *out_impl = impl;

    // Periodic blink refresh: we use the renderer closure itself to trigger
    // TickBlink() every frame (FTXUI redraws 30-60fps — more than enough).
    return Renderer([impl] {
        impl->TickBlink();
        return impl->Render();
    }) | CatchEvent([impl](Event e) {
        return impl->HandleEvent(e);
    });
}

}  // namespace ui::components
