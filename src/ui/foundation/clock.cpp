// clock.cpp — impl unit for loom.ui.foundation.clock.
//
// Owns the process-global test override and the steady_now() /
// set_steady_now_for_testing() bodies. The interface (clock.cppm) is
// declaration-only so the inline-def ratchet sees no bodies there.
//
// This unit does NOT touch FTXUI, so `import std;` is safe here (the
// LLVM #184957 aligned-operator-new hazard only bites app impl units that
// import std while the primary's GMF pulls libc++ textually via FTXUI).
module;

#include <chrono>
#include <optional>

module loom.ui.foundation.clock;

import std;

namespace loom::ui::clock {

namespace {

// Process-global test override. Empty ⇒ steady_now() reads the real clock.
// Not atomic: the seam is for single-threaded test replay (RFC 0003 §8.3).
std::optional<std::chrono::steady_clock::time_point> now_override;

} // namespace

std::chrono::steady_clock::time_point steady_now() noexcept {
    return now_override.value_or(std::chrono::steady_clock::now());
}

void set_steady_now_for_testing(
    std::optional<std::chrono::steady_clock::time_point> now) noexcept {
    now_override = std::move(now);
}

} // namespace loom::ui::clock
