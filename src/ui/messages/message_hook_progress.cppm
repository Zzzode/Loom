/// @file message_hook_progress.cppm
/// @brief Hook execution progress message rendering  (+ spinner + percent)
///
///   ⏳ Running hooks...  [████████░░] 60%
///   ◐ pre-run / load-config     120ms
///   ● pre-run / validate         34ms
///   ○  post-run / cleanup       pending
module;

#include <ftxui/component/animation.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <cmath>

export module loom.ui.messages.message_hook_progress;

import std;

import loom.ui.foundation.design_figures;

import loom.ui.foundation.theme_provider;

import loom.ui.messages.message_components;

export namespace loom::ui::messages {

using namespace ftxui;

/// Hook execution state
enum class HookState {
    Pending,
    Running,
    Completed,
    Failed,
    Skipped,
};

/// Hook progress entry
struct HookProgressEntry {
    std::string hook_name;
    HookState state{HookState::Pending};
    std::optional<double> duration_ms;
    std::optional<std::string> error;
};

/// ─── Stateless element renderer (kept for non-interactive contexts) ───
[[nodiscard]] inline Element render_hook_progress(const std::vector<HookProgressEntry>& hooks) {
    if (hooks.empty()) return text("");

    std::vector<Element> elements;
    for (const auto& hook : hooks) {
        auto [indicator, ind_color] = [&]() -> std::pair<std::string, Color> {
            switch (hook.state) {
            case HookState::Pending:
                return {"○", loom::ui::design::theme::current_theme().palette->muted};
            case HookState::Running:
                return {"◐", loom::ui::design::theme::current_theme().palette->warning};
            case HookState::Completed:
                return {"●", loom::ui::design::theme::current_theme().palette->success};
            case HookState::Failed:
                return {loom::ui::design::figures::kCross.data(),
                        loom::ui::design::theme::current_theme().palette->danger};
            case HookState::Skipped:
                return {"⊘", loom::ui::design::theme::current_theme().palette->muted};
            }
            return {"?", loom::ui::design::theme::current_theme().palette->text};
        }();

        std::string suffix;
        if (hook.duration_ms) suffix = std::format(" ({:.0f}ms)", *hook.duration_ms);

        elements.push_back(hbox({
            text(indicator) | color(ind_color),
            text(" " + hook.hook_name + suffix),
        }));

        if (hook.error) {
            elements.push_back(text("    " + *hook.error) |
                               color(loom::ui::design::theme::current_theme().palette->danger) |
                               dim);
        }
    }

    return vbox(elements);
}

// ─── Aggregate helper ──────────────────────────────────────────────────

struct HookProgressSummary {
    int total = 0;
    int completed = 0;
    int failed = 0;
    int running = 0;
    double percent = 0.0;  // 0.0 - 100.0
};

[[nodiscard]] inline HookProgressSummary Summarize(const std::vector<HookProgressEntry>& hooks) {
    HookProgressSummary s;
    s.total = static_cast<int>(hooks.size());
    for (const auto& h : hooks) {
        switch (h.state) {
        case HookState::Completed:
            ++s.completed;
            break;
        case HookState::Failed:
            ++s.failed;
            break;
        case HookState::Running:
            ++s.running;
            break;
        default:
            break;
        }
    }
    const int done = s.completed + s.failed;
    s.percent = s.total > 0 ? (100.0 * done / s.total) : 100.0;
    return s;
}

// ─── Interactive component (spinner + progress bar + dismiss) ──────────

class HookProgressComponent : public ComponentBase {
public:
    using OnDismissFn = std::function<void()>;

    explicit HookProgressComponent(std::vector<HookProgressEntry> hooks,
                                   OnDismissFn on_dismiss = nullptr)
        : hooks_(std::move(hooks))
        , on_dismiss_(std::move(on_dismiss)) {
        if (on_dismiss_) {
            dismiss_btn_ =
                Button((" " + std::string(loom::ui::design::figures::kCross) + " dismiss "),
                       [this] {
                           if (on_dismiss_) on_dismiss_();
                       }) |
                dim;
            Add(dismiss_btn_);
        }
    }

    Element Render() override {
        auto s = Summarize(hooks_);

        // Header: animated spinner + title + percentage
        const std::string spin_char{
            loom::ui::design::theme::current_theme().a11y.reduced_motion
                ? loom::ui::design::figures::kBullet
                : loom::ui::design::figures::spinner_frame_glyph(static_cast<int>(
                      tick_count_ % loom::ui::design::figures::kSpinnerFrameCount))};

        // Progress bar: length = 20 chars
        constexpr int kBarWidth = 20;
        const int filled =
            std::min(kBarWidth, static_cast<int>(std::round(s.percent / 100.0 * kBarWidth)));
        const std::string_view kFill = "█";
        const std::string_view kEmpty = "░";
        std::string bar = "[";
        bar.reserve(kBarWidth * 3 + 2);
        for (int i = 0; i < filled; ++i)
            bar.append(kFill);
        for (int i = 0; i < kBarWidth - filled; ++i)
            bar.append(kEmpty);
        bar += "]";

        Color accent = s.failed > 0    ? loom::ui::design::theme::current_theme().palette->danger
                       : s.running > 0 ? loom::ui::design::theme::current_theme().palette->warning
                                       : loom::ui::design::theme::current_theme().palette->success;

        Element dismiss_el = on_dismiss_ && dismiss_btn_ ? dismiss_btn_->Render() : text("");
        auto header = hbox({
            text(spin_char + " ") | color(accent) |
                (loom::ui::design::theme::current_theme().a11y.reduced_motion ? nothing : blink),
            text("Running hooks") | bold,
            text(std::format("  {:>3.0f}%", s.percent)) | bold | color(accent),
            text("  " + bar) | color(accent),
            filler(),
            dismiss_el,
        });

        auto meta = text(std::format("  {} completed / {} failed / {} total", s.completed, s.failed,
                                     s.total)) |
                    dim;

        // Per-hook list (collapsible)
        Elements rows;
        if (!collapsed_) {
            for (const auto& hook : hooks_) {
                auto [ind, col] = [&]() -> std::pair<std::string, Color> {
                    switch (hook.state) {
                    case HookState::Pending:
                        return {"○", loom::ui::design::theme::current_theme().palette->muted};
                    case HookState::Running:
                        return {spin_char,
                                loom::ui::design::theme::current_theme().palette->warning};
                    case HookState::Completed:
                        return {"●", loom::ui::design::theme::current_theme().palette->success};
                    case HookState::Failed:
                        return {loom::ui::design::figures::kCross.data(),
                                loom::ui::design::theme::current_theme().palette->danger};
                    case HookState::Skipped:
                        return {"⊘", loom::ui::design::theme::current_theme().palette->muted};
                    }
                    return {"?", loom::ui::design::theme::current_theme().palette->text};
                }();
                std::string suffix;
                if (hook.duration_ms) suffix = std::format("  ({:.0f}ms)", *hook.duration_ms);
                rows.push_back(hbox({
                    text(ind) | color(col),
                    text(" " + hook.hook_name) | dim,
                    text(suffix) | dim |
                        color(loom::ui::design::theme::current_theme().palette->muted),
                }));
                if (hook.error) {
                    rows.push_back(text("    " + *hook.error) |
                                   color(loom::ui::design::theme::current_theme().palette->danger) |
                                   dim);
                }
            }
        }

        auto toggle = text(std::format("  [{}] {} hooks ", collapsed_ ? "+" : "−",
                                       collapsed_ ? "show" : "hide")) |
                      dim | color(loom::ui::design::theme::current_theme().palette->info);

        Elements all;
        all.push_back(header);
        all.push_back(meta);
        if (!rows.empty()) all.push_back(vbox(std::move(rows)) | indent(2));
        all.push_back(toggle);

        // Animate spinner every ~100ms by flagging PostChildrenRender
        ++tick_count_;
        return vbox(std::move(all));
    }

    bool OnEvent(Event event) override {
        if (event == Event::Return || event == Event::Character(' ') ||
            event == Event::Character('c') || event == Event::Character('C')) {
            collapsed_ = !collapsed_;
            return true;
        }
        if (event == Event::Character('x') || event == Event::Character('X')) {
            if (on_dismiss_) on_dismiss_();
            return true;
        }
        return ComponentBase::OnEvent(event);
    }

    // Update hooks state vector from outside (e.g. engine progress events)
    void Update(std::vector<HookProgressEntry> hooks) { hooks_ = std::move(hooks); }

private:
    std::vector<HookProgressEntry> hooks_;
    OnDismissFn on_dismiss_;
    bool collapsed_ = false;
    std::size_t tick_count_ = 0;
    Component dismiss_btn_;
};

[[nodiscard]] inline Component
MakeHookProgressMessage(std::vector<HookProgressEntry> hooks,
                        HookProgressComponent::OnDismissFn on_dismiss = nullptr) {
    return Make<HookProgressComponent>(std::move(hooks), std::move(on_dismiss));
}

}  // namespace loom::ui::messages
