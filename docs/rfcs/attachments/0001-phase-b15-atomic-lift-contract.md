# RFC 0001 Phase B batch 15 — atomic lift compile-checked contract (landed 2026-09-27)

**Status: LANDED as commit 0b9b439 after an independent compile-checked spike in an isolated worktree and a separate adversarial review (full rebuild, serial ctest 1712/1712 debug+release, graph_check --target-core8 9-singleton PASS, nm single-anchor audit, --list-runtime-tools byte-parity). The technical contract below is the exact shape shipped; file/build-tree paths referencing the /tmp spike worktree are historical. Post-B15 follow-ups: prune the three installed-but-unread seam accessors (missing-tool backend + the two MCP DTO providers — roots call make_missing_tool_backend/collect_* directly), delete the zero-importer cc.tools.list_mcp_resources_tool, and prune the four carried textual-dead services.api.bootstrap imports in the lifted agent subtree (each is a legal 9→7 down-edge today).**

---


## 1. Final `cc.tools.runtime_backends.port`

**File:** `src/tools/runtime_backends_port.cppm` (interface) + NEW `src/tools/runtime_backends_port.cpp` (impl anchor). Both in cc_tools (`tools/runtime_backends_port.cpp` added to the PRIVATE block).

Key design decision (spike-validated): **the MCP snapshot sink (`set_mcp_snapshots_sink`) does NOT move into this port.** Its signature names `cc::services::mcp::McpServerSnapshot`; naming a services type in the rank-8 port would recreate a tools→services area edge (final target graph has zero). The sink stays with the lifted `cc.orchestration.tools.mcp` (9→7 down is legal). The snapshot-derived *providers* (plain `std::function<std::vector<ToolDefinition>()>` etc.) DO live on the port because they name only DTO types.

Exact exported content (`export namespace cc::tools`):

```cpp
using SkillLoaderExecutor = std::function<
    std::optional<cc::core::Result<cc::core::ToolResult>>(const cc::core::ToolInput&)>;
// matches the live RuntimeExecutor alias in runtime_registry.cppm:60
using RuntimeToolExecutor =
    std::function<cc::core::Result<cc::core::ToolResult>(const cc::core::ToolInput&)>;
using MissingToolBackend =
    std::function<cc::core::Result<cc::core::ToolResult>(
        std::string_view tool_name, const cc::core::ToolInput& input)>;
using McpToolDefinitionsProvider = std::function<std::vector<cc::core::ToolDefinition>()>;
using McpInputSchemasProvider = std::function<std::unordered_map<std::string,std::string>()>;
// exact make_agent_tool 5-arg shape (agent_tool.cppm:1362)
using AgentToolFactory = std::function<std::unique_ptr<cc::core::ITool>(
    AgentConfig, int depth, cc::core::ToolRegistry* registry,
    AgentLivePermissionCheckFn permission_check, bool permission_hook_valid_for_background)>;
```

Interface imports (REQUIRED for complete types in the std::function signatures): `std`, `cc.types.types`, `cc.types.tool_types`, `cc.tools.tool` (ITool/ToolDefinition), `cc.tools.agent_types` (AgentConfig/AgentLivePermissionCheckFn).

Per slot, a trio `set_X / clear_X / X& accessor` for: `skill_loader`, `lsp`, `mcp`, `list_mcp_resources`, `read_mcp_resource`, `mcp_auth`, `computer_use` (ONE slot covering both `'computer_use'` and `'computer'`), `missing_tool`, `mcp_tool_definitions`, `mcp_input_schemas`, `agent_tool_factory`.

Storage (impl unit, ratchet-safe; verified single anchor with nm): one function-local
```cpp
namespace cc::tools::detail {
struct RuntimeBackendSlots {
    std::optional<SkillLoaderExecutor> skill_loader;
    RuntimeToolExecutor lsp, mcp, list_mcp_resources, read_mcp_resource, mcp_auth, computer_use;
    MissingToolBackend missing_tool;
    McpToolDefinitionsProvider mcp_tool_definitions;
    McpInputSchemasProvider mcp_input_schemas;
    AgentToolFactory agent_tool_factory;
};
RuntimeBackendSlots& runtime_backend_slots() { static RuntimeBackendSlots slots; return slots; }
}
```
nm on `libcc_tools.a`: one strong `T cc::tools::detail::runtime_backend_slots...`, one `b slots`, one guard var — all in `runtime_backends_port.cpp.o` only.

**graph_check caveat (must carry):** the impl unit's 4 cc-imports are all flagged as NEW dead imports by the textual heuristic (the slot bodies use the types unqualified inside `namespace cc::tools`). Each needs a `// arch-check: keep-import` marker — done in the spike.

**Redundancy finding for the implementer:** `missing_tool` slot is installed but never *read* — main/server composition roots call `cc::orchestration::make_missing_tool_backend()` directly (register must NOT auto-attach it: that would iterate/spawn MCP connections for every missing tool in hermetic tests). Recommend either dropping the 3 missing-tool functions from the port or accepting it as the documented seam; the spike kept the shape the brief requested.

The old B12 `skill_loader_executor_override` + setter/clearer declarations in `runtime_registry.cppm:312/337-339` and their defs in `runtime_registry_skills.cpp:178-194` are DELETED; dispatch reads `skill_loader_executor_slot()` from the port.

## 2. `register_runtime_tools` before/after

### 2a. runtime_registry.cppm
- DELETE import `cc.tools.lsp` (was :23). Keep `cc.tools.computer_use` (:22) — the inline override vars still name its provider types.
- DELETE LSP decls (was :154-158): `parse_lsp_action`, `format_lsp_result`, `execute_lsp_tool`.
- DELETE computer-use block (was :281-320): `parse_computer_action`, `run_computer_use_command_backend`, `computer_json_optional_string`, struct `ComputerUseCommandBackendResult`, `parse_computer_command_result`, the two `computer_use_command_*_provider` decls, and the `skill_loader_executor_override()` decl (was :312).
- KEEP (in `namespace cc::tools::detail`, same place): the two `inline std::optional<...Provider> computer_use_{capture,input}_provider_override;` vars.
- The FOUR test setters move OUT of `namespace detail` and become **inline strong-in-cc_tools definitions** at namespace `cc::tools` scope (spike-verified: compile + link clean; in `libcc_tools.a` they are `V` weak COMDAT on `runtime_registry.cppm.o`, merged once at final link, absent from loom since only tests reference them):
```cpp
inline void set_runtime_computer_use_capture_provider_for_testing(...) { detail::computer_use_capture_provider_override = std::move(provider); }
inline void clear_runtime_computer_use_capture_provider_for_testing() { detail::computer_use_capture_provider_override.reset(); }
inline void set_runtime_computer_use_input_provider_for_testing(...) { ... }
inline void clear_runtime_computer_use_input_provider_for_testing() { ... }
```
- DELETE `normalize_name_for_mcp` / `connected_computer_use_mcp_server` / `execute_computer_use` detail decls (was :343-348).
- DELETE `collect_mcp_tool_definitions` / `collect_mcp_input_schemas` (was :434-437) — re-exported from orchestration in `namespace cc::tools` (call sites unchanged).
- `RuntimeToolOptions` gains the LAST member with a default member initializer (mandatory — without it `-Werror,-Wmissing-designated-field-initializers` fires at ~80 `RuntimeToolOptions{...}` sites, incl. team_create/team_delete):
```cpp
AgentToolFactory agent_tool_factory = {};
```

### 2b. register.cpp (agent block, BEFORE the move)
Before (register.cpp:111-123): `auto permission_check = std::move(options.permission_check);` then `make_agent_tool(...)` using the moved checker.
After:
```cpp
const auto& slot_factory = agent_tool_factory();                 // port accessor
const auto& agent_factory = options.agent_tool_factory ? options.agent_tool_factory : slot_factory;
AgentConfig agent_config;
agent_config.parent_permission_mode = std::move(options.parent_permission_mode);
if (agent_factory) {
    registry.register_tool(agent_factory(std::move(agent_config), 0, &registry,
        options.permission_check,                                     // COPY, pre-move
        options.permission_hook_valid_for_background));
}
auto permission_check = std::move(options.permission_check);     // simple() lambda keeps using it
```
Imports: delete `cc.orchestration.agent` (sed-renamed from cc.tools.agent) and `cc.orchestration.tools.mcp`; add `cc.tools.agent_types` (AgentConfig) and `cc.tools.runtime_backends.port`. DELETE the two `collect_mcp_*` bodies (end of file, was :519-559) — moved verbatim to the mcp impl TU. When no factory is installed (unit tests that construct registries without the pre-main install TU) the Agent tool is simply absent — pre-existing fail-closed behavior for 'Agent'.

### 2c. dispatch.cpp — six seam lookups
`computer_use||computer`, `lsp`, `list_mcp_resources`, `read_mcp_resource`, `mcp`, `mcp_auth` each become:
```cpp
if (name == "lsp") {
    if (auto& backend = lsp_backend(); backend) return backend(input);
    return ToolResult::error(std::format("Runtime tool '{}' has no runtime handler", name));
}
```
**Null-sink literal verified live at dispatch.cpp:368 (master):** `` "Runtime tool '{}' has no runtime handler" `` — byte-identical in every branch. Skill branch: `skill_loader_executor_override()` → `skill_loader_executor_slot()`. Drop `cc.orchestration.tools.mcp` import; add `cc.tools.runtime_backends.port`.

### 2d. executors.cpp
DELETE the LSP trio (was :52-114): `parse_lsp_action` (11-string mirror), `format_lsp_result`, `execute_lsp_tool`; drop the lsp import. Everything else stays.

## 3. Orchestration side (compiles)

`src/orchestration/runtime_backends.cppm` — module `cc.orchestration.runtime_backends`, structure:
- imports: std, cc.services.image, image_codec.port, runtime_backends.port, types, tool_types, cc.tools.tool, cc.tools.runtime_registry, cc.skills.skill, cc.tools.agent_runtime, **cc.orchestration.tools.{lsp,mcp}, cc.orchestration.agent, cc.orchestration.mcp_connectivity**.
- NON-exported inter-TU decls in `namespace cc::orchestration::detail`: `lsp_backend / mcp_backend / list_mcp_resources_backend / read_mcp_resource_backend / mcp_auth_backend` (each `Result<ToolResult>(const ToolInput&)`), plus a `namespace cc::tools::detail { execute_computer_use }` decl (moved TU keeps ns `cc::tools`).
- `export namespace cc::tools { collect_mcp_tool_definitions / collect_mcp_input_schemas decls }`.
- `export namespace cc::orchestration`: existing `make_image_codec()`, `make_skill_loader_executor()`, NEW `make_missing_tool_backend()` (inline; byte-exact union of the 3 pre-B15 lambdas — see surprises), and `install_runtime_backends()` whose `std::call_once` binds: image codec, skill executor, six backends (`set_computer_use_backend` wraps `execute_computer_use`), missing-tool, both providers (function pointers to the exported cc::tools collectors), agent factory (lambda forwarding to `cc::tools::make_agent_tool` 5-arg, checker passed per call), and `cc::orchestration::mcp_connectivity::wire_mcp_connectivity()` (bridge folded in).

New impl units (all `module cc.orchestration.runtime_backends;`, PRIVATE in cc_orchestration.cmake):
- `runtime_backends_lsp.cpp` — verbatim 11-string `parse_lsp_action` mirror (diagnostics/definition/references/completion/hover/symbols/implementation/workspaceSymbol/prepareCallHierarchy/incomingCalls/outgoingCalls, fallback Symbols), `format_lsp_result`, `lsp_backend`. Imports: std, types.tool_types, orch.tools.lsp, cc.tools.runtime_registry (json_string/json_int, `cc::tools::format_error`). Everything qualified `cc::tools::` (lives in `cc::orchestration::detail`).
- `runtime_backends_mcp.cpp` — `namespace cc::orchestration::detail` four backends (namespaced, each own `input.json()`), CLOSED with `} // namespace cc::orchestration::detail`, then `namespace cc::tools { collect_* }` (exported via the cppm decls). Imports: std, types.tool_types, orch.tools.mcp, cc.tools.runtime_registry.
- `runtime_backends_computer_use.cpp` — MOVED TU verbatim. Module renamed; ADDS `import cc.tools.runtime_registry` (override vars + json helpers); mcp import renamed; keeps image_codec.port + cc.services.image (transitively). The four setter bodies DELETED (now inline in tools). `ComputerUseCommandBackendResult` struct + `computer_json_optional_string` become TU-local (they were registry-interface decls used only by this TU — do not re-declare twice: the spike hit a redefinition doing both).
- (no separate `runtime_backends_install.cpp` — the call_once body is inline in the cppm; an impl TU would add a file for no gain. This is a spike deviation from the plan text, compile-justified.)

`src/orchestration/mcp_connectivity.cppm` — moved; only changes: module→`cc.orchestration.mcp_connectivity`, namespace `cc::bootstrap::mcp_connectivity`→`cc::orchestration::mcp_connectivity` (to_hook_status/project_connectivity/wire_mcp_connectivity names unchanged). Imports unchanged incl. the no-leading-`::` rule.

## 4. File-move manifest + CMake

git mv (19), module names changed in each moved file:

| source | dest | old module → new module |
|---|---|---|
| src/tools/agent_tool.cppm | src/orchestration/agent/agent_tool.cppm | cc.tools.agent → cc.orchestration.agent |
| src/tools/agent_run.cppm | src/orchestration/agent/agent_run.cppm | cc.tools.agent.run → cc.orchestration.agent.run |
| src/tools/agent_resume.cppm | src/orchestration/agent/agent_resume.cppm | …agent.resume |
| src/tools/agent_fork.cppm | src/orchestration/agent/agent_fork.cppm | …agent.fork |
| src/tools/agent_sub_utils.cppm | src/orchestration/agent/agent_sub_utils.cppm | cc.tools.agent.utils → cc.orchestration.agent.utils |
| src/tools/agent_sub_utils_{budget,config,hooks,json,messages,teammates,tools_mcp}.cpp (7) | src/orchestration/agent/ (same basenames) | module cc.orchestration.agent.utils |
| src/tools/spawn_multi_agent.cppm | src/orchestration/agent/spawn_multi_agent.cppm | cc.tools.spawn_multi_agent → cc.orchestration.agent.spawn_multi_agent |
| src/tools/mcp_tool.cppm | src/orchestration/tools/mcp_tool.cppm | cc.tools.mcp → cc.orchestration.tools.mcp |
| src/tools/mcp_core_settings_loader.cpp | src/orchestration/tools/ | module cc.orchestration.tools.mcp |
| src/tools/mcp_snapshots_sink.cpp | src/orchestration/tools/ | module cc.orchestration.tools.mcp |
| src/tools/lsp_tool.cppm | src/orchestration/tools/lsp_tool.cppm | cc.tools.lsp → cc.orchestration.tools.lsp |
| src/tools/runtime_registry_computer_use.cpp | src/orchestration/runtime_backends_computer_use.cpp | cc.tools.runtime_registry → cc.orchestration.runtime_backends |
| src/bootstrap/mcp_connectivity.cppm | src/orchestration/mcp_connectivity.cppm | cc.bootstrap.mcp_connectivity → cc.orchestration.mcp_connectivity |

New (not moves): `src/tools/runtime_backends_port.cpp`, `src/orchestration/runtime_backends_lsp.cpp`, `src/orchestration/runtime_backends_mcp.cpp`, `tests/test_runtime_backends_install.cpp`.

CMake edits:
- **cc_tools.cmake**: remove the 5 agent FILE_SET rows + lsp/mcp FILE_SET rows + spawn row; remove the 7 agent_sub_utils PRIVATE rows, computer_use PRIVATE row, 2 mcp PRIVATE rows; ADD `tools/runtime_backends_port.cpp` PRIVATE. Final links: `PUBLIC cc_utils cc_types cc_skills_core yyjson uv_a` (drop cc_config + cc_services — grep-verified zero remaining cc.tools.* imports of services/config/hooks; only 3 `cc.skills.file_access.port` edges remain).
- **cc_orchestration.cmake**: rewritten — 10 FILE_SET rows (runtime_backends, mcp_connectivity, 6 agent cppm, mcp, lsp) and 12 PRIVATE rows (7 agent.utils impls, 2 mcp slot impls, 3 runtime_backends impls); links `PUBLIC cc_tools cc_services cc_skills_core cc_hooks cc_config cc_utils cc_types`.
- **cc_skills.cmake**: remove `cc_tools`; **ADD `cc_config`** (skills/bundled/* import cc.config.config — reached before only transitively through cc_tools; first real compile failure of the spike, replay adjacency `skills->{config,utils}` predicted this).
- **cc_bootstrap.cmake**: drop mcp_connectivity FILE_SET row and `cc_tools` link.
- **cc_commands.cmake**: add `cc_orchestration` PUBLIC (mcp_cmd, mcp.core_settings_loader, color).
- **cc_ui.cmake**: add `cc_orchestration` PUBLIC (2 prompt impl TUs).
- **cc_server.cmake**: already PUBLIC cc_orchestration from B11 — no change.
- **cc_core.cmake**: add `cc_orchestration` to the INTERFACE list (test_fix_lsp_tool links cc_core only).
- **tests/CMakeLists.txt**: test_tools sources `test_tools.cpp test_runtime_backends_install.cpp` (links already carry cc_orchestration from B11); test_fix_notifs swap `cc_bootstrap`→`cc_orchestration`. No new test target.

## 5. Importer rewrites + install points

Mechanical rewrites (script: exact-token, longest-first; WILDCARD LESSON BELOW):
- `cc.tools.agent.utils/.run/.resume/.fork` → `cc.orchestration.agent.*`; `cc.tools.agent` (boundary) → `cc.orchestration.agent`; `cc.tools.mcp`/`.lsp` → `cc.orchestration.tools.*`; `cc.tools.spawn_multi_agent` → `cc.orchestration.agent.spawn_multi_agent`; `cc.bootstrap.mcp_connectivity` → `cc.orchestration.mcp_connectivity`.
- External files: src/main.cpp; src/server/server_routes.cppm; src/commands/{mcp_cmd.cppm, color.cppm, mcp/core_settings_loader.cppm}; src/ui/prompt/{at_attachments_impl.cpp, autocomplete_sources_impl.cpp}; tests/{test_tools.cpp, test_tasks.cpp, test_fix_lsp_tool.cpp, test_fix_notifs.cpp}. Moved files' intra-subtree imports renamed automatically. `cc.tools.agent_runtime`, `agent_color_manager`, `agent_types`, `agent_worktree`, `mcp_classify`, `list_mcp_resources_tool` were deliberately NOT renamed (stay in tools).
- The 4 carried textual-dead `import cc.services.api.bootstrap;` (agent_tool/run/resume/fork) carried verbatim — graph_check stays green with them (not baseline rows; attributed by the parser).
- main.cpp: DELETE the `cc.orchestration.mcp_connectivity` import (folded); DELETE the standalone `cc.orchestration.tools.mcp` import (no direct MCP symbols left in main after lambda removal); the old B7 `wire_mcp_connectivity()` call at ~1813 is REPLACED by `cc::orchestration::install_runtime_backends()` at **main.cpp:1799** — immediately after `install_core_settings_mcp_loader()`:1790, dominating list_runtime_tools (:1832), run_runtime_tool_once (:1849), and both register sites (:665 executed via :1849, :1918). The redundant B11 install call before the list block deleted.
- main.cpp: BOTH missing-tool lambdas (:671 run_runtime_tool_once; the big interactive one ~1947) replaced by `cc::orchestration::make_missing_tool_backend()` (note: missing semicolon after the replacement was a compile fix at the second site).
- server_routes.cppm: install stays at :776 (per-session, call_once-noop after main in the loom binary — the pre-accept-loop move the plan suggested is unnecessary given call_once; standalone embedders install on first session); lambda at :798 replaced by the factory; mcp import dropped.
- **The 7 agent-test factory bind sites (test_tools.cpp:4645/4757/4926/8716/8847/8879/8969 in the plan): ZERO body edits needed.** New `tests/test_runtime_backends_install.cpp` is a module TU linked into test_tools with a namespace-scope global whose ctor calls `install_runtime_backends()` pre-main. It binds the agent factory (plus all six backends, missing-tool, providers, connectivity) process-wide; 1712/1712 including the team/background-resume 'Agent' dispatch paths prove it. The plan's "zero body edits was false" prediction is itself falsified by the spike. The pre-existing `FileToolServicesGuard` still re-installs codec/skill and clears in dtor.

## 6. Baseline + CI edits

- `tools/arch/dead_imports_baseline.txt`: **44** `cc.tools.agent{,.fork,.resume,.run}` rows → `cc.orchestration.agent*`; the agent.fork target row `-> cc.tools.mcp` also becomes `-> cc.orchestration.tools.mcp`; rename ONLY the at_attachments row (now :140) `-> cc.orchestration.tools.mcp`; LEAVE the very next row (autocomplete_sources `-> cc.skills.bundled`) untouched. Total changed lines: 45.
- `tools/arch/inline_def_baseline.txt`: `cc.tools.agent 19` → `cc.orchestration.agent 19`; `cc.tools.agent.utils 6 c1 c2-done` → `cc.orchestration.agent.utils 6 c1 c2-done` (flags preserved). PLUS a spike-required re-freeze: **`cc.tools.runtime_registry 5 c1 c2-done` → `9 c1 c2-done`** (the 4 inlined computer-use test setters; the plan didn't predict this). Re-freeze is justified (strong symbols deliberately kept in cc_tools); reviewer should record it.
- `.github/workflows/arch-check.yml`: **no edit** — `--target-core8` was already flipped on at B5 (workflow :34); the plan's "flip after line 28" is stale.
- The gate reports "3 removed" dead rows and "0 removed" upward edges on BOTH master and the spike (pre-existing ui repl_screen drift) — not caused by B15.

## 7. Surprises / compile errors hit (most valuable section)

1. **Regex `.` matched `_`** in the importer-rewrite script: patterns written as plain strings (`cc.tools.agent.run`) were compiled as regex and corrupted every `cc.tools.agent_runtime` import into `cc.orchestration.agent.runtime` (20+ files incl. agent_runtime.cppm itself). Fixed by reverting `cc.orchestration.agent.runtime`→`cc.tools.agent_runtime` tree-wide and using boundary lookarounds. Lesson: reuse the exact ordered mapping with word-boundary assertions; `agent_runtime`/`agent_color_manager`/`agent_types`/`agent_worktree` must NOT move.
2. **cc_skills needs cc_config after dropping cc_tools** (`module 'cc.config.config' not found` in skills/bundled/update_config.cppm) — transitive-only dependency exposed; replay's final adjacency already shows skills→config.
3. **Reopened `namespace cc::tools::detail` at file scope is an error** — after the exported-cc::tools setter block I wrote `namespace cc::tools::detail {` outside the original `export namespace cc::tools`, producing `cc::tools::cc::core` "no member named 'core'" cascades. Fix: reopen plain `namespace detail {` while still inside the exported block.
4. **4th RuntimeToolOptions member needs `= {}`** or `-Wmissing-designated-field-initializers` fails the build at the ~80 designated-init sites (first seen in team_create:179 / team_delete:173).
5. **moved computer_use TU redefinition** when `computer_json_optional_string` was both left in place and re-added; and `ComputerUseCommandBackendResult` had to move INTO the TU (the registry-interface decls were deleted per design).
6. **mcp TU nested-namespace bug**: inserting `namespace cc::tools {…collectors…}` without first closing `cc::orchestration::detail` made `cc::core` resolve as `cc::orchestration::detail::cc::core`. Explicit closing brace required.
7. **Missing semicolon** after replacing the second main.cpp missing-tool lambda.
8. **graph_check flags all 4 impl-unit imports of the new anchor TU** as NEW dead imports — keep-import markers required (unqualified names inside `cc::tools`).
9. **inline-def ratchet 5→9** for runtime_registry (4 setters) — baseline re-freeze required; not in the plan.
10. **Plan inaccuracies found by building:** (a) the 7 per-test factory binds are unnecessary — one pre-main installer TU covers them; (b) CI target-core8 flip already happened at B5; (c) `native_agent_store` does NOT land in orchestration — `cc.tools.agent_runtime` (and its 6 store impl TUs) stay in cc_tools; nm shows the singleton's one strong `T` in `libcc_tools.a/agent_runtime_store_impl.cpp.o`, zero copies in orchestration; (d) the three pre-B15 missing-tool lambdas were NOT textually identical (first/server guarded last_error against nothing; the interactive one had dead ToolNotFound/ServerNotFound guard branches) — spike unifies on unconditional `last_error = format_error(err)` (matches 2 of 3 sites, incl. the server); full ctest is green.
11. **No separate install TU needed** — call_once body inline in runtime_backends.cppm compiles and keeps the guard symbol in the interface TU; the planned runtime_backends_install.cpp adds nothing.
12. `list_mcp_resources_tool.cppm` kept in cc_tools per plan; it is genuinely unused (the lifted backend uses mcp_tool.cppm's own ListMcpResourcesTool) — confirm follow-up prune.

## 8. Irreducibility

The diff is irreducible as ONE commit for the *graph cut*, but the spike found **one independently-green slice** and two cheap mechanical preparatory slices:
- **Independently green:** the port's *additive extension* (six slots + mcp providers + agent factory + missing-tool + anchor impl unit, all unused) + the final `RuntimeToolOptions.agent_tool_factory = {}` member can land first against the old code — pure additions, gate green. (The B12 skill slot must simultaneously migrate from runtime_registry to the port or storage diverges, so fold that migration into this slice.)
- **Mechanical-only, green independently:** the CMake upper-layer additions (cc_skills +cc_config; cc_commands/cc_ui/cc_core +cc_orchestration) are safe in advance — unused forward target links.
- The actual LIFTS remain atomic: every module rename must coincide with (a) the file moves, (b) registry body deletion + seam lookups, (c) the collector/setter re-homing, (d) CMake row moves and link trims, (e) baseline renames — any intermediate state has a tools↔orchestration 2-node SCC or new 8→9 non-contract edges (matches the replay simulation).
Recommendation: at most pre-land the two prep slices; keep the lift one commit.

## 9. Verification results

- `cmake --build --preset spike-linux -j8`: compile + link all targets incl. tests, `-Werror`, **green**.
- `python3 tools/arch/graph_check.py --target-core8`: modules 846 / 969 units; module cycles 0; illegal-up 10 (pre-existing config/migrations backlog, untouched); dead imports 139 baseline backlog (same 3 pre-existing removals master shows, zero NEW); **9 singleton areas PASS, SCCs none**. Live 9-area adjacency, exactly matching the replay prediction:
  `config->{utils}; hooks->{utils}; services->{config,utils}; skills->{config,utils}; state->{task_types,utils}; task_types->{utils}; tools->{skills,utils}; utils->{}; orchestration->{config,hooks,services,skills,tools,utils}`; **zero tools→orchestration module edges** (script-verified).
- `inline_def_check.py`: OK after the documented runtime_registry 5→9 re-freeze.
- `ctest --preset spike-linux -j1`: **100% — 1712/1712 passed, 0 failed (97.5s)**; skipped/disabled are pre-existing platform gates. (100% of the suite run.)
- nm: one strong `runtime_backend_slots()` + one `slots` bss + one guard in cc_tools' runtime_backends_port.cpp.o; 4 computer-use setters inline-V on runtime_registry.cppm.o (merged once, GC'd from loom); native_agent_store singleton exactly one strong `T` (in cc_tools/agent_runtime_store_impl — see surprise 10c).
- `loom --list-runtime-tools` (clean HOME) spike vs master binary: **identical 44-name sorted output**.

## 10. Checkpoints
```
633be3b spike: B15 atomic lifts + unified runtime_backends seam (builds green)
cbacaa3 (base) docs(rfc): record Phase B batch 13 on macos-14 (1712/1712)
```
(Machine-local `.deps-cache` symlink and `CMakeUserPresets.json` are gitignored / excluded from the commit.)
