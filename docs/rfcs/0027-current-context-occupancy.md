---
rfc: 27
title: Current Context Occupancy
status: implemented
owners: "@Zzzode"
reviewers: ["agent:/root/compaction_design_audit"]
created: 2026-10-19
last-reviewed: 2026-10-19
tracking: local RFC-0027 implementation history; user requested a local footer correction
---

# RFC 0027 — Current Context Occupancy

## Summary

Show an estimated current working-context token count and its share of the
active model window in the footer. Keep session usage and cost cumulative.
Reuse the query owner's actual business-request schema and usage observation;
status reads must never invoke dynamic providers, prepare requests or sample a
model. Keep estimates explicitly marked with `~`.

## Motivation

At baseline `1ebc5e941eb09b65d2770c255a3efaff315c0ba5`, both presets register
3089 tests. The preceding real Release PTY recording shows 624.2K/256.0K and
244% after the actual business body shrank from 1246304 to 20803 bytes and a
durable checkpoint committed. This proves the displayed quantity is not the
working context; serialized bytes themselves are not a token measurement.
`AppAdapter::ProjectRuntimeMetadataToScreenState` assigns cumulative input plus
output to context_token_count. Custom statusline current_usage and percentages
also consume cumulative counters. `get_usage()` must keep its billing meaning.
The old context_utilization estimate omits actual dynamic/verbatim MCP schemas
and provider calibration, and prepares a static request while reading status.

## Goals

- G1. Token count and percentage describe one current committed working context,
  including retained outputs/tools, system instructions and known request schema.
- G2. Repeated business turns and summary cost cannot accumulate as occupancy.
- G3. Successful compaction/clear/restore/history reduction invalidates stale
  floors; model switches use the newly resolved window without HTTP.
- G4. Status reads are cached, side-effect free and do not re-enter providers or
  prepare a request. Unchanged reads must not rescan large histories.
- G5. Preserve cumulative usage/cost, wire/checkpoint/agent-record formats,
  permissions, footer row budgets, user command priority and threshold colors.

## Non-Goals

Exact provider tokenization, live counting of uncommitted streaming deltas,
background discovery of future dynamic schemas, child-pane billing redesign,
new configuration, changed compaction policy, provider-native context APIs,
installation, commits or push. The UI keeps its existing last stable snapshot
while a query worker runs and refreshes after its completion handoff.

## Proposal

### Ownership and dependency direction

QueryEngine owns a small read-only ContextUsageSnapshot and context_usage()
accessor: estimated tokens (uint64), resolved window and optional latest successful
business TokenUsage. context_utilization delegates to the same source. Reuse the
existing compact estimator and conversation revision; no new service, target,
mutable global or persisted DTO. The App projects it into existing UI stores.
The user's request explicitly targets this cross-layer context-display behavior.

### Current working-context estimate

Cache the last business request's actual prepared schema (tools, output_config,
response_format) at the existing HTTP preparation boundary and use the same
system/message projection as the business builder. Never call a dynamic or MCP
provider or backend prepare from a getter. Before the first business preparation,
use an owner-initialized static schema estimate without invoking providers.
The estimate uses the most recently sampled schema: newly discovered dynamic
tools become known at the next real request assembly. This is always an estimate,
including cached provider usage, media and opaque blocks; it is not a claim of
provider-exact measurement or complete unsampled dynamic metadata.

Reuse matching UsageObservation: Messages includes ordinary, cache-created and
cache-read input; OpenAI prompt input already includes cache and is not doubled.
Use shared estimate_request_tokens over current working messages, not the full
audit or a sum of request usages. Retained assistant/tool contents are counted
as messages; reserved future output and safety margin are not occupied tokens.
Cache by conversation_revision_; invalidate directly when schema/calibration
changes. Apply the existing observation identity and resets on model/backend,
clear/restore, compaction and content replacement so an old large floor cannot
survive a smaller working history. The getter returns tokens/window together
under the existing owner locking rules. Do not broaden engine concurrency;
App's query_running completion handoff remains the synchronization boundary.

The actual prepared schema may become known even if the HTTP request later
fails; this must not publish a successful usage observation. Reuse the builder's
pure conversation projection, including compact/snip/system rules, rather than
duplicating a subtly different history walk. Static initialization may use a
private provider-disabled builder mode; every real request retains its existing
provider behavior. This is not a new public option.

Only successful business responses update latest request usage. Summary usage
continues to affect session billing only. Failed requests cannot publish summary
or partial usage as a successful request; a successful response without usage
publishes no measured current_usage. Existing TokenUsage does not retain a
presence bit, so all-zero usage is conservatively treated as unreportable; do
not add a wire-presence protocol change for this display fix. Clear/reset/restore/model switch/history
replacement/compaction invalidate obsolete request usage. A session-id-only
change does not discard a still-current conversation.

### UI and external statusline

Use 64-bit context counts in chrome, footer and related projection fields.
Production estimated occupancy renders `~N% X/Y`, with the existing responsive
count visibility, colors and segment priority. A defaulted estimate marker keeps
manually supplied known renderer fixtures valid. Correct the settings preview's
hardcoded denominator to the active window. Do not clamp used percentage to hide
real over-capacity working history; remaining percentage has a zero floor.

Custom statusline JSON retains total_input_tokens, total_output_tokens and cost
as cumulative values. current_usage becomes the optional latest business usage.
Add nullable current_context_tokens and current_context_is_estimated under
context_window; used_percentage, remaining_percentage and exceeds_200k_tokens
use the same current snapshot. Existing names/types remain; the new fields are
additive. Document the corrective change from cumulative current_usage and
percentage values. Configured command output retains priority.
At the existing successful model, clear, compact, resume and clear-session
boundaries, refresh custom command input directly through TriggerStatuslineUpdate.
Reuse its busy-worker deferral; do not add observers, tickers or testing APIs.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Query snapshot | Query owner and native transport tests | done | Both protocols and query/stream separate current/cumulative usage, count cached inputs once, retain actual schemas, avoid getter side effects, invalidate after history/model changes |
| B | Footer projection | App, chrome/footer, statusline JSON and native UI tests | done | Current tokens/window drive all occupancy values; 64-bit and estimate marker, percentages and existing layout/command priority pass; actual custom status-command JSON refreshes at successful lifecycle boundaries |
| C | Acceptance | Native PTY, builds, reviews and docs | done | Debug/Release -Werror and focused plus complete serial CTest; reconcile every added case; graph/inline/RFC gates and frozen hashes; actual Release PTY shows occupancy drop after compaction and correct switch/clear/restore values; independent approval |

## Production Readiness Review

- Correctness: native query tests cover business/summary/failure isolation,
  cached input, schema snapshots, lifecycle invalidation and zero side effects.
  Native App/renderer/JSON tests and actual CLI PTY cover the production seam.
- Build: declarations/out-of-line bodies follow existing modules, no new targets
  or dependencies. Default Ninja concurrency; record both builds and serial
  suites. Linux PSS and hosted CI remain unavailable locally and disclosed.
- Compatibility: get_usage and session cost retain their meanings; statusline
  field names/types stay readable with additive occupancy metadata. No changes
  to configuration, wire, history/checkpoint/agent formats or authentication.
- Observability: existing request/session traces remain authoritative. A `~`
  marker distinguishes estimated occupancy; no fabricated provider count.
- Documentation: amend current developer guidance and add the occupancy decision.
  No new project conventions or CLAUDE.md updates are needed.
- Rollback: revert owner snapshot and UI/test/docs together; no data migration.
- Deletion: remove the cumulative-to-occupancy assignments and duplicate old
  estimator path where the new owner source supersedes it; no unrelated cleanup.

## Rollout and rollback

Independent agent design and readiness approval precedes production edits.
Implement A and B in disjoint owners against the approved snapshot fields,
then integrate native end-to-end coverage; root alone runs builds/tests.
Preserve the clean baseline and all unrelated work. Leave all changes local.
Reverting the scoped source/tests/docs restores previous behavior without a
persisted migration; the user need not perform design review or sign-off.

## Drawbacks

Owner-local ephemeral schema and estimate caches add invalidation obligations.
Estimated occupancy can differ from provider tokenization. Before the first
request or after unsampled dynamic changes it includes only known schema data;
the `~` marker and documentation must retain that limit. The busy UI retains a
stable committed snapshot instead of racing mutable worker state.

## Alternatives considered

1. Keep cumulative usage: known wrong after repeated turns and compaction.
2. Multiply the old utilization ratio by window: loses actual dynamic schema and
   usage calibration, and keeps request preparation in rendering paths.
3. Rebuild full business input on every render: re-enters stateful providers,
   adds large-history work and can change real request behavior.
4. Display only last provider input: stale after retained outputs, compaction,
   clear/restore and model changes; unavailable before the first request.

## Testing and verification plan

Baseline 3089 registrations per preset. Extend established query, UI, command
JSON and forkpty targets; record exact additions/deletions after discovery.
Cover cumulative-vs-current, cached input, current schema and getter side effects,
compaction/history replacements, restore/clear/model/backend changes, failures,
usage-less responses and 64-bit display. Preserve existing semantic goldens;
review any intentional marker/layout effects without broad regeneration.
Add two focused native CLI/PTY cases: repeated business turns with clear, and
restored large history with compaction/model switches/clear/resume. Native App
tests capture actual status-command JSON, rather than testing only serialization.
Use controlled loopback responses for deterministic budget behavior. A real
Release process in kernel PTY must show a smaller current estimate after a
checkpoint while session totals/cost remain accumulated, then switch window and
clear/restore. Controlled responses do not certify provider semantic quality.

Local acceptance uses the current macOS presets. The unchanged non-executed
inventory consists of 25 skips (16 tree-sitter-dependent, five provider opt-in,
three platform source guards and one Windows-only case) and two disabled UI
cases. Tree-sitter-enabled execution, real-provider opt-in acceptance, Linux PSS,
Windows execution and hosted CI were not established by these local runs.

## Documentation impact

Update docs/dev/context-compaction.md, docs/decisions/design-decisions.md and
this RFC's docs index entry. Preserve RFC 0026's historical limitation and record
this follow-up as the replacement rather than rewriting its history.

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| Exact tokens without universal tokenizer | @Zzzode | Calibrated working-context estimate with explicit marker |
| Unsampled dynamic schema | @Zzzode | Last real business snapshot; no extra provider calls from UI |
| Streaming drafts | @Zzzode | Existing stable UI handoff; committed history only |

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-19 | — | Provisional RFC opened after source trace | Uncommitted | Baseline 1ebc5e94 clean, 3089 registrations/preset. Actual preceding PTY displays 244% after successful compaction; query/current and UI/cumulative sources traced. agent:/root/audit_subagent_compaction supplied bounded query-accounting analysis; no production edits |
| 2026-10-19 | A/B | Independent design and PRR Approved; implementation authorized | Uncommitted | agent:/root/compaction_design_audit Approved owner-local schema/revision caching, truthful estimates, immutable busy-UI handoff, additive statusline fields and five direct refresh boundaries. Baseline graph: 644 modules, 820 units, 0 cycles/upward violations; 60 targets, 0 target SCCs. Agent-run source and execution gates remain pending. Root owns UI/docs/integration; query and native E2E work have disjoint ownership |
| 2026-10-19 | A/B | Independent production and test source review Approved | Uncommitted | agent:/root/compaction_design_audit Approved projection reuse, schema/revision caching, invalidation, business/summary isolation, five direct UI refresh boundaries and 64-bit display/JSON. Two P2 fixture findings resolved before execution: JSON strings now use JsonMutDoc, and PTY does not expect a cost-command report bypassed by the actual App. Precise cumulative billing remains covered by actual App status-command JSON; PTY asserts current occupancy and lifecycle behavior. Backend factory reset clears snapshots without preparing a custom backend |
| 2026-10-19 | C | Initial Debug build failure corrected without relaxing warnings | Uncommitted | First build exit 1 in 90.86 s: four missing optional designated initializers in the new App fixture triggered -Werror. Explicit nullopt initializers preserve the intended profiles. Second Debug build exit 0 in 20.95 s. Original failure remains in first-debug-build.log |
| 2026-10-19 | C | First focused failure corrected at the actual persistence boundary | Uncommitted | First focused 16 cases: 15 passed, 1 failed (App custom statusline after manual compact). The resume fixture wrote audit files outside the engine's configured sessions_dir, so durable checkpoint validation correctly rejected compaction. Fixture now writes the actual engine directory; source/guards/assertions unchanged. agent:/root/compaction_design_audit Approved the correction. Third Debug rebuild exit 0, 19.03 s |
| 2026-10-19 | C | Preserved old Release rejected; Debug expanded focus passed | Uncommitted | New native context-occupancy-repeated case fails the preserved baseline Release in 10.37 s: no complete estimated current-occupancy footer. Corrected Debug expanded focus passes 110/110, exit 0, 33.21 s, including all 15 added cases and related model/statusline regressions. Release build/focus, both complete serial suites and final integrity/terminal gates remain pending. Evidence: build/context-occupancy-validation-20261019/ |
| 2026-10-19 | A/B | Query and UI phases graduated after dual-preset focused verification | Uncommitted | Release build exit 0, 243.82 s; Release expanded focus 110/110, 29.66 s. Both inventories register 3104 cases: 15 named additions, zero removals against the 3089 baseline. All additions pass in both focused runs. Graph and inline gates pass; complete serial suites remain pending |
| 2026-10-19 | C | Independent actual Release PTY evidence review Approved | Uncommitted | agent:/root/compaction_design_audit verified seven original ANSI/row/JPEG captures and five request bodies: approximately 119% 305.1K/256K before compaction becomes 2% 5.3K/256K after it; restore to 1M retains 5.3K. One durable checkpoint and the literal constraint persist; local switch/clear/resume make no extra HTTP requests; process exits 0. Release SHA-256 f99e07f812953d08cde7f38ba616c79e23ddf695a41d9cd41fc5cedf7ca78aa0 and all 2599 input hashes match acceptance. Clear rebuilds a normal system prompt (11.7K); the imported seed/checkpoint omits that system prompt, so resume returns 5.3K. Usage/cost are controlled fixture data; images render raw kernel-PTY bytes through xterm, not a native terminal-app capture. Manual observer was corrected to actual Resuming session feedback without reissuing the command. No hosted/provider-capacity or summary-quality claim |
| 2026-10-19 | C | First complete Debug run exposed intentional streaming golden changes | Uncommitted | First complete serial Debug run exit 8, 596.74 s: 3104 registrations, 3048 actual passes, 29 failed, 25 skipped, 2 disabled. All 29 failures are StreamingReplay/StreamingMigration snapshots with old cumulative zero-usage footers. The test-only semantic footer path normalizer also did not recognize the new estimate marker. Root extends only that grammar to accept one leading marker, adds positive/negative/idempotence cases, and reviews every regenerated golden line before new builds and full runs. Original complete log/XML retained; Release full suite was not started |
| 2026-10-19 | C | Snapshot-only correction audited | Uncommitted | agent:/root/compaction_tests independently decoded all 72 golden comparisons in the 29 failed cases: each differs only on its last footer line; no body, tool, thinking or layout delta. Established update mode rewrote exactly 72 final lines from zero usage to the current estimate; snapshot-footer-audit.json locks each change. Update run was 33/34 because one explicit status-row assertion also expected old 0%; its expected literal now matches the same ~2% golden. This assertion stays exact, and no values are normalized away. Production source and Release executable remain unchanged; old terminal manifests remain authoritative for capture-time inputs |
| 2026-10-19 | C | Independent snapshot correction review Approved; final acceptance refrozen | Uncommitted | agent:/root/compaction_design_audit Approved all 72 exact footer-only golden changes and the three test-support/assertion files. Existing case registration stays unchanged. Ordinary Debug expanded focus passes 144/144, 53.20 s. final-inputs.json freezes the same 2599 paths; terminal-acceptance-bridge.json records exactly 75 test-only differences against the retained capture-time input manifest. The original 22 modified source/test files (including all 16 production files) and Release executable SHA remain unchanged. Final Release rebuild/focus and both complete serial suites run against the refrozen inputs |
| 2026-10-19 | C | Final complete acceptance and independent implemented gate Approved | Uncommitted | agent:/root/compaction_design_audit independently verified both complete XML/log/result inventories: each registers 3104, with 3077 actual passes, 25 skips, two disabled and zero failures; Debug exit 0 in 571.03 s, Release exit 0 in 485.69 s. Both ordinary expanded focuses pass 144/144 (53.20/48.95 s), all 15 additions run and pass, zero removals, unchanged non-executed inventory. Final Release test rebuild exit 0 in 35.88 s. All 2599 inputs and 222 executable hashes independently recomputed with zero drift; final-drift.json records empty differences. Actual Release PTY and its test-only manifest bridge remain valid. Source/design/PRR, snapshot correction and final execution reviews Approved; no unresolved P1/P2. Graph, inline and RFC gates pass; developer guidance, decisions and index updated. All phases done; changes remain uncommitted. Evidence: build/context-occupancy-validation-20261019/final-* and terminal-current/ |
