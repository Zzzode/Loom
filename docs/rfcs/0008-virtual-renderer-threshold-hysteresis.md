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
When the visible entry count oscillates around 80 during a long task —
driven by streaming deltas, thinking-block collapse, tool-preview expansion,
or chain merges — the renderer flips between two structurally different
output trees on consecutive frames. Because FTXUI repaints the full
viewport on every frame (no cell-level diff), each flip produces a visible
full-screen flicker that affects all previously-rendered Boxes, not just the
new one.

This RFC adds a hysteresis band: enter virtual mode at `> 80`, exit only at
`< 60`. The mode state persists in `MessagesStore` across frames. This is
the minimal incident fix; a follow-up RFC will unify the two renderer paths
(Option C) so the virtual path also renders leading/trailing elements.

## 2. Motivation

### 2.1 User report

> 现在存在一个问题，就是有一个长任务出来以后，后面的 Box 在展示的过程中会偶尔会全屏刷新一下，也就是前面的 Box 也会重新做一次闪烁。

The user expects that when a new Box (message block) appears, only the new
Box renders — previous Boxes are unaffected. In practice, during long tasks
the entire screen occasionally flickers.

### 2.2 Root cause (three-bot investigation, 2026-10-10)

Dev-0, Dev-2, and Dev-1 independently investigated and converged on a
multi-layer cause. The threshold oscillation is the **trigger**; FTXUI's
full-viewport repaint is the **amplifier** that makes it visible.

#### Layer 1: Stateless threshold switch (the trigger)

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

#### Layer 2: FTXUI full-viewport repaint (the amplifier)

FTXUI's `ScreenInteractive::Draw()` (`screen_interactive.cpp:729-820`)
calls `Screen::ToString()` (`screen.cpp:416-446`), which serializes **all
pixels** every frame — there is no cell-level diff against the previous
frame. `Clear()` (`screen.cpp:519-531`) resets the in-memory pixel buffer
after each frame. Every frame is a full-viewport rewrite.

This means a mode switch is not a localized update — the entire terminal
viewport is rewritten, and the structural difference between paths makes
the rewrite visible as a flicker.

#### Layer 3: What oscillates the visible count around 80

The threshold checks `visible.size()` — the count of **visible entries**
after `build_visible_rows()`, not the visual line count. Events that change
the entry count include:

- Streaming deltas that add or merge entries (chain merges/splits)
- Thinking-block collapse (shrinks a block, may merge adjacent entries)
- Tool-preview expansion/collapse (adds/removes preview entries)
- `conversation_projection_dirty_` triggering a full projection rebuild
  (on commit, SyncState)
- Search/filter changes

During a long task, several of these fire per second. If the visible count
is near 80, any one can push it across the threshold.

#### Layer 4: Viewport height lag (secondary, two-step position change)

`repl_screen_layout.cpp:90-99` uses the **previous frame's** Box height for
the viewport. When Todo/Spinner/teammate strip changes bottom height, this
frame uses the old height, next frame corrects — a two-step position change
that is visible as a brief jump. This is independent of the threshold
oscillation but compounds the perception of flicker.

### 2.3 What is confirmed vs not confirmed

| Claim | Confidence | Evidence |
|-------|-----------|----------|
| FTXUI repaints full viewport every frame | Confirmed | Source: `screen.cpp:416-446` |
| Threshold switch is stateless (no hysteresis) | Confirmed | Source: `messages_list_view.cpp:335-345` |
| Two paths produce structurally different trees | Confirmed | Source: leading/trailing discarded in virtual path |
| Visible count oscillates around 80 in the wild | **Not yet captured** | Conditional chain is valid; PTY telemetry needed |
| LOOM_DEBUG stderr pollution causes flicker | **Being checked** | `debug("app.render", ...)` at `app_render_event.cpp:200`; user checking env |
| ioctl EINTR → 80×24 fallback → `\x1b[2K` clear | 0.05 | Linux TIOCGWINSZ reads locked winsize; EINTR unlikely |

The threshold oscillation is a **correctness defect regardless of whether
it is the on-site flicker's primary cause**: a renderer that flips between
two structurally different modes at a hard boundary is inherently unstable.
This RFC fixes that defect.

### 2.4 Evidence

| Metric | Current | Target | How measured |
|--------|---------|--------|-------------|
| Mode switches per minute (visible ≈ 80) | Unbounded (oscillation) | 0 (hysteresis band) | Frame telemetry counter |
| Leading/trailing elements after virtual switch | Lost | Preserved (Option C follow-up) | Renderer-parity tests |
| Full-viewport repaints per frame | 1 (FTXUI architecture) | 1 (unchanged — out of scope) | FTXUI source |

## 3. Goals

- G1. The renderer mode does not oscillate when `visible.size()` fluctuates
  near the threshold. Once virtual mode is entered, it stays entered until
  `visible.size()` drops below the exit threshold (60).
- G2. Mode state is deterministic and resettable: empty list, first render,
  session restore, and `/clear` all start in static mode.
- G3. Mode state is updated only by the UI thread that completes a list
  render — never by telemetry, background events, or stream callbacks.
- G4. Frame telemetry records `visible.size()`, current mode, and
  mode-change counter, so threshold oscillation can be measured in the wild.
- G5. Regression tests prove: 79→81→79 stays virtual until < 60; 60→59
  returns to static; both transitions preserve pin/scroll state and
  leading/trailing/unseen-divider/filler/pill elements.

## 4. Non-Goals

- **Unifying the static and virtual renderer paths** (Option C). The
  virtual path currently discards leading/trailing elements. Fixing this is
  a larger correctness change deferred to a follow-up RFC. This RFC only
  prevents oscillation between the two paths.
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

Named constants in `messages_list.cppm`:

```cpp
/// Enter virtual (windowed) rendering when visible entries exceed this.
inline constexpr std::size_t kVirtualEnterThreshold = 80;
/// Exit virtual rendering only when visible entries drop below this.
/// Must be < kVirtualEnterThreshold (asserted at compile time).
inline constexpr std::size_t kVirtualExitThreshold = 60;
```

The existing `kVirtualThreshold = 80` and `kMaxRenderedLastN = 80` are
kept: `kMaxRenderedLastN` still caps the static path's last-N render;
`kVirtualThreshold` is superseded by the enter/exit pair and deprecated.

### 5.2 Persistent mode state in MessagesStore

Add to `messages_store.cppm`:

```cpp
/// Hysteresis state for the virtual renderer threshold.
/// Persisted across frames so the mode does not oscillate when
/// visible.size() fluctuates near the threshold.
/// Updated ONLY by the UI thread after completing a list render.
enum class VirtualRendererMode : std::uint8_t {
    Static,
    Virtual,
};
VirtualRendererMode virtual_renderer_mode = VirtualRendererMode::Static;
```

The existing `virtual_list_active` bool is kept for scroll-path consumers
(`repl_screen_scroll.cpp`, `repl_screen_layout.cpp`) — it is set per-frame
from the mode and the JumpHandle output, exactly as today. The new
`virtual_renderer_mode` is the **persistent decision**; `virtual_list_active`
is the **per-frame observation**.

### 5.3 Decision logic in render_messages_list_view

```cpp
// Pseudocode — actual implementation in messages_list_view.cpp
const auto visible_count = visible.size();
const auto prev_mode = store ? store->virtual_renderer_mode
                             : VirtualRendererMode::Static;

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

if (next_mode != prev_mode) {
    // Telemetry: record mode transition
    debug("messages.virtual", "mode switch {} → {} at visible={}",
          prev_mode, next_mode, visible_count);
}

if (store) {
    store->virtual_renderer_mode = next_mode;
}

if (next_mode == VirtualRendererMode::Virtual) {
    (void)trailing_elements;
    return render_messages_list_virtual_rows(...);
}
// static path...
```

### 5.4 Invariants

These invariants are mandated by Dev-0's review and must hold in all
implementations:

**INV-1: Mode state is updated only by the UI thread that completes a list
render.** Telemetry, stream callbacks, background events, and animation
tickers must not write `virtual_renderer_mode`. The write happens
exclusively inside `render_messages_list_view()` (or its caller in
`repl_screen_messages.cpp`), which runs on the FTXUI UI thread. This
prevents event reordering from causing state regression.

**INV-2: Initial mode is always Static.** Empty list, first render, session
restore, `/clear`, and terminal resize all start in
`VirtualRendererMode::Static`. Virtual mode is entered only by crossing the
enter threshold (`visible.size() > 80`). No code path may inherit or
restore a previous session's mode. This is enforced by:
- `MessagesStore` default-initializes `virtual_renderer_mode = Static`
- `/clear` resets `MessagesStore` (already does today)
- Session restore creates a fresh `MessagesStore` (already does today)

**INV-3: Exit threshold < Enter threshold.** Compile-time assertion:
`static_assert(kVirtualExitThreshold < kVirtualEnterThreshold)`.

### 5.5 Frame telemetry

Add to the existing `debug("app.render", ...)` call site (or a new
`debug("messages.virtual", ...)` channel):

- `visible.size()` — current visible entry count
- `virtual_renderer_mode` — current mode (Static/Virtual)
- `virtual_mode_switch_count` — cumulative mode transitions this session

This telemetry is gated behind `LOOM_DEBUG` like all other debug output. It
allows threshold oscillation to be measured in the wild without a PTY
recording.

### 5.6 Interface surface

No new public modules. No new config keys. No new commands. Changes are
confined to:

| File | Change |
|------|--------|
| `src/ui/messages/messages_list.cppm` | Add `kVirtualEnterThreshold`, `kVirtualExitThreshold`; deprecate `kVirtualThreshold` |
| `src/ui/screens/messages_store.cppm` | Add `VirtualRendererMode` enum + `virtual_renderer_mode` field |
| `src/ui/messages/messages_list_view.cpp` | Hysteresis decision logic; mode write to store; telemetry |
| `src/ui/screens/repl_screen_messages.cpp` | Pass store to view (already does); set `virtual_list_active` from mode |

No wire-protocol, persisted-shape, or config changes. No golden impact
(the renderer output is unchanged within each mode; only the mode
*selection* changes).

## 6. Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|-------|-------|-------|--------|-------------------------------|
| A | Hysteresis + telemetry | Constants, MessagesStore field, decision logic, telemetry | proposed | All tests pass (debug + release); regression tests prove G1/G2/G5; `graph_check.py` 0 cycles; BMI PSS delta < 5% for touched modules |
| B | (Follow-up) Unify renderer paths | Option C: virtual path renders leading/trailing elements in full content coordinate system | proposed | Renderer-parity tests pass for 79→80→81→80 with pinned + scrolled states; separate RFC |

Phase B is a separate RFC. This RFC covers only Phase A.

## 7. Production Readiness Review

| Question | Answer |
|----------|--------|
| Is the change backward-compatible? | Yes — no persisted state, no config, no wire changes |
| Is there a rollback story? | Yes — revert the commit; mode falls back to stateless threshold |
| Are there new failure modes? | Mode could get "stuck" in virtual if visible count never drops below 60. This is acceptable: virtual mode is correct for large transcripts; the static path's last-N cap would be worse. |
| Is telemetry safe? | Yes — counts and mode names only, no message content |
| Are there edge cases? | Empty list → Static (INV-2). First render → Static (INV-2). Session restore → Static (INV-2). Resize → mode unchanged (no reset needed; resize doesn't change visible count). |
| Performance impact? | Negligible — one branch + one enum write per frame |
| Golden test impact? | None — renderer output unchanged within each mode |

## 8. Rollout and rollback

- **Rollout:** Single commit. No feature flag (the fix is a correctness
  improvement to an internal renderer decision; no user-visible config).
- **Rollback:** `git revert` the commit. The mode falls back to the
  stateless `visible.size() > 80` check. No data migration needed.
- **Ordering:** No dependencies on other RFCs. Can land independently.

## 9. Drawbacks

1. **Mode can get "stuck" in virtual.** If a session grows past 80 entries
   and stays above 60, the renderer stays in virtual mode even if the user
   scrolls to a region where the static path would suffice. This is
   acceptable: the virtual path is strictly more capable for large
   transcripts (it renders O(viewport) rows, not O(N)), and the static
   path's last-N cap would hide older messages.

2. **Two constants to tune instead of one.** The exit threshold (60) is a
   judgment call. Too close to 80 and the band is too narrow to prevent
   oscillation; too far and the renderer stays virtual longer than needed.
   60 provides a 20-entry band (25% of enter threshold) which is wide
   enough to absorb normal streaming fluctuation without being so wide that
   the renderer never exits virtual for medium transcripts.

3. **Leading/trailing elements are still lost in virtual mode.** This is
   the known Option C defect. The hysteresis band makes it less likely to
   be hit (mode switches are rarer), but does not fix it. Users with > 80
   visible entries still lose the logo and filler.

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
hysteresis band is a sufficient incident fix.

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

## 11. Testing and verification plan

### 11.1 New regression tests

| Test | Scenario | Expected |
|------|----------|----------|
| `VirtualThreshold.StaysVirtualAboveExit` | 79→81→79 visible entries | Mode enters Virtual at 81, stays Virtual at 79 |
| `VirtualThreshold.ReturnsToStaticBelowExit` | 81→60→59 visible entries | Mode stays Virtual at 60, returns to Static at 59 |
| `VirtualThreshold.InitialModeIsStatic` | Empty list, first render | Mode is Static |
| `VirtualThreshold.SessionRestoreIsStatic` | Restore session with > 80 entries | Mode starts Static, enters Virtual on first render |
| `VirtualThreshold.ClearResetsMode` | Virtual mode → /clear | Mode is Static after clear |
| `VirtualThreshold.PreservesPinScrollOnSwitch` | Pinned + scrolled states at both transitions | Pin/scroll state unchanged across mode switch |
| `VirtualThreshold.PreservesLeadingTrailing` | Both transitions | Leading/trailing/unseen-divider/filler/pill preserved per mode |

### 11.2 Existing tests

No existing tests are modified. The renderer output within each mode is
unchanged.

### 11.3 Build and test

- `cmake --workflow --preset dev` (debug build)
- `ctest --preset debug -j$(nproc)` — all tests pass
- `cmake --workflow --preset ci` (release build + test)
- `ctest --preset release -j$(nproc)` — all tests pass
- `graph_check.py --target-ui9` — 0 cycles
- Golden tests: no UPDATE_GOLDENS needed (output unchanged within modes)

### 11.4 PTY verification (post-merge)

Record terminal output during a long task with `LOOM_DEBUG=1`. Verify:
- Mode switches occur only at threshold crossings (enter at > 80, exit at < 60)
- No oscillation (no repeated enter/exit within a short window)
- If LOOM_DEBUG is the on-site flicker cause, this RFC's telemetry will
  show zero mode switches during the flicker, exonerating the threshold

## 12. Documentation impact

- [ ] `CLAUDE.md` — no changes needed (internal renderer detail)
- [ ] `docs/decisions/design-decisions.md` — add entry for hysteresis band
- [ ] Code comments — document INV-1/INV-2/INV-3 at the decision site

## 13. Open questions

| Question | Owner | Resolved by |
|----------|-------|-------------|
| Is LOOM_DEBUG the on-site flicker cause? | 张地 | env check |
| What is the actual mode-switch frequency in the wild? | Dev-1 | Post-merge telemetry |
| Should the exit threshold be configurable? | Dev-1 | Post-merge data (default: no) |

## 14. Implementation History

| Date | Phase | Description | Commit | Tests | Review |
|------|-------|-------------|--------|-------|--------|
| 2026-10-10 | — | RFC drafted (provisional) | — | — | — |
