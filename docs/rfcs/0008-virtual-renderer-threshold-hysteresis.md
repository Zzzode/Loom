---
rfc: 8
title: Virtual Renderer Threshold Hysteresis
status: provisional
owners: "@Zzzode"
reviewers:
  - "Dev-0 (agent)"
  - "Dev-2 (agent)"
created: 2026-10-10
last-reviewed: 2026-10-10
tracking: local
---

# RFC 0008 — Virtual Renderer Threshold Hysteresis

## 1. Summary

The message list switches between a static renderer and a virtual (windowed)
renderer at a hard threshold of 80 visible entries with **no hysteresis**.
When the visible entry count oscillates around 80 — driven by streaming
deltas, chain merges/splits, or projection rebuilds — the renderer flips
between two structurally different output trees on consecutive frames.
FTXUI repaints the full viewport on every frame (no cell-level diff), so a
mode switch rewrites the entire terminal viewport.

This RFC adds a hysteresis band: enter virtual mode at `> 80`, exit only at
`< 60`. The mode type is defined in the messages layer; the persistent state
lives in `MessagesStore` (screens layer) and is passed to the view through
`MessagesListInput`. The view computes the next mode and writes it back
through an output field, preserving the screens → messages dependency
direction.

The on-site correlation between mode switches and the user-reported flicker
is **not yet captured** (§2.3). This RFC fixes the threshold oscillation as
a correctness defect regardless; telemetry added here will verify the
on-site link post-merge.

A follow-up RFC will unify the two renderer paths (Option C) so the virtual
path also renders leading/trailing elements.

## 2. Motivation

### 2.1 User report

> 现在存在一个问题，就是有一个长任务出来以后，后面的 Box 在展示的过程中会偶尔会全屏刷新一下，也就是前面的 Box 也会重新做一次闪烁。

The user expects that when a new Box (message block) appears, only the new
Box renders — previous Boxes are unaffected. In practice, during long tasks
the entire screen occasionally flickers.

### 2.2 Root cause (three-bot investigation, 2026-10-10)

Dev-0, Dev-2, and Dev-1 independently investigated and converged on a
multi-layer analysis.

#### Layer 1: Stateless threshold switch

`messages_list_view.cpp:335-345`:

```cpp
constexpr std::size_t kBigChatThreshold = kMaxRenderedLastN;  // 80
auto visible = build_visible_rows(input);
if (visible.size() > kBigChatThreshold) {
    (void)trailing_elements;  // DISCARDED
    return render_messages_list_virtual_rows(...);
}
// static path: vbox with leading_elements + rows + trailing_elements
```

The check is evaluated fresh every frame with no memory of the previous
frame's mode. When `visible.size()` crosses 80 in either direction, the
renderer switches paths. The two paths produce **structurally different
element trees**:

| Aspect | Static path (≤ 80) | Virtual path (> 80) |
|--------|--------------------|---------------------|
| Leading elements (logo, welcome card) | Rendered | Discarded |
| Trailing filler (elastic spacer) | Rendered | Discarded |
| Layout | `vbox(leading + rows + trailing)` | `spacer + slice + spacer` |
| Scroll coordinate system | FTXUI yframe native | VirtualListState (JumpHandle) |

A mode switch therefore changes the position and identity of every row in
the tree, not just the new row.

#### Layer 2: FTXUI full-viewport repaint

FTXUI's `ScreenInteractive::Draw()` (`screen_interactive.cpp:729-820`)
calls `Screen::ToString()` (`screen.cpp:416-446`), which serializes **all
pixels** every frame — there is no cell-level diff against the previous
frame. `Clear()` (`screen.cpp:519-531`) resets the in-memory pixel buffer
after each frame. Every frame is a full-viewport rewrite.

This means a mode switch is not a localized update — the entire terminal
viewport is rewritten. Whether the structural difference between paths is
perceived as flicker depends on the terminal's frame application timing and
the user's visual sensitivity; the on-site link is not yet captured
(§2.3).

#### Layer 3: What can change the visible count

The threshold checks `visible.size()` — the count of **visible entries**
after `build_visible_rows()`, not the visual line count. Events that can
change the entry count include:

- Streaming deltas that add or merge entries (chain merges/splits)
- `conversation_projection_dirty_` triggering a full projection rebuild
  (on commit, SyncState)
- Search/filter changes

Whether thinking-block collapse or tool-preview expansion changes the
*entry count* (as opposed to visual line count) is not guaranteed by the
current code — these may or may not cross the threshold depending on
whether they add/remove entries or only change row heights.

#### Layer 4: Viewport height lag (secondary, independent)

`repl_screen_layout.cpp:90-99` uses the **previous frame's** Box height for
the viewport. When Todo/Spinner/teammate strip changes bottom height, this
frame uses the old height, next frame corrects — a two-step position change.
This is independent of the threshold oscillation and out of scope for this
RFC.

### 2.3 What is confirmed vs not confirmed

| Claim | Confidence | Evidence |
|-------|-----------|----------|
| FTXUI repaints full viewport every frame | Confirmed | Source: `screen.cpp:416-446` |
| Threshold switch is stateless (no hysteresis) | Confirmed | Source: `messages_list_view.cpp:335-345` |
| Two paths produce structurally different trees | Confirmed | Source: leading/trailing discarded in virtual path |
| Visible count can cross 80 during long tasks | Plausible | Chain merges/splits, projection rebuilds change entry count; wild frequency not yet captured |
| Mode switches cause the on-site flicker | **Not yet captured** | Conditional chain is valid; PTY + telemetry needed to verify |
| LOOM_DEBUG stderr pollution causes flicker | **Being checked** | `debug("app.render", ...)` at `app_render_event.cpp:200`; user checking env |
| ioctl EINTR → 80×24 fallback → `\x1b[2K` clear | 0.05 | Linux TIOCGWINSZ reads locked winsize; EINTR unlikely |

The threshold oscillation is a **correctness defect regardless of whether
it is the on-site flicker's primary cause**: a renderer that flips between
two structurally different modes at a hard boundary is inherently unstable.
This RFC fixes that defect and adds telemetry to verify the on-site link.

### 2.4 Evidence

| Metric | Current | Target | How measured |
|--------|---------|--------|-------------|
| Mode switches for entry sequences staying in [60, 80] | Unbounded (oscillation at 80) | 0 (hysteresis band) | `messages.virtual` debug channel, stderr redirected to file |
| Leading/trailing elements after virtual switch | Lost | Lost (unchanged — Option C follow-up) | Renderer-parity tests (future RFC) |
| Full-viewport repaints per frame | 1 (FTXUI architecture) | 1 (unchanged — out of scope) | FTXUI source |

## 3. Goals

- G1. The renderer mode does not oscillate when `visible.size()` fluctuates
  within the hysteresis band [60, 80]. Once virtual mode is entered, it
  stays entered until `visible.size()` drops below 60. Sequences that
  cross both thresholds (e.g., 59→81→59) still switch modes — this is
  correct behavior.
- G2. Mode state is deterministic and resettable: empty list, first render,
  session restore, and `/clear` all start in static mode. Resize preserves
  the current mode.
- G3. Mode state is updated only by the UI thread that completes a list
  render — never by telemetry, background events, or stream callbacks.
- G4. Frame telemetry records `visible.size()`, current mode, and
  mode-switch counter, so threshold oscillation can be measured in the wild.
  The counter is UI-thread-affined, resets on `/clear` and fresh session,
  and increments only on actual mode change.
- G5. Regression tests prove the hysteresis decision logic: 79→81→79 stays
  virtual until < 60; 60→59 returns to static; mode state survives
  `/clear` → Static, session restore → Static, and resize → unchanged.
  Pin/scroll numeric state is preserved across mode switches. This RFC
  does **not** promise cross-mode visual parity (leading/trailing
  preservation) — that is Option C territory.

## 4. Non-Goals

- **Unifying the static and virtual renderer paths** (Option C). The
  virtual path currently discards leading/trailing elements. Fixing this is
  a larger correctness change deferred to a follow-up RFC. This RFC only
  prevents oscillation between the two paths.
- **Cross-mode visual parity.** After this RFC, entering virtual mode at
  > 80 still loses logo/filler until < 60. The 60–80 band now stays
  virtual (previously it would have returned to static at ≤ 80). This is
  a deliberate trade-off: stability over visual completeness in the band.
- **Reducing FTXUI's full-viewport repaint frequency.** FTXUI has no
  cell-level diff; changing this would require upstream FTXUI changes or a
  custom renderer. Out of scope.
- **Throttling stream-event render posts.** `handle_stream_event()` calls
  `PostRenderEvent()` on every accepted event. Throttling is a separate
  performance optimization.
- **Fixing viewport height lag** (`repl_screen_layout.cpp:90-99` uses
  previous frame's Box height). Independent issue, separate fix.
- **Synchronized Output (?2026 BSU/ESU).** Would reduce perceived flicker
  by batching frame output, but requires terminal capability detection and
  is a separate enhancement.

## 5. Proposal

### 5.1 Hysteresis band

Replace the single threshold with a two-threshold hysteresis band:

```
static ──[visible.size() > 80]──▶ virtual
virtual ──[visible.size() < 60]──▶ static
```

Named constants in `messages_list.cppm`, with `static_assert` at the
definition site:

```cpp
/// Enter virtual (windowed) rendering when visible entries exceed this.
inline constexpr std::size_t kVirtualEnterThreshold = 80;
/// Exit virtual rendering only when visible entries drop below this.
inline constexpr std::size_t kVirtualExitThreshold = 60;
static_assert(kVirtualExitThreshold < kVirtualEnterThreshold,
              "hysteresis exit must be below enter");
```

The existing `kMaxRenderedLastN = 80` is kept — it still caps the static
path's last-N render. The existing `kVirtualThreshold = 80` is **removed**
after its only remaining consumer (the dead `MessagesListComponent`) is
deleted (§5.6).

Boundary semantics are strict: enter at `> 80` (not `>= 80`), exit at
`< 60` (not `<= 60`). At exactly 80, the mode stays Static (or stays
Virtual if already in Virtual). At exactly 60, the mode stays Virtual.

### 5.2 Mode type in the messages layer

Define the mode enum in `messages_list.cppm` (the messages layer), not in
`screens/messages_store.cppm`. This preserves the dependency direction:
screens already imports messages (`messages_store.cppm:28`); messages must
not import screens.

```cpp
/// Which renderer path is active for the message list.
/// Persisted across frames by the caller (screens layer) to implement
/// hysteresis at the virtual-renderer threshold.
enum class VirtualRendererMode : std::uint8_t {
    Static,
    Virtual,
};
```

### 5.3 Mode passing through MessagesListInput

The mode is passed to the view through `MessagesListInput` and written back
through a `mutable` output field — the same pattern as `virtual_jh_out`
(`messages_list.cppm:397`).

Add to `MessagesListInput`:

```cpp
/// Previous frame's renderer mode.  Set by the caller (screens layer)
/// from MessagesStore.  Defaults to Static for stateless callers
/// (golden tests, dialogs, first render).
VirtualRendererMode prev_virtual_mode = VirtualRendererMode::Static;

/// This frame's renderer mode, computed by the view from
/// prev_virtual_mode + visible.size().  The caller reads this after
/// render returns and stores it in MessagesStore.
mutable VirtualRendererMode next_virtual_mode = VirtualRendererMode::Static;
```

The view's decision logic (`messages_list_view.cpp`):

```cpp
const auto visible_count = visible.size();
const auto prev_mode = input.prev_virtual_mode;

VirtualRendererMode next_mode;
switch (prev_mode) {
    case VirtualRendererMode::Static:
        next_mode = (visible_count > kVirtualEnterThreshold)
                    ? VirtualRendererMode::Virtual
                    : VirtualRendererMode::Static;
        break;
    case VirtualRendererMode::Virtual:
        next_mode = (visible_count < kVirtualExitThreshold)
                    ? VirtualRendererMode::Static
                    : VirtualRendererMode::Virtual;
        break;
}

input.next_virtual_mode = next_mode;

if (next_mode != prev_mode) {
    debug("messages.virtual", "mode switch {} → {} at visible={}",
          static_cast<int>(prev_mode), static_cast<int>(next_mode),
          visible_count);
}

if (next_mode == VirtualRendererMode::Virtual) {
    (void)trailing_elements;
    return render_messages_list_virtual_rows(...);
}
// static path...
```

The caller (`repl_screen_messages.cpp`) reads `next_virtual_mode` after
render and stores it:

```cpp
auto element = ml::render_messages_list_view(input, ...);
if (store) {
    store->virtual_renderer_mode = input.next_virtual_mode;
    // existing virtual_list_active / virtual_jh propagation...
}
```

**Stateless callers** (golden tests, dialogs that don't set
`prev_virtual_mode`) always default to Static. This is correct: stateless
callers have no persistent state, so hysteresis doesn't apply.

**The interactive `MessagesListComponent` is excluded** — it is dead code
(§5.6) and is deleted in this RFC.

### 5.4 Persistent state in MessagesStore

Add to `messages_store.cppm`:

```cpp
/// Hysteresis state for the virtual renderer threshold.
/// Persisted across frames so the mode does not oscillate when
/// visible.size() fluctuates within the hysteresis band.
/// Updated ONLY by the UI thread after completing a list render
/// (repl_screen_messages.cpp reads input.next_virtual_mode).
VirtualRendererMode virtual_renderer_mode = VirtualRendererMode::Static;

/// Cumulative mode transitions this session.  UI-thread-affined.
/// Resets on /clear and fresh session.  Increments only on actual
/// mode change (not every frame).
std::uint32_t virtual_mode_switch_count = 0;
```

The existing `virtual_list_active` bool is kept for scroll-path consumers
(`repl_screen_scroll.cpp`, `repl_screen_layout.cpp`) — it is set per-frame
from the mode and the JumpHandle output, exactly as today. The new
`virtual_renderer_mode` is the **persistent decision**; `virtual_list_active`
is the **per-frame observation**.

### 5.5 Invariants

**INV-1: Mode state is updated only by the UI thread that completes a list
render.** Telemetry, stream callbacks, background events, and animation
tickers must not write `virtual_renderer_mode` or
`virtual_mode_switch_count`. The write happens exclusively in
`repl_screen_messages.cpp` (the RenderMessages caller), which runs on the
FTXUI UI thread, reading `input.next_virtual_mode` computed by the view.
This prevents event reordering from causing state regression.

**INV-2: Initial mode is always Static.** The following events reset
`virtual_renderer_mode` to Static and `virtual_mode_switch_count` to 0:

| Event | Reset location | Mechanism |
|-------|---------------|-----------|
| First render (fresh store) | `messages_store.cppm` default member init | `= VirtualRendererMode::Static` |
| `/clear` | `app_handle_submit.cpp:414-422` (clear handler) | Explicit `store.virtual_renderer_mode = Static; store.virtual_mode_switch_count = 0;` added alongside existing divider/scroll resets |
| Session restore (runtime) | `app_handle_submit.cpp:668-677` (restore handler) | Explicit reset added alongside existing restore logic |
| Session restore (startup) | Fresh `MessagesStore` construction | Default init |

**Resize does NOT reset the mode.** Resize is not a new list lifecycle;
forcing a > 80 virtual list back to static on resize would re-introduce
the last-N cap and a needless structural switch. On resize, the mode is
preserved and geometry is recomputed.

**SyncState does NOT reset the mode.** SyncState is called on every commit
and query completion; resetting there would discard hysteresis state on
every streaming commit, defeating the purpose. SyncState only clears the
messages vector (existing behavior).

**INV-3: Exit threshold < Enter threshold.** Compile-time assertion at the
constant definition site: `static_assert(kVirtualExitThreshold <
kVirtualEnterThreshold)`.

### 5.6 Delete dead MessagesListComponent

`MessagesListComponent` (`messages_list_component.cpp`,
`messages_list.cppm:803-817`) has **zero callers** in production code and
tests. It is dead code. It also contains the second `kVirtualThreshold`
consumer (`messages_list_component.cpp:335`) with the same stateless
threshold defect.

Per project convention ("prefer deleting dead code to fixing it"), this
RFC deletes:

- `src/ui/messages/messages_list_component.cpp` (entire file)
- `MessagesListComponent` class + `MakeMessagesList` factory from
  `messages_list.cppm:803-817`
- `messages_list_component.cpp` entry from
  `src/cmake/targets/loom_ui_messages.cmake:40`
- `kVirtualThreshold` constant from `messages_list.cppm:634` (after the
  component is its only remaining consumer)

This removes the second threshold implementation and prevents the RFC from
fixing one path while leaving a public API with the same defect.

### 5.7 Frame telemetry

Add a `debug("messages.virtual", ...)` call at the mode-decision site in
`messages_list_view.cpp`. The debug channel is `messages.virtual`.

**Verification requires stderr redirection.** `loom::utils::debug()` writes
to `std::cerr` (`debug.cppm:55-60`). If stderr shares the TTY with the TUI,
it corrupts the display. To verify telemetry without polluting the UI:

```bash
LOOM_DEBUG=messages.virtual loom 2>/tmp/loom-debug.log
```

The PTY captures only stdout (UI output); stderr goes to the log file.

**Baseline capture:** To prove the fix reduces mode switches, capture
telemetry before and after:
1. Revert the RFC changes, build, run a long task with
   `LOOM_DEBUG=messages.virtual 2>/tmp/baseline.log`
2. Re-apply the RFC changes, build, run the same task with
   `LOOM_DEBUG=messages.virtual 2>/tmp/with-hysteresis.log`
3. Compare `mode switch` count in both logs

The telemetry logs:
- `visible.size()` — current visible entry count (every mode switch)
- `prev_mode → next_mode` — the transition
- `virtual_mode_switch_count` — cumulative counter (from MessagesStore)

**LOOM_DEBUG env var format:** `LOOM_DEBUG=messages.virtual` (glob match
against the namespace). `LOOM_DEBUG=1` does NOT match — the filter uses
glob namespace matching (`debug_filter.cppm:77`), and `"1"` does not match
`"messages.virtual"`.

### 5.8 Interface surface

No new public modules. No new config keys. No new commands. Changes are
confined to:

| File | Change |
|------|--------|
| `src/ui/messages/messages_list.cppm` | Add `VirtualRendererMode` enum, `kVirtualEnterThreshold`, `kVirtualExitThreshold`; add `prev_virtual_mode` / `next_virtual_mode` to `MessagesListInput`; remove `kVirtualThreshold`, `MessagesListComponent`, `MakeMessagesList` |
| `src/ui/messages/messages_list_view.cpp` | Hysteresis decision logic; write `next_virtual_mode`; telemetry |
| `src/ui/screens/messages_store.cppm` | Add `virtual_renderer_mode` + `virtual_mode_switch_count` fields |
| `src/ui/screens/repl_screen_messages.cpp` | Set `input.prev_virtual_mode` from store; read `input.next_virtual_mode` after render; increment switch counter |
| `src/ui/app/app_handle_submit.cpp` | Reset mode + counter in `/clear` handler (:414-422) and session restore handler (:668-677) |
| `src/cmake/targets/loom_ui_messages.cmake` | Remove `messages_list_component.cpp` entry |

No wire-protocol, persisted-shape, or config changes.

**Golden impact:** Mode selection changes at the 80-entry boundary. Golden
tests that exercise exactly 80 visible entries with a persistent
`prev_virtual_mode` may produce different output (static vs virtual).
Golden tests that use the default (Static) `prev_virtual_mode` are
unaffected — they always take the static path at ≤ 80. Golden tests must
be reviewed for any that construct > 80 visible entries; if found, their
output is expected to change (virtual path) and must be re-baselined with
`UPDATE_GOLDENS=1`.

## 6. Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|-------|-------|-------|--------|-------------------------------|
| A | Hysteresis + telemetry + dead code deletion | Constants, mode type, MessagesStore fields, decision logic, telemetry, delete MessagesListComponent, reset on clear/restore | proposed | All tests pass (debug + release serial ctest); regression tests prove G1/G2/G5; `graph_check.py --target-ui9` 0 cycles; BMI PSS delta < 5% for `messages_list.cppm` and `messages_store.cppm` producers (measured before/after); no new `kVirtualThreshold` references |

**Follow-up (separate RFC):** Option C — unify static and virtual renderer
paths so the virtual path also renders leading/trailing elements in a
unified content coordinate system. This RFC's hysteresis design is
compatible with the future unified path: the mode type and MessagesStore
field carry over; only the view's internal branching changes.

## 7. Production Readiness Review

| Question | Answer |
|----------|--------|
| Is the change backward-compatible? | Yes — no persisted state, no config, no wire changes. `MessagesListComponent` deletion is safe (zero callers). |
| Is there a rollback story? | Yes — revert the commit. The mode falls back to stateless `visible.size() > 80`. `MessagesListComponent` would need to be restored from git history if any out-of-tree caller exists (none found). |
| Are there new failure modes? | Mode could get "stuck" in virtual if visible count stays in [60, 80]. This is the deliberate hysteresis trade-off: stability over visual completeness. The virtual path is strictly more capable for large transcripts. |
| Is telemetry safe? | Yes — counts and mode names only, no message content. Requires stderr redirection to avoid TTY pollution. |
| Are there edge cases? | Empty list → Static (default). First render → Static (default). `/clear` → Static (explicit reset). Session restore → Static (explicit reset). Resize → mode preserved, geometry recomputed. SyncState → mode preserved. Exactly 80 → stays Static (or stays Virtual). Exactly 60 → stays Virtual. |
| Performance impact? | Negligible — one branch + one enum write per frame. `build_visible_rows()` is already called every frame. |
| Golden test impact? | Tests with > 80 visible entries and persistent mode may change. Tests with default (Static) mode at ≤ 80 are unaffected. Must review and re-baseline if needed. |
| Thread safety? | Mode state is UI-thread-affined (INV-1). No locking needed — FTXUI's event loop is single-threaded. The `mutable` output field on `MessagesListInput` follows the existing `virtual_jh_out` pattern. |

## 8. Rollout and rollback

- **Rollout:** Single commit. No feature flag (the fix is a correctness
  improvement to an internal renderer decision; no user-visible config).
- **Rollback:** `git revert` the commit. The mode falls back to the
  stateless `visible.size() > 80` check. `MessagesListComponent` is
  restored from git history. No data migration needed.
- **Ordering:** No dependencies on other RFCs. Can land independently.

## 9. Drawbacks

1. **Mode can get "stuck" in virtual in the 60–80 band.** If a session
   grows past 80 entries and shrinks to 70, the renderer stays in virtual
   mode. Previously it would have returned to static at ≤ 80, restoring
   the logo and filler. This is the deliberate hysteresis trade-off:
   stability (no oscillation) over visual completeness (logo/filler) in
   the band. The 60–80 band is 25% of the enter threshold — wide enough
   to absorb normal streaming fluctuation.

2. **Two constants to tune instead of one.** The exit threshold (60) is a
   judgment call. Too close to 80 and the band is too narrow to prevent
   oscillation; too far and the renderer stays virtual longer than needed.
   60 provides a 20-entry band which is wide enough to absorb normal
   streaming fluctuation without being so wide that the renderer never
   exits virtual for medium transcripts.

3. **Leading/trailing elements are still lost in virtual mode.** This is
   the known Option C defect. The hysteresis band makes mode switches rarer
   but does not fix the structural difference between paths. Users with
   > 80 visible entries still lose the logo and filler until < 60.

4. **Dead code deletion is irreversible without git history.** If an
   out-of-tree caller of `MessagesListComponent` exists (none found in
   this repo), it would break. The component is restored by reverting the
   commit.

## 10. Alternatives considered

### A. Hysteresis band (this RFC)

Enter at > 80, exit at < 60. Minimal change, directly addresses the
oscillation trigger. **Selected.**

### B. Raise the threshold to 200+

Simply moving the threshold higher reduces how often it is crossed but
does not fix the structural defect: any session that grows past the new
threshold will oscillate at the new boundary. It also delays the
performance benefit of the virtual renderer for sessions between 80 and
200 entries. **Rejected** — moves the oscillation, does not eliminate it.

### C. Unify the two renderer paths

Make the virtual path also render leading/trailing elements in a unified
content coordinate system. This eliminates the structural difference
between paths, making mode switches invisible. **Deferred to a follow-up
RFC** — larger change, needs renderer-parity tests first, and the
hysteresis band is a sufficient incident fix. This RFC's design is
forward-compatible with Option C.

### D. Do nothing

The oscillation is a correctness defect. Even if it is not the primary
on-site flicker cause (LOOM_DEBUG is being checked), the stateless
threshold is inherently unstable. **Rejected.**

### E. Synchronized Output (?2026 BSU/ESU)

Wrap frame output in Begin/End Synchronized Update sequences so the
terminal applies the frame atomically. This would reduce *perceived*
flicker for all repaints, not just mode switches. However, it requires
terminal capability detection, a fallback path, and does not address the
structural defect. **Deferred** — separate enhancement, orthogonal to this
RFC.

### F. Add hysteresis to MessagesListComponent instead of deleting it

The component has zero callers. Adding hysteresis to dead code is wasted
effort. **Rejected** — delete per project convention.

## 11. Testing and verification plan

### 11.1 New regression tests

Tests live in `tests/ui/test_ui_messages.cpp` (existing file for message
list view tests).

| Test | Scenario | Expected |
|------|----------|----------|
| `VirtualThreshold.StaysVirtualAboveExit` | prev=Static, 79→81→79 | Mode enters Virtual at 81, stays Virtual at 79 |
| `VirtualThreshold.ReturnsToStaticBelowExit` | prev=Virtual, 81→60→59 | Mode stays Virtual at 60, returns to Static at 59 |
| `VirtualThreshold.InitialModeIsStatic` | Default-constructed input, empty list | Mode is Static |
| `VirtualThreshold.StatelessCallerDefaultsStatic` | No prev_virtual_mode set, 100 entries | Mode is Virtual (enters at > 80 from Static default) |
| `VirtualThreshold.ClearResetsMode` | Virtual mode → /clear → 70 entries | Mode is Static after clear; stays Static at 70 |
| `VirtualThreshold.SessionRestoreResetsMode` | Virtual mode → restore 70 entries | Mode is Static after restore; stays Static at 70 |
| `VirtualThreshold.ResizePreservesMode` | Virtual mode → resize → 70 entries | Mode stays Virtual |
| `VirtualThreshold.SyncStatePreservesMode` | Virtual mode → SyncState → 70 entries | Mode stays Virtual |
| `VirtualThreshold.ExactBoundary80` | prev=Static, exactly 80 | Mode stays Static (> 80 is strict) |
| `VirtualThreshold.ExactBoundary60` | prev=Virtual, exactly 60 | Mode stays Virtual (< 60 is strict) |
| `VirtualThreshold.PinScrollNumericStatePreserved` | Pinned + scrolled states at both transitions | Numeric pin/scroll values unchanged across mode switch |
| `VirtualThreshold.SwitchCounterIncrements` | Two mode transitions | Counter = 2; resets to 0 on /clear |

### 11.2 Existing tests

- `tests/ui/test_ui_messages.cpp` — existing tests use default
  `prev_virtual_mode = Static`; those with ≤ 80 entries are unaffected.
  Any test with > 80 entries must be reviewed for mode change.
- Golden tests in `tests/ui/test_ui_render.cpp` — review for > 80 visible
  entries; re-baseline with `UPDATE_GOLDENS=1` if output changes.

### 11.3 Build and test

- `cmake --workflow --preset dev` (debug build)
- `ctest --preset debug -j$(nproc)` — all tests pass
- `cmake --workflow --preset ci` (release build + test)
- `ctest --preset release -j$(nproc)` — all tests pass
- Serial ctest (debug + release) as the signal gate; known timing flakes
  (`ServerMain.DirectConnect*`, `Tools.RuntimeTeamCreate*`,
  `HooksIntegrationTest.BlockingHookFiresOnEveryToolCall`) re-run in
  isolation if they fail under parallel load
- `graph_check.py --target-ui9` — 0 cycles
- BMI PSS: measure `messages_list.cppm` and `messages_store.cppm`
  producer PSS before/after; delta < 5%
- Inline ratchet: no new inline violations

### 11.4 PTY + telemetry verification (post-merge)

1. Build with RFC changes
2. Run a long task: `LOOM_DEBUG=messages.virtual loom 2>/tmp/loom-debug.log`
3. PTY captures stdout (UI output) only
4. Verify in `/tmp/loom-debug.log`:
   - Mode switches occur only at threshold crossings (enter at > 80, exit
     at < 60)
   - No oscillation (no repeated enter/exit within the [60, 80] band)
   - Counter increments match actual mode transitions
5. Baseline comparison: revert RFC, repeat, compare switch count
6. If LOOM_DEBUG is the on-site flicker cause, telemetry will show zero
   mode switches during the flicker, exonerating the threshold

## 12. Documentation impact

- [ ] `CLAUDE.md` — no changes needed (internal renderer detail)
- [ ] `docs/decisions/design-decisions.md` — add entry for hysteresis band
  and MessagesListComponent deletion
- [ ] Code comments — document INV-1/INV-2/INV-3 at the decision site

## 13. Open questions

| Question | Owner | Resolved by |
|----------|-------|-------------|
| Is LOOM_DEBUG the on-site flicker cause? | 张地 | env check |
| What is the actual mode-switch frequency in the wild? | Dev-1 | Post-merge telemetry (§11.4) |
| Should the exit threshold be configurable? | Dev-1 | Post-merge data (default: no) |
| When should Option C (path unification) be scheduled? | Dev-1 | After this RFC graduates + telemetry data |

## 14. Implementation History

| Date | Phase | Description | Commit | Tests | Review |
|------|-------|-------------|--------|-------|--------|
| 2026-10-10 | — | RFC drafted (provisional) | 49d31f8 | — | — |
| 2026-10-10 | — | Design review round 1: Dev-0 REQUEST_CHANGES (3 P0 + 2 P1), Dev-2 REQUEST_CHANGES (5 P1 + 4 P2) | — | — | Dev-0, Dev-2 |
