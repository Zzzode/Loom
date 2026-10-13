module;

#include <ftxui/dom/elements.hpp>

export module loom.ui.widgets.fast_icon;

import std;

import loom.ui.foundation.theme_provider;

import loom.ui.foundation.design_figures;

export namespace ui::components {

struct FastIconOptions {
    bool cooldown = false;
};

ftxui::Element FastIcon(const FastIconOptions& options = {}) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    using namespace ftxui;
    namespace figs = loom::ui::design::figures;

    if (options.cooldown) {
        return text(std::string(figs::kLightningBolt)) | dim;
    }

    return text(std::string(figs::kLightningBolt)) | color(pal.fast_mode);
}

std::string GetFastIconString(bool apply_color = true, bool cooldown = false) {
    namespace figs = loom::ui::design::figures;

    if (!apply_color) {
        return std::string(figs::kLightningBolt);
    }
    if (cooldown) {
        return std::string(figs::kLightningBolt);
    }

    // Note: Color application would depend on theme system
    return std::string(figs::kLightningBolt);
}

} // namespace ui::components
