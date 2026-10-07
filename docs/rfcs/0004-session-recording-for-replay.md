---
rfc: 4
title: Session Recording for Replay Fixtures
status: accepted
owners: "@Zzzode"
reviewers:
  - agent:design-review-r1 (request-changes: 12 findings, 3 fatal)
  - agent:design-review-r2 (request-changes: 6 findings, 2 medium)
  - agent:design-review-r3 (approve: all findings addressed)
created: 2026-10-07
last-reviewed: 2026-10-07
tracking: local
---

# RFC 0004 — Session Recording for Replay Fixtures

## Summary

Add a `loom record` command that captures a live API session — the SSE event
stream, engine commits, and tool executions — as a JSONL replay fixture
compatible with the RFC 0003 streaming replay harness. This closes the loop
between real-world sessions and the test fixture library: developers can
record a bug-triggering session and immediately replay it as a regression
test, without hand-authoring JSONL.

## Motivation

RFC 0003 established a fixture-driven replay testing system with 27 hand-
authored fixtures. Hand-authoring is precise but slow: each fixture requires
manual JSONL construction, and reproducing a real bug means reverse-
engineering the event sequence from debug traces. The project already
persists two traces per session (`messages.jsonl` and `dump-prompts/`), but
neither is in the replay fixture format — `messages.jsonl` records committed
messages (not streaming events), and `dump-prompts/` records raw API
request/response pairs (not the core-level event stream).

### Evidence

| Metric | Current | Target | How measured |
|---|---|---|---|
| Fixture authoring time | ~15–30 min per hand-authored fixture | <1 min (record + trim) | Developer time per fixture |
| Real-bug coverage | 0 fixtures from real sessions | All streaming bugs reproducible | Bug → fixture conversion |
| Trace → fixture conversion | Manual (read traces, hand-write JSONL) | Automatic (`loom record`) | Steps in workflow |

## Goals

- G1. A `loom record` command that captures a live session as a JSONL replay
  fixture in RFC 0003 format.
- G2. Recorded fixtures are immediately replayable through
  `StreamReplayHarness` with no manual editing.
- G3. Sensitive data (API keys, Authorization headers) is never written to
  the fixture file.
- G4. Recording is opt-in and has zero overhead when disabled.

## Non-Goals

- N1. Recording on the wire (HTTP/SSE) layer. Recording happens at the
  `core::StreamEvent` boundary, after wire parsing. Wire-level recording
  would couple fixtures to a specific wire backend (Messages API vs OpenAI).
- N2. Automatic fixture minimization (shrinking). A recorded fixture may
  contain irrelevant events; shrinking is a separate concern (RFC 0005
  property-based fuzzing may address this).
- N3. Continuous recording of every session. Recording is explicitly
  triggered by the user.
- N4. Recording tool execution internals (stdout/stderr streams). Only
  `ToolExecutionStart` / `ToolExecutionEnd` events are captured, matching
  the existing fixture format.

## Proposal

### Architecture

```
Live session:
  HTTP SSE → WireBackend → core::StreamEvent → QueryEngine
                                              ↓
                                         App handler
                                              ↓
                                         [Recorder] ← writes JSONL fixture
```

The recorder taps into the event stream at the `core::StreamEvent` boundary
— the same boundary where the replay harness injects events. This ensures
recorded fixtures are wire-agnostic and directly replayable.

### Recording trigger

One mode:

**`loom record`** — starts a new session with recording enabled. The
recorder taps into the query engine's event stream and writes the fixture
when the session ends. Output: `~/.loom/recordings/recorded_<timestamp>.jsonl`
(with `--install` flag to copy into `tests/fixtures/streaming_sessions/`).

Converting existing sessions (`loom record <session-id>`) requires always-on
streaming event capture, which is a separate concern — see "Future work"
below.

### Fixture format

Recorded fixtures use the exact RFC 0003 JSONL format (§6 of RFC 0003):
- `stream_start`, `content_block_start`, `content_block_delta`,
  `content_block_stop`, `stream_end`, `stream_error`
- `tool_execution_start`, `tool_execution_end` (engine-internal events,
  emitted by `execute_pending_tools`)
- `__commit__` (captured from engine commits)
- `__end_query__` (captured when the query thread finishes)
- `__checkpoint__` (inserted at key moments: after commit, after end_query)

**Note**: `ToolExecutionProgress` is never emitted by the live engine — it
exists only in hand-authored fixtures. Recorded fixtures will not contain
progress events.

### Sensitive data redaction

The recorder redacts:
- API keys and Authorization headers (never present at the
  `core::StreamEvent` boundary — they live in the wire layer)
- Text blocks matching configurable regex patterns (e.g., API key patterns)
  are replaced with `<redacted>`
- Tool results from configurable tool names (e.g., `Bash` with sensitive
  commands) are truncated or redacted
- Redaction rules are configured via `settings.json` under a `record` key:
  ```json
  {
    "record": {
      "redact_patterns": ["sk-[a-zA-Z0-9]+", "ghp_[a-zA-Z0-9]+"],
      "redact_tools": ["Bash"]
    }
  }
  ```
- Redacted fixtures may no longer reproduce the original bug — the user
  should review the fixture before committing it as a regression test

### Detailed design

#### Tap point

The recorder taps into the query engine at the `stream_query` level
(`src/query/query_engine_loop.cpp`). The engine's `stream_query` method
orchestrates the full query lifecycle:

1. Calls `stream_single_api_call` for the SSE stream (emits streaming
   events via `options.on_event`)
2. Calls `execute_pending_tools` for tool execution (emits
   `ToolExecutionStart` / `ToolExecutionEnd` via `options.on_event`)
3. Calls `append_message` for commits (lines 105, 142, 150, 168, 180)
4. Returns when the query finishes

The recorder is installed via two new optional callbacks in `QueryOptions`
(`src/query/query_engine.cppm`):

```cpp
struct QueryOptions {
    // ... existing fields ...
    StreamCallback on_event;  // existing: called for every StreamEvent

    /// Called when a message is committed to the conversation.
    /// The recorder serializes this as a __commit__ line.
    std::function<void(const Message&)> on_commit;

    /// Called when the query thread finishes (stream_query returns).
    /// The recorder serializes this as __end_query__.
    std::function<void()> on_end_query;
};
```

The engine calls these at the appropriate points in `stream_query`:
- `options.on_event(ev)` — already called for every StreamEvent (streaming
  + tool-execution)
- `options.on_commit(message)` — called inside `append_message` itself
  (not at the 5 explicit call sites in `stream_query`), so it fires for
  ALL commits including native agent notification commits from
  `append_pending_native_agent_notifications()` (line 125), which
  internally calls `append_message` and has no access to `QueryOptions`
- `options.on_end_query()` — called when `stream_query` returns, including
  early returns (e.g., budget exceeded at line 82). Implemented via RAII
  scope guard to ensure it fires on all exit paths

The App (`app_handle_submit.cpp`) sets these callbacks when the `record`
subcommand is active:

```cpp
// app_handle_submit.cpp — inside HandleSubmit
if (recorder_) {
    opts.on_event = [this, &st](const StreamEvent& ev) {
        recorder_->record_event(ev);  // thread-safe (mutex-protected)
        if (st.stop_requested()) return;
        handle_stream_event(ev);
    };
    opts.on_commit = [this](const Message& msg) {
        recorder_->record_commit(msg);
    };
    opts.on_end_query = [this]() {
        recorder_->record_end_query();
    };
}
```

The `st.stop_requested()` guard is preserved for `handle_stream_event` (to
avoid updating UI state after the query thread is stopped) but intentionally
bypassed for `record_event` (to capture the complete event stream).

**Thread safety**: `execute_pending_tools` executes read-only tools in
parallel via `std::async` (`query_engine_tools.cpp:63-81`). Each parallel
`execute_single_tool` call emits `ToolExecutionStart` / `ToolExecutionEnd`
via `options.on_event` from a different thread. The recorder's
`record_event` must be mutex-protected to serialize concurrent writes to
the JSONL file.

This captures the complete event stream — streaming events, tool-execution
events, commits, and end-query — at the same boundary where the replay
harness injects them.

#### Serialization

The recorder uses a dedicated `StreamEvent → JSON` serializer that targets
the RFC 0003 fixture format. This is **not** a wrapper around
`message_to_jsonl_()` (which produces wire-format JSON with different
field names and omits `model`/`stop_reason`).

Each `StreamEvent` variant maps to a JSONL line:

| StreamEvent | JSON `"type"` | Key fields |
|---|---|---|
| `StreamStart` | `stream_start` | `message_id`, `model` |
| `ContentBlockStart` | `content_block_start` | `index`, `block` (ContentBlock) |
| `ContentBlockDelta` | `content_block_delta` | `index`, `delta_text` |
| `ContentBlockStop` | `content_block_stop` | `index` |
| `ToolExecutionStart` | `tool_execution_start` | `tool_use_id`, `tool_name`, `input_json` |
| `ToolExecutionEnd` | `tool_execution_end` | `tool_use_id`, `result`, `is_error` |
| `StreamEnd` | `stream_end` | `stop_reason`, `usage` |
| `StreamError` | `stream_error` | `error_type`, `message` |

The `ContentBlock` variant serializes using the same `"type"` discriminator
as the fixture parser (`text`, `thinking`, `tool_use`, `tool_result`,
`image`, `document`).

**Note**: `content_block_start` for tool_use blocks has empty `input_json`
in the live engine (default-constructed at block-start time). Hand-authored
fixtures use `"input_json":"{}"`. The parser accepts both; the recorder
writes what the engine produces (empty string).

#### Commit serialization

The `on_commit` callback receives a `Message` variant. The recorder
serializes it as `{"type":"__commit__","message":{...}}` using the fixture
format:

- **AssistantMessage**: `role`, `model`, `stop_reason`, `content` (array of
  ContentBlock JSON)
- **UserMessage**: `role`, `content` (array of ContentBlock JSON)
- **ToolResultMessage**: `role: "tool"`, `tool_use_id`, `tool_name`,
  `is_error`, `content` (array of ContentBlock JSON)

This differs from the wire format produced by `message_to_jsonl_()`, which
omits `model`/`stop_reason` for assistant messages and uses `role: "user"`
for tool results. The fixture parser (`streaming_replay.hpp:329-384`) expects
the fixture format, not the wire format.

#### Round-trip test

A mandatory round-trip test guards against serializer/parser drift:
construct each `StreamEvent` variant and each `Message` variant, serialize
to JSON, parse back via `streaming_replay.hpp`, and compare. This is the
automated guard for the string/shape-based coupling between serializer and
parser.

#### Checkpoints

The recorder inserts `__checkpoint__` lines at key moments:
- After each `__commit__`: `{"type":"__checkpoint__","name":"after_commit_N"}`
- After `__end_query__`: `{"type":"__checkpoint__","name":"after_end_query"}`

#### Multi-turn sessions

A single live session may contain multiple query rounds (user submits,
assistant responds, user submits again). The recorder captures all rounds
in a single fixture. Each round starts with a `StreamStart` and ends with
`__end_query__`. The replay harness already supports multi-turn fixtures
(see `multi_turn_with_thinking.jsonl`).

#### CLI integration

`loom record` is a CLI subcommand. The main entry point
(`src/main.cpp`) checks for the `record` subcommand and creates a
`SessionRecorder` before starting the TUI. When the session ends, the
recorder writes the fixture and prints the output path.

#### Headless mode

Headless mode (`engine.query()`, `main.cpp:1090`) goes through
`execute_tool_loop` / `call_api` — not `stream_single_api_call`. In this
path, `on_event` fires only for `ToolExecutionStart` / `ToolExecutionEnd`
(from `execute_single_tool`); no streaming events (`StreamStart`,
`ContentBlockDelta`, `StreamEnd`, etc.) are emitted. A headless recording
would therefore lack the streaming event sequence that makes a fixture
replayable.

**Recording is streaming-only.** The `loom record` command requires the
streaming path (TUI mode). Headless recording is not supported — the
non-streaming path does not produce the event stream that the replay
harness expects. This may be revisited if the non-streaming path is
migrated to emit streaming events in the future.

#### Output location

Default: `~/.loom/recordings/recorded_<timestamp>.jsonl`. With `--install`:
copies the fixture to `tests/fixtures/streaming_sessions/` and prints the
fixture name for use in tests.

#### Overhead

When the `record` subcommand is absent, no callbacks are installed — the
recorder is never constructed. Overhead is zero (no conditional checks in
the event path).

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Recorder core | `on_commit`/`on_end_query` callbacks in QueryOptions + StreamEvent→JSON serializer + JSONL writer + checkpoints + round-trip test | proposed | A recorded fixture replays through StreamReplayHarness with zero edits; serializer is the inverse of streaming_replay.hpp parser (round-trip test passes for all 9 event types + 3 message types) |
| B | CLI integration | `loom record` subcommand + output to ~/.loom/recordings/ + `--install` flag + redaction rules | proposed | `loom record` produces a valid fixture from a live session; `--install` copies to fixtures dir; redacted patterns never appear in output |

## Production Readiness Review

TBD — to be filled before `implementable` gate.

## Future work

- **Always-on streaming trace**: capture streaming events in every session
  (not just record mode), enabling `loom record <session-id>` to convert
  existing sessions to fixtures. This requires a persistent event log
  separate from `messages.jsonl` (which records committed messages, not
  streaming events).
- **Fixture minimization (shrinking)**: automatically trim irrelevant events
  from recorded fixtures. RFC 0005's shrinker may address this.

## Rollout and rollback

- **Rollout**: additive — new command + new recorder module. No production
  behavior change when recording is disabled.
- **Rollback**: delete the new files. No data migration, no config change.

## Drawbacks

- Recorded fixtures may contain user content that should not be committed to
  version control. Mitigation: redaction + `.gitignore` for recorded
  fixtures by default.
- The recorder adds a small amount of complexity to the event pipeline.
  Mitigation: zero overhead when disabled — no callbacks are installed,
  no conditional checks in the event path.

## Alternatives considered

### A1. Hand-author fixtures only (status quo)

**Pros**: full control over fixture content; no new code.
**Cons**: slow (~15–30 min per fixture); cannot capture real-world event
sequences without manual reverse-engineering. **Rejected** — the fixture
library grows slowly and real bugs are hard to reproduce.

### A2. Record at the HTTP/SSE wire layer

**Pros**: captures the raw wire protocol; useful for wire-level debugging.
**Cons**: couples fixtures to a specific wire backend; requires wire-level
parsing in the replay harness; duplicates the wire parsing logic. **Rejected**
— the replay harness starts at the `core::StreamEvent` boundary (RFC 0003
N3), so wire-level recording would need a separate replay path.

### A3. Convert `dump-prompts/` to fixtures

**Pros**: `dump-prompts/` already captures API request/response pairs.
**Cons**: `dump-prompts/` records the API request body and parsed response,
not the streaming event sequence; converting requires replaying the SSE
stream through the wire parser, which is fragile. **Rejected** — too indirect;
recording at the `core::StreamEvent` boundary is simpler and more reliable.

## Testing and verification plan

- New unit tests for the recorder (event tap, JSONL writer, redaction,
  round-trip serialization)
- Integration test: record a session → replay the fixture → compare screens
- Round-trip test: serialize each StreamEvent variant + each Message variant
  to JSON, parse back via `streaming_replay.hpp`, compare — guards against
  serializer/parser drift
- Expected ctest total: ~2030 (2014 existing + ~16 new recorder tests)
- Dual preset `-Werror`; serial ctest

## Documentation impact

- [ ] `CLAUDE.md` — add `loom record` to debug traces section
- [ ] `docs/dev/` — add a guide on recording and replaying sessions
- [ ] `docs/decisions/design-decisions.md` — record the recording boundary
      decision

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| Should recording be a compile-time flag or runtime toggle? | @Zzzode | Runtime CLI subcommand (`loom record`) — zero overhead when absent |
| How to handle multi-turn sessions in a single fixture? | @Zzzode | Capture all rounds in one fixture; each round starts with StreamStart (matches existing multi_turn_with_thinking.jsonl pattern) |
| Should recorded fixtures be auto-committed or gitignored? | @Zzzode | Default to `~/.loom/recordings/`; `--install` flag copies to fixtures dir for explicit commit |
| Should `loom record <session-id>` convert existing traces? | @Zzzode | Deferred to future work — existing traces (messages.jsonl, dump-prompts) don't capture streaming events; requires always-on capture |

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-07 | — | RFC opened (provisional) |  | — |
| 2026-10-07 | — | Design review R1: request-changes (12 findings, 3 fatal) |  | Tap point, const-ref, wire-format serializer |
| 2026-10-07 | — | Design review R2: request-changes (6 findings, 2 medium) |  | Headless mode, on_commit coverage |
| 2026-10-07 | — | Design review R3: approve → status accepted |  | All 18 findings addressed across 2 rounds |
