# Context compaction

RFC 0025 gives the main query engine and native child agents the same context
budget and semantic checkpoint policy. The shared `loom.services.compact.context_compaction`
module owns token estimation, safe history boundaries, bounded summarization and
candidate validation. Each owner supplies its provider transport, cancellation,
accounting and durable checkpoint commit.

Before a business request, the policy reserves the configured output tokens and
`max(64, window / 50)` safety tokens, and also honors the configured compaction
threshold. It includes system instructions, tool schemas and every content
block in its estimate. Provider input usage calibrates the next estimate;
Messages cache-read and cache-creation tokens count as input. This remains an
estimate, particularly for media and opaque provider blocks.

A checkpoint has six nonempty sections: Goal, Constraints, Decisions, Progress,
Next Steps and References. The original latest human request and a bounded tail
of complete tool exchanges remain verbatim. Earlier complete exchanges within
the same task can be summarized. Runtime continuation prompts and typed task
events cannot replace the human request anchor. Summarization has no tools or
business hooks and consumes usage/cost, but does not consume a business turn.

Known context-length errors, including HTTP 200 in-band stream errors, get at
most one compaction recovery per business sampling step. Generic HTTP 413
recovery additionally requires a strictly smaller serialized request body.
Authentication, rate-limit and server errors retain their ordinary handling.
An invalid, truncated, cancelled, oversized or ineffective checkpoint leaves
the original working history intact. The same is true of a checkpoint write
failure. `auto_compact=false` disables the proactive trigger; explicit `/compact`
and reactive overflow recovery remain available.

## Model-aware budgets (RFC 0026)

The active model resolves its context window and output cap independently.
The context window uses an exact endpoint/wire/model profile, a generic
exact-model profile, an applicable official-endpoint catalog entry, then the
configured global fallback. The output cap uses the first present cap in the
scoped profile, generic profile or applicable catalog; without a cap, effective
output equals the requested output. A context-only profile retains any
applicable generic or catalog output cap. The catalog is capacity metadata, not a model availability
or recommendation list. A model name on a custom proxy never activates official
catalog limits. There is no network capacity probing or arbitrary alias discovery.

Configure private models once in `settings.json`; switching the loaded models
then updates budgets automatically:

```json
{
  "contextWindowSize": 200000,
  "maxOutputTokens": 64000,
  "modelContextLimits": [
    {"model": "large-model", "contextWindowSize": 1000000},
    {"model": "small-model", "contextWindowSize": 256000,
     "maxOutputTokens": 16384,
     "baseUrl": "https://gateway.example/v1", "wireApi": "openai"}
  ]
}
```

Use the provider's exact numeric limits: a label such as 256K does not establish
whether its value is 256000 or 262144. `baseUrl` and `wireApi` appear together;
the supported canonical wire names are `messages` and `openai`. Omit both for
an exact-model profile applying at any endpoint. Matching removes trailing
slashes only; tenant paths remain distinct. `baseUrl` scopes capacity lookup;
it does not change the request endpoint or credentials. Scoped URLs cannot contain userinfo,
query, fragment or control characters. Invalid entries and duplicate identities
within one file fail the configuration load. Higher tiers overlay matching
profiles; an empty array clears the set. Ordinary project saves preserve its
own profile JSON without copying effective user/local profiles into the project.

The old global `contextWindowSize` is now a fallback, including an explicitly
configured non-default value. To keep a deliberate ceiling for a catalog model,
use an exact profile, for example
`{"model":"gpt-4.1","contextWindowSize":100000}`. Profiles are file-only and
do not expand the scalar configuration-tool interface. Editing profile files
requires restarting Loom or rebuilding QueryEngine; `/clear` is insufficient.
Already loaded model switches, SDK overrides and existing overload fallback
update budgets before sampling without restarting. Native children and their
descendants resolve their own selected model from the same immutable policy,
including after resume; they do not reuse the parent's resolved capacity.

Output requests remain reversible: the wire uses the smaller of requested
output and a known model output cap, while switching back can restore the
original request. Summary output respects the model cap independently from the
business output setting. An impossible output reserve still reports an explicit
error. For Messages Forced thinking, the manual thinking budget must be below
effective output; a smaller model cap that violates this is rejected before
HTTP without changing the configured thinking request. Switching back restores
the original request. `QueryEngine::model_context()` exposes the resolved window, effective
output and separate `source` / `output_cap_source` values; the status line uses
the active resolved window. An AppState-only model list reports unknown context
instead of mislabeling its output-token field as a detected window.

## Persistence

Main sessions keep the full original `messages.jsonl` audit. The owner writes
stable IDs on new audit messages; older messages remain readable. It verifies
the referenced audit message is on disk before committing a checkpoint. It writes
version 1 `context_checkpoint` snapshots and `context_request_anchor` records to
`context-checkpoints.jsonl`. A checkpoint includes the audit anchor, canonical
working messages, human request anchor and task-budget remaining value. Resume
uses the latest valid checkpoint plus subsequent audit messages. Old sessions
without checkpoints retain their existing restore path.

Older audits without owner anchor metadata use the typed-message fallback.
Ordinary request-anchor metadata is best-effort; successful checkpoints require
the checked durable commit described above. Static instructions or the latest
request can be too large to compact safely, in which case the owner reports an
error and retains its working history.

Native child sidechains carry additive version 1 `context-checkpoint` records
with the retained API message snapshot and request anchor. The authoritative
agent record is atomically persisted before publishing the working snapshot.
Original sidechain messages remain available for audit. Unknown or malformed
checkpoint records do not replace valid history. The session-memory `summary.md` file
is a best-effort convenience view; it is not the recovery source.

## Verification

The native test targets are `test_context_compaction`, `test_model_context`,
`test_wire_transport`, `test_query_engine_svc`, `test_sdk_harness`,
`test_command_registry` and `test_agent`. They cover both
wire protocols, proactive and reactive paths, long tools, closed parallel tool
groups, original request retention, repeated overflow, malformed summaries,
cancel/failure paths, write failure and resume.

Model-budget tests additionally exercise actual 1,000,000/256,000 profiles through
captured loopback HTTP requests: synchronous/streaming switches, SDK overrides,
overload fallback, mixed parent/child models, grandchildren and resume. They
verify independent output caps, restoration of requested output, scoped policy
isolation and rejection of impossible manual thinking before HTTP. These tests
establish local resolution and request behavior, not a remote provider's actual
capacity or semantic summary quality.

Two opt-in tests use the configured real provider and expose only a local
read-only arithmetic fixture. They assert a late random constraint and the
verified result `47` survive the actual summary and final continuation:

```bash
export LOOM_COMPACTION_LIVE_SETTINGS=/absolute/path/to/isolated-settings.json
export LOOM_COMPACTION_LIVE_EVIDENCE=/absolute/path/to/private-evidence
build/debug/tests/query/test_compaction_live --gtest_filter='CompactionLive.*'
build/debug/tests/tools/test_agent --gtest_filter='CompactionLive.*'
```

The settings file must contain the intended provider, model and credentials.
Keep it private and outside version control. An optional
`LOOM_COMPACTION_LIVE_WIRE_API` selects the backend; the tests also accept
`LOOM_COMPACTION_LIVE_AUTH_TOKEN` for bearer authentication. Without explicit
settings the live tests skip. A skipped live test is not provider acceptance
evidence. Deterministic signature-retention tests establish byte preservation;
the live tests disable thinking and do not establish every provider's signed
thinking compatibility after prefix replacement.
