---
rfc: 28
title: Automatic Model Generation Defaults
status: implemented
owners: "@Zzzode"
reviewers: ["agent:/root/settings_audit (design and PRR)", "agent:/root/generation_review (implementation and graduation)"]
created: 2026-10-19
last-reviewed: 2026-10-19
tracking: local RFC-0028 implementation history; user requested implementation without commits
---

# RFC 0028 — Automatic Model Generation Defaults

## Summary

Make ordinary model switching require no output-token or thinking-budget tuning.
Use provider defaults where the supported wire permits omission, derive required
Messages output limits from the existing exact model policy, and preserve old
explicit overrides. Remove generation tuning controls from ordinary settings.
Keep context headroom independent from whether an output limit is sent.

## Motivation

At baseline d02e651dea79c0d9275af7593e738f44c0d874bf the worktree is clean.
ModelSettings and ModelParams both default to 16384; both wire backends always
emit max_tokens. ConfigManager saves that default and extendedThinking=false.
The settings dialog converts absent temperature to 0.7 and writes it on any save.
resolve_engine_config maps true to Adaptive and therefore drops thinkingBudget.
The main CLI separately constructs config and does not apply these thinking
settings. Both query loops replace a configured output limit with at least
64000 after truncation. These are concrete configuration/transport discrepancies.
Existing debug/release build directories register 3006/3004 tests; reconfiguration
will establish the current-source total rather than treating stale discovery as
an authoritative baseline.

Provider contracts checked 2026-10-19: Anthropic Messages requires max_tokens;
OpenAI-compatible Chat Completions accepts omission on supporting servers;
Responses and native Gemini are not supported Loom wire protocols. Claude manual
thinking is model-specific; omission retains the provider's thinking default.
Sources: https://platform.claude.com/docs/en/api/messages/create and
https://code.claude.com/docs/en/env-vars and
https://platform.claude.com/docs/en/build-with-claude/extended-thinking.

## Goals

- G1. New settings saves omit maxOutputTokens, thinkingBudget, extendedThinking
  and temperature unless already explicitly configured.
- G2. Automatic OpenAI-compatible business requests omit max_tokens; Messages
  requests carry a positive automatically resolved value. Summaries retain their
  own bounded explicit output budget.
- G3. Existing positive output overrides retain their meaning across switching,
  fallback, native descendants and truncation continuation. No request silently
  raises an explicit limit.
- G4. Unspecified thinking uses provider defaults. Explicit true plus a budget
  selects manual mode; explicit true without a budget retains adaptive mode.
  Existing false keeps omission semantics for compatibility. No new promise that
  false disables reasoning on arbitrary services.
- G5. Saving unrelated settings preserves absent or explicit generation options.
- G6. No new service, target, provider discovery worker, model-name guessing or
  dependency direction changes; full debug/release verification and native tests.

## Non-Goals

New Responses/Gemini backends, live model discovery, credentials/proxy changes,
new effort controls, catalog expansion without separate source-backed evidence,
provider-independent thinking capability inference, total-task spending policy,
user-home configuration edits, commits, push or deployment. Old automatically
written numbers cannot be distinguished from deliberate settings and are kept.

## Proposal

### Ownership and shared scope

Config owns unset/explicit persistence. Existing compact.model_context owns
capacity and default output resolution; QueryEngine and native agent owners use
it before their existing transports. Settings UI stops writing generation knobs.
The user authorized this cross-module change by requesting the preceding plan.
A UI-only deletion cannot remove forced wire values, and a wire-only deletion
cannot satisfy required Messages parameters or compaction headroom. No new
module or library is introduced.

### Output policy

Keep the existing integer contracts and define zero as an internal automatic
sentinel in ModelSettings, ModelParams and ApiExecutionContext's original output
request. It is never a zero-valued provider parameter or an accepted profile cap.
Existing positive file/env/SDK requests remain explicit; config-tool JSON null
clears the user-tier override using its established tier semantics. CLI displays
auto and structured projections use null, never a misleading numeric zero.

ResolvedModelContext retains positive output_tokens for context headroom and
adds an optional request_output_tokens: null means omit the wire output field.
An explicit request is capped by the independently resolved known model cap;
its context feasibility still uses existing fail-closed checks.
For automatic mode, derive reserve from the independent output cap, or 16384
when unknown, bounded by half of the window remaining after the existing safety
margin max(64, window/50), with a floor of one token. This leaves input headroom
for small windows. Messages sends this positive value; OpenAI-compatible omits
max_tokens and uses the service default. This reserve is an estimate, not a claim
that an unknown provider will generate exactly that amount. A service whose
Chat Completions endpoint requires an explicit limit can use the existing
advanced override. Profile maxOutputTokens remains capacity metadata, not an
implicit user request.

Reuse exact endpoint/wire/model profiles and catalog ordering from RFC 0026.
Preserve raw zero or positive requests across immutable native snapshots;
descendants resolve their own model. Native business transport and main business
transport must agree on omission; summary requests always retain explicit bounds.
For a manual thinking budget combined with automatic output, raise the automatic
reserve target to at least budget+1 before applying model-cap and half-window
bounds. If those bounds prevent budget < effective output, reject before HTTP
with the existing explicit diagnostic. Never raise an explicit output override.
Cover absent/true/false thinking flags combined with a budget: absent/true select
manual, false retains legacy omission and does not activate the budget.

The existing three-attempt truncation continuation may remain, but must retain
the configured request and resolved model cap on every continuation.

### Thinking and persistence

ModelSettings.extended_thinking becomes optional<bool>; absent means provider
default. A shared settings-to-ThinkingConfig resolver used by main CLI and
assembly selects Default when absent without budget, Disabled (legacy omission)
when false, Forced when a budget is specified and not disabled, and Adaptive
when true without budget. Default and Disabled omit thinking on the Messages
wire; Adaptive/Forced retain their existing protocol shapes. OpenAI-compatible
retains its existing omission of unsupported thinking fields. Do not invent a
universal mapping from numeric budgets to reasoning effort.

Save output/thinking flags only when explicit, preserving destination-file
patch behavior and unrelated keys. Missing/null options do not become numbers
or false on ordinary saves. Keep positive JSON/env limits and explicit booleans
readable. Remove max output, extended thinking, thinking budget and temperature
controls/state copying from the ordinary dialog; advanced config APIs remain.
The dialog no longer offers a synthetic 0.7 value or unsupported temperature
range. Context fallback and view-only show-thinking remain in their owners.
Null-clear metadata, string projections and typed readers change together.

### Compatibility and observability

No wire endpoint, session/checkpoint shape or persisted native-agent shape
changes. Existing explicit settings remain explicit; no background edits to a
user's files. New host-only resolved metadata distinguishes request omission
from headroom. Actual request dumps continue recording the exact body. The existing
exercised golden tests remain unchanged; the removed controls and focus rows are
verified by the actual component tests and truecolor PTY frames. Unreferenced
settings-tab snapshot assets are not regenerated.
Rollback restores source behavior; old readers tolerate absent settings keys.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Automatic generation | Config, resolver, both owners/transports, dialog, tests, docs | done | Native config/policy/wire/HTTP/child/UI tests pass; debug and release build with Werror; serial ctest totals recorded; RFC and graph gates pass; real PTY settings save inspected |

## Production Readiness Review

- Correctness: test unset and explicit values, null clearing, file/env precedence,
  save preservation, tiny windows, unknown endpoints, model switching/fallback,
  summaries, streaming/nonstreaming truncation and native descendants.
- UI: review changed truecolor goldens and exercise actual settings save in a
  PTY; no changes to palette, renderer timing or terminal protocol.
- Build: existing targets only, declarations in module interfaces, no concurrency
  reduction and no new textual third-party includes. Run graph and RFC lint.
  Linux PSS and hosted macos-14 CI cannot be measured in this local uncommitted
  macOS checkout; disclose that limit rather than claiming those gates passed.
- Persistence: atomic patch/save machinery and physical scope boundaries remain;
  no secret values emitted, no automatic user-home edits, no schema migration.
- Rollback: revert this phase as one logical uncommitted diff; old files remain
  readable. No runtime worker, timer or network discovery is added.
- Observability: retain prompt dumps, source/output-cap source, cumulative billing
  and context occupancy. The auto reserve remains an explicitly documented estimate.
- Documentation/deletion: update context-compaction guide, decision register and
  docs index; remove obsolete UI control code. AGENTS/CLAUDE build/path conventions
  do not change. English comments/docs only.

## Rollout and rollback

Complete design and PRR review before production edits. Implement in the current
local branch without commits, integrate disjoint ownership changes, then run
native regression/dual-preset tests and independent code review. No published
issue is created: this local RFC/history is the tracking record. Revert only
this task's changes if rollback is needed, preserving unrelated concurrent edits.

## Drawbacks

Unknown services still require an estimated reserve; omission behavior depends on
the service implementation. Legacy explicit values remain until users clear them.
This intentionally keeps advanced overrides for compatibility while simplifying
the normal path. Older thinking models use their own default when unspecified,
which may mean no thinking; automatic mode does not promise maximal reasoning.

## Alternatives considered

1. Do nothing: leaves forced numbers, ignored budgets and unintended UI writes.
2. Delete all fields and always omit provider parameters: breaks required
   Messages requests and removes compaction reserve/explicit compatibility.
3. Always use one large hardcoded limit or the model maximum: can reject small
   models or reserve the entire context. Keep model-aware headroom instead.
4. Add live discovery/provider registry: materially expands network/lifecycle
   scope; defer until there is a concrete supported-provider contract.

## Testing and verification plan

Use existing test_model_context, test_config, test_runtime_config,
test_wire_backends, test_wire_transport, test_agent_wire, settings-layout and
UI golden infrastructure; add no disposable scripts as completion evidence.
Reconfigure/build debug and release and run complete serial ctest with logs,
report first-pass failures separately from unchanged reruns. Record registered,
passed, skipped and failed counts. Run tools/rfc/rfc_lint.py, architecture graph,
inline-body checks and git diff --check. Review golden deltas and record PTY
frames for settings before/after saving against an isolated config directory.

## Documentation impact

- docs/dev/context-compaction.md: auto versus explicit output and reserve.
- docs/decisions/design-decisions.md: defaults, omission and legacy thinking.
- docs/README.md: RFC index.
- No path/build convention changes; AGENTS.md and CLAUDE.md remain unchanged.

## Open questions

None. Unknown provider metadata uses the documented fallback; explicit legacy
false retains omission semantics. Hosted CI is unverified, not a local blocker.

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-19 | — | Opened provisional; design and PRR pending | Uncommitted | Baseline source discrepancies and stale discovery counts recorded above |
| 2026-10-19 | — | Design and PRR approved after P1 joint-budget correction | Uncommitted | agent:/root/settings_audit approved; manual budget/automatic output and overflow checks required |
| 2026-10-19 | A | Independent implementation and real-PTY test review approved | Uncommitted | agent:/root/generation_review found no blocking issues; runtime graduation remains pending full verification |
| 2026-10-19 | A | Same-instance explicit-to-auto persistence correction approved | Uncommitted | agent:/root/generation_review approved explicit clear-intent preservation; command regression verifies disk null and fresh-instance auto |
| 2026-10-19 | A | Local implementation graduated | Uncommitted | agent:/root/generation_review approved after independently matching frozen source/binary hashes, full test-name coverage, rerun sets and real PTY evidence |
| 2026-10-19 | A | User authorized publication with RFC 0029 | p/zode/automatic-model-defaults | Publish the verified automatic model policy together with service capacity discovery; prior local validation history remains unchanged |

### Local validation result

Both debug and release builds passed with warnings treated as errors. The final
release suite ran serially in two complementary selections: ordinary tests with
`LOOM_MODEL` unset, and historical streaming snapshots with their fixture model
`LOOM_MODEL=model_api/experimental_0812_256k`. The test-name union exactly matches
all 3128 registered tests, without overlap: 3101 passed, 25 skipped by existing
platform/live-test conditions, two disabled, zero failed.

Debug's first full run had 3065 passed, 36 failed, 25 skipped and two disabled.
Four stale assertions and the same-instance auto-persistence bug were corrected;
29 snapshot failures needed the fixture model, one test collided on an existing
fixed temporary directory across concurrent presets, and one timing-sensitive
clipboard test passed unchanged. All 36 failures passed on rerun. The latest
affected-module selection had 234 passed and four failures caused by applying
the snapshot model environment too broadly; all four passed with that variable
unset, without another source change. The preliminary overlapping release run
was interrupted and is not counted as full-suite evidence.

The latest eight real CLI/PTY model-context and generation-default scenarios
passed, with saved settings and loopback HTTP request captures. RFC lint,
architecture/core/UI/store/TLL checks, inline-body checks and diff whitespace
checks passed. Local logs, the complete changed-file list, failure history,
source hashes and both executable hashes are archived under
`build/generation-defaults-validation-20261019/`, indexed by `validation.json`.
No source/test file changed after the verified dual build. External live model
services, hosted CI and Linux PSS remain unverified. No commit or push was made.
