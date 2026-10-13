---
rfc: 9
title: Optional Interactive Tool Round Budget
status: implemented
owners: "@Zzzode"
reviewers:
  - "agent:round_budget_review (design and PRR)"
created: 2026-10-13
last-reviewed: 2026-10-13
tracking: local
---

# RFC 0009 — Optional Interactive Tool Round Budget

## Summary

Interactive queries have no implicit tool-round limit. An optional positive
`--max-tool-rounds N` budget pauses a query after committing its current tool
results. A visible, clickable `[C] Continue` action resumes the model request
from those results with another N rounds, without replaying the user prompt
or completed tools. Non-interactive default limits remain unchanged.

## Motivation

### Evidence

Session `e874bab8-adac-4e3e-aab7-bc38a7b8101b` stopped after exactly 20 tool
rounds and 36 completed tool calls (2 Bash, 1 Glob, 33 Read). There were zero
failed results or failed API responses. Every response requested tools, so
the implicit limit prevented the model from producing its review. Results
were already committed; the UI lacked a continuation action.

## Goals

- Default interactive requests complete after more than 20 rounds.
- Explicit budgets preserve committed progress and offer keyboard and mouse
  continuation with no duplicate user prompt or executed tool.
- Pauses are distinct from API failures; API Retry retains its current role.
- Debug/release native tests and actual remote-model PTY workflows pass.

## Non-Goals

Permission-policy changes, automatic continuation after an explicit budget,
changes to dollar/token budgets, persisted formats, wire protocols, session
reload recovery, SDK default limits, or broad UI refactors.

## Proposal

The owning module is QueryEngine's streaming loop, with local projection and
interaction changes in the app/screens layers. No new module, service or
dependency edge is required. Existing `QueryOptions.max_tool_rounds` becomes
an effective optional streaming budget: null means unlimited. Its default
initializer is 20 to preserve non-interactive callers' existing behavior.
AppAdapter explicitly assigns its optional interactive budget to each query.
The blocking loop retains its existing default.

The main parser accepts a strictly positive uint32 through both separated
and equals forms of `--max-tool-rounds`. The internal main→UI C bridge passes
zero for an absent budget; this is not an external ABI. Both declarations
must change together. CLI documentation identifies the option as interactive.

Budget exhaustion keeps the existing `tool_round_limit` event classification
and unsuccessful query-end result, but the UI projects a neutral pause with
`can_continue`, not an API error. A screen-owned hit box and callback make
`[C] Continue` clickable; C only activates with an empty prompt, no running
query and no active dialog. AppAdapter rechecks eligibility and the engine's
resumable conversation tail. It uses the existing committed-conversation
resume path, without re-appending input, history or attachments. Each
continuation starts a fresh round budget. New submissions/clear replace the
transient pause entry. Existing cancellation, monetary budget and API retry
semantics remain intact.

The budget counts the existing loop's continuation iterations (including
token-limit recovery nudges), not individual calls in a parallel tool batch.
No input/result fields or persistence formats change. The existing event
string and new UI metadata are shape couplings covered by native tests.
Session/dump traces continue to contain canonical committed progress. No
existing truecolor golden changes are expected: the new pause state has
dedicated rendered assertions and PTY coverage.

## Alternatives considered

- Do nothing: ordinary successful tasks stop without a final answer.
- Raise the default to a larger constant: preserves arbitrary interruption.
- Automatically restart the original query: repeats prompts and side effects.
- Add a general checkpoint service: unnecessary; committed history and the
  existing resume path already provide the required invariant.

## Phases and graduation criteria

| Phase | Scope | Graduation criteria | Status |
|---|---|---|---|
| A | Engine, CLI, pause UI, regressions | Default >20 rounds; explicit pause/resume once; keyboard/mouse guards; PTY and remote model; full debug/release green; independent review | done |

## Production Readiness Review

- Correctness: test unlimited, explicit and resumed budgets, committed tool
  results, API failures, input/dialog/running guards and once-only effects.
- Build: no target changes except native test registration; existing module
  graph remains acyclic. Record warm builds and test totals. BMI PSS cannot
  be measured through Linux smaps on this macOS host; no new abstractions or
  module dependencies are introduced.
- Observability: retain event classification and canonical session traces.
- Documentation: this RFC, CLI help and docs index describe the new behavior.
  No global conventions or design-decision catalog invariants change.
- Deletion: remove the streaming loop's hidden fallback constant; retain the
  deliberate non-interactive default in QueryOptions.
- Compatibility: additive CLI flag; existing SDK/blocking defaults unchanged;
  no external wire or persisted contract changes, no compatibility aliases.

## Rollout and rollback

One local phase, uncommitted as requested. Compare against the task-start
baseline, preserving prior uncommitted work. Roll back only this phase's
engine, UI, parser, tests and documentation if verification fails.

## Testing and verification plan

Native QueryEngine scripted requests, screen/app interactions, actual loom
PTY tests with deterministic models, then a remote model through a real PTY.
Run focused coverage and full debug and release suites. Baseline: 2373
registered tests, 2351 passed, 20 skipped and 2 disabled. Reconcile additions
in implementation history. No lower concurrency or increased timeouts as
substitutes for fixes.

## Open questions

None. The user authorized optional budgets and an explicit continuation
entry; default interactive execution is unlimited.

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-13 | — | RFC opened (provisional) | Uncommitted | 20 rounds; 36 successful calls; zero failed API responses |
| 2026-10-13 | A | Design and PRR approved; implementation started | Uncommitted | agent:round_budget_review; strict parser, hit-box guards and budget→API Retry coverage required |
| 2026-10-13 | A | Implementation and final test addendum approved | Uncommitted | agent:round_budget_review; stdout-capture P2 fixed; no unresolved findings |
| 2026-10-13 | A | Graduation criteria passed | Uncommitted | 14 new regressions; 2387 registered, 2365 passed/20 skipped/2 disabled per preset; debug 25.19s/release 24.61s at -j16; 6 actual remote PTY cases; 240 selection stress runs; graph 634 modules/780 units, zero cycles/upward edges/dead imports/SCCs; dual -Werror builds and RFC lint clean |

### Verification notes

The first debug full run exposed an existing clipboard fixture timeout. Native
diagnostics showed a backend wait timeout after writing its bytes, while FTXUI
was busy-spinning on EOF stdin. A test-local open stdin pipe removes that
incorrect environment; 40 repetitions of each of three variants passed in each
preset with unchanged deadlines and concurrency. A test-local cwd restoration
also prevents a Bash teardown case contaminating following UI tests. No
production clipboard or terminal changes were made. The final full suites both
pass. Persisting paused UI metadata across process restarts and Windows PTY
verification remain outside this phase. Linux smaps PSS and CI-runner build
times were unavailable on this macOS host; local dual builds, graph invariants
and test measurements above are the recorded evidence.
