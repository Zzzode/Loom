---
rfc: 29
title: Model Capacity Metadata
status: implemented
owners: "@Zzzode"
reviewers: ["agent:/root/generation_review (design, PRR, implementation and graduation)"]
created: 2026-10-19
last-reviewed: 2026-10-19
tracking: local RFC-0029 implementation history; user requested correction without commits
---

# RFC 0029 — Model Capacity Metadata

## Summary

Read context capacity from the configured model service before falling back to
local estimates. Reuse the existing immutable model policy for main queries,
native descendants and model switching. No model-name guessing, service-specific
CLI dependency, disk cache or new user tuning knob is introduced.

## Motivation

The configured service returned HTTP 200 from GET /v1/models with twelve entries.
The exact selected model's entry includes context_length=1048576. A restricted
local capture is in build/model-context-discovery-20261019/models-response.json;
credentials are not recorded. Current Loom never requests this metadata and
resolves the model to the 200000-token fallback. The existing bootstrap fetch
returns an empty response and has no active runtime caller. Its cache is not
endpoint-scoped and must not be reused for capacities.

The baseline for this correction is the uncommitted RFC 0028 implementation on
p/zode/automatic-model-defaults. It has 3128 registered tests, successful dual
builds and independent graduation review. Preserve every existing change.

## Goals

- G1. With no contextWindowSize or modelContextLimits, an exact model advertising
  context_length=1048576 resolves to 1048576 before the first business request.
- G2. Main CLI, assembly, direct runtime Agent and SDK QueryEngine construction
  load the same metadata policy; native descendants inherit the frozen policy
  and switching selects the selected model's entry without another request.
- G3. Explicit profiles retain precedence. Valid server metadata precedes the
  built-in catalog and global fallback. Token usage is never treated as capacity.
- G4. Missing/invalid capacity, unsupported metadata endpoints and network errors
  preserve existing catalog/fallback behavior and never prevent a business call.
- G5. Metadata does not become a user override or leak across endpoint, wire,
  credentials or tenant identities. No credentials or raw metadata are logged.
- G6. Ordinary saves omit an unspecified default contextWindowSize; the ordinary
  settings panel stops presenting this global fallback as active model capacity.

## Non-Goals

BytedCLI integration, universal metadata standards, inference from model names,
probing capacity with large prompts, new providers, dynamic background refresh,
disk/global caches, model-list UI redesign, schema migrations, new configuration
flags, commits, push or deployment. Unrecognized metadata keeps fallback behavior.

## Proposal

### Ownership and authorized shared scope

Query's existing HTTP transport owns the bounded metadata request; compact's
existing model_context owner parses metadata and resolves capacities. Shared
ModelContextConfig carries discovered entries separately from explicit profiles,
reusing ModelContextLimit. Runtime entry points prepare a session-local snapshot
before registering native tools. Config/UI changes only remove misleading default
persistence and the manual fallback control. The user's explicit requirement is
shared automatic capacity identification; a user-file profile or UI-only edit
cannot fulfill it. Existing policy, wire and session formats remain intact.

### Metadata contract

Use GET /v1/models derived from the configured API base, preserving tenant paths
and a pre-existing /v1 suffix. Reuse the current wire's credential selection:
Messages sends Bearer when an auth token exists, otherwise x-api-key; OpenAI
uses Bearer with auth token preferred over API key. Messages also retains its
API-version header. No redirect following or endpoint fallback probes.

Support the observed envelope data[] with a nonempty exact id and a strictly
positive uint32 integer context_length. Parse optional positive max_output_tokens
independently when present. Reject floats, strings, zero, negatives and overflow;
unknown fields are ignored. Ignore malformed entries. Conflicting duplicate ids
are excluded, never arbitrarily selected. Scope all admitted entries to the exact
configured endpoint and canonical wire. Usage counts never enter this path.

Use the existing query_engine_http implementation unit and its httplib dependency;
do not add textual third-party includes to interfaces or another service layer.
The request uses 1-second connection/read/write timeouts, no retries, a
4-MiB decompressed body limit enforced inside the content receiver, and no
redirects. A scoped deadline guard calls client.stop() after two seconds of
cumulative elapsed time, including slow headers and continuously arriving body
chunks; it is cancelled/joined as soon as GET returns. This is a transient
request deadline, not a persistent worker. The HTTP library holds a socket mutex during synchronous DNS, per-address
connection attempts and TLS initialization; stop() takes the same mutex. Those
socket-establishment stages can outlive cancellation. The two-second cumulative
guard bounds established-connection HTTP transfer, not the entire startup
operation. Record that limit explicitly. A non-2xx, oversized body, parse error or timeout yields no discovered
entries and the existing resolver proceeds. Do not write response bodies, headers
or secrets to normal logs. The locally captured investigation response is not a
runtime cache and stays gitignored with restricted permissions.

### Snapshot and precedence

Add runtime-only metadata limits to existing ModelContextConfig. QueryEngineConfig
holds an immutable internal ModelMetadataSnapshot containing limits and the exact
effective endpoint, canonical wire, API key and auth token identity. The snapshot
itself records an attempted load, including an empty result. Preparation compares
all identity fields; copying a prepared config and changing any of them discards
the prior snapshot and requests fresh metadata. Tenant paths are part of endpoint
identity. This is session-local, never logged or persisted. No boolean-only reuse
marker is permitted. A prepared config reuses its matching snapshot when native
tools and QueryEngine are constructed. Preparation
runs once after endpoint/credential/wire selection, before tool-context snapshots,
in both CLI routes and assemble. Direct QueryEngine construction prepares if
needed. Resolution stays pure and does no networking. No process-global cache is
introduced; each runtime root/session prepares its own policy. Existing immutable
native context propagation requires no separate agent network client.

For context: scoped explicit profile, generic explicit profile, exact scoped
metadata, applicable official catalog, then configured fallback. For output cap:
explicit profile caps first, metadata cap if present, applicable catalog, then
none. A context-only metadata record must not erase an independent known output
cap. Request output remains the RFC 0028 policy, distinct from context capacity.
Provenance uses metadata so tests and diagnostics distinguish detection from a
fallback. All string-based source consumers must tolerate this added value.

### Configuration and interface behavior

Keep contextWindowSize as an advanced global fallback and existing explicit
values readable. A fresh default save omits it; an existing explicit value is
preserved. Explicit command writes of the default must still persist. Remove
its editable ordinary-settings control and draft-state copying; the footer
already reports the active resolved capacity. Preserve themes, terminal rendering
and other controls. Existing truecolor goldens do not change; add a real PTY case
whose displayed denominator comes from server metadata without file capacities.
After verification, back up and remove the redundant 200000 fallback from the
user's settings without adding a manually maintained per-model capacity.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Server capacity metadata | Query HTTP and runtime preparation, compact policy, config/UI, tests and docs | done | 12 added tests reconcile 3128 to 3140; dual Werror builds pass; release coverage: 3113 passed, 25 skipped, 2 disabled, zero failures; all 26 debug first-pass fixture failures pass exact-name rerun; 34 debug snapshots and 13 focused checks pass; real CLI shows 1048.6K without manual capacities; graph/RFC/inline gates and independent graduation review pass |

## Production Readiness Review

- Correctness: controlled HTTP GET tests cover exact values, both wires,
  credential precedence, prefix/v1 handling, duplicate/invalid fields, failures,
  explicit precedence, switching and native policy inheritance. Retain existing
  model-budget and generation-default tests. Baseline 3128, reconcile added tests.
- Startup behavior: 1-second I/O timeouts, a 2-second cumulative cancellation
  guard, 4-MiB streaming body limit, no retries, no redirects and no global cache
  or long-lived worker. Synchronous DNS and library socket establishment are not strictly bounded by
  that two-second guard. Unsupported services continue normally. A short
  startup delay is accepted; record the bound and test the failure path.
- Build: existing module implementation units and dependency edges only; no
  Ninja concurrency caps. Run core/UI graph and inline checks. Hosted CI and
  Linux PSS cannot be measured locally and remain explicitly unverified.
- Rollback: revert only RFC 0029 changes while preserving the already verified
  RFC 0028 diff; JSON fields and sessions remain compatible. Retain a restricted
  backup before user settings edits.
- Observability: source=metadata is host-only provenance, no new persisted wire
  fields. Existing prompt dumps remain business-request evidence, no secrets or
  entire model lists are added to logs.
- Documentation: update context-compaction guide, decision register and RFC index.
  No AGENTS/CLAUDE build or path convention changes. Remove obsolete UI code.

## Rollout and rollback

Independent design/PRR approval precedes production edits. Implement in the current
uncommitted branch with disjoint file ownership, then verify native tests, both
build presets, real PTY and independent review. This local history is tracking;
no external issue or commit is requested. Rollback only this correction, leaving
RFC 0028 behavior and unrelated concurrent edits intact.

## Drawbacks

Services without metadata incur a best-effort startup request and keep an
estimate. Established-connection HTTP transfer is cancelled after two seconds, but
synchronous DNS and library socket establishment may outlive the guard.
Only an explicit documented response shape is supported, not every provider's
extensions. A session snapshot can become stale if a service changes capacity
mid-session; restart refreshes it. A service's declared capacity is trusted data,
not runtime proof that arbitrary million-token requests will be accepted.

## Alternatives considered

1. Do nothing: continues ignoring the exact capacity already returned by service.
2. Ask users to maintain profiles or set the global fallback to 1M: fails automatic
   switching and can incorrectly assign one model's capacity to all models.
3. Add a hardcoded gateway/model alias: addresses only this instance and duplicates
   service-owned facts; unnecessary when metadata exists.
4. Reuse bootstrap cache or add a global discovery service: introduces identity,
   expiry and lifecycle complexity; a root-session snapshot is sufficient.

## Testing and verification plan

Use existing model-context, wire-transport, config/command, settings-layout and
PTY frameworks. Metadata GETs must be distinguishable from business POSTs and not
consume existing fixture replies. Add no disposable validation scripts. Record
first-pass failures separately from reruns, including environment-dependent
historical streaming snapshots. Cover same URL with changed credential and copied configs, timed slow-header/body
responses, and receiver-enforced size limits. Both presets build with default Ninja parallelism;
serial test selections together must cover the full registered set. Run RFC lint,
graph, inline and diff checks; retain source and executable hashes with evidence.

## Documentation impact

Context-compaction guide, design decision register, RFC index and local validation
record. No new command-line switch or persisted session/agent format.

## Local validation results

Evidence is retained under `build/model-context-discovery-20261019/` in this
checkout. `validation.json` records full result counts, failure history, review,
user-settings backup and limitations. `source-and-binaries.json` freezes hashes
for 35 source/test files and both CLI binaries. The reviewer independently
matched these hashes and both release selections against the current complete
3140-test registration. The build directory is local, gitignored evidence.

- Debug and release builds passed with default Ninja parallelism and Werror.
  One new test initially referenced a private engine method; it was corrected
  to observe the real tool execution input. The original failed build logs and
  subsequent successful build logs are both retained.
- The first serial debug main selection ran 3106 tests: 3053 passed, 26 failed,
  25 skipped and 2 disabled. Three existing raw HTTP fixtures incorrectly treated
  the new metadata GET as a business request. They now return 404 before
  recording, notification or scripted-response consumption; business assertions
  remain unchanged. All 26 exact failing names passed after the fixture rebuild.
  An earlier numeric-index rerun after regex exclusion did not select the exact
  failure set and is not accepted as complete evidence. The accepted native
  CTest `--tests-from-file` rerun uses `debug-failed-test-names.txt`.
- Historical streaming snapshots ran separately with their fixture model
  environment: all 34 passed in each preset. Other tests ran with `LOOM_MODEL`
  unset so configuration-precedence tests retained their intended environment.
- Release main plus snapshots cover all 3140 registrations with disjoint test
  names: 3113 passed, 25 skipped, 2 disabled and zero failed. The 13 focused
  metadata and PTY tests also passed. The 12 additions are 10 native tests and
  2 real CLI PTY cases, covering automatic capacity, switching and native
  inheritance as well as HTTP/auth/path/error behavior.
- RFC lint, core/UI graph and inline checks passed. Source and binary hashes
  match the executed artifacts. Hosted CI and Linux PSS remain unverified.
- The configured live service returned `context_length: 1048576`. After backing
  up the user settings and removing only its redundant `contextWindowSize`, a
  real CLI launch displayed `11.8K/1048.6K`; all 24 other settings entries were
  preserved and no per-model profile was added. Restricted raw captures and
  extracted footer text are retained. No real million-token generation or load
  test was performed. Socket-establishment cancellation limits remain as stated
  in the proposal.

`agent:/root/generation_review` independently approved design/PRR,
implementation, fixture corrections and final graduation. This is a completed
local implementation; no commit, push, deployment or hosted CI run is claimed.

## Open questions

None. The observed server context_length is authoritative for this endpoint;
absence or invalid data uses the documented existing fallback.

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-19 | — | Opened provisional after live metadata investigation | Uncommitted | GET /v1/models returned exact context_length=1048576; current resolver uses 200000 fallback |
| 2026-10-19 | — | Design and PRR approved after identity/deadline corrections | Uncommitted | agent:/root/generation_review approved exact identity reuse, 1s I/O / 2s cumulative cancellation / 4MiB receiver limits and stated DNS limitation |
| 2026-10-19 | A | Implementation started | Uncommitted | Preserve RFC 0028 baseline; independent file ownership and native test coverage |
| 2026-10-19 | A | Implementation and fixture adaptations independently approved | Uncommitted | agent:/root/generation_review approved implementation and three existing raw HTTP fixture corrections; first debug main had 26 failures, all passed the exact-name rerun; numeric-index rerun is excluded as complete evidence |
| 2026-10-19 | A | Graduation approved; phase completed | Uncommitted | agent:/root/generation_review verified 35 source/test and 2 binary hashes; 3140 release registrations covered: 3113 passed / 25 skipped / 2 disabled / 0 failed; debug snapshots 34/34 and focused 13/13 passed; dual builds, graph, RFC and inline gates passed; live CLI without manual capacity showed 1048.6K; hosted CI/PSS and real 1M request unverified |
| 2026-10-19 | A | User authorized publication with RFC 0028 | p/zode/automatic-model-defaults | Publish the independently verified automatic model policy; prior local-only history and validation limits remain unchanged |
