// Shared Loom weave and welcome rendering. No terminal I/O or animation.
module;
#include <ftxui/dom/elements.hpp>

module loom.ui.foundation.logo;

import std;
import loom.ui.foundation.theme_provider;

namespace loom::ui::logo {
namespace detail {
[[nodiscard]] auto make_weave_icon() -> Element {
    const auto accent = loom::ui::design::theme::current_theme().palette->loom_body;
    // Plain weave alternates the warp and weft across three nine-cell rows.
    return vbox({
        text("█░█░█░█░█") | color(accent),
        text("░█░█░█░█░") | color(accent),
        text("█░█░█░█░█") | color(accent),
    }) | ftxui::center;
}
} // namespace detail

[[nodiscard]] auto RenderCondensedLogoElement(
    const LogoDisplayData& data, int term_cols) -> ftxui::Element {
    using namespace ftxui;

    // Theme tokens.  The palette accessors are function-local statics, so
    // they are immune to the inline-variable static-init-order fiasco that
    // previously forced hardcoded brand colors here.
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    const Color kText  = pal.text;
    const Color kMuted = pal.muted;
    Element weave_icon = detail::make_weave_icon();

    // --- Text column: 3 rows, width = max(term_cols - 15, 20)
    // (15 = 9 icon + 2 gap + 4 pad).
    const int text_width = std::max(term_cols - 15, 20);

    // Row 1: <Text bold>Loom</Text>
    Element row1 = text("Loom") | bold | color(kText);

    // Row 2: model [· billing]
    // formatModelAndBilling — when model+billing fits on one
    // line we render "{model} · {billing}", else two separate dim lines
    // (here simplify: single line since truncation is handled elsewhere by
    // the text container; text_width limit truncates overflow naturally).
    Element row2_body;
    if (data.billing_type.empty()) {
        row2_body = text(data.model_display_name);
    } else {
        row2_body = hbox({
            text(data.model_display_name),
            text(" \xC2\xB7 ") | color(kMuted),  // · U+00B7 MIDDLE DOT
            text(data.billing_type),
        });
    }
    Element row2 = std::move(row2_body) | dim | color(kMuted);

    // Row 3: [@{agent} · ] truncate-path(cwd)
    // MIDDLE truncation with U+2026 "…" as the
    // ellipsis between the first segment and the trailing segments.  For
    // simplicity we approximate with "prefix … /last/segment" when overflow.
    std::optional<std::string> agent_prefix;
    int agent_width = 0;
    if (data.agent_name.has_value() && !data.agent_name->empty()) {
        agent_prefix = "@" + *data.agent_name + " \xC2\xB7 ";
        agent_width  = static_cast<int>(agent_prefix->size());
    }
    int cwd_max = std::max(text_width - agent_width, 8);
    std::string cwd_display = data.cwd;
    if (static_cast<int>(cwd_display.size()) > cwd_max) {
        // Middle-truncate approx: keep first segment + " … " + last segment
        auto first_slash = cwd_display.find('/', 1);
        auto last_slash  = cwd_display.rfind('/');
        if (first_slash != std::string::npos && last_slash != std::string::npos
            && last_slash > first_slash + 1) {
            std::string head = cwd_display.substr(0, first_slash + 1);
            std::string tail = cwd_display.substr(last_slash);
            cwd_display = head + "\xE2\x80\xA6" + tail;  // … U+2026
            // If still too long, fall back to suffix truncation.
            if (static_cast<int>(cwd_display.size()) > cwd_max) {
                cwd_display = "\xE2\x80\xA6" + cwd_display.substr(
                    cwd_display.size() - static_cast<std::size_t>(cwd_max - 3));
            }
        } else {
            cwd_display = "\xE2\x80\xA6" + cwd_display.substr(
                cwd_display.size() - static_cast<std::size_t>(cwd_max - 3));
        }
    }
    Elements row3_parts;
    if (agent_prefix) {
        row3_parts.push_back(text(*agent_prefix));
    }
    row3_parts.push_back(text(cwd_display));
    Element row3 = hbox(std::move(row3_parts)) | dim | color(kMuted);

    Element text_col = vbox({
        std::move(row1),
        std::move(row2),
        std::move(row3),
    });

    // Outer layout: hbox with 2-column gap.
    // gap = 2 means two whitespace columns between the icon and text col.
    return hbox({
        std::move(weave_icon),
        text("  "),
        std::move(text_col),
    });
}

[[nodiscard]] auto RenderWelcome(WelcomeTheme /*theme*/) -> Element {
    const auto& palette = *loom::ui::design::theme::current_theme().palette;
    return vbox({
        text("Welcome to Loom") | bold | color(palette.text) | ftxui::center,
        text(""),
        detail::make_weave_icon(),
        text("Loom") | bold | color(palette.loom_body) | ftxui::center,
        text(""),
    }) | size(WIDTH, EQUAL, kWelcomeFixedWidth);
}

} // namespace loom::ui::logo
