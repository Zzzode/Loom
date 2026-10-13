// Implementations for loom.ui.prompt.prompt_input_footer.
module;

#include <cmath>
#include <cstdio>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

module loom.ui.prompt.prompt_input_footer;

import std;
import loom.ui.chrome.ansi_render;
import loom.ui.foundation.theme_provider;

namespace loom::ui::prompt::footer {
using namespace ftxui;

[[nodiscard]] std::string GetLastPathComponents(
    const std::string& cwd, int n)
{
    if (cwd.empty()) return "";
    fs::path p(cwd);
    std::vector<std::string> parts;
    for (const auto& comp : p) {
        if (!comp.empty() && comp != "/") {
            parts.push_back(comp.string());
        }
    }
    if (parts.empty()) return cwd;
    int start = std::max(0, static_cast<int>(parts.size()) - n);
    std::string result;
    for (int i = start; i < static_cast<int>(parts.size()); ++i) {
        if (!result.empty()) result += "/";
        result += parts[i];
    }
    return result;
}
[[nodiscard]] std::string FormatTokensK(std::int64_t tokens) {
    if (tokens <= 0) return "0K";
    if (tokens < 1000) return std::to_string(tokens);
    double k = tokens / 1000.0;
    // Show 1 decimal for thousands
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1fK", k);
    return std::string(buf);
}

[[nodiscard]] Element RenderStatusLine(const StatusLineOptions& opts) {
    if (!opts.should_display) return text("");
    const int padding = std::max(0, opts.padding_x);
    const int usable = opts.columns > 0 ? std::max(0, opts.columns - 2 * padding) : 0;
    if (opts.columns > 0 && usable == 0) return text(" ") | size(HEIGHT, EQUAL, 1);
    Element content;
    if (opts.content.empty()) {
        if (opts.builtin) {
            auto data = *opts.builtin;
            if (opts.columns > 0) data.columns = usable;
            content = RenderBuiltinStatusLine(data);
        } else if (opts.is_fullscreen) {
            content = text(" ");
        } else {
            return text("");
        }
    } else {
        namespace msgs = loom::ui::messages;
        auto lines = std::static_pointer_cast<std::vector<Element>>(
            msgs::ansi_to_ftxui_elements(opts.content));
        // Preserve external command colors and the existing dim chrome style.
        content = vbox(std::move(*lines)) | dim;
    }
    if (opts.columns > 0) content = std::move(content) | size(WIDTH, EQUAL, usable);
    if (padding > 0) {
        const std::string pad(static_cast<std::size_t>(padding), ' ');
        content = hbox({text(pad), std::move(content), text(pad)});
    }
    return std::move(content) | size(HEIGHT, EQUAL, 1);
}

namespace {
// FTXUI glyphs include empty continuation cells for fullwidth characters.
// Elide only at glyph boundaries, preserving combining marks and UTF-8.
std::string Ellipsize(const std::string& value, int columns) {
    if (string_width(value) <= columns) return value;
    if (columns < 1) return "";
    std::string result;
    int used = 0;
    for (const auto& glyph : Utf8ToGlyphs(value)) {
        if (glyph.empty()) continue;
        const int width = string_width(glyph);
        if (used + width > columns - 1) break;
        result += glyph;
        used += width;
    }
    return result + "…";
}

struct StatusField {
    std::string value;
    Color foreground;
    bool emphasized = false;
    bool abbreviate = false;
    int priority = 0;
    std::string selected;
};
} // namespace

[[nodiscard]] Element RenderBuiltinStatusLine(const BuiltinStatusLineData& data) {
    if (!data.status_bar_enabled) return text("");
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    const bool bounded = data.columns > 0;
    const int columns = data.columns;
    std::vector<StatusField> fields;
    if (data.segments.cwd) {
        auto folder = GetLastPathComponents(data.cwd, 2);
        fields.push_back({folder.empty() ? "~" : folder, pal.muted, false, true, 2, {}});
    }
    if (data.segments.git && !data.git_branch.empty())
        fields.push_back({data.git_branch, pal.success, false, true, 4, {}});
    if (data.segments.model && !data.model_name.empty())
        fields.push_back({data.model_name, pal.info, true, true, 1, {}});
    if (data.segments.tokens) {
        const double pct = data.context_window_size > 0
            ? 100.0 * data.context_token_count / data.context_window_size : 0.0;
        const auto usage_color = pct >= 80.0 ? pal.danger : pct >= 50.0 ? pal.warning : pal.muted;
        auto usage = std::format("{}%", static_cast<int>(std::round(pct)));
        if (!bounded || columns >= 90)
            usage += std::format(" {}/{}", FormatTokensK(data.context_token_count),
                FormatTokensK(data.context_window_size));
        fields.push_back({std::move(usage), usage_color, false, false, 0, {}});
    }
    if (data.segments.cost && data.cost_usd && *data.cost_usd > 0.0) {
        char cost[32];
        std::snprintf(cost, sizeof(cost), "$%.4f", *data.cost_usd);
        fields.push_back({cost, pal.muted, false, false, 5, {}});
    }
    if (data.segments.tasks && data.task_count > 0)
        fields.push_back({std::format("{} team{}", data.task_count,
            data.task_count == 1 ? "" : "s"), pal.warning, false, false, 6, {}});
    if (data.segments.agent && !data.agent_name.empty())
        fields.push_back({"@" + data.agent_name, pal.info, false, true, 7, {}});
    if (!data.mouse_mode_label.empty())
        fields.push_back({data.mouse_mode_label, pal.muted, false, false, 3, {}});

    int remaining = columns;
    bool selected_any = false;
    // Prioritize whole semantic fields before composing their display order.
    // Percentage, model and folder survive first; optional chrome uses slack.
    for (int priority = 0; priority <= 7; ++priority) {
        for (auto& field : fields) {
            if (field.priority != priority) continue;
            if (!bounded) { field.selected = field.value; continue; }
            const int available = remaining - (selected_any ? 3 : 0);
            const int minimum = field.abbreviate ? (priority == 1 ? 6 : 4)
                : string_width(field.value);
            if (available < minimum) continue;
            const int cap = field.abbreviate
                ? std::min(available, std::max(minimum, std::min(priority == 1 ? 32 : 22, columns / 3)))
                : available;
            field.selected = field.abbreviate ? Ellipsize(field.value, cap) : field.value;
            remaining -= string_width(field.selected) + (selected_any ? 3 : 0);
            selected_any = true;
        }
    }
    Elements parts;
    for (const auto& field : fields) {
        if (field.selected.empty()) continue;
        if (!parts.empty()) parts.push_back(text(" · ") | color(pal.muted) | dim);
        auto value = text(field.selected) | color(field.foreground);
        parts.push_back(std::move(value) | (field.emphasized ? bold : dim));
    }
    return hbox(std::move(parts)) | size(HEIGHT, EQUAL, 1);
}

[[nodiscard]] Element RenderPromptInputFooter(const FooterOptions& opts) {
    // ── Special: suggestions overlay replaces entire footer ──────────
    if (opts.has_suggestions && opts.suggestions_content) {
        return hbox({ text("  "), std::move(opts.suggestions_content) });
    }

    // ── Special: help menu replaces entire footer ────────────────────
    if (opts.help_open) {
        // Help menu content would be injected here; for now show placeholder
        return hbox({ text("  Help menu") | dim });
    }

    // ── Left column ────────────────────────────────────────────────────
    Elements left_col;
    left_col.reserve(2);

    // StatusLine (shown only when the caller's visibility gate allows it).
    // In fullscreen mode, reserves a blank row even while loading so the
    // footer height never shifts (stable height — same trick as LeftSide).
    // Exit message / pasting / history search affect only LeftSide (below),
    // not StatusLine — they have separate rows.
    // P0-6: Also show when builtin fallback data is available (standalone mode
    // where user's command returns empty or no statusLine configured).
    const bool has_builtin = opts.status_line.builtin.has_value();
    bool show_status_line = opts.status_line.should_display
        && (opts.is_fullscreen || !opts.status_line.content.empty() || has_builtin);

    // LeftSide (always present — ModeIndicator reserves 1 row)
    left_col.push_back(RenderLeftSide(opts.left_side));

    Element left_el = vbox(std::move(left_col)) | flex;

    // ── Right column ───────────────────────────────────────────────────
    // Right controls share one horizontal row below status.
    Elements right_col;

    // Notifications (shown in both fullscreen and non-fullscreen in CPP
    // since we reserve stable height; the BuiltinStatusLine already covers
    // the info need).
    {
        Element notif_el = RenderNotifications(opts.notification);
        // Check if the notification element has actual content (not just
        // a blank placeholder row).  We detect this by checking if the
        // notification data has any active field.
        const auto& nd = opts.notification;
        const bool has_active =
            QueueHasItems(nd.queue) ||
            (nd.dynamic_text && !nd.dynamic_text->empty()) ||
            nd.ide.connected ||
            nd.is_overage_mode ||
            nd.api_key_status == ApiKeyStatus::Invalid ||
            nd.api_key_status == ApiKeyStatus::Missing ||
            nd.debug_mode ||
            (nd.verbose && nd.api_key_status == ApiKeyStatus::Valid && nd.token_usage > 0) ||
            // P1: typed notification pills (footer-notifications-stub)
            nd.auto_updater.has_value() ||
            nd.new_release.has_value() ||
            (nd.pro_renewal.has_value()
             && nd.pro_renewal->days_remaining > 0
             && nd.pro_renewal->days_remaining < 7);

        if (has_active) {
            right_col.push_back(std::move(notif_el));
        }
    }

    // Bottom row: undercover + bridge status (inline items)
    Elements bottom_row;

    // Undercover (ant-only)
    if (opts.is_undercover) {
        if (!bottom_row.empty()) bottom_row.push_back(text(" ") | dim);
        bottom_row.push_back(text("undercover") | dim);
    }

    // Bridge status indicator
    if (opts.bridge.status != BridgeStatus::Disabled
        && (opts.bridge.explicit_remote
            || opts.bridge.status == BridgeStatus::Reconnecting))
    {
        if (!bottom_row.empty()) bottom_row.push_back(text(" ") | dim);
        bottom_row.push_back(RenderBridgeStatus(opts.bridge));
    }

    if (!bottom_row.empty()) {
        if (!right_col.empty()) right_col.push_back(text("  "));
        right_col.push_back(hbox(std::move(bottom_row)));
    }

    const bool has_right = !right_col.empty();
    Element right_el = has_right ? hbox(std::move(right_col)) : text("");

    Elements rows;
    if (show_status_line) {
        rows.push_back(hbox({text("  "), RenderStatusLine(opts.status_line) | flex,
            text("  ")}) | size(HEIGHT, EQUAL, 1));
    }
    if (opts.is_narrow) {
        rows.push_back(hbox({text("  "), std::move(left_el), text("  ")}));
        if (has_right)
            rows.push_back(hbox({text("  "), std::move(right_el), text("  ")}));
    } else {
        rows.push_back(hbox({text("  "), std::move(left_el), filler(),
            std::move(right_el), text("  ")}));
    }
    return vbox(std::move(rows));
}

} // namespace loom::ui::prompt::footer
