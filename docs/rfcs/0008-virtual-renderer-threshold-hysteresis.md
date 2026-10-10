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
| Mode switches for 79↔81 entry sequence | 2+ switches (79→81 enters, 81→79 exits, repeat) | 1 switch (enters Virtual at 81, stays Virtual at 79) | `messages.virtual` debug channel, stderr redirected to file |
| Leading/trailing elements after virtual switch | Lost | Lost (unchanged — Option C follow-up) | Renderer-parity tests (future RFC) |
| Full-viewport repaints per frame | 1 (FTXUI architecture) | 1 (unchanged — out of scope) | FTXUI source |

## 3. Goals

- G1. The renderer mode does not oscillate when `visible.size()` fluctuates
  within the hysteresis band [60, 80]. Once virtual mode is entered, it
  stays entered until `visible.size()` drops below 60. Sequences that
  cross both thresholds (e.g., 59→81→59) still switch modes — this is
  correct behavior. **Residual oscillation:** sequences that swing
  `visible.size()` by > 20 entries per frame (e.g., a chain merge/split of
  a 20+ row chain) can still cross both thresholds and oscillate. The band
  absorbs fluctuations contained within [60, 80]; it cannot absorb
  fluctuations that span the full band. The telemetry (G4) will detect
  residual oscillation in the wild.
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
  Pin-to-bottom state is preserved across mode switches. **Unpinned scroll
  continuity is explicitly excluded from Phase A** — the two renderer
  paths use incompatible scroll coordinate systems (JumpHandle visual
  lines vs EstimateTranscriptRows heuristic), and preserving the raw
  `scroll_offset` integer does not preserve the visual scroll position.
  On mode switch, if not pinned to bottom, the scroll position is reset
  to bottom (predictable degradation). Cross-mode scroll continuity is
  Option C territory (unified coordinate system).

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

### 5.3 Decision logic and same-frame scroll reset

The view computes `next_virtual_mode` from `prev_virtual_mode` +
`visible.size()`. Critically, when the mode switches and the user is not
pinned to bottom, the view **resets the scroll to bottom for this frame** —
before building the Element. This ensures the first frame of the new mode
already uses the bottom-anchored scroll position, not the old coordinate
system's stale offset.

```cpp
// Pseudocode — actual implementation in messages_list_view.cpp
const auto visible_count = visible.size();
const auto prev_mode = input.prev_virtual_mode;
const auto next_mode = decide_virtual_renderer_mode(prev_mode, visible_count);
input.next_virtual_mode = next_mode;

const bool mode_switched = (next_mode != prev_mode);
const bool reset_to_bottom = mode_switched && !input.pin_to_bottom;

if (next_mode == VirtualRendererMode::Virtual) {
    (void)trailing_elements;
    // ... build virtual rows ...
    if (input.pin_to_bottom || reset_to_bottom) {
        state.scroll_top = state.jh.total() - state.viewport_rows;
        state.sticky_bottom = true;
    } else {
        state.scroll_top = std::max(0, scroll_top_lines);
    }
    return render_list_as_elements(...);
}
// static path:
// ... build vbox ...
if ((input.pin_to_bottom || reset_to_bottom) &&
    visible.size() > 1 && content_exceeds_viewport) {
    list = std::move(list) | focusPositionRelative(0, 1);  // bottom
} else if (input.scroll_offset > 0) {
    list = std::move(list) | focusPosition(0, input.scroll_offset + vp / 2);
}
```

The `decide_virtual_renderer_mode` helper is a `constexpr` pure function
in `messages_list.cppm`:

```cpp
[[nodiscard]] constexpr auto decide_virtual_renderer_mode(
    VirtualRendererMode prev,
    std::size_t visible) noexcept -> VirtualRendererMode
{
    switch (prev) {
        case VirtualRendererMode::Static:
            return (visible > kVirtualEnterThreshold)
                ? VirtualRendererMode::Virtual
                : VirtualRendererMode::Static;
        case VirtualRendererMode::Virtual:
            return (visible < kVirtualExitThreshold)
                ? VirtualRendererMode::Static
                : VirtualRendererMode::Virtual;
    }
    return VirtualRendererMode::Static;
}
```

**Stateless callers** (golden tests, dialogs) default to
`prev_virtual_mode = Static`. The view decides the mode from this default —
no pre-decision step is needed. The hysteresis is a no-op for stateless
callers because they always start from Static.

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
(`repl_screen_scroll.cpp`, `repl_screen_layout.cpp`). It continues to be
derived from `virtual_jh_out` presence (the existing logic at
`repl_screen_messages.cpp:463-470`), **not** from the mode directly. This
is important: `mode=Virtual` implies the virtual path ran, which implies
`virtual_jh_out` is populated — but this equivalence is an emergent
property of the exit threshold being > 0, not an enforced invariant.
Keeping the jh_out-presence derivation ensures `virtual_list_active` is
only true when a valid JumpHandle exists, preventing stale `virtual_jh`
from being read by ScrollTranscript. The new `virtual_renderer_mode` is
the **persistent decision**; `virtual_list_active` is the **per-frame
observation**.

### 5.5 Invariants

**INV-1: Mode state is updated only by the UI thread that completes a list
render.** Telemetry, stream callbacks, background events, and animation
tickers must not write `virtual_renderer_mode` or
`virtual_mode_switch_count`. The write happens exclusively in
`repl_screen_messages.cpp` (the RenderMessages caller), which runs on the
FTXUI UI thread, reading `input.next_virtual_mode` computed by the view.
This prevents event reordering from causing state regression.

Note: the surrounding codebase does not universally follow UI-thread-only
discipline — `handle_stream_event` (`app_handle_submit.cpp:301-302,310,327`)
writes `screen_state_->task_view_store` directly from the stream thread
under `result_mutex_`. INV-1 is therefore enforced by **placement** (the
mode write is in the render path, which runs on the UI thread), not by
convention. The implementer must not move the mode write to a stream
callback or background event handler.

**INV-2: Initial mode is always Static.** The following events reset
`virtual_renderer_mode` to Static and `virtual_mode_switch_count` to 0:

| Event | Reset location | Mechanism |
|-------|---------------|-----------|
| First render (fresh store) | `messages_store.cppm` default member init | `= VirtualRendererMode::Static` |
| Slash `/clear` | `app_handle_submit.cpp:414-422` (clear handler) | Calls `reset_virtual_renderer_state()` |
| Callback clear (session-expired card) | `app_constructor.cpp:260-268` (`cbs.on_clear_session`) | Calls `reset_virtual_renderer_state()` |
| Session restore (runtime) | `app_handle_submit.cpp:668-677` (restore handler) | Calls `reset_virtual_renderer_state()` |
| Session restore (startup) | Fresh `MessagesStore` construction | Default init |

To avoid duplicating field assignments across three call sites, a
`reset_virtual_renderer_state()` helper is added to `MessagesStore` (or as
a free function in the screens area). It resets `virtual_renderer_mode =
Static` and `virtual_mode_switch_count = 0`. All three call sites (slash
clear, callback clear, restore) call this helper alongside their existing
divider/scroll resets.

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

The view does NOT emit telemetry. Transition telemetry is emitted by the
caller (`repl_screen_messages.cpp`) at the commit point — after the view
returns and the mode is committed to the store. This ensures the mode
state, counter, and log line are the same UI-thread linearization point:

```cpp
// repl_screen_messages.cpp, after render_messages_list_view returns:
const auto prev_mode = store->virtual_renderer_mode;
const auto next_mode = input.next_virtual_mode;
if (next_mode != prev_mode) {
    store->virtual_renderer_mode = next_mode;
    ++store->virtual_mode_switch_count;
    debug("messages.virtual",
          "mode switch {} → {} at visible={}, count={}",
          static_cast<int>(prev_mode), static_cast<int>(next_mode),
          input.visible_count_out, store->virtual_mode_switch_count);
}
```

The view writes `visible_count_out` (a `mutable` field on
`MessagesListInput`) so the caller can log the visible count without
calling `build_visible_rows` again. The debug channel is
`messages.virtual`. The call fires **only on mode transitions**
(`next_mode != prev_mode`), not every frame — so the per-call mutex
acquisition in `debug()` (`debug.cppm:54`) is rare and negligible.

**Verification requires stderr redirection.** `loom::utils::debug()` writes
to `std::cerr` (`debug.cppm:55-60`). If stderr shares the TTY with the TUI,
it corrupts the display. To verify telemetry without polluting the UI:

```bash
LOOM_DEBUG=messages.virtual loom 2>/tmp/loom-debug.log
```

The PTY captures only stdout (UI output); stderr goes to the log file.

**Baseline capture:** To prove the fix reduces mode switches, the
telemetry and the hysteresis land as **two separate commits** so the
baseline can be captured with the old threshold but the new telemetry:

1. **Commit 1 (telemetry only):** Add the `messages.virtual` debug channel
   and mode-decision logging, but keep the old stateless threshold
   (`visible.size() > 80`). Build, run a long task with
   `LOOM_DEBUG=messages.virtual 2>/tmp/baseline.log`. Count `mode switch`
   lines.
2. **Commit 2 (hysteresis):** Change the decision logic to the hysteresis
   band. Build, run the same task with
   `LOOM_DEBUG=messages.virtual 2>/tmp/with-hysteresis.log`. Count
   `mode switch` lines.
3. Compare switch counts. The hysteresis build should show zero switches
   for entry sequences that stay within [60, 80].

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
| `src/ui/messages/messages_list.cppm` | Add `VirtualRendererMode` enum, `decide_virtual_renderer_mode()` helper, `kVirtualEnterThreshold`, `kVirtualExitThreshold`; add `prev_virtual_mode` / `next_virtual_mode` / `visible_count_out` to `MessagesListInput`; remove `kVirtualThreshold`, `MessagesListComponent`, `MakeMessagesList` |
| `src/ui/messages/messages_list_view.cpp` | Hysteresis decision via helper; same-frame scroll reset on unpinned switch; write `next_virtual_mode` + `visible_count_out` |
| `src/ui/screens/messages_store.cppm` | Add `virtual_renderer_mode` + `virtual_mode_switch_count` fields |
| `src/ui/screens/repl_screen_messages.cpp` | Set `input.prev_virtual_mode` from store; read `input.next_virtual_mode` after render; increment switch counter |
| `src/ui/app/app_handle_submit.cpp` | Call `reset_virtual_renderer_state()` in slash `/clear` handler (:414-422) and session restore handler (:668-677) |
| `src/ui/app/app_constructor.cpp` | Call `reset_virtual_renderer_state()` in `cbs.on_clear_session` (:260-268) |
| `src/cmake/targets/loom_ui_messages.cmake` | Remove `messages_list_component.cpp` entry |

No wire-protocol, persisted-shape, or config changes.

**Build-time impact:** `messages_store.cppm` is an interface unit (`.cppm`)
with 12 direct importers and ~40–50 transitive importers (via
`repl_state.cppm`, `repl_screen.cppm`, `app.cppm`). Adding two fields to it
triggers a one-time BMI recompilation of its importer closure on the
implementing commit. This is comparable to the `design_tokens` fan-out
(~62 modules) noted in CLAUDE.md and is a one-time cost, not a per-edit
cost. The `VirtualRendererMode` enum is defined in `messages_list.cppm`
(5 direct importers), which `messages_store.cppm` already imports — no new
module edge is added.

**Golden impact:** Mode selection changes only for sessions with history
that cross from > 80 into the [60, 80] band. In the old stateless code,
> 80 was already Virtual — so golden tests with > 80 entries and default
(Static) `prev_virtual_mode` are unaffected (they enter Virtual at > 80 in
both old and new code). The change is in the 60–80 band with history:
previously Virtual, now stays Virtual (previously would have exited to
Static at ≤ 80). Golden tests that construct > 80 entries with a
persisted Virtual mode may produce different output and must be reviewed
and re-baselined with `UPDATE_GOLDENS=1` if needed. Golden tests with
default (Static) mode at ≤ 80 are unaffected.

## 6. Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|-------|-------|-------|--------|-------------------------------|
| A | Hysteresis + telemetry + dead code deletion | Constants, mode type, MessagesStore fields, decision helper, same-frame scroll reset, telemetry, delete MessagesListComponent, reset on clear/restore | proposed | All tests pass (debug + release serial ctest); regression tests prove G1/G2/G5; `graph_check.py --target-ui9 --store-lint` 0 violations; no new `kVirtualThreshold` references; BMI PSS of `messages_store.cppm` before/after recorded in Implementation History; importer count before/after recorded |

**Follow-up (separate RFC):** Option C — unify static and virtual renderer
paths so the virtual path also renders leading/trailing elements in a
unified content coordinate system. This RFC's hysteresis design is
compatible with the future unified path: the mode type and MessagesStore
field carry over; only the view's internal branching changes.

## 7. Production Readiness Review

| Question | Answer |
|----------|--------|
| Is the change backward-compatible? | Yes — no persisted state, no config, no wire changes. `MessagesListComponent`/`MakeMessagesList` is an exported module interface but Loom does not install its own modules (only tree-sitter headers are installed, per `src/CMakeLists.txt:77-78`), so it is an internal API with no supported out-of-tree consumers. Deletion is safe. |
| Is there a rollback story? | Yes — revert the commit. The mode falls back to stateless `visible.size() > 80`. `MessagesListComponent` would need to be restored from git history if any out-of-tree caller exists (none found). |
| Are there new failure modes? | Mode could get "stuck" in virtual if visible count stays in [60, 80]. In this band the static path would render logo + filler + all rows, so this is the acknowledged cost of hysteresis — stability over visual completeness. The virtual path is still correct (O(viewport) rows, no last-N cap); it just lacks leading/trailing elements. |
| Is telemetry safe? | Yes — counts and mode names only, no message content. Requires stderr redirection to avoid TTY pollution. |
| Are there edge cases? | Empty list → Static (default). First render → Static (default). `/clear` → Static (explicit reset). Session restore → Static (explicit reset). Resize → mode preserved, geometry recomputed. SyncState → mode preserved. Exactly 80 → stays Static (or stays Virtual). Exactly 60 → stays Virtual. |
| Performance impact? | Negligible — one branch + one enum write per frame. `build_visible_rows()` is already called every frame. |
| Golden test impact? | Tests with > 80 visible entries and persistent mode may change. Tests with default (Static) mode at ≤ 80 are unaffected. Must review and re-baseline if needed. |
| Thread safety? | Mode state is UI-thread-affined (INV-1). No locking needed — FTXUI's event loop is single-threaded. The `mutable` output field on `MessagesListInput` follows the existing `virtual_jh_out` pattern. |

## 8. Rollout and rollback

- **Rollout:** Two commits to enable baseline comparison (§5.7):
  1. **Commit 1 (telemetry + dead code deletion):** Add the
     `messages.virtual` debug channel, `visible_count_out` field,
     `decide_virtual_renderer_mode` helper, and delete
     `MessagesListComponent`. Keep the old stateless threshold.
  2. **Commit 2 (hysteresis):** Change the decision logic to the
     hysteresis band, add `prev_virtual_mode`/`next_virtual_mode` to
     `MessagesListInput`, add MessagesStore fields, add reset calls to
     clear/restore handlers.
  No feature flag (the fix is a correctness improvement to an internal
  renderer decision; no user-visible config).
- **Rollback:** Revert commit 2 (hysteresis). The mode falls back to the
  stateless `visible.size() > 80` check. Commit 1 (telemetry + deletion)
  can remain — it is independently useful and has no behavior change.
  `MessagesListComponent` is restored from git history if needed.
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

5. **Scroll coordinate-system mismatch on mode switch (mitigated).**
   `scroll_offset` in `MessagesStore` is shared between two incompatible
   coordinate systems: the virtual path uses JumpHandle prefix-sum visual
   lines (`repl_screen_scroll.cpp:664-696`); the static path uses
   `EstimateTranscriptRows` heuristic yframe coordinates
   (`repl_screen_scroll.cpp:722-749`). This RFC mitigates the mismatch by
   resetting unpinned scroll to bottom **in the same frame** as the mode
   switch (§5.3) — the first frame of the new mode already uses the
   bottom-anchored position, not the old coordinate system's stale offset.
   Pinned-to-bottom state is preserved (both paths handle it natively).
   A full fix (unified scroll coordinate system) is Option C territory.

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
RFC** — larger change, needs renderer-parity tests first. This RFC limits
threshold oscillation; the field flicker benefit remains unverified until
telemetry data is collected post-merge. This RFC's design is
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

Tests are split across two files by what they verify:

**Pure view/decision-logic tests** in `tests/ui/test_ui_messages.cpp`:
These test the hysteresis decision function directly with controlled
entry-count sequences.

| Test | Scenario | Expected |
|------|----------|----------|
| `VirtualThreshold.StaysVirtualAboveExit` | prev=Static, 79→81→79 | Mode enters Virtual at 81, stays Virtual at 79 |
| `VirtualThreshold.ReturnsToStaticBelowExit` | prev=Virtual, 81→60→59 | Mode stays Virtual at 60, returns to Static at 59 |
| `VirtualThreshold.InitialModeIsStatic` | Default-constructed input, empty list | Mode is Static |
| `VirtualThreshold.StatelessCallerDefaultsStatic` | No prev_virtual_mode set, 100 entries | Mode is Virtual (enters at > 80 from Static default) |
| `VirtualThreshold.ExactBoundary80` | prev=Static, exactly 80 | Mode stays Static (> 80 is strict) |
| `VirtualThreshold.ExactBoundary60` | prev=Virtual, exactly 60 | Mode stays Virtual (< 60 is strict) |
| `VirtualThreshold.PinToBottomPreserved` | Pinned state at both transitions | Pin-to-bottom flag unchanged across mode switch |
| `VirtualThreshold.UnpinnedScrollResetToBottom` | Unpinned (scrolled up) at mode switch | Scroll position reset to bottom (predictable degradation) |
| `VirtualThreshold.SwitchCounterIncrements` | Two mode transitions | Counter = 2 |

**AppAdapter lifecycle tests** in `tests/ui/test_app_runtime.cpp`:
These call the real `/clear`, callback-clear, and restore handlers to
verify the mode is reset through the actual code paths, not just manual
enum assignment.

| Test | Scenario | Expected |
|------|----------|----------|
| `VirtualThreshold.SlashClearResetsMode` | Virtual mode → slash `/clear` → 70 entries | Mode is Static after clear; stays Static at 70 |
| `VirtualThreshold.CallbackClearResetsMode` | Virtual mode → `cbs.on_clear_session()` → 70 entries | Mode is Static after callback clear; stays Static at 70 |
| `VirtualThreshold.SessionRestoreResetsMode` | Virtual mode → restore 70 entries | Mode is Static after restore; stays Static at 70 |
| `VirtualThreshold.ResizePreservesMode` | Virtual mode → resize → 70 entries | Mode stays Virtual |
| `VirtualThreshold.SyncStatePreservesMode` | Virtual mode → SyncState → 70 entries | Mode stays Virtual |
| `VirtualThreshold.ClearResetsCounter` | Virtual mode with counter=2 → `/clear` | Counter is 0 after clear |

#### Dynamic multi-frame tests

The production oscillation scenario is dynamic — streaming deltas, chain
merges, and projection rebuilds change `visible.size()` across consecutive
frames. These tests in `tests/ui/test_ui_messages.cpp` simulate multi-frame
sequences:

| Test | Scenario | Expected |
|------|----------|----------|
| `VirtualThreshold.StreamingOscillation` | Entries added 75→82→78→85→72 across 5 frames | Mode: Static→Virtual(82)→Virtual(78)→Virtual(85)→Virtual(72); no flip-flop |
| `VirtualThreshold.SelectionSurvivesSwitch` | `selected_row_idx` set; mode switches Static→Virtual | Selection row index preserved (same row identity in both paths) |
| `VirtualThreshold.SearchSurvivesSwitch` | `search_query` non-empty; mode switches | Search state preserved; both paths have search support |
| `VirtualThreshold.UnseenDividerSurvivesSwitch` | Unseen divider active; mode switches | Divider anchor preserved across switch |
| `VirtualThreshold.ScrollDuringSwitch` | Scroll events arrive between frames during mode switch | No crash; unpinned scroll reset to bottom (predictable degradation) |

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
- `graph_check.py --target-ui9 --store-lint` — 0 violations
- BMI PSS: measure `messages_store.cppm` producer PSS before/after using
  the project's BMI measurement tooling; record values in Implementation
  History
- Importer count: `grep -rc "import loom.ui.screens.messages_store" src/
  --include="*.cppm" --include="*.cpp" | grep -v ":0" | wc -l` before/
  after; record in Implementation History
- Inline ratchet: no new inline violations

### 11.4 PTY + telemetry verification (post-merge)

1. Build commit 1 (telemetry + deletion, old stateless threshold)
2. Run a long task: `LOOM_DEBUG=messages.virtual loom 2>/tmp/baseline.log`
3. PTY captures stdout (UI output) only
4. Build commit 2 (hysteresis)
5. Run the same task: `LOOM_DEBUG=messages.virtual loom 2>/tmp/with-hysteresis.log`
6. Compare `mode switch` count in both logs. The hysteresis build should
   show zero switches for entry sequences that stay within [60, 80].
7. If the user still sees flicker with zero mode switches in the log, the
   threshold is exonerated and the cause lies elsewhere (e.g., LOOM_DEBUG
   stderr pollution, viewport height lag). stderr pollution and mode
   switches can co-occur — they are separately excluded/correlated, not
   mutually exclusive.

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
| 2026-10-10 | — | Revision 2: all P0/P1 addressed (state passing, G5 scope, reset lifecycle, dead code deletion, telemetry, motivation) | d26735e | — | — |
| 2026-10-10 | — | Design review round 2: build/perf agent (1 P0 + 4 P1 + 6 P2), correctness agent (3 P1 + 7 P2). Most already fixed in rev 2; new findings addressed in rev 3. | — | — | build/perf agent, correctness agent |
| 2026-10-10 | — | Revision 3: dynamic tests, scroll coord mismatch doc, BMI fan-out, PSS metric fix, virtual_list_active derivation, residual oscillation, INV-1 placement note, PRR wording, baseline two-commit method | 39d978e | — | — |
| 2026-10-10 | — | Design review round 3: Dev-0 REQUEST_CHANGES (2 P0 + 2 P1), Dev-2 1 P1 + 3 P2. All addressed in rev 4. | — | — | Dev-0, Dev-2 |
| 2026-10-10 | — | Revision 4: second clear path + shared reset helper, unpinned scroll reset to bottom, telemetry at caller commit point, internal API boundary, test split (view vs AppAdapter), evidence/golden/PTY wording fixes | 20f9843 | — | — |
