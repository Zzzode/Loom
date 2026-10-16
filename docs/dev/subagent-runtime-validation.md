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
