---
rfc: 18
title: Live Generation TPS Beside the Work Indicator
status: implemented
owners: "@Zzzode"
reviewers:
  - "agent:tps_review (independent design, PRR and implementation review)"
created: 2026-10-15
last-reviewed: 2026-10-15
tracking: local
---

# RFC 0018 — Live Generation TPS Beside the Work Indicator

## Summary

Append an explicitly estimated live output rate (`~42.1 tok/s`) to the existing
work indicator above the editor. Use foreground model content deltas only;
keep Tasks, Teams, editor, footer, and the work row's height and placement.
No wire fields, settings, persisted records, or token-estimator changes.

## Motivation

### Evidence

| Measurement | Current | Target |
|---|---|---|
| Rate fields consumed by `RenderSpinner` | 0 | One optional estimated rate |
| Live output counts in `ContentBlockDelta` | Text fragments only | Reuse the existing text estimator, label with `~` |
| Exact provider output usage | `StreamEnd`, after generation | Do not present estimates as provider counts |
| Above-editor work row height | One row | One row |
| Tasks/Teams composition | Separate activity rows | Identical order, budget and geometry |

The existing `/stats` throughput uses total input plus output divided by the
whole session duration. It is not a live model generation rate and is not reused.

## Goals

- G1. Show an estimated rate beside foreground generation, including text,
  thinking and model-generated tool arguments. Never count tool results,
  input/cache usage or transport chunk count as output tokens.
- G2. Hide before enough samples exist, at block/stream completion, tool
  execution, cancellation, errors, idle, and after two seconds without deltas.
- G3. Preserve Tasks/Teams/editor/footer positions at narrow and wide widths.
- G4. Keep pure rendering, existing theme tokens, the existing worker/UI mutex
  handoff, and event-driven refreshes. No recurring render ticker.

## Non-Goals

Exact model-specific tokenization, provider throughput claims, persisted
performance history, background-agent rates, new configuration, footer fields,
changes to wire events, tools, permissions, model retry policy or shared
`TokenEstimator` semantics. Approved brand assets remain untouched.

## Proposal

The owner is interactive UI: app measures worker stream progress and projects
an optional rate onto the existing task-view store; screens render it beside
the work label. This is inside the user-authorized UI behavior. Local closure
uses existing app storage and the screen store; engine/wire/prompt/footer
modules require no change. Internal declarations may change; no released API,
external configuration or persisted contract changes.

### Detailed design

- Keep a private app-owned staged generation-rate record under `result_mutex_`.
  Record block identity and activity time only after an accepted nonempty
  delta. At the existing UI consumption point under the same mutex, sample
  the accumulated text, thinking or tool-input buffer. Reuse `loom.model.token_budget`'s
  `TokenEstimator`; do not sum per-chunk estimates or copy response buffers.
- Sample at most once per 100ms. Estimate the accumulated current block,
  retaining a two-second window and its preceding baseline (at most 22
  timestamp/count samples).
  Divide retained-oldest to newest estimated-count growth by their monotonic
  elapsed seconds, as defined below.
  A first sample is a baseline; never divide by zero or count its already
  received contents as newly timed output. Clock rollback and block changes
  start a new baseline. Zero token growth remains hidden. Chunk-independence tests hold baseline and sampling
  instants fixed, since changing delivery timing legitimately changes a
  client-observed rate. Sustained generation scans the existing block buffer
  at most ten times per second, not on every network fragment.
- Reset on submission, stream start, a different block, current block stop,
  tool execution start, any non-tool stream error, stream end and cancellation.
  Cancellation latches measurement off under the mutex until a new submission;
  a late callback cannot re-enable it.
  Do not clear on a rejected duplicate or another block's stop. Interleaved
  blocks restart the sample rather than mixing unlike cumulative counts.
- UI consumption computes a fresh optional rate under the existing mutex;
  inactive outer queries cannot expose it. A private one-shot expiry deadline
  uses the existing deadline wake thread to refresh once after two seconds of
  silence. Each accepted delta extends expiry; no independent timer or ticker.
- Append `  ~{:.1f} tok/s` before any spinner tip, using the existing muted
  palette token without dim. No wrap, extra element row, Tasks reordering or budget change.
  Width-constrained existing layout may clip optional text on small terminals.
- The `~` is visible in every estimated rate; exact usage is not retroactively
  substituted after completion because the label is already hidden.

### State ownership, math, and transition contract

The app-owned staged record contains active block identity, last delta time,
sample history and a cancellation-suppressed latch, all guarded by
`result_mutex_`. Query callbacks never write `TaskViewStore`. Only UI-side
`ConsumePendingResult` samples the existing buffer under this mutex and assigns
`TaskViewStore::estimated_generation_tps`; staged invalidation/reset is consumed
at the same point. Extend the worker-to-UI handoff regression so accepted
worker events leave the screen store unchanged until the UI drains them.

The first UI sample records `(now, estimated accumulated count)` and displays
nothing. Subsequent samples require at least 100ms since the previous sample and a
new content delta since that sample.
Before appending, remove the oldest sample while the *second* oldest is at or
before `now - 2s`; this retains at most one preceding boundary baseline. Then
append and cap the entire deque, including that baseline, at 22 samples.
Compute `(newest.count - oldest.count) / seconds(newest.time - oldest.time)`.
Only show with at least two samples, positive elapsed time, strictly positive
count growth, and a finite result. Otherwise the optional value is empty.
Network arrival timing changes may alter a transient estimate: the claim is
cumulative-buffer/chunk-count independence at fixed baseline and sample times,
not exact provider throughput or transport-timing independence.

On user interruption, acquire the mutex and set the suppressed latch, clear
active/history/rate and clear the expiry atomic **before** abort/request_stop.
Late StreamStart/Delta cannot clear suppression; only a new HandleSubmit
clears the latch. Ordinary StreamEnd clears the current API round, not the
whole user turn. ToolExecutionStart clears the current active measurement
without latching suppression, so the next API round's StreamStart and deltas
can produce a fresh baseline. On every outer stream_query return, invalidate
the measurement under the mutex before publishing `query_running_=false`,
even when cancellation produced no StreamEnd. Accepted duplicate filtering
precedes observations/resets; an unrelated block stop does not clear the
current block. Unknown/inactive buffers cannot produce a rate.

Before updating last delta time for any accepted nonempty delta, compare now
with the previous last delta. On `now - previous_last_delta >= 2s`, or clock
rollback, clear active/history/cached rate and the expiry atomic first, then
record this delta as fresh activity. This applies even if an old expiry Custom
event has not yet reached the UI. UI sampling can only establish a new
baseline after that reset and cannot bridge the silent interval.

Every UI consumption validates `clock::steady_now()` against last delta + 2s
and clears expired state, including cached rate and the expiry atomic. The
existing real-clock deadline loop only posts Custom once at the production
expiry instant; it never calculates rate or accesses the plain screen store.
Fake-clock tests drain/render after advancing the injected clock and do not
rely on the real 50ms loop. New submission, block/round reset, cancel, expiry,
outer completion and teardown clear the atomic; no screen attached means no
real expiry wake is armed. Teardown joins the existing wake thread before
record destruction, preserving current worker lifetime ownership.

String/shape seams: the app-produced optional rate and screen's spinner
parameter must change together. Task/Team format strings, tool tags, stream
variants, fixtures' event vocabulary and persisted traces remain unchanged.
Truecolor golden impact is limited to live-generation status suffixes; cases
with no timed deltas remain byte-identical. Clock-pinned tests avoid flaky
speed snapshots. No new diagnostic prints or raw token contents are logged.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Indicator measurement and display | Existing app/screen owners; tests and docs | done | Fake-clock tests prove rate, chunk independence, hiding and reset paths; geometry checks preserve task/editor rows; debug/release native PTY checks show generation-only rate and lifecycle hiding; both builds and relevant UI tests pass; RFC and architecture lint pass |

## Production Readiness Review

- Correctness: deterministic clock tests exercise ASCII/non-ASCII buffers,
  chunk grouping, warmup, rollover, stale data, round/block resets, duplicate
  events, tool arguments versus execution, errors, completion and cancel.
- Layout/goldens: measure Tasks/Teams/editor rows with/without the optional
  suffix at 40/80/120 columns and short/multiline layouts. Inspect regenerated
  live-status goldens; preserve unrelated dirty branding/dialog changes.
- Build/module discipline: new bodies stay in existing implementation units;
  declarations and private storage only in interfaces. No new module/target,
  third-party include, dependency download, library split or concurrency cap.
  Run graph/inline lint. Linux `/proc` PSS and macOS CI measurements are not
  available on this macOS host and are reported as unverified, not invented.
- Compatibility: no settings, paths, serialization, wire, permission or
  cancellation semantics change; statistics are ephemeral UI state only.
- Observability/lifecycle: no new thread, periodic query render, print path or
  trace field. One-shot expiry is cleared during teardown/cancel/reset.
- Documentation: describe the estimate and hide conditions in the existing
  design-language guide. No convention change requiring CLAUDE.md edits.
- Deletion/deprecation: no obsolete public abstraction or migration introduced.
- Platform: portable chrono/FTXUI code; real PTY checks on this macOS host.
  Linux and CI are separate unverified environments.

## Rollout and rollback

Ship one local uncommitted change after independent design/PRR review. Revert
only this RFC's app/screen fields, suffix, tests and documentation to remove
it. No user data or settings rollback is needed. No commits, push or external
tracking issue are requested; tracking is local, as in recent Loom RFCs.

## Drawbacks

The value is an approximation of token throughput, not provider billing usage.
Periodic bounded sampling scans the current generated block, and one optional
label uses horizontal space. The two-second silence expiry intentionally hides
stale rates even if a remote response remains open.

## Alternatives considered

- Do nothing: fails the explicit request for a live rate near the indicator.
- Count network fragments: cheap but incorrect; chunk grouping is transport
  behavior, not token count.
- Change shared wire events for continuous exact usage: providers do not
  reliably expose that data, and completion-only counts arrive too late.
- Add a model-specific tokenizer: dependencies and compatibility work are
  disproportionate to an explicitly estimated UI indicator.
- Sum independent per-delta estimates: results depend on fragmentation and
  repeatedly count the estimator's minimum/rounding allowance.
- Always recompute on every delta or add a render ticker: unnecessary work;
  capped sampling plus existing stream events and a one-shot expiry suffice.

## Testing and verification plan

Record the discovered pre-change CTest registration count and reconcile new
native tests without deleting existing tests. Add focused AppAdapter event
injection tests using the existing injectable monotonic clock and renderer;
exercise the real executable through the existing PTY harness. Cover the current Messages streaming path without changing its wire schema.
An attempted OpenAI SSE PTY case exposed a pre-existing boundary:
`stream_query` calls `stream_single_api_call`, whose parser consumes Messages
events even when the non-streaming wire backend is OpenAI. OpenAI SSE is
therefore not an applicable native TUI case in this scope; its integration
remains unverified and no engine change is bundled here.
Run debug and release `-Werror` builds, relevant serial UI CTest suites, focused
PTY cases, RFC lint and module graph checks. Record remaining environmental
limitations and pre-existing failures separately from passed checks.

## Documentation impact

Update this RFC and the docs index, add the metric semantics to
`docs/design/design-language.md`, and record the ephemeral app-to-screen
measurement coupling in the existing decision register. No public manual
configuration or changed build instructions.

## Open questions

None. The estimate marker, two-second inactivity cutoff, foreground-only
scope, and unchanged Tasks layout follow the requested behavior and available
stream data. Exact model tokenization remains explicitly outside scope.

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-15 | — | Drafted for independent design and PRR review | local, uncommitted | Current event and spinner source inspected; baseline 2694 CTest registrations; no src edits yet |
| 2026-10-15 | — | First review requested changes; explicit ownership, math, cancellation priority and clock contract added | local, uncommitted | Reviewer agent:tps_review; four blocking findings addressed in state ownership contract |
| 2026-10-15 | A | Design and PRR approved; Phase A started | local, uncommitted | agent:tps_review; all five blocking findings resolved; RFC/store graph/inline lint pass; baseline 2694 tests |
| 2026-10-15 | A | Implementation and local graduation measurements complete; independent code review pending | local, uncommitted | Debug/release full `-Werror` builds pass. Per preset: serial UI 1105 passed, 2 pre-existing disabled, 0 failures (debug 166.09s, release 119.29s); query 117 passed; new generation PTY 2 passed; existing compressed-chain/stream/tool cancellation PTY 3 passed. Registration 2708 = baseline 2694 + 12 unit + 2 PTY, no deletions. 637 modules / 796 units: 0 cycles, 0 upward edges; store and inline lint pass; RFC lint 18 pass. Six approved branding hashes unchanged. Linux PSS, Linux execution and macOS CI timings remain unverified; current interactive OpenAI SSE is an existing engine limitation outside UI scope. |
| 2026-10-15 | A | Independent implementation and RFC conformance review approved; Phase A graduated | local, uncommitted | Reviewer agent:tps_review: APPROVE; no unresolved material findings. Final tested source and branding hashes match. The previous verification row records all measured graduation criteria and remaining platform/engine limits. RFC marked implemented; docs index updated. |
