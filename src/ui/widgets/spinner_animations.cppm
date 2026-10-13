/// @file spinner_animations.cppm
/// @brief Custom spinner animation sets
module;
#include <cstdint>
#include <ftxui/dom/elements.hpp>
export module loom.ui.widgets.spinner_animations;

import std;
import loom.ui.foundation.design_figures;

import loom.ui.foundation.theme_provider;
export namespace loom::ui::components {
using namespace ftxui;
inline constexpr auto& kDotsAnimation = loom::ui::design::figures::kSpinnerFrames;
inline constexpr std::array<std::string_view, 4> kLineAnimation = {"-","\\","|","/"};
inline constexpr std::array<std::string_view, 8> kCircleAnimation = {"◜","◠","◝","◞","◡","◟","◜","◠"};
[[nodiscard]] inline std::string_view get_spinner_frame(const auto& animation, uint32_t frame) {
    return animation[frame % animation.size()];
}
[[nodiscard]] inline Element render_spinner_with_label(std::string_view label, uint32_t frame) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    return hbox({text(std::string(kDotsAnimation[frame % kDotsAnimation.size()])) | color(pal.info), text(" " + std::string(label))});
}
} // namespace
