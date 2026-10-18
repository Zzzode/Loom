# Subagent runtime correction and acceptance

This change fixes three concrete defects: delegated requests ignored the parent
wire protocol, exhausted runs could report success, and the non-team batch
helper returned a queue receipt as a completed result.

## Execution boundaries

`AgentTool` owns delegated execution in `src/orchestration/agent/`. The existing
wire modules now belong to the `loom_wire` CMake target, below orchestration
and query. Their exported module names and file paths are unchanged. Both
loops reuse the existing serializers and decoders. HTTP ownership stays in
the engine or the services client, including cancellation and joining active
transfers. There is no second OpenAI protocol implementation.

The primary engine also previously used the selected backend only for its
request body. Its actual HTTP path, headers and response parser were fixed to
Messages. The correction therefore includes the query HTTP owner: the complete
prepared request now determines routing and non-streaming response parsing;
OpenAI streaming uses indexed neutral deltas. Messages keeps its existing
extended event handler for signed thinking and other Messages-specific fields.

`ToolInput.api_context` is an immutable, internal execution snapshot. It carries
the selected protocol, endpoint, credentials, API version and transport timeout.
The parent injects its actual resolved session state at tool execution. Nested,
background, team-dispatch and message-triggered local resume paths forward the
snapshot. Explicit empty credentials remain empty; they do not fall back to a
different process environment. The snapshot is never serialized into tool JSON,
agent records, transcripts or persisted history. Restarted local continuations
use the current authenticated caller's snapshot.

OpenAI tool starts and argument fragments preserve their response-local index.
Each consumer owns its own indexed accumulator, so interleaved parallel calls
cannot concatenate arguments into a different call. Agent completion requires
both protocol completion and successful HTTP transfer completion. HTTP errors,
malformed frames and truncated streams are failures.

## Completion and batch semantics

A normal delegated run succeeds only on `end_turn` without pending tool calls.
Only `tool_use` permits tool execution. Reaching the configured round limit,
`max_tokens`, unknown/missing stop reasons, or contradictory stop metadata
returns an explicit failure, preserves partial output and structured history,
and uses the existing `Failed` status. Deliberate lifecycle-hook stops keep
their previous successful-stop semantics. No persisted status enum changes.

Non-team `spawn_agents()` futures own the supplied shared tool registry and
execute the complete foreground Agent loop inside the future. They return a
terminal output/error, rather than starting another background queue. Missing
registry is an explicit error before registration. API context, parent model
and live permission callback are copied into the future. Statistics count
actual model calls and reported input/output usage; they are not text-length
estimates. The registry may execute one shared Agent adapter concurrently;
its diagnostic counters use atomic access, while batch futures use separate
Agent instances so each returned statistic belongs to that execution.

Team batch calls are launch operations for the existing teammate backends.
Their receipts report `started=true, completed=false`; the merger distinguishes
started from completed. External teammate execution is not a new wait protocol.
New DTO fields are appended to preserve existing positional aggregate inputs.
No configuration format, CLI spelling or persisted agent schema is migrated.

## Foreground cancellation

The full PTY suite exposed an existing dropped cancellation callback: Escape
aborted the parent query, but a foreground Agent checked only its native
record, leaving a parked model transfer running. Foreground execution now
owns a bounded cancellation watcher that forwards the parent callback into
its native record. It stops and joins before cleanup and terminal-state
publication. Existing model and Bash cancellation paths consume that record.
Background execution never retains the borrowed parent callback. A native
parked-header regression and the real FTXUI single-Escape scenario verify the
propagation and connection drain.

Cancellation during a tool batch also persists a matching result for every
emitted call. Completed results remain intact, the interrupted call records
that side effects may have occurred, and unstarted calls are marked not
executed. This keeps resume filtering from discarding the assistant's
execution history. The regression reloads from disk and inspects actual
Messages/OpenAI resume requests, including their original call/result IDs.

## Native acceptance

The new project-native suites cover:

- round exhaustion, last-round success, hook stops, failed-run resume, partial
  output and invalid stop metadata without unintended tool side effects;
- actual query HTTP route/authentication/response parsing for OpenAI, indexed
  interleaved tool calls, usage after finish, prefixed endpoints, injected
  prepared paths and error/truncation handling;
- delegated protocol and session isolation, recursion, background/resume,
  stalled-header cancellation from both native and parent signals, SSE
  fragmentation, and measured token usage;
- batch final results, concurrent request overlap, registry ownership, captured
  session configuration, live permission denial and honest team receipts.

Build and run debug and release separately. Several older tests use fixed
system-temporary paths, so simultaneous test runs across presets can collide.
Use the project's normal build presets and serial CTest as the graduation
signal. Run graph, inline-body and RFC checks as well.

## Live endpoint and terminal acceptance

`tests/e2e/e2e_subagent_live.cpp` starts the actual FTXUI executable through a
PTY. CTest registers three opt-in scenarios; ordinary test runs return skip
code 125 without contacting an external endpoint. Use an absolute staged
`LOOM_CONFIG_DIR` containing the intended endpoint settings. Do not commit
credentials or pass them on command lines.

```sh
export LOOM_LIVE_SUBAGENT_TEST=1
export LOOM_CONFIG_DIR=/absolute/private/staged-config
export LOOM_LIVE_EVIDENCE_DIR=/absolute/private/evidence
export LOOM_WIRE_API=messages # repeat with openai
ctest --preset debug -R '^E2E_SubagentLive_' -j1 --output-on-failure
```

The parallel scenario verifies overlapping child command intervals and final
outputs from two background agents. Cancellation scenarios use actual Ctrl+D
or SIGHUP, verify persisted cancellation and process drain, then restart the
terminal and use `send_message` to resume the same agent. A random proof token
must be recalled from the old history, and the structured history prefix must
remain intact. The old command's start marker must remain unchanged and its
end marker absent throughout resume, so replaying cancelled work cannot be
hidden by a later successful answer. The harness records redacted terminal logs, timestamps, PIDs,
record snapshots and stage JSONL in the evidence directory.

## Changed areas and compatibility

| Owning area | Necessary change |
| --- | --- |
| `src/orchestration/agent/` | Protocol adaptation, completion rules, real batch execution, cancellation and resume outcomes |
| `src/query/` | Capture resolved session context and use the complete selected wire request/response path |
| `src/services/api/` | Reuse owned HTTP transfers with per-request stream decoders and retain fragmented SSE fields |
| `src/types/tool_types.cppm`, `src/tools/agent_types.cppm` | Append internal execution-context/result fields without changing existing aggregate positions |
| `src/tools/runtime_*`, `src/main.cpp` | Forward context through registration, dispatch, message resume and direct invocation |
| CMake and `tools/arch/graph_check.py` | Place existing wire modules in an acyclic shared target and keep install/layer checks complete |
| `tests/tools/`, `tests/query/`, `tests/e2e/` | Native regression and actual endpoint/PTY acceptance coverage |

Shared changes are confined to execution context, wire transport, SSE framing
and CMake dependency ownership. Exported wire module names stay unchanged.
No CLI syntax, persisted schema, credential file format or status enum changes.
The full 39-file task manifest is included with the local validation evidence.

## Independent review and results

Design reviewer: `agent:/root/budget_fix`. The initial review blocked loss of
OpenAI tool-call indexes; approval followed the indexed accumulator amendment,
complete internal API snapshot, preserved Messages enterprise preparation and
cancel/join ownership requirements.

Code reviewer: `agent:/root/independent_review`. Material findings were resolved:

- preserve interleaved OpenAI tool indexes and require successful HTTP plus
  protocol completion before dispatch or success;
- use the complete prepared request in the parent engine, not only its body;
- keep copied API snapshots out of persistence and background ownership safe;
- make shared-adapter diagnostic counters race-free;
- forward foreground Escape cancellation and preserve cancelled tool outcomes;
- reject live resume tests that replay the original cancelled command.

Pre-final full testing found the dropped foreground cancellation callback.
Pre-final live recovery also reissued cancelled commands: one Messages case
failed, while two OpenAI cases initially reached a successful answer only after
repeating the old command. Those OpenAI receipts were disqualified. The revised
native regression verifies the missing cancellation outcomes deterministically;
the strengthened live fixture detects replay immediately. Earlier traces remain
preserved as diagnostic evidence and are not counted as final acceptance.

Final acceptance ran from isolated committed baseline
`196076149a21dcf6cf8365d7051099a1c9e7b0c4`, with only the 39 task files overlaid.
During acceptance, the original checkout retained unrelated user work and
the complete task diff uncommitted. Subsequent commit packaging is recorded
in Git history. Toolchain: Homebrew Clang 23.1.2, CMake 4.4.1, macOS arm64.
Both debug and release builds passed with `-Werror`; release used `-O2`.

| Final serial CTest | Registered | Passed | Skipped | Disabled | Failed | Seconds |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Debug | 2757 | 2732 | 23 | 2 | 0 | 461.69 |
| Release | 2757 | 2732 | 23 | 2 | 0 | 405.99 |

All counts were parsed from complete final logs and both commands exited 0.
The 23 skips comprise 16 opt-in dangerous Bash cases, one Windows PowerShell
case, three platform/source clipboard guards and the three opt-in live tests.
Two existing fullscreen click-callback tests remain disabled. The live tests
were exercised separately against the configured actual endpoint and model:

| Protocol | Parallel | Ctrl+D cancellation and resume | SIGHUP cancellation and resume |
| --- | --- | --- | --- |
| Messages | PASS, 7.984673 s child-command overlap | PASS | PASS |
| OpenAI | PASS, 5.803553 s child-command overlap | PASS | PASS |

All four recovery cases verified persisted cancellation, actual child process
exit, original agent identity, recalled history proof and strict structured
history prefix extension. No cancelled command restarted or produced an end
marker during recovery. Foreground single-Escape cancellation also passed in
the native real-PTY suite. The 14 focused cancellation/wire regressions passed.
Graph checks (core8, UI9, store and target linkage), interface-body ratchet and
RFC lint passed. Final hashes had zero drift across 2576 source/fixture/CMake
inputs and 216 executables. The task overlay is byte-identical in both
checkouts, and 2629 unrelated original files retain their initial hashes.

Local evidence is in `build/subagent-validation-20261017/`: full CTest logs,
per-test logs, live records/terminal traces, task manifest and source/binary
hashes. Credentials are excluded; the temporary credential settings copy was
removed after live acceptance. Original user settings remain untouched.

Validation scope is the current macOS host and configured endpoint/model.
Linux, Windows, ASan, physical-terminal variants and other providers/models
were not independently exercised. No commit, push or release was performed during the acceptance run.

Final independent gate: **APPROVED**, reviewer `agent:/root/independent_review`.
The reviewer independently recomputed both full-suite counts, both hash
manifests and all six final live evidence sets. No unresolved blockers remain.


## RFC 0021: runtime visibility and bounded delegation

The main screen now projects ordinary native-agent instances above the prompt,
with a count summary, display name, and queued/running/completed/failed/cancelled
state. Active instances sort first. Terminal instances remain visible in the
current session. Small terminals share the existing five-row activity budget
with todos and teams; overflow is counted. `/agents` remains a definition library.

`NativeAgentStore` publishes changes outside its locks. App subscribes through
the existing shared UI action queue, coalesces dirty notifications, and projects
only records matching the current root `parent_session_id`. Team records keep
their existing projection. Closing the queue before unsubscribe makes an admitted
late callback harmless; no render ticker or new polling thread is introduced.
Root identity propagates through launch, nested execution, and public mailbox
resume. Legacy/unowned and other-session records are excluded. `/clear` retains
its existing session identity and does not hide active work; successful session
resume switches the projection and failed resume preserves it.

The production QueryEngine default prompt and actual Agent tool metadata now
teach proportional delegation for substantial independent research, review,
and implementation. Each task needs a self-contained objective, context, scope,
and expected evidence. Concurrent work uses `run_in_background=true`, explicit
`general-purpose`, and result collection through the actual non-blocking
`task_output` contract. Small or sequential work stays local. Launch receipts
are not completion; the parent must collect, verify, and integrate results.
Custom system-prompt replacement semantics and user/repository constraints are
preserved. The request's tool filter remains authoritative. Fork guidance no
longer invents automatic parent forking or unconditional commits. Public sources
and the independent design/PRR gate are recorded in RFC 0021.

Realistic research validation also exposed an existing QueryEngine bug: a
30,000-byte tool-output prefix split a legal UTF-8 box-drawing scalar. The JSON
writer rejected the result and complete request, leaving the HTTP body empty
and omitting that result from the transcript. The local fix retreats from a
continuation byte to the scalar boundary before appending the existing marker;
the byte budget, marker, and ASCII behavior are preserved. A native streamed
HTTP regression first failed without the fix, then passed for every cut within
two-, three-, and four-byte scalars, complete scalars, and ASCII boundaries.

The new shared changes are additive internal session fields and native-store
change subscription. No public CLI syntax, persisted schema, credential/config
format, task notification tags, or wire protocol is changed.

Independent review recorded a non-blocking performance boundary: projection
uses the existing store `list()`, which reloads and parses complete sidechains
under its mutex before producing small display DTOs. The live research sample
remained responsive with roughly 300–800 KB sidechains, but latency/throughput
were not benchmarked; no absence-of-regression claim is made. A lightweight
in-memory summary snapshot is deferred until measurement justifies that API.

A separate, pre-existing Read pagination observation is excluded from RFC 0021:
the live model reported 1,333 lines after `offset=642` without a limit on a
692-line source snapshot. Static review suggests the remaining-line bound in
`FileReadTool` is incorrect for this path. No focused deterministic pagination
reproduction was added, and the Read owner was not changed. The UTF-8 sample
only accepts nonempty valid subsequent wire bodies and a normal protocol end;
it does not validate that model summary or pagination behavior.

### RFC 0021 acceptance results

Final macOS arm64 validation used frozen inputs in the isolated managed
checkout and the same debug executable as the final real-terminal sample.
Both debug and release `-Werror` builds passed.

| Final serial CTest | Registered | Passed | Skipped | Disabled | Failed | Seconds |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Debug | 2790 | 2765 | 23 | 2 | 0 | 475.29 |
| Release | 2790 | 2765 | 23 | 2 | 0 | 408.29 |

The 33 additional native registrations cover observer/lifetime behavior, root
session propagation and public mailbox resume, both wire request formats and
prompt availability/customization, runtime display/layout, and UTF-8 output
boundaries. Both final suites passed on their first complete frozen run.
Earlier focused failures (narrow layout, a short-terminal fixture, and missing
background permission setup in a resume fixture) were resolved before these
runs. The UTF-8 regression was deliberately run red before its production fix.
The pre-fix full debug run is diagnostic evidence rather than final acceptance.
Skip/disabled categories remain those documented above for the earlier runtime
acceptance; the three external-endpoint CTest cases are opt-in.

The final configured-endpoint sample launched two background agents in one
assistant response. Each performed exactly one real Read and one native Bash
marker command. Their command intervals overlapped by **29.846546 seconds**.
Both reached Completed; while the parent remained idle, the activity strip
automatically changed from two running to two completed. The parent then
collected both actual `task_output` results and ended normally. Captures are
unaltered FTXUI PTY ANSI replayed read-only in xterm for screenshots; they are
not desktop-terminal screenshots. No REA was used.

A separate larger audit, without an explicit delegation request, did launch
two background general-purpose agents concurrently. One model stream timed
out; the remaining agent was cancelled by actual Ctrl+D and the application
exited normally. It proves proactive launch/status behavior, not completed
research or general model quality. An intermediate final-sample launcher used
the wrong working directory and was disqualified before the corrected sample;
its lookup/marker evidence is not counted as completed research.

RFC lint, module/target graph gates, the interface-body ratchet, and task-scoped
diff whitespace checks passed. The whole dirty checkout retains pre-existing
terminal-cell padding in unrelated golden files, which global diff-check flags. Frozen hashes show no drift across 2590 source,
fixture, and build inputs and 214 invoked executables. The 37-file task manifest
is in `build/subagent-visibility-validation-20261017/task-paths.json`. The other
54 pre-existing dirty files retain their content: 52 are byte-identical; the
README and UI CMake retain their original bytes plus only the task additions.
All 35 non-shared task files match the isolated checkout exactly.

Final live evidence and running/completed screenshots are in
`build/subagent-visibility-accepted-parallel-20261017/`; exact UTF-8 wire evidence
is in `build/subagent-visibility-utf8-20261017/`. Full logs, frozen hashes,
source preservation, and disqualified diagnostic runs remain local evidence.
Temporary credential copies were removed; user settings were only read.
No commit, push, or release was performed for RFC 0021. Linux/Windows, ASan,
producer BMI PSS, long-session throughput/latency, and other endpoint/model
combinations were not independently validated.

Final RFC 0021 independent code gate: **APPROVED**, reviewer
`agent:/root/independent_review`. Runtime/UI/prompt workstreams are integrated;
all blocking findings are resolved. Remaining non-blocking limitations are
explicitly documented above.


### RFC 0021 Phase D: mixed activity priority correction

The previous mixed PTY showed two completed ordinary Subagent detail rows,
while the four-item Tasks plan was reduced to its header. Phase D fixes the
owning runtime compositor, with no scheduler/store/protocol/shared-widget change.

One combined summary reports Subagent active/total, Tasks active/done/total,
and Team Agent running/total counts. Remaining rows (at most four) use stable
fair rounds: in-progress Task, active Subagent, running Team Agent. Labels say
`Task`, `Subagent` and `Team Agent`. Failed Subagents and unknown-status teammates
follow active work, then pending Tasks. Completed/cancelled Subagents, completed
Tasks and idle teammates stay in the summary; idle is never called completed.
At narrow widths, remove history and totals before omitting complete active
count fragments. With no runtime Subagents, the prior todo/team path is unchanged.

The project-native pre-fix regression run failed 8 of 11 tests, including the
exact reported mixed state. Independent Phase D design/PRR was APPROVED by
`agent:/root/independent_review` after resolving active Task aggregate counts
and the empty-runtime boundary. Final Phase D measurements and evidence follow.


Real execution also exposed a separate refresh gap: native Team Agent changes
woke the ordinary Subagent projection but left the teammate DTO stale until
SyncState/pane/modal refresh. The minimal app-owned correction invokes the
existing team projector only when the UI consumes a changed native dirty flag.
It adds no polling or callback/lifetime changes. The native App regression uses
actual store notifications and terminal mutations through Custom/OnEvent while
the parent remains busy, checking all status mappings, counts and isolation.
Independent design/PRR and static integration review APPROVED this local scope.

| Phase D final serial CTest | Registered | Passed | Skipped | Disabled | Failed | Seconds |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 2797 | 2772 | 23 | 2 | 0 | 508.06 |
| Release | 2797 | 2772 | 23 | 2 | 0 | 414.21 |

Both final full suites passed their first frozen run; no failed-test retry.
Focused native suites each passed 19/19. Six compositor cases and one App team
wake case reconcile the seven new registrations against 2790. The compositor
red run (8/11 failures), team-wake red run (1/1 failure), and successful but
pre-integration Debug run are diagnostic evidence retained separately.
Both final -Werror builds used default Ninja concurrency. Module/UI/store/target
graph checks, interface-body ratchet, RFC lint and scoped whitespace checks pass.
Frozen source/build/fixture inputs (2590) and executables (214) have zero drift.
The seven-file ownership manifest and complete logs/XML are in ignored
`build/activity-priority-validation-20261017/`; 82 unrelated dirty files remain
byte-identical, HEAD is unchanged and the index is untouched.

Final configured-endpoint evidence is in
`build/activity-priority-accepted-20261017/`. The frozen debug binary SHA-256 is
`9a18bd005b0929a1f434c6d4f3be096a5ffa119f088b1b74ade19d2d865e88f6`.
Actual `/resume` restored the root session and `/clear` kept its completed
Subagents. A fresh team worker executed a real 90-second Bash marker while the
plan remained 1/4 done, including one in-progress Task. The final parent-busy
frame shows both Task and Team Agent immediately; the idle mixed frame shows
completed Subagents only in the summary; after actual todo_write marks all four
plan entries complete, the activity area is one aggregate row. Four native
records completed with exactly one Bash each and exact SUB_A/SUB_B/TEAM_WORKER/
TEAM_FINAL outputs. This sample makes no three-command-overlap or agent-cancel
claim; fixed marker slot names do not establish those scenarios. Actual PTY
bytes are replayed read-only in xterm for screenshots; no REA.

The invalid 180-second helper demo is excluded. The separate CLI ConversationStore
resume attempt is excluded; UI `/resume` is the accepted path. A long endpoint
request was cancelled with Esc before a cleared-context retry. The final sample
selected public `/effort low`, without claiming provider-side enforcement.
All diagnostics remain labelled in evidence. Final app exit was 0; temporary
credential settings copies were removed after execution, and a credential-value
scan of retained text artifacts found zero hits. No user settings were changed.

Non-blocking existing P2: the shared footer still calls teammate_count `teams`;
the mixed activity rows now use correct Team Agent/Subagent terminology. Shared
footer/widget changes are outside this owning scope. Existing long-history store
list cost (including event-only team refresh), other platforms, ASan, hosted CI
and Linux PSS remain unverified. No shared module or established public contract
changed, and no commit/push was made.


Final Phase D independent graduation gate: **APPROVED**, reviewer
`agent:/root/independent_review`. All blocking findings are resolved; recorded
P2 limitations remain. RFC 0021 is implemented and Phase D is done.


## Phase E: layered plans, execution and background status

This section supersedes Phase D's mixed aggregate and the deferred teammate
wording issue above. Plans and execution now occupy contiguous Tasks and Agents
sections within the same existing zero-to-five-row activity limit. Tasks retain
active and done counts; current Tasks get a named row before spare execution
rows. Agent details distinguish Subagent from Team Agent. Current execution
hides completed/cancelled history and idle team details; completion retains
separate short Tasks and Agents summaries. No runtime browser is implied by
`/agents`, which remains a definition library.

When the foreground spinner is hidden, actual background ordinary queued/running
instances or running teammates keep a static background-status label in the
existing work-row slot. It does not set query_running, create a fake Thinking
state, report a foreground token rate, or add a polling/animation ticker.
Existing modal/error focus guards remain. Clipboard feedback is normalized and
clipped by Unicode terminal cells to the full row width, fixed to its measured
width, and gets priority over left work text; editor/caret/footer geometry is
preserved. Both shared footer count paths say Team Agent/Team Agents while
keeping legacy data/configuration field names and focus behavior.

Owners: runtime renderer/interface, REPL layout, prompt footer's two rendered
labels, four existing native UI test files, this guide and RFC 0021. No runtime
store, scheduler, observer, query protocol, persisted schema, CLI command or
configuration contract changes belong to Phase E. Independent design/PRR and
static code reviews were approved by `agent:/root/independent_review`.

Initial focused testing found four failures. One was a production regression:
the no-Agent/no-Task fallback wrapped an empty panel in a one-row container.
An explicit nonempty-plan guard restores zero activity rows. The remaining
failures required old idle-detail assertions and a hardcoded Subagent glyph to
follow the new approved behavior. A separate review P1 restored active Task
counts in header-only budgets. Both debug and release focused runs then passed
75/75. Complete-suite and final PTY graduation results are recorded below;
focused results alone are not graduation.

Evidence is retained in `build/subagent-layering-validation-20261017/` and
`build/subagent-layering-live-20261017/`. The isolated managed checkout receives
only the Phase E patch relative to the saved dirty baseline, not unrelated
accessibility work. `prompt_input_footer.cppm` therefore has intentionally
different whole-file hashes across the shared and isolated checkout: the
preexisting dim-to-muted work stays only in the shared checkout; the Phase E
label patch is the same. Do not claim the two complete dirty trees or this
shared file are byte-identical. Source and executable manifests identify the
actual compiled and executed isolated inputs.


### Phase E final measured results

| Complete serial CTest, first full run | Registered | Passed | Skipped | Disabled | Failed | Seconds |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 2808 | 2783 | 23 | 2 | 0 | 479.15 |
| Release | 2808 | 2783 | 23 | 2 | 0 | 411.98 |

Both exited 0 without failed-test retries; both focused suites passed 75/75.
The earlier build missing-import diagnostic and four focused failures are
retained as diagnostics, not reported as successful first attempts. Both final
builds used -Werror and default Ninja concurrency; Release used -O2 -DNDEBUG.
Registration changed by +11 (27 added names and 16 removed/renamed). Final
architecture/store/target and interface-body checks pass. Frozen manifests
cover 2590 source/build/fixture inputs and 214 executables with zero drift.
Complete logs/XML and final-test-results.json are in the validation directory.

The actual configured endpoint launched an ordinary Subagent plus two Team
Agents whose real Bash commands overlap for 89.934487 seconds. All four native
records, including an earlier ordinary agent, completed with exact marker
outputs. The initial unbracketed prompt was truncated and is excluded from the
three-agent proof; bracketed-paste input was checked against the accepted
session message. Fixed marker slot names do not establish cancellation/resume.
The plan was updated through actual todo_write, retaining one active Task
during background execution and completing all three afterward.

The frozen debug binary SHA-256 is
`0bda68ea1dc5c186b524049bf34470abcecc3a0c01ad4e9d82f8939a2f3704c2`.
The live directory retains original PTY ANSI, matching prefix/frame hashes,
native records, input-integrity.json, native-execution-proof.json and
result-final.json. Captures are running.jpg (120x36), completed.jpg (120x36)
and narrow.jpg (actual resize to 60x30). Each screenshot is a disclosed read-only
xterm replay of the actual FTXUI PTY output, because native Terminal automation
was unavailable. Running frames preserve the background hint, separate sections,
role labels, input and footer; completion folds to two summaries and removes
the hint. The footer reads `2 Team Agents`. No REA was used.

The app exited 0. Temporary credential settings were removed; 193 retained
artifacts were scanned without credential-value hits; the owned viewer server
and tab were closed. The final ownership manifest has 11 files. HEAD is unchanged,
the index is empty, and 82 unrelated preexisting dirty files are byte-preserved.
The intentional isolated footer baseline difference described above remains.
Only shared footer display wording changed; no established public contract,
runtime store, scheduler, protocol or configuration changed.

Independent final graduation: **APPROVED**, reviewer
`agent:/root/independent_review`, recorded in final-review.json and RFC 0021.
All blocking findings are resolved; Phase E is done. Limits: macOS/one configured
endpoint; ASan, Linux/Windows, hosted CI and producer PSS were not run. Existing
credential-dependent native live E2E registrations were skipped; actual endpoint
PTY acceptance is separate evidence. Existing long-history store cost remains
unbenchmarked; tiny activity budgets intentionally retain counts before names.
No commit, push or release was made.


## Phase F: explicit three-group hierarchy

The user's review of the Phase E real frames identified missing child indentation
and an ambiguous combined Agents heading. Phase F replaces that local composition
with Tasks, Subagents and Agent Teams peer sections. Children start two cells
beyond their heading, without repeating a role prefix on each line. Agent Teams
headings explicitly count agents, not teams. Empty sections disappear; folded
completion/cancellation/idle stays separate by category. Task completion grace
remains controlled by the existing completed_collapsed flag, including in mixed
sessions; native expiry/clear/resume/history coverage is retained.

A visible foreground or background work parent gets one MessageResponse hook
before the first heading; later headings align at that column and children two
cells farther in. Full-width copy feedback suppresses that hook when the work
label has no remaining cells. The REPL uses the existing measured fixed chrome,
half-screen bottom limit and transcript floor to derive activity capacity; the
extra five-row and term_rows/6 caps are removed. Three groups can therefore each
show a child when six rows fit, while short terminals preserve input/footer and
current group counts according to the explicit RFC state priorities.

The owning files are the runtime compositor/interface, REPL layout, existing
runtime/chrome/UI/native PTY tests, RFC 0021 and this guide. Shared TodoPanel and
LiveTeammate renderers, footer, scheduler, stores, query, settings, persisted data
and protocol remain unchanged by this phase. Independent design/PRR APPROVED:
agent:/root/independent_review, after resolving completion-grace and zero-width
copy-parent findings. Code review is approved; final measured graduation is recorded below.

The Phase F dirty baseline is saved in
`build/activity-hierarchy-validation-20261017/`: 2675 files, 93 already dirty,
HEAD f115620a679664a8d1f0014928de3a5b652b89cb and empty index. Only the Phase F
owned delta is applied to the existing isolated managed checkout; unrelated
preexisting accessibility work is preserved in the shared checkout.
Configured-endpoint PTY acceptance artifacts will be retained in
`build/activity-hierarchy-live-20261017/`. No REA, commits or pushes.


Phase F measured evidence so far: Debug complete serial CTest first full run
exited 0 with 2810 registered, 2785 passed, 23 skipped, 2 disabled and 0 failed
in 473.20 seconds. Focused Debug passed 83/83, including native real PTY task
response/completion tests. Initial focused 81/83 and the initial two explicit
FTXUI string conversion build errors remain diagnostic logs. Registrations
reconcile from 2808 to 2810: 16 added names, 14 removed/renamed, net +2. Native
runtime assertions cover supplied budgets 0 through 12 and the new chrome
regression verifies all three groups on measured 40/60/80/120-column layouts.
Independent static code/RFC review: APPROVED, agent:/root/independent_review.

The frozen debug executable SHA-256 is
`9d229efd9531f3ffce63f0b903a10b27d913484d5dc666a7e5aabce83e42c110`.
Configured-endpoint acceptance launched two ordinary Subagents and two Team
Agents, each with one actual Bash marker command and exact expected output;
all four native records completed. Four-command overlap is 84.933393 seconds.
The complete 1085-character launch prompt matches the session input exactly;
bracketed paste used 100-character chunks and a separate Enter. Fixture marker
slots cancel/resumed do not imply cancellation/resume scenarios.

Original PTY running and completed frames are 120x40; the narrow frame follows
an actual PTY resize to 60x30. All three ANSI prefixes match the original raw
stream and metadata SHA. Read-only xterm accessibility rows and native tests
verify heading column 5 and child glyph column 7 (zero-based), with one response
connector. Completed pinned activity is exactly three independent summaries at
rows 32-34: checked Tasks 4/4 completed, Subagents 2 completed, and Agent Teams
counting two idle agents. There are no activity children or background hint.
Historical Todo tool output above the pinned area remains an expanded transcript
result; it is not the current activity block and is unchanged by this phase.
The screenshot provenance is read-only xterm replay of real FTXUI PTY output;
native desktop-terminal automation is unavailable. No REA was used.

See native-execution-proof.json, frame-integrity.json, rendered-rows.json,
hierarchy-geometry.json, result-final.json and captures/ in the live directory.
The actual app exited 0, temporary settings were removed, 57 retained artifacts
were scanned without credential-value hits, and original settings hash stayed
unchanged. The owned viewer server and browser tab were closed. An independent
reviewer verified these live proofs; both complete suites and final hash/preservation checks are now complete;
final independent graduation is approved as recorded below.


### Phase F complete verification

| Complete serial CTest, first full run | Registered | Passed | Skipped | Disabled | Failed | Seconds |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 2810 | 2785 | 23 | 2 | 0 | 473.20 |
| Release | 2810 | 2785 | 23 | 2 | 0 | 409.95 |

Both full runs exited 0 without failed-test retries. Both focused runs passed
83/83. Builds used -Werror/default Ninja parallelism; Release used -O2 -DNDEBUG.
Final graph/UI/store/target checks pass (638 modules, 800 units, no new cycles
or upward edges), interface-body violations are empty, RFC lint passes 21/21
shared and 20/20 isolated (unrelated RFC 0019 excluded), and scoped whitespace
checks pass. After both full suites, frozen 2590 source/build/fixture inputs and
214 executables have zero drift. All nine owning files match between checkouts;
whole unrelated dirty trees are intentionally not equated. HEAD and the empty
index are unchanged, 85 unrelated preexisting dirty files retain original bytes,
and there are no unrelated nonignored additions/changes.

The phase modifies only the local compositor/interface, REPL layout and four
existing native test files plus two docs. Shared TodoPanel/LiveTeammate widgets,
footer, stores, scheduling, query, configuration, persisted/protocol formats and
established public contracts are unchanged. This is local macOS/one-endpoint
acceptance: ASan, Linux/Windows devices, hosted CI and producer PSS remain unrun;
long-history store cost remains unbenchmarked. Existing credential-dependent
registered live E2E skips are separate from the actual configured-endpoint PTY
proof above. Very small heights intentionally prioritize fixed chrome and group
counts over every detail. No commits, push or release.


Final Phase F independent graduation: **APPROVED**,
`agent:/root/independent_review`. The reviewer independently checked both full
XML/logs, recomputed all 2590 source and 214 executable SHA values, verified real
PTY command/frame/hierarchy/cleanup evidence, and confirmed preservation. All
material findings are resolved. Phase F is done; RFC 0021 is implemented. The
renderer worker owned its implementation/interface/runtime/UI assertions; the
main agent integrated REPL layout, chrome/native PTY checks, builds and live proof.
All work remains uncommitted.


## Task completion notification display repair

The engine intentionally appends native agent completion envelopes as user
messages for the next model turn. The App projection treated them as ordinary
user text after the old collapse pipeline became a no-op, exposing XML tags,
output/worktree paths, entity escapes and result bodies in the transcript.

This repair belongs to App message projection. Complete known single-text-block
notification envelopes become existing background-task system notices, such as
`Agent completed · sub-read`, `Agent failed · review`, or `Agent stopped · scan`.
Only the normalized, once-decoded, length-bounded agent label and terminal status
reach display state. A failed summary with ambiguous delimiters uses its task ID
instead of risking disclosure of the failure cause. Cursor-based parsing rejects
unknown/duplicate fields, nested scalar markup, incomplete envelopes and prose.
Mixed messages, embedded examples and fenced XML remain ordinary user content.

Both single and split projection paths apply the same local rule, including
committed rendering, synchronization, virtual history and session restoration.
The engine messages, persisted history, model input, producers and established
protocol remain unchanged. Because persisted UserMessage has no trusted origin
flag, a manually entered message exactly matching the complete known envelope is
indistinguishable and receives the same display treatment. No persisted metadata
or public contract is added for this UI-only repair.

Owned files: app_message_projection.cpp, messages_list_geometry.cpp,
test_app_runtime.cpp and this guide. Native long-history acceptance exposed a
shared row-height defect: a one-line BackgroundTask estimate clipped its actual
top margin plus content. Only this subtype now reserves two rows; all other
shared message shapes and renderers retain their existing behavior.
Seven native regression cases exercise real notification producers, all terminal
statuses, both quote encodings, entity decoding, ambiguity, controls/UTF-8 limits,
strict grammar, ordinary content preservation and App render/restore paths.
Independent design/PRR: APPROVED, agent:/root/independent_review, after resolving
strict-grammar and ambiguous-failure-label findings. Static review and the narrowly scoped shared geometry re-review are APPROVED,
agent:/root/independent_review. Final Debug focused verification passes 275/275;
both complete suites pass; independent final graduation is APPROVED. No commit or push.


Diagnostic history is retained separately: the initial build found three test
compile errors, then the first seven-case run passed six cases and exposed the
long-history fixture timestamp ordering. The first 275-case focused run passed
274; further diagnosis exposed existing same-turn Assistant row estimate drift.
The final long-history fixture uses 81 distinct real producer notifications to
isolate this repair; its virtual render, committed cache, sync and restore paths
pass after the BackgroundTask-specific height fix. General Assistant row height
estimates remain outside this repair. These initial failures are not reported
as successful first-pass verification.


Final Debug first full serial run: 2817 registered, 2792 passed, 23 skipped,
2 disabled, 0 failed, 483.10 seconds; focused 275/275. Release first full serial run: the same
2817 registered / 2792 passed / 23 skipped / 2 disabled / 0 failed,
414.39 seconds; focused 275/275.
Final code/shared-geometry and real PTY stage review: APPROVED,
agent:/root/independent_review. Final Debug executable SHA-256:
`7fd4a329f5d9b085cddf218af0232d3301ea3c83faca9e0b345c17ddb96fe8ee`.

The acceptance app uses the isolated build, copied into
`build/task-notification-live-20261017/loom-debug`. It restored actual history,
then ran a new ordinary Agent with exactly one native Bash marker command.
The marker ran 8.010387 seconds and returned NOTICE_FINAL_DONE; its completion
notification was delivered. Original XML remains in persisted history and the
model request, while final 120x40 and actually resized 60x30 PTY frames display
only the compact system notices. Final-frame metadata/SHA values match original
terminal byte prefixes. Screenshots are read-only xterm replays of real FTXUI
PTY output, not desktop Terminal captures. No REA.

Live artifacts: binary.json, native-execution-proof.json, frame-integrity.json,
final-*-rows.json and captures/final-*.jpg under the live directory. Earlier
first-pass artifacts precede the geometry refinement and are diagnostic only.
The final app exited 0, staged settings were removed, original settings hash
and mtime stayed unchanged, and 56 retained artifacts had zero credential hits.
The temporary viewer server and browser tab were closed. This local macOS/one
endpoint acceptance does not establish Linux/Windows, ASan or hosted CI results.


Both full runs exited 0 without retries; the registration delta is exactly seven
new native cases. Builds used -Werror and default Ninja parallelism; Release uses
-O2 -DNDEBUG. Final graph/core/UI/store/target checks pass with no module cycles
or illegal upward edges; interface-body violations are empty. RFC lint passes
21/21 in the shared checkout and 20/20 in isolation (unrelated RFC 0019 excluded).
Scoped whitespace checks pass. Final source/executable/preservation results are
recorded in build/task-notification-validation-20261017.


Final independent graduation: **APPROVED**, agent:/root/independent_review.
The reviewer directly parsed both complete XML/logs and registration names,
recomputed all 2590 source inputs and 214 executable SHA values, verified the
final live executable, and independently checked all 2675 initial working files:
2671 unrelated files have zero missing/drift, all four owned files match between
checkouts, 93 unrelated preexisting dirty files are preserved, HEAD remains
f115620a679664a8d1f0014928de3a5b652b89cb and the index is empty. The only change
after graduation is this guide's gate record; code/tests remain frozen. There
are no unresolved material review findings. Public interfaces, producer tags,
model input and persisted formats are unchanged; the sole shared-module change
is BackgroundTask's correct virtual row height. No commit, push or release.


## Consecutive Assistant history spacing repair

The previous notification repair exposed an independent virtual-list defect.
Static message rendering already uses per-message margins; the virtual renderer
still used a stale per-turn state machine. Consecutive AssistantText rows therefore
rendered one line each after the first while geometry reserved two. At more than
80 visible rows the list switched to virtual mode, accumulated this discrepancy
and could clip a trailing notification.

The owning module is messages_list. A private payload spacing helper now supplies
both static and virtual paths with the existing static policy: assistant/other
rows have a margin, user continuations suppress it, and UserToolResult plus
UserBashOutput remain flush. Non-payload rows do not change user continuation
state. Static behavior is preserved. General Markdown measurement remains outside
this bug fix.

Real PTY resume testing exposed two additional causes of clipped tail replies.
Every current faithful SystemText summary, including Plain resume events, paints
a top margin plus one event row; its estimate is now two, matching the measured
leaf for all ten subtypes. The virtual body now grows horizontally beside its
ASCII gutter. Its height estimates use the actual payload width: terminal
columns minus the outer scroll indicator and, only on overflow, the inner
gutter. Short lists with wrapping are checked before deciding that second
column is needed. Compressed-chain budgets use that same width. These changes
stay inside the messages UI; no stores, caches, public interfaces, event
encodings, model input or persisted data are changed. The shared virtual-list
renderer changes only horizontal growth beside its existing gutter.

Independent design/PRR APPROVED: agent:/root/independent_review. Before production
changes, all three focused native cases failed against the old renderer. After
the fix, all three pass: static/virtual parity at 40/60/120 columns covers user
continuations, images, tool/Bash results and compact groups; the 120-Assistant
history preserves its tail, repeats frames stably, responds to the first wheel
event and PageUp, and repins across resize. The existing App committed/cache/sync
restore regression now covers both 81 notifications and 81 consecutive Assistant
messages. Initial test compilation used an unavailable Event member; it was
corrected before the red run. The subsequent width red run measured only 33
body columns at terminal widths 40, 60 and 120; the expected widths are 38, 58
and 118 respectively. Native coverage also checks short lists with and without
the inner gutter, every SystemText subtype, and a restored mixed history ending
in a new reply. The final Debug build and all five focused regressions pass; the broader
App/message/Repl/virtual subset passes 279/279. Independent final static review
is APPROVED by agent:/root/independent_review. Two earlier Debug full runs were
intentionally interrupted to strengthen assertions and then fix the real resume
reproduction; neither is counted as a complete passing run. Final full
Final complete Debug and Release serial runs both passed: 2821 registered,
2796 passed, 23 skipped, 2 disabled, zero failures, exit 0. Complete XML/logs
show exactly four new cases and no removed registrations. Both builds used
-Werror and default Ninja parallelism; Release used -O2 -DNDEBUG. Both broader
focused UI subsets passed 279/279. The final graph/core/UI/store/target checks
and inline-definition constraints pass in both checkouts; RFC lint passes
21/21 in the shared checkout and 20/20 in isolation (unrelated RFC0019 excluded).
Verification used the existing isolated acceptance snapshot. Its 2590 frozen
code/fixture inputs and 214 executable hashes are checked against that snapshot,
not against unrelated shared-checkout WIP. The 44 pre-existing non-task
differences remain outside this graduation. The six owned files match between
checkouts. This guide is excluded from the code freeze because its results are
recorded after execution; its final revision is covered separately by
final-owned-hashes.json. validation-scope.json records these boundaries.

The final live binary has already restored the public /resume workflow with
120 explicitly seeded consecutive Assistant records (not model-generated). Its
previous real-endpoint reply, which was clipped by the old build, is visible at
120x40 and 60x30. First-wheel scrolling and PageUp move immediately; CtrlDown
restores exactly the same visible rows. The native debug trace confirms virtual
mode. Screenshots are captured from a read-only xterm replay of the actual PTY
byte stream, with resize offsets and SHA values retained in
build/assistant-history-live-20261017. This is not a static UI mock or REA run.
The final build then sent a new request to the configured real endpoint after
restore; its persisted LIVE_HISTORY_OK answer is visible at 60x30 and after
returning to 120x40. The app exited 0. All final captured streams match raw PTY
prefixes and SHA values; 78 retained artifacts contained zero credential hits.
Staged settings were removed, original settings SHA and mtime were unchanged,
and the temporary viewer server and tab were closed.
Final screenshot paths: captures/final-long-complete.jpg (120x40),
captures/final-endpoint-complete.jpg (60x30) and
captures/final-first-wheel.jpg under the live evidence directory.
This local macOS/one-endpoint acceptance does not establish Linux/Windows,
ASan or hosted CI results.

Release -O2 verification also passed the 279-case focused subset. An independent
real Release PTY restored the same saved history at 120x40 and 60x30; HISTORY119,
the clean completion notice, both saved real-endpoint answers, the resume event
and status line remained visible. Release did not send another endpoint query.
Its process exited 0, captures match raw stream prefixes/SHA values, 25 scanned
artifacts had no credential hits, and private settings/viewer were cleaned up.
These artifacts are under build/assistant-history-live-20261017/release.

### Internal task notifications: source-backed comparison

The XML envelope is a text encoding of an asynchronous task lifecycle event,
containing task_id/status/summary, output/result references and optional workspace
metadata. XML entities protect field boundaries. Loom currently appends this
model-visible compatibility envelope as UserMessage; UI projection is separate
from the original persisted model context. This explanation describes the current
implementation, not a recommendation to display protocol text to people.

Representative mature implementations separate runtime state, model context and
user presentation; this is an inference from the following primary sources, not
a global ranking or a claim that every product uses the same encoding:

- [Claude Agent SDK task notifications](https://code.claude.com/docs/en/agent-sdk/typescript#sdktasknotificationmessage):
  typed task lifecycle messages, task/run identifiers, terminal status and summary;
  task-notification origin distinguishes automated completion from human input.
  [Background subagents](https://code.claude.com/docs/en/sub-agents#run-subagents-in-foreground-or-background)
  describes later-turn completion delivery and running-first task UI.
- [Codex app-server](https://learn.chatgpt.com/docs/app-server): typed ThreadItem
  records and item/started plus item/completed lifecycle; model-visible tool
  output remains distinct from UI text and ordinary user turns.
- [OpenCode SDK](https://opencode.ai/docs/sdk#sessions): child sessions, abort,
  event subscription and TUI toast are separate interfaces. The
  [pinned status-event schema](https://github.com/anomalyco/opencode/blob/a49f179170b620f719fc6c25986e971af47b5268/packages/schema/src/session-status-event.ts)
  associates typed busy/retry/idle states with a session ID.

Pre-RFC research recommendation (subsequently adopted by RFC 0022): make runtime notifications typed,
carry trusted origin and task identity, and project independently into model
context and user status/detail views. Legacy XML can remain a model-boundary
encoding during migration. The format itself is not the defect; conflating a
runtime event with human input and UI prose is the architectural limitation.
Do not simply delete model notifications: the coordinating agent needs completion
results to continue work. That preceding validation did not implement a core/persistence migration;
the following RFC 0022 section tracks the subsequent implementation.


Final independent graduation: **APPROVED**, agent:/root/independent_review.
The reviewer directly parsed both complete test suites and registration sets,
recomputed frozen binaries and owned-source hashes, verified both real PTY
streams/screenshots, and confirmed unrelated-file/dirty-index preservation.
There are no unresolved material findings. The guide is the only post-gate
revision; code/tests stay frozen. final-owned-hashes.json records the final
document revision separately. No commit, push or product release was performed.


## RFC 0022 structured task events

Design and PRR gates approved before implementation: agent:/root/independent_review
and agent:/root/notification_research. The local RFC is implemented locally and uncommitted. Phase A passed static
review and 244 focused native tests; Phase B native and real-runtime checks
and independent final graduation are approved. No commit, push or REA is used.

The accepted vertical slice separates HumanInput, LegacyHistory and TaskEvent
origin from model-context text. Native completion has typed actor/status/name
and exact parent-session routing. Local JSONL, conversation save/load and fixture
recording preserve origin through one codec. New human XML remains ordinary
input after restore; only ambiguous old history receives legacy display parsing.
Both model wire backends retain the same context bytes without local metadata.
App cache equality includes origin and typed labels are not XML-decoded.

This verification starts from a full shared-source snapshot, including unrelated
pre-existing accessibility work, in the existing attached isolated checkout.
The refreshed baseline Debug build passes with 2880 registered tests. An initial
baseline build reused stale BMIs after source copying preserved older mtimes;
refreshing copied-file mtimes corrected it without source edits. The first
implementation build found range-loop-copy and designated-initializer-order
warnings in new/updated tests under -Werror; those test declarations were fixed.
Neither failed build is described as a passing verification.

Evidence directory: build/structured-task-events-validation-20261017. It retains
all 2675 initial working-file hashes and bytes, the source synchronization list,
RFC review findings, exact task diff, build logs and complete test results.
Debug and Release full native suites and real PTY checks pass. Linux PSS and hosted
CI are unavailable on this macOS host; they are not inferred from local builds.


### RFC 0022 review findings and closure

| Severity | Finding and impact | Resolution and native coverage | Reviewer |
|---|---|---|---|
| P1 | Named teammate backend setup overwrote the actual parent with the environment fallback, so strict event routing could lose its notification | Prefer the explicit nonempty plan owner; preserve the missing-owner fallback. AgentToolSpawnsTeammateWithDeterministicAgentId and AgentToolSpawnsTeammateWithUniqueNameWhenTeamAlreadyHasMember cover both branches | agent:/root/independent_review |
| P1 | Direct-connect human JSONL lacked origin, allowing new user XML to become ambiguous legacy text after restore | Write origin=human only for new server user messages. ServerRoutes.MessageRoutePersistsHumanXmlOriginAcrossAssemblyRestore exercises the real route, JSONL, assembly restore and follow-up request | agent:/root/notification_research |
| P2 | RFC wording suggested redundant TaskEvent metadata on NativeAgentRecord | Clarify that the factory derives events from existing native fields; only delivered UserMessage records persist provenance | agent:/root/independent_review |

All static findings are resolved and independently approved. Production code
and tests are frozen in frozen-sources.json; subsequent edits are documentation
and evidence only. The final test registration is 2901: baseline 2880 plus
21 named regressions, with zero removals (test-registration-delta.json).

Real acceptance uses a PTY and the configured Messages endpoint. The first
run creates event-success, event-budget and event-cancel; the budget failure
is intentional at maxTurns=1. Native terminal states become exactly three
TaskEvent UserMessages owned by the parent. Ten actual request dumps retain
the full model-context XML while carrying zero origin/task_event fields.
The final frozen Debug binary then restores that same persisted session via
UI /resume, issues a real follow-up query, retains all three model contexts,
and does not duplicate the three event records. 120x40 and 60x30 raw frames,
read-only xterm screenshots, SHA hashes, dimensions and exit=0 are retained in
build/structured-task-events-live-20261017, including final-debug/.

An initial validation command incorrectly used CLI --resume, which targets the
separate conversation store rather than UI session JSONL. Its exit=1 and error
are preserved in final-debug/cli-conversation-resume-attempt; they are not counted
as a passing restore. The subsequent public UI /resume is the successful path.
No product changes were made to hide this distinction.

The source credential file remains byte- and mtime-identical; staged settings
are deleted on normal exit. The retained nonbinary evidence contains zero raw
credential byte matches. The repository-wide git diff --check reports existing
ANSI golden trailing spaces from unrelated work; the owned-file check passes.
Those unrelated goldens are preserved byte-for-byte.


### RFC 0022 final measurements

| Gate | Result |
|---|---|
| Debug final build | exit 0, default Ninja, -Werror, 39.87 s incremental body/comment rebuild |
| Debug full first pass | 2901 registered: 2876 passed, 23 skipped, 2 disabled, 0 failed; serial ctest, 491.53 s |
| Release build | exit 0, default Ninja, -O2 -DNDEBUG, -Werror, 313.57 s |
| Release full first pass | 2901 registered: 2876 passed, 23 skipped, 2 disabled, 0 failed; serial ctest, 413.08 s |
| Registration reconciliation | baseline 2880 + 21 additions - 0 removals; Debug/Release names identical |
| Architecture | 640 modules / 803 units; zero module cycles and new upward edges; core/UI/store/target and inline checks pass in shared and isolated checkouts |
| RFC lint | 22 files / 22 unique RFC numbers, pass |
| Core types BMI bytes | Debug 2499320 before / 2713092 after; Release 2499336 before / 2713108 after |
| New BMI bytes | Debug origin 26440 / codec 23664; Release origin 26460 / codec 23680 |
| Observed rebuild closure | Debug reviewed change: 385 compile objects, including 184 module interfaces, 131.44 s; Release: 442 objects, 223 interfaces, 313.57 s |

Build counts describe the observed combined incremental changes, not a pure
single-interface microbenchmark. Release starts from the older local preset
cache with the full shared-source snapshot, so its before/after size and fan-out
are not attributed exclusively to this RFC. BMI file bytes are not PSS. Linux
PSS, hosted CI/cold timings, Windows, Linux and ASan are unverified on this host.
The 23 conditional skips and two pre-existing disabled tests are not called
passes; the optional E2E_SubagentLive suite was skipped in generic CTest, with
separate configured-endpoint acceptance below.

The final frozen Release executable (072e57ab8715e3e1016dd0359bf6c989bc32c77aebbbce0b3eb0aadf8da816ae)
repeats the complete native lifecycle using new release-success, release-budget
and release-cancel tasks, then creates an empty team and launches the named
release-teammate@event-release-team via Agent. The three new Subagent events are
completed/failed/stopped; the named TeamAgent completes. All four new events
carry the actual restored parent session, while the three old event IDs stay
unique. Seven event records are present, four new full XML contexts occur in
actual requests, and local origin/task_event metadata is absent from the wire.
This final Release run, rather than the earlier binary, proves the complete
end-to-end lifecycle on frozen source.

Final Release captures retain running 120x40, all four terminal notices at
120x54 and narrow 60x34. The raw PTY output is replayed read-only through xterm
for screenshots; no product rows are synthesized or edited. All 12 retained
frames across three runs exactly match prefixes of terminal.raw and their
recorded SHA. All three successful PTYs exit normally with code 0. The earlier
misdirected CLI resume attempt remains separately documented as a failure.
final-audit.json records credential preservation, no secret byte matches and
removed staged settings. Source freeze, binary manifests, full JUnit XML and
final owned-doc hashes are separate artifacts. Final graduation is approved
by agent:/root/independent_review; independent runtime evidence review is
approved by agent:/root/notification_research. No commit or push is performed.


Final RFC 0022 graduation: **APPROVED**, agent:/root/independent_review.
The independent reviewer recomputed 2591 frozen-source and 36 owned-path hashes,
read both complete JUnit files and exact registration delta, and verified scope,
2675-file preservation, source/binary identity, graph/inline/RFC lint and actual
Release lifecycle evidence. agent:/root/notification_research independently
reviewed all 12 raw frame prefixes, three normal exits, seven unique event IDs,
455 request messages without local metadata and every preserved XML context.
No P0/P1/P2 findings remain. Only RFC/index/guide approval text changes after
that gate; final-owned-hashes.json records this last documentation revision.
Production/test source and both verified binaries remain frozen. All commands
ran from the attached isolated checkout using source identical to the shared
working tree; the final live binaries are retained with the evidence. No commit,
push, REA, production deployment or hosted CI release is claimed.


## Commit-scope validation (2026-10-17)

The final submission selects only subagent delegation, session-scoped activity,
layered Tasks/Subagents/Agent Teams, compact notifications, transcript geometry
and typed task events. It retains unrelated accessibility, contrast, reduced
motion and RFC 0019 work as uncommitted changes in the primary checkout.
The four mixed files are selected by hunk: docs/README.md, the decision register,
prompt_input_footer.cppm and tests/ui/CMakeLists.txt. Their unrelated content is
preserved. The primary working source bytes are unchanged; README's RFC 0022
status no longer says uncommitted, and this validation record is appended.

The attached isolated checkout is reconstructed from the selected Git tree,
with its preceding complete snapshot saved under isolated-before/. The tested
tree is 91ea48e2971eaa71b2897f59dcceaa3721745c13. The final code commit tree is
d9e9e81c6579848d123b8568be5a7c5df5f2e3f9, differing only in README's status
wording. All 2586 source/test/tool/CMake inputs are byte-identical to the tested
tree and remain frozen. The following documentation commit changes this guide
and the two RFC history records only. No implementation behavior changes during commit preparation.

Both presets are freshly configured and built with default Ninja parallelism
and -Werror; Release retains -O2 -DNDEBUG. Full CTest runs serially with -j1.
These are new executions against the selected submission, not reused full-suite
results from the prior combined working tree.

| Preset | Build exit | Build time | Registered | Passed | Skipped | Disabled | Failed | Full test time |
|---|---|---|---|---|---|---|---|---|
| Debug | 0 | 106.86 s | 2842 | 2817 | 23 | 2 | 0 | 477.37 s |
| Release | 0 | 228.55 s | 2842 | 2817 | 23 | 2 | 0 | 406.77 s |

Both registration sets are identical. Compared with the previous combined
2901-test working snapshot, the selected tree omits 59 uncommitted tests:
20 TextContrastControls, 13 TextContrast, 12 ReducedMotion, 10 DialogTextContrast
and four ReducedMotionCursor. No subagent tests are removed. Those source files
and their five CMake entries remain in the primary working tree. The 23 skips
and two pre-existing disabled tests are not reported as passes.

Graph, core/UI/store/target and inline checks pass; RFC lint passes all 21
selected documents. The 22nd document in the combined checkout is unrelated
uncommitted RFC 0019. Scope and per-commit dependency review is APPROVED by
agent:/root/independent_review. The review corrected RFC 0021 placement to the
complete runtime/UI commit and removed the uncommitted label from RFC 0022's
index entry. build/subagent-commit-validation-20261017 retains the prospective
trees, patches, original working-file bytes/hashes, full JUnit, source/binary
manifests and commit preservation evidence.

The six behavior commits are:

| Commit | Logical change |
|---|---|
| d39b6c1b | fix(query): preserve UTF-8 at tool output limits |
| abe7f452 | fix(ui): align virtual history geometry with rendering |
| 870c54ae | feat(agent): guide scoped parallel delegation |
| eacf613b | feat(ui): separate tasks, subagents and team activity |
| 24be27c1 | fix(ui): render compact background task notifications |
| cffc8498 | feat(agent): separate task events from model context |

The preceding real-endpoint/PTy evidence remains tied to its recorded binaries;
this commit-only follow-up does not claim a new external-endpoint run. The native
full suites include the public PTY workflows. Windows/Linux, ASan, hosted CI and
PSS remain unverified. Structured event delivery retains its documented best-
effort crash semantics. No REA is used.


## Deterministic background sleep cancellation (2026-10-19)

The earlier first-pass failure did not retain its terminal error, so its exact
cancellation phase cannot be recovered. A controlled native GTest experiment
now demonstrates the test defect without relying on retries: a synchronous
Store observer pauses the worker immediately after publishing its assistant
transcript, before the pre-tool cancellation checkpoint. TaskStop succeeds;
releasing the worker produces `cancelled before executing tool sleep`. The old
predicates nevertheless fail all three assertions, including a 5883 ms latency
measurement, because they poll for the conjunction of Cancelled and the different
`while executing tool sleep` reason. The latency includes the exhausted polling
loop and does not establish a slow cancellation operation. Raw red XML, output
and the controlled old-predicate source remain in
`build/agent-cancel-validation-20261019/`.

The owning fix is test-local. Existing synchronous Store observers and the live
permission callback establish two deterministic phases: cancellation before sleep
dispatch, and pending cancellation at the sleep handoff after the pre-dispatch
checkpoint. The latter is explicitly named AtSleepToolHandoff; it does not claim
that a condition-variable wait has begun. A promise observes the first terminal
record and its steady-clock timestamp independently of its error text. State,
phase and latency are asserted separately, and TaskOutput must preserve the stopped
status and phase. Both cases forbid a second model request. Cleanup releases gates,
unsubscribes, cancels and joins workers before destroying captured state or removing
runtime data, including fatal-assertion paths. No arbitrary 30 ms sleep or enlarged
latency threshold remains.

This replaces one misleading DuringSleepToolExecution test with two explicit
phase regressions: one intentional registration rename and one added case. No
production module, shared test hook, API, configuration, persisted data or wire
contract changes. Actual in-flight cancellation is separately checked in the real
native PTY by sampling the worker stack through AgentTool and the Sleep/AbortSignal
condition-variable wait, then issuing Stop through the instance details UI and
checking first terminal observation, task output and model request count. A watcher
thread's 5 ms sleep is not evidence of the tool's wait. Independent diagnosis:
`agent:/root/urlwrap_cancellation_failure_audit`; source/final review:
`agent:/root/settings_wordwrap_review`. Debug/Release complete-suite and real-PTY
results will be appended after execution. No commit or push is part of this task.


Final frozen cancellation validation:

| Preset | Build seconds | Focused passed | Registered | Passed | Skipped | Disabled | Failed | Full seconds |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Debug | 9.84 | 16/16 | 3004 | 2979 | 23 | 2 | 0 | 502.78 |
| Release | 17.34 | 16/16 | 3004 | 2979 | 23 | 2 | 0 | 432.48 |

These are fresh default-preset builds with -Werror and complete unfiltered serial
CTest executions on the final deadline-corrected input. Both full first passes
succeed without retries; the deliberately failing controlled old-predicate red
experiment remains separate. Every complete XML test name appears exactly once
and matches registration, including the intentional two-addition/one-removal
rename. The earlier successful pre-review builds remain separately archived;
they are not substituted for these final executions. Graph/core8/UI9/store/TLL,
inline-definition, theme and RFC lint pass.

The native Debug PTY uses the frozen binary SHA-256
`1e8d7d98d62d8f18f2eb80cc83789b9b4035211acca5354f22ae8590c1cd2803`.
A controlled loopback SSE endpoint launches a real 300-second sleep. macOS sample
shows the worker in AgentTool, execute_agent_sleep_tool, SleepTool::execute,
AbortSignal::wait_for and its condition-variable wait; the watcher is separate.
The user-facing instance detail Stop key produces Cancelled with the exact
`while executing tool sleep` reason. First persisted terminal observation follows
the input-write timestamp by 28.816 ms with 5 ms sampling. This wall-clock
observation is not a monotonic performance benchmark; native regressions separately
use the stop-relative steady-clock deadline. Screens show running/Stop, cancelled
with its reason/Continue, and the ended instance in History. Exactly one worker
model request occurs, with no continuation request over 249.571 seconds after Stop.
The parent only spawns the worker and completes. No external provider run is claimed.

The native child exits 0 after Ctrl+D; owned servers are terminated and reaped,
staged dummy settings are removed, and original user settings retain hash/mode/mtime.
Input preservation covers 2697 Git-visible files and 218 executable hashes in the
primary and attached cached checkout. Unowned bytes/modes and tracked symlink
targets remain unchanged. Product/test/executable bytes stay frozen through full
execution; only this result-only documentation append receives a final controlled
hash. A stat-versus-lstat evidence-checker mismatch was corrected consistently
with the before snapshot and separately recorded, without changing any input.
Final independent evidence verdict is retained in the source-review report.

The historical first-pass terminal error remains unrecoverable, so its precise
phase is not retrospectively asserted. The test defect is demonstrated by the new
controlled red experiment and covered by deterministic phase regressions plus an
actual in-flight terminal run. Other platforms, ASan/TSan, hosted CI and skipped
live-provider cases remain unverified. No production/shared module or established
public/persisted/wire contract is modified. Both related files remain uncommitted.
