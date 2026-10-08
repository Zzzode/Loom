// Event-driven render wakes: bounded welcome animation and one-shot deadlines.
module;

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

module loom.ui.app.app;

import std;

import loom.ui.screens.repl_state;
import loom.ui.screens.task_view_store;

namespace loom::ui {

namespace repl = loom::ui::repl_screen;

void AppAdapter::StartUiAnimationTicker() {
    spinner_thread_ = std::jthread([this](std::stop_token st) {
        constexpr auto kTick = std::chrono::milliseconds(50);
        constexpr int kWelcomeIntroTicks = 80;
        int welcome_render_ticks = 0;
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(kTick);
            if (st.stop_requested()) break;
            bool advanced = false;
            if (welcome_animation_active_.load(std::memory_order_relaxed)) {
                if (welcome_render_ticks < kWelcomeIntroTicks) {
                    ++welcome_render_ticks;
                    advanced = true;
                }
            } else {
                welcome_render_ticks = 0;
            }
            // Thinking collapse is a one-shot transition, not a query ticker.
            auto deadline = thinking_collapse_deadline_ms_.load();
            const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            if (deadline != 0 && now_ms >= deadline &&
                thinking_collapse_deadline_ms_.compare_exchange_strong(deadline, 0)) {
                advanced = true;
            }
            auto paste_due = paste_submission_deadline_ms_.load();
            if (paste_due != 0 && now_ms >= paste_due &&
                paste_submission_deadline_ms_.compare_exchange_strong(paste_due, 0)) {
                advanced = true;
            }
            if (advanced) {
                ui_animation_tick_count_.fetch_add(1, std::memory_order_relaxed);
                PostRenderEvent();
            }
        }
    });
}

void AppAdapter::PostRenderEvent() {
    if (auto* screen = screen_.load(std::memory_order_acquire)) {
        screen->Post(Event::Custom);
    }
}

} // namespace loom::ui
