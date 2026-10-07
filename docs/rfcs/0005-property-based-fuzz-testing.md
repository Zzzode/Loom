---
rfc: 5
title: Property-Based Fuzz Testing of Streaming Event Sequences
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-10-07
last-reviewed: 2026-10-07
tracking: local
---

# RFC 0005 — Property-Based Fuzz Testing of Streaming Event Sequences

## Summary

Add a property-based fuzz testing system that generates random valid
streaming event sequences, injects them through the RFC 0003 replay harness,
and asserts the RFC 0003 invariant catalog (INV-01 through INV-07) holds.
This complements the hand-authored fixture library: fixtures target known
bugs and specific scenarios, while fuzzing explores the unbounded space of
valid event sequences to find unknown edge cases.

## Motivation

RFC 0003 established 27 hand-authored fixtures and 7 invariants. Hand-
authored fixtures are precise but finite — they cover known scenarios, not
unknown ones. The streaming event pipeline has a large state space: event
ordering, block index reuse, interleaved thinking/text/tool blocks, error
events mid-stream, and duplicate events are all valid inputs that could
trigger undefined behavior or UI glitches. The invariant checker (INV-01
through INV-07) is the perfect oracle for fuzzing: it already encodes the
properties that must hold on every valid event sequence.

### Evidence

| Metric | Current | Target | How measured |
|---|---|---|---|
| Fixture coverage | 27 hand-authored scenarios | 10,000+ generated sequences | Fuzzer iterations |
| Event sequence space | Unexplored beyond known cases | Crashes/assertion failures found | Fuzzer findings |
| Invariant coverage | INV-01–07 on 27 fixtures | INV-01–07 on every generated sequence | Invariant checks per run |

## Goals

- G1. A generator that produces random valid `core::StreamEvent` sequences
  (respecting the state machine: StreamStart → blocks → StreamEnd).
- G2. Generated sequences are replayable through `StreamReplayHarness`
  without modification.
- G3. The invariant checker (INV-01 through INV-07) runs on every generated
  sequence; any invariant failure is a fuzzer finding.
- G4. Fuzzer findings are automatically minimized (shrunk) to the smallest
  reproducing sequence.
- G5. Fuzz tests run as part of `ctest` with a fixed seed for reproducibility,
  and as a standalone fuzzer binary for long-running exploration.

## Non-Goals

- N1. Fuzzing the wire/HTTP layer. The fuzzer starts at the
  `core::StreamEvent` boundary (same as RFC 0003).
- N2. Fuzzing the engine loop (tool execution, retries, compaction). The
  fuzzer bypasses the engine, same as the replay harness.
- N3. Mutation-based fuzzing of existing fixtures. The fuzzer generates
  sequences from scratch (generator-based), not by mutating existing
  fixtures.
- N4. Coverage-guided fuzzing (libFuzzer-style). Generator-based fuzzing
  with shrinking is sufficient for the event sequence space; coverage
  guidance is a future enhancement.

## Proposal

### Architecture

```
EventSequenceGenerator
  ├── generates: StreamStart → [ContentBlockStart → Delta* → Stop]* → StreamEnd
  ├── respects: block index ordering, valid block types, state machine
  ├── configurable: max blocks, max deltas, error injection, tool calls
  └── seeded: deterministic with --seed, random by default

StreamReplayHarness (existing, RFC 0003)
  ├── injects generated events into AppAdapter
  ├── simulates __commit__ and __end_query__
  └── captures screens at checkpoints

InvariantChecker (existing, RFC 0003)
  ├── INV-01: no crash
  ├── INV-02: thinking not truncated
  ├── INV-03: no duplicate content (≤2 occurrences)
  ├── INV-04: streaming text cleared after end_query
  ├── INV-05: no empty tool blocks
  ├── INV-06: tool result before assistant text
  └── INV-07: collapsed label after grace expiry (with clock seam)

Shrinker
  ├── takes a failing sequence
  ├── removes events one at a time (delta debugging)
  └── produces the minimal reproducing sequence
```

### Event sequence generator

The generator produces sequences that respect the streaming state machine:

1. `StreamStart` (always first)
2. Zero or more content blocks, each:
   - `ContentBlockStart` (with a valid `ContentBlock` variant)
   - Zero or more `ContentBlockDelta`
   - `ContentBlockStop`
3. Optionally: `ToolExecutionStart` / `ToolExecutionProgress` /
   `ToolExecutionEnd` (for tool_use blocks)
4. Optionally: `StreamError` (mid-stream error, terminal)
5. `StreamEnd` (always last, unless error)
6. `__commit__` (simulated engine commit with the assembled message)
7. `__end_query__`

Generator knobs:
- `max_blocks` (default 5)
- `max_deltas_per_block` (default 10)
- `error_probability` (default 0.05)
- `tool_call_probability` (default 0.3)
- `thinking_probability` (default 0.3)

### Shrinking

When an invariant fails, the shrinker reduces the sequence:
1. Remove events one at a time (binary search / delta debugging)
2. Simplify block content (shorten text, remove thinking)
3. Remove entire blocks
4. The result is the minimal sequence that still triggers the failure

### Integration

Two modes:

1. **ctest mode** (`test_streaming_fuzz`): runs a fixed number of iterations
   (default 100) with a fixed seed. Fast (<1s), deterministic, runs in CI.
2. **Standalone mode** (`loom-fuzz`): runs indefinitely (or until
   `--max-iterations`) with a random seed. For local exploration. Findings
   are written to `tests/fixtures/streaming_sessions/fuzz_<hash>.jsonl`.

### Detailed design

#### Library: rapidcheck

[rapidcheck](https://github.com/emil-e/rapidcheck) is the chosen property-based
testing library. Rationale:
- Actively maintained (last push 2026-10-05), BSD-2-Clause license
- C++23-compatible, FetchContent-friendly
- Built-in shrinking (the key differentiator from raw fuzzing)
- GTest integration (`rapidcheck::gtest`, `RC_GTEST_PROP`) — the project
  already uses GTest
- Rich generator combinators for building structured event sequences

Added via FetchContent in `tests/CMakeLists.txt`, following the GoogleTest
pattern (disable module scanning and `-Werror` around `MakeAvailable`):

```cmake
FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG        <pin-a-commit>
    GIT_SHALLOW    TRUE
)
set(RC_ENABLE_TESTS OFF CACHE INTERNAL "")
set(RC_ENABLE_EXAMPLES OFF CACHE INTERNAL "")
# ... wrap in CMAKE_CXX_SCAN_FOR_MODULES OFF + CMAKE_COMPILE_WARNING_AS_ERROR OFF
FetchContent_MakeAvailable(rapidcheck)
```

#### Generator design

The generator produces `StreamEvent` sequences directly in C++ (bypassing
the JSONL parsing layer). It respects the `DedupTracker` state machine
(`src/ui/messages/message_pipeline.cppm` lines 98–193):

**Per-block-index lifecycle:**
```
NotSeen ──Start──► Open ──Delta*──► Stopped (terminal)
```

**Transition rules (from DedupTracker):**
- `should_accept_start(idx)`: NotSeen → Open (accept); Open → accept
  ("bogus but harmless"); Stopped → DROP
- `should_accept_delta(idx)`: NotSeen → Open (accept, out-of-order);
  Open → accept; Stopped → reject
- `should_accept_stop(idx)`: NotSeen → Stopped (accept); Open → Stopped
  (accept); Stopped → DROP (idempotent)

**Per-tool-use-id lifecycle:**
- `should_accept_exec_start(id)`: first mention → accept; else drop
- `should_accept_exec_end(id)`: first `"end:<id>"` → accept; else drop
- `ToolExecutionProgress`: no dedup, always accepted

**Reset semantics:**
- `StreamStart` calls `clear_indices()` (not `clear()`) — block indices
  reset per stream, tool-use-id dedup persists across streams within a turn

The generator uses rapidcheck's `rc::gen` combinators to produce sequences
that respect these rules. Generator knobs:
- `max_blocks` (default 5)
- `max_deltas_per_block` (default 10)
- `error_probability` (default 0.05)
- `tool_call_probability` (default 0.3)
- `thinking_probability` (default 0.3)

#### Known semantic quirks (fuzz targets)

The research identified several crash-safe but semantically-wrong behaviors
that the fuzzer should target:

1. **Delta before Start**: `DedupTracker` promotes the index to Open and
   accepts, but the delta handler falls through to `streaming_text_` (no
   tool/thinking entry exists). A tool/thinking delta arriving before its
   Start is misclassified as plain text.
2. **Stop without Start**: accepted; index marked Stopped. Subsequent Starts
   are dropped; subsequent Deltas are rejected.
3. **ImageBlock/DocumentBlock/ToolResultBlock deltas**: ContentBlockStart
   creates no entry for these types, so their deltas are misrouted to
   `streaming_text_`.
4. **StreamError**: sets `pending_error_` but does not stop processing of
   subsequent events.

These are not crashes — they are semantic bugs that the invariant checker
may or may not catch. The fuzzer should additionally assert **state-level
invariants** directly on `AppAdapter`:
- Every `streaming_tools_[i].complete` transition happens at most once
- `streaming_text_` never contains text from a tool/thinking index
- `streaming_thinking_[i].complete` transitions at most once

#### Integration with existing infrastructure

The fuzzer reuses:
- **`StreamReplayHarness`** (`tests/streaming_replay.hpp`): injects events
  via `test_seams(&app).inject_stream_event_for_testing(ev)`, manages
  `query_running_`, simulates `__commit__` and `__end_query__`
- **`InvariantChecker`** (`tests/invariant_checker.hpp`): INV-01 through
  INV-07, checked after every event
- **`AppTestingSeams`** (`src/ui/app/app_testing_seams.cpp`): provides
  `inject_stream_event_for_testing()`, `set_query_running_for_testing()`,
  `clear_streaming_thinking_for_testing()`

The fuzzer generates `StreamEvent` values, wraps them in `ReplayStep`
vectors, and plays them through `StreamReplayHarness::play_with_invariants()`.

#### Build system integration

Two targets:

1. **`test_streaming_fuzz`** (ctest mode): a GTest test that runs a fixed
   number of iterations (default 100) with a fixed seed. Uses
   `RC_GTEST_PROP` for rapidcheck/GTest integration. Fast (<1s),
   deterministic, runs in CI.

2. **`loom-fuzz`** (standalone mode): a libFuzzer-compatible binary that
   runs indefinitely (or until `--max-iterations`) with a random seed.
   For local exploration. Findings are written to
   `tests/fixtures/streaming_sessions/fuzz_<hash>.jsonl`.

Both targets link `loom_core`, `loom_ui`, `rapidcheck`, and GTest (same as
`test_streaming_replay`). The existing `tests/fuzz/CMakeLists.txt` pattern
(`-fsanitize=fuzzer`) is used for the standalone target.

#### ctest integration

```cmake
# tests/CMakeLists.txt
add_executable(test_streaming_fuzz test_streaming_fuzz.cpp)
target_link_libraries(test_streaming_fuzz PRIVATE
    loom_core loom_ui GTest::gtest loom_test_main rapidcheck)
add_test(NAME StreamingFuzz COMMAND test_streaming_fuzz)
```

The fixed seed is hardcoded (or via `--seed` flag) so CI runs are
reproducible. The iteration count is configurable via `--iterations`.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | rapidcheck integration + generator | FetchContent + StreamEvent generator respecting DedupTracker state machine | proposed | Generator produces 1000 valid sequences that replay without harness errors |
| B | Invariant integration | Wire generator → StreamReplayHarness → InvariantChecker + state-level invariants | proposed | 1000 generated sequences pass INV-01–07 + state invariants; any failure is shrunk and reported |
| C | ctest + standalone integration | test_streaming_fuzz target + loom-fuzz binary | proposed | `ctest -R StreamingFuzz` passes (fixed seed, 100 iterations); `loom-fuzz --max-iterations 10000` runs clean |
| D | Bug hunt | Long-running fuzz session + triage findings | proposed | At least one previously-unknown semantic bug found, shrunk, and filed as a fixture |

## Production Readiness Review

TBD — to be filled before `implementable` gate.

## Rollout and rollback

- **Rollout**: additive — new test target + new fuzz binary. No production
  behavior change.
- **Rollback**: delete the new files. No data migration, no config change.

## Drawbacks

- Fuzz tests are non-deterministic by nature. Mitigation: fixed seed in
  ctest mode; findings are saved as deterministic fixtures.
- The generator may produce sequences that are technically valid but
  unrealistic. Mitigation: generator knobs bias toward realistic patterns
  (thinking → tool_use → text, etc.).
- Shrinking adds complexity. Mitigation: start with a simple delta-debugging
  shrinker; sophisticated shrinking is a future enhancement.

## Alternatives considered

### A1. Hand-authored fixtures only (status quo)

**Pros**: precise, reviewable, deterministic.
**Cons**: finite coverage; cannot explore unknown edge cases. **Rejected** —
complements, not replaces, hand-authored fixtures.

### A2. Mutation-based fuzzing of existing fixtures

**Pros**: reuses existing fixtures as seeds; simpler generator.
**Cons**: mutations may produce invalid sequences (e.g., delta without
start); limited to neighborhoods of existing fixtures. **Rejected** —
generator-based fuzzing covers the full valid sequence space.

### A3. Coverage-guided fuzzing (libFuzzer)

**Pros**: finds edge cases guided by code coverage; mature tooling.
**Cons**: requires libFuzzer integration (clang-specific); the event
sequence space is better explored by a generator than by byte-level mutation;
shrinking is harder. **Deferred** — generator-based fuzzing is simpler and
more targeted; coverage guidance is a future enhancement.

### A4. rapidcheck (C++ property-based testing library)

**Pros**: mature, well-tested, built-in shrinking, CMake integration.
**Cons**: adds a dependency; may not perfectly fit the event sequence
generator model. **Considered** — evaluate in Phase A; if rapidcheck's
generator model fits, use it; otherwise, implement a custom generator.

## Testing and verification plan

- New test target `test_streaming_fuzz` with fixed-seed deterministic runs
- Standalone `loom-fuzz` binary for long-running exploration
- Deliberately inject a bug (e.g., revert one of the three original fixes)
  and verify the fuzzer finds and shrinks it
- Expected ctest total: TBD
- Dual preset `-Werror`; serial ctest

## Documentation impact

- [ ] `CLAUDE.md` — add fuzz testing to the testing section
- [ ] `docs/dev/` — add a guide on running the fuzzer and triaging findings
- [ ] `docs/decisions/design-decisions.md` — record the generator vs
      mutation decision

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| Use rapidcheck or a custom generator? | @Zzzode | rapidcheck — only actively-maintained, C++23-compatible, FetchContent-friendly option with built-in shrinking |
| How to handle INV-07 (clock-dependent) in fuzzing? | @Zzzode | Skip INV-07 in fuzzing (it requires clock manipulation); the clock-seam tests (ThinkingGraceExpiry, ThinkingCollapseGrace) already cover it |
| Should fuzz findings be auto-committed as fixtures? | @Zzzode | No — findings are written to `tests/fixtures/streaming_sessions/fuzz_<hash>.jsonl` for manual review and commit |
| What iteration count for ctest mode? | @Zzzode | 100 iterations with fixed seed (<1s, deterministic); standalone mode runs until `--max-iterations` or indefinitely |
| Should the fuzzer generate invalid sequences too? | @Zzzode | No — the generator respects the DedupTracker state machine; invalid-sequence handling is tested by the existing `duplicate_events` and `stream_interrupted` fixtures |

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-07 | — | RFC opened (provisional) |  | — |
