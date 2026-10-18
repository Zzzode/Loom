---
rfc: 24
title: Interactive Subagent and Team Agent Sessions
status: implemented
owners: "@Zzzode"
reviewers: ["agent:/root/interactive_design_review", "agent:/root/interactive_prr_review"]
created: 2026-10-18
last-reviewed: 2026-10-18
tracking: https://github.com/Zzzode/Loom/issues/4
---

# RFC 0024 — Interactive Subagent and Team Agent Sessions

## Summary

Enter on a selected native Agent instance opens its interactive conversation in
Loom's main transcript/editor area. Reuse the existing message renderer, editor,
Agent execution and retained sidechain, while keeping the root QueryEngine and
notification owner unchanged. Support direct text input, bounded live output,
queued running-worker guidance, same-ID ended-worker continuation, targeted Stop
and explicit return to Main. Ordinary Subagents and native Team Agents share the
interaction but retain their distinct roles and lifecycle. Instance metadata is
a secondary Details action. This extends RFCs 0021/0022 and selectively replaces
RFC 0023's Enter-to-detail rule after phase A graduation; earlier implementation
and verification records remain accurate history.

## Motivation

### Evidence

RFC 0023 local acceptance registered 2874 tests per preset (2849 passed, 23
skipped, 2 disabled). Its real configured-endpoint PTY proved inspection and
same-ID Continue (four retained entries, four appended, result 150), but Enter
opens a bounded text inspector, not a usable session. The user explicitly
requested selection, Enter and direct conversation for both kinds of Agent.

Source inspection on 2026-10-18 established:

- `instance_browser.cpp` opens `detail_open_` on Return. App exposes no child
  composer or active conversation target.
- `NativeAgentRecord.sidechain_entries` contains structured messages for both
  ordinary and native Team workers. No separate child QueryEngine or SessionStorage
  exists. `agent_id` is the child identity, not a root session UUID.
- Root Render/SyncState write the root MessagesStore; root interrupt aborts the
  root engine. Merely replacing visible rows would route input/cancel incorrectly.
- The worker drains pending messages before a model turn, then can break at
  end_turn. A send racing finalization needs an atomic acceptance boundary.
- Agent resume startup reads then upserts a record; a new mailbox entry could be
  overwritten unless admission/merge is protected by the Store lock.
- Existing model SendMessage resume dispatch bypasses the new host QueryEngine
  policy path; it is not the implementation seam for this UI feature.

| Metric | Current | Target | How measured |
|---|---|---|---|
| Enter destination | Details inspector | Target-bound conversation | Native browser/App tests and live PTY |
| Root effects from child view/input | No child view | Zero root conversation/session mutation from navigation or child input | Exact root-history/ID assertions |
| Native categories with direct text interaction | Direct ordinary ended Continue only | Ordinary and native Team, running and ended | Query/Agent/Store fixtures and real endpoint |
| Accepted messages lost at startup/finalization | No UI guarantee | Zero; processed or visibly pending | Barrier-controlled native races |
| Selected-agent cancellation affecting parent/sibling | No child composer | Zero | Stop-token tests and overlapping real Bash processes |
| Registered baseline | 2874 | Baseline plus exact named additions, no deletions | Complete Debug/Release reconciliation |

### Comparable implementations

Official sources rechecked 2026-10-18:

- [Claude Code teams](https://code.claude.com/docs/en/agent-teams#talk-to-teammates-directly)
  select a teammate, Enter its transcript, then type directly. Some built-in
  commands still belong to the lead and require safeguards.
- [Claude Code subagents](https://code.claude.com/docs/en/sub-agents#resume-subagents)
  retain context, accept follow-up messages, and expose direct transcript input
  for explicit resumption after a user stop.
- [Codex subagents](https://learn.chatgpt.com/docs/agent-configuration/subagents#managing-subagents)
  document CLI thread switching and inspection; web lists can be read-only.

These establish useful patterns, not full command/permission parity across
clients. Loom must name its own supported capabilities and exact targets.

## Goals

- G1. Select ordinary or native Team instances and Enter a real structured
  conversation, including during parent execution; metadata remains accessible.
- G2. Keep independent held root/child transcript/editor state, including draft,
  cursor, history, editor selection, disclosure and scroll; returning restores Main.
- G3. Direct plain text reaches exactly the selected owned instance. Running
  messages are queued without spawning a duplicate. Ended messages resume the
  same ID through current policy/hooks; cancelled workers need explicit user input.
- G4. Parent and sibling work continues while viewing, messaging or stopping a
  child. Viewing never consumes notifications or changes root session identity.
- G5. Keep permission decisions bound to their original request. A different
  visible transcript cannot confer authority or hide a pending request.
- G6. Show existing tool/result/thinking/text messages through the normal UI,
  with event-driven bounded in-memory partial assistant text where available.
- G7. Preserve 40/60/112-column input/status usability, failed-history folding,
  exact classification and old histories. Reject unsupported actions explicitly.

## Non-Goals

A new scheduler/event bus/session database, standalone child QueryEngines,
renaming persisted identities, unifying Tasks and Agents, automatic retry or
budget escalation, new permissions/settings/models, arbitrary cross-session
messaging, external pane reattachment/process control, immutable attempt lineage,
child image/file/IDE attachment workflows, App-level child transcript drag-copy
and hyperlink launching, native Agent AskUser enablement, and broad root UI
cleanup. Text and bracketed paste are supported. Unsupported actions must give
an explicit reason and never fall through to Main.

## Proposal

### Ownership, shared changes and dependency direction

App owns active view identity and target-bound callbacks. Screens owns held
ReplScreen state/components and submit acceptance. Existing message projection
owns rendering structured core Messages. QueryEngine owns host launch policy;
AgentTool owns worker execution and streaming; NativeAgentStore owns atomic
mailbox acceptance, lifecycle and cancellation. No feature imports App/screens
upward. Child ReplScreen composition stays in App, not the lower feature module.

Necessary internal shared changes are additive browser callbacks, a checked
submit seam that preserves rejected drafts, transient native input/live-text
state, and scoped host input/Team continuation. UI-only control would bypass
policy or race worker ownership; a duplicate engine/scheduler would introduce
conflicting execution and permissions. Existing external tool schemas, persisted
root session formats, XML envelope and command meanings remain unchanged. The
native result correction below restores the existing standard error signal in
sidechains and both wire backends. Rollback removes new entrypoints/helpers
without data migration.

### Native tool-result error fidelity (live review finding)

The real cancelled Team session exposed a successful green tool card for a
cancelled Bash call. The native API ContentBlock lacks an error field, and its
worker, sidechain codec and wire adapter therefore lose the execution outcome.
The existing UI already consumes typed per-call errors. Inferring errors from
text or marking every call failed when the Agent is cancelled would incorrectly
reclassify earlier successful tools.

Add `is_error = false` at the end of native ContentBlock. Actual failures,
permission refusals and cancellation placeholders set it; completed preceding
results retain their own outcome. Sidechain and Messages serializers emit the
standard optional `is_error` field only when true; this is an additive native
sidechain shape correction. Absent fields in old records read as false. Root
Session, TaskEvent, identity and settings formats do not change. The OpenAI
adapter forwards the flag to its existing error-prefix mapping.
No new role, setting, migration or public tool argument is introduced. Old
histories cannot recover a signal that was never recorded and are not rewritten.

The shared assembly reader also discards later tool results in a native batch.
Correct its owning block parser to retain existing ToolResultBlock values and
use the existing UserMessage representation for multiple results or mixed blocks.
Keep the legacy first-result ToolResultMessage fast path for a single result,
including its document suffix. A child-only split adapter would duplicate JSON
classification and conceal this demonstrated shared codec defect. Affected
readers are App history/child projection, SDK prior lines and server seed lines.
Tests must prove per-call success/error matching, mixed content retention, old
single-result/document compatibility and both native wire representations.
Rollback keeps these writer/reader changes together; no data migration is needed.

### Dynamic-skill subscription lifetime (native regression finding)

The existing two-App subscription test crashed during destruction. Its native
macOS stack identifies the dynamic-skill unsubscribe lambda writing through a
vector iterator invalidated by a second subscription's growth. This owner bug
precedes interactive views but directly breaks the required App lifecycle gate.
Correct only the Skills callback owner: capture a stable slot index and clear
that slot under its existing mutex. The callback table only appends and clears
individual slots; it never erases, reuses or compacts them. Preserve callback
order, exception policy, emit locking and the public subscribe/unsubscribe API.
No App destructor workaround, new observer service or persistence change is
needed. Native coverage forces capacity growth and verifies out-of-order and
repeated unsubscription without removing live listeners. Re-run the original
two-App case as well as both complete suites. The owner and its regression
roll back together.

### TeamStore concurrency correction (implementation review finding)

Implementation review confirmed a blocking owning-module defect: TeamStore has
no mutex and returns escaped raw Team pointers; even get_by_id_or_name can load,
insert or replace records. New host membership reads and Working writes can race
sibling finalizers that update/persist a member, independent of root busy state.
This is directly required for native Team interactive correctness, not optional
cleanup. A UI-only guard, a mutex around only get(), or a new host-only lock would
leave existing writers and escaped pointers outside the invariant.

Correct the owning `loom.tools.team` Store directly: serialize container/record
operations and persistence under one owner mutex, use private locked helpers,
and return independent Team snapshots from public create/get/get_by_id_or_name.
All mutations (membership, status, task assignment/completion and deletion) stay
inside Store methods; no raw pointer escapes a lock. Avoid recursive_mutex and
an external service/adapter. Move changed Store implementation bodies to a .cpp
unit; module identity and ownership remain intact. Host membership can use a
locked predicate or value snapshot, never a borrowed live record.

Audit and migrate affected internal C++ callers: native-team construction and
runtime dispatch, teammate start, SendMessage roster/broadcast, team deletion,
Agent finalization and host query/admission, plus their native tests. Do not
change public tool schemas, team JSON, assignment semantics, errors or ordering.

Existing tests observing a live pointer must explicitly obtain a fresh snapshot;
do not disguise a copied record as the old live pointer contract. The existing
exported C++ module is an internal CLI implementation seam; its by-value API
change has build-time migration, not a persisted or external protocol migration.

Verification adds synchronized concurrent member status/persistence, membership
lookup and add/remove/snapshot regressions plus native Team continuation with a
running sibling. Full dual builds reveal all affected callsites. TSan is not
available in the local configured workflow and must remain unverified unless
run. Roll back this owner correction and its caller migrations together, before
removing the host feature. The user-authorized native Team workflow and the
proven root cause justify this smallest shared owning-module correction.

### Native launch identity correction (configured-endpoint finding)

The existing `team_create(start_native_agents=true)` path launches roster members
through a background Agent without a teammate `name`. Unlike the named teammate
executor path, it previously persisted no backend identity, so a real native
member failed the new interactive capability check. Keep that check strict.
At the actual background execution owner, label a fresh attached-registry launch
as `in-process` only when its exact ID already exists in the named team's roster
and backend, task and pane metadata are all absent. Do not backfill unknown
persisted resumes or infer capability from an inherited team name alone.

For a fresh ordinary Agent without explicit team input, inherit the leader team
only for an existing named teammate request or exact roster identity. An ordinary
Subagent launched after team creation must remain a Subagent, with direct input
and same-ID continuation. Keep the original teammate flat-roster and background
spawn restrictions based on the caller's collaboration scope, independently of
the child's role. Preserve explicit team input, empty-name validation, legacy
records and external backends. No new persisted role field is needed.

Backend identity must not grant permissions: retain the existing named teammate
tool exception only for nonempty teammate name, team and in-process backend.
Registry-native roster members keep their previous background tool set on fresh
launch and continuation. Test the public registry launch, live queue, Stop and
same-ID host continuation, alongside unknown/external/nonmember rejection and
wire tool-set assertions. No JSON schema, public tool argument or setting changes.

A real native sidechain also exposed its initial prompt twice: an early diagnostic
transcript write materialized a legacy entry before the structured prompt. Simply
reordering two Store mutations still lets an observer reload a synthesized
transcript before the diagnostic append. Commit the structured initial message
and its diagnostic transcript entry atomically in the existing Store owner:
append_sidechain_message accepts an optional diagnostic entry, copies it under
the same mutex and persists once. Only the initial prompt uses this argument;
existing callers keep their default behavior. Do not deduplicate user text or
add an external lock. Keep fork and deferred/no-registry behavior, old history
and model message order unchanged. Public ordinary/registry fixtures verify one
initial prompt in memory/disk/wire and one diagnostic, with and without reload.

### Identity and retained views

The view identity is the pair `(root_session_id, agent_id)`. Main is an explicit
root view. Never replace `current_session_id_`, call root `/resume`, mutate root
engine conversation, or reinterpret parent_session_id as a viewed-child ID.
Keep root ReplScreenState/component held; create a separate held child screen
state/component only when opened. Retain each opened child's draft and transcript
state for this root session. Root projections continue into the root store;
child projections refresh only from their matching native record generation.
Never swap App's screen_state_ pointer behind an existing captured component.
Clear child views when the owning root session actually changes; pending actions
capture immutable root/child identity and reject stale scope.

Parse native structured sidechain entries into normal core Messages using the
existing codecs/projection. Do not use the resume-history filter, which discards
unresolved tool calls, or reconstruct messages from human-readable transcript
lines. Keep stable stored entry UUIDs; reconcile tool results by tool_use_id only
within the selected conversation. Legacy/malformed entries produce bounded safe
fallback text or explicit unavailable history, without following record paths.

### Navigation and commands

Within the focused instance list: Up/Down selects; Left/Right changes category;
Tab changes Active/History; Enter opens the selected native conversation.
`i` opens the existing bounded Details inspector, which also retains Continue/Stop.
`/subagents <full-id>` opens the conversation. The modal closes before navigation.
The child header identifies Main > Subagent/Team Agent, native status and name,
and has a visible return hint. `Alt+Left` and `/back` return to Main; `/stop`
and Ctrl-C target only the child. Escape in a child view returns to the instance
list, without cancelling work. These routes are view-local; Main keeps its current
key behavior. `/exit` and Ctrl-D remain explicit application exit, never a hidden
root message. `/subagents` opens the list in either view.

Other slash commands and shell/attachment modes in a child view are rejected
with a target-specific notice and retained draft. In particular `/clear`,
`/compact`, `/rewind`, `/model`, `/resume` and local `!` commands must never
silently operate on Main. Skills/tool use initiated by the child remain within
its existing definition and permission policy; this phase does not interpret
slash skills locally. The child uses normal multiline text editing and scrolling. App-level transcript
drag-copy and hyperlink launching remain Main-only in this phase; root selection
highlights must never paint over child output.
The checked submit result prevents the existing unconditional Return clearing
from losing rejected text. Delayed clipboard/IDE attachment workers stay bound
to Main; child attachment shortcuts explicitly refuse them. Text paste remains
owned by the child editor. No new global key steals cursor/history arrows while
the ordinary editor is focused.

### Native input and execution ownership

| Native state | Direct text input | Exact behavior |
|---|---|---|
| Running and accepting input, owned here | Queue | Store atomically checks ID, root owner, registry token, native backend and non-cancelling admission, then appends to existing mailbox. Show queued status until worker consumes it. No new Agent invocation. |
| Starting, finishing, cancellation pending | Refuse for now | Explicit reason; preserve draft. Do not claim delivery. |
| Completed / Failed, cleaned up | Continue | Same-ID host Agent launch through QueryEngine permission, PreToolUse/PostToolUse and registry. Current definition/budget applies. |
| Cancelled, cleaned up | Explicit user Continue by text | Same policy path; no mailbox or model message automatically wakes a user-stopped worker. |
| Parent busy, ended child | Refuse for now | Explicit parent-operation reason; preserve draft. Existing root engine launch state is not thread-safe for concurrent host launches. Viewing, running-worker queue and Stop still work. |
| Foreign/missing owner, external pane, unsupported nested launch | Refuse | Exact capability reason; never launch a native substitute or change ownership. |

Native Team input uses the same worker and history path as ordinary input.
Extend the host resume constraint to capture immutable category/backend/team
metadata, alongside ID/type/root/cwd. Validate before dispatch, after both policy
rewrite points, and atomically during admission; hooks cannot redirect the child
or turn a resume into a fresh spawn. Preserve original team metadata and result
routing. Ended native Team continuation requires its existing team/member to
remain resolvable; removed membership gives a specific refusal. On admitted
resume, project that same member Running, preserving member count and task
assignment; finalization uses existing Done/Error routing. Never create a new
team/member/task merely to continue. Native tests cover Running-to-terminal and
no duplicate membership. Native nested histories can be inspected and existing owned work stopped;
ended nested continuation remains unavailable until original depth/backend
context is durably reconstructible. Pane-only teammates remain in `/teams` with
an explicit explanation; this phase does not claim their terminal is attached.

Worker input acceptance is transient Store-owned execution state. Enable it only
after record startup is complete. At a normal end_turn boundary, under the same
Store mutex either observe pending messages and keep the loop running, or close
acceptance before final publication. Finalizers close it on error/cancel/budget
exit. Thus a late send is either accepted into the mailbox or rejected; it is
never overwritten by startup. Budget exhaustion may leave an accepted queued
message pending: show the count/content with an explicit continuation-required
state, preserve it for an explicit continuation, and do not promise processing.
Retained queued input is FIFO: if A and B were accepted before a budget exit,
then explicit Continue text C follows A and B in the retained sidechain and next
model request, exactly once. Worker startup must drain retained input before
appending the new continuation prompt. Add an exact A/B/C ordering regression.
The existing model SendMessage semantics are not broadened or silently rewritten.

### Live projection and permissions

Add transient assistant text/activity state to the native Store, capped at
64 KiB of UTF-8 per agent. Clip at character boundaries, retain the latest tail
with an explicit truncation notice, and test oversized/multibyte deltas. Update
it by the existing worker stream callback and clear it at turn commit/end.
It is excluded from runtime JSON, sidechain and model context. Event notifications
reuse the existing Store observer/coalesced App wake. A stable transient row is
replaced by its committed message without double display. No ticker, second
reader, UI polling, or per-render component reconstruction is introduced.

Permission/question overlays remain application-level. The request's captured
callback, not the visible view, determines the recipient. An actionable root
queue overlay temporarily takes display/input priority over the child screen;
retain the child draft and selected view. Keep the request source visible using
known source labels; where the existing callback has no actor metadata, identify
the actual tool/server rather than invent an Agent identity.

Source inspection confirmed a single-slot concurrency defect, so local repair
is required for the reachable tool-permission and MCP-elicitation responders.
Serialize each request kind across reset, UI dispatch, wait and exact-ID cleanup;
associate each active slot with a monotonically unique request token. Both
resolve and delayed show/cleanup closures validate that token; late callback A
cannot resolve/remove request B. A queued caller rechecks sticky shutdown before
showing any overlay. `abort_all` wakes active waiters and queued callers return
denied without a new prompt. Do not use PopBottom to remove an unidentified
request. Native barrier tests exercise opposite responses, late callbacks,
cleanup ordering, queued shutdown and root/child view switches.

Native Agent AskUser stays prohibited by `all_agent_disallows_tool`; opening a
conversation does not broaden that policy. The root-only AskUser responder is
unchanged; this phase adds no child question support or new response coordinator.

Stop during a still-unanswered approval/elicitation requests cancellation but
may remain pending until that reachable original prompt is answered or denied;
show this state accurately, never mark the worker stopped early. Returning to
Main exposes that prompt; its decision belongs only to the original request.
No selected Stop calls global `abort_all` or resolves a sibling's request.
Tests deny the pending target request, verify target cleanup, and prove sibling
requests retain independent decisions. Application shutdown still aborts all
waits and joins workers. Immediate cancellation of an external MCP server's
unanswered question is not promised by this phase.

### Events, compatibility and coupling catalog

RFC 0022 completion ownership stays the original root parent_session_id. Only
normal root QueryEngine processing consumes pending TaskEvents. Viewing,
projecting child rows, input queueing, stopping and host continuation never drain
or acknowledge those events, fabricate root tool pairs, or parse XML for identity.
A root completion received while a child is visible updates the root projection,
not the child's conversation. Same-ID repeated terminal events retain RFC 0023
semantics. Root traces and child sidechain remain separate evidence.

Coupled updates: browser Options/Return/Details tests; App callbacks/view selection;
ReplScreen checked-submit path; AgentTool stream/commit/finalize and Store transient
state; host ToolInput constraint and Store admission; sidechain decode and UI
projection. All new implementation bodies stay in .cpp units. Truecolor impact is
limited to the child breadcrumb/status and list hint; no palette token/RGB or
unrelated golden refresh. Existing session/runtime files load without migration.

### Child message presentation and narrow status

A native worker accepts input independently of Main's query-running flag. The
shared compressed-chain renderer assumes that flag and a tail-position chain,
which can falsely label a still-running native tool as Worked when guidance is
queued. A held child view therefore uses the existing per-view option to show
individual tool/thinking/message cards. Main keeps its normal compression and
query lifecycle; do not introduce a fake busy query or change global chain rules.
Render category and status before a potentially long child name, so the state
remains visible at 40 columns. Keep return controls and the parent-busy reason
within the existing header/footer budget; full identity remains in Details.

The team-create success diagnostic is redundant with the launch result and native
metadata. Remove that transcript append: writing it before the first structured
message can materialize a false legacy User and pollute same-ID model context.
Keep the actual initial prompt, native status, counters and old histories intact.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Scoped conversation views | Structured projection, held screen/editor, list navigation, return, command/cancel routing | done | Both complete native suites passed; actual Enter for ordinary/Team, root draft retained, per-call success/error and queued guidance verified; 40/60/112-column captures, permission-priority regressions and graph/inline checks approved by agent:/root/interactive_design_review |
| B | Direct native interaction | Atomic running input, ordinary/Team ended policy continuation, live text, finalization races | done | Native barriers/policy/metadata tests passed; real queue processed once, Stop removed only Team PIDs, both same-ID histories retained 3-entry prefixes in 7 entries, root history stayed 16/16 and 21/21, one Team member retained, failure resumed to 150; agent:/root/interactive_prr_review approved |
| C | Integration and graduation | Docs, complete dual builds/suites, configured-endpoint real PTY | done | 2874 baseline +49 names, no deletions; each preset 2923 registered / 2898 passed / 23 skipped / 2 disabled / 0 failed; default Ninja -Werror builds; 53 source/test inputs and 218 executable hashes unchanged; final real PTY and preservation audit approved, with the unowned .gitignore observation retained separately |

## Production Readiness Review

- Correctness: extend project-native Store/Query/Agent/App/browser tests; test
  admission boundaries with barriers, not timing-only sleeps. Expected total is
  2874 plus actual registered additions, reconciled by name; no deletions.
- UI/goldens: reuse native message/editor renderer; test caret/footer, tool rows,
  selection and root/child isolation at 40/60/112 columns. Independently review
  changed expectations; preserve unrelated theme/accessibility WIP/goldens.
- Build: configure/build Debug and Release with -Werror and default Ninja
  parallelism, full unfiltered CTest -j1. Disclose first-pass failures separately
  from fixes/retries. Record local incremental wall time, compiled-object fanout
  and affected BMI byte sizes. Linux compiler PSS and hosted macos-14 cold/warm
  timings are unavailable here; do not substitute RSS/BMI for them. ASan and
  other platform runs remain unverified unless executed.
- Architecture: graph core8/ui9/store/TLL and inline-body checks before/after;
  zero new cycles/upward edges/dead imports/interface inline bodies. No scheduler,
  new dependency, global settings or protected common-widget refactor.
- Lifecycle: observer callback queues close before teardown; retained view
  callbacks capture root+child IDs; launch workers are joined; permissions remain
  reachable; input and cancellation never target the wrong engine.
- Rollback: remove child navigation, then new host input/live projection helpers;
  RFC 0023 inspector/history still works. No stored-data migration; native error
  fidelity rolls back with its writer/reader pair.
- Observability: native sidechain plus root messages/dump prompts retained;
  queue acknowledgement differs from processed input and terminal success.
- Documentation/deletion: update RFCs 0021–0023, docs index, instance guide and
  coupling register. Remove obsolete Enter-to-details-only hint, not historical
  records. AGENTS changes only if a new repository convention is introduced.
- External effects: use privately staged existing endpoint credentials and
  disposable fixture work; delete temporary credentials, preserve original hash
  and mtime. No REA, commit or push. Tracking issue required before implementable.

## Rollout and rollback

Independent design and PRR approvals precede any production implementation.
A and B can be developed with disjoint UI/runtime ownership after both gates;
integrate and graduate A then B then C. Freeze all current dirty files first;
use the attached managed checkout carrying RFC 0023 as verification baseline,
copying only this task's deltas while preserving unrelated work. Record complete
source/fixture/executable hashes. Revert UI entry and checked-submit seam before
runtime helpers; existing records and RFC 0023 Details remain readable.

## Drawbacks

Retained per-view screen state costs memory; child history decode has a bounded
per-change cost. Root/child command and permission ownership needs explicit
routing. Full attachment, nested resume and external-terminal parity remain
unsupported. Ended continuation is serialized with parent engine operations;
active queueing does not guarantee an extra model turn when the budget is spent.

## Alternatives considered

1. Keep Details plus Continue only: already verified but does not satisfy direct
   conversation, familiar message rendering or target-local input.
2. Put child messages into root engine / call root resume: conflates view and
   execution identity, changes root history and misroutes cancellation/events.
3. Create a QueryEngine and session store per opened child: duplicates active
   workers, budgets, policies and histories; viewing must not start execution.
4. Raw UI SendMessage/registry dispatch: bypasses host policy and cannot resolve
   startup/finalization mailbox races or distinguish external workers safely.

## Testing and verification plan

Baseline is the frozen RFC 0023 selected tree: 2874 registered tests. Preserve
all baseline names; list exact additions and disabled/skipped counts. Run focused
native tests, graph/store/target/inline checks and RFC lint before dual default
parallel builds and complete serial suites. Preserve first failures and fixes.

Real configured-endpoint acceptance runs ordinary and native Team fixtures with
separate marker files. Open each with Enter, observe actual text/tool output,
send a distinct follow-up and return to Main with its draft intact. Keep parent
and child Bash processes overlapping, queue guidance, stop only selected child,
verify sibling/parent result and absent cancelled end marker. Inspect a controlled
budget failure in History and explicitly resume the same ID. Capture live PTY
pixels at wide, 60 and 40 columns; browser-backed PTY transport is acceptable if
identified, static synthetic rows/replay-only output and REA are not. Retain
actual requests, native records and process/marker evidence. Clean temporary
credentials and fixture/server processes; do not commit.

## Local implementation measurements

Default Ninja incremental builds 13 and 14 completed with -Werror in both
presets. Build 13 compiled 62 objects / 12 interface objects per preset in
58.34 s Debug and 183.12 s Release; build 14 compiled 12 objects / 8 interface
objects per preset in 57.08 s and 169.50 s. These are local incremental waves,
not cold/warm hosted timings. Interface edits still fan out through BMI imports.

The following are PCM file byte sizes, not compiler PSS or runtime memory.
The original nine producers use the frozen selected baseline; error-fidelity
and Skills rows use their separately captured pre-correction samples.

| Producer | Debug before → after (bytes) | Release before → after (bytes) |
|---|---:|---:|
| `loom.ui.screens.repl_screen` | 36601316 → 36601316 | 36596840 → 36596840 |
| `loom.ui.screens.repl_state` | 17975936 → 17994072 | 17973868 → 17991996 |
| `loom.tools.agent_runtime` | 2914500 → 2917576 | 2914516 → 2917592 |
| `loom.ui.app.app` | 45383728 → 45637152 | 45336084 → 45589500 |
| `loom.ui.app.app_dialog_registration` | 24788 → 24788 | 24808 → 24808 |
| `loom.ui.app.app-impl` | 614072 → 614040 | 613788 → 613756 |
| `loom.ui.app.permission_coordinator` | 809568 → 365716 | 809584 → 365732 |
| `loom.ui.features.agents.instance_browser` | 12974948 → 12975240 | 12974964 → 12975256 |
| `loom.query.query_engine` | 2613164 → 2613836 | 2613136 → 2613808 |
| `loom.orchestration.agent.utils` | 892792 → 892792 | 892808 → 892808 |
| `loom.services.api.client` | 2722672 → 2722912 | 2722260 → 2722500 |
| `loom.query.assembly` | 330436 → 330436 | 330452 → 330452 |
| `loom.skills.load_skills_dir` | 5188576 → 5181372 | 5188196 → 5180992 |

TeamStore PCM was captured only after its owner correction: 1549708 Debug /
1549728 Release bytes; there is no comparable before sample. Linux producer PSS,
hosted macos-14 cold/warm builds, ASan/TSan and other operating systems remain
unverified. Detailed build waves and exact registrations are retained in
`build/interactive-agent-session-validation-20261018/`.

### Local graduation result

Independent overall graduation review by `agent:/root/interactive_prr_review`
APPROVED A, then B, then C after both complete XML reports and final live audit.
`agent:/root/interactive_design_review` independently approved UI/source and
40/60/112-column live evidence. RFC 0024 is implemented locally, uncommitted.

Canonical full suites used the original default TMPDIR, Debug then Release,
unfiltered `ctest --preset <preset> -j1`: each registered 2923 tests, with 2898
passed, 23 environment/opt-in skipped, 2 existing disabled and zero failed.
Measured wall times were 500.87 s Debug and 428.79 s Release. All 2874 baseline
names remain; exactly 49 were added, none removed, with identical preset name
sets. Final focused owner regressions were 9/9 per preset (`*-focused14.xml`).
Earlier build/focused/full failures, including the proven Skills lifetime bug,
remain separately recorded and are not described as first-pass green.

Both 53-entry source/test freezes and both 109-executable freezes had zero
post-suite drift. The real PTY used that same Release SHA
`70654cfa7a53662841fe742cd9d371e5e5935940a4c2642ff3c49756021b123e`.
Graph checks covered 642 modules / 813 units with no new module cycle, upward
edge or dead import; core8/ui9/store/TLL and inline gates passed. RFC lint passed
24/24. HEAD remains unchanged and the index empty; no commit/push or REA.
All unrelated initial work is retained. One unowned `.gitignore` delta appeared
in the shared session with unestablished provenance; it was preserved and excluded
from the verification checkout. Detailed hashes, complete results and failure
records are in `build/interactive-agent-session-validation-20261018/README.md`.

Final native acceptance proves ordinary and Team Enter, queued guidance, targeted
Stop, history/Details and explicit same-ID continuation. Both continued histories
retain three prior entries in seven total, with root history hashes unchanged
(16 and 21 lines respectively); one original Team member remains and the budget
failure resumes to 150. Cancelled tools are red/error while earlier/new successful
tools remain green. Forty-column geometry is native 40×28, not browser scaling.
The initial parent `sleep 140` independently timed out (137); the model then ran
`sleep 60` successfully and made two TaskOutput polls. Targeted Stop isolation is
proven by the earlier process snapshots, not a claimed normal initial-parent exit.
Real request dumps cover the root only; child wire fidelity uses native two-backend
tests. Approval/MCP preemption uses native regressions, not a live server screenshot.
Staged credentials and all tracked PTY/worker/server processes were cleaned up;
original settings hash and mtime are unchanged. Text-only/external/nested limits
and unavailable instrumentation listed above remain explicit.

## Documentation impact

- RFC 0021: activity selection becomes a conversation entry after phase A;
  categories/indentation/row priority and proactive delegation remain intact.
- RFC 0022: distinguish visible view identity from parent notification ownership;
  no new provenance fields; native tool errors retain their standard wire meaning.
- RFC 0023: record selective navigation/control extension, preserving all prior
  phase/graduation history; the old inspector remains under Details.
- Update docs index, runtime instance guide and coupling register; comments/docs
  remain English. No repository-wide convention needs an AGENTS change yet.

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| External pane attachment and ended nested continuation | @Zzzode | Explicitly unavailable this phase; future backend/durable-context design required |
| Child attachment workflows and command parity | @Zzzode | Text-only input and explicit command refusal for this phase; no silent root fallback |
| Hosted timing, Linux PSS and other platforms | @Zzzode | Report unavailable/unverified locally; do not claim hosted evidence |

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-18 | — | Opened provisional after user authorized interactive ordinary/Team session design | Uncommitted | Source mapping by agent:/root/interactive_ui_map and agent:/root/interactive_runtime_map; RFC 0023 baseline 2874 tests; no production implementation started |

| 2026-10-18 | — | Independent design review approved; accepted | Uncommitted | agent:/root/interactive_design_review; P1 request ownership and P2 pending FIFO resolved; native AskUser remains prohibited |
| 2026-10-18 | — | Independent PRR approved; implementable | Uncommitted | agent:/root/interactive_prr_review; request generations/exact cleanup, 64 KiB live bound, FIFO and existing Team membership invariants resolved; tracking issue 4; provisional RFC lint 24/24 passed |
| 2026-10-18 | A/B | Implementation started after both independent approvals | Uncommitted | Disjoint UI/App and runtime/query ownership; A then B then C graduation required; baseline source/WIP and 2874 native registrations frozen |

| 2026-10-18 | B | Independent implementation review found unsynchronized escaped TeamStore pointers | Uncommitted | agent:/root/interactive_prr_review P1; host root-busy guard cannot serialize background sibling finalizers; owning-module snapshot/transaction correction proposed and scoped above before implementation |

| 2026-10-18 | B | TeamStore owning-module concurrency correction independently approved before implementation | Uncommitted | agent:/root/interactive_design_review and agent:/root/interactive_prr_review APPROVED; same owner lock, value snapshots, atomic existing-member Working mutation, no TeamStore-to-NativeStore callbacks, necessary caller migration and concurrency regressions |
| 2026-10-18 | A | UI implementation review requested active-draft permission-preemption correction | Uncommitted | agent:/root/interactive_design_review P1; selecting root alone does not override existing typing/animation suppression; local effective request-priority parameters and two-draft regression required |

| 2026-10-18 | B | Configured-endpoint native Team identity correction independently approved | Uncommitted | agent:/root/interactive_prr_review APPROVED; actual team_create native roster startup lacked backend metadata; fresh attached native owner plus exact roster proves in-process identity; named-role tool exception remains separately constrained; real registry and negative-path tests required |

| 2026-10-18 | B | Real sidechain duplicate initial prompt source correction independently approved | Uncommitted | agent:/root/interactive_prr_review APPROVED; structured initial prompt must precede diagnostic legacy fallback; unchanged old history/model order, no UI text deduplication; public ordinary/registry-native wire regressions required |

| 2026-10-18 | A | Full native PTY gate found approval status transition regression; correction approved | Uncommitted | agent:/root/interactive_prr_review APPROVED; accepted tool response removes only its exact dialog on the same UI event, while token lifecycle finish remains worker-owned; preserve late-response isolation and existing OSC working/question sequence; no common-widget or protocol change |

| 2026-10-18 | B | Independent review rejected inherited team context as ordinary child identity; minimal correction approved | Uncommitted | agent:/root/interactive_prr_review P2; only named/exact-roster fresh children inherit implicit leader team, caller-scope spawn restrictions remain; real registry team-create-before-ordinary queue/continue and unchanged permissions required |

| 2026-10-18 | B | Native observer regression disproved separate reordered prompt writes; atomic owner commit approved | Uncommitted | agent:/root/interactive_prr_review APPROVED; structured fallback can reload a diagnostic between two mutations; existing Store append accepts optional paired diagnostic under one lock/persist; only initial prompt opts in; exact-one assertions remain unchanged |

| 2026-10-18 | A/B | Real native Team legacy startup diagnostic removed after independent approval | Uncommitted | agent:/root/interactive_prr_review APPROVED; no fake initial User or resumed model-context diagnostic; real registry observer fixture checks memory/disk/raw files and fresh/queued/resumed wire |
| 2026-10-18 | A | Native child cards and narrow status priority independently approved and source reviewed | Uncommitted | agent:/root/interactive_design_review and agent:/root/interactive_prr_review APPROVED; reuse existing per-view compression bypass, Main unchanged; actual Alt-Left return assertion; 40/60/112 queued/tool rendering and narrow failed-state coverage |
| 2026-10-18 | A/B | Live cancellation-card error fidelity correction independently approved before implementation | Uncommitted | agent:/root/interactive_design_review and agent:/root/interactive_prr_review APPROVED; native ContentBlock typed error, true-only standard serialization and owning assembly batch decode; no text inference or blanket cancelled-Agent remapping; preserve successful prior calls and legacy single-result/document semantics |
| 2026-10-18 | A/B | Native error fidelity implementation independently re-reviewed | Uncommitted | agent:/root/interactive_prr_review APPROVED; five coupled owner files and three native test files; five added registrations cover true-only encoding, two wire protocols, mixed batch per-call UI status and single-result/document compatibility; full C graduation pending |
| 2026-10-18 | C | Focused two-App lifecycle crash diagnosed; minimal Skills owner correction independently approved before implementation | Uncommitted | agent:/root/interactive_design_review and agent:/root/interactive_prr_review APPROVED; native crash stack proves vector iterator invalidation in dynamic-skill unsubscribe, stable slot under existing mutex preserves API/emit semantics; deterministic growth/unsubscribe regression and original two-App case required |
| 2026-10-18 | A | Scoped conversation views graduated after independent UI review | Uncommitted | agent:/root/interactive_design_review APPROVED; native final suites, actual Enter/return, Main draft, per-call outcomes, failure Details and 40/60/112-column screenshots |
| 2026-10-18 | B | Direct native interaction graduated after independent runtime/live review | Uncommitted | agent:/root/interactive_prr_review APPROVED; exact-once queue, Team-only process cancellation, same-ID prefixes 3 to 7, root 16 to 16 / 21 to 21, original member retained and total 150; parent timeout and root-only wire limits disclosed |
| 2026-10-18 | C | Overall graduation approved; implemented locally, uncommitted | Uncommitted | agent:/root/interactive_prr_review APPROVED A then B then C; each complete canonical preset 2923 registered / 2898 passed / 23 skipped / 2 disabled / 0 failed, 500.87 s Debug / 428.79 s Release; focused14 9/9 each; +49 names / zero deletion; 53 source/test inputs and 218 executables zero drift; graph/inline/RFC24/24 and live SHA matched; no commit/push |
