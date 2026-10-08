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

#include <cstddef>
#include <cstdint>

#include <ftxui/dom/elements.hpp>

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
};

namespace detail {

namespace thm = loom::ui::design::theme;

/// Palette-only status token: in_progress => success, pending => muted,
/// completed => subtle (dimmed further by the strikethrough decorator).
[[nodiscard]] inline const ftxui::Color& status_color(TodoDisplayItem::Status s) {
    const auto& pal = *thm::current_theme().palette;
    switch (s) {
        case TodoDisplayItem::Status::InProgress: return pal.success;
        case TodoDisplayItem::Status::Pending:    return pal.muted;
        case TodoDisplayItem::Status::Completed:  return pal.subtle;
    }
    return pal.muted;
}

/// The status glyph for a todo item.
[[nodiscard]] inline std::string_view status_glyph(TodoDisplayItem::Status s) {
    switch (s) {
        case TodoDisplayItem::Status::InProgress: return "●";
        case TodoDisplayItem::Status::Pending:    return "○";
        case TodoDisplayItem::Status::Completed:  return "✓";
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

    if (max_cols <= 1 || out.size() <= max_cols) return out;
    // Byte-truncate, then back off any UTF-8 continuation bytes (10xxxxxx) so
    // we never emit half a multibyte sequence before the ellipsis.
    std::size_t cut = max_cols - 1;
    while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) {
        --cut;
    }
    out.resize(cut);
    out += "…";
    return out;
}

}  // namespace detail

/// One todo row:
///   ● content        (in_progress — success color)
///   ○ content        (pending — muted)
///   ✓ content        (completed — subtle + strikethrough)
/// Pure render of state-owned data.
[[nodiscard]] inline Element RenderTodoRow(const TodoDisplayItem& item, int avail_cols) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    const auto& glyph_color = detail::status_color(item.status);
    const bool done = item.status == TodoDisplayItem::Status::Completed;

    // Reserve 4 cols for "● " / "✓ " glyph + spacing; clamp to a sane floor.
    const int content_cols = std::max(8, avail_cols - 4);
    Element content = text(detail::one_line(item.content, static_cast<std::size_t>(content_cols)))
                    | color(done ? pal.subtle : pal.text);
    if (done) content = content | strikethrough | dim;

    return hbox({
        text(std::string(detail::status_glyph(item.status)) + " ") | color(glyph_color),
        std::move(content),
        filler(),
    });
}

/// Pinned chrome panel: a progress header + one row per todo item. Returns an
/// empty element when there are no items (so callers can skip it), matching
/// RenderLiveTeammateStrip's contract.
[[nodiscard]] inline Element RenderTodoPanel(
    const std::vector<TodoDisplayItem>& items, int term_cols = 120) {
    if (items.empty()) return text("");

    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    const auto done = std::ranges::count_if(items, [](const auto& it) {
        return it.status == TodoDisplayItem::Status::Completed;
    });

    Elements rows;
    rows.reserve(items.size() + 1);
    rows.push_back(
        text(std::format("Tasks ({}/{} done)", done, items.size()))
        | dim | color(pal.muted));
    for (const auto& item : items) {
        rows.push_back(RenderTodoRow(item, term_cols));
    }
    return vbox(std::move(rows));
}

}  // namespace loom::ui::todos
