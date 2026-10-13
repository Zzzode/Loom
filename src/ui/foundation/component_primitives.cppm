/// @file component_primitives.cppm
/// @brief Keyboard hints and externally driven spinner glyphs.
module;

#include <cstdint>
#include <ftxui/dom/elements.hpp>

export module loom.ui.foundation.component_primitives;

import std;

import loom.ui.foundation.design_tokens;
import loom.ui.foundation.theme_provider;

export namespace loom::ui::design::primitives {

using namespace loom::ui::design::tokens;
using namespace loom::ui::design::theme;

// ─── kbd ─────────────────────────────────────────────────────────────────────
/// Render a keyboard-shortcut hint (⌘K style).
[[nodiscard]] inline ftxui::Element kbd(std::string_view label) {
    using namespace ftxui;
    return hbox({
        ftxui::text(" "),
        ftxui::text(std::string(label)) | inverted | bold,
        ftxui::text(" "),
    });
}

// ─── spinner (braille glyph) ─────────────────────────────────────────────────
/// Braille spinner.  10 frames; pass an external frame counter so the
/// caller can control the tick rate.
inline constexpr std::array<std::string_view, 10> k_spinner_frames = {
    "⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"
};

[[nodiscard]] inline ftxui::Element spinner_glyph(std::uint64_t frame,
                                                  Role role,
                                                  const Theme& theme) {
    using namespace ftxui;
    auto base = theme.color_for(role);
    if (theme.a11y.reduced_motion) {
        return text("●") | color(base);
    }
    const auto idx = frame % k_spinner_frames.size();
    return text(std::string(k_spinner_frames[idx])) | color(base);
}

} // namespace loom::ui::design::primitives
