# RFC 0001 — Zero-importer module triage (197 candidates at HEAD `0a12cd9`)

Status: triage attachment (read-only analysis; no source changes made).
Date: 2026-09-28.
Scope: every `export module X;` primary interface unit under `src/**/*.cppm`
with **zero** textual `import X;` / `export import X;` in `src/`, `tests/`,
`benchmarks/`, or `src/main.cpp`.

## 1. Methodology and headline counts

The scanner was reproduced at HEAD (837 exported primary modules; 197
zero-importer modules — exact match to the mechanical scan in the brief).
Each candidate was then checked five independent ways:

1. **Textual reachability.** Raw fixed-string grep of the full module name
   tree-wide (catches comments, macro/token-pasted forms, weird syntax).
   Only 11 modules produced hits outside their own file; every hit resolved
   to either (a) a *sibling* module whose name is a prefix extension
   (`cc.services.diagnostic` inside `import cc.services.diagnostic.dump_diagnostic;`),
   or (b) stale roadmap comments (see §5). No `#include` of any candidate
   `.cppm` exists anywhere.
2. **Link-level audit (strongest signal).** For every candidate object
   (`<target>.dir/<rel>.cppm.o`) the set of strong global symbols
   (`T/D/B/R`, including the per-module `initializer for module` `_ZGI…`
   symbol that every importing TU must reference) was intersected with the
   defined symbols of **all 47 ELF binaries** in `build/debug` — `loom`,
   `pare-benchmark`, every `tests/test_*`, the three `tests/e2e/*`,
   `tests/benchmarks/bench_core`, and `bin/phase3_permission_smoke`.
   **Result: 0 of 197 contribute any strong symbol to `loom`; 196 contribute
   to no binary at all; 1 (`cc.hooks.file_watcher`) is pulled into exactly
   one test binary, accidentally** (see §4, DO-NOT/SECOND-LOOK).
3. **Undefined-symbol audit.** All strong symbols of all 197 candidate
   objects were cross-referenced against the undefined-symbol union of all
   1,331 compiled objects and 35 static archives. **Zero candidate strong
   symbols are referenced undefined by any TU** — i.e. no module-attached
   entity of any candidate is called anywhere, even from a same-target
   object. (Targets are plain STATIC libraries with no `--whole-archive`;
   unreferenced archive members are dropped at link time, verified via the
   `loom` link rule in `build/debug/build.ninja`.)
4. **Re-export audit.** All 21 `export import` lines tree-wide were listed;
   **none of the 19 re-exported modules is in the 197** (the scanner counts
   `export import` as an import). Six modules are reachable *only* through an
   umbrella export-import and would make a strict-(non-export)-import scan;
   they are listed in §3 and are all genuinely live (umbrella has live
   importers) except one dead-chain oddity noted there.
5. **Companion/shape audits.**
   - Zero candidate has a `.cpp` module implementation unit (`module X;`).
   - Each candidate is listed exactly once in exactly one
     `src/cmake/targets/cc_*.cmake` FILE_SET (1:1 ownership verified).
   - Module initializer objects were inspected: nearly all are size `0x6`
     (bare `ret`); ~40 modules with larger initializers only construct
     namespace-scope `const` strings/tokens — **none of these objects is
     linked**, so no initializer currently runs. There is no
     self-registration/side-effect pattern among the 197 (no registry
     populated by a namespace-scope object).
   - String-keyed registries were checked (`runtime_registry_*.cpp`,
     `command_registry_init_*.cpp`, `ui/tools/tool_ui_*`,
     `utils/hooks/hooks_registry.cppm`): every factory registered by string
     is registered from an impl unit that **imports the factory's module**,
     so the zero-import result already rules out dynamic wiring. Tool-name
     strings for the dead tool modules (`"task_stop"`, `"task_update"`,
     `"task_output"`, `"task_get"`, `"brief"`, `"testing"`) are served by
     *different* live code (`cc.tools.task` /
     `runtime_registry_executors.cpp`).
   - `tools/arch/*_baseline.txt` rows that name candidates are listed per
     batch; the graph/inline ratchets only fail on *gains*, so shrinking
     (deleting rows together with the module) is the supported flow.

### Headline counts

| Count | Set |
|---|---|
| 837 | exported primary modules at HEAD |
| 197 | zero direct importers (the candidate set) |
| **188** | **DEAD-CONFIRMED** in this triage |
| **9** | **LIKELY-DEAD-NEEDS-SECOND-LOOK** (technically dead; product/link caveat) |
| 0 | KEEP-REEXPORT / KEEP-STATIC-INIT / KEEP-DYNAMIC / KEEP-OTHER |
| +18 | transitive orphans (not in the 197: imported **only** by candidate modules; fixed-point closure), also absent from all 47 binaries |
| **215 files / 62,117 LOC** | full dead island if the 197 + closure are removed (21.6% of the 287,557 lines of `.cppm` in `src/`) |

Build freshness: every candidate source is older than its compiled object
and the tree is clean at `0a12cd9`, so the link audit reflects HEAD.

Bottom line: **all 197 modules are functionally dead in every shipped
binary at HEAD.** The 9 second-look classifications are conservatism about
intent (parked "UI2" screens/input stack, a dead-on-arrival SDK surface, and
one test-link quirk), not evidence of liveness.

## 2. Re-export map (finding 4)

All 21 `export import` lines, with the re-exported module's status:

| Umbrella (live importers) | Re-exported module | In 197? |
|---|---|---|
| `cc.services.mcp.types` (25), `cc.config.config` (13) | `cc.config.mcp_types` | no — live |
| `cc.skills.skill` (24) | `cc.skills.file_access.port` | no — live |
| `cc.skills.load_skills_dir` (11) | `cc.skills.skill` | no — live |
| `cc.tools.tool` (49) | `cc.types.types`, `cc.types.tool_types` | no — live |
| `cc.ui.screens.repl_screen` (13) | `cc.ui.screens.repl_state` | no — live |
| `cc.ui.widgets.all_components` (6) | `cc.ui.foundation.components_figures`, `cc.ui.foundation.ui_types`, `cc.ui.widgets.fast_icon`, `pr_badge`, `spinner`, `dev_bar`, `stats`, `tag_tabs`, `text_input`, `cc.ui.dialogs.feature_dialogs` | no — live (REEXPORT-LIVE) |
| `cc.migrations.config_orchestrator` (**0 importers — itself candidate**) | `cc.migrations.migration_runner`, `cc.migrations.schema_versions`, `cc.migrations.migration_registry` | not in 197, but see below |
| `cc.migrations.migration_registry` (only the dead orchestrator) | `cc.migrations.migration_runner` | not in 197 |

Strict-(non-export-import)-only scan yields 203 zero-importer modules; the
six extra vs the 197 are `cc.ui.dialogs.feature_dialogs`,
`cc.ui.widgets.{dev_bar,fast_icon,pr_badge,stats}` (all REEXPORT-LIVE via
`all_components`, 6 importers) and `cc.migrations.migration_registry`
(reached only by the dead orchestrator — it is one of the **18 transitive
orphans** and ships in batch B7; `migration_runner`/`schema_versions`
remain live via `tests/test_migrations.cpp`).

## 3. Classification tables

Legend — **DC** = DEAD-CONFIRMED; **SL** = LIKELY-DEAD-NEEDS-SECOND-LOOK.
Shared evidence for every row (omitted below for brevity): zero textual
importers; zero strong-symbol U-refs in 1,331 objects / 35 archives;
strong symbols absent from `loom` and (except `file_watcher`) from all 47
debug ELFs; no companion `.cpp` impl unit; exactly one CMake FILE_SET row.
The "evidence" column gives the *additional* distinguishing fact
(live-twin homonym file, stale comment, baseline row, registry fact).

### 3.1 buddy (target `cc_buddy` — whole target dead; batch B2)

| Module | Class | Evidence |
|---|---|---|
| `cc.buddy.buddy_hooks` | DC | `BuddyNotification`/`TriggerPosition` name collisions only (homonyms in `ui/prompt/combined_highlights.cppm`); sole buddy chain, target has zero live importers |
| `cc.buddy.buddy_prompt` | DC | `AttachmentMessage` homonym is the live `ui/messages/message_row.cppm` type; only referenced by orphan companions `buddy_companion`/`buddy_types` |

### 3.2 cli (batch B8)

| Module | Class | Evidence |
|---|---|---|
| `cc.cli.handlers.agents` | DC | live CLI agent surface is `cc.cli.websocket_transport` (imported by `main.cpp`) + `commands/agents.cppm`; `AgentInfo` etc. are homonyms |
| `cc.cli.handlers.plugins_handler` | DC | live plugin CLI lives in `commands/plugin_cmd.cppm` + `utils/plugin/plugin_loader.cppm`; `PluginManifest`/`PluginInfo` homonyms |
| `cc.cli.transports` | DC | twin transport stack `cc.bridge.transport` / `cc.cli.websocket_transport` (used by `main.cpp`, `tests/test_bridge.cpp`); `StdioTransport` homonym in `services/mcp/transport_stdio.cppm` |

### 3.3 config / constants / context (batches B9 / B3)

| Module | Class | Evidence |
|---|---|---|
| `cc.config.model_config` | DC | live model registry is `services/api/models.cppm` + `utils/model/model_aliases.cppm`; `ModelCapabilities`/`ModelCost` homonyms; `ANTHROPIC_API_KEY` hits are env reads, not refs |
| `cc.constants.figures` | DC | live figures: `ui/foundation/design_figures.cppm`/`components_figures.cppm` (duplicated constants; consumed by `tests/test_ui_light.cpp`) |
| `cc.constants.output_styles` | DC | live styles: `commands/output_style.cppm` + `skills` frontmatter path; `SettingSource` homonym |
| `cc.context.mailbox` | DC | left side of one `dead_imports_baseline.txt` row (delete with module); live messaging = `tools/runtime_message_delivery.cppm`/`send_message_tool.cppm` |
| `cc.context.notifications` | DC | whole `cc_context` target dead (2/2 modules); live notification flow goes through `cc.state` + `ui/prompt` |

### 3.4 entrypoints (target `cc_entrypoints` — whole target; batch B1)

| Module | Class | Evidence |
|---|---|---|
| `cc.entrypoints.control_types` | SL | header: "Control protocol types … external integration"; whole SDK surface unlinked — product decision needed; twins `server/types.cppm`, `entrypoints/core_types.cppm` (itself orphan) |
| `cc.entrypoints.runtime_types` | SL | same; `EffortLevel` homonym of live `utils/model/effort.cppm` |
| `cc.entrypoints.sdk_types` | SL | same; `ToolResultBlock`/`ThinkingBlock` homonyms of live `cc.types.types` (used by wire/compaction/history) |
| `cc.entrypoints.settings_types` | SL | same; `Settings` homonym of live `cc.config.settings` |

Transitive companions in the same target (also SL-by-association, in B1):
`cc.entrypoints.control_schemas`, `core_schemas`, `core_types`,
`sandbox_types` — their only importers are the four SL modules above.

### 3.5 hooks (target `cc_hooks`, 26/40 modules; batch B12)

These are the TS-era "hook" faithful ports; the live app constructs hooks
only in `ui/app/app_constructor.cpp` (imports `cc.hooks.lifecycle_hooks`
and `cc.hooks.cost_hook` — both live, not candidates).

| Module | Class | Evidence (live twin / note) |
|---|---|---|
| `cc.hooks.assistant_history` | DC | live history is `cc.session.history`; `HistoryMessage` homonym `types/history.cppm` |
| `cc.hooks.auto_save` | DC | live auto-save state in `state/persistence.cppm` (`is_auto_save_enabled`) |
| `cc.hooks.away_summary` | DC | `AwaySummary` consumed nowhere; homonym struct appears inline in live message screens |
| `cc.hooks.background_task_navigation` | DC | live task navigation in `ui/screens/repl_screen_messages.cpp`/`commands/tasks_cmd.cppm` |
| `cc.hooks.diff_data` | DC | live diffs: `ui/visual/diff_view.cppm` + `utils/git/git_diff.cppm` (`DiffLine` homonyms) |
| `cc.hooks.file_watcher` | **SL** | **only candidate present in any test binary** (`test_tools`): its weak `V std::__atomic_unique_lock…__set_locked_bit` (a libc++-module COMDAT) is U-referenced by `test_tools.cpp.o`/`cc_ui` members, so the archive member is accidentally co-extracted, dragging in strong `FileWatcher` symbols that nothing calls. Not in `loom`. Delete only after a clean full rebuild shows `test_tools` re-links (the `V` resolves from `libcc_std`, as `loom` already proves) |
| `cc.hooks.ide_integration` | DC | live IDE bridge: `services/ide_integration.cppm` + `ui/dialogs` live renderers |
| `cc.hooks.input_buffer` | DC | live input: `hooks/text_input.cppm` + `ui/prompt/prompt_input.cppm` (own `InputBuffer`) |
| `cc.hooks.input_hooks` | DC | `PasteDetector` homonym defined locally in `ui/prompt/prompt_input.cppm` |
| `cc.hooks.main_loop_model` | DC | `ModelConfig` homonym of live `services/api/models.cppm` |
| `cc.hooks.notifications` | DC | `Notification` homonym widely defined in live modules (`hooks/lifecycle_hooks.cppm`, `services/notifier/notifier.cppm` etc.) |
| `cc.hooks.permission_context` | DC | live permission store: `hooks/tool_permission_gate.cppm`/`permission_resolver.cppm`; call sites in `main.cpp`/`server_main.cppm` resolve to `types/permissions.cppm` twin |
| `cc.hooks.permissions` | DC | live modes: `utils/security/permissions_engine.cppm` + `hooks/tool_permissions.cppm`; `parse_permission_mode` homonym there |
| `cc.hooks.pr_status` | DC | `PrReviewState` defined locally in live `ui/widgets/pr_badge.cppm`/`all_components.cppm` |
| `cc.hooks.prompt_suggestion` | DC | live suggestion service: `services/prompt_suggestion/prompt_suggestion.cppm`; state twin in `cc.state` |
| `cc.hooks.repl_bridge` | DC | live bridge: `cc.bridge.transport`/`api.cppm`/`config.cppm` (`BridgeConfig`/`BridgeMessage` homonyms, `tests/test_bridge.cpp`) |
| `cc.hooks.settings_hooks` | DC | live settings: `utils/settings/settings_manager.cppm` (`SettingValue`, `UnsubscribeFn` homonyms) |
| `cc.hooks.swarm_hooks` | DC | live swarm state in `coordinator/swarm.cppm` + app team shards |
| `cc.hooks.swarm_permission_poller` | DC | no external refs at all |
| `cc.hooks.task_hooks` | DC | live tasks: `tasks/` subsystem + `tools/runtime_registry_executors.cpp` (`TaskStatus` homonym) |
| `cc.hooks.tasks` | DC | second duplicate task-hook island; `TaskChangeCallback`/`TaskStatus` shared only with the equally-dead `task_hooks.cppm`; live task types in `tasks/types.cppm` |
| `cc.hooks.tool_permission.coordinator_handler` | DC | live coordinator path: `hooks/tool_permission.cppm` (imported by `main.cpp`) |
| `cc.hooks.tool_permission.interactive_handler` | DC | same; live interactive prompting in `ui/permissions` live panels |
| `cc.hooks.tool_permission.swarm_worker_handler` | DC | same |
| `cc.hooks.turn_diffs` | DC | `FileDiff` homonyms in live `hooks/diff_data.cppm`…/`ui/visual/diff_view.cppm`; whole turn-diff hook chain unlinked |
| `cc.hooks.vim_input` | DC | live vim: `cc.vim.vim_controller` (2 importers) + `hooks/text_input.cppm` (`VimInputHook` homonym) |

### 3.6 keybindings (batch B6)

| Module | Class | Evidence |
|---|---|---|
| `cc.keybindings.keybinding_system` | DC | live system: `keybindings/load_user_bindings.cppm`/`defaults.cppm` + `commands/keybindings_cmd.cppm` |
| `cc.keybindings.keybindings` | DC | duplicate parser island; references only dead `resolver`/`keybinding_system`; live resolver is `keybindings/validate.cppm`/`shortcut_format.cppm` |
| `cc.keybindings.match` | DC | live key matching in `hooks/text_input.cppm`/`keybindings/load_user_bindings.cppm` (own `KeyEvent`) |
| `cc.keybindings.resolver` | DC | `get_default_bindings`/`load_user_bindings` name hits are live functions in `keybindings/defaults.cppm`/`load_user_bindings.cppm`, not this module |

### 3.7 memdir / migrations / plugins / query / schemas / session / skills (batches B7 / B9 / B4)

| Module | Class | Evidence |
|---|---|---|
| `cc.memdir.memory` | DC | live memory: `cc.memdir.memdir` (2 importers) + `cc.memdir.paths` (4) |
| `cc.migrations.config_orchestrator` | DC | 4 rows in `upward_edge_baseline.txt` (delete with module); live migrations are `migration_runner`+`concrete` (`tests/test_migrations.cpp`) |
| `cc.plugins.loader` | DC | live loader: `utils/plugin/plugin_loader.cppm` + `plugins/plugin.cppm`/`marketplace.cppm`; `SemVer`/`PluginManifest` homonyms |
| `cc.query.config` | DC | `EffortLevel` homonym of live `utils/model/effort.cppm`; query config lives in `query_engine_ctor.cpp` |
| `cc.query.stop_hooks` | DC | stop logic inline in live `query_engine_loop.cpp`; `StopReason` homonym in `server/types.cppm` |
| `cc.query.token_budget` | DC | live budgeting: `utils/model/token_budget.cppm` (`TokenBudget`, `max_context_tokens` call sites in live `query_engine*`) |
| `cc.schemas.validation_schemas` | DC | whole `cc_schemas` target (1/1); `ValidationRule` homonym in live `utils/settings/settings_rules.cppm` |
| `cc.session.session` | DC | live session: `cc.session.history` (`main.cpp`) + `cc.session.storage` (4 importers); `SessionStorage` homonym is `utils/session/session_storage.cppm` |
| `cc.skills.bundled.verify` | DC | every other `skills/bundled/*` skill has a live importer (registry); only `verify` is unregistered |

### 3.8 services (27 modules; batch B13)

| Module | Class | Evidence (live twin / note) |
|---|---|---|
| `cc.services.agent_summary` | DC | live summaries in `query/query_engine_compaction.cpp`; `TokenBudget` homonym `utils/model/token_budget.cppm` |
| `cc.services.api.error_utils` | DC | `ApiError` homonym in `constants/constants.cppm`; retry logic in live `services/api/with_retry.cpp` |
| `cc.services.api.logging` | DC | live usage accounting: `constants/cost_tracker.cppm` + `services/api/client.cppm` (`TokenUsage` homonym) |
| `cc.services.auto_dream` | DC | no live wiring; `MemoryEntry` homonym `memdir/memdir` (live) |
| `cc.services.auto_dream.consolidation_lock` | DC | live lock file util: `utils/fs/lockfile.cppm` (`LockFile` homonym) |
| `cc.services.compact` | DC | **949 LOC**; live compaction imports `cc.services.compact.api_microcompact` (`query_engine.cppm`, `tests/test_services.cpp`) — a different module |
| `cc.services.compact.grouping` | DC | only consumer is the dead `cc.services.compact`; no external refs |
| `cc.services.compact.types` | DC | `CompactResult`/`CompactConfig` re-defined locally in live `auto_compact.cppm` (the live one) |
| `cc.services.diagnostic` | DC | live diagnostic used by commands is `cc.services.diagnostic.dump_diagnostic` (imported by `perf_issue.cppm`, `bughunter.cppm`) — different module |
| `cc.services.lsp.manager` | DC | live LSP facade: `services/lsp/LSPServerManager.cppm`/`client.cppm`; `ServerCapabilities` homonym there |
| `cc.services.magic_docs` | DC | `Framework` homonyms in live `about_dialog.cppm`/agent runtime; no service wiring |
| `cc.services.mcp.mcp_server` | DC | live MCP server: `services/mcp/connection_manager.cppm` + daemon wiring; `TransportConfig` homonym |
| `cc.services.mcp_transport` | DC | live transports: `services/mcp/in_process_transport.cppm`/`connection_manager.cppm` (`LinkedTransportPair` homonym there) |
| `cc.services.notifier` | DC | live user notifications go through `cc.state`/`ui/prompt`; no desktop-notifier call sites |
| `cc.services.oauth.types` | DC | DTO twin of live `services/mcp/types.cppm` |
| `cc.services.output_styles` | DC | live loader: `commands/output_style.cppm` (`OutputStyle` homonym) + skills frontmatter |
| `cc.services.policy.types` | DC | `PolicyViolation` homonym in live `utils/plugin/plugin_validation.cppm` |
| `cc.services.prevent_sleep` | DC | no refs; `TimePoint` is a std chrono homonym |
| `cc.services.proxy` | DC | live proxy resolution: `utils/http/http.cppm`/`proxy_utils.cppm` (`ProxyConfig` homonym); env-name hits are unrelated env reads |
| `cc.services.remote_session` | DC | live remote sessions: `cc.bridge.session_api`/`ccr_client.cppm` |
| `cc.services.remote_settings.sync_cache_state` | DC | live `SyncState` defined in `cc.ui.screens.repl_state` + app shards |
| `cc.services.remote_settings.types` | DC | no external refs |
| `cc.services.session_memory` | DC | live memory injection: `query_engine_system_prompt.cpp` + `utils/session/session_helpers.cppm` (`SessionMemory` homonym) |
| `cc.services.tips` | DC | **entire tips-registry feature dead** — only cross-ref is the other dead file `tip_registry.cppm`; live "tips" are static spinner/welcome strings in `ui/app/app_constructor.cpp` |
| `cc.services.tips.tip_registry` | DC | referenced only by dead `cc.services.tips` |
| `cc.services.tool_summary` | DC | live summaries: `commands/summary.cppm` + session listing (`SessionSummary` homonym) |
| `cc.services.vcr` | DC | VCR testing service: zero refs; `MatchStrategy` homonym in live permission rule UIs |

### 3.9 tools (batch B11)

| Module | Class | Evidence |
|---|---|---|
| `cc.tools.brief` | DC | `"brief"` is registered/dispatched in live `runtime_registry_register.cpp:275`/`runtime_registry_dispatch.cpp:60`; executor `execute_brief` is defined in `runtime_registry_executors.cpp:359` — not this class |
| `cc.tools.config` | DC | live config surface is `/config` command (`commands/config.cppm`, fixed recently in followup c8); `ConfigAction` homonym there |
| `cc.tools.shared_tool` | DC | 26 LOC, zero refs |
| `cc.tools.task_get` | DC | live `task_get` = `cc.tools.task::TaskGetTool` (`task_tool.cppm:356`), string-dispatched in `runtime_registry_executors.cpp:171` |
| `cc.tools.task_output` | DC | same: live class in `task_tool.cppm:509`; `get_task_output` homonym in live `utils/tasks/task_output.cppm` |
| `cc.tools.task_stop` | DC | live class `cc.tools.task::TaskStopTool` in `task_tool.cppm:433` (candidate's class lives in namespace `cc::tools::task_stop`, distinct mangle; confirmed no ODR clash) |
| `cc.tools.task_update` | DC | live class in `task_tool.cppm:467`; candidate's `TaskUpdateTool` is a different entity (`cc::tools::task_update`) |
| `cc.tools.testing_tool` | DC | `"testing"` registered live at `runtime_registry_register.cpp:425` with an inline executor; `summarize`/`status_text` are local lambdas elsewhere |

### 3.10 types (batch B10)

| Module | Class | Evidence |
|---|---|---|
| `cc.types.hooks` | DC | `TextInputState` homonym in live `hooks/text_input.cppm`; `CompletionItem` in live `hooks/typeahead.cppm` |
| `cc.types.logs` | DC | live persisted logs: `types/history.cppm` + `session/storage.cppm` |
| `cc.types.permissions` | DC | live permission DTOs: `hooks/tool_permissions.cppm` (imported by `main.cpp`) + `utils/security/permissions_engine.cppm` |
| `cc.types.plugin` | DC | live plugin DTOs: `plugins/plugin.cppm` |
| `cc.types.timestamp` | DC | call sites use std chrono / live `types/history.cppm`; `Timestamp` homonyms |

### 3.11 ui — chrome + foundation (batch B14)

| Module | Class | Evidence |
|---|---|---|
| `cc.ui.chrome.renderer` | DC | live rendering is the FTXUI event-driven screen stack (`ui/chrome/layout.cppm`, renderer shards in `ui/app`); this is the Ink-era screen-buffer port |
| `cc.ui.foundation.design_extras` | DC | live fuzzy picker: `ui/prompt/fuzzy_rank_nucleo.cppm` + `ui/widgets/custom_select` |
| `cc.ui.foundation.dialog` | DC | live: `ui/dialogs/dialog_frame.cppm`/`wizard_dialog.cppm` (`DialogConfig` homonym) |
| `cc.ui.foundation.divider` | DC | `render_horizontal_divider`/`repeat_divider_segment` zero call sites; dividers inlined in live primitives |
| `cc.ui.foundation.list_item` | DC | zero refs |
| `cc.ui.foundation.progress_bar` | DC | live progress: `ui/messages` live renderers |
| `cc.ui.foundation.status_icon` | DC | zero refs; live icons in `design_figures` |
| `cc.ui.foundation.tabs` | DC | live tabs: `ui/widgets/tag_tabs.cppm` (re-exported via `all_components`) |
| `cc.ui.foundation.themed_box` | DC | live boxes: `ui/foundation` live primitives (`components_figures` umbrella) |
| `cc.ui.foundation.themed_text` | DC | zero refs |
| `cc.ui.foundation.ui_formatting` | DC | zero refs; only external name hit is a TS-provenance comment at `ui_formatting.cppm:10`; live formatting lives in the foundation tokens/primitives |

### 3.12 ui — dialogs (batch B15)

Faithful-ported FTXUI dialogs that were superseded by the live dialog
system (`dialog_frame.cppm`, `dialog_launchers.cppm` live,
`all_renderers.cppm`, `settings_dialog.cppm`, `feature_dialogs.cppm`,
`mcp_dialogs.cppm`, `cost_threshold_dialog.cppm`, quick-open/elicitation
dialogs). External name hits were all verified as local re-definitions in
the live files (`UsageSnapshot`/`ModelUsageRow`/`SubsystemHealth`/
`RenderGeneralTab` all defined locally in live `settings_dialog.cppm`;
`HandleIdleReturnEvent` in live `dialog_default_renderers.cppm`;
`RenderIdeStatusIndicator` local in live `prompt_input_footer.cppm`;
`DialogFrame` imported from live `dialog_frame.cppm`).

| Module | Class | Evidence |
|---|---|---|
| `cc.ui.dialogs.about` | DC | live About tab inside `settings_dialog.cppm`; `DialogFrame` twin |
| `cc.ui.dialogs.bridge_dialog` | DC | `BridgeStatus` homonym in live `repl_screen_layout.cpp` |
| `cc.ui.dialogs.confirmation` | DC | `ExitLoopClosure` homonym in live `dialog_launchers.cppm`; live confirmations via `prompt_dialog.cppm` |
| `cc.ui.dialogs.feedback_survey` | DC | zero refs |
| `cc.ui.dialogs.global_search_dialog` | DC | live global search: `ui/dialogs/quick_open.cppm`; `SearchResult` homonyms |
| `cc.ui.dialogs.help_view` | DC | live help: `commands/help.cppm` + settings dialog tabs |
| `cc.ui.dialogs.ide_dialogs` | DC | live IDE prompts in `dialog_default_renderers.cppm`/`all_renderers.cppm` |
| `cc.ui.dialogs.idle_return_dialog` | DC | renderer wired live is the local `HandleIdleReturnEvent` in `dialog_default_renderers.cppm` (different namespace) |
| `cc.ui.dialogs.launchers` | DC | live launchers: `ui/dialogs/dialog_launchers.cppm` (note reversed filename/module-name pairing) |
| `cc.ui.dialogs.managed_settings_security` | DC | live managed-security UI inside `settings_dialog.cppm`/`feature_dialogs.cppm` |
| `cc.ui.dialogs.output_style_picker` | DC | live picker: `ui/dialogs/feature_dialogs.cppm` (`OutputStyle` from `commands/output_style.cppm`) |
| `cc.ui.dialogs.permission_dialog` | DC | `render_permission_dialog` hit in `repl_screen_events.cpp:48` is a comment; live UI in `ui/permissions` live panels |
| `cc.ui.dialogs.permission_prompts` | DC | live prompts: `ui/permissions/permissions_components.cppm`; `PermissionResponse` homonym in `hooks/tool_permissions.cppm` |
| `cc.ui.dialogs.sandbox_dialog` | DC | `SandboxDependencyCheck`/`RenderDependenciesTab` homonyms in dead twin `sandbox_settings.cppm` |
| `cc.ui.dialogs.sandbox_settings` | DC | **1,015 LOC**, `inline_def_baseline.txt` row; live sandbox UI in `feature_dialogs.cppm`/settings dialog; sole importer-orphan `cc.utils.platform` rides with its batch |
| `cc.ui.dialogs.settings_status_page` | DC | `SubsystemHealth` local twin in live `settings_dialog.cppm` |
| `cc.ui.dialogs.settings_view` | DC | **inline-def baseline row**; live settings UI is `ui/dialogs/settings_dialog.cppm` |
| `cc.ui.dialogs.usage_dialog` | DC | `UsageSnapshot`/`ModelUsageRow` locally redefined in live `settings_dialog.cppm`; `team_details_dialog.cppm` comments explicitly say it inlines its own copy rather than import this |

### 3.13 ui — features/agents, tasks, teams (batch B17)

Four transitive companions ship in this batch (only importers are the
modules below): `task_components.cppm`, `teams_overview.cppm`,
`agent_view.cppm`, `team_status.cppm`.

| Module | Class | Evidence |
|---|---|---|
| `cc.ui.features.agents.agent_details_dialog` | DC | `ToolChipsElement` homonym in live `agent_cards.cppm`/`agent_shared_widgets.cppm` |
| `cc.ui.features.agents.agent_editor` | DC | live agent management: `agent_cards.cppm`, `agent_wizard.cppm`, app agent-menu shards |
| `cc.ui.features.agents.agent_list` | DC | `TagTabsComponent` homonym is the live widget `ui/widgets/tag_tabs.cppm` |
| `cc.ui.features.tasks.task_details_dialog` | DC | live task UI: `ui/tools/tool_ui_task.cppm` + message rows; avatar twins in orphan `task_components.cppm` |
| `cc.ui.features.tasks.task_list_ui` | DC | **inline-def baseline row**; live rows in `repl_screen_messages.cpp` |
| `cc.ui.features.tasks.task_list_view` | DC | **inline-def baseline row**; whole table/kanban port unwired |
| `cc.ui.features.tasks.task_view` | DC | live todo view in message rendering pipeline |
| `cc.ui.features.tasks.task_wizard` | DC | wizard framework live (`ui/dialogs/wizard_dialog.cppm`) but this 3-step port is unwired; defines strong vtable symbols, all unreferenced |
| `cc.ui.features.teams.swarm_collaboration_view` | DC | left side of `dead_imports_baseline.txt` rows; zero refs |
| `cc.ui.features.teams.team_details_dialog` | DC | file comments document the deliberate non-import of dead siblings (`usage_dialog`); live team UI in app team shards |

### 3.14 ui — permissions, messages, prompt, screens, widgets (batches B16 / B18)

Transitive companions in B18: `messages/shell_time_display.cppm` (only
importer dead `shell_progress_message`), `prompt/combined_highlights.cppm`
and `prompt/mode_indicator.cppm` (only importers dead `text_input_widget`
/ `prompt_input_full` — SL companions, held with the UI2 stack).

| Module | Class | Evidence |
|---|---|---|
| `cc.ui.messages.message_response` | DC | live response rendering: `ui/messages/assistant_text_message.cppm` + markdown renderers |
| `cc.ui.messages.shell_progress_message` | DC | `dead_imports_baseline.txt` row; live shell progress in message stream shards |
| `cc.ui.messages.tool_messages` | DC | dead-imports baseline; live tool rows in `ui/messages/tool_use_message*` |
| `cc.ui.messages.user_bash_input_message` | DC | dead-imports baseline (2 rows); live: `ui/messages` user-message components |
| `cc.ui.permissions.advanced_prompts` | DC | 1,254 LOC, inline-def baseline; live AskUserQuestion UI in `permissions_components.cppm` (local types) |
| `cc.ui.permissions.batch_panel` | DC | live queue UX in `screens/repl_screen_dialog_panels.cpp` |
| `cc.ui.permissions.permission_request` | DC | live request DTOs flow through `hooks/permission_resolver.cppm` + app state |
| `cc.ui.permissions.permission_rules` | DC | dead-imports baseline; live rules UI: `permission_rule_list.cppm`/`permission_scope_editor.cppm`; `tests/test_ui_dialogs.cpp` imports the distinct `permission_rules_ui` module |
| `cc.ui.permissions.permission_views` | DC | dead-imports baseline; DTO twins of live `permissions_components.cppm` |
| `cc.ui.permissions.permission_worker_badge` | DC | live badge in prompt footer / chrome panels |
| `cc.ui.permissions.sandbox_config` | DC | live sandbox config in settings/feature dialogs |
| `cc.ui.prompt.notifications` | DC | live prompt notifications rendered by `ui/prompt/prompt_input_footer.cppm` from app state |
| `cc.ui.prompt.prompt_input_full` | **SL** | 417 LOC; named "UI2" in the live `repl_screen.cppm:25` roadmap comment — parked alternative full prompt implementation; sole consumer-orphans `combined_highlights` + `mode_indicator` |
| `cc.ui.prompt.prompt_queued_commands` | DC | live queued-command display in prompt footer |
| `cc.ui.screens.log_selector` | **SL** | 1,745 LOC, inline-def baseline; faithful LogSelector port, unwired |
| `cc.ui.screens.resume_screen` | **SL** | 1,771 LOC, inline-def baseline; live resume flow is the session picker driven from app/bootstrap; parked full-screen port |
| `cc.ui.widgets.partial_completions` | DC | dead-imports baseline; 0x160 keyword table unreferenced; live completions: `hooks/typeahead.cppm` + `fuzzy_rank_nucleo.cppm` (whose comment cites this file as a sync twin) |
| `cc.ui.widgets.spinner_widget` | DC | live spinner: `ui/widgets/spinner.cppm` (re-exported via `all_components`) |
| `cc.ui.widgets.text_input_widget` | **SL** | 1,069 LOC, inline-def baseline; parked full input widget ("UI2" stack); orphan-importer of `combined_highlights`/`text_highlighting` |

### 3.15 utils (44 modules; batch B19)

All DC unless noted. Live-twin map (each verified): abort/cancellation
inline in live task/query TUs (`abort_controller`); model selection via
`utils/model/effort.cppm` + `services/api/models.cppm` (`agent_model`,
`thinking`, `tokens`, `system_prompt`, `model.*`); caching via
`utils/cache/cache_paths.cppm` (imported by `tests/test_utils.cpp` —
different module); code search via `tools/grep` live stack
(`code_indexing`); FS/git/process utils via live `utils/fs`,
`utils/git/git.cppm`, `utils/process` siblings (`cwd`, `tempfile`,
`glob_utils`, `fs_operations` — whose `FileWatcher` is yet another local
twin, `get_worktree_paths`, `editor_utils`, `exec_file`,
`file_history`); env/config via live `config/settings.cppm`
(`env_dynamic`, `env_validation`, `config_utils`); errors via live
`utils/error` (`errors_utils`); MCP helpers via live
`services/mcp/*` + `tools/mcp_tool.cppm` (`mcp_helpers`,
`mcp_transport`, `mcp_validation`); message mapping via live wire/types
modules (`message_mappers`); installer/marketplace via live
`plugins/` + `utils/plugin/*` (`native_installer`,
`plugin_marketplace_lifecycle`); PDF/media unused (`pdf`); HTTP peer
address unused (`peer_address`); plan mode lives in `commands/plan.cppm`
(`plans`); session helpers live twin `utils/session/session_storage.cppm`
(`session_restore`, `session_helpers`); settings validation via live
`utils/settings/settings_manager.cppm` (`settings_rules`); PowerShell
parsing live twin is **`cc.tools.powershell`** (registered tool;
`powershell_parser` also has an inline-def baseline row); swarm spawn
utils live in `swarm_helpers.cppm`/`swarm_backends*` (`swarm`,
`swarm_coordination`); task output live twin
`utils/tasks/...`? — the live consumers call `tools/runtime_registry_executors`
(`task_output`); theme via design tokens/theme settings (`theme`,
`system_theme`); user/uid helpers unused (`user_utils`);
`stats_utils` is 89 LOC with zero refs.

| Module | Class | Extra evidence |
|---|---|---|
| all 44 `cc.utils.*` rows in batch B19 | DC | per-twin map above; additionally `cc.utils.swarm` appears in a `tests/CMakeLists.txt` **comment** about live `cc.utils.swarm_backends` and in `inline_def_baseline.txt` (row deleted with module) |

Transitive companions landing in B19: `cc.utils.model_aliases` (only
importer is dead `agent_model`); `cc.utils.platform` (only importer is
dead `ui/dialogs/sandbox_settings.cppm` — **must ship after B15**);
`cc.utils.text_highlighting` (only importers are dead
`combined_highlights` + `text_input_widget` — **ships after B18**, or
with it).

### 3.16 vim (batch B5)

| Module | Class | Evidence |
|---|---|---|
| `cc.vim.operators` | DC | live vim ops execute through `cc.vim.vim_controller`/`vim_mode` (importers exist) |
| `cc.vim.vim_motions` | DC | same; motions applied inside live controller |

Transitive companion: `cc.vim.text_objects` — not in the 197 (its sole
importer is the dead `vim_operators.cppm`), ships in B5.

## 4. DO-NOT-DELETE / explicit holds

Nothing in the 197 is KEEP under any mechanism tested. The following are
**holds pending a human/product second look**, not liveness findings:

1. **`cc.hooks.file_watcher` (SL)** — do not batch blindly with B12:
   after deletion do a from-scratch debug + release build and confirm
   `tests/test_tools` relinks cleanly (the accidental `V`-symbol anchor
   must resolve from `libcc_std`; `loom` already proves the definition
   exists there) and run the full serial `ctest -j1`.
2. **The parked "UI2" stack (SL, B18):** `cc.ui.prompt.prompt_input_full`,
   `cc.ui.widgets.text_input_widget`, `cc.ui.screens.resume_screen`,
   `cc.ui.screens.log_selector` (+ transitive companions
   `cc.ui.prompt.combined_highlights`, `cc.ui.prompt.mode_indicator`,
   `cc.utils.text_highlighting`) are large (≈6.2k LOC) faithful ports
   explicitly referenced as future UI in live roadmap comments
   (`repl_screen.cppm:10,25`; `fuzzy_rank_nucleo.cppm:12,31`). They are
   dead code today; deleting is consistent with the recent c7/c9
   precedent ("delete unreferenced … UI island") but a product owner
   should confirm the UI2 plan is abandoned.
3. **The `cc_entrypoints` SDK surface (SL, B1, 8 files / 1,936 LOC)** —
   headers say "SDK types for external integration". The Loom
   decoupling work made this a pure harness and nothing links the
   target, but confirm no external-API/embedder commitment before
   deleting the whole target.
4. **Baselines are coupled, not blockers:** rows in
   `tools/arch/dead_imports_baseline.txt`
   (`cc.context.mailbox`, `cc.services.compact`, `cc.utils.swarm`,
   `swarm_collaboration_view`, `shell_progress_message`,
   `tool_messages`, `user_bash_input_message`, `permission_views`,
   `prompt_input_full`, `partial_completions`),
   `upward_edge_baseline.txt` (4× `config_orchestrator`), and
   `inline_def_baseline.txt` (`sandbox_settings`, `task_list_view`,
   `advanced_prompts`, `log_selector`, `resume_screen`,
   `text_input_widget`, `powershell_parser`) must be edited in the same
   commit as the module deletions (the ratchets fail on gains;
   deletions shrink them).
5. **`cc.migrations.migration_registry`/`config_orchestrator`** — do not
   remove `migration_runner`/`schema_versions`/`concrete` in the same
   sweep: they remain live via `tests/test_migrations.cpp` and the
   bootstrap path.
6. **No force-link / static-init safety case exists:** even modules with
   non-empty module initializers (`about`, `confirmation`, `help_view`,
   `settings_view`, `agent_details_dialog`, `agent_list`, `launchers`,
   `lsp.manager`, `log_selector`, `resume_screen`, `prompt_input_full`,
   `buddy_prompt`, `skills.bundled.verify`, etc.) are absent from every
   binary, so their initializers provably do not run today. This does
   NOT generalize to shared-library builds — the repo builds only static
   archives today; if a future change flips on shared libs or
   whole-archive linking, this audit must be redone.

## 5. Stale cross-references found (comment cleanup candidates)

These live-file comments name candidate modules and should be updated
when/if the modules are deleted:

- `ui/visual/code_highlight.cppm:5` — "lives in
  `cc.ui.widgets.partial_completions`"
- `ui/screens/repl_screen.cppm:25` — "prompt input ->
  `cc.ui.prompt.prompt_input_full` (UI2)"
- `ui/features/teams/team_details_dialog.cppm:11,37,267` —
  `cc.ui.dialogs.usage_dialog` "would be ideal" (uses local copy)
- `ui/prompt/fuzzy_rank_nucleo.cppm:12,31` — scoring twin of
  `cc.utils.file_index`, kept "without a hard import"
- `ui/screens/repl_screen_events.cpp:48` — commented-out
  `render_permission_dialog(...)` call

## 6. Proposed deletion batches

Sequencing principle: leaves-first, each batch independently buildable
with debug + release + serial ctest green; transitive companions are
folded into the batch that removes their sole importers. B15 must precede
the `cc.utils.platform` part of B19; B18 must precede the
`text_highlighting` part of B19. Every batch requires the same companion
checks:

- remove the exact FILE_SET row(s) from the owning
  `src/cmake/targets/cc_*.cmake` (1:1 ownership; table below);
- for whole-target batches (B1/B2/B3/B4), also remove the
  `include(...)` in `src/CMakeLists.txt` and the `target_link_libraries`
  references (`cc_core.cmake` lists all four; `cc_hooks.cmake` links
  `cc_context`; `cc_coordinator.cmake` links `cc_entrypoints`);
- shrink the matching `tools/arch/*_baseline.txt` rows;
- post-delete audit: rerun the zero-importer scan (new orphans?),
  `nm`-verify removed module initializers (`_ZGI…`) are absent from
  `loom` and every `tests/test_*` after a clean rebuild, full debug+release
  build, `ctest --preset local-linux -j1`;
- expect BMI fan-in recompiles inside the same target only; no
  cross-target importer exists to break.

| Batch | Files | LOC | Targets touched | Risk notes |
|---|---|---|---|---|
| **B1 entrypoints (whole target)** | 8 | 1,936 | delete `cc_entrypoints` | SL product decision; remove link rows in `cc_core`/`cc_coordinator` |
| **B2 buddy (whole target)** | 4 | 543 | delete `cc_buddy` | remove `cc_core` link row |
| **B3 context (whole target)** | 2 | 413 | delete `cc_context` | dead-imports baseline row; remove `cc_core`+`cc_hooks` link rows |
| **B4 schemas (whole target)** | 1 | 187 | delete `cc_schemas` | remove `cc_core` link row |
| **B5 vim** | 3 | 1,191 | `cc_vim` | includes transitive `vim_text_objects` |
| **B6 keybindings** | 4 | 902 | `cc_keybindings` | leave `defaults/validate/shortcut_format/load_user_bindings` |
| **B7 memdir/migrations/session** | 4 | 2,224 | `cc_memdir`, `cc_migrations`, `cc_session` | includes transitive `migration_registry`; 4 upward-edge baseline rows; do NOT touch `migration_runner`/`schema_versions`/`concrete` |
| **B8 cli handlers/transports** | 3 | 971 | `cc_cli` | keep live `websocket_transport`/`sse_transport`/`ccr_client` |
| **B9 config/constants/query/skill/plugin** | 8 | 1,315 | `cc_config`, `cc_constants`, `cc_query`, `cc_skills`, `cc_plugins` | live twins all inside same targets — verify each target still links |
| **B10 types leaf DTOs** | 5 | 812 | `cc_types` | keep `types.cppm`/`tool_types`/`history`/`command`/`task_types` |
| **B11 orphan task_*/brief/config/shared/testing tools** | 8 | 1,109 | `cc_tools` | string names stay (served by live `cc.tools.task` + runtime executors) |
| **B12 hooks** | 26 | 6,247 | `cc_hooks` | **contains SL `file_watcher` — land it separately at batch end with clean rebuild**; keep live `lifecycle_hooks`/`cost_hook`/`text_input`/`tool_permissions`/`permission_resolver` |
| **B13 services** | 27 | 6,012 | `cc_services` | biggest conceptual cluster; includes full tips feature (2 files) and 949-LOC dead compact island — keep `compact.api_microcompact`, live `mcp/*`, LSP, rate-limit modules |
| **B14 ui foundation/chrome** | 11 | 1,928 | `cc_ui` | primitives only; live foundation umbrella `components_figures`/`design_tokens` untouched |
| **B15 ui dialogs** | 18 | 5,506 | `cc_ui` | inline-def rows (`sandbox_settings`); frees `cc.utils.platform` for B19 |
| **B16 ui permissions** | 7 | 3,450 | `cc_ui` | inline-def row (`advanced_prompts`) |
| **B17 ui features (agents/tasks/teams)** | 14 | 9,474 | `cc_ui` | includes 4 transitive companions (`task_components`, `teams_overview`, `agent_view`, `team_status`); inline-def rows (`task_list_ui`/`task_list_view`) |
| **B18 ui messages/prompt/screens/widgets** | 15 | 7,948 | `cc_ui` | **contains 4 SL UI2 modules + 3 companions**; split the SL files to a follow-up PR if product defers; several dead-import/inline baseline rows; frees `text_highlighting` for B19 |
| **B19 utils** | 47 | 9,949 | `cc_utils` | land AFTER B15/B18 so `platform`/`text_highlighting` are already orphaned; inline-def row (`powershell_parser`); dead-import row (`swarm`) |

Exact file-by-file manifests (module → path → LOC) for every batch were
generated during triage; the union is 215 files / 62,117 LOC. Batch
numbering B1–B19 matches this table (B14–B18 are the five `cc_ui`
sub-batches).

## 7. Estimate

- **Truly dead at HEAD: 197/197** from the binary's point of view (0/197
  in `loom`; 196/197 in all 47 binaries).
- Safe-to-delete now (DC): **188/197** direct candidates, plus **11 of
  the 18 transitive companions** whose sole importers are DC (the other 7
  companions belong to the SL entrypoints/UI2 sets) → **199 files /
  ~53.8k LOC** (62,117 total less the ~8.4k LOC held under SL).
- Recommended second-look holds (SL): **9/197** direct + 7 companions
  (~8.4k LOC) — one test-link quirk (`file_watcher`), four
  whole-target SDK DTO files (+4 schema/type companions), four parked
  UI2 screens/widgets (+3 UI2-stack companions).
- Keepers: **0**. No re-export umbrella, static initializer, or
  string-keyed registration rescues any candidate.

## Appendix A — reproducibility

Scanner and audit scripts used (kept outside the repo during triage;
can be re-derived):

1. enumerate `export module X;` over `src/**/*.cppm`; count
   `(export )?import X;` over `src`, `tests`, `benchmarks`,
   `src/main.cpp` → 197.
2. per candidate: `nm -S -C <target>.dir/<rel>.cppm.o` for strong symbols
   and module-initializer size; intersect with `nm --defined-only` of
   every ELF under `build/debug`.
3. undefined-symbol union: `nm` over 35 `build/debug/src/lib*.a` +
   1,331 `**/CMakeFiles/**/*.o`; intersect with candidate strong symbol
   sets (0 hits) and with weak `V` object symbols (1 hit:
   `file_watcher`).
4. fixed-string grep of each module name + exported-symbol word greps
   with homonym resolution; CMake FILE_SET ownership map;
   `export import` enumeration; transitive fixed-point closure over the
   import graph.
