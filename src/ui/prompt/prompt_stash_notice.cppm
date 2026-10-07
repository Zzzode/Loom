/// @file prompt_stash_notice.cppm
/// @brief Notice when user input is stashed during processing.
/// Renders "{figures.pointerSmall} Stashed (auto-restores after submit)"
/// with dim styling when a stash is present.  The notice sits above the input
/// area so the user knows their typed text was saved and will be restored
/// after the current request completes.
module;


#include <ftxui/dom/elements.hpp>

export module loom.ui.prompt.prompt_stash_notice;

import std;

import loom.ui.foundation.design_figures;  // kPointerSmall (figures.pointerSmall '›')

export namespace loom::ui::prompt {
using namespace ftxui;

/// Data for the stash notice (rendered above the prompt input).
struct StashNotice {
    std::string stashed_text;   ///< The stashed input text (for context display)
    std::size_t char_count{0};  ///< Character count of the stashed text
};

/// Render the stash notice element.
///
/// The CPP version also shows the character count in dim text so the user
/// knows how much was stashed (useful when the stashed input was very long).
[[nodiscard]] inline Element render_stash_notice(const StashNotice& notice) {
    if (notice.stashed_text.empty()) return text("");

    namespace figs = loom::ui::design::figures;
    return hbox({
        text("  ") | dim,  // left padding = 2
        text(std::string(figs::kPointerSmall) + " Stashed (auto-restores after submit)")
            | dim,
    });
}

} // namespace loom::ui::prompt