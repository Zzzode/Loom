# RFC 0001 follow-up — `cc_entrypoints` → `cc_sdk`: reposition the embeddable surface (design, agent-reviewed)

Status: design proposal (independent read-only analysis, 2026-09-28). Phase 1
(the rename) is approved for implementation; phases 2–3 are unscheduled.

Product input: Loom must theoretically remain usable **as a library /
embeddable binary**. The current `cc_entrypoints` island survives for that
purpose but its name is wrong; renaming/restructuring is approved, building
the embed feature is not.

## 1. Inventory verdict

8 modules / 1936 LOC under `src/entrypoints/` (target `cc_entrypoints`):
`core_schemas` (470; Zod-derived DTOs, no ser/de), `core_types` (284; SDK
wire-message family), `control_schemas` (497; the 21-subtype stdio/WS
control protocol), `control_types` (152; Stdin/Stdout message variants),
`runtime_types` (184; `Options`, `MessageCallback`, `QueryHandle`,
`SDKSession` — the only behavioral shapes), `sandbox_types` (158),
`settings_types` (99; loose optional<string>-bag Settings), `sdk_types`
(92; a smaller hand model — Role/4-member ContentBlock/6-value
ControlRequestType; orphan even inside the island).

Findings:
- No runtime, no `from_json`/`to_json`, no impl units. All TUs import only
  `std` + siblings — the target's own `PUBLIC cc_utils cc_state cc_config`
  link is entirely dead.
- It models the **CLI-as-subprocess** SDK protocol (types literally named
  `StdinMessage`/`StdoutMessage`; "used by SDK builders to communicate with
  the CLI process"), not a link ABI.
- Duplicates and has **drifted from** live twins: two content-block models
  (4 vs the live 6 members in cc.types.types), two Settings (string bag vs
  cc.config typed sections), two EffortLevel, two ControlRequest shapes
  (21 JSON subtypes vs a 6-enum hand set). The live binary instead uses
  `cc.server.types` (full yyjson ser/de; the direct-connect HTTP/SSE DTOs)
  and hand-rolled JSON in `cc.bridge` — zero importers of the island.
- The de-facto embedded harness already exists inside an HTTP route:
  `server_routes.cppm::detail::execute_native_query()` (~:704-838) assembles
  ConfigManager → QueryEngineConfig → install_runtime_backends() →
  ToolRegistry + register_runtime_tools + missing-tool backend + MCP
  providers → QueryEngine.query(), in-process. The future embed API is that
  recipe extracted into a reusable handle.

**Verdict:** not a foundation to build on verbatim — preserve the 21-subtype
requirements catalogue and the three `runtime_types` shapes; when the embed
feature lands, types converge onto `cc.types.types` + `cc.config` +
`cc.server.types`.

## 2. Decision: `cc_entrypoints` → `cc_sdk`

Target `cc_sdk`, directory `src/sdk/`, modules `cc.sdk.*`, leaf
sub-namespaces preserved. The installed package later exposes it as
`loom::sdk` via CMake export namespace (target names are not C++-visible);
C++ namespace stays `cc::sdk.*` for now — promoting the module import
string to `loom.sdk` is a phase-3 one-time public-ABI decision.

Runner-up `cc_embed` remains free to choose pre-consumer; `sdk` covers both
future outcomes (in-process API + spawn/subprocess client).

## 3. Phase 1 rename spec (rename-only batch)

| Old | New module |
|---|---|
| entrypoints/core_schemas.cppm | sdk/core_schemas.cppm → `cc.sdk.core_schemas` |
| entrypoints/core_types.cppm | sdk/core_types.cppm → `cc.sdk.core_types` |
| entrypoints/control_schemas.cppm | sdk/control_schemas.cppm → `cc.sdk.control_schemas` |
| entrypoints/control_types.cppm | sdk/control_types.cppm → `cc.sdk.control_types` |
| entrypoints/runtime_types.cppm | sdk/runtime_types.cppm → `cc.sdk.runtime_types` |
| entrypoints/sandbox_types.cppm | sdk/sandbox_types.cppm → `cc.sdk.sandbox_types` |
| entrypoints/settings_types.cppm | sdk/settings_types.cppm → `cc.sdk.settings_types` |
| entrypoints/sdk_types.cppm | **sdk/types.cppm → `cc.sdk.types`** (avoids stutter; prime phase-2 deletion candidate) |

Namespaces `cc::entrypoints::*` → `cc::sdk::*` (sdk_types already `cc::sdk`).
Edits: 8 module-decl + 7 namespace-decl + 7 intra-island import lines (~22
lines). CMake: rename `cc_entrypoints.cmake`→`cc_sdk.cmake`, drop its dead
link libs; remove the target from `cc_core.cmake` and `cc_coordinator.cmake`
(core must not depend on the public surface — direction inversion); move the
include() in `src/CMakeLists.txt` to immediately before `loom.cmake` (after
cc_core). Graph: replace `TARGET_RANK["cc.entrypoints"] = 15` with
`"cc.sdk": 16` (above orchestration/CLI; all future sdk→harness edges legal,
nothing internal points up at it; fail-closed unranked check enforces same
commit). No baseline rows exist. No tests/CI/presets reference the island.

## 4. Phases 2–3 (future, unscheduled)

- **Phase 2 — type reconciliation:** adopt cc.types.types content
  blocks/messages and cc.config settings/sandbox as canonical (alias then
  delete twins; delete orphaned cc.sdk.types); move process-boundary control
  DTOs to cc.server/cc.bridge WITH cc.utils.json ser/de (the 21 subtypes
  become the spawn/remote client schema); in-process control becomes C++
  callbacks; collapse the duplicate EffortLevel.
- **Phase 3 — minimal embed entrypoint:** new `cc.sdk.harness` (+impl unit)
  with an opaque PIMPL handle: `HarnessConfig` (model/cwd/budget/permission
  mode + injected api_key_provider, backend_factory (WireBackend seam),
  permission_callback, event_sink), `Harness::run/stream/abort`, backed by
  QueryEngine/query|stream_query|abort, the extracted execute_native_query
  assembly recipe, ToolPermissionHook ask-user bridging, NativeMcpRuntime
  for MCP, QueryEngine session storage for resume/fork, and QueryResponse for
  TurnResult. Gate: the server route itself re-expressed through Harness as
  the first in-tree consumer before any external commitment; revive the root
  CMakeLists install block (`install(EXPORT … NAMESPACE loom::)`, BMI
  install story); decide the `loom.sdk`/`loom::sdk` promotion then.

`loom --server` already serves the "embeddable binary" half (direct-connect
HTTP/SSE).

## 5. Why rename now

The zero-importer window is the global cost minimum (~22 lines + cmake/rank;
zero binary impact — the island's symbols are in no executable); the current
name is actively wrong and was one product decision away from graveyard
deletion; the name is robust to every phase-2 outcome; deferring only hides
the substantive type decisions inside a mechanical diff later.
