/// @file todo_panel.cppm
/// @brief Persistent todo-list panel, pinned above the prompt input.
///
/// Renders the AI-managed task list (populated via the `todo_write` tool):
///   - A one-line progress header ("Tasks (X/Y done)")
///   - One row per item: a status glyph + the task content
///
/// All state is state-owned by ReplScreenState (TodoStore.items, projected
/// from the TodoWriteTool singleton by AppAdapter). The functions here are
/// pure Element builders: they read their arguments, retain nothing, and may
/// only touch palette tokens. No RGB literals are introduced.
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

#include <cstddef>
#include <cstdint>

export module loom.ui.features.todos.todo_panel;

import std;

import loom.ui.foundation.theme_provider;

export namespace loom::ui::todos {

using namespace ftxui;

/// Display projection of one todo item. Mirrors the tool-layer
/// loom::tools::TodoItem subset the renderer needs (content + status +
/// priority), decoupled so the features area does not import the tool module.
/// Mapped by AppAdapter::ProjectTodosToScreenState.
struct TodoDisplayItem {
    enum class Status : std::uint8_t { Pending, InProgress, Completed };
    enum class Priority : std::uint8_t { High, Medium, Low };

    std::string content;
    Status status = Status::Pending;
    Priority priority = Priority::Medium;

    bool operator==(const TodoDisplayItem&) const = default;
};

namespace detail {

namespace thm = loom::ui::design::theme;

/// Palette-only status token: in_progress => success, pending => muted,
/// completed => muted, with strikethrough on the label.
[[nodiscard]] inline const ftxui::Color& status_color(TodoDisplayItem::Status s) {
    const auto& pal = *thm::current_theme().palette;
    switch (s) {
    case TodoDisplayItem::Status::InProgress:
        return pal.success;
    case TodoDisplayItem::Status::Pending:
        return pal.muted;
    case TodoDisplayItem::Status::Completed:
        return pal.muted;
    }
    return pal.muted;
}

/// The status glyph for a todo item.
[[nodiscard]] inline std::string_view status_glyph(TodoDisplayItem::Status s) {
    switch (s) {
    case TodoDisplayItem::Status::InProgress:
        return "●";
    case TodoDisplayItem::Status::Pending:
        return "○";
    case TodoDisplayItem::Status::Completed:
        return "✓";
    }
    return "○";
}

/// Collapse text to one trimmed line and hard-truncate to max_cols-1 with a
/// trailing ellipsis. Mirrors live_teammates.cppm's one_line_tail so a long
/// task description never wraps the panel.
[[nodiscard]] inline std::string one_line(std::string_view raw, std::size_t max_cols) {
    std::string out;
    out.reserve(raw.size());
    bool last_was_space = false;
    for (char ch : raw) {
        if (ch == '\r' || ch == '\n') ch = ' ';
        if (ch == ' ') {
            if (last_was_space) continue;
            last_was_space = true;
        } else {
            last_was_space = false;
        }
        out.push_back(ch);
    }

    const auto first = out.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    const auto last = out.find_last_not_of(' ');
    out = out.substr(first, last - first + 1);

    if (max_cols == 0) return {};
    if (string_width(out) <= static_cast<int>(max_cols)) return out;
    std::string clipped;
    int used = 0;
    for (const auto& glyph : Utf8ToGlyphs(out)) {
        if (glyph.empty()) continue;
        const int width = string_width(glyph);
        if (used + width > static_cast<int>(max_cols) - 1) break;
        clipped += glyph;
        used += width;
    }
    return clipped + "…";
}

}  // namespace detail

/// One todo row:
///   ● content        (in_progress — success color)
///   ○ content        (pending — muted)
///   ✓ content        (completed — muted + strikethrough)
/// Pure render of state-owned data.
[[nodiscard]] inline Element RenderTodoRow(const TodoDisplayItem& item, int avail_cols) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    const auto& glyph_color = detail::status_color(item.status);
    const bool done = item.status == TodoDisplayItem::Status::Completed;

    const int columns = std::max(0, avail_cols);
    const std::string prefix =
        columns == 0 ? ""
                     : std::string(detail::status_glyph(item.status)) + (columns > 1 ? " " : "");
    const int prefix_cols = string_width(prefix);
    const int content_cols = std::max(0, columns - prefix_cols);
    const auto label = detail::one_line(item.content, static_cast<std::size_t>(content_cols));
    Element content =
        text(label) | color(done ? pal.muted : pal.text) | size(WIDTH, EQUAL, string_width(label));
    if (done) content = content | strikethrough;
    return hbox({
               text(prefix) | color(glyph_color) | size(WIDTH, EQUAL, prefix_cols),
               std::move(content),
               filler() | flex,
           }) |
           size(WIDTH, EQUAL, columns);
}

/// Pinned chrome panel: a progress header + one row per todo item. Returns an
/// empty element when there are no items (so callers can skip it), matching
/// RenderLiveTeammateStrip's contract.
[[nodiscard]] inline Element RenderTodoPanel(const std::vector<TodoDisplayItem>& items,
                                             int term_cols = 120, int max_rows = -1,
                                             bool completed_collapsed = false) {
    if (items.empty() || max_rows == 0) return vbox({});
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    const auto done = std::ranges::count_if(
        items, [](const auto& it) { return it.status == TodoDisplayItem::Status::Completed; });
    const bool all_completed = static_cast<std::size_t>(done) == items.size();
    if (all_completed && completed_collapsed) max_rows = 1;
    const int detail_rows =
        max_rows < 0 ? static_cast<int>(items.size())
                     : std::min(static_cast<int>(items.size()), std::max(0, max_rows - 1));
    std::string header = max_rows < 0
                             ? std::format("Tasks ({}/{} done)", done, items.size())
                             : std::format("Tasks {}/{} done (+{} hidden)", done, items.size(),
                                           items.size() - static_cast<std::size_t>(detail_rows));
    if (all_completed)
        header = std::format("{}Tasks {}/{} completed", completed_collapsed ? "✓ " : "", done,
                             items.size());
    Elements rows;
    rows.push_back(
        text(detail::one_line(header, static_cast<std::size_t>(std::max(0, term_cols)))) |
        color(pal.muted) | size(WIDTH, EQUAL, std::max(0, term_cols)));
    if (max_rows < 0) {
        for (const auto& item : items)
            rows.push_back(RenderTodoRow(item, term_cols));
    } else {
        // Render active work first without changing the state-owned item order.
        int remaining = detail_rows;
        for (bool active : {true, false}) {
            for (const auto& item : items) {
                if (remaining == 0) break;
                if ((item.status == TodoDisplayItem::Status::InProgress) == active) {
                    rows.push_back(RenderTodoRow(item, term_cols));
                    --remaining;
                }
            }
        }
    }
    return vbox(std::move(rows));
}

}  // namespace loom::ui::todos
