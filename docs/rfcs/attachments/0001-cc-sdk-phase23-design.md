# RFC 0001 follow-up — cc_sdk phases 2–3: type convergence and the opaque `cc.sdk.harness` embedding entrypoint (design)

Status: design proposal (independent read-only analysis, 2026-09-29). Phase 1
(the rename, c11) is implemented; phases 2–3 are unscheduled and this document
specifies them. The verification gate is the **local** `local-linux` /
`local-linux-release` dual-preset build + serial `ctest -j1`; GitHub CI is not
a gate and must not be required.

Product input (unchanged from the c11 design): Loom must remain usable as a
library / embeddable binary. `loom --server` already serves the "embeddable
binary" half (direct-connect HTTP/SSE). Phases 2–3 build the "linkable library"
half: converge the SDK DTOs onto the live canonical types, then extract the
in-process query recipe into an opaque embedding handle.

## 0. Current state (verified 2026-09-29)

### 0.1 The island

8 modules / 1936 LOC under `src/sdk/` (target `cc_sdk`, graph rank 16, zero
importers outside the island — verified by grep for `import cc.sdk` and
`cc::sdk` across `src/` and `tests/`):

| Module | LOC | Content |
|---|---|---|
| `cc.sdk.types` | 92 | Orphan hand model (Role/4-member ContentBlock/6-value ControlRequestType). Zero importers even inside the island. |
| `cc.sdk.core_schemas` | 470 | Zod-derived DTOs (ModelUsage, McpServerConfig variant, PermissionMode, HookEvent 28, AgentDefinition, …). No ser/de. |
| `cc.sdk.core_types` | 284 | Re-exports + the SDK stdout wire-message family (SDKUserMessage … SDKToolProgressMessage, 10-member SDKMessage variant). |
| `cc.sdk.control_schemas` | 497 | The 21-subtype stdio/WS control protocol + ~20 context-usage sub-structs. No ser/de. |
| `cc.sdk.control_types` | 152 | Aliases + 4 runtime-only request structs + Stdin/Stdout message variants. |
| `cc.sdk.runtime_types` | 184 | `Options`, `MessageCallback`, `QueryHandle`, `SDKSession` — the only behavioral shapes. |
| `cc.sdk.sandbox_types` | 158 | `SandboxSettings` + network/filesystem/ripgrep config. **No live twin.** |
| `cc.sdk.settings_types` | 99 | Loose `optional<string>`-bag `Settings` + `MergedSettings`. |

All TUs import only `std` + siblings; the target has no link libraries
(`src/cmake/targets/cc_sdk.cmake`). The island is type-only and unwired.

### 0.2 The embedding recipe (the phase-3 source)

`cc.server.server_routes` `detail::execute_native_query()`
(`src/server/server_routes.cppm:704-838`) already assembles the full engine
in-process:

1. `cc::core::ConfigManager manager; manager.load();` (`:709-712`)
2. Build `cc::core::QueryEngineConfig` from settings (api_key, base_url, model,
   max_tokens, temperature, context_window, retry, thinking, cwd,
   always_deny_rules) (`:714-731`)
3. `permission_handler_for_session` → `cc::hooks::ToolPermissionHook` with
   `set_ask_user_response_fn` bridging to `DirectPermissionRequest`/handler,
   with session-state caching (`:737-769`)
4. `cc::orchestration::install_runtime_backends()` (`std::call_once`-guarded)
   (`:776`)
5. `cc::core::ToolRegistry registry; cc::tools::register_runtime_tools(registry,
   RuntimeToolOptions{.permission_check = …})` (`:778-791`)
6. `registry.set_missing_tool_handler(cc::orchestration::make_missing_tool_backend())`
   (`:797-798`)
7. `config.tools = registry.get_visible_definitions()` (`:800`)
8. `config.dynamic_tools_provider` / `config.mcp_input_schema_provider` →
   `collect_mcp_tool_definitions` / `collect_mcp_input_schemas` (`:804-810`)
9. `cc::core::QueryEngine engine(std::move(config), registry)` (`:812`)
10. `engine.set_external_abort_callback(cancel_flag)`;
    `engine.set_permission_hook(&permission_hook)` (`:813-820`)
11. `seed_query_engine_from_session(engine, prior_message_lines)` (`:821`)
12. `engine.query(request.content)` → `QueryResponse{message, total_usage,
    tool_rounds, elapsed}` (`:826-827`)
13. Map to `DirectQueryResult{assistant_id, content=assistant_text, model,
    input_tokens, output_tokens, tool_rounds, elapsed_ms}` (`:829-837`)

### 0.3 The canonical type targets

- **`cc.types.types`** (`src/types/types.cppm`): `Role` (4, +Tool, `:55`),
  `ContentBlock` (6-member variant: Text/ToolUse/ToolResult/Image/Document/
  Thinking, `:168`), `Message` (5-member variant, `:234`), `TokenUsage`
  (`:252`), `StreamEvent` (9-member, `:333`), `Error`/`ErrorCode`/`Result`
  (`:357-428`), strong IDs (`:42-48`).
- **`cc.config.config`** (`src/config/config.cppm`): typed `Settings`
  (ModelSettings/PermissionSettings/DisplaySettings/NetworkSettings/
  FeatureFlags/mcp_servers/system_prompt/custom_instructions/XaaIdpSettings,
  `:146-156`), `ConfigManager` (`:255`).
- **`cc.config.mcp_types`** (`src/config/mcp_types.cppm`): flat
  `McpServerConfig` (`:20`) + `McpOAuthConfig` (`:12`).
- **`cc.config.settings`** (`src/config/settings.cppm`): `SettingsScope`
  (User/Project/Local, `:181`), `SettingValue`, `SettingsEntry`.
- **`cc.server.types`** (`src/server/types.cppm`): the direct-connect HTTP/SSE
  DTOs with full yyjson ser/de (`DirectQueryRequest`/`Result`,
  `DirectPermission*`, `DirectQueryStreamChunk`, `ServerSession`, …) — the
  established ser/de pattern (free `X_to_json`/`X_from_json` + ADL
  `to_json`/`from_json`).
- **`cc.utils.json`** (`src/utils/serdes/json.cppm`): `JsonVal`/`JsonMutDoc`/
  `parse` — the ser/de engine.
- **`cc.utils.effort`** (`src/utils/model/effort.cppm:8`): `EffortLevel`
  (Low/Medium/High/Max) — identical to the SDK's two duplicates.
- **`cc.tools.mode_validation`** (`src/tools/mode_validation.cppm:28`):
  `PermissionMode` (kDefault/kAcceptEdits/kBypassPermissions/kDontAsk).
- **`cc.tools.agent_runtime`** (`src/tools/agent_runtime.cppm:162`):
  `AgentDefinition` (the engine's agent shape).
- **`cc.hooks.tool_permissions`** (`src/hooks/tool_permissions.cppm`):
  `PermissionDecision` (`:13`), `PermissionResponse` (`:20`),
  `PermissionRule` (`:27`), `PermissionContext` (`:59`), `ToolPermissionHook`
  (`:89`).
- **`cc.session.storage`** (`src/session/storage.cppm`): `SessionMetadata`
  (`:29`), `list_recent_sessions`/`load_session_metadata`/`append_message`.
- **`cc.query.query_engine`** (`src/query/query_engine.cppm`):
  `QueryEngineConfig` (`:170`), `QueryOptions` (`:243`), `QueryResponse`
  (`:263`), `StreamCallback` (`:240`), `QueryEngine` (`:313`) with
  `query`/`stream_query`/`abort`/`set_session_storage`/`set_dump_prompts_dir`.
- **`cc.query.wire_protocol`** (`src/query/wire_protocol.cppm`): `WireBackend`
  (`:174`), `RequestInput`/`PreparedRequest`/`ParsedResponse`/`StreamDelta`.

### 0.4 The live control-protocol speakers (hand-rolled today)

- `cc.server.server_main` (`src/server/server_main.cppm`): hand-rolls
  `control_request_subtype` (`:445`), `control_request_id` (`:458`),
  `control_response_decision` (`:470`), `permission_control_request_json`
  (`:526`, raw `operator<<` JSON string building) over WebSocket frames.
- `cc.bridge.bridge_messaging` (`src/bridge/bridge_messaging.cppm:78-96`):
  hand-rolls `SDKControlRequest`/`SDKControlResponse` (subtype/mode/model/
  max_thinking_tokens only) with `is_sdk_control_*` type guards.

Both speak a subset of the 21-subtype protocol the island models, but neither
imports the island — they hand-roll the JSON. This is the drift the c11 design
flagged ("hand-rolled JSON in cc.bridge").

---

## 1. Phase 2 — type convergence

### 1.1 Principles

1. **The canonical type wins.** Where a live twin exists in `cc.types` /
   `cc.config` / `cc.tools` / `cc.hooks` / `cc.utils` / `cc.session`, the SDK
   type is replaced by a `using`-alias of the canonical type, then the twin is
   deleted. Aliases ship first (one commit), deletion follows (next commit) so
   any out-of-tree consumer has a migration window.
2. **Wire DTOs move down, not up.** The 21-subtype control protocol is a
   process-boundary wire schema that the live binary (server/bridge) speaks.
   It must live at a rank the live binary can import (≤ 13), never at the rank-16
   apex — otherwise the live binary would import upward to the SDK, re-inverting
   the dependency direction c11 fixed.
3. **The island stays free of engine/HTTP/services linkage.** Phase 2 adds
   ser/de (free functions over `cc.utils.json`) and CONVERGE aliases that
   `import` the canonical type modules (`cc.types`, `cc.config`,
   `cc.tools.mode_validation`/`agent_runtime`, `cc.hooks`, `cc.session`). Those
   imports add link dependencies on the canonical **type** targets (for their
   BMIs): `cc_utils`, `cc_types`, `cc_config`, `cc_constants`, `cc_tools`,
   `cc_hooks`, `cc_session` (transitively `cc_state`, `cc_skills_core`,
   `cc_task_types`, `cc_vim`, `yyjson`, `uv_a`). What phase 2 does **not** add
   is engine/runtime/HTTP linkage: no `cc_query`, no `cc_server`, no
   `cc_services` — so `OpenSSL`/`httplib`/`CURL::libcurl` never enter the SDK
   closure. (The earlier "gains only `cc_utils`" wording was imprecise: the
   CONVERGE aliases inherently require the canonical type targets' BMIs. The
   load-bearing property is the absence of engine/HTTP/services linkage, not a
   single-dep closure.)
4. **Zero-importer window.** Every deletion is safe today (zero importers
   outside the island); the alias-then-delete sequence keeps it safe.

### 1.2 Per-DTO mapping

Legend: **CONVERGE** = alias canonical type, delete twin. **MOVE** = relocate to
the control-protocol module (§1.3) with ser/de. **STAYS** = no live twin; keep
in the island. **DELETE** = orphan/drifted subset with no successor.

#### `cc.sdk.types` (types.cppm) — DELETE ENTIRE MODULE

| SDK type | Canonical target | Action |
|---|---|---|
| `Role` (3) | `cc::core::Role` (4, +Tool) — types.cppm:55 | DELETE twin |
| `ContentBlockType` (5) | (none — `std::variant` discriminates) | DELETE |
| `TextBlock` | `cc::core::TextBlock` — types.cppm:78 | DELETE twin |
| `ToolUseBlock` (string id) | `cc::core::ToolUseBlock` (`ToolUseId`) — types.cppm:83 | DELETE twin |
| `ToolResultBlock` (string content) | `cc::core::ToolResultBlock` (`variant<string, vector<ToolResultContentItem>>`) — types.cppm:103 | DELETE twin |
| `ThinkingBlock` (no signature) | `cc::core::ThinkingBlock` (+`signature`) — types.cppm:162 | DELETE twin |
| `ContentBlock` (4) | `cc::core::ContentBlock` (6) — types.cppm:168 | DELETE twin |
| `Message` (flat struct) | `cc::core::Message` (5-member variant) — types.cppm:234 | DELETE twin |
| `ControlRequestType` (6) | `cc::sdk::control` 21-subtype protocol | DELETE (drifted subset) |
| `ControlRequest` | `cc::sdk::control::ControlRequest` — control_schemas.cppm:453 | DELETE twin |
| `SessionInfo` | `cc::session::SessionMetadata` — storage.cppm:29 | CONVERGE |

This module is the prime phase-2 deletion candidate (c11 design §3): zero
importers, every type has a live twin or is a drifted subset.

#### `cc.sdk.core_schemas` (core_schemas.cppm)

| SDK type | Canonical target | Action |
|---|---|---|
| `ModelUsage` | `cc::core::TokenUsage` — types.cppm:252 (token fields); `cost_usd`/`context_window`/`max_output_tokens`/`web_search_requests` have no type-layer twin (live in `BudgetTracker`/`QueryEngineConfig`) | **Whole-struct MOVE to rank 13, all 8 fields retained.** The wire struct moves with the wire family (§1.3) because `SDKResultSuccess`/`SDKResultError` embed `unordered_map<string, ModelUsage>` (core_types.cppm:101,127) and carry every field on the wire — the token fields (`input_tokens`/`output_tokens`/`cache_read_input_tokens`/`cache_creation_input_tokens`) are NOT stripped. The CONVERGE onto `TokenUsage` is a separate concern: it applies to the SDK's standalone token-accounting type `NonNullableUsage` (core_types.cppm:43) and to consumers wanting the canonical type, not to the wire struct's fields. The four no-twin fields (`web_search_requests`/`cost_usd`/`context_window`/`max_output_tokens`) are retained in the moved wire struct as-is. **Prerequisite to the MOVE, not deferred.** **Wire spelling (corrected 2026-10-01):** the result-message field is `modelUsage` (camelCase) and the `ModelUsage` sub-fields are camelCase on the wire (`inputTokens`/`outputTokens`/`cacheReadInputTokens`/`cacheCreationInputTokens`/`webSearchRequests`/`costUSD`/`contextWindow`/`maxOutputTokens`), matching the TS `ModelUsageSchema`/`SDKResultSuccessSchema` and the live emitters (server_main.cppm:596,610; server_routes.cppm:194; bridge_messaging.cppm:669). The C++ field names stay snake_case; only the JSON keys are camelCase. The earlier `model_usage` spelling in this design was the drifted form — see review history. |
| `ApiKeySource` (5) | (none) | STAYS (SDK-only telemetry; zero consumers — deletion candidate) |
| `ConfigScope` (3) | `cc::config::SettingsScope` — settings.cppm:181 | CONVERGE (same 3 values) |
| `SDK_BETA` | (none) | STAYS (protocol constant) |
| `ThinkingConfig` (variant) | `cc::core::ThinkingConfig` (`Mode` enum + `budget_tokens`) — query_engine.cppm:163 | CONVERGE (map variant→`Mode`) |
| `OutputFormat` (`JsonSchemaOutputFormat`) | `cc::core::QueryEngineConfig::ResponseSchema` — query_engine.cppm:200 | CONVERGE |
| `McpServerConfig` (4-transport variant) | `cc::core::McpServerConfig` (flat) — mcp_types.cppm:20 | CONVERGE (variant→flat; `sse`/`http` = `transport`+`url`; the `sdk` in-process transport is runtime-only → `runtime_types`) |
| `McpServerStatus`/`McpServerInfo`/`McpServerCapabilities`/`McpConnectionStatus`/`McpServerTool`/`McpToolAnnotation` | (none in type layer; live MCP status is in `cc.services`/`cc.orchestration`) | STAYS (SDK-only status view) |
| `PermissionMode` (5) | `cc::tools::mode_validation::PermissionMode` (4) — mode_validation.cppm:28 | CONVERGE (note: the live tree itself has 10 `PermissionMode` defs — a separate consolidation, not phase-2 scope) |
| `PermissionUpdate`/`PermissionRuleValue`/`PermissionBehavior`/`PermissionAddRules`/`PermissionReplaceRules`/`PermissionRemoveRules`/`PermissionSetMode`/`PermissionAddDirectories`/`PermissionRemoveDirectories` | `cc::hooks::PermissionRule` + `PermissionDecision` — tool_permissions.cppm:27,13; server `DirectPermissionRule` — server_routes.cppm:53 | CONVERGE |
| `PermissionResult` (Allow/Deny) | `cc::hooks::PermissionResponse` — tool_permissions.cppm:20 | CONVERGE |
| `PermissionDecisionClassification` | (none) | STAYS |
| `HookEvent` (28) | `cc::hooks` lifecycle events — lifecycle_hooks.cppm:18-72 | PARTIAL (map the subset the engine emits; the rest STAYS) |
| `ExitReason` (6) | (none) | STAYS |
| `BaseHookInput` | `cc::hooks::PermissionContext` — tool_permissions.cppm:59 (partial overlap) | PARTIAL |
| `SlashCommand` | (none — `cc.types.command` has `ParsedCommand`, not `SlashCommand`) | MOVE (with the protocol — `ControlInitializeResponse.commands`, control_schemas.cppm:49; it is a control-channel wire shape, not an island-only type) |
| `AgentInfo` | `cc::tools::agent_runtime::AgentDefinition` — agent_runtime.cppm:162 (partial) | MOVE (with the protocol — `ControlInitializeResponse.agents`, control_schemas.cppm:50) |
| `AgentDefinition` | `cc::tools::agent_runtime::AgentDefinition` — agent_runtime.cppm:162 | CONVERGE (SDK shape is a subset) |
| `ModelInfo` | (none — `cc.utils.model` has `known_models()` strings only) | MOVE (with the protocol — `ControlInitializeResponse.models`, control_schemas.cppm:53) |
| `AccountInfo` | (none) | MOVE (with the protocol — `ControlInitializeResponse.account`, control_schemas.cppm:54) |
| `FastModeState` | (none) | MOVE (with the protocol — `ControlInitializeResponse.fast_mode_state`, control_schemas.cppm:56; also the wire messages `SDKResultSuccess`/`SDKPostTurnSummaryMessage`, core_types.cppm:103,130) |
| `SDKAssistantMessageError` | `cc::core::ErrorCode` — types.cppm:357 | CONVERGE |
| `SettingSource` (3) | `cc::config::SettingsScope` — settings.cppm:181 | CONVERGE (dup of `ConfigScope`) |
| `SdkPluginConfig` | `cc.plugins` (partial) | PARTIAL |
| `RewindFilesResult` | (none) | STAYS |

#### `cc.sdk.core_types` (core_types.cppm)

| SDK type | Canonical target | Action |
|---|---|---|
| All `using X = core_schemas::X` re-exports | — | Follow the `core_schemas` mapping |
| `NonNullableUsage` | `cc::core::TokenUsage` — types.cppm:252 | CONVERGE |
| `SDKUserMessage`/`SDKAssistantMessage`/`SDKResultSuccess`/`SDKResultError`/`SDKSystemMessage`/`SDKPartialAssistantMessage`/`SDKCompactBoundaryMessage`/`SDKStatusMessage`/`SDKToolProgressMessage`/`SDKPostTurnSummaryMessage`/`SDKStreamlinedTextMessage`/`SDKStreamlinedToolUseSummaryMessage` | (none — the live binary uses `cc.server.types` `DirectQuery*` + `cc.core.Message`) | MOVE (spawn/remote client schema → §1.3) |
| `SDKMessage` variant (10) | — | MOVE with the above |
| `SDKSessionInfo` | `cc::session::SessionMetadata` — storage.cppm:29 | CONVERGE |
| `HOOK_EVENTS`/`EXIT_REASONS` arrays | (none) | STAYS (protocol constants) |

#### `cc.sdk.control_schemas` (control_schemas.cppm) — MOVE

All 21 request subtypes, their responses, the ~20 context-usage sub-structs
(`ContextCategory`, `ContextGridSquare`, `MemoryFileInfo`, `McpToolInfo`, …),
`ControlRequest`/`ControlResponse`/`ControlCancelRequest` wrappers,
`KeepAliveMessage`, `UpdateEnvironmentVariablesMessage` → **MOVE** to the
control-protocol module (§1.3) with `cc.utils.json` ser/de.

**Move closure (F1):** the moved DTOs reference `core_schemas::SlashCommand`,
`ModelInfo`, `AccountInfo`, `FastModeState`, `AgentInfo` (control_schemas.cppm:
49,50,53,54,56) and the moved wire family references `ModelUsage` and
`FastModeState` (core_types.cppm:101,103,127,130). A rank-13 module cannot
import `cc.sdk.core_schemas` (rank 16) — that would be a new upward edge.
The move therefore takes the **whole wire closure** with it:

- The STAYS types the moved DTOs reference (`SlashCommand`, `ModelInfo`,
  `AccountInfo`, `FastModeState`, `AgentInfo`) are control-channel wire shapes
  (the `initialize` response carries them) and **MOVE with the protocol** —
  their per-DTO rows above are re-marked MOVE accordingly.
- The CONVERGE types the moved DTOs reference (`AgentDefinition`,
  `PermissionUpdate`, `SDKAssistantMessageError`) are **converged first** (the
  alias commit, §4.1) so the moved DTOs reference the canonical
  `cc::tools::AgentDefinition` / `cc::hooks::*` / `cc::core::ErrorCode` types.
- `ModelUsage` MOVEs as a whole struct with the wire family (see its row
  above) — all 8 fields are retained because `modelUsage` (camelCase on the
  wire; the C++ field is `model_usage`) carries them on the wire; the
  `TokenUsage` CONVERGE applies to the standalone `NonNullableUsage`
  type, not to the wire struct's fields.

After this, the moved module is self-contained at rank 13: it imports
`cc.utils.json` (rank 2) + the canonical type modules (`cc.types` rank 0,
`cc.tools` rank 8, `cc.hooks` rank 4) + `std` — no rank-16 import remains.

#### `cc.sdk.control_types` (control_types.cppm) — MOVE

Aliases follow `control_schemas`. The 4 runtime-only structs
(`SDKControlEndSessionRequest`, `SDKControlChannelEnableRequest`,
`SDKControlMcpAuthenticateRequest`, `SDKControlMcpOAuthCallbackUrlRequest`)
and the `StdoutMessage`/`StdinMessage` variants → **MOVE** with the protocol.

#### `cc.sdk.runtime_types` (runtime_types.cppm)

| SDK type | Canonical target | Action |
|---|---|---|
| `EffortLevel` | `cc::utils::EffortLevel` — effort.cppm:8 (identical 4 values) | DELETE dup |
| `Options`/`InternalOptions` | `cc::core::QueryEngineConfig` + `QueryOptions` — query_engine.cppm:170,243 | CONVERGE (SDK `Options` is a subset) |
| `MessageCallback` | `cc::core::StreamCallback` — query_engine.cppm:240 | CONVERGE |
| `QueryHandle` (abort/get_result) | phase-3 `Harness` handle (§2.2) | SUPERSEDED in phase 3 |
| `SDKSessionOptions`/`SDKSession` | phase-3 `Harness` (§2.2) | SUPERSEDED in phase 3 |
| `ListSessionsOptions`/`GetSessionInfoOptions`/`GetSessionMessagesOptions`/`SessionMutationOptions`/`ForkSessionOptions`/`ForkSessionResult` | `cc::session::storage` (`list_recent_sessions`, `load_session_metadata`, `load_messages`, …) | CONVERGE |
| `SdkMcpToolDefinition` | (none — in-process MCP tool injection) | STAYS until phase 3 (harness concern) |
| `McpSdkServerConfigWithInstance` (`shared_ptr<void>`) | (none — in-process MCP server) | STAYS until phase 3 (harness concern) |

#### `cc.sdk.sandbox_types` (sandbox_types.cppm) — STAYS

`SandboxSettings`/`SandboxNetworkConfig`/`SandboxFilesystemConfig`/
`RipgrepConfig` have **no live twin** (the live tree has only boolean/
command-level sandbox concepts — `should_use_sandbox`, `sandbox_toggle`). The
SDK is the only structured sandbox model. **STAYS** — candidate for promotion
to `cc.config` if the live tree ever adopts structured sandbox settings.

#### `cc.sdk.settings_types` (settings_types.cppm)

| SDK type | Canonical target | Action |
|---|---|---|
| `Settings` (loose string bag) | `cc::core::Settings` (typed sections) — config.cppm:146 | CONVERGE (the typed `Settings` is canonical; the string bag is the TS-era loose model) |
| `SettingsSource` (5) | `cc::core::ConfigSource` (5 tiers) — config.cppm:163 | CONVERGE |
| `SettingsLayer`/`MergedSettings` | `cc::core::ConfigManager` (already tracks provenance) | CONVERGE |

### 1.3 Control DTO ser/de layer

**New module: `cc.server.control_protocol`** (rank 13, in the `cc_server`
target). It owns:

- The 21-subtype control request/response DTOs (moved from
  `cc.sdk.control_schemas`).
- The `StdinMessage`/`StdoutMessage` envelope variants (moved from
  `cc.sdk.control_types`).
- The SDK stdout wire-message family (moved from `cc.sdk.core_types`).
- The wire-closure types the moved DTOs reference — `SlashCommand`, `ModelInfo`,
  `AccountInfo`, `FastModeState`, `AgentInfo`, and the `ModelUsage` wire struct
  (moved whole, all 8 fields retained, from `cc.sdk.core_schemas`; §1.2 move
  closure).
- Full `cc.utils.json` ser/de following the `cc.server.types` pattern:
  free `X_to_json(const X&) -> std::string` and
  `X_from_json(std::string_view) -> std::expected<X, std::string>`, plus ADL
  `to_json`/`from_json` overloads in `cc::server` (the pattern at
  server/types.cppm:352-804).

**Formats:** JSON over the existing transports. The on-wire shape is the
CLI-as-subprocess stdio protocol (newline-delimited JSON on stdin/stdout) and
the WebSocket control-frame protocol the server already speaks
(server_main.cppm:445-530). The ser/de is the single source of truth for both;
the hand-rolled `operator<<` JSON in `server_main.cppm:526` and the
`is_sdk_control_*` guards in `bridge_messaging.cppm:122-138` are replaced by
calls into the canonical ser/de.

**Boundary:** the module imports `cc.utils.json` (rank 2) + the canonical type
modules (`cc.types` rank 0, `cc.tools` rank 8, `cc.hooks` rank 4 — for the
converged `AgentDefinition`/`PermissionUpdate`/`ErrorCode` references) + `std`.
It sits at rank 13 so both `cc.server` (rank 13) and `cc.bridge` (rank 13) can
import it without an upward edge, and `cc.sdk` (rank 16) can import it downward
if a future SDK client needs the wire schema. (The earlier claim that the module
"imports only `cc.utils.json` + `std`" was wrong: the moved DTOs reference
canonical types after convergence — §1.2 move closure.)

**Why `cc.server` and not `cc.bridge`:** `cc.server.types` already establishes
the "canonical server-facing DTOs with ser/de" pattern, and the server is the
primary speaker of the control protocol today. `cc.bridge` becomes a consumer.
If a future review prefers the bridge as the owner, the module can move to
`cc.bridge.control_protocol` without a rank change (both are rank 13) — this is
a reversible placement decision, not a layering one.

**Why not keep the DTOs in `cc.sdk` with ser/de:** that would leave the wire
schema at the rank-16 apex. The live server/bridge would then either import
upward (illegal, re-inverting c11) or keep hand-rolling (the drift we are
eliminating). Moving the DTOs down is the only option that lets the live binary
consume the canonical types.

**`cc.sdk.control_schemas`/`control_types` after the move: deleted outright, no
re-export shim.** The c11 alias-then-delete courtesy (§1.1 principle 1) is
deliberately not applied here, for three reasons:

1. **Zero importers.** Grep across `src/` and `tests/` confirms no consumer of
   `cc.sdk.control_schemas`/`control_types` outside the island; the courtesy
   protects a consumer that does not exist.
2. **A shim would contaminate the island's link closure.** A re-export shim
   (`export import cc.server.control_protocol;`) makes `cc_sdk` `PUBLIC`-link
   `cc_server`, which transitively pulls `cc_query` → `cc_services` →
   `httplib`/`uv_a`/`yyjson`/`OpenSSL`/`CURL::libcurl` (cc_server.cmake:13-19,
   cc_services.cmake:66-77). That destroys the "type-only, no link libraries"
   property (§0.1) and contradicts §1.1 principle 3 ("the `cc_sdk` target gains
   only `cc_utils`") for the shim's entire lifetime. The embeddable surface's
   link closure must not depend on the HTTP server.
3. **Install ships one phase later.** The `loom::sdk` export (§2.6) lands in
   phase 3, after the shim's "one release" window — so the shim serves no
   in-tree packaging purpose either.

Out-of-tree consumers (none verified) migrate by importing
`cc.server.control_protocol` directly. This is the zero-importer principle
(§1.1 principle 4) applied to the MOVE, just as it is to the DELETEs. The
`cc_sdk` target's phase-2 link deps remain the canonical type targets (for the
CONVERGE aliases' BMIs) + `cc_utils` (for ser/de) — no `cc_server`, no
`cc_query`, no `cc_services` (§1.1 principle 3).

### 1.4 Phase-2 deletions (summary)

| Delete | Why |
|---|---|
| `cc.sdk.types` (whole module) | Orphan; every type has a live twin or is a drifted subset. |
| `cc.sdk.control_schemas` / `cc.sdk.control_types` (whole modules) | Moved to `cc.server.control_protocol` and deleted outright — zero importers; a re-export shim would `PUBLIC`-link `cc_server` (and its httplib/OpenSSL/curl closure) into the type-only island (§1.3). |
| `cc.sdk` duplicate `EffortLevel` (×2) | Identical to `cc::utils::EffortLevel`. |
| `cc.sdk` duplicate `Settings`/`SettingsSource`/`MergedSettings` | Typed `cc::core::Settings`/`ConfigSource`/`ConfigManager` are canonical. |
| `cc.sdk` duplicate `McpServerConfig` variant | Flat `cc::core::McpServerConfig` is canonical. |
| `cc.sdk` duplicate `ThinkingConfig`/`OutputFormat` | `cc::core::ThinkingConfig`/`QueryEngineConfig::ResponseSchema` are canonical. |
| `cc.sdk` duplicate `PermissionMode`/`PermissionUpdate`/`PermissionResult` | `cc::tools::mode_validation`/`cc::hooks` are canonical. |
| `cc.sdk` duplicate `SessionInfo`/`SDKSessionInfo` | `cc::session::SessionMetadata` is canonical. |

Net: the island shrinks from 8 modules to ~4 (`core_schemas` residuals,
`core_types` residuals — the STAYS protocol constants, `sandbox_types`,
`runtime_types` residuals); `types`, `control_schemas`, and `control_types` are
deleted outright (the latter two moved to `cc.server.control_protocol`), before
phase 3 absorbs the runtime shapes.

---

## 2. Phase 3 — the opaque `cc.sdk.harness` embedding entrypoint

### 2.1 The extracted recipe: `cc.query.assembly`

The `execute_native_query` recipe (§0.2) is extracted into a new module
**`cc.query.assembly`** (rank 10, in the `cc_query` target) as an
**assemble-only** free function plus a shared config resolver:

```cpp
// src/query/query_assembly.cppm  (export module cc.query.assembly;)
export namespace cc::query {

/// Per-caller overrides layered on top of ConfigManager settings when
/// resolving the engine config. The server adapter fills these from the
/// DirectQueryRequest; the harness fills them from HarnessConfig. ONE
/// resolver (below) consumes them — the recipe's settings→config mapping
/// (server_routes.cppm:714-731) is not re-implemented per caller.
struct AssemblyOverrides {
    std::optional<std::string> requested_model;
    std::optional<std::string> api_key;        // bypasses settings.network.api_key
    std::optional<std::string> base_url;
    std::optional<std::string> wire_api;       // "messages" | "openai"
    std::optional<std::string> cwd;
};

/// Shared settings → QueryEngineConfig resolver (the recipe's :714-731
/// mapping). Does NOT hard-error on an empty api_key — the caller enforces
/// its own key policy: the server adapter requires a real key for the
/// direct-connect Anthropic path (server_routes.cppm:733-735); a harness
/// with a loopback/gateway base_url supplies a placeholder key and bypasses
/// the check. This is the single resolution path for both callers.
[[nodiscard]] cc::core::Result<cc::core::QueryEngineConfig> resolve_engine_config(
    const cc::core::Settings& settings, const AssemblyOverrides& overrides);

struct AssemblyConfig {
    cc::core::QueryEngineConfig engine;        // resolved via resolve_engine_config
    std::optional<std::filesystem::path> sessions_dir;    // enable session storage
    std::optional<std::filesystem::path> dump_prompts_dir;
    std::vector<std::string> prior_message_lines;         // resume seed (parsed at rank 10)
    std::shared_ptr<std::atomic_bool> cancel_flag;         // external abort
    /// Test seam: register extra tools (e.g. a mock permission-gated tool)
    /// into the registry before the config.tools snapshot is taken.
    std::function<void(cc::core::ToolRegistry&)> register_extra_tools;
};

struct AssemblyCallbacks {
    std::optional<cc::hooks::AskUserResponseFn> ask_user;  // permission bridge
    std::optional<cc::tools::AgentLivePermissionCheckFn> permission_check;
};

/// Assemble-only: take the resolved config → build ToolPermissionHook →
/// ToolRegistry (runtime tools + missing-tool backend) → QueryEngine, wire
/// the abort/permission hooks, and seed prior messages. Does NOT run a turn.
/// (The caller loads ConfigManager and calls resolve_engine_config first —
/// the ConfigManager is a local, discarded after settings are read, not an
/// owned member.) assemble() also performs the recipe's step 8 internally:
/// it sets config.dynamic_tools_provider / config.mcp_input_schema_provider
/// to cc::tools::collect_mcp_tool_definitions / collect_mcp_input_schemas
/// (server_routes.cppm:804-810), so MCP tool discovery is preserved in the
/// re-expressed server route and not dropped by an implementer reading this
/// sketch literally.
[[nodiscard]] cc::core::Result<AssemblyHandle> assemble(
    const AssemblyConfig& config, const AssemblyCallbacks& callbacks);

/// Reusable assembly result. Move-only PIMPL owning ToolPermissionHook +
/// ToolRegistry + QueryEngine (declaration/construction order: hook,
/// registry, engine — §2.3).
class AssemblyHandle {
public:
    AssemblyHandle(AssemblyHandle&&) noexcept;
    AssemblyHandle& operator=(AssemblyHandle&&) noexcept;
    ~AssemblyHandle();
    /// The assembled engine. Valid for the handle's lifetime. Callers run
    /// query()/stream_query()/abort()/restore_conversation() on it.
    [[nodiscard]] cc::core::QueryEngine& engine() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cc::query
```

The server's one-shot path is then `assemble(...)` + `handle.engine().query(
content)` + map-to-`DirectQueryResult` — three statements in the adapter, not a
second recipe. The harness calls `assemble(...)` once at construction and reuses
`handle.engine()` for every `run()`/`stream()`. Both wrap the same assemble
step; only the run/map tail differs. (The earlier sketch's `assemble_and_run`
ran a turn and returned a handle "after running" — the wrong shape for
assemble-once-run-many — and defined `AssemblyResult` while returning an
undefined `AssemblyHandle`. Both are fixed above.)

**Hidden rank-13 dependencies, resolved (F4):**

- *Prior-message parsing.* The recipe's `seed_query_engine_from_session`
  (server_routes.cppm:336) calls `session_line_to_message`, a rank-13 helper,
  to parse JSONL lines into `cc::core::Message`. A rank-10 assembly cannot call
  it. Resolution: the assembly parses `prior_message_lines` itself using its
  own `cc::query::parse_session_message_value` (query_assembly.cpp:67, rank 10)
  and seeds via `engine.restore_conversation(messages)` (query_engine.cppm:393)
  — the real resume path, not the lossy `append_message_for_testing`-based
  server helper. `seed_query_engine_from_session` and `session_line_to_message`
  are deleted from `server_routes.cppm` (the server adapter passes
  `prior_message_lines` through `AssemblyConfig` unchanged).
  *(Deviation from the original sketch, recorded: the sketch named the
  non-lossy `cc::tools::agent::utils::message_from_json_value`
  (agent_sub_utils_json.cpp:585), but that returns
  `cc::services::api::Message`, incompatible with `restore_conversation`'s
  `cc::core::Message` with no existing converter. `parse_session_message_value`
  is role/content-string-only and drops `tool_use`/`tool_result`/`image`
  blocks — a non-lossy `cc::core::Message` reader is a follow-up. See §2.3
  resume path.)*
- *Test executor seam.* `execute_native_query` short-circuits through
  `query_executor_override` (server_routes.cppm:707, set via
  `set_query_executor_for_testing`; consumed by tests/test_services.cpp:7889).
  The server adapter must preserve this: the override check stays in the
  adapter (a `cc.server`-local test seam), ahead of the `assemble(...)` call, so
  the existing test is unaffected. The assembly itself knows nothing of it.

**Why a new rank-10 module and not `cc.server` or `cc.sdk`:** the recipe calls
`QueryEngine` (rank 10), `register_runtime_tools` (rank 8),
`install_runtime_backends` (rank 9), `ToolPermissionHook` (rank 4), and session
storage (rank 6). The lowest rank that can see all of these is 10. Putting it in
`cc.server` (rank 13) would make `cc.sdk.harness` depend on the HTTP server
(semantically wrong for an embeddable surface). Putting it in `cc.sdk` (rank 16)
would make the server import upward to use it (illegal). Rank 10 in `cc_query`
is the natural home: it is engine assembly, not HTTP serving. (The earlier
"pulls in httplib/OpenSSL" argument against `cc.server` placement was moot:
`cc_query` PUBLIC-links `cc_services`, which PUBLIC-links OpenSSL/CURL/httplib —
so those enter the SDK closure either way. The real distinction is that
`cc.server` would add the HTTP server *target itself*, not that it would add
OpenSSL. See §3.4 for the accepted phase-3 closure.)

**Link cost (corrected):** `cc_query` `PUBLIC`-links `cc_tools`, `cc_hooks`,
`cc_session`, `cc_memdir`, `cc_services` **already** (cc_tools.cmake:126), and
`cc_config`/`cc_state`/`cc_utils`/`CURL::libcurl` (cc_query.cmake:28). The
assembly reuses all of these. The **only genuinely new `PUBLIC` dependency is
`cc_orchestration`** (for `install_runtime_backends` and
`make_missing_tool_backend`, via `cc.orchestration.runtime_backends`). No cycle
— `cc.orchestration` does not import `cc.query`. (The original sketch also
cited `message_from_json_value` as a reason; the assembly instead uses its own
`parse_session_message_value` — see the F4 deviation above — so that reason is
dropped.) The earlier claim that `cc_query` "gains `PUBLIC cc_tools
cc_orchestration cc_hooks cc_session`" overstated the cost by reading only
`cc_query.cmake` and missing `cc_tools.cmake:126`; see open question 6.

**Server re-expression:** `cc.server.server_routes::execute_native_query`
becomes a thin adapter: preserve the `query_executor_override` short-circuit,
build `AssemblyConfig`/`AssemblyOverrides` from the route-local
`DirectQueryRequest`, call `cc::query::assemble`, run
`handle.engine().query(request.content)`, map to `DirectQueryResult`. The
~135-line recipe is deleted from `server_routes.cppm`. The server is thus
re-expressed through the **same extracted assemble step** the harness wraps —
satisfying the c11 dogfooding intent without an upward edge.

### 2.2 The opaque harness surface

**New module: `cc.sdk.harness`** (rank 16, in the `cc_sdk` target) with an
implementation unit. The public surface is an opaque PIMPL handle:

```cpp
// src/sdk/harness.cppm  (export module cc.sdk.harness;)
import cc.types.types;            // ContentBlock, Message, TokenUsage, StreamEvent, Result
import cc.query.wire_protocol;    // WireBackend (for BackendFactory)
import cc.hooks.tool_permissions; // PermissionContext, PermissionResponse
import cc.query.assembly;         // the extracted recipe

export namespace cc::sdk {

/// Per-turn options. Only fields with a per-turn engine seam are exposed;
/// construction-time config lives in HarnessConfig (see mapping below).
struct TurnOptions {
    std::string prompt;
    std::optional<std::string> model;              // via QueryEngine::set_model_params
    std::optional<std::vector<std::string>> allowed_tools;  // → QueryOptions.enabled_tools
    std::vector<cc::core::ContentBlock> attachments;  // @-mention files → QueryOptions.attachments
};

/// One turn's result (maps cc::core::QueryResponse).
struct TurnResult {
    cc::core::AssistantMessage message;
    cc::core::TokenUsage usage;
    std::uint32_t tool_rounds = 0;
    std::chrono::milliseconds elapsed{0};
    bool budget_exceeded = false;
    bool success = true;
    std::vector<std::string> errors;
};

/// Ask-user permission callback (bridges ToolPermissionHook).
using PermissionCallback =
    std::function<cc::hooks::PermissionResponse(const cc::hooks::PermissionContext&)>;

/// Streaming event sink (bridges StreamCallback).
using EventSink = std::function<void(const cc::core::StreamEvent&)>;

/// Wire backend factory. When unset, the engine builds its default
/// Anthropic/OpenAI backend from QueryEngineConfig.
///
/// SCOPE (S1): this seam intercepts request-body serialization only —
/// make_wire_backend() is called solely from build_request_body
/// (query_engine_wire.cpp:281) and only prepare() is used. The transport
/// (send_request's httplib POST, query_engine_http.cpp:121; SSE streaming;
/// response parsing) does NOT go through WireBackend, so a factory returning
/// a mock backend does not prevent real HTTP calls. For a no-network test,
/// use a loopback HTTP server (§4.4), not a mock backend.
using BackendFactory =
    std::function<std::unique_ptr<cc::query::wire::WireBackend>()>;

/// API key provider. Called at construction; the key is never stored in the
/// config struct (so it cannot be serialized by accident).
using ApiKeyProvider = std::function<std::string()>;

/// Construction configuration.
struct HarnessConfig {
    std::string model;
    std::filesystem::path cwd;
    std::optional<double> max_budget_usd;
    std::optional<std::uint32_t> max_turns;
    std::optional<std::string> system_prompt;
    std::optional<std::string> append_system_prompt;
    std::optional<std::string> base_url;
    std::optional<std::string> wire_api;             // "messages" | "openai"
    ApiKeyProvider api_key_provider;                 // injected, never stored
    BackendFactory backend_factory;                  // optional WireBackend seam
    PermissionCallback permission_callback;          // optional; absent = engine default
    EventSink event_sink;                            // optional
    std::optional<std::filesystem::path> sessions_dir;       // resume/fork
    std::optional<std::filesystem::path> dump_prompts_dir;
    std::vector<std::string> always_deny_rules;
};

/// Opaque embedding handle. Move-only; non-copyable.
class Harness {
public:
    explicit Harness(HarnessConfig config);
    ~Harness();
    Harness(Harness&&) noexcept;
    Harness& operator=(Harness&&) noexcept;
    Harness(const Harness&) = delete;
    Harness& operator=(const Harness&) = delete;

    /// Run one turn to completion (blocking). Wraps QueryEngine::query.
    [[nodiscard]] cc::core::Result<TurnResult> run(const TurnOptions& options);

    /// Run one turn with streaming events. Wraps QueryEngine::stream_query.
    void stream(const TurnOptions& options, const EventSink& sink);

    /// Abort the in-flight turn (thread-safe). Sets the harness abort flag
    /// (wired as the engine's external abort callback) and calls
    /// QueryEngine::abort. See §2.3 for the entry/reset lifecycle.
    void abort() noexcept;

    /// Resume a prior session from disk (requires sessions_dir).
    [[nodiscard]] cc::core::Result<void> resume(std::string_view session_id);

    /// Current session id (for resume/fork correlation).
    [[nodiscard]] std::string session_id() const;

    /// Current conversation (thread-safe copy).
    [[nodiscard]] std::vector<cc::core::Message> conversation() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cc::sdk
```

**TurnOptions → engine seam mapping (every field mapped or dropped):**

| `TurnOptions` field | Engine seam | Per-turn? |
|---|---|---|
| `prompt` | `query(user_message)` / `stream_query(user_message)` | yes |
| `model` | `QueryEngine::set_model_params` (query_engine.cppm:407, existing runtime setter) | yes |
| `allowed_tools` | `QueryOptions::enabled_tools` (query_engine.cppm:247) | yes |
| `attachments` | `QueryOptions::attachments` (query_engine.cppm:255) | yes |
| ~~`max_turns`~~ | **dropped** — `QueryEngineConfig::max_turns` is construction-time (query_engine.cppm:194); no per-turn setter | construction-time only |
| ~~`max_budget_usd`~~ | **dropped** — `QueryEngineConfig::max_budget_usd` is construction-time (:193) | construction-time only |
| ~~`system_prompt`~~ | **dropped** — `QueryEngineConfig::custom_system_prompt` is construction-time (:178) | construction-time only |
| ~~`disallowed_tools`~~ | **dropped** — maps to `QueryEngineConfig::always_deny_rules` (:230), construction-time; `QueryOptions` has no deny field | construction-time only |

`QueryOptions` (query_engine.cppm:243-256) carries only `on_event`/
`include_thinking`/`max_tool_rounds`/`enabled_tools`/`prompt_uuid`/`is_meta`/
`attachments` — none of `max_turns`/`max_budget_usd`/`system_prompt`/
`disallowed_tools` have a per-turn engine seam. Exposing them on a long-lived
engine would require either new `QueryEngine` setters (scope creep) or an engine
rebuild per turn (defeats the reuse model). They stay on `HarnessConfig`
(construction-time); varying them per turn requires a new `Harness`. This
resolves F5/S2 once for both reviews.

**Construction:** `Harness::Impl` calls `cc::query::assemble(config, callbacks)`
(§2.1) **once** in the constructor body, resolving `HarnessConfig` →
`AssemblyOverrides` → `resolve_engine_config` (the shared resolver, not a second
settings→config implementation). The `api_key` hard-error (server_routes.cppm:
733-735) is **not** in the resolver: the harness supplies a placeholder key when
none is configured and a `base_url` is set (a loopback test server or gateway
ignores it), so a mock/loopback harness is never blocked by an empty key.
Subsequent `run()`/`stream()` calls reuse `handle.engine()`.

### 2.3 Ownership and lifetime

- **Harness is the sole owner.** `Harness::Impl` holds the `HarnessConfig`
  callbacks, an `AssemblyHandle` (the result of `assemble(...)`, which owns the
  `ToolPermissionHook` + `ToolRegistry` + `QueryEngine`), and the abort flag.
  The `ConfigManager` is a construction-path local (load → `settings()` →
  `resolve_engine_config` → discard), not an owned member — it is unused after
  the config is resolved. The embedder holds one `Harness` by value (or
  `unique_ptr`).
- **Member declaration order (load-bearing), in two Impls:**
  - `Harness::Impl`: (1) the `HarnessConfig` callbacks (`permission_callback_`,
    `event_sink_`, `backend_factory_`, `api_key_provider_`) — declared **first**,
    constructed first, destroyed **last**; (2) `AssemblyHandle handle_`; (3)
    `abort_requested_` (`std::atomic_bool`).
  - `AssemblyHandle::Impl` (the pieces): `permission_hook_`
    (`unique_ptr<ToolPermissionHook>`) → `registry_` (`unique_ptr<ToolRegistry>`)
    → `engine_` (`unique_ptr<QueryEngine>`), each constructed in the
    `assemble(...)` body (not in init-lists).

  **Why this order:** the hook's `ask_user_response_fn` wraps the
  `PermissionCallback` (tool_permissions.cppm:87), and the registry's
  `RuntimeFunctionTool`s store the `permission_check` lambda, which captures
  `&permission_hook_` (runtime_registry.cppm:69,85 — the lambda is *stored* in
  each tool, not just invoked). The reference graph is therefore
  `registry → hook → callbacks`. Reverse declaration order guarantees reverse
  destruction: in `Harness::Impl`, `handle_` dies before the callbacks (so the
  hook's reference to them is never dangling); inside `AssemblyHandle::Impl`,
  `engine_` dies first (stops invoking tools), then `registry_` (its tools'
  lambdas are released while `permission_hook_` still lives), then
  `permission_hook_`. No dangling reference at any destruction step. (The
  earlier draft's `registry → hook → engine` order was safe only because the
  engine is the sole invoker and dies first; the order above is robust
  regardless of invoker identity.)
- **Body construction, not init-lists.** `QueryEngine` is non-movable (deleted
  move ctor, query_engine.cppm:321-322) and `ToolPermissionHook` holds a
  `std::mutex`; more importantly, `config.tools` is a **snapshot** consumed at
  query time (query_engine_wire.cpp:269, query_engine_system_prompt.cpp:182), so
  the engine must be constructed *after* `register_runtime_tools` +
  `registry.get_visible_definitions()`. Direct members in declaration order
  would init-list the engine before the body can register tools, yielding an
  engine that sees zero tools. The `assemble(...)` body therefore runs, in
  order: build the hook → `make_unique<ToolRegistry>` + `register_runtime_tools`
  + `set_missing_tool_handler` → snapshot `config.tools` → set
  `config.dynamic_tools_provider`/`mcp_input_schema_provider` (recipe step 8,
  server_routes.cppm:804-810 — MCP tool discovery) →
  `make_unique<QueryEngine>(std::move(config), *registry_)` →
  `engine_->set_permission_hook(permission_hook_.get())` →
  `engine_->set_external_abort_callback(...)` → seed prior messages. The
  `unique_ptr` members make this body-ordered construction explicit and leave
  both Impls movable.
- **Callbacks do not outlive the Harness.** `PermissionCallback`, `EventSink`,
  `BackendFactory`, `ApiKeyProvider` are `std::function` moved from
  `HarnessConfig` into `Impl` (declared first, per above). The embedder must
  ensure any captured state outlives the `Harness` — documented, not enforced
  (the same contract as `QueryEngine::set_permission_hook`).
- **One Harness = one session.** Multi-turn conversation lives in the owned
  `QueryEngine` (its `conversation_`). Concurrent `run()` calls on one Harness
  are not supported (the engine is single-flight, matching `execute_native_query`
  which constructs a fresh engine per request). For concurrent sessions, the
  embedder constructs multiple `Harness` instances.
- **Abort lifecycle (specified).** `Harness::abort()` sets `abort_requested_`
  AND calls `handle_.engine().abort()`. The harness passes `abort_requested_`
  to `assemble(...)` as `AssemblyConfig::cancel_flag` (a
  `shared_ptr<atomic_bool>`); `assemble(...)` wires it as the engine's external
  abort callback (`engine.set_external_abort_callback([flag]{ return
  flag->load(); })`), so an in-flight turn stops at the next engine abort
  checkpoint (query_engine.cppm:759). `run()`/`stream()` entry does
  `if (abort_requested_.exchange(false)) return error("Query interrupted");`
  — matching the server's pre-query cancel check (server_routes.cppm:823-825).
  The `exchange` (not a plain load) is essential: it returns the error once for
  an abort requested before the turn, then clears the flag so the *next* turn
  proceeds. The engine's own `aborted_` flag is auto-reset at `query()`/
  `stream_query()` entry (query_engine_loop.cpp:25,85), so an engine-level abort
  never poisons a later turn; the harness-level flag is what makes
  "abort before run" observable and what the §4.4 test asserts.
- **Resume path (no `cc.server` import).** `Harness::resume(session_id)` uses
  the real engine resume path, not the server-local lossy helper:
  `cc::session::load_messages(*sessions_dir_, session_id)` (storage.cppm:228,
  rank 6 — returns `vector<JsonDoc>`) → parse each doc with
  `cc::query::parse_session_message_value` (query_assembly.cpp:67, rank 10 —
  the same reader `assemble()` uses for `prior_message_lines`, exported so
  the harness needs no `cc.server` import) →
  `handle_.engine().restore_conversation(messages)` (query_engine.cppm:393,
  which also rebuilds content-replacement state). This replaces
  `seed_query_engine_from_session` (server_routes.cppm:336 — lossy: parses only
  role/content strings, drops tool blocks, and uses
  `append_message_for_testing`).

  **Deviation from the original sketch (recorded):** the sketch specified the
  non-lossy `cc::tools::agent::utils::message_from_json_value`
  (agent_sub_utils_json.cpp:585), but that returns
  `cc::services::api::Message` (a flat struct), incompatible with
  `restore_conversation`'s `cc::core::Message` (5-member variant) with no
  existing converter. The harness therefore uses `parse_session_message_value`,
  which is itself role/content-string-only and drops `tool_use`/`tool_result`/
  `image` blocks — so a session that used tools cannot be faithfully resumed
  via this path yet. A non-lossy `cc::core::Message` reader is a follow-up.
  Both `cc.session` (rank 6) and `cc.query.assembly` (rank 10) are importable
  from `cc.sdk` (rank 16) without touching `cc.server`. (The original claim
  that "`OpenSSL::Crypto` never enters the SDK link closure" was false:
  `cc_query` PUBLIC-links `cc_services`, which PUBLIC-links OpenSSL — see
  §3.4 for the accepted phase-3 closure.)

### 2.4 The WireBackend seam — and its limit (S1 redesign)

`QueryEngine::make_wire_backend()` is `private` and non-virtual
(query_engine.cppm:658; query_engine_wire.cpp:124-179). Phase 3 adds an
injection seam:

```cpp
// In cc.query.query_engine (QueryEngine class, public):
using WireBackendFactory =
    std::function<std::unique_ptr<cc::query::wire::WireBackend>()>;
void set_wire_backend_factory(WireBackendFactory factory);
```

When set, `make_wire_backend()` delegates to the factory; when unset, the
existing Anthropic/OpenAI construction runs unchanged. This is a ~10-line
surgical change to `query_engine.cppm` + `query_engine_wire.cpp`, no behavior
change for existing callers. The `HarnessConfig::backend_factory` maps to this
seam.

**The seam does not reach the transport — and the design no longer claims it
does.** Verified: `make_wire_backend()` is called in exactly one place
(`build_request_body`, query_engine_wire.cpp:281) and only `prepare()` is used
there. `send_request` (query_engine_http.cpp:121) hardcodes the httplib POST;
`parse_api_response` (:205) parses the body directly; the `WireBackend`
virtuals `parse_response`/`parse_stream_event`/`stop_reason_is_tool_use` have
**zero** call sites in the engine. A factory returning a mock `WireBackend`
therefore intercepts request-body serialization only — `run()` still does a
real httplib POST to `base_url` and fails with `ConnectionFailed` against a
non-listening endpoint. The earlier "~10-line surgical change makes the harness
testable with a mock backend, no real API calls" claim was wrong on that point.

**Redesign decision (stated): the phase-3 first-consumer test uses a loopback
HTTP server, not a mock transport.** The test spins up an `httplib::Server` on
`127.0.0.1` with an ephemeral port, serves canned Anthropic Messages API
responses (SSE for the streaming case), and points `HarnessConfig::base_url` at
it with a dummy `api_key`. The real transport, SSE path, retry, and tool loop
then run end-to-end with zero external network calls. This requires **no engine
transport change** — the loopback server is the transport mock. The
`set_wire_backend_factory` seam is retained for what it actually does
(request-body inspection in tests) and is documented as body-serialization-only
on the `BackendFactory` type (§2.2).

**Deferred alternative:** routing `send_request`/stream through `WireBackend`
(a real transport seam) is a much larger change — it touches the httplib TU,
SSE streaming, retry, and 413-compaction, and would make `WireBackend` the
single transport boundary. It is a worthwhile follow-up but is **out of phase-3
scope**; the loopback-server test unblocks the first-consumer gate without it.

### 2.5 First-consumer gate

Per the c11 design: "the server route itself re-expressed through Harness as the
first in-tree consumer before any external commitment." Resolved as follows
(see §3 for the layering reason the server cannot import the Harness class
directly):

1. **The recipe is extracted to `cc.query.assembly` (§2.1).** The server route
   is re-expressed through the extracted recipe — the same code the harness
   wraps. This proves the recipe is reusable and faithful (the server's
   direct-connect tests must stay green).
2. **The Harness class gets a direct in-tree test consumer** (`test_sdk_harness`,
   §4.4) that constructs a `Harness`, runs a turn against a **loopback HTTP
   server** serving canned Anthropic responses (the `WireBackend` seam does not
   reach the transport — §2.4), and asserts the `TurnResult`. Tests are not in
   the `graph_check.py` module graph (it scans `src/` only —
   graph_check.py:34,188-189), so the test creates no upward edge.
3. **No external commitment.** The `Harness` surface is not installed/exported
   until the test consumer is green and the server route is re-expressed. The
   `loom::sdk` CMake export (§2.6) ships in the same phase but is explicitly
   marked experimental in the installed config.

### 2.6 Install/EXPORT revival

The root `CMakeLists.txt:310-359` install block is fully commented out. Phase 3
revives it:

- **Uncomment and update** `install(TARGETS … EXPORT LOOMTargets …)`. The old
  target list (cc_app, cc_core, cc_tools, …) is stale. The new list installs
  `cc_sdk` and every `cc_*` library target in its transitive `PUBLIC` link
  closure. **The list must be computed from the actual link graph, not
  hand-maintained** — the earlier draft listed `cc_wire`, which does not exist
  (wire backends are modules inside `cc_query`, so `cmake --install` would fail
  at configure time), and omitted `cc_services`, `cc_memdir`, `cc_skills_core`,
  `cc_task_types`. The closure is derived by walking `target_link_libraries`
  from `cc_sdk`: `cc_query` (cc_query.cmake:28 → `cc_utils cc_state cc_config
  CURL::libcurl`; cc_tools.cmake:126 → `cc_tools cc_hooks cc_session cc_memdir
  cc_services`), `cc_orchestration`, `cc_types`, `cc_constants`,
  `cc_skills_core` (via `cc_tools`), `cc_task_types` (via `cc_state` —
  cc_state.cmake:29 links it directly; `cc_core` is not in this closure),
  and their transitive `cc_*` deps. The `loom` executable is installed
  separately (`RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}`).
- **Third-party (FetchContent) dependencies must be installed too (S5b).** The
  closure `PUBLIC`-links FetchContent-built targets that are **not** installed
  today: `cc_services` → `CURL::libcurl yyjson uv_a httplib::httplib
  OpenSSL::SSL/Crypto` (cc_services.cmake:66-77); `cc_session`/`cc_state`/
  `cc_tools` → `yyjson`/`uv_a` (cc_session.cmake:8, cc_tools.cmake:114-121).
  `yyjson`/`libuv`/`httplib` are built via `FetchContent_Declare`
  (CMakeLists.txt:207-257); an installed `LOOMTargets.cmake` that references
  them would break in a consumer project where those targets do not exist, and
  `find_dependency(yyjson)` cannot conjure a FetchContent target. **Plan:**
  install the FetchContent third-party targets alongside the `cc_*` targets —
  add `yyjson`, `uv_a`, `httplib` to the same `install(TARGETS …
  EXPORT LOOMTargets)` call (FetchContent targets are ordinary CMake targets
  and can be installed; use the real target name `httplib`, not the alias
  `httplib::httplib` — CMake rejects installing ALIAS targets), so the
  exported set is self-contained. Externally
  provided deps (`CURL::libcurl`, `OpenSSL::SSL/Crypto`) stay as
  `find_dependency(CURL)` / `find_dependency(OpenSSL)` in `loomConfig.cmake`.
  (Making the third-party deps `PRIVATE` instead is not viable without proof of
  no interface leakage — `cc_services`'s module interfaces expose their types —
  so installing them is the chosen path.)
- **ABI policy (S5c): source-compatible only, no ABI stability.** The "opaque
  PIMPL" harness is not ABI-sealed: its public surface passes by value
  `cc::core::AssistantMessage`/`TokenUsage`/`ContentBlock`/`StreamEvent`/
  `Message`, `cc::hooks::PermissionResponse`/`PermissionContext`, and the
  polymorphic `WireBackend`. With a prebuilt static lib + consumer-rebuilt
  BMIs, the installed package is **source-compatible within one toolchain
  generation**, not ABI-stable. "Compatible clang" is defined as: the same
  `clang++` major version (the project pins LLVM 22 locally), the same C++
  standard library (`libc++` on macOS/Homebrew, `libstdc++` on Linux — no
  mixing), and a matched `clang-scan-deps`. Consumers rebuild the module BMIs
  from the installed `.cppm` files against their own compiler; there is no
  prebuilt `.pcm`/`.mod` and no cross-version ABI guarantee. This is stated in
  the installed `loomConfig.cmake` and the package is marked experimental.
  The installed config is `loomConfig.cmake` (lowercase package name) under
  `lib/cmake/loom` — CMake config-mode search is case-sensitive, so
  `find_package(loom)` requires exactly that spelling (verified with CMake
  3.31.2; `find_package(LOOM)` does not find it).
- **Namespace:** `NAMESPACE loom::` (the c11 design specifies `loom::sdk`; the
  old block said `LOOM::`). The consumer does `find_package(loom)` and links
  `loom::sdk`.
- **BMI install story:** CMake 3.28 (the project minimum, CMakeLists.txt:1)
  supports `install(TARGETS … FILE_SET CXX_MODULES DESTINATION …)`. The `.cppm`
  interface files are installed; the consumer's compiler rebuilds the BMIs.
  Compiler-specific `.pcm`/`.mod` files are **not** installed (they are not
  portable across clang versions). The installed package therefore requires the
  consumer to use a compatible `clang++`/`clang-scan-deps` pair — documented in
  the installed `loomConfig.cmake`.
- **`LOOMConfig.cmake.in`** (`cmake/LOOMConfig.cmake.in`) exists but references
  `Boost` (line 7), which the tree no longer uses. Update it: drop `Boost`,
  keep `find_dependency(CURL)`, and add `find_dependency(OpenSSL)` for the
  externally-provided `OpenSSL::SSL/Crypto`. Do **not** `find_dependency`
  `yyjson`/`uv_a`/`httplib` — those are installed as targets in the
  `LOOMTargets` export (previous bullet), so they resolve from the export set
  itself. **httplib's transitive system deps (added during the P3-server
  fix):** the installed `loom::httplib` target references `Threads::Threads`,
  `ZLIB::ZLIB`, and `Brotli::*` in its `INTERFACE_LINK_LIBRARIES` (httplib's
  compression/OpenSSL support, on by default when the system has the libs),
  and its `INTERFACE_COMPILE_DEFINITIONS` propagate
  `CPPHTTPLIB_BROTLI_SUPPORT`/`CPPHTTPLIB_ZLIB_SUPPORT` — so the `cc_*`
  objects reference those symbols and a consumer link needs them. The config
  therefore also `find_dependency`s `Threads`, `ZLIB`, and `Brotli
  COMPONENTS encoder decoder common`. CMake ships no `FindBrotli`, so
  httplib's own `cmake/FindBrotli.cmake` is installed next to the config and
  the config appends its directory to `CMAKE_MODULE_PATH` before the
  `find_dependency(Brotli)` call.
- **`loom.sdk` module-name promotion:** deferred. The C++ module names stay
  `cc.sdk.*` (the import string is not CMake-visible). Promoting to `loom.sdk`
  is a one-time public-ABI break that should wait for a real external consumer.
  The install namespace (`loom::`) and the module name (`cc.sdk.*`) are
  orthogonal — a consumer links `loom::sdk` and imports `cc.sdk.harness`.

---

## 3. Layering invariants and graph_check verification

### 3.1 The rank-16 apex invariants

`cc.sdk` sits at rank 16 (`tools/arch/graph_check.py:89`), the apex. The
invariants:

1. **The island stays free of engine/runtime linkage until phase 3.** Phase 2
   adds `cc.utils.json` ser/de (rank 2) and CONVERGE aliases that import the
   canonical **type** modules (`cc.types`, `cc.config`, `cc.tools.*` type
   modules, `cc.hooks`, `cc.session` — all downward from rank 16). What phase 2
   does **not** add is engine/runtime imports: no `cc.query`, no
   `cc.orchestration` until phase 3 (the harness). See §1.1 principle 3 for the
   link-closure statement.
2. **No upward edges into the SDK.** No module at rank < 16 may import
   `cc.sdk.*`. `graph_check.py` flags any edge `m → i` where `rank(i) > rank(m)`
   as upward (`upward_edges`, graph_check.py:266-276). Since 16 is the maximum,
   every edge out of `cc.sdk` is downward (legal); every edge into `cc.sdk` from
   a lower rank is upward (illegal).
3. **The `is_contract` exemption is narrow.** `graph_check.py:119-130` exempts
   imports of modules whose leaf is `port`/`contract`/`*_types`, or that are
   `cc.types.*`. Of the 8 SDK modules, `core_types`/`control_types`/
   `runtime_types`/`sandbox_types`/`settings_types` are contract-exempt;
   `core_schemas`/`control_schemas`/`types` are **not**. Phase 2 must not rely
   on the exemption to sneak an SDK type into a lower-ranked module — the
   convergence aliases point DOWNWARD (SDK → canonical), never upward.
4. **Fail-closed unranked check.** Any new area must have a `TARGET_RANK` entry
   or `graph_check.py` fails (`graph_check.py:642-643, 714-717`). The new
   `cc.query.assembly` module is in area `cc.query` (rank 10, already ranked).
   The new `cc.server.control_protocol` module is in area `cc.server` (rank 13,
   already ranked). No new rank entries are needed.

### 3.2 Why the server cannot import `cc.sdk.harness` directly

`cc.server` is rank 13; `cc.sdk` is rank 16. An import
`cc.server.server_routes → cc.sdk.harness` is an upward edge (13 → 16), which
`graph_check.py` flags as a **new** upward edge (not in the baseline) → FAIL.
`cc.sdk.harness` is not contract-exempt (leaf `harness`). This is why §2.1
extracts the recipe to `cc.query.assembly` (rank 10): the server imports the
recipe (13 → 10, downward), and the harness wraps the recipe (16 → 10,
downward). The server is re-expressed through the same recipe without an upward
edge.

### 3.3 Phase 2 graph effects

| Change | Graph effect |
|---|---|
| SDK modules alias `cc.types`/`cc.config`/`cc.tools`/`cc.hooks`/`cc.utils`/`cc.session` | New downward edges from `cc.sdk` (16) to ranks 0-8. All legal. |
| New `cc.server.control_protocol` module (rank 13) imports `cc.utils.json` (2), `cc.types` (0), `cc.tools` (8), `cc.hooks` (4) | All downward from 13. Legal. (The converged DTOs reference canonical types; the STAYS wire-closure types move with the protocol — §1.2.) |
| `cc.server.server_main` / `cc.bridge` import `cc.server.control_protocol` | Same-rank (13→13) or downward; not upward. Must not create a module cycle (verify: `cc.server` does not import `cc.bridge`). |
| `cc.sdk.control_schemas`/`control_types` deleted outright (no re-export shim) | Node removals; zero importers. `cc_sdk` does **not** gain `PUBLIC cc_server` — no `cc_query`/`cc_services`/OpenSSL/httplib/curl enters the SDK closure (§1.1 principle 3). A shim was rejected because it would drag `cc_server`'s closure into the island (§1.3). |
| Delete `cc.sdk.types` | Removes a node; zero importers, no edge loss. |
| Delete duplicate `EffortLevel`/`Settings`/etc. | Node removals; zero importers outside the island. |

### 3.4 Phase 3 graph effects

| Change | Graph effect |
|---|---|
| New `cc.query.assembly` module (rank 10) imports `cc.tools` (8), `cc.orchestration` (9 — `runtime_backends` for `install_runtime_backends`/`make_missing_tool_backend`), `cc.hooks` (4), `cc.session` (6), `cc.config` (1) | All downward from 10. Legal. **Link:** only `cc_orchestration` is a new `PUBLIC` dep of `cc_query` — `cc_tools`/`cc_hooks`/`cc_session` are already linked (cc_tools.cmake:126). (The original sketch cited `agent_sub_utils`/`message_from_json_value`; the assembly uses its own `parse_session_message_value` — §2.1 F4 deviation.) |
| `cc.server.server_routes` imports `cc.query.assembly` | 13 → 10, downward, legal. The `query_executor_override` short-circuit stays in the adapter (a `cc.server`-local test seam), ahead of `assemble(...)`. |
| New `cc.sdk.harness` module (rank 16) imports `cc.query.assembly` (10), `cc.types` (0), `cc.hooks` (4), `cc.query.wire_protocol` (10), `cc.session` (6) | All downward from 16. Legal. No `cc.server` import. **Link cost (accepted):** `cc_sdk` PUBLIC-links `cc_query`, which PUBLIC-links `cc_services` (cc_tools.cmake:133) + `CURL::libcurl` (cc_query.cmake:42); `cc_services` PUBLIC-links `OpenSSL::SSL/Crypto`, `CURL::libcurl`, `httplib`, `yyjson`, `uv_a` (cc_services.cmake:66-81). So OpenSSL and libcurl DO enter the SDK closure via `cc_services` — the phase-3 cost of embedding the engine. `cc_services`' module interfaces expose OpenSSL/CURL types (gcp_adc.cppm's `EvpPkeyPtr`, client.cppm's CURL handle), so those links cannot be made PRIVATE to stop propagation. What stays out is the `cc_server` target itself (the HTTP server routes/main). (`resume()` uses `parse_session_message_value`, rank 10 — not `message_from_json_value`; see §2.3 deviation note.) |
| `QueryEngine::set_wire_backend_factory` added | No graph effect (same module). |
| Install/EXPORT revival | No graph effect (CMake only). |

### 3.5 Verification commands

Both phases are verified with the local dual-preset gate (per the 2026-09-29
directive, GitHub CI is not a gate):

```bash
# Architecture gate (fast, no build):
python3 tools/arch/graph_check.py            # default gate: no unranked, no cycles,
                                             # no NEW upward edges, no NEW dead imports
python3 tools/arch/graph_check.py --json     # machine-readable for review

# Build + test gate (both presets, serial):
cmake --preset local-linux && cmake --build --preset local-linux -j8
ctest --preset local-linux -j1
cmake --preset local-linux-release && cmake --build --preset local-linux-release -j8
ctest --preset local-linux-release -j1
```

The `graph_check.py` default gate (`graph_check.py:686-691`) fails on: unranked
areas, module-level cycles, NEW upward edges, NEW dead imports. Known baseline
backlog is tolerated. Both phases must produce zero NEW upward edges and zero
NEW dead imports.

---

## 4. Per-phase rollback and test plan

### 4.1 Phase 2 rollback

Phase 2 is a sequence of small, independently-revertible commits:

1. **Alias commit:** add `using`-aliases in the SDK modules pointing at the
   canonical types — this **includes converging the types the MOVE'd DTOs
   reference** (`AgentDefinition`, `PermissionUpdate`, `SDKAssistantMessageError`,
   and the standalone `NonNullableUsage` → `TokenUsage`) so the moved module
   references canonical types, not rank-16 island types (§1.2 move closure);
   `ModelUsage` itself moves whole to rank 13 (all fields retained, no
   convergence of its fields); add the `cc.server.control_protocol` module with
   the moved DTOs + ser/de; rewire `server_main`/`bridge` to the canonical
   ser/de. Rollback: `git revert` the alias commit. The island is unchanged
   (twins still present); the new module is unused.
2. **Deletion commit(s):** delete `cc.sdk.types`, the duplicate
   `EffortLevel`/`Settings`/`McpServerConfig`/etc., and
   `cc.sdk.control_schemas`/`control_types` (moved to
   `cc.server.control_protocol` and deleted outright — §1.3, no re-export
   shim).
   Rollback: `git revert` each deletion commit. Because the aliases shipped
   first, any out-of-tree consumer has a migration window; reverting a deletion
   restores the twin without touching the alias.

Each commit must pass the dual-preset gate before the next. If a deletion
breaks an unexpected consumer, revert just that deletion — the aliases keep the
canonical type reachable.

### 4.2 Phase 2 test plan

- **New: `test_sdk_serde`** (links `cc_sdk` + `cc_server`): round-trip tests
  for every moved control DTO — `X_from_json(X_to_json(x)) == x` for each of
  the 21 subtypes, the `Stdin`/`Stdout` envelopes, and the SDK stdout
  wire-message family. This is the ser/de correctness gate. (Round-trip alone
  cannot detect drift from the *live* wire format — see the golden gate below.)
- **New: `test_sdk_convergence`** (links `cc_sdk`): static asserts that the
  alias types are the canonical types (`std::is_same_v<cc::sdk::…, cc::core::…>`)
  so a future drift is caught at compile time.
- **New: golden wire-compatibility gate (S6, gate-blocking for the hand-rolled
  deletion).** Replacing the live hand-rolled JSON (`server_main.cppm:445-537`,
  `bridge_messaging.cppm:78-138`) with new ser/de over the island DTOs risks
  silent field-name/shape drift (camelCase vs snake_case, `request_id`
  placement, subtype spelling). Round-trip tests cannot catch this. Before the
  hand-rolled code is deleted, capture the **current** output of
  `server_main`/`bridge_messaging` for every spoken control subtype as golden
  fixtures, then assert the new `cc.server.control_protocol` ser/de produces
  **byte-identical** (or, where field order is semantically irrelevant,
  semantically identical after a canonical parse) output for each fixture. The
  hand-rolled JSON is deleted only after this gate is green.
- **Golden gate — result messages (added 2026-10-01).** The S6 gate above
  covers the 21-subtype control protocol; the SDK stdout result-message
  serializers (`server_main.cppm:572-614`, `server_routes.cppm:180-198`,
  `bridge_messaging.cppm:647-684`) sat outside its scope, so the
  `modelUsage`/`model_usage` wire drift went undetected. Four golden fixtures
  (`server_result_message`, `server_error_result`,
  `server_result_ingress_event`, `bridge_serialize_result_message`) now freeze
  the live result-message bytes — the `modelUsage` key (camelCase) and the
  nested `server_tool_use` object inside `usage` — so a future rewire of the
  live emitters to the canonical ser/de is verified byte-for-byte.
- **Existing server regression net (name corrected):** there is no
  `test_server`. The server route is covered in substance by
  `tests/test_services.cpp` `TEST(ServerRoutes, …)` (e.g.
  `:7466`, `:7599`), which exercises the real recipe via
  `LocalMessagesServer` — these must stay green after the route is
  re-expressed through `cc.query.assembly`. (The doc previously misnamed this
  `test_server`.)
- **`bridge_messaging` has NO existing regression net (F6):** `tests/test_bridge.cpp`
  does not import `cc.bridge.bridge_messaging` and does not test
  `SDKControlRequest`/`is_sdk_control_*` (zero references in `tests/`). The
  bridge rewire therefore runs uncovered unless one is added. Resolution: the
  golden gate above covers the bridge's spoken subtypes at the byte level, and
  a new unit test (in `test_bridge` or a sibling) asserts the `is_sdk_control_*`
  type guards and the `SDKControlRequest`/`Response` ser/de against the
  canonical `cc.server.control_protocol` types. The earlier "whichever covers
  `bridge_messaging`" claim is dropped — nothing covers it today.
- **`graph_check.py`:** zero NEW upward edges, zero NEW dead imports.
- **Dual-preset:** `local-linux` + `local-linux-release`, serial `ctest -j1`.

### 4.3 Phase 3 rollback

Phase 3 is also a sequence of revertible commits:

1. **`cc.query.assembly` extraction commit:** move the recipe from
   `server_routes.cppm` to `query_assembly.cppm`; rewire the server route to
   call it. Rollback: `git revert` — restores the recipe in `server_routes.cppm`.
   The server route is the only caller; no SDK change.
2. **`QueryEngine::set_wire_backend_factory` commit:** add the seam. Rollback:
   `git revert` — the seam is additive; no caller depends on it until the
   harness lands. *(Implementation deviation, recorded: the seam landed in
   the harness commit `f81cddd` rather than its own commit — the harness is
   the first and only caller, so a revert of the harness commit also reverts
   the seam. The seam remains additive and behavior-neutral for existing
   callers; the deviation is in commit granularity, not in the seam's design
   or rollback safety.)*
3. **`cc.sdk.harness` commit:** add the harness module + impl unit +
   `test_sdk_harness`. Rollback: `git revert` — the harness is additive; the
   server route (already on `cc.query.assembly`) is unaffected.
4. **Install/EXPORT commit:** uncomment + update the root install block.
   Rollback: `git revert` — re-comments the block; no build/test effect.

Each commit passes the dual-preset gate before the next. The harness commit is
the riskiest (new code linking the engine); it is isolated from the server
re-expression so a harness problem never blocks the server.

### 4.4 Phase 3 test plan

- **New: `test_sdk_harness`** (links `cc_sdk` + `cc_query`):
  - **Loopback HTTP server, not a mock `WireBackend` (S1).** Spin up an
    `httplib::Server` on `127.0.0.1` with an ephemeral port; serve a canned
    Anthropic Messages API response (and an SSE stream for the streaming
    case). Construct a `Harness` with `base_url` pointing at the loopback and
    a dummy `api_key`. Assert `run()` returns a `TurnResult` with the expected
    `message`/`usage`/`tool_rounds` — exercising the real transport, SSE, and
    tool loop with zero external calls. (A mock `WireBackend` cannot do this:
    the seam reaches request-body serialization only — §2.4.)
  - Assert `abort()` before `run()` causes `run()` to return an error
    (`"Query interrupted"`). This works because the harness checks its own
    `abort_requested_` flag at `run()` entry (§2.3) — the engine's `aborted_`
    is auto-reset at `query()` entry (query_engine_loop.cpp:25), so the
    harness-level flag is what makes a pre-run abort observable.
  - Assert `stream()` delivers `StreamEvent`s to the `EventSink` (via the
    loopback SSE response).
  - Assert `resume(session_id)` with `sessions_dir` set restores a prior
    conversation: seed a `messages.jsonl` via `cc.session.storage`, then
    `resume` (which loads via `load_messages` + `parse_session_message_value` +
    `restore_conversation`, §2.3) and check `conversation()`.
  - Assert the `PermissionCallback` is invoked for a tool that requires
    permission. Register a mock permission-gated tool via the
    `AssemblyConfig::register_extra_tools` seam (§2.1) — the earlier draft
    named no registration seam, so the test could not have been written.
- **Existing server direct-connect tests (name corrected):** there is no
  `test_server`; the faithfulness gate is `tests/test_services.cpp`
  `TEST(ServerRoutes, …)`, which must stay green after the server route is
  re-expressed through `cc.query.assembly` (the recipe behavior is unchanged).
  This includes the `set_query_executor_for_testing` test at
  `tests/test_services.cpp:7889`, which the adapter preserves by keeping the
  `query_executor_override` short-circuit ahead of `assemble(...)` (§2.1).
- **Existing: `test_query_engine`:** must stay green after the
  `set_wire_backend_factory` seam is added (the seam is additive; the default
  path is unchanged).
- **`graph_check.py`:** zero NEW upward edges (in particular, no
  `cc.server → cc.sdk` edge), zero NEW dead imports.
- **Dual-preset:** `local-linux` + `local-linux-release`, serial `ctest -j1`.
- **Install smoke test (manual, one-time):** `cmake --install` the build tree,
  then configure a tiny consumer project that does `find_package(loom)` and
  links `loom::sdk`; verify it compiles and links against the installed `.cppm`
  files. This validates the BMI install story before any external commitment.

---

## 5. Open questions / deferred decisions

1. **`cc.server.control_protocol` vs `cc.bridge.control_protocol` placement.**
   Both are rank 13; the module can move between them without a layering change.
   Recommended: `cc.server` (it already owns `cc.server.types` with the ser/de
   pattern). Deferred to the phase-2 implementation batch.
2. **`PermissionMode` consolidation.** The live tree has 10 `PermissionMode`
   definitions (mode_validation.cppm:28, app_state.cppm:22, ui_types.cppm:198,
   tasks/types.cppm:219, …). Phase 2 converges the SDK's copy onto
   `cc.tools.mode_validation` but does not consolidate the live duplicates —
   that is a separate cleanup with its own blast radius.
3. **`ModelUsage` extra fields — resolved (no longer deferred).** `cost_usd`/
   `context_window`/`max_output_tokens`/`web_search_requests` have no
   type-layer twin. This was previously deferred, but it is **not** deferrable:
   the moved wire messages embed `unordered_map<string, ModelUsage>`
   (core_types.cppm:101,127), so `ModelUsage` must have a rank-≤13 home before
   the MOVE. Resolution: `ModelUsage` moves **whole** to
   `cc.server.control_protocol` as a wire struct, all 8 fields retained — the
   wire carries them via `modelUsage` (camelCase; §1.2 row), so no field is
   stripped (§1.2 move
   closure). The `TokenUsage` CONVERGE applies to the standalone
   `NonNullableUsage` type, not to the wire struct. If no consumer needs the
   four no-twin fields after the move, they are dropped in a later cleanup —
   but the move does not wait on that decision.
4. **`loom.sdk` module-name promotion.** Deferred until a real external consumer
   exists (§2.6). The install namespace `loom::` ships in phase 3; the C++
   module names stay `cc.sdk.*`.
5. **In-process MCP server injection.** `SdkMcpToolDefinition` and
   `McpSdkServerConfigWithInstance` (runtime_types.cppm:164-182) model an
   in-process MCP server handed to the harness. Phase 3's `HarnessConfig` does
   not yet expose this — it needs a `NativeMcpRuntime`-style registration path.
   Deferred to a phase-3 follow-up once the core harness is green.
6. **`cc_query` link-closure expansion — corrected.** The earlier draft claimed
   `cc_query` "gains `PUBLIC cc_tools cc_orchestration cc_hooks cc_session`",
   reading only `cc_query.cmake`. In fact `cc_tools.cmake:126` already does
   `target_link_libraries(cc_query PUBLIC cc_tools cc_hooks cc_session cc_memdir
   cc_services)`, so `cc_tools`/`cc_hooks`/`cc_session` are **already** in the
   closure. The only genuinely new `PUBLIC` dep is `cc_orchestration`. The
   fallback (keep the recipe in `cc.server` and duplicate ~135 lines in the
   harness) is therefore even less attractive than stated — the link-cost
   argument for it was based on an overstated expansion.

---

## Review history

- **2026-09-29 — two adversarial design reviews, verdict request-changes (both).**
  All required changes applied:
  - *F1 (blocker):* the phase-2 MOVE closure is now moved wholesale — the STAYS
    types the moved DTOs reference (`SlashCommand`, `ModelInfo`, `AccountInfo`,
    `FastModeState`, `AgentInfo`) are re-marked MOVE with the protocol, the
    CONVERGE types they reference are converged first, and `ModelUsage` moves
    as a whole struct to rank 13 (all 8 fields retained — the wire carries them
    via `model_usage`; the `TokenUsage` CONVERGE applies to the standalone
    `NonNullableUsage` type, not to the wire struct's fields); the §1.3
    "imports only `cc.utils.json` + std" boundary claim is corrected (§1.2 move
    closure, §1.3, §3.3).
  - *F2 / S7:* the `cc.server.control_protocol` re-export shim is deleted —
    `cc.sdk.control_schemas`/`control_types` are removed outright (zero
    importers; a shim would `PUBLIC`-link `cc_server` and its httplib/OpenSSL/
    curl closure into the island). Principle 3 is refined to state the real
    phase-2 link closure: the canonical type targets (for the CONVERGE aliases'
    BMIs) + `cc_utils`, with no `cc_query`/`cc_server`/`cc_services` (and thus
    no OpenSSL/httplib/curl) in the SDK closure (§1.1, §1.3, §3.3).
  - *F3 / S3:* the extracted entry point is now assemble-only (`assemble(...)`
    returning a reusable `AssemblyHandle` with an `engine()` accessor), distinct
    from the server's assemble+query+map one-shot path; the settings→config
    mapping is a shared `resolve_engine_config` resolver; the empty-`api_key`
    hard-error moves out of the resolver to the server adapter, so a
    loopback/gateway harness bypasses it (§2.1, §2.2).
  - *F4:* `seed_query_engine_from_session`/`session_line_to_message` (rank 13)
    are deleted; the assembly parses `prior_message_lines` at rank 10 via the
    existing non-lossy `message_from_json_value` and seeds via
    `restore_conversation`; the `query_executor_override` test seam is preserved
    in the server adapter ahead of `assemble(...)` (§2.1, §3.4).
  - *F5 / S2:* every `TurnOptions` field is mapped to an engine seam or dropped —
    `model`→`set_model_params`, `allowed_tools`→`QueryOptions::enabled_tools`,
    `attachments`→`QueryOptions::attachments`; `max_turns`/`max_budget_usd`/
    `system_prompt`/`disallowed_tools` are dropped (construction-time, live on
    `HarnessConfig`) (§2.2 mapping table).
  - *F6:* test names corrected — there is no `test_server`; the server
    regression net is `tests/test_services.cpp` `TEST(ServerRoutes,…)`;
    `bridge_messaging` has zero existing coverage, so a golden wire gate + a new
    `is_sdk_control_*`/ser-de unit test are required (§4.2).
  - *F7:* the link-cost analysis is corrected — `cc_tools.cmake:126` already
    links `cc_query` `PUBLIC`ly to `cc_tools`/`cc_hooks`/`cc_session`/`cc_memdir`/
    `cc_services`; the only new dep is `cc_orchestration` (§2.1, open question 6).
  - *S1 (gate-blocking):* the `WireBackend` seam is re-scoped to request-body
    serialization only (the transport is not intercepted); the phase-3
    first-consumer test is redesigned to use a loopback HTTP server serving
    canned Anthropic/SSE responses; the full transport-seam alternative is
    recorded as deferred (§2.4, §2.5, §4.4).
  - *S4:* ownership/lifetime specified — member declaration order is callbacks →
    hook → registry → engine (matching the `registry → hook → callbacks`
    reference graph), `unique_ptr` members constructed in the constructor body
    after tool registration (the `config.tools` snapshot), callbacks declared
    first so they outlive the hook, abort uses a harness flag wired as the
    external abort callback with an `exchange`-at-entry (the engine auto-resets
    its own `aborted_`), and `resume()` uses `load_messages` +
    `message_from_json_value` + `restore_conversation` with no `cc.server`
    import (§2.3).
  - *S5:* the install list is computed from the real link graph (`cc_wire`
    removed; `cc_services`/`cc_memdir`/`cc_skills_core`/`cc_task_types` added),
    the FetchContent third-party targets (`yyjson`/`uv_a`/`httplib`) are
    installed into the export set (with `find_dependency` only for CURL/
    OpenSSL), and an ABI policy is stated (source-compatible only, same clang
    major + same C++ stdlib, consumer-rebuilt BMIs, no ABI stability) (§2.6).
  - *S6:* a golden wire-compatibility gate is required — byte-identical (or
    canonical-parse-semantically-identical) golden tests over the current
    `server_main`/`bridge_messaging` output for every spoken subtype before the
    hand-rolled JSON is deleted; round-trip `test_sdk_serde` alone cannot detect
    live-wire drift (§4.2).
- **2026-09-29 — independent verification (agent: design-verify), verdict
  approved.** Four minor non-blocking issues folded in: (1) `ModelUsage`
  disposition made explicit — the wire struct moves whole to rank 13 with all 8
  fields retained (the `TokenUsage` CONVERGE applies to `NonNullableUsage`, not
  to the wire struct's fields); (2) dropped the `cc_core` mention from the
  §2.6 install closure (`cc_task_types` arrives via `cc_state`, which links it
  directly; `cc_core` is not in the closure); (3) `assemble()` now states
  explicitly that it performs recipe step 8 (`dynamic_tools_provider`/
  `mcp_input_schema_provider` → `collect_mcp_tool_definitions`/
  `collect_mcp_input_schemas`) internally so MCP tool discovery is not dropped
  from the re-expressed server route; (4) corrected `httplib::httplib` (an
  ALIAS target, which CMake rejects installing) to the real target name
  `httplib` in the §2.6 install plan.
- **2026-10-01 — P2-tests review (agent: p2-tests-review), verdict
  request-changes; all findings addressed in a fix commit.** The P2-tests
  step (7872e0b) added the ser/de round-trip + convergence tests; the review
  found two wire-drift defects the round-trip-only gate could not catch, plus
  process/scope deviations:
  - *Wire drift (high):* `SDKResultSuccess`/`SDKResultError` ser/de emitted
    `model_usage` (snake_case) but every live emitter and the TS
    `SDKResultSuccessSchema` use `modelUsage` (camelCase). Fixed: the ser/de
    now emits/reads `modelUsage`. The design's own `model_usage` spelling
    (§1.2 row, §1.2 move closure, §5.3) was the drifted form and is corrected
    above to `modelUsage`.
  - *Wire drift (high):* `ModelUsage` ser/de emitted snake_case sub-fields
    (`input_tokens`, …) but the TS `ModelUsageSchema` uses camelCase
    (`inputTokens`, `outputTokens`, `cacheReadInputTokens`,
    `cacheCreationInputTokens`, `webSearchRequests`, `costUSD`, `contextWindow`,
    `maxOutputTokens`). Fixed: the ser/de now emits/reads camelCase keys (the
    C++ struct fields stay snake_case).
  - *Usage map shape (low):* the `usage` field was `unordered_map<string,int>`,
    which silently dropped the nested `server_tool_use` object the live wire
    emits. The TS schema is `NonNullableUsagePlaceholder = z.unknown()` (opaque).
    Fixed: `usage` is now an opaque JSON string (`usage_json`), preserving the
    full live shape; the dead `int_map_to_json`/`int_map_from_json` helpers and
    the `NonNullableUsage` alias were removed from the wire module.
  - *Missing field (low):* `structured_output` (`z.unknown().optional()` in the
    TS schema) was absent from the DTO. Fixed: added as
    `optional<string>` (raw JSON) with ser/de.
  - *Golden gate gap (root cause of the drift going undetected):* the S6 golden
    gate covered only the 21-subtype control protocol; result messages sat
    outside its scope. Fixed: four result-message golden fixtures
    (`server_result_message`, `server_error_result`,
    `server_result_ingress_event`, `bridge_serialize_result_message`) now
    freeze the live result-message bytes — the `modelUsage` key and the nested
    `usage` shape — so a future rewire of the live emitters to the canonical
    ser/de is verified byte-for-byte. Wire-spelling locks in `test_sdk_serde`
    assert the camelCase keys directly (a round-trip alone cannot).
  - *Stale comment (low):* the `test_sdk_serde` header claimed string
    comparison with single-key maps; the comparison is semantic (`json_eq`)
    and several tests use multi-key maps. Fixed.
  - *Sequencing deviation (med, recorded):* the alias-then-delete sequence
    (§1.1 principle 1, §4.1 step 1) was not followed for four types. The
    island `EffortLevel` twin was deleted outright in P2-delete (239fe0b) with
    no alias — the name `cc::sdk::runtime::EffortLevel` did not exist between
    239fe0b and P2-tests (7872e0b), where the alias was restored — and
    `ConfigScope`/`SettingSource`/`AgentDefinition` were converged (alias +
    twin deletion in one shot) only in P2-tests. Zero importers were verified
    by grep at 7872e0b^ (only `core_types` re-exports), so no breakage
    occurred; the deviation is recorded here rather than silently left.
  - *Scope deviation (med, recorded):* the P2-tests commit carried ~1695 lines
    of production ser/de (required for the round-trip tests the design
    mandates) plus a production parse-behavior fix (`read_optional_double`,
    correcting `HookCallbackMatcher` timeout corruption from P2-alias that the
    golden gate missed because no fixture includes a hook-matcher timeout).
    The ser/de is defensible (the tests cannot exist without it); the timeout
    fix is a behavior change landing outside the step that introduced the bug,
    and is now covered by the `InitializeRequest` round-trip test
    (`.timeout = 30.0`). Recorded here; no code action needed.
- **2026-10-01 — P3-harness review (agent: p3-harness-review), verdict
  request-changes; all findings addressed in a fix commit.** The P3-harness
  step (f81cddd) added the `cc.sdk.harness` opaque entrypoint; the review
  found one false link-closure claim, one non-deterministic test, and two
  stale doc references:
  - *False link-closure claim (high):* the cmake comment and §2.3/§3.4
    claimed "`OpenSSL::Crypto` stays out of the SDK closure" because
    `cc_sdk` does not link `cc_server`. In fact `cc_query` PUBLIC-links
    `cc_services` (cc_tools.cmake:133) and `CURL::libcurl`
    (cc_query.cmake:42), and `cc_services` PUBLIC-links `OpenSSL::SSL/Crypto`,
    `CURL::libcurl`, `httplib`, `yyjson`, `uv_a` (cc_services.cmake:66-81) —
    so OpenSSL and libcurl DO enter the SDK closure via `cc_services`. The
    reviewer's alternative (make `cc_services`' OpenSSL/CURL links PRIVATE)
    is not viable: `cc_services`' module interfaces expose those types
    (gcp_adc.cppm's exported `EvpPkeyPtr` over `EVP_PKEY*`; client.cppm's
    CURL handle). Fixed by correcting the cmake comment, §2.1, §2.3, and
    §3.4 to state the real closure and explicitly accept it as phase-3 cost
    (the cost of embedding the engine). What stays true: `cc_sdk` does not
    link the `cc_server` target itself. (§2.6 already stated this closure for
    install; the phase-3 sections are now consistent with it.)
  - *Non-deterministic test (med):* `AbortBeforeRunReturnsErrorOnce` set no
    `base_url`, so the second `run()` POSTed to the default API
    endpoint — a real external call on a networked machine.
    Fixed: `config.base_url = "http://127.0.0.1:1"` (closed local port) so
    the transport failure is deterministic and local (§4.4 zero-external-calls).
  - *Commit-granularity deviation (low, recorded):* the
    `set_wire_backend_factory` seam landed in the harness commit (f81cddd),
    not its own commit as §4.3 item 2 specifies. Defensible (the harness is
    the first/only caller); recorded in §4.3. No code action.
  - *Stale resume-path reference (low, recorded):* §2.3/§4.4 specified the
    non-lossy `message_from_json_value`, but the harness (consistent with
    the assembly) uses `parse_session_message_value`, which is
    role/content-string-only and drops `tool_use`/`tool_result`/`image`
    blocks. The deviation (a `cc::services::api::Message` →
    `cc::core::Message` converter does not exist) is now recorded in §2.3;
    a non-lossy reader is a follow-up.
- **2026-10-01 — P3-server review (agent: p3-server-review), verdict
  request-changes; all findings addressed in a fix commit.** The P3-server
  step (d4b347e) re-expressed the server route through `cc.query.assembly`
  and revived the install/EXPORT; the review found one install-blocking
  defect, one package-name defect, and three recorded lows:
  - *Exported targets carry no cxx_std_23 (high, install-blocking):* the
    root CMakeLists.txt sets `CMAKE_CXX_STANDARD 23` only at directory
    scope, and no `cc_*` target calls `target_compile_features`, so the
    installed `LOOMTargets.cmake` recorded zero `CXX_COMPILE_FEATURES`. A
    consumer linking `loom::sdk` failed at generate time on every
    synthesized module target ("has C++ sources that use modules, but does
    not include cxx_std_20 (or newer) … found cxx_std_17") — the installed
    `.cppm` files could not be compiled by any downstream project, failing
    the design's own §4.4 install smoke test. Fixed: a loop over the 21
    exported `cc_*` targets in the root install block now sets
    `target_compile_features(<t> PUBLIC cxx_std_23)` (in-tree no-op: the
    tree already builds C++23 via the directory-level standard). Verified
    with the §4.4 smoke test: a consumer doing `find_package(loom)` +
    `target_link_libraries(consumer PRIVATE loom::sdk)` with no
    workaround now configures, compiles the installed `.cppm` files (348
    synth-module BMIs), links, and runs.
  - *Install config not self-contained (high, discovered via the §4.4
    smoke test):* once the cxx_std_23 gate was cleared, the consumer
    configure failed on `Threads::Threads` (referenced by the installed
    `loom::httplib` target's `INTERFACE_LINK_LIBRARIES`). httplib's
    compression/OpenSSL support is on by default, so its interface also
    references `ZLIB::ZLIB` and `Brotli::*`, and its
    `INTERFACE_COMPILE_DEFINITIONS` propagate
    `CPPHTTPLIB_BROTLI_SUPPORT`/`CPPHTTPLIB_ZLIB_SUPPORT` — the `cc_*`
    objects were compiled against those symbols, so a consumer link needs
    them resolved. Fixed: `loomConfig.cmake` now `find_dependency`s
    `Threads`, `ZLIB`, and `Brotli COMPONENTS encoder decoder common`;
    CMake ships no `FindBrotli`, so httplib's own `cmake/FindBrotli.cmake`
    is installed next to the config and the config appends its directory
    to `CMAKE_MODULE_PATH` first. (The smoke-test consumer also needs the
    build's libc++ toolchain flags and `CMAKE_CXX_EXTENSIONS OFF` — the
    §2.6 ABI policy's "same clang++ major + same C++ stdlib" requirement;
    the std BMI is built with extensions off and a `gnu++23` TU cannot
    load it.)
  - *Package-name case mismatch (med):* the design (§2.6, §4.4) documents
    `find_package(loom)`, but the installed config was `LOOMConfig.cmake`
    (from `project(LOOM)`), which on a case-sensitive filesystem answers
    only to `find_package(LOOM)` (verified with CMake 3.31.2). Fixed: the
    installed files are now `loomConfig.cmake` / `loomConfigVersion.cmake`
    under `lib/cmake/loom`; §2.6 notes the case-sensitivity. The template
    keeps its on-disk name `cmake/LOOMConfig.cmake.in`; its
    `check_required_components` now takes `loom`.
  - *Tree-sitter install-closure gap (low):* under
    `CC_ENABLE_TREE_SITTER=ON`, `cc_utils` PUBLIC-links `tree-sitter`/
    `tree-sitter-bash`, which were not in the install set — and their
    plain build-tree `INTERFACE_INCLUDE_DIRECTORIES` would themselves
    break `install(EXPORT)` ("prefixed in the build directory", verified
    with a scratch CMake 3.31.2 project). Fixed: the targets join the
    export set conditionally, their include dirs are wrapped in
    `$<BUILD_INTERFACE:…>`, and `tree_sitter/api.h` is installed (the bash
    grammar entry point is consumed via an `extern "C"` declaration, so no
    tree-sitter-bash header is needed). Default is OFF, so the default
    install path is unchanged and verified; the ON configuration follows
    the same pattern as the yyjson INSTALL_INTERFACE fix but is not
    buildable on this host (no network to fetch tree-sitter).
  - *Commit-granularity deviation (low, recorded):* the commit combined
    the server re-expression (§4.3 item 1's rewire half, deferred from
    c56489e by declared deviation) with the install/EXPORT revival
    (§4.3 item 4), making revert coarser than the design's revertible-
    commit plan. The split was already declared in c56489e's message;
    recorded here for the review log. No code action.
  - *Keep-imports + comment imprecision (low, recorded):* the four
    keep-imports in `server_routes.cppm` (`cc.query.query_engine`,
    `cc.tools.tool`, `cc.tools.runtime_registry`,
    `cc.orchestration.runtime_backends`) are retained purely as
    BMI-reachability workarounds for the clang 22 SIGSEGV (LLVM #184957),
    with no textual name references — documented, arch-check-marked, and
    graph_check-clean (zero new dead imports); fragile but not a blocker.
    The CMakeLists install-block comment claiming ftxui-screen enters the
    closure via a textual `<ftxui/screen/color.hpp>` include was imprecise
    — it enters via `cc_utils`'s PUBLIC TLL `ftxui::screen`
    (cc_utils.cmake:109); the comment is corrected (the `screen` install
    entry itself was already right).
