// app_animation.cpp — plain impl unit for loom.ui.app.app. Owns the animation
// ticker + render-event post bodies (RFC 0001 Phase C batch 2):
//   AppAdapter::StartUiAnimationTicker — the jthread animation ticker
//   AppAdapter::PostRenderEvent        — Post(Event::Custom) helper
//
// Declarations stay in app.cppm. PostRenderEvent is a private member called
// from member functions across six impl units; private access holds within
// member functions regardless of TU, so no friend or access change is
// needed. StartUiAnimationTicker's lambda is moved VERBATIM — same `this`
// capture, same stop_token loop, same TriggerStatuslineUpdate() call.
//
// LLVM #184957: like app_extra_methods.cpp / app_handle_submit.cpp /
// app_prompt_suggestion_wiring.cpp / app_team.cpp / app_run.cpp, this unit
// must NOT `import std;` — under the reduced-BMI writer a cold module cache
// mis-merges the global aligned operator new when an app impl unit imports
// std while the primary's GMF pulls libc++ textually via FTXUI. Keep textual
// std headers in the global module fragment (see CMakeLists.txt:283-292).
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
        // The UI is event-driven: re-renders happen only on state changes,
        // never on a fixed timer.  This ticker exists solely to advance
        // ANIMATIONS (the welcome-intro asterisk hue sweep, the query
        // spinner).  Once the welcome intro has played (asterisk_sweep_ms ×
        // sweep_count = 1500 × 2 = 3000ms ≈ 60 ticks) the screen is static,
        // so we stop forcing re-renders at idle — FTXUI otherwise re-emits
        // the whole frame + cursor-move sequences 20×/s, which flickers on
        // terminals that paint hidden-cursor movement.  Event-driven
        // re-renders (input, queries, statusline, cost hooks) still work
        // normally.
        constexpr int kWelcomeIntroTicks = 80;  // 80 × 50ms = 4s (3s sweep + margin)
        int query_statusline_tick = 0;
        int welcome_render_ticks = 0;
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(kTick);
            if (st.stop_requested()) break;

            const bool query_active = query_running_.load();
            const bool welcome_active =
                screen_state_ &&
                screen_state_->messages_store.messages.empty() &&
                !repl::IsToolAnimating(screen_state_->task_view_store);
            if (!welcome_active) welcome_render_ticks = 0;

            // Re-render only while an animation is actually advancing:
            // an active query (spinner) or the welcome-intro sweep.
            // At static idle we skip — no animation to drive.
            if (query_active) {
                // spinner animation: keep ticking
            } else if (welcome_active &&
                       welcome_render_ticks < kWelcomeIntroTicks) {
                ++welcome_render_ticks;
            } else {
                query_statusline_tick = 0;
                continue;
            }

            ui_animation_tick_count_.fetch_add(1, std::memory_order_relaxed);
            PostRenderEvent();

            if (query_active && ++query_statusline_tick % 20 == 0) {
                this->TriggerStatuslineUpdate();
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
