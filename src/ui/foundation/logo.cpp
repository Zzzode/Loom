// Shared Loom wordmark and welcome rendering. No terminal I/O or animation.
module;
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

module loom.ui.foundation.logo;

import std;
import loom.ui.foundation.theme_provider;

namespace loom::ui::logo {
namespace detail {
[[nodiscard]] auto make_wordmark(bool large) -> Element {
    const auto& palette = *loom::ui::design::theme::current_theme().palette;
    // Source: docs/design/assets/loom-identity.svg (388x168 wordmark).
    // SVG contours sampled at 16x16 points per cell; eighth blocks
    // refine edges without enlarging the 28x6 / 19x4 banner or thinning stems.
    // B/J = body/join; lowercase reverses ink and terminal background.
    constexpr std::array<std::string_view, 6> large_glyphs{
        "██▍                         ",
        "██▍  ▃▄▅▅▄▖▗▅▅▅▄  ▂▄▅▄▃▄▅▅▄ ",
        "██▍ ▋██▀▀▜▉▃▅▅▜█▉▋██▀▜██▀▜█▊",
        "██▍ ▋█▉  ▗▇█  ▐█▉▋██ ▐█▉ ▐█▉",
        "██▍▁▋██▁▁▐▀▅▁▁▐█▉▋█▉ ▐█▊ ▐█▉",
        "▝▁██ ▃███▁▅▁███▁▘▋█▉ ▐█▊ ▐█▉",
    };
    constexpr std::array<std::string_view, 6> large_styles{
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBB",
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBB",
        "BBBBbBBBBBBJbbBBBbBBBBBBBBBB",
        "BBBBbBBBBJJJBBBBBbBBBBBBBBBB",
        "BBBBbBBBBJJBBBBBBbBBBBBBBBBB",
        "BbBBBbBBBbbbBBBbBbBBBBBBBBBB",
    };
    constexpr std::array<std::string_view, 4> compact_glyphs{
        "█▋   ▁   ▁         ",
        "█▋ ▟█▃█▋▃▜▇▏▇▛██▃█▖",
        "█▋ ▏▋ ▍▁ ▊█▊█▎▎▉ █▋",
        "▂█▋▜█▆▊▁▆▆▁▊█▎▎▉ █▌",
    };
    constexpr std::array<std::string_view, 4> compact_styles{
        "BBBBBBBBBBBBBBBBBBB",
        "BBBBBbBbbBBBBBBBbBB",
        "BBBbBBjjBbBbBBbBBBB",
        "bBBBBBBbBBbbBBbBBBB",
    };
    const std::span<const std::string_view> glyphs = large
        ? std::span<const std::string_view>(large_glyphs)
        : std::span<const std::string_view>(compact_glyphs);
    const std::span<const std::string_view> styles = large
        ? std::span<const std::string_view>(large_styles)
        : std::span<const std::string_view>(compact_styles);
    Elements rows;
    for (std::size_t y = 0; y < glyphs.size(); ++y) {
        Elements cells;
        const auto characters = ftxui::Utf8ToGlyphs(std::string(glyphs[y]));
        for (std::size_t x = 0; x < characters.size(); ++x) {
            const char role = styles[y][x];
            Element cell = text(characters[x]) | color(
                role == 'J' || role == 'j' ? palette.loom_body : palette.text);
            // SGR reverse preserves the terminal's actual background in gaps.
            if (role == 'b' || role == 'j') cell = std::move(cell) | ftxui::inverted;
            cells.push_back(std::move(cell));
        }
        rows.push_back(hbox(std::move(cells)));
    }
    return vbox(std::move(rows)) | ftxui::center;
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
    const bool large = layout_mode_from_cols(term_cols) == LogoLayoutMode::Horizontal;
    const bool text_only = term_cols < 29;
    Element wordmark = text_only ? text("loom") | bold | color(kText) | center
                                : detail::make_wordmark(large);

    // Reserve the icon, two-column gap and four columns of outer padding.
    const int wordmark_width = large ? 28 : (text_only ? 4 : 19);
    const int text_width = std::max(term_cols - wordmark_width - 6, 8);

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
    }) | size(WIDTH, EQUAL, text_width) | vcenter;

    // Outer layout: hbox with 2-column gap.
    // gap = 2 means two whitespace columns between the icon and text col.
    return hbox({
        std::move(wordmark),
        text("  "),
        std::move(text_col),
    });
}

[[nodiscard]] auto RenderWelcome(WelcomeTheme /*theme*/) -> Element {
    const auto& palette = *loom::ui::design::theme::current_theme().palette;
    return vbox({
        text("Welcome to Loom") | bold | color(palette.text) | ftxui::center,
        text(""),
        detail::make_wordmark(true),
        text("Loom") | bold | color(palette.text) | ftxui::center,
        text(""),
    }) | size(WIDTH, EQUAL, kWelcomeFixedWidth);
}

} // namespace loom::ui::logo
