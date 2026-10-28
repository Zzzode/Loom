/// @file live_teammates.cppm
/// @brief Live leader-side teammates strip + TeamsView modal body.
///
/// Renders the live teams surface:
///   - Per-teammate live status dot + output tail, pinned above the prompt
///   - Footer count (the "N teams" pill stays owned by prompt_input_footer;
///     this module only renders rows)
///   - TeamDetailView roster modal
///
/// All state is state-owned by ReplScreenState (live_teammates vector +
/// teams_overview_selected_index). The functions here are pure Element
/// builders: they read their arguments, retain nothing, and may only touch
/// palette tokens plus the per-teammate identity color resolved through the
/// same agent_color_manager + shared resolver agent_cards uses. No RGB
/// literals are introduced.
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

#include <cstddef>
#include <cstdint>

export module loom.ui.features.teams.live_teammates;

import std;

import loom.ui.foundation.theme_provider;
import loom.ui.features.agents.agent_shared_widgets;
import loom.tools.agent_color_manager;

export namespace loom::ui::teams::live {

using namespace ftxui;

/// Live projection of one teammate (leader view). 1:1 with the task brief's
/// {agent_id,name,color,status,last_output_tail,pane_id}.
struct LiveTeammate {
    std::string agent_id;
    std::string name;
    std::string color;               // agent color NAME (e.g. "cyan"), resolved at render time
    std::string status = "unknown";  // "running" | "idle" | "unknown"
    std::string last_output_tail;    // single-line tail of pane/transcript output
    std::string pane_id;             // tmux pane id; empty for in-process teammates
};

namespace live_detail {

namespace thm = loom::ui::design::theme;

/// Palette-only status token: running => success, idle => muted, anything
/// else (unknown / a future awaiting-approval token) => warning.
[[nodiscard]] inline const ftxui::Color& status_palette_color(std::string_view status) {
    const auto& pal = *thm::current_theme().palette;
    if (status == "running") return pal.success;
    if (status == "idle") return pal.muted;
    return pal.warning;
}

}  // namespace live_detail

/// Public status color helper (palette tokens only).
[[nodiscard]] inline Color status_color(std::string_view status) {
    return live_detail::status_palette_color(status);
}

/// Per-teammate identity color. The ONLY non-palette color allowed on a
/// teammate row, and even it goes through the existing name parser + shared
/// resolver (same path agent_cards.cppm uses); unparseable names fall back to
/// the palette text token.
[[nodiscard]] inline Color identity_color(const LiveTeammate& t) {
    auto parsed = loom::tools::agent_color_manager::parse_color_name(t.color);
    if (parsed) return loom::ui::agents::shared::agent_color_to_ftxui(*parsed);
    return loom::ui::design::theme::current_theme().palette->text;
}

/// Collapse pane/transcript text to one trimmed line and hard-truncate to
/// max_cols-1 with a trailing ellipsis.
[[nodiscard]] inline std::string one_line_tail(std::string_view raw, std::size_t max_cols = 160) {
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

/// One CoordinatorAgentStatus AgentLine row:
///   ● @name status  output-tail
/// Pure render of state-owned data.
[[nodiscard]] inline Element RenderLiveTeammateRow(const LiveTeammate& t, int avail_cols) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    const int columns = std::max(0, avail_cols);
    Elements segments;
    int remaining = columns;
    auto append = [&](std::string_view value, int capacity, Color foreground, bool strong = false) {
        const auto clipped = one_line_tail(value, static_cast<std::size_t>(std::max(0, capacity)));
        const int width = string_width(clipped);
        Element segment = text(clipped) | color(foreground) | size(WIDTH, EQUAL, width);
        if (strong) segment = segment | bold;
        segments.push_back(std::move(segment));
        remaining -= width;
    };
    if (remaining > 0) append("●", 1, status_color(t.status));
    if (remaining > 0) {
        segments.push_back(text(" ") | size(WIDTH, EQUAL, 1));
        --remaining;
    }
    // Reserve space for status and tail before allowing a long name to grow.
    const int status_reserve = std::min(remaining / 3, string_width(" " + t.status));
    const int tail_reserve = t.last_output_tail.empty() ? 0 : std::min(remaining / 3, 12);
    append("@" + t.name, std::max(0, remaining - status_reserve - tail_reserve), identity_color(t),
           true);
    if (remaining > 0) {
        // Explicit spaces live in their own segment so trimming text cannot
        // concatenate name/status or status/tail.
        segments.push_back(text(" ") | size(WIDTH, EQUAL, 1));
        --remaining;
        append(t.status, std::max(0, remaining - tail_reserve), status_color(t.status));
    }
    if (remaining > 0 && !t.last_output_tail.empty()) {
        segments.push_back(text(" ") | size(WIDTH, EQUAL, 1));
        --remaining;
        append(t.last_output_tail, remaining, pal.muted);
    }
    return hbox(std::move(segments)) | size(WIDTH, EQUAL, columns);
}

/// Pinned chrome strip: one status + output-tail row per teammate. Returns an
/// empty element when there are no teammates (so callers can skip it).
[[nodiscard]] inline Element RenderLiveTeammateStrip(const std::vector<LiveTeammate>& ts,
                                                     int term_cols = 120, int max_rows = -1) {
    if (ts.empty() || max_rows == 0) return vbox({});
    Elements rows;
    const bool overflow = max_rows >= 0 && static_cast<int>(ts.size()) > max_rows;
    const int details = max_rows < 0
                            ? static_cast<int>(ts.size())
                            : std::min(static_cast<int>(ts.size()), max_rows - (overflow ? 1 : 0));
    if (overflow) {
        const auto& pal = *loom::ui::design::theme::current_theme().palette;
        rows.push_back(
            text(one_line_tail(std::format("Teams {} (+{} hidden)", ts.size(),
                                           ts.size() - static_cast<std::size_t>(details)),
                               static_cast<std::size_t>(std::max(0, term_cols)))) |
            color(pal.muted) | size(WIDTH, EQUAL, std::max(0, term_cols)));
    }
    for (int i = 0; i < details; ++i)
        rows.push_back(RenderLiveTeammateRow(ts[static_cast<std::size_t>(i)], term_cols));
    return vbox(std::move(rows));
}

/// TeamsView modal body (TeamsDialog TeamDetailView). Selection index
/// lives in the queue-owned payload; it is clamped here for rendering only.
/// The borrowed state pointer never escapes this call.
[[nodiscard]] inline Element RenderTeamsOverview(std::string_view team_name,
                                                 const std::vector<LiveTeammate>& ts,
                                                 int selected_index, int term_cols = 120,
                                                 int term_rows = 40) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;

    Elements body;
    if (ts.empty()) {
        body.push_back(text(" No teammates") | dim);
    } else {
        const int clamped = std::clamp(selected_index, 0, static_cast<int>(ts.size()) - 1);
        body.reserve(ts.size());
        for (std::size_t i = 0; i < ts.size(); ++i) {
            const auto& t = ts[i];
            const bool selected = static_cast<int>(i) == clamped;
            Elements row;
            row.reserve(5);
            row.push_back(text(selected ? "❯ " : "  ") | color(selected ? pal.text : pal.muted));
            row.push_back(text("@" + t.name) | color(identity_color(t)) |
                          (selected ? bold : nothing));
            if (t.status == "idle") {
                row.push_back(text(" [idle]") | dim | color(pal.muted));
            } else {
                row.push_back(text(""));
            }
            row.push_back(text("  " + t.status) | color(status_color(t.status)) | dim);
            row.push_back(
                text("  " + one_line_tail(t.last_output_tail,
                                          static_cast<std::size_t>(std::max(16, term_cols - 24)))) |
                dim);
            body.push_back(hbox(std::move(row)));
        }
    }

    const std::string subtitle =
        std::to_string(ts.size()) + (ts.size() == 1 ? " teammate" : " teammates");

    Element content = vbox({
        hbox({
            text("Team " + std::string(team_name)) | bold | color(pal.primary),
            filler(),
            text(subtitle) | dim,
        }),
        separator(),
        vbox(std::move(body)),
        separator(),
        text(" ↑/↓ select · Enter view · Esc close") | dim,
    });
    return window(text(""), std::move(content)) |
           size(HEIGHT, LESS_THAN, std::max(6, term_rows - 4));
}

}  // namespace loom::ui::teams::live
