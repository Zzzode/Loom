---
rfc: 3
title: Streaming Payload Replay Testing
status: implemented
owners: "@Zzzode"
reviewers: ["agent:design-review#1 (request-changes — F1-F5 fatal, H1-H5 high, M1-M8 medium; all applied in revision 2)", "agent:design-verify#1 (F1-F5 confirmed fixed; N2 design gap + N1/N3/N4/N5 minor; all applied in revision 3)"]
created: 2026-10-07
last-reviewed: 2026-10-07
implemented: 2026-10-07
tracking: local
---

# RFC 0003 — Streaming Payload Replay Testing

## 1. Summary

Loom's UI tests render individual messages and markdown constructs in
isolation, but the **streaming event pipeline** — the path from
`core::StreamEvent` arrival through App-side streaming state to FTXUI
rendering — has no systematic test coverage. Three bugs shipped through this
gap in one week (duplicate assistant text, thinking content truncated to 200
chars, completed thinking blocks hidden entirely; all three have since been
fixed). This RFC proposes a four-layer test system: a version-controlled
JSONL event fixture library, a replay harness that injects events directly
into the App's event handler and simulates engine commits (bypassing
HTTP/SSE), golden screen snapshots at named checkpoints, and a scoped
invariant checker. The system reuses the existing `LOOM_UPDATE_SNAPSHOTS`
mechanism and follows the same patterns as Textual's `run_test()` + Pilot +
snapshot suite and Neovim's `screen.lua`.

**Honest scope**: event injection tests the *streaming preview path* and the
*streaming ↔ committed interaction path*. It does **not** test the engine
loop (tool execution, retries, permission denial, compaction) — those remain
on HTTP mock server tests.

## 2. Motivation

### 2.1 The three bugs that slipped through

All three were in the event → state → render pipeline, and all three were
invisible to the existing test suite. All three have been fixed; the test
system would have caught them:

| Bug | Root cause | Why tests missed it |
|---|---|---|
| Duplicate assistant text when Bash runs | `streaming_text_` not cleared after `AssistantMessage` commit; `has_in_flight` stays true due to thinking 30s grace | No test feeds a `StreamEnd` → commit sequence and asserts on the rendered screen where both streaming and committed rows are visible |
| Thinking content truncated to 200 chars | Three `substr(0, 200)` sites feed truncated `content_preview` as `raw_text` to the thinking renderer | No test injects a thinking block with >200 chars and checks the expanded render |
| Completed thinking block hidden after completion | `messages_list_filter.cpp` filtered out `state == Complete` rows; `messages_list_payload_row.cpp` had a defensive `return text("")` (both removed in the fix) | The test that covered this asserted the *buggy* behavior (hiding was expected) |

### 2.2 Root cause: the pipeline is untested

The streaming pipeline has five stages:

```
HTTP SSE → SseEventDecoder → wire StreamEvent → core::StreamEvent
  → App::on_event lambda → streaming state (streaming_text_, streaming_thinking_, ...)
  → project_messages → MessageDisplayEntry → FTXUI render
```

Stages 1–3 (SSE decoding, wire parsing) are tested at the wire level by
`test_sse_mock.cpp`. Stage 5 (markdown → FTXUI element) is tested by
`test_markdown_render_snapshot.cpp`. **Stage 4 — the App's event handler and
its streaming state — has no direct test coverage.** Existing E2E tests
(`test_ui_e2e.cpp`) exercise stage 4 indirectly through HTTP mock servers,
but:

- They are slow (start a real HTTP server on a random port per test, ~500ms
  each).
- They are brittle (timing-dependent, port conflicts, thread synchronization).
- They encode SSE event sequences as hardcoded C++ string literals, not
  reviewable fixtures.
- They primarily assert on the final screen state; intermediate renders are
  captured in only a few places.

### 2.3 The two paths that need testing

The three bugs live in two distinct paths, both of which the replay harness
must cover:

**Path A: Streaming preview.** `core::StreamEvent` → App handler →
`streaming_text_` / `streaming_thinking_` / `streaming_tools_` → `Render()`
projection. The truncated-thinking bug lives here. This path is testable by
injecting events directly into the App handler.

**Path B: Streaming ↔ committed interaction.** The engine's
`stream_query` appends the assembled `AssistantMessage` to the conversation
*after* the SSE stream completes (`query_engine_loop.cpp:142`). `Render()`
then projects **both** the committed messages (from `get_conversation()`)
**and** the in-flight streaming rows simultaneously
(`app_render_event.cpp:141` + `:228-246`). The duplicate-text and
hidden-thinking bugs live in this interaction. Testing Path B requires
**both** injecting streaming events **and** simulating the engine commit,
so that committed and streaming rows coexist on screen.

### 2.4 Industry precedent

| Project | Approach | What Loom adopts |
|---|---|---|
| **Textual** (Python TUI) | `run_test()` headless mode + Pilot API + `pytest-textual-snapshot` SVG comparison | Headless render + snapshot comparison + interaction-driven assertions |
| **Neovim** | `screen.lua`: virtual terminal grid, `Screen:expect()` with eventual-state polling, `snapshot_util()` auto-generation | Eventual-state assertion, snapshot auto-generation, intermediate-state checking |
| **Halfhand** | PTY-wrapped flight recorder; structured event capture (prompts, tool calls, results) for AI agents | Structured event fixtures as version-controlled cassettes |
| **tuiwright** | PTY-based testing + asciinema cast + PNG snapshot; pins `TERM`/`LANG`/`TZ` for determinism | Deterministic environment pinning |
| **VCR/Polly** | Record real HTTP interactions, replay as cassettes | Fixture as cassette concept; deterministic replay |

The consensus: **drive the UI through its real event pipeline with canned
payloads, assert on observable screen state, and use golden snapshots for
visual regression.** The specific gap in Loom is that the "canned payloads"
are hardcoded SSE strings in test `.cpp` files rather than independent,
reviewable fixtures, and there is no direct event-injection seam.

## 3. Goals

- **G1.** A JSONL fixture library under `tests/fixtures/streaming_sessions/`
  that encodes realistic `core::StreamEvent` sequences plus engine-commit
  simulation as version-controlled, reviewable files.
- **G2.** A replay harness that loads fixtures and injects events directly
  into the App's event handler, manages `query_running_` state, and
  simulates engine commits. Tests run in milliseconds, not seconds.
- **G3.** Golden screen snapshots at named checkpoints within a fixture,
  regenerated via `LOOM_UPDATE_SNAPSHOTS=1` (same mechanism as the markdown
  snapshot suite). Time-varying content (spinner glyphs, relative
  timestamps) is normalized to placeholders.
- **G4.** A scoped invariant checker that runs after every injected event
  and asserts properties that must hold in the states where they apply.
- **G5.** The three bugs from §2.1 encoded as fixtures with invariants that
  would have caught them.
- **G6.** Existing E2E tests in `test_ui_e2e.cpp` / `test_ui_runtime.cpp`
  migrated to fixture-driven replay where the fixture approach is strictly
  better (faster, more readable, more maintainable). HTTP mock server tests
  remain for wire-level and engine-loop integration.

## 4. Non-Goals

- **N1.** Recording real API sessions for replay. Halfhand-style flight
  recording is a separate concern; this RFC uses hand-authored fixtures.
  A future RFC may add a `loom record` command that captures live sessions
  as fixtures.
- **N2.** Property-based / fuzz testing of event sequences. The invariant
  checker (G4) is the foundation; a fuzzer that generates random valid event
  sequences and runs them through the invariant checker is a natural
  follow-up but is not in scope.
- **N3.** Testing the HTTP/SSE/wire layers. `test_sse_mock.cpp` covers SSE
  decoding at the wire level. The replay harness starts at the
  `core::StreamEvent` boundary.
- **N4.** Testing the engine loop. Tool execution, retries, permission
  denial, context compaction, and multi-round tool loops all happen inside
  `QueryEngine::stream_query`, which replay bypasses. These remain on HTTP
  mock server tests.
- **N5.** Screenshot/visual testing. FTXUI renders to a character grid, not
  pixels. ANSI-stripped plain-text snapshots are the assertion format.
- **N6.** PTY-based testing. Loom's FTXUI screen can be rendered headlessly
  via `ftxui::Screen::Create()` + `Render()`, which is faster and more
  deterministic than a real PTY. PTY testing is only needed for raw-mode
  terminal behavior, which is not the target of this RFC.

## 5. Proposal

```
┌──────────────────────────────────────────────────────────────┐
│  Layer 4: Scenario E2E (App-side only)                       │
│  Multi-turn conversations, streaming ↔ committed interaction │
│  → test_streaming_replay.cpp (fixture-driven)                │
│  Engine-loop tests (retries, permissions, compaction)        │
│  remain on HTTP mock servers (N4)                            │
├──────────────────────────────────────────────────────────────┤
│  Layer 3: Screen assertion                                   │
│  Golden snapshots at checkpoints + scoped invariant checker  │
│  → test_streaming_replay.cpp                                 │
├──────────────────────────────────────────────────────────────┤
│  Layer 2: Replay harness                                     │
│  StreamReplayHarness: load fixture → inject events →         │
│  manage query_running_ → simulate commits → render           │
│  → tests/streaming_replay.hpp                                │
├──────────────────────────────────────────────────────────────┤
│  Layer 1: Event fixture library                              │
│  JSONL files: core::StreamEvent + __commit__ + checkpoints   │
│  → tests/fixtures/streaming_sessions/*.jsonl                 │
└──────────────────────────────────────────────────────────────┘
```

### 5.1 Layer 1: Event fixture library

Each fixture is a JSONL file. Every line is one of:
- A `core::StreamEvent` (streaming event to inject into the App handler)
- A `__commit__` pseudo-event (simulates engine appending a committed message)
- A `__end_query__` pseudo-event (simulates the query thread finishing)
- A `__checkpoint__` marker (captures a screen snapshot)

Fixtures are hand-authored (not generated) so they are reviewable in PRs
and serve as executable documentation of the event lifecycle.

### 5.2 Layer 2: Replay harness

The harness loads a fixture, deserializes each line, and:
- For `core::StreamEvent` lines: injects into the App's event handler
- For `__commit__` lines: calls `engine.append_message_for_testing()` to
  put a committed message in the conversation (Path B)
- For `__end_query__` lines: sets `query_running_ = false`, which triggers
  the idle path (`ConsumePendingResult` → `SyncState()`) on the next render.
  The idle path clears `streaming_text_` / `streaming_tools_` /
  `streaming_markdown_` but **not** `streaming_thinking_` — the thinking
  preview persists for the 30s grace period by design
  (`is_streaming_thinking_visible()`).
- For `__checkpoint__` lines: captures the rendered screen

The harness also manages `query_running_`:
- Set to `true` before the first event (simulating `HandleSubmit`)
- Set to `false` on `__end_query__` (simulating the query thread finishing)

Without this, `Render()` skips the streaming projection entirely (it is
gated on `is_query_running()`), and `SyncState()` (the idle path) clears
streaming state.

### 5.3 Layer 3: Screen assertion

Two complementary assertion modes:

- **Golden snapshots**: at named checkpoints, the rendered screen is
  normalized (ANSI-stripped with OSC awareness, spinner glyphs and relative
  timestamps replaced with placeholders, trailing whitespace trimmed) and
  compared byte-by-byte against a golden file. Regenerated via
  `LOOM_UPDATE_SNAPSHOTS=1`.
- **Invariant checks**: after *every* event, a set of predicates runs
  against the rendered screen. Each invariant is scoped to the states where
  it is valid (see §8.2).

### 5.4 Layer 4: Scenario E2E

Only App-side scenarios migrate to fixture-driven replay: multi-turn
conversations, streaming ↔ committed interaction, thinking block lifecycle,
tool call streaming. Engine-loop scenarios (retries, permissions,
compaction, multi-round tool loops) remain on HTTP mock servers (N4).

## 6. Fixture format specification

### 6.1 JSONL event format

Each line is a JSON object with a `"type"` discriminator. Streaming event
types map 1:1 to the `core::StreamEvent` variant in
`src/types/types.cppm:331`.

#### `stream_start`

```json
{"type":"stream_start","message_id":"msg_1","model":"test-model"}
```

Maps to `StreamStart { MessageId, std::string model }`.

#### `content_block_start`

The `block` field is a `ContentBlock` variant. Supported block types:

**TextBlock:**
```json
{"type":"content_block_start","index":0,"block":{"type":"text","text":""}}
```

**ThinkingBlock:**
```json
{"type":"content_block_start","index":0,"block":{"type":"thinking","thinking":"","signature":"sig-1"}}
```

**ToolUseBlock:**
```json
{"type":"content_block_start","index":0,"block":{"type":"tool_use","id":"tu1","name":"Bash","input_json":"{}"}}
```

**ImageBlock** (flat core type, matching `src/types/types.cppm:142`):
```json
{"type":"content_block_start","index":0,"block":{"type":"image","media_type":"image/png","data":"iVBOR...","width":800,"height":600}}
```

**DocumentBlock:**
```json
{"type":"content_block_start","index":0,"block":{"type":"document","media_type":"application/pdf","data":"JVBER..."}}
```

#### `content_block_delta`

```json
{"type":"content_block_delta","index":0,"delta_text":"Hello "}
```

Maps to `ContentBlockDelta { uint32_t index, std::string delta_text }`.

#### `content_block_stop`

```json
{"type":"content_block_stop","index":0}
```

Maps to `ContentBlockStop { uint32_t index }`.

#### `tool_execution_start`

```json
{"type":"tool_execution_start","tool_use_id":"tu1","tool_name":"Bash","input_json":"{\"command\":\"ls\"}"}
```

Maps to `ToolExecutionStart { tool_use_id, tool_name, input_json }`.

#### `tool_execution_progress`

```json
{"type":"tool_execution_progress","tool_use_id":"tu1","partial_result":"file1.txt\nfile2.txt"}
```

Maps to `ToolExecutionProgress { tool_use_id, partial_result }`.

#### `tool_execution_end`

```json
{"type":"tool_execution_end","tool_use_id":"tu1","result":"file1.txt\nfile2.txt","is_error":false}
```

Maps to `ToolExecutionEnd { tool_use_id, result, is_error }`.

#### `stream_end`

```json
{"type":"stream_end","stop_reason":"end_turn","usage":{"input_tokens":10,"output_tokens":5}}
```

Maps to `StreamEnd { optional<string> stop_reason, TokenUsage usage }`.
`stop_reason` may be `null`.

#### `stream_error`

```json
{"type":"stream_error","error_type":"overloaded_error","message":"Overloaded"}
```

Maps to `StreamError { error_type, message }`.

### 6.2 Pseudo-events

#### `__commit__`

Simulates the engine appending a committed `AssistantMessage` to the
conversation. This is the bridge to Path B (streaming ↔ committed
interaction). Without it, the conversation is empty and `Render()` shows
only streaming state — the duplicate-text and hidden-thinking bugs cannot
reproduce.

```json
{"type":"__commit__","message":{"role":"assistant","model":"test-model","stop_reason":"end_turn","content":[{"type":"text","text":"Hello world"}]}}
```

The `content` array uses the same block format as `content_block_start`
(§6.1). The harness deserializes this into a `core::AssistantMessage` and
calls `engine.append_message_for_testing(Message{assistant_msg})`.

#### `__end_query__`

Simulates the query thread finishing. Sets `query_running_ = false`, which
causes the next `Render()` to take the idle path (`SyncState()`), clearing
`streaming_text_` / `streaming_tools_` / `streaming_thinking_`.

```json
{"type":"__end_query__"}
```

#### `__checkpoint__`

Captures a screen snapshot. Not forwarded to the engine or App.

```json
{"type":"__checkpoint__","name":"after_thinking"}
```

Checkpoint names must be `[a-z0-9_]+`. Golden files are named
`<fixture_name>.<checkpoint_name>.txt`.

### 6.3 Fixture catalog

Phase 1 fixtures (the three bugs + basic flows):

| Fixture | What it covers | Path | Bug it guards |
|---|---|---|---|
| `text_simple` | Plain text streaming | A | — |
| `thinking_then_text` | Thinking block → text, with commit | A+B | Hidden thinking |
| `thinking_long_truncated` | Thinking > 200 chars, with commit | A+B | Truncated thinking |
| `thinking_multiple` | Two thinking blocks in one turn | A+B | Hidden thinking |
| `tool_call_bash` | Tool use → execution → result | A | — |
| `tool_call_then_text` | Tool use → result → assistant text, with commit | A+B | Duplicate text |
| `multi_turn_with_thinking` | Full multi-turn with thinking and commits | A+B | All three |
| `error_overloaded` | 529 error event | A | — |
| `error_rate_limit` | 429 error event | A | — |

Phase 2 fixtures (edge cases):

| Fixture | What it covers |
|---|---|
| `empty_content` | Empty text blocks |
| `very_long_text` | Text wrapping at 120 columns |
| `cjk_and_emoji` | CJK + emoji in thinking and text |
| `multiple_tools_parallel` | Two concurrent tool calls |
| `tool_result_with_image` | Tool returns image content |
| `duplicate_events` | Duplicate start/delta/stop (dedup) |
| `stream_interrupted` | Stream cut mid-sequence |

Fixtures that require the engine loop (deferred to HTTP mock server tests,
N4): `tool_permission_denied`, `compact_boundary`, `retry_after_overloaded`.

## 7. Replay harness design

### 7.1 Test seam: `AppAdapter::handle_stream_event`

The App's event handling logic currently lives in a lambda inside
`app_handle_submit.cpp:274` (~115 lines):

```cpp
opts.on_event = [this, &st](const core::StreamEvent& ev) {
    if (st.stop_requested()) return;
    // ... ~115 lines of event handling
};
```

**Refactoring**: extract the lambda body into a method:

```cpp
// app_handle_submit.cpp (or a new app_stream_event.cpp)
void AppAdapter::handle_stream_event(const core::StreamEvent& ev) {
    // ... the ~115 lines, unchanged
}
```

The lambda becomes a thin wrapper:

```cpp
opts.on_event = [this, &st](const core::StreamEvent& ev) {
    if (st.stop_requested()) return;
    handle_stream_event(ev);
};
```

The test seam is a public test-only method, following the existing
`append_message_for_testing` convention (no `#ifdef` — all 28 existing
seams are unconditional `_for_testing` methods):

```cpp
/// Inject a stream event directly into the App's event handler.
/// Bypasses the query thread, HTTP, and wire layers.
/// For testing only — production code goes through QueryEngine::stream_query.
void inject_stream_event_for_testing(const core::StreamEvent& ev);
```

The method calls `handle_stream_event(ev)` directly. Thread safety is
already handled by the `result_mutex_` locks inside the handler.

### 7.2 `query_running_` management

The streaming projection in `Render()` is gated on
`is_query_running()` (`app_render_event.cpp:137`). When `query_running_` is
false, `Render()` takes the idle path (`SyncState()`), which **clears**
`streaming_text_` / `streaming_tools_` / `streaming_thinking_`
(`app_agent_menu.cpp:430-432`). Without managing `query_running_`, injected
events are invisible.

The harness manages it through a test-only setter:

```cpp
/// Set the query_running_ flag for testing.
/// When true, Render() projects streaming state.
/// When false, Render() takes the idle path (SyncState), clearing streaming state.
/// Also sets the spinner to Requesting (matching HandleSubmit) so that
/// __end_query__ triggers the idle path even for fixtures with no content events.
void set_query_running_for_testing(bool running);
```

The harness calls `set_query_running_for_testing(true)` before the first
event and `set_query_running_for_testing(false)` on `__end_query__`.

The harness also clears the process-global `thinking_stream_last_seen`
map (`messages_list_payload_row.cpp:39-40`) between fixtures, so that
`was_recently_streaming()` does not leak state across tests.

### 7.3 `StreamReplayHarness` API

A new test utility in `tests/streaming_replay.hpp` (header-only if small,
`.cpp` + `.hpp` if the deserializer grows):

```cpp
namespace loom::testing {

/// A single step in a replay.
struct ReplayStep {
    enum class Kind { Event, Commit, EndQuery, Checkpoint };
    Kind kind = Kind::Event;
    std::optional<core::StreamEvent> event;    // Kind::Event
    std::optional<core::AssistantMessage> message;  // Kind::Commit
    std::string checkpoint_name;                // Kind::Checkpoint
};

/// Load a fixture file and parse it into replay steps.
/// Fixture path is resolved relative to tests/fixtures/streaming_sessions/.
/// Throws on parse error with the line number and JSON context.
[[nodiscard]] std::vector<ReplayStep> load_fixture(
    std::string_view fixture_name);

/// The replay harness: injects events into an App, simulates commits,
/// manages query_running_, and captures screens.
class StreamReplayHarness {
public:
    /// Construct with the app and engine to drive.
    /// The engine is needed for __commit__ (append_message_for_testing).
    StreamReplayHarness(ui::AppAdapter& app, core::QueryEngine& engine);

    /// Play all steps. At each checkpoint, captures the rendered screen
    /// (normalized) and stores it.
    /// Returns a map of checkpoint_name → rendered screen text.
    [[nodiscard]] std::map<std::string, std::string> play(
        std::span<const ReplayStep> steps,
        int width = 120, int height = 40);

    /// Play all steps, calling `invariant_check` after every event.
    /// The checker receives the rendered screen and the step index.
    [[nodiscard]] std::map<std::string, std::string> play_with_invariants(
        std::span<const ReplayStep> steps,
        std::function<void(std::string_view screen, std::size_t step_idx)> invariant_check,
        int width = 120, int height = 40);

private:
    ui::AppAdapter& app_;
    core::QueryEngine& engine_;

    void apply_step(const ReplayStep& step);
    [[nodiscard]] std::string capture_screen(int width, int height) const;
};

}  // namespace loom::testing
```

### 7.4 Screen normalization

The `capture_screen` method renders the app to plain text and normalizes
time-varying content so golden snapshots are reproducible:

1. **ANSI stripping**: use the OSC-aware `strip_ansi` from
   `test_markdown_render_snapshot.cpp:54` (handles both CSI and OSC 8
   hyperlinks), not the CSI-only version in `test_ui_helpers.h:49`.
2. **Spinner normalization**: replace spinner glyphs
   (`⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏`) with `<spinner>`.
3. **Timestamp normalization**: replace relative timestamps
   (`just now`, `Xm ago`, `Xh ago`) with `<timestamp>`.
4. **Trailing whitespace**: trim per line, collapse trailing blank lines.

The normalization functions live in a shared header
(`tests/screen_normalize.hpp`) so both the markdown snapshot suite and the
streaming replay suite use the same implementation (consolidating the two
existing copies).

### 7.5 Fixture deserialization

A JSON parser in `tests/streaming_replay.hpp` (or `.cpp`) that maps JSONL
lines to `ReplayStep` values. Uses the existing `loom::serdes::json` module
for parsing. The parser covers the event types in §6.1 and the
pseudo-events in §6.2.

## 8. Screen assertion

### 8.1 Golden snapshots

Golden files live under `tests/fixtures/streaming_snapshots/`:

```
tests/fixtures/streaming_snapshots/
├── thinking_then_text.after_thinking.txt
├── thinking_then_text.after_commit.txt
├── thinking_long_truncated.after_thinking.txt
├── tool_call_then_text.after_commit.txt
├── ...
```

The test file `tests/test_streaming_replay.cpp` has one TEST per fixture:

```cpp
TEST(StreamingReplay, ThinkingThenText) {
    auto steps = testing::load_fixture("thinking_then_text");
    testing::StreamReplayHarness harness(app, engine);
    auto snapshots = harness.play(steps);
    expect_streaming_snapshot("thinking_then_text", "after_thinking",
                              snapshots["after_thinking"]);
    expect_streaming_snapshot("thinking_then_text", "after_commit",
                              snapshots["after_commit"]);
}
```

`expect_streaming_snapshot()` follows the same pattern as
`expect_snapshot()` in `test_markdown_render_snapshot.cpp:132`:

- Compare against `tests/fixtures/streaming_snapshots/<fixture>.<checkpoint>.txt`
- If `LOOM_UPDATE_SNAPSHOTS=1`, write the golden file and pass
- On mismatch, print a unified diff and fail

### 8.2 Invariant catalog

Invariants are predicates on the rendered screen that must hold after
**every** step, but each invariant is **scoped to the states where it is
valid**. An invariant that fires on correct intermediate state is worse
than no invariant — it trains developers to ignore failures.

Each invariant is a function `(screen, step_idx, context) → void` where
`context` indicates whether the current state is streaming, committed, or
post-query. The catalog:

| ID | Invariant | Scoped to | Rationale |
|---|---|---|---|
| INV-01 | **No crash**: the screen renders without throwing | Always | Baseline |
| INV-02 | **Thinking not truncated in expanded view**: if a thinking block is expanded (visible `∴ Thinking…` label + body), the full thinking content must appear | Always (when thinking is expanded) | Truncated thinking bug |
| INV-03 | **No duplicate content after commit**: after a `__commit__` step while `query_running_` is still true, the same 40+ char text block must not appear in both a streaming row and a committed row | Post-commit, pre-`__end_query__` | Duplicate text bug |
| INV-04 | **Streaming text cleared after query end**: after `__end_query__`, the streaming text area must not contain text that also appears in a committed assistant message | Post-`__end_query__` | Duplicate text bug |
| INV-05 | **No empty tool blocks after completion**: after `ToolExecutionEnd` for a tool, the visible `● <ToolName>` line must be followed by either an input line or a result line | Post-`ToolExecutionEnd` | Empty tool call |
| INV-06 | **Tool result before assistant text**: in a multi-turn flow with commits, the tool result line must appear before the subsequent assistant text line | Fixtures with tool + text commits | Ordering |

**Deliberately excluded invariants** (unsound or untestable in replay):

- ~~"Completed thinking is collapsed"~~ — during the 30s grace period,
  completed thinking is expanded (if streaming tail) or hidden (if not
  tail); the collapsed label appears only after grace expiry AND from the
  committed message. Testing this requires a clock seam (§8.3).
- ~~"No orphaned spinners"~~ — the spinner is advanced by a wall-clock
  ticker; in replay the glyph is non-deterministic. Spinner glyphs are
  normalized to `<spinner>` in golden output instead.

### 8.3 Clock seam

The 30s thinking grace period (`is_streaming_thinking_visible()`) and the
3s collapse grace (`kThinkingCollapseGrace` in `messages_list_payload_row.cpp`)
both call `std::chrono::steady_clock::now()` directly. Testing grace expiry
without sleeping requires an injectable clock.

**Implemented** as `loom.ui.foundation.clock`: a `steady_now()` function with
a process-global `set_steady_now_for_testing()` override. Seven
`steady_clock::now()` call sites were migrated to `clock::steady_now()`.
When the override is unset, behavior is identical to direct
`steady_clock::now()` calls. The following grace-expiry tests were added:

- `thinking_grace_expiry` fixture + `ThinkingGraceExpiry` test: thinking
  completes → 30s passes → collapsed label appears from committed message
  (INV-07)
- `thinking_collapse_grace` fixture + `ThinkingCollapseGrace` test: 3s
  collapse grace keeps a thinking row expanded, then collapses it
- `ClockResetBetweenFixtures` test: the harness clears the clock override
  at the start of every `play()` so simulated time cannot leak across
  fixtures

## 9. Phases and graduation criteria

| Phase | Title | Gate |
|---|---|---|
| 1 | Test seam + harness + fixture format | debug + release build, `test_streaming_replay` passes |
| 2 | Initial fixture library + golden snapshots | all 9 fixtures pass with golden snapshots + invariants |
| 3 | Invariant checker | all invariants pass on all fixtures |
| 4 | Migrate existing App-side E2E tests | ctest green, migrated tests <100ms each |
| 5 | Edge-case fixtures + clock seam | ongoing — fixtures and invariants grow with the codebase |

### Phase 1: Test seam + harness + fixture format (1 session)

1. Extract `handle_stream_event()` from the `on_event` lambda in
   `app_handle_submit.cpp` into a method on `AppAdapter`.
2. Add `inject_stream_event_for_testing()` and
   `set_query_running_for_testing()` public methods.
3. Create `tests/streaming_replay.hpp` with `load_fixture()`,
   `StreamReplayHarness`, and `ReplayStep`.
4. Create `tests/screen_normalize.hpp` with the OSC-aware `strip_ansi`,
   spinner/timestamp normalization, and whitespace trimming.
5. Create `tests/test_streaming_replay.cpp` with one smoke test
   (`text_simple` fixture).
6. Create `tests/fixtures/streaming_sessions/text_simple.jsonl`.

**Gate**: debug + release build, `test_streaming_replay` passes.

### Phase 2: Initial fixture library + golden snapshots (1 session)

1. Author the 9 Phase 1 fixtures (§6.3), including `__commit__` and
   `__end_query__` pseudo-events where needed.
2. Add golden snapshot comparison to `test_streaming_replay.cpp`.
3. Generate golden files via `LOOM_UPDATE_SNAPSHOTS=1`.
4. Add the three bug-specific fixtures with invariants that would have
   caught the bugs:
   - `tool_call_then_text` + INV-03 (duplicate text, post-commit)
   - `thinking_long_truncated` + INV-02 (truncated thinking)
   - `thinking_then_text` + golden snapshot at `after_commit`: during the
     30s grace, the committed thinking row is **hidden** by the filter
     (`messages_list_filter.cpp:534`), while the streaming-tail thinking
     row is visible (expanded). The snapshot asserts this grace-period
     behavior. The collapsed-label assertion (after grace expiry) is
     covered by the clock-seam tests (§8.3).

**Gate**: all 9 fixtures pass with golden snapshots + invariants.

### Phase 3: Invariant checker (1 session)

1. Implement the `InvariantChecker` class with INV-01 through INV-06.
2. Wire `play_with_invariants()` into all existing replay tests.
3. Run the full invariant suite against all fixtures.

**Gate**: all invariants pass on all fixtures.

### Phase 4: Migrate existing App-side E2E tests (1 session)

1. Audit `test_ui_e2e.cpp` and `test_ui_runtime.cpp` for tests that use
   HTTP mock servers but don't test wire-level or engine-loop behavior.
2. Migrate App-side tests to fixture-driven replay where the fixture
   approach is strictly better (faster, more readable).
3. Keep HTTP mock server tests for wire-level integration (SSE parsing,
   HTTP headers, chunked encoding) and engine-loop behavior (retries,
   permissions, compaction, multi-round tool loops).

**Gate**: ctest green, migrated tests run in <100ms each (vs. ~500ms
for HTTP mock server tests).

### Phase 5: Edge-case fixtures (ongoing)

1. Author the 7 Phase 2 fixtures (§6.3) as edge cases are discovered.
2. Add new invariants as new bug classes are found.
3. ~~When the clock seam lands (§8.3), add grace-expiry fixtures and INV-07.~~
   Done — see §8.3 for the clock seam and grace-expiry tests.

**Gate**: ongoing — fixtures and invariants grow with the codebase.

## 10. Risks and mitigations

| Risk | Mitigation |
|---|---|
| **Test seam rots**: `handle_stream_event` drifts from the lambda's behavior | The lambda is a 3-line wrapper that calls `handle_stream_event`. There is no duplicated logic to drift. |
| **Fixture format churn**: `core::StreamEvent` or `ContentBlock` variant changes, breaking fixtures | The deserializer is the single point of change. Adding a new event/block type is a ~20-line parser addition + one fixture. The variant has been stable since the initial port, but the churn surface includes `ContentBlock` (6 alternatives) and `TokenUsage`, not just `StreamEvent`. |
| **Golden snapshot flakiness**: screen output varies across environments | Pin `TERM=xterm-256color`, `LANG=C.UTF-8`, `TZ=UTC` in the test target (same as tuiwright). Normalize spinner glyphs and relative timestamps to placeholders (§7.4). The existing markdown snapshot suite already runs green on both macOS and Linux. |
| **Invariant false positives**: invariants that are too strict fail on valid screens | Invariants are scoped to the states where they are valid (§8.2). Each invariant is validated against all fixtures before being enabled by default. |
| **Test maintenance burden**: fixtures become stale as the UI evolves | Fixtures are version-controlled and reviewed in PRs. Golden snapshots are regenerated via `LOOM_UPDATE_SNAPSHOTS=1` (same workflow as markdown snapshots). The regeneration is a deliberate, reviewed action — not automatic. |
| **`__commit__` diverges from real engine assembly**: the fixture-specified committed message may not match what the engine would actually assemble | The `__commit__` message is hand-authored to match the streaming events in the same fixture. This is a feature, not a bug: it lets tests focus on the UI's rendering of the streaming ↔ committed interaction without coupling to the engine's assembly logic. Engine assembly is tested separately by wire-level tests. |

## 11. Alternatives considered

### A1. Record real API sessions for replay (Halfhand-style)

**Pros**: zero fixture authoring effort, captures real-world event sequences.
**Cons**: requires API access to record, fixtures contain model-specific
output that changes across providers, can't test edge cases (errors,
timeouts) without real failures. **Rejected for now** — hand-authored
fixtures are more targeted and reviewable. A `loom record` command is a
natural future extension (N1).

### A2. PTY-based testing (tuiwright-style)

**Pros**: tests the real terminal I/O path, catches raw-mode issues.
**Cons**: slower (process spawn + PTY setup), more brittle (terminal
capability negotiation), harder to assert on screen state. **Rejected** —
FTXUI's headless `Screen::Create()` + `Render()` is sufficient for
rendering correctness. PTY testing is only needed for raw-mode terminal
behavior, which is a separate concern (N6).

### A3. HTTP mock server for everything (status quo)

**Pros**: tests the full stack including wire protocol and engine loop.
**Cons**: slow (~500ms per test), brittle (port conflicts, thread timing),
SSE sequences hardcoded as C++ string literals. **Rejected as the primary
approach** — HTTP mock servers remain for wire-level and engine-loop
integration tests (Layer 4, N4), but the replay harness is the primary tool
for event-pipeline testing (Layers 1–3).

### A4. Property-based fuzzing of event sequences

**Pros**: catches unexpected edge cases, no fixture authoring.
**Cons**: requires a generator for valid event sequences, harder to
reproduce failures, needs the invariant checker as a prerequisite.
**Deferred** (N2) — the invariant checker (Phase 3) is the foundation.
A fuzzer that generates random valid event sequences and runs them through
`play_with_invariants()` is a natural follow-up.

### A5. Event injection without commit simulation (revision 1 of this RFC)

**Pros**: simpler harness, no engine dependency.
**Cons**: cannot reproduce the duplicate-text and hidden-thinking bugs
(they require committed and streaming rows to coexist). G5 fails 2/3.
**Rejected** — the `__commit__` pseudo-event (§6.2) is the bridge that
makes Path B testable.

## Production Readiness Review

| Criterion | Status | Evidence |
|---|---|---|
| Test seam is minimal and non-invasive | ✅ | `handle_stream_event()` extracted from lambda; lambda is 3-line wrapper; test seams are private + friend proxy (P5 convention) |
| Fixtures are version-controlled and reviewed | ✅ | 27 JSONL fixtures under `tests/fixtures/streaming_sessions/` |
| Golden snapshots are deterministic | ✅ | 62 golden files under `tests/fixtures/streaming_snapshots/`; `loom_test_main` scrubs terminal-identifying env vars; `normalize_snapshot()` handles ANSI/spinner/timestamp |
| Invariants catch the three original bugs | ✅ | INV-02 (truncation), INV-07 (hidden thinking via grace-expiry); Bug 1 (duplicate text) caught by targeted same-turn fixture + `count_text_on_screen` check; negative verification confirmed all 3 |
| Migrated tests are faster | ✅ | 8 E2E tests: ~500ms → ~0.66ms each (750× speedup) |
| Full test suite green | ✅ | 2014/2014 ctest (debug + release) |
| Clock seam enables time-dependent tests | ✅ | `steady_now()` + `set_steady_now_for_testing()`; 7 call sites migrated; grace-expiry tests pass |
| No new warnings | ✅ | `-Werror -Wall -Wextra -Wpedantic` clean |

## Rollout and rollback

**Rollout**: the test system is additive — new files only, no production
behavior changes (except the three pre-existing bug fixes, which were
already landed). The clock seam (`loom.ui.foundation.clock`) is a new
module with zero production call-site behavior change (all call sites
behave identically when the override is unset).

**Rollback**: delete the new files and revert the source modifications.
The test seam extraction (`handle_stream_event`) is a pure refactor —
reverting it restores the lambda without behavior change. The clock seam
revert restores `steady_clock::now()` calls. No data migration, no
config change, no wire-format change.

## Testing and verification plan

1. **Unit tests**: 28 streaming replay tests (20 StreamingReplay + 8
   StreamingMigration) covering text, thinking, tool calls, errors,
   edge cases, grace expiry, collapse grace, and clock reset.
2. **Golden snapshots**: 62 checkpoint snapshots across 27 fixtures,
   compared with OSC-aware ANSI stripping and normalization.
3. **Invariant checks**: INV-01 through INV-07 run on every fixture,
   covering crash-freedom, thinking truncation, duplicate content,
   streaming cleanup, empty tool blocks, tool-result ordering, and
   grace-expiry collapse.
4. **Full suite**: `ctest --preset debug` and `ctest --preset release`
   both 2014/2014 green.
5. **Negative verification**: each of the three original bug fixes was
   temporarily reverted; the corresponding test/invariant/golden
   snapshot failed as expected, then the fix was restored. Bug 1
   (duplicate text) initially had a test gap — no fixture combined
   tool_use + text in the same turn — fixed by adding the
   `tool_use_and_text_same_turn` fixture with a targeted
   `count_text_on_screen` assertion.

## Implementation History

| Date | Phase | Event | Evidence |
|---|---|---|---|
| 2026-10-07 | — | RFC opened (draft); three adversarial design reviews (agent:design-review#1, request-changes; agent:design-verify#1, verified) | 3 revisions, F1-F5 + N1-N5 findings all applied |
| 2026-10-07 | 1–3 | Test seam, replay harness, fixture library, golden snapshots, invariant checker implemented via multi-agent workflow | 9 StreamingReplay tests, 9 fixtures, 18 goldens, INV-01–06 |
| 2026-10-07 | 4–5 | E2E test migration, edge-case fixtures, clock seam, grace-expiry tests implemented via multi-agent workflow | 8 StreamingMigration tests, 16 new fixtures, 40 new goldens, INV-07, clock module |
| 2026-10-07 | — | RFC promoted draft → implemented; negative verification confirmed Bug 2 and Bug 3 caught; Bug 1 test gap found and fixed with same-turn fixture | 2014/2014 ctest, 28 tests, 62 goldens |
| 2026-10-07 | — | Negative verification: Bug 2 (truncation) caught by INV-02, Bug 3 (hidden thinking) caught by INV-07; Bug 1 (duplicate text) had a test gap — no fixture had tool_use + text in the same turn. Fixed by adding `tool_use_and_text_same_turn` fixture + `count_text_on_screen` targeted check. Clock seam grace-period tests added (3s collapse grace + clock reset) | agent:negative-verify |

## Evidence

- **Test count**: 28 streaming replay tests (20 StreamingReplay + 8 StreamingMigration), 2014/2014 full suite
- **Fixtures**: 27 JSONL event fixtures (9 original + 8 migration + 7 edge-case + 1 grace-expiry + 1 collapse-grace + 1 same-turn)
- **Golden snapshots**: 62 checkpoint snapshots across all fixtures
- **Invariants**: INV-01 through INV-07, all passing on all fixtures
- **Migration speedup**: 8 E2E tests from ~500ms to ~0.66ms each (750×)
- **Clock seam**: `loom.ui.foundation.clock` module, 7 `steady_clock::now()` call sites migrated
- **Build**: debug + release `-Werror` clean
- **Negative verification**: 3/3 original bugs caught by tests when fixes reverted
