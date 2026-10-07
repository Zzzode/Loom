/// @file clock.cppm
/// @brief Injectable steady-clock seam (RFC 0003 §8.3).
///
/// The streaming UI has two grace periods that read the wall clock:
///   - the 30s thinking grace (is_streaming_thinking_visible, Render prune,
///     thinking projection in app_render_event.cpp; streaming_ended_at write
///     in app_handle_submit.cpp), and
///   - the 3s collapse grace (was_recently_streaming / mark_streaming in
///     messages_list_payload_row.cpp).
///
/// Both call steady_now() instead of std::chrono::steady_clock::now()
/// directly, so tests can simulate grace-period expiry without sleeping by
/// installing a process-global override via set_steady_now_for_testing().
/// When no override is installed steady_now() is exactly
/// steady_clock::now() — production behavior is unchanged.
///
/// Declaration-only interface: the bodies (and the override storage) live
/// in clock.cpp so the inline-def ratchet (inline_def_check.py) sees zero
/// inline bodies here.
module;

#include <chrono>
#include <optional>

export module loom.ui.foundation.clock;

import std;

export namespace loom::ui::clock {

/// The current steady-clock time.
///
/// Returns the test override installed by set_steady_now_for_testing() when
/// one is present; otherwise returns std::chrono::steady_clock::now().
/// Production behavior is identical to calling steady_clock::now() directly
/// when no override is set.
[[nodiscard]] std::chrono::steady_clock::time_point steady_now() noexcept;

/// Install (or clear, with std::nullopt) a process-global override for
/// steady_now().
///
/// For testing only — production code must never call this. The override is
/// process-global and not synchronized: it is intended for single-threaded
/// test replay (RFC 0003 §8.3), not concurrent use. The replay harness
/// clears it (std::nullopt) at the start of every play() so clock state
/// never leaks across fixtures.
void set_steady_now_for_testing(
    std::optional<std::chrono::steady_clock::time_point> now) noexcept;

} // namespace loom::ui::clock
