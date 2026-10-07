---
rfc: 5
title: Property-Based Fuzz Testing of Streaming Event Sequences
status: implementable
owners: "@Zzzode"
reviewers:
  - agent:design-review-r1 (request-changes: 12 findings, 3 high)
  - agent:design-review-r2 (request-changes: 7 findings, 2 medium)
  - agent:design-review-r3 (approve: all findings addressed)
  - agent:prr-review (approve: 7 findings addressed, play_with_split_invariants contradiction fixed)
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
EventSequenceGenerator (rapidcheck)
  ├── generates: StreamStart → [ContentBlockStart → Delta* → Stop]* → StreamEnd
  ├── respects: DedupTracker-accepted transitions (including out-of-order)
  ├── configurable: max blocks, max deltas, error injection, tool calls,
  │                 out-of-order probability, multi-commit sequences
  ├── text lengths: mix of short + 40–500 chars (exercises INV-02/03/04/05)
  ├── id correlation: ToolUseBlock.id ↔ ToolExecutionStart/End.tool_use_id
  └── seeded: deterministic with fixed seed (ctest), random (standalone)

StreamReplayHarness (existing, RFC 0003)
  ├── injects generated events into AppAdapter
  ├── simulates __commit__ and __end_query__
  ├── fresh App per iteration (avoids seen_tool_use_ids_ leakage)
  └── captures screens at checkpoints only (not per-step)

InvariantChecker (existing, RFC 0003)
  ├── INV-01: no crash
  ├── INV-02: thinking not truncated (needs ≥50-char thinking text)
  ├── INV-03: no duplicate content (≤2 occurrences, needs ≥40-char text)
  ├── INV-04: streaming text cleared after end_query (needs ≥40-char text)
  ├── INV-05: no empty tool blocks
  ├── INV-06: tool result before assistant text (needs multi-commit)
  └── INV-07: collapsed label after grace expiry (skipped — clock-dependent)

State-level invariants (new, via streaming_state_snapshot_for_testing())
  ├── streaming_tools_[i].complete transitions at most once
  ├── streaming_text_ never contains text from a tool/thinking index
  └── streaming_thinking_[i].complete transitions at most once

rapidcheck built-in shrinking
  ├── automatically shrinks failing sequences
  └── produces minimal reproducing input
```

### Event sequence generator

The generator produces sequences that the `DedupTracker` accepts — this
includes both well-formed sequences and out-of-order transitions that the
tracker tolerates. The key insight is that "valid" means "accepted by
DedupTracker", not "well-formed lifecycle": the tracker accepts
delta-before-start, stop-without-start, and post-error events, and these
are exactly the transitions that trigger the semantic quirks listed below.

**Well-formed sequence (the common case):**
1. `StreamStart` (always first)
2. Zero or more content blocks, each:
   - `ContentBlockStart` (with a valid `ContentBlock` variant)
   - Zero or more `ContentBlockDelta`
   - `ContentBlockStop`
3. Optionally: `ToolExecutionStart` / `ToolExecutionEnd` (for tool_use blocks)
4. Optionally: `StreamError` (mid-stream error, terminal)
5. `StreamEnd` (always last, unless error)
6. `__commit__` (simulated engine commit with the assembled message)
7. `__end_query__`

**Out-of-order transitions (generator knobs, default 10% probability each):**
- `delta_before_start`: emit a `ContentBlockDelta` for an index that has not
  seen a `ContentBlockStart` yet (DedupTracker promotes to Open and accepts)
- `stop_without_start`: emit a `ContentBlockStop` for an index that has not
  seen a `ContentBlockStart` (accepted; index marked Stopped, poisoning
  subsequent Starts)
- `post_error_events`: emit 0–3 events after `StreamError` (the handler sets
  `pending_error_` but does not stop processing)
- `duplicate_stream_start`: emit a second `StreamStart` mid-sequence
  (simulates reconnect; `clear_indices()` resets block indices but
  `seen_tool_use_ids_` persists)

**Multi-commit sequences:**
The generator can produce 1–3 query rounds in a single sequence. Each round
has its own `StreamStart` → blocks → `StreamEnd` → `__commit__` →
`__end_query__`. This exercises INV-06 (tool result committed in an earlier
round than assistant text). The generator inserts `__checkpoint__` steps
after each `__commit__` and after each `__end_query__` so that the
`screen_check` callback fires at the right moments.

**Text length distribution:**
- 50% short (1–39 chars) — exercises normal rendering
- 30% medium (40–99 chars) — exercises INV-03/04/05 thresholds
- 20% long (100–500 chars) — exercises INV-02 (≥50-char thinking) and
  wrap/truncation paths

**Tool-use-id correlation:**
When a `ToolUseBlock` is generated, its `id` field is used as the
`tool_use_id` in the corresponding `ToolExecutionStart` and
`ToolExecutionEnd` events. Without this correlation, the handler's id-match
loop silently no-ops and tool invariants never fire.

Generator knobs:
- `max_blocks` (default 5)
- `max_deltas_per_block` (default 10)
- `error_probability` (default 0.05)
- `tool_call_probability` (default 0.3)
- `thinking_probability` (default 0.3)
- `out_of_order_probability` (default 0.10)
- `max_rounds` (default 3, for multi-commit sequences)

### Shrinking

rapidcheck provides built-in shrinking: when a property fails, it
automatically reduces the generated input to the minimal reproducing case.
This is a key reason for choosing rapidcheck over a custom generator —
shrinking structured event sequences (removing events, simplifying text,
shortening sequences) is handled by the library, not by hand-written delta
debugging.

The shrunk result is saved as a JSONL fixture (see "Findings serialization"
below) for manual review and commit.

### Integration

Two modes:

1. **ctest mode** (`test_streaming_fuzz`): a GTest test that runs a fixed
   number of iterations (default 100) with a fixed seed. Uses
   `RC_GTEST_PROP` for rapidcheck/GTest integration. Fast, deterministic,
   runs in CI.

2. **Standalone mode** (`loom-fuzz`): a plain `main()` rapidcheck driver
   (no libFuzzer, no sanitizer) that runs indefinitely (or until
   `--max-iterations`) with a random seed. For local exploration. Findings
   are written to `tests/fixtures/streaming_sessions/fuzz_<hash>.jsonl`.

The standalone mode is a simple `main()` that configures `rc::Config`
(seed, max iterations) and calls the same property function as the ctest
mode. It does **not** use `-fsanitize=fuzzer` — libFuzzer supplies its own
`main()` and drives byte-level mutations, which is incompatible with
rapidcheck's structured generators. Coverage-guided fuzzing (libFuzzer)
is deferred to a future enhancement (see Alternative A3).

### Detailed design

#### Library: rapidcheck

[rapidcheck](https://github.com/emil-e/rapidcheck) is the chosen property-based
testing library. Rationale:
- Actively maintained (last push 2026-10-05), BSD-2-Clause license
- C++23-compatible, FetchContent-friendly
- Built-in shrinking (the key differentiator from raw fuzzing)
- GTest integration (`rapidcheck_gtest`, `RC_GTEST_PROP`) — the project
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
set(RC_ENABLE_GTEST ON CACHE INTERNAL "")  # enables rapidcheck_gtest target
# ... wrap in CMAKE_CXX_SCAN_FOR_MODULES OFF + CMAKE_COMPILE_WARNING_AS_ERROR OFF
FetchContent_MakeAvailable(rapidcheck)
```

The GTest integration target is `rapidcheck_gtest` (not `rapidcheck::gtest`),
which provides `RC_GTEST_PROP`. Add `rapidcheck` to the `.deps-cache`
mechanism in the root `CMakeLists.txt` for offline/restricted-network builds.

#### Generator design

The generator produces `StreamEvent` sequences directly in C++ (bypassing
the JSONL parsing layer). It respects the `DedupTracker` state machine
(`src/ui/messages/message_pipeline.cppm` lines 98–193), including
out-of-order transitions that the tracker accepts:

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
- `ToolExecutionProgress`: no dedup, always accepted (but never emitted by
  the live engine — only in hand-authored fixtures)

**Reset semantics:**
- `StreamStart` calls `clear_indices()` (not `clear()`) — block indices
  reset per stream, tool-use-id dedup persists across streams within a turn

The generator uses rapidcheck's `rc::gen` combinators to produce sequences
that respect these rules. See "Event sequence generator" above for the
full sequence shape, out-of-order knobs, multi-commit support, text-length
distribution, and tool-use-id correlation.

#### State snapshot seam

State-level invariants need read access to `streaming_tools_`,
`streaming_thinking_`, and `streaming_text_`, which are private
(`src/ui/app/app.cppm:260-288`). The existing testing seams
(`app_testing_seams.cpp`) only inject/set/clear — there is no snapshot
accessor.

A new seam `streaming_state_snapshot_for_testing()` returns a struct with
the fields needed for state-level invariants. The struct is defined in
`app.cppm` (the module interface, alongside the existing `AppTestingSeams`
friend declaration) so the seam's return type is visible to importers:

```cpp
struct StreamingStateSnapshot {
    struct ToolEntry {
        std::string tool_use_id;
        std::string tool_name;
        bool complete;
        bool is_error;
    };
    struct ThinkingEntry {
        int index;
        bool complete;
    };
    std::vector<ToolEntry> tools;
    std::vector<ThinkingEntry> thinking;
    std::string streaming_text;
};
```

This seam is added to `AppTestingSeams` and used by the fuzzer's
state-level invariant checks. It is also useful for the existing replay
tests.

#### Performance: screen invariants at checkpoints only

`play_with_invariants` performs a full 120×40 FTXUI render after every
step when a checker is installed (`streaming_replay.hpp:649-658`). At
~60 steps/sequence × 100 iterations that is ~6,000 full renders — roughly
17× the per-step render count of the entire existing 27-fixture suite.

Merely skipping screen-invariant checks inside the callback does NOT
prevent the render — the harness renders unconditionally before invoking
the callback. A harness modification is required:

**New method `play_with_split_invariants`** (added to `StreamReplayHarness`):
takes two callbacks:
- `state_check` (per-step): called after every event for state-level
  invariants (via `streaming_state_snapshot_for_testing()`). Does NOT
  render the screen.
- `screen_check` (checkpoint-only): called only at `__checkpoint__` steps
  (after `__commit__` and after `__end_query__`). Renders the screen and
  runs screen-text invariants.

This reduces renders from ~6,000 to ~200 per 100-iteration run (2
checkpoints × 100 iterations), comparable to the existing fixture suite.

The existing `play_with_invariants` is unchanged — existing fixtures and
tests continue to use it.

#### Findings serialization

When a property fails, rapidcheck shrinks the input to the minimal
reproducing sequence. The fuzzer then serializes this sequence to JSONL
using a `StreamEvent → JSON` serializer that also handles `__commit__` and
`__end_query__` pseudo-events (mirroring the wire shape in
`streaming_replay.hpp:211-444`), writing to
`tests/fixtures/streaming_sessions/fuzz_<hash>.jsonl`.

This serializer is also used by RFC 0004's `loom record` command — the two
RFCs share the same serialization code. If RFC 0004 lands first, the fuzzer
reuses its serializer; if RFC 0005 lands first, the recorder reuses the
fuzzer's.

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
may or may not catch. The fuzzer additionally asserts **state-level
invariants** on the `StreamingStateSnapshot` (see "State snapshot seam"
above):
- Every `tools[i].complete` transition happens at most once
- `streaming_text` never contains text from a tool/thinking index
- Every `thinking[i].complete` transition happens at most once

#### Known-quirk policy for ctest mode

The state-level invariant "`streaming_text` never contains text from a
tool/thinking index" fires on quirk #1 (delta-before-start for tool/thinking
blocks), which the generator explicitly targets via the
`delta_before_start` knob. If ctest mode runs all invariants on
out-of-order sequences, the test fails on known quirks — not new bugs.

**ctest mode runs two test cases:**

1. **Well-formed sequences** (out-of-order knobs disabled:
   `out_of_order_probability = 0.0`): all invariants (INV-01–06 +
   state-level). This verifies the pipeline is correct on valid input.
   Clean pass expected.

2. **Out-of-order sequences** (all knobs at default): INV-01 (no crash)
   only. This verifies the pipeline doesn't crash on weird-but-accepted
   input. Clean pass expected (the known quirks are semantic, not crashes).
   State-level invariants are excluded because the `streaming_text`
   invariant fires on quirk #1 (delta-before-start for tool/thinking
   blocks); the `complete`-transition invariants are also excluded for
   consistency — the out-of-order case verifies crash-freedom only, while
   standalone mode runs the full invariant suite with known-quirk filtering.

**Standalone mode** runs all invariants on all sequences. Known quirks
(documented above) are filtered from findings — only previously-unknown
invariant failures are reported. This prevents re-finding the four known
quirks on every run while still catching NEW instances (e.g., a new type
of misclassification not covered by the known-quirk list).

#### Integration with existing infrastructure

The fuzzer reuses:
- **`StreamReplayHarness`** (`tests/streaming_replay.hpp`): injects events
  via `test_seams(&app).inject_stream_event_for_testing(ev)`, manages
  `query_running_`, simulates `__commit__` and `__end_query__`
- **`InvariantChecker`** (`tests/invariant_checker.hpp`): INV-01 through
  INV-07, checked at checkpoints (not per-step — see "Performance" above)
- **`AppTestingSeams`** (`src/ui/app/app_testing_seams.cpp`): provides
  `inject_stream_event_for_testing()`, `set_query_running_for_testing()`,
  `clear_streaming_thinking_for_testing()`, and the new
  `streaming_state_snapshot_for_testing()`

**Fresh App per iteration**: the harness never calls `event_dedup_.clear()`
between plays (only `StreamStart`'s `clear_indices()` runs, which preserves
`seen_tool_use_ids_`). Reusing one `App` across fuzz iterations would leak
tool-use-id dedup state. The fuzzer constructs a fresh `App` (and fresh
`StreamReplayHarness`) for each iteration.

The fuzzer generates `StreamEvent` values, wraps them in `ReplayStep`
vectors, and plays them through
`StreamReplayHarness::play_with_split_invariants()` (the new method with
per-step state check and checkpoint-only screen check — see "Performance"
above).

#### Build system integration

Two targets:

1. **`test_streaming_fuzz`** (ctest mode): a GTest test that runs a fixed
   number of iterations (default 100) with a fixed seed. Uses
   `RC_GTEST_PROP` for rapidcheck/GTest integration. Registered via
   `gtest_discover_tests` (matching the project's existing GTest pattern,
   e.g., `tests/CMakeLists.txt:266`). Deterministic, runs in CI.

2. **`loom-fuzz`** (standalone mode): a plain `main()` rapidcheck driver
   (no `-fsanitize=fuzzer`, no libFuzzer) that runs indefinitely (or until
   `--max-iterations`) with a random seed. Configured via `rc::Config`
   (seed, max iterations, test timeout). For local exploration. Findings
   are written to `tests/fixtures/streaming_sessions/fuzz_<hash>.jsonl`.
   Built in CI but **not** registered as a ctest — only
   `test_streaming_fuzz` is a registered ctest. `loom-fuzz` is invoked
   manually for long-running exploration.

Both targets link `loom_core`, `loom_ui`, `rapidcheck`, `rapidcheck_gtest`,
and GTest (same as `test_streaming_replay`). The standalone target does
**not** link `-fsanitize=fuzzer` — libFuzzer supplies its own `main()` and
drives byte-level mutations, which is incompatible with rapidcheck's
structured generators.

**Relationship to existing fuzz infrastructure**: the project already has
`tests/fuzz/CMakeLists.txt` with two `-fsanitize=fuzzer` targets gated on
`ENABLE_FUZZING` (`tests/CMakeLists.txt:691-695`). Those targets do
byte-level mutation of wire-protocol inputs — a different engine and a
different purpose. The new `loom-fuzz` is a structured-generator driver
for event-sequence space; it is always built (not gated on `ENABLE_FUZZING`)
because it has no sanitizer dependency and runs as a normal ctest in CI.

#### ctest integration

```cmake
# tests/CMakeLists.txt
add_executable(test_streaming_fuzz test_streaming_fuzz.cpp)
target_link_libraries(test_streaming_fuzz PRIVATE
    loom_core loom_ui GTest::gtest loom_test_main
    rapidcheck rapidcheck_gtest)
gtest_discover_tests(test_streaming_fuzz)
```

The fixed seed is hardcoded in the test (via `rc::Config::seed`) so CI
runs are reproducible. The iteration count is set via `rc::Config::maxSuccess`
(default 100).

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | rapidcheck integration + generator + serializer | FetchContent (with `RC_ENABLE_GTEST=ON`) + StreamEvent generator respecting DedupTracker (including out-of-order) + StreamEvent→JSONL serializer + `streaming_state_snapshot_for_testing()` seam | proposed | Generator produces 1000 sequences that replay without harness errors; serializer round-trips through `streaming_replay.hpp` parser |
| B | Invariant integration | Wire generator → StreamReplayHarness → InvariantChecker (at checkpoints) + state-level invariants (via snapshot seam) + fresh App per iteration | proposed | 1000 generated sequences pass INV-01–06 + state invariants; any failure is shrunk and serialized to JSONL |
| C | ctest + standalone integration | `test_streaming_fuzz` target (gtest_discover_tests, fixed seed, 100 iterations, two test cases: well-formed + out-of-order) + `loom-fuzz` plain main() driver (rc::Config, random seed) | proposed | `ctest -R StreamingFuzz` passes (fixed seed, 100 iterations, both test cases); `loom-fuzz --max-iterations 10000` runs clean; ctest time measured and within budget |
| D | Bug hunt | Long-running fuzz session + triage findings | proposed | ≥1M iterations run; any findings triaged, shrunk, and filed as fixtures (or documented as known quirks) |

## Production Readiness Review

### 1. Correctness and tests

- [x] Every new behaviour has a new test (unit / module / e2e named).
  - Two fuzz properties: well-formed sequences (all invariants) and
    out-of-order sequences (INV-01 only)
  - Serializer round-trip test: StreamEvent → JSON → parse → compare
  - Generator validation: 1000 sequences replay without harness errors
  - State snapshot seam tests
- [x] Failure paths covered (empty input, error result, timeout, abort).
  - Empty sequence (StreamStart → StreamEnd only): valid, no crash
  - StreamError mid-sequence: error handled, subsequent events processed
  - rapidcheck shrinking: failing input reduced to minimal reproducer
- [x] Golden suites assessed: `test_ui_*`, `test_dialog_*`,
  `test_prompt_dialog`, `test_cost_threshold_dialog`; if rendering
  changes, goldens regenerated and **manually reviewed** (not blindly
  accepted).
  - No rendering changes — the fuzzer is test-only, reuses the existing
    replay harness and screen capture. No production UI code is modified
    (only the new `streaming_state_snapshot_for_testing()` seam, which is
    test-only).
- [x] Known timing flake list respected; no new wall-clock assertions with
  tight upper bounds under scheduler contention.
  - Fixed seed in ctest mode for determinism. No wall-clock assertions.
- [x] Expected serial ctest total stated; deletions reconcile exactly.
  - ~2020 (2014 existing + ~6 new: 2 fuzz properties + serializer
    round-trip + generator validation + state snapshot seam tests)
- [x] String/shape-based cross-module couplings (registry keys, tag formats,
  wire fields) changed on BOTH emitter and consumer sides;
  `docs/decisions/design-decisions.md` consulted.
  - The fixture format is the coupling: the serializer (emitter) must match
    the parser in `streaming_replay.hpp` (consumer). The round-trip test
    guards this. `design-decisions.md` consulted — no existing tag formats
    or registry keys are touched.

**Notes:** The fuzzer adds one new test-only seam
(`streaming_state_snapshot_for_testing()`) to `AppTestingSeams`. This is
additive — existing tests and production code are unaffected. The
`play_with_split_invariants` method is a new addition to
`StreamReplayHarness`, not a modification of existing methods.

### 2. Build system and module discipline

- [x] Affected producer-TU BMI PSS measured before AND after (PSS from
  `/proc/<pid>/smaps_rollup`, not RSS), numbers recorded.
  - Test-only changes — no production BMI impact. The new seam is a
    declaration in `app_testing_seams.cpp` (implementation file, not
    interface). rapidcheck is a test-only dependency.
- [x] No Ninja concurrency reduction anywhere; memory handled by TU
  splitting / type erasure.
  - No concurrency changes.
- [x] Interface files gain declarations, not definitions (RFC 0001 Phase C
  direction); god-interface inline-body count does not increase.
  - `streaming_state_snapshot_for_testing()` is a declaration in the
    existing `AppTestingSeams` pattern; implementation in
    `app_testing_seams.cpp`. No new inline bodies in interface files.
- [x] After RFC 0001 Phase A: no new textual standard-library or third-party
  includes in module units (`import std;` / `cc.third_party.*` used).
  - rapidcheck is used only in test files (`test_streaming_fuzz.cpp`),
    not in module units. Test files may use textual includes (they are
    not module units).
- [x] Named partitions used only for PIMPL internals, not for cross-area
  layering.
  - No named partitions.
- [x] No new upward dependency edges; directory-level SCCs do not grow
  (architecture graph lint result attached).
  - No new production dependencies. Test-only dependency on rapidcheck
    does not affect the module graph. Graph lint to be run during Phase A.

**Notes:** rapidcheck is added via FetchContent with `RC_ENABLE_GTEST=ON`,
`RC_ENABLE_TESTS=OFF`, `RC_ENABLE_EXAMPLES=OFF`. Added to `.deps-cache`
for offline builds.

### 3. Rollback and compatibility

- [x] Commits are atomic per phase; each phase revertible independently.
  - Phase A (rapidcheck + generator + serializer + seam) is independently
    revertible. Phase B (invariant integration) depends on Phase A.
    Phase C (ctest + standalone) depends on Phase B. Phase D (bug hunt)
    is operational, not code.
- [x] No flag-day interface changes: PIMPL / type erasure / re-export shims
  keep importers building during migration.
  - `streaming_state_snapshot_for_testing()` is additive — existing
    importers of `AppTestingSeams` build without modification.
- [x] Persisted data compatibility considered (`~/.loom/sessions`,
  settings cascade, config schema) with migration or read-tolerance.
  - Fuzz findings written to `tests/fixtures/streaming_sessions/` (test
    fixtures, not user data). No persisted data changes.
- [x] Wire-protocol compatibility (`wire_anthropic` / `wire_openai`) —
  field additions are additive; removals justified.
  - No wire protocol changes. The fuzzer operates at the
    `core::StreamEvent` boundary, after wire parsing.

**Notes:**

### 4. Observability

- [x] Session traces remain valid: `messages.jsonl` block coverage and
  `dump-prompts/<id>.jsonl` request/response dumps.
  - Unaffected — the fuzzer is test-only, does not modify trace
    persistence.
- [x] New background workers / threads are event-driven; no constant-rate
  render ticker (UI rule); teardown/abort paths defined.
  - No new threads. The fuzzer runs synchronously in the test process.
- [x] New diagnostics log through the existing debug channels; no new ad-hoc
  print paths.
  - Fuzz findings are written as JSONL fixtures (the diagnostic output).
    rapidcheck's built-in reporting goes to stdout (test output), not
    ad-hoc print paths.
- [x] Build/performance metrics from this PRR recorded in the RFC.
  - ctest time for `test_streaming_fuzz` to be measured during Phase C.
    Target: <5s total (2 test cases × 100 iterations, with 2 checkpoint
    renders per iteration = ~400 full renders + ~12,000 event injections).

**Notes:**

### 5. Documentation and deletion

- [x] `CLAUDE.md` updated if conventions, build layout, or paths change.
  - `CLAUDE.md` testing section updated to mention fuzz testing.
- [x] Non-obvious decisions added to `docs/decisions/design-decisions.md`.
  - The generator-vs-mutation decision, the known-quirk policy, and the
    checkpoint-only rendering optimization are non-obvious and will be
    recorded.
- [x] Dead code made obsolete by the work is deleted in the SAME phase
  - No dead code — this is a new feature, not a replacement.
- [x] Deprecated modules/shapes have a stated removal trigger and migration
  note; no silent shape drift.
  - No deprecations.
- [x] All comments/docs in English.
  - Yes.

**Notes:**

### 6. Platform readiness (macos-14 / Linux)

- [x] Debug + release presets build `-Werror` clean on the Linux dev box.
  - To be verified during Phase A.
- [x] macos-14 CI green at default parallelism (3 vCPU / 14 GB); no swap.
  - To be verified during Phase A. rapidcheck is platform-independent.
- [x] Termios / signals / Apple-only guards correct for both platforms.
  - No platform-specific code.
- [x] truecolor (`COLORTERM=truecolor TERM=xterm-256color`) golden path
  intact.
  - No rendering changes.
- [x] Offline dependency cache unaffected; no new network fetch required.
  - rapidcheck added to `.deps-cache` mechanism. FetchContent clones from
    GitHub when cache is absent (same pattern as GoogleTest).

**Notes:**

### Review sign-off

| Role | Reviewer | Date | Verdict |
|---|---|---|---|
| Design | agent:design-review-r1/r2/r3 | 2026-10-07 | approve |
| Production readiness | agent:prr-review | 2026-10-07 | approve |
| Code (per phase) | | | pending |

## Rollout and rollback

- **Rollout**: additive — new test target + new fuzz binary. No production
  behavior change.
- **Rollback**: delete the new files. No data migration, no config change.

## Drawbacks

- Fuzz tests are non-deterministic by nature. Mitigation: fixed seed in
  ctest mode; findings are saved as deterministic fixtures.
- The generator may produce sequences that are technically valid but
  unrealistic. Mitigation: generator knobs bias toward realistic patterns
  (thinking → tool_use → text, etc.) while also exploring out-of-order
  transitions that trigger known quirks.
- rapidcheck adds a dependency. Mitigation: FetchContent with pinned commit,
  disabled tests/examples, and `.deps-cache` support for offline builds.

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

### A4. Custom generator (no library)

**Pros**: no dependency; full control over generator behavior.
**Cons**: no built-in shrinking (the key differentiator from raw fuzzing);
must implement generator combinators, shrinking, and seed management by
hand. **Rejected** — rapidcheck provides all of these out of the box, is
actively maintained, and integrates with the project's existing GTest
setup.

## Testing and verification plan

- New test target `test_streaming_fuzz` with fixed-seed deterministic runs
- Standalone `loom-fuzz` binary for long-running exploration
- Deliberately inject a bug (e.g., revert one of the three original fixes)
  and verify the fuzzer finds and shrinks it
- Expected ctest total: ~2020 (2014 existing + ~6 new: 2 fuzz properties
  + serializer round-trip + generator validation + state snapshot seam tests)
- Dual preset `-Werror`; serial ctest
- ctest time budget measured in Phase C; iteration count adjusted if needed

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
| What iteration count for ctest mode? | @Zzzode | 100 iterations with fixed seed via `rc::Config::maxSuccess`; standalone mode runs until `--max-iterations` or indefinitely |
| Should the fuzzer generate invalid sequences too? | @Zzzode | The generator produces DedupTracker-*accepted* sequences, including out-of-order transitions (delta-before-start, stop-without-start, post-error) that trigger the known quirks. Sequences the tracker *rejects* (e.g., start-after-stop) are not generated — they are dropped by the tracker and cannot reach the handler. |

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-07 | — | RFC opened (provisional) |  | — |
| 2026-10-07 | — | Design review R1: request-changes (12 findings, 3 high) |  | Engine incompatibility, generator reach, state read path |
| 2026-10-07 | — | Design review R2: request-changes (7 findings, 2 medium) |  | Harness render behavior, invariant/quirk contradiction |
| 2026-10-07 | — | Design review R3: approve → status accepted |  | All 19 findings addressed across 2 rounds |
| 2026-10-07 | — | PRR review: approve → status implementable |  | 7 findings addressed; play_with_split_invariants contradiction fixed, checkpoint generation specified |
