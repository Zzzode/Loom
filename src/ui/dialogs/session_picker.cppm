/// @file session_picker.cppm
/// @brief Interactive session picker dialog — a fullscreen standalone dialog
///        for /resume: search input + scrollable session list with keyboard
///        navigation (↑/↓, j/k, Enter, Esc).
///
/// MODULE:   loom.ui.dialogs.session_picker
/// LICENCE:  Exported.  Imported by default_renderers to register, and
///           by app code to push the picker onto the dialog queue.
///
/// Modeled on quick_open.cppm — same fuzzy-substring filtering, same
/// scroll-indicator pattern, same event-handling structure.  The differences
/// are: no category headers (each row is a session with metadata columns),
/// the on_select callback carries a session id string, and the dialog is
/// Standalone-slot (fullscreen) rather than Modal.
module;

#include <cctype>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>
#include <cstddef>

export module loom.ui.dialogs.session_picker;

import std;

import loom.ui.dialogs.system;
import loom.ui.dialogs.frame;
import loom.ui.permissions.components;

export namespace loom::ui::dialogs::session_picker {

using namespace ftxui;
namespace dsys = loom::ui::dialogs::system;
namespace dframe = loom::ui::dialogs::frame;
using SessionPickerEntry = dsys::SessionPickerEntry;

// ============================================================
// Fuzzy filtering
// ============================================================

/// Filter sessions based on a fuzzy query string (case-insensitive).
/// Matches query characters in order within title, cwd, or session_id.
[[nodiscard]] inline auto filter_sessions(std::span<const SessionPickerEntry> sessions,
                                          std::string_view query)
    -> std::vector<SessionPickerEntry>
{
    if (query.empty()) {
        return std::vector<SessionPickerEntry>(sessions.begin(), sessions.end());
    }

    std::vector<SessionPickerEntry> results;

    auto to_lower = [](std::string_view s) -> std::string {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    };

    std::string lower_query = to_lower(query);

    for (const auto& s : sessions) {
        std::string lower_title = to_lower(s.title);
        std::string lower_cwd = to_lower(s.cwd);
        std::string lower_id = to_lower(s.session_id);

        // Fuzzy match: query chars appear in order in any searchable field.
        auto fuzzy_match = [&](const std::string& haystack) {
            size_t qi = 0;
            for (char c : haystack) {
                if (qi < lower_query.size() && c == lower_query[qi]) {
                    ++qi;
                }
            }
            return qi == lower_query.size();
        };

        if (fuzzy_match(lower_title) || fuzzy_match(lower_cwd) || fuzzy_match(lower_id)) {
            results.push_back(s);
        }
    }

    return results;
}

// ============================================================
// Rendering
// ============================================================

/// Render the SessionPicker dialog using FTXUI + DialogFrame.
/// Standalone slot: fills the terminal with a small margin.
[[nodiscard]] inline Element RenderSessionPicker(
    const dsys::SessionPickerPayload& p,
    const dsys::DialogRenderContext& ctx)
{
    auto filtered = filter_sessions(p.sessions, p.query);
    int count = static_cast<int>(filtered.size());
    int selected = p.selected_index;
    if (selected >= count) selected = std::max(0, count - 1);
    if (selected < 0) selected = 0;

    // Max visible items (leave room for search input + divider + padding).
    // Standalone slot: use full terminal dimensions.
    const int visible_rows = ctx.term_rows;
    int max_visible = std::max(3, visible_rows - 8);

    // Calculate scroll window
    int start = 0;
    if (selected >= max_visible) {
        start = selected - max_visible + 1;
    }
    int end = std::min(count, start + max_visible);

    // ---- Build session list ----
    Elements list_els;
    namespace pc = loom::ui::permissions::components;

    if (filtered.empty()) {
        list_els.push_back(
            hbox({
                text("  "),
                text("No sessions found") | dim,
            })
        );
    } else {
        for (int i = start; i < end; ++i) {
            const auto& s = filtered[i];
            bool is_selected = (i == selected);

            auto prefix = is_selected
                ? text("❯ ") | color(Color::Cyan)
                : text("  ");

            auto title_el = is_selected
                ? text(s.title) | bold
                : text(s.title);

            // Right-aligned metadata: age · N msgs · cwd
            auto meta = std::format("{}  ·  {} msgs  ·  {}",
                s.age_string, s.message_count, s.cwd);

            Elements row_els = { prefix, title_el, filler(), text(meta) | dim };

            auto row = hbox(std::move(row_els));
            if (is_selected) {
                row = row | bgcolor(Color::BlueLight) | color(Color::White);
            }
            list_els.push_back(row);
        }

        // Scroll indicators
        if (start > 0) {
            list_els.insert(list_els.begin(),
                hbox({
                    text("  ↑ "),
                    text(std::to_string(start) + " more") | dim,
                    filler(),
                })
            );
        }
        int remaining = count - end;
        if (remaining > 0) {
            list_els.push_back(
                hbox({
                    text("  ↓ "),
                    text(std::to_string(remaining) + " more") | dim,
                    filler(),
                })
            );
        }
    }

    auto list_el = vbox(std::move(list_els));

    // ---- Search input row ----
    auto query_display = p.query.empty()
        ? text("Type to search sessions...") | dim
        : text(p.query) | bold;

    auto search_row = hbox({
        text("❯ ") | color(Color::Cyan),
        query_display,
        filler(),
        text(std::to_string(count) + " sessions") | dim,
    });

    // ---- Assemble full content (search + divider + list) ----
    auto full_content = vbox({
        search_row,
        text(""),
        pc::ThinDivider(),
        text(""),
        list_el | yflex,
    });

    // Wrap in DialogFrame (standalone: PanelPadded, fills the terminal)
    dframe::DialogFrameProps props;
    props.title = "Resume Session";
    props.subtitle = "Select a conversation to resume";
    props.style = dframe::FrameStyle::Permission;
    props.content = full_content;
    props.inner_padding_x = 1;
    props.inner_padding_y = 1;
    props.full_border = true;
    props.rounded = true;
    props.pane_variant = dframe::PaneVariant::PanelPadded;

    const int avail_cols = std::max(40, ctx.term_cols - 2);
    const int avail_rows = std::max(10, ctx.term_rows - 2);
    return dframe::DialogFrame(props, ctx.theme)
        | size(WIDTH, EQUAL, avail_cols)
        | size(HEIGHT, EQUAL, avail_rows);
}

// ============================================================
// Event handling
// ============================================================

/// Handle keyboard events for the SessionPicker dialog.
/// Returns true if the event was handled.
inline bool HandleSessionPickerEvent(
    dsys::SessionPickerPayload& p,
    const Event& event)
{
    auto filtered = filter_sessions(p.sessions, p.query);
    int count = static_cast<int>(filtered.size());

    // Navigation
    if (event == Event::ArrowUp || event == Event::Character('k') ||
        event == Event::Character('K'))
    {
        if (p.selected_index > 0) {
            p.selected_index--;
        } else if (count > 0) {
            p.selected_index = count - 1;  // wrap to bottom
        }
        return true;
    }
    if (event == Event::ArrowDown || event == Event::Character('j') ||
        event == Event::Character('J'))
    {
        if (p.selected_index < count - 1) {
            p.selected_index++;
        } else {
            p.selected_index = 0;  // wrap to top
        }
        return true;
    }

    // Page up/down
    if (event == Event::PageUp) {
        p.selected_index = std::max(0, p.selected_index - 8);
        return true;
    }
    if (event == Event::PageDown) {
        p.selected_index = std::min(count - 1, p.selected_index + 8);
        return true;
    }

    // Home / End
    if (event == Event::Home) {
        p.selected_index = 0;
        return true;
    }
    if (event == Event::End) {
        p.selected_index = std::max(0, count - 1);
        return true;
    }

    // Selection — fire on_select with the session id
    if (event == Event::Return) {
        if (count > 0 && p.on_select) {
            int idx = std::min(p.selected_index, count - 1);
            if (idx >= 0) {
                p.on_select(filtered[idx].session_id);
            }
        }
        return true;
    }

    // Cancel — fire on_select with empty string
    if (event == Event::Escape) {
        if (p.on_select) {
            p.on_select("");
        }
        return true;
    }

    // Backspace — remove last char from query
    if (event == Event::Backspace || event == Event::Delete) {
        if (!p.query.empty()) {
            p.query.pop_back();
            p.selected_index = 0;  // reset selection on filter change
        }
        return true;
    }

    // Printable characters — add to query
    if (event.is_character()) {
        char c = event.character()[0];
        // Ignore control characters
        if (std::isprint(static_cast<unsigned char>(c))) {
            p.query += c;
            p.selected_index = 0;  // reset selection on filter change
            return true;
        }
    }

    return false;
}

} // namespace loom::ui::dialogs::session_picker
