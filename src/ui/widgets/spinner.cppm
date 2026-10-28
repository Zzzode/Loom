module;

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

export module loom.ui.widgets.spinner;

import std;

import loom.ui.foundation.design_figures;

import loom.ui.foundation.theme_provider;

export namespace ui::components {

enum class SpinnerMode { Thinking, Processing, Idle };

struct SpinnerOptions {
    SpinnerMode mode = SpinnerMode::Processing;
    bool reduced_motion = false;
    std::string message;
    bool verbose = false;
    /// Caller-driven animation frame index.  When >= 0, the element renders
    /// frames_[frame % frames.size()] instead of the static first glyph, so a
    /// parent Renderer that ticks the frame actually animates the spinner.
    int frame = -1;
};

namespace detail {

std::vector<std::string> GetSpinnerFrames() {
    const auto& frames = loom::ui::design::figures::kSpinnerFrames;
    return {frames.begin(), frames.end()};
}

/// Number of canonical ordinary-work animation frames.
[[nodiscard]] inline std::size_t SpinnerFrameCount() {
    return GetSpinnerFrames().size();
}

/// Glyph for a given animation frame (wraps modulo frame count).
/// Used by callers (e.g. RenderToolHeader) that drive animation externally
/// and want a single Element instead of an interactive component.
[[nodiscard]] inline std::string SpinnerGlyphAt(int frame) {
    const auto& theme = loom::ui::design::theme::current_theme();
    if (theme.a11y.reduced_motion) return std::string(loom::ui::design::figures::kBullet);
    return std::string(loom::ui::design::figures::spinner_frame_glyph(std::max(0, frame)));
}

class SpinnerImpl {
public:
    SpinnerImpl(const SpinnerOptions& options)
        : options_(options)
        , frames_(GetSpinnerFrames())
        , start_time_(std::chrono::steady_clock::now()) {}

    ftxui::Element Render() {
        const auto& pal = *loom::ui::design::theme::current_theme().palette;
        using namespace ftxui;

        if (options_.mode == SpinnerMode::Idle) {
            return hbox({text(std::string(loom::ui::design::figures::kTick)) | color(pal.success),
                         text(" Idle") | color(pal.muted)});
        }

        if (options_.reduced_motion ||
            loom::ui::design::theme::current_theme().a11y.reduced_motion) {
            auto element = text(std::string(loom::ui::design::figures::kBullet)) | color(pal.muted);
            if (!options_.message.empty()) {
                return hbox({element, text(" " + options_.message) | color(pal.muted)});
            }
            return element;
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
        // Caller-driven frame wins when set; otherwise animate from elapsed time.
        int frame_index = options_.frame >= 0 ? (options_.frame % static_cast<int>(frames_.size()))
                                              : static_cast<int>((elapsed / 120) % frames_.size());

        auto spinner = text(frames_[frame_index]) | color(pal.primary);

        if (options_.message.empty()) {
            return spinner;
        }

        return hbox({spinner, text(" " + options_.message) | color(pal.text)});
    }

private:
    SpinnerOptions options_;
    std::vector<std::string> frames_;
    std::chrono::steady_clock::time_point start_time_;
};

}  // namespace detail

ftxui::Component Spinner(const SpinnerOptions& options = {}) {
    using namespace ftxui;

    auto impl = std::make_shared<detail::SpinnerImpl>(options);

    return Renderer([impl] { return impl->Render(); });
}

ftxui::Element SpinnerElement(const SpinnerOptions& options = {}) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    using namespace ftxui;

    if (options.mode == SpinnerMode::Idle) {
        return hbox({text(std::string(loom::ui::design::figures::kTick)) | color(pal.success),
                     text(" Idle") | color(pal.muted)});
    }

    if (options.reduced_motion || loom::ui::design::theme::current_theme().a11y.reduced_motion) {
        auto element = text(std::string(loom::ui::design::figures::kBullet)) | color(pal.muted);
        if (!options.message.empty()) {
            return hbox({element, text(" " + options.message) | color(pal.muted)});
        }
        return element;
    }

    // Caller-driven frame wins when set (>= 0); otherwise static first glyph.
    auto glyph =
        options.frame >= 0 ? detail::SpinnerGlyphAt(options.frame) : detail::SpinnerGlyphAt(0);
    auto spinner = text(glyph) | color(pal.primary);

    if (options.message.empty()) {
        return spinner;
    }

    return hbox({spinner, text(" " + options.message) | color(pal.text)});
}

}  // namespace ui::components
