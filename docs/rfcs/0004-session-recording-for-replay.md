---
rfc: 4
title: Session Recording for Replay Fixtures
status: provisional
owners: "@Zzzode"
reviewers: []
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

Two modes:

1. **`loom record`** — starts a new session with recording enabled. When the
   session ends (or the user presses a key), the fixture is written to
   `tests/fixtures/streaming_sessions/recorded_<timestamp>.jsonl`.
2. **`loom record <session-id>`** — converts an existing session's traces
   (`messages.jsonl` + `dump-prompts/`) into a replay fixture. This is
   useful when a bug was encountered in a previous session.

### Fixture format

Recorded fixtures use the exact RFC 0003 JSONL format (§6 of RFC 0003):
- `stream_start`, `content_block_start`, `content_block_delta`,
  `content_block_stop`, `stream_end`, `stream_error`
- `tool_execution_start`, `tool_execution_progress`, `tool_execution_end`
- `__commit__` (captured from engine commits)
- `__end_query__` (captured when the query thread finishes)
- `__checkpoint__` (inserted at key moments: after commit, after end_query)

### Sensitive data redaction

The recorder redacts:
- API keys and Authorization headers (never present at the
  `core::StreamEvent` boundary — they live in the wire layer)
- User prompts that may contain sensitive information (replaced with
  `<redacted>` placeholder, configurable)
- Tool results that may contain secrets (configurable redaction rules)

### Detailed design

TBD — pending codebase research.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Recorder core | Event tap + JSONL writer + redaction | proposed | A recorded fixture replays through StreamReplayHarness with zero edits |
| B | `loom record` command | CLI integration + session lifecycle | proposed | `loom record` produces a valid fixture from a live session |
| C | Trace conversion | `loom record <session-id>` from existing traces | proposed | A fixture from `messages.jsonl` + `dump-prompts/` replays correctly |

## Production Readiness Review

TBD — to be filled before `implementable` gate.

## Rollout and rollback

- **Rollout**: additive — new command + new recorder module. No production
  behavior change when recording is disabled.
- **Rollback**: delete the new files. No data migration, no config change.

## Drawbacks

- Recorded fixtures may contain user content that should not be committed to
  version control. Mitigation: redaction + `.gitignore` for recorded
  fixtures by default.
- The recorder adds a small amount of complexity to the event pipeline.
  Mitigation: zero-overhead when disabled (compile-time or runtime flag).

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

- New unit tests for the recorder (event tap, JSONL writer, redaction)
- Integration test: record a session → replay the fixture → compare screens
- Expected ctest total: TBD
- Dual preset `-Werror`; serial ctest

## Documentation impact

- [ ] `CLAUDE.md` — add `loom record` to debug traces section
- [ ] `docs/dev/` — add a guide on recording and replaying sessions
- [ ] `docs/decisions/design-decisions.md` — record the recording boundary
      decision

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| Should recording be a compile-time flag or runtime toggle? | TBD | Phase A design |
| How to handle multi-turn sessions in a single fixture? | TBD | Phase A design |
| Should recorded fixtures be auto-committed or gitignored? | TBD | Phase B design |

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-07 | — | RFC opened (provisional) |  | — |
