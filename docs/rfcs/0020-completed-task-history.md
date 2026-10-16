---
rfc: 20
title: Completed Task Feedback and History
status: implemented
owners: "@Zzzode"
reviewers: ["agent:review_task_completion (independent design and PRR)"]
created: 2026-10-16
last-reviewed: 2026-10-16
tracking: local
---

# RFC 0020 — Completed Task Feedback and History

## Summary

Retain completed todo data, show the completed checklist for three seconds,
then collapse the pinned MessageResponse block to `Tasks 3/3 completed`.
Persist the completed checklist in the existing successful tool-result message
so replacing or explicitly clearing active tasks cannot erase the record.

## Motivation

### Evidence

The real model/PTY task in session `5c57c706-4d86-4e4a-9c02-d84854f9254d`
completed three tasks, but its all-completed replacement became an empty list.
`todo_write` normalizes nonempty all-completed replacements to empty before
execution. Its native test explicitly expects `0 total`. The result message
contains counts only, so the visible history has no completed checklist.

| Measurement | Current | Target |
|---|---|---|
| Retained items after completing three tasks | 0 | 3 completed |
| Completion feedback duration | 0 seconds | 3 seconds, then one summary row |
| Completed item labels in successful result | 0 | All items, at most 10 |
| Additional threads / periodic render tickers | 0 | 0 |

## Goals

- Preserve completed items for both replace and merge operations.
- Show a nonempty all-completed list expanded for three seconds from its
  first UI projection, then retain a one-row completed summary.
- Retain the full completed list in normal message history, search, session
  persistence and replay through existing tool-result text.
- Preserve task indentation, work-row alignment and bounded activity budgets.

## Non-Goals

New task databases, settings, expansion controls, task IDs, background-agent
UI, wire formats, message variants, permissions, or changes to unrelated
dialogs, themes, reduced-motion work and public configuration.

## Proposal

The owners are the existing todo tool and interactive todo projection. The
requested lifecycle explicitly authorizes their coordinated change. A render-only
fix cannot recover task data discarded by parsing, and counts alone cannot
preserve completed labels in history. No shared message renderer is changed.

### Detailed design

- Delete the parser's all-completed-to-empty normalization. An explicit empty
  replacement still clears the active list, and agent scope semantics remain.
- Capture completed items in the successful execution result while holding
  the existing store mutex, only when the resulting nonempty list is entirely
  completed. The adapter appends a completion summary and all completed labels
  to its existing text result. Keep the existing counts prefix. This handles
  partial merge input without losing unchanged completed items and prevents a
  concurrent replacement from changing the recorded snapshot.
- On `/clear` and successful interactive `/resume`, clear only the default
  todo scope so the previous conversation cannot leak a completion summary.
  Failed resume preserves current tasks and agent scopes remain unchanged.
  Existing `/clear` conversation semantics remain unchanged; completion text
  uses normal saved history rather than a separate archive. Test both
  boundaries in the same process.
- Existing message projection, persistence and result rendering consume that
  normal tool result; do not synthesize extra assistant messages or write to
  history during rendering. Old saved sessions remain readable without migration.
- The UI-owned todo store holds an optional completion observation time and
  a collapsed flag. App projection compares complete display contents/status/
  priority to the previous projection; a changed list resets observation.
  Nonempty all-completed lists start the three-second monotonic grace. Repeated
  renders do not extend it. Empty or reopened/new work clears completion state.
- Use the existing injectable UI clock for observation, deadline arithmetic
  and the collapse predicate. The existing worker reads real steady_clock,
  never the unsynchronized single-thread-only test override. Without an
  override both clocks have the identical monotonic epoch. Fake-clock tests
  attach no screen, arm no worker deadline, and manually drain after advancing;
  real-clock PTY tests independently prove the idle wake. This follows RFC 0018
  and avoids changing the shared clock contract. Add one atomic deadline to the existing
  app deadline worker; it posts one Custom event, never touches plain UI state,
  and is only armed with an attached screen. App teardown already joins this
  worker. Projection computes collapse, including idle queries, and disarms
  the deadline after expiry. A stale wake only triggers a fresh projection.
- Render the all-completed header as `Tasks N/N completed` during the grace
  and after collapse. Collapsed lists demand only one activity row. During
  grace, normal terminal height budgets may already hide detail rows; never
  displace the editor to force expanded details. Teams continue sharing the
  activity budget; a combined one-row summary retains completion wording.
- Size completed labels to their visible terminal-cell width and render
  trailing row padding through a separate unstyled filler. Strike only the
  label, including a truncation ellipsis; never strike status glyphs or blank
  padding. Native cell assertions cover ASCII, CJK and very narrow terminals.
- No wire/schema/tag changes: the internal TodoWriteResult snapshot and
  TodoStore collapsed flag are coordinated producer/consumer seams. Tool
  output text gains an additive checklist; no consumer parses the prior counts.
  Existing plain/truecolor palette tokens remain; no expected existing golden
  changes for active or empty tasks. New native pixel tests cover completion.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Retain and collapse completion | Todo tool, app projection/deadline, todo screen/panel, tests, docs | done | Native tests prove replace/merge retention, explicit clear, immutable completion history; fake-clock UI tests prove grace, collapse and reopen; native real-PTY idle expiry and history replay pass; debug/release builds and full CTest pass; real configured-model task captures running/complete/collapsed states; RFC/architecture lint pass |

## Production Readiness Review

- Correctness: native registry tests cover completed replacement/merge,
  explicit clear, scope isolation and complete result labels. UI tests cover
  repeated render, exact grace boundary, changed/reopened list and short widths.
  Native PTY checks provide the real event-loop expiry proof; saved tool results
  provide replay coverage. No tight upper wall-clock bound is required.
- Build: existing targets/modules only; data declarations in interfaces and
  projection logic in existing implementation units. No new dependency or
  concurrency cap. Run graph and inline-definition checks. Linux `/proc` PSS
  and macOS CI runner timings are unavailable on this host and remain unverified.
- Compatibility/rollback: preserve existing todo input shape, empty clear,
  agent cleanup and result prefix. Added successful output is ordinary text,
  read by old session readers. Revert this single phase to restore prior UI
  policy; historical completion text remains valid.
- Observability: normal messages/dump-prompts persist unmodified envelopes;
  no trace schema, logging channel, thread or constant-frequency render added.
- Documentation: this RFC and docs index explain policy. No build/path convention
  change requires CLAUDE.md edits. Remove obsolete parser normalization/test.
- Platform: chrono/FTXUI portable; both local presets and macOS PTY verified.
  Linux execution and hosted CI remain explicitly unverified.

## Rollout and rollback

One coherent phase; the owner authorized committing and pushing the task
lifecycle and strikethrough fixes together on 2026-10-16.
Revert only the phase's todo, projection, deadline, render and native test
changes. No settings migration or persisted record rewrite is needed.

## Drawbacks

Completion results contain up to ten additional labels in model context and
message history. Completion grace adds one small transient state transition
and deadline. A tiny terminal can only show the progress header even during grace.

## Alternatives considered

- Do nothing: removes requested completion feedback and checklist history.
- Hide only in the renderer: cannot recover parser-discarded completed data.
- Clear after a timeout: loses the active completed snapshot unnecessarily.
- Add a new history message/store: duplicates existing tool-result persistence.
- Keep full details permanently: consumes activity rows after work is complete.
- New render ticker/thread: unnecessary; the existing one-shot wake service fits.

## Testing and verification plan

The committed baseline `42a5d172` registers 2711 CTests. Eight added tool, UI
and PTY cases produce 2719 registrations with zero deletions. Earlier shared-
checkout counts also included unrelated accessibility cases; final graduation
uses an isolated feature-only checkout and records those counts separately. Extend native tools/UI and the existing PTY fixture.
Use the injectable monotonic clock for three-second transitions, and real
forkpty for idle wake and historical result rendering. Run debug and release
builds with warnings as errors and full CTest, RFC lint, UI/store graph and
inline lint. Then run an actual task against the configured model endpoint,
retain raw frames and session results, and display running/completed/collapsed
captures. Captures illustrate the native assertions; disposable scripts are
not substitutes for project-native regression evidence. For this delivery, the
owner explicitly overrides serial scheduling: run all cases with eight parallel
workers per preset, without exclusions or retry-until-pass flags.

## Documentation impact

This RFC and the docs index record the task lifecycle. Implementation comments
identify the snapshot and one-shot deadline boundaries. No shared convention changes.

## Open questions

None. Three seconds reuses the existing thinking-collapse grace convention.
Explicitly clearing tasks remains possible; message history is retained.

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-16 | — | Drafted for independent design and PRR review | local | Parser clear, counts-only result, app projection and existing deadline service inspected; baseline 2767 registrations; no production edits |
| 2026-10-16 | A | Design and PRR approved; implementation started | local | agent:review_task_completion APPROVED; clock clarification and baseline 2767 addressed |
| 2026-10-16 | A | Independent lifecycle review identified session-switch leakage; local reset added | local | agent:review_task_completion: clear only default scope on /clear and successful /resume; pending-task leakage was pre-existing, retained completion requires closure |
| 2026-10-16 | A | Final implementation and lifecycle review approved; focused verification complete | local, uncommitted | agent:review_task_completion APPROVED. Final debug/release builds and 30/30 focused CTests per preset pass (11.18s / 9.18s), including same-process failed/successful resume, clear and agent isolation. Existing debug session regressions 5/5 pass. Final real-model task: 10/10 native tests pass (2.64s), report written/read, raw running/expanded/collapsed frames retained; observed collapse 3.06s. RFC, UI/store/TLL graph and inline lint pass; 638 modules, zero cycles/new back edges. First debug full run: 2776 registered, 2736 passed, 20 skipped, 2 disabled, 18 failed; all 18 isolated reruns pass. Current registration is 2777: initial 2767 + 7 task regressions + 3 concurrent pre-existing accessibility test additions, zero deletions. Release full suite pending. |
| 2026-10-16 | A | Correct metrics and record final full serial results; graduation withheld | local, uncommitted | Correction to prior row: graph has 637 modules / 796 units; inline lint checks 638 interfaces. Initial debug failures were 17 failures plus 1 timeout. Initial release: 2754 passed, 20 skipped, 2 disabled, 1 failure (403.71s), isolated rerun passed. Final frozen-code serial debug: 2777 registered, 2754 passed, 20 skipped, 2 disabled, 1 failure, E2E_Pty_Selection_wide waiting for native clipboard payload (485.28s); isolated rerun passed (5.47s), not equivalent to a green full run. Final frozen-code serial release: 2777 registered, 2755 passed, 20 skipped, 2 disabled, zero failures (411.71s). No source/test fixture drift across the final pair; only this RFC and an unrelated accessibility report changed. agent:review_task_completion retains code approval and approves honest closure, but withholds phase graduation; causality of selection failure is unproven. RFC remains implementable / A in progress until full serial debug and release are green. |
| 2026-10-16 | A | Strikethrough corrected; isolated feature-only verification recorded, graduation withheld | local, awaiting commit | agent:review_task_completion APPROVED the 16-file code/scope; label cells and unstyled filler have native ASCII/CJK/ellipsis coverage. Exact committed baseline 42a5d172 has 2711 tests, plus 8 task regressions gives 2719, zero deletions. Cold debug/release -Werror builds pass (150.94s / 350.84s); RFC lint 19, graph and inline checks pass. All 31 task-related cases pass in all four serial runs. First isolated debug/release each: 2696 passed, 20 skipped, 2 disabled, 1 Selection_wide native-clipboard failure (482.92s / 422.06s). Ordinary confirmation debug: 2696 passed, 20 skipped, 2 disabled, 1 GrepTest.CancellationTerminatesAndReapsChild failure waiting for fixture PID (465.82s); Selection_wide passes. Ordinary confirmation release: 2697 passed, 20 skipped, 2 disabled, zero failures (396.31s). Frozen source/fixtures have zero drift across all runs. Actual-model terminal session b19e366d-6714-4674-8e6b-0d69b6c7f0a5 passes the strike regression and retains running/expanded/collapsed raw frames. Prior failures remain disclosed; full debug gate is unsatisfied, A remains in progress and RFC remains implementable. |
| 2026-10-16 | A | Preserve full gate; diagnose and repair native clipboard test fixture | local, uncommitted | Owner requires ordinary full serial green. Next ordinary debug: 2696 passed, 20 skipped, 2 disabled, 1 Selection_release-only failure (473.93s); no source/fixture drift. A clipboard-debug full diagnostic reproduced the native failure and also reported one WelcomeMonogram first-resize frame failure; neither is a passing gate. Existing diagnostics report bytes=5, wait deadline expired after 3105ms. CTest selected reproduction 3 failed on attempt 3; native shell child sample stayed at _dyld_start before script entry, and syspolicyd recorded a strongly correlated 3003ms network timeout followed by provenance tracking for that PID. Replaced only the fake clipboard shell scripts with symlinks into the running native PTY fixture, retaining real product spawn/pipe/EOF/exit-status and all byte/toast assertions; no product timeout or clipboard policy change. agent:review_task_completion APPROVED fixture/code and separate test/feature commits. Debug/release builds and 11/11 selection cases per preset pass; 30 ordinary selected release-only CTest invocations pass. Grep stage instrumentation was temporary, did not reproduce in 100 baseline attempts, and was fully restored. Root causes of the prior Grep and welcome failures remain unresolved. Fresh frozen ordinary full serial debug/release remain required. |
| 2026-10-16 | A | Record independent welcome resize diagnosis without widening feature scope | local, uncommitted | agent:review_task_layout identifies a pre-existing two-sample resize race: Loom builds the welcome DOM from its terminal-size query, then FTXUI Draw separately reads terminal size for the pixel grid. A resize between the reads can retain the large wordmark on a 40-column grid. No welcome, shared FTXUI renderer or assertion changes; the earlier failure remains recorded and is not claimed fixed. Final ordinary full suites must still pass. |
| 2026-10-16 | A | Preserve failed release gate and isolate native fake ripgrep | local, uncommitted | Frozen native-clipboard pair: debug 2697 passed, 20 skipped, 2 disabled, zero failures (473.14s); release 2696 passed, 20 skipped, 2 disabled, one Grep CancellationTerminatesAndReapsChild PID-start timeout (403.13s). Source/fixture drift is zero. Bounded baseline/candidate CTest observations (50/20 passes) sampled a visible shell child before script PID creation, mostly at _dyld_start and a synchronous loader notification wait; they did not capture the full failing 3-second delay or matching syspolicyd PID event, so Grep causality remains unproven. Replaced fake-rg scripts only in test_grep.cpp with four native modes in the same executable image, preserving PATH/spawn/pipes, matching diagnostics, PID publication, flood truncation, closed stdout, cancellation/deadline/reap assertions and all original limits. agent:review_task_completion APPROVED the local test-only change. Both presets build; 58/58 combined focused cases pass per preset (22.05s / 19.20s); final close-error handling rechecked per preset and 30 ordinary selected debug cancellation CTests pass. Test fixtures form a separate two-file commit; task/strike feature remains one coherent commit. Fresh final ordinary full serial debug/release remain required before any commit or graduation. |
| 2026-10-16 | A | Owner overrides serial verification scheduling | local, uncommitted | Owner explicitly rejected serial validation latency and requested a faster verification workflow. Interrupted the last serial debug run with SIGINT; it was partial and is not a full passing result. Final validation now runs every registered test with 8 workers per preset on this 18-logical-core host, with no exclusions, retry-until-pass flag or clipboard-debug override. This owner-authorized scheduling override is specific to this delivery; no repository-wide policy change. Full debug/release completion and independent conformance review remain required. Correction to the preceding focused row: release focused CTest time is 19.23s, not 19.20s. |
| 2026-10-16 | A | Complete owner-authorized parallel full verification; shared resize defect blocks release gate | local, uncommitted | Full unfiltered -j8 debug: 2719 registered, 2697 passed, 20 skipped, 2 disabled, zero failures (58.96s). Full unfiltered -j8 release: 2696 passed, 20 skipped, 2 disabled, one E2E_Pty_WelcomeMonogram first banner-40 failure (51.59s). All task/strike, native clipboard and native Grep cases pass. Frozen source/fixture drift is zero; no repeat-until-pass flags or clipboard debug. Native trace again shows the old large welcome wordmark on the new 40-column grid. No shared renderer edits applied. A minimal Draw-loop frame rejection proposal is under independent design review; scope authorization is required by the owner-provided Shared-Change Gate before implementation. Commit/push and RFC graduation remain withheld. |
| 2026-10-16 | A | Owner authorizes minimal shared resize repair; implementation reviewed | local, uncommitted | Owner explicitly authorized the shared rendering-loop fix. CMakeLists.txt adds one include; PatchFtxuiResize.cmake rejects frames when terminal size differs across component Render and queues an internal closure, without application-visible Custom events or recursive retry. agent:review_task_completion and agent:review_task_layout APPROVED the refined design; implementation review APPROVED. Exact patch application, repeat configuration idempotence and unexpected-source FATAL guard pass. Candidate uses private FTXUI source; primary dependency SHA is unchanged. Final debug/release configure and -Werror builds pass (builds 49.96s / 181.37s); both configure passes retain identical patched-source SHA. Native debug WelcomeMonogram first-resize case passes 20 consecutive until-fail repetitions with unchanged assertions. Final full unfiltered -j8 checks remain required; no retry-until-pass policy is used. Shared resize fix is a separate third coherent commit. |
| 2026-10-16 | A | Repair deterministic cancellation UI fixture after preserving failed full checks | local, uncommitted | Shared-resize full release passes all 2697 cases plus 20 skipped/2 disabled (55.04s). Full debug and one ordinary full debug confirmation each report one existing CtrlCWhileStreamingQueryCancelsWithoutExiting failure (60.48s / 61.96s); no source/fixture/binary drift. The failing assertion is after Ctrl+C, not before query startup: the worker can finish immediate native HTTP abort before the test asserts query_running and transient Cancelling text. The test directly calls Render and does not execute the shared Draw patch. In the two symmetric Ctrl+C/Esc UI cases only, an existing abort callback now records the request while the existing server latch controls transport completion; all original assertions and timeouts remain, plus an explicit abort-request assertion. The atomic capture outlives engine/app teardown. Native PTY tests retain real immediate socket cancellation coverage. No product cancellation behavior, API or shared test helper changed. Debug/release builds pass (16.11s / 22.29s) and 11/11 focused UI/native cancellation cases pass per preset (6.62s / 6.13s). This local fixture fix is a separate test commit; final full parallel checks remain required. |
| 2026-10-16 | A | Final frozen parallel gates pass; phase graduation recorded | local, awaiting commit | Final unfiltered ctest --preset debug -j8 --output-on-failure and release equivalent each register 2719 tests: 2697 passed, 20 skipped, 2 disabled, zero failures (60.53s / 59.50s). No retry flags, exclusions or diagnostic environment overrides. Source/fixture and executed-binary drift are zero; candidate patched FTXUI is unchanged and primary dependency bytes are preserved. Both configurations build with -Werror; final cancellation-focused checks pass 11/11 per preset. Prior failures and diagnostic limitations remain in this history. Independent agent:review_task_completion approved feature, native fixtures, shared resize and cancellation fixture; final conformance review confirms graduation. Delivery contains 20 unique files in four coherent commits: native process fixtures, authorized shared resize repair, cancellation UI fixture, and task lifecycle/strikethrough. Existing public inputs, configuration and persisted schemas remain unchanged; Linux and hosted CI remain unverified. |
