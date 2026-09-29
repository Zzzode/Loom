# RFC 0001 Phase D — cross-target migration and module-name rename plan

Attached 2026-09-29. Plans the work explicitly deferred when Phase D landed
(2026-09-26, commit `88b4aae`): the 115-file re-home kept every module NAME as
`cc.utils.*`, and the section-12 entry records that "cross-library/target and
module-NAME migration (renaming cc.utils.* to cc.fs.* etc., moving
statusline/theme to cc_ui) is a separate rename-heavy change and remains out of
scope." This attachment is the plan for that change.

Inputs read for this plan:

- RFC 0001 §4.4 (`docs/rfcs/0001-module-architecture-target.md:271-292`) and
  the §12 Phase D entries (`:535` onward, rows dated 2026-09-26).
- The OQ-4 domain mapping (`0001-oq4-baselines-and-utils-mapping.md`).
- `src/utils/` current layout, `src/cmake/targets/cc_utils.cmake`, and every
  destination target's cmake file.
- The lint stack under `tools/arch/` (`graph_check.py`, `inline_def_check.py`,
  the three baseline files).
- CLAUDE.md "A hazard specific to this codebase" (string/shape-based couplings
  break silently).

Verification gate (directive 2026-09-29): the LOCAL `local-linux` /
`local-linux-release` dual preset plus serial `ctest -j1`. GitHub CI is not a
gate and must not be required.

## 0. Current state (what Phase D left behind)

- **124 module primaries** live in `cc_utils`, all named `cc.utils.*`
  (`src/cmake/targets/cc_utils.cmake:5-128` lists 124 FILE_SET entries; 124
  `export module` declarations under `src/utils/`, one per file). They sit in
  ~30 domain directories but the name/path decoupling means the lint still sees
  one flat `cc.utils` area.
- **17 module implementation units** (`module cc.utils.X;` `.cpp` files,
  `cc_utils.cmake:130-149`): 7 for `json`, 8 for `swarm_backends`, 1 each for
  `skill_usage` and `swarm_helpers`.
- **47 of the 171 original modules were deleted after the OQ-4 mapping**: 46 as
  unreferenced duplicates on 2026-09-28 (commit `56bf7e8`, "RFC-0001 B
  followup c10-b19", 9695 deletions) plus `cc.utils.tool_management` on
  2026-09-27 (commit `f1cae09`, orphaned by Phase B). 171 − 47 = 124, the live
  count. The OQ-4 mapping's 171-row destination table is therefore stale: the
  live target set is the 124 above. Notable deletions: `theme`, `system_theme`,
  `pdf` (the three REVIEW-resolved media/theme modules — their OQ-4
  destinations are now moot), `cwd`, `glob_utils`, `tempfile`, `config_utils`,
  `errors_utils`, `settings_rules`, `cache`, `mcp_helpers`/`mcp_validation`/
  `mcp_transport`, `message_mappers`, 8 of the 13 `model.*` modules,
  `powershell_parser`, `native_installer`, `user_utils`, `abort_controller`,
  `exec_file`, `editor_utils`, `code_indexing`, `session_helpers`/
  `session_restore`, `task_output`/`plans`, `stats_utils`, `swarm`/
  `swarm_coordination`, `agent_model`, `thinking`, `tokens`, `platform`,
  `env_dynamic`/`env_validation`, `peer_address`, `file_history`, `fs_operations`,
  `file_index`, `get_worktree_paths`, `plugin_marketplace_lifecycle`.
- **`ide_integration` is already done**: moved to `src/services/` in Phase D
  and renamed `cc.utils.ide_integration` → `cc.services.ide_integration` in
  Phase B b3 (commit `906e888`). It is the one completed precedent for the
  rename track and proves the recipe.
- **Blast radius**: 252 `src/` files, 29 `tests/` files, and 6
  `src/benchmarks/pare/` files contain at least one `import cc.utils.*`.
  `tests/test_utils.cpp` alone imports 53 `cc.utils.*` modules (a link/smoke
  TU) and must be edited in every batch.
- **No `"cc.utils..."` string literals exist in `src/`** (verified
  2026-09-29): there is no reflection-by-name on module names. The
  string-coupling surface is the lint baselines and CMake, plus comment
  occurrences of `cc.utils.*` names (see §3.11) — not runtime string
  literals.
- **No `__FILE__` in `src/utils/`** (verified): path moves are behavior-safe.

## 1. Inventory — where the 124 modules live, and which are misplaced

All 124 are in `cc_utils` today. "Misplaced" below means: the module's domain
already has a real target elsewhere that should own it (or, for teams, needs
one created). The rename-only domains are correctly placed in the generic
utility target and only need their NAME changed.

| Current dir under `src/utils/` | Modules (live) | Count | OQ-4 destination area | Action |
|---|---|---:|---|---|
| `hooks/` | hooks_config, hooks_execution, hooks_registry | 3 | `cc.hooks` | **MOVE → `cc_hooks`** (target exists, 14 `cc.hooks.*` modules) |
| `plugin/` | plugin_dependency_resolver, plugin_identifier, plugin_lifecycle, plugin_loader, plugin_manager, plugin_marketplace, plugin_marketplace_rules, plugin_validation, plugin_versioning | 9 | `cc.plugins` | **MOVE → `cc_plugins`** (target exists) |
| `session/` | list_sessions, session_storage | 2 | `cc.session` | **MOVE → `cc_session`** (target exists) |
| `skills/` | loom_code_hints (file `loom_hints.cppm`), skill_usage (+impl) | 2 | `cc.skills` | **MOVE → `cc_skills`** (target exists) |
| `tasks/` | task_utils | 1 | `cc.tasks` | **MOVE → `cc_tasks`** (target exists) |
| `settings/` | settings_manager, settings_merge, settings_paths, settings_sources, settings_validation | 5 | `cc.config` | **MOVE → `cc_config`** (target exists) |
| `statusline/` | statusline_runner | 1 | `cc.ui.statusline` | **MOVE → `cc_ui`** |
| `messages/` | collapse_notifications, collapse_read_search, message_predicates | 3 | `cc.messages.support` / `cc.ui.messages` | **MOVE → `cc_ui`** (test-only importers; see D4) |
| `tools/` | tool_helpers, script_tool_enabled | 2 | `cc.tools.support` | **MOVE → `cc_tools`** (target exists) |
| `types/` | tagged_id, content_array | 2 | `cc.types` / `cc.types.wire` | **MOVE → `cc_types`** (pure leaves, rank 0) |
| `teams/` | agent_swarms_enabled, control_message_compat, team_helpers | 3 | `cc.teams` | **MOVE → NEW `cc_teams` target** |
| `swarm/` | swarm_backends (+8 impl), swarm_helpers (+impl), swarm_pane_observer | 3 | `cc.teams.swarm` | **MOVE → NEW `cc_teams` target** |
| `model/` | effort, model_cost, model.model, providers, token_budget | 5 | `cc.model` | rename only, stay in `cc_utils` (no `cc_model` target; see D7) |
| `security/` | auto_mode_denials, permissions, permissions_engine, privacy_level, query_guard, sanitization, tool_deny_rules | 7 | `cc.security` / `.permissions` / `.sanitize` | rename only, stay in `cc_utils` (see D7) |
| `fs/` | atomic_replace, file, file_persistence, file_read_cache, lockfile, memory_file_detection, path, path_utils, read_file_in_range | 9 | `cc.fs` | rename only |
| `fs/` (`file_edit_utils.cppm`) | file_edit | 1 | `cc.fs.edit` | rename only |
| `text/` | diff_utils, format, markdown_utils, parse_int, parse_references, semantic_boolean, semantic_number, string, string_utils, words | 10 | `cc.text` | rename only |
| `serdes/` | frontmatter_parser, json (+7 impl), yaml | 3 | `cc.serdes` | rename only |
| `platform/` | binary_check, clipboard, find_executable, hyperlink, platform_paths, terminal_helpers, xdg | 7 | `cc.platform` | rename only |
| `env/` | env, env_utils | 2 | `cc.platform.env` | rename only |
| `http/` | github_utils, http, http_encoding, proxy_utils, ssrf_guard | 5 | `cc.net.http` | rename only |
| `process/` | async, exec_sync, process, timeouts | 4 | `cc.process` | rename only |
| `bash/` | bash_execution, bash_security, bash_shell_quoting | 3 | `cc.process.bash` | rename only |
| `shell/` | shell, shell_parser, shell_providers, shell_rule_matching | 4 | `cc.process.shell` | rename only |
| `git/` | commit_attribution, detect_repository, git, git_diff, git_filesystem, gitignore | 6 | `cc.scm.git` | rename only |
| `containers/` | array_utils, circular_buffer, object_group_by, set_utils | 4 | `cc.containers` | rename only |
| `crypto/` | crypto, hash, uuid_utils | 3 | `cc.crypto` | rename only |
| `diagnostics/` | activity_manager, debug, debug_filter, fps_tracker, log | 5 | `cc.diagnostics` | rename only |
| `parsing/` | argument_substitution, slash_command_parsing, text_highlighting | 3 | `cc.parsing.cli` / `.highlight` | rename only |
| `tree_sitter/` | tree_sitter.base, tree_sitter.bash | 2 | `cc.parsing.tree_sitter` | rename only |
| `agent/` | agent_id | 1 | `cc.agent` | rename only |
| `cache/` | cache_paths | 1 | `cc.cache` | rename only |
| `media/` | image_store | 1 | `cc.media` | rename only |
| `prompt/` | prompt_category | 1 | `cc.prompt.support` | rename only (deletion disproven by `test_utils.cpp` assertions) |
| `error/` | error | 1 | `cc.error` | **KEEP `cc.utils.error`** (OQ-4 says "stays"; see D1) |

Misplaced total: 36 modules → 10 existing targets + 1 new target. Rename-only:
87 modules (plus `error` frozen). The 87 stay in `cc_utils`; their target does
not change, only their module name and the lint's area for them.

## 2. The migration

### 2.1 Naming convention (decision D0)

**Mechanical, verbatim leaf rename**: `cc.utils.<leaf>` → `cc.<dest>.<leaf>`.
The leaf segment is preserved exactly, so the rewrite is a pure prefix swap
that can be scripted and grep-audited. This matches the OQ-4 mapping, which
lists leaves verbatim under each destination namespace (e.g. `cc.process.bash`
lists `bash_execution`, not `execution`). Stutter such as
`cc.process.bash.bash_execution` or `cc.cache.cache_paths` is tolerated; a
later style pass can de-stutter. Two deliberate exceptions, where verbatim
would collide or confuse, are called out below (D3, and the settings group).

**Namespaces do not change.** Every module exports `cc::utils::<leaf>` (e.g.
`cc::utils::json`, `cc::utils::path` — verified across the move candidates).
Module names and C++ namespaces are decoupled; renaming the module does not
require touching `cc::utils::` qualifiers at any use site, and this plan does
not. Renaming namespaces is a separate, much larger change (every qualified
use site, not just imports) and is explicitly out of scope (D6).

### 2.2 Rename-only modules (stay in `cc_utils`)

Pure prefix swap per the table in §1. New lint areas created (all at rank 2,
same as `cc.utils` today): `cc.agent`, `cc.cache`, `cc.containers`, `cc.crypto`,
`cc.diagnostics`, `cc.fs`, `cc.media`, `cc.model`, `cc.net`,
`cc.parsing`, `cc.platform`, `cc.prompt`, `cc.scm`, `cc.security`, `cc.serdes`,
`cc.text`. `area_of()` takes the first two segments
(`tools/arch/graph_check.py:108-110`), so `cc.net.http.http` → area `cc.net`,
`cc.model.model` → area `cc.model`, etc.

`error` keeps the name `cc.utils.error` (D1), so the `cc.utils` area does not
fully disappear; `json` renames to `cc.serdes.json` (D2).

### 2.3 Target reassignments (the cross-target track)

Per-module detail. "Links" notes what the destination target must add.

**Importer-side link wiring (B5 moves).** The "Links" column covers the moved
modules' own imports (correctly "none" — every moved module imports only
`cc.utils.*` + std). But the importers' targets must also link the new home of
each moved module they import. The B5 batch descriptions in §4 call these out
per batch; the summary: B5c adds `cc_plugins` to `cc_commands` and
`cc_services`; B5d adds `cc_session` to `cc_commands`; B5f adds `cc_tasks` to
`cc_tools`. Verified no cycle risk: `cc_plugins`, `cc_session`, `cc_tasks` do
not link back to `cc_commands`/`cc_services`/`cc_tools`.

| Module | New name | New target | Link wiring |
|---|---|---|---|
| hooks_config | `cc.hooks.config` | `cc_hooks` | none (cc_hooks links cc_utils) |
| hooks_execution | `cc.hooks.execution` | `cc_hooks` | none |
| hooks_registry | `cc.hooks.registry` | `cc_hooks` | none |
| plugin_dependency_resolver | `cc.plugins.plugin_dependency_resolver` | `cc_plugins` | none |
| plugin_identifier | `cc.plugins.plugin_identifier` | `cc_plugins` | none |
| plugin_lifecycle | `cc.plugins.plugin_lifecycle` | `cc_plugins` | none |
| plugin_loader | `cc.plugins.plugin_loader` | `cc_plugins` | none |
| plugin_manager | `cc.plugins.plugin_manager` | `cc_plugins` | none |
| plugin_marketplace | `cc.plugins.plugin_marketplace` | `cc_plugins` | none (verbatim avoids the existing `cc.plugins.marketplace` — see D3) |
| plugin_marketplace_rules | `cc.plugins.plugin_marketplace_rules` | `cc_plugins` | none |
| plugin_validation | `cc.plugins.plugin_validation` | `cc_plugins` | none |
| plugin_versioning | `cc.plugins.plugin_versioning` | `cc_plugins` | none |
| list_sessions | `cc.session.list_sessions` | `cc_session` | none |
| session_storage | `cc.session.app_storage` (D3) | `cc_session` | none |
| loom_code_hints | `cc.skills.hints` | `cc_skills` | none |
| skill_usage (+impl) | `cc.skills.support` | `cc_skills` | none |
| task_utils | `cc.tasks.support` | `cc_tasks` | none |
| settings_manager | `cc.config.settings_manager` | `cc_config` | + `MODULE_RANK_OVERRIDE` → 2 (D5) |
| settings_merge | `cc.config.settings_merge` | `cc_config` | same |
| settings_paths | `cc.config.settings_paths` | `cc_config` | same |
| settings_sources | `cc.config.settings_sources` | `cc_config` | same; **B5c must land first** (see ordering) |
| settings_validation | `cc.config.settings_validation` | `cc_config` | same |
| statusline_runner | `cc.ui.statusline.runner` | `cc_ui` | none |
| collapse_notifications | `cc.ui.messages.collapse_notifications` | `cc_ui` | none (test-only) |
| collapse_read_search | `cc.ui.messages.collapse_read_search` | `cc_ui` | none (test-only) |
| message_predicates | `cc.ui.messages.message_predicates` | `cc_ui` | none (test-only) |
| tool_helpers | `cc.tools.support.tool_helpers` | `cc_tools` | none (cc_tools links cc_utils) |
| script_tool_enabled | `cc.tools.support.script_tool_enabled` | `cc_tools` | none |
| tagged_id | `cc.types.tagged_id` | `cc_types` | none (pure leaf, rank 0) |
| content_array | `cc.types.wire.content_array` | `cc_types` | none (pure leaf, rank 0) |
| agent_swarms_enabled | `cc.teams.agent_swarms_enabled` | **`cc_teams` (new)** | cc_teams links cc_utils |
| control_message_compat | `cc.teams.control_message_compat` | `cc_teams` | same |
| team_helpers | `cc.teams.team_helpers` | `cc_teams` | same |
| swarm_backends (+8 impl) | `cc.teams.swarm.backends` | `cc_teams` | same |
| swarm_helpers (+1 impl) | `cc.teams.swarm.helpers` | `cc_teams` | same |
| swarm_pane_observer | `cc.teams.swarm.pane_observer` | `cc_teams` | same |

New target `cc_teams`: new file `src/cmake/targets/cc_teams.cmake`,
`include()`d in `src/CMakeLists.txt` after `cc_utils` (line 95) and before
`cc_tools` (line 110). `cc_teams` links `cc_utils` PUBLIC. Its importers
(`cc_tools`, `cc_orchestration`, `cc_ui`, `loom`) each add `cc_teams` to their
`target_link_libraries`. Verified the teams/swarm modules import only
`cc.utils.*` (atomic_replace, json, intra-team), so there is no
teams→tools/services edge and no link cycle.

Layering check for every move (importer ranks vs new area rank, per
`TARGET_RANK` at `tools/arch/graph_check.py:64-91`):

- hooks → `cc.hooks` (rank 4): importers are `cc.query` (10), `cc.ui` (12),
  and `statusline_runner` (moving to `cc.ui` in the same effort). All
  downward. The modules' own imports are `cc.utils.*` only → downward.
- plugins → `cc.plugins` (7): importers are `cc.commands` (11), `cc.services`
  (7, same rank — no SCC since plugins imports nothing from services), `cc.ui`
  (12). Downward/equal.
- session → `cc.session` (6): importers `cc.commands` (11), `cc.ui` (12),
  `main.cpp`. Downward.
- skills → `cc.skills` (5): importers `cc.ui` (12). Downward.
- tasks → `cc.tasks` (6): importer `cc.tools` (8). Downward.
- settings → `cc.config` (area rank 1, overridden to 2 per D5): importers
  `cc.ui` (12) and `cc.plugins.plugin_identifier` (7). Downward.
- statusline/messages → `cc.ui` (12): same-area.
- tools.support → `cc.tools` (8): importers `cc.query` (10), `cc.orchestration`
  (9). Downward.
- types → `cc.types` (0): pure leaves, test-only importers.
- teams → `cc.teams` (new, rank 7): importers `cc.tools` (8),
  `cc.orchestration` (9), `cc.ui` (12), `main.cpp`. Downward. Rank 7 (not 8)
  so that `cc.tools` → `cc.teams` is strictly downward.

### 2.4 Hard ordering constraints

1. **statusline → `cc_ui` (B5a) MUST land before hooks → `cc_hooks` (B5b).**
   `statusline_runner` imports `hooks_execution`
   (`src/utils/statusline/statusline_runner.cppm`). If hooks renames to
   `cc.hooks.execution` (rank 4) while statusline is still `cc.utils.*`
   (rank 2), the edge becomes a NEW upward edge (2→4) that `graph_check.py`
   fails on. Moving statusline first makes the edge `cc.ui` (12) → `cc.hooks`
   (4), downward.
2. **plugins → `cc_plugins` (B5c) MUST land before settings → `cc_config`
   (B5g).** `plugin_identifier` imports `settings_sources`. If settings moves
   to `cc_config` first, `cc_utils` (still owning plugin_identifier) would need
   to link `cc_config`, but `cc_config` links `cc_utils` PUBLIC
   (`src/cmake/targets/cc_config.cmake`) → link cycle. After B5c,
   `plugin_identifier` is in `cc_plugins`; B5g then adds `cc_config` to
   `cc_plugins`'s link libraries (cc_plugins → cc_config → cc_utils, acyclic).
3. **Baseline/lint edits travel in the SAME commit as the rename they
   reference** (see §3). A rename that lands without its baseline update fails
   the arch-check workflow.
4. **`TARGET_RANK` entries for a new area land in the first batch that
   introduces that area** (`graph_check.py` fails closed on unranked areas,
   `:642-643`).

## 3. String-coupling audit checklist (per renamed module)

Unlike the Phase D path moves (which changed nothing but CMake path lines), a
rename changes the module NAME, and importers are found by name. For EVERY
module renamed `cc.utils.X` → `cc.Y.X`, grep and update:

1. **`import cc.utils.X;`** — every importer in `src/`, `tests/`,
   `benchmarks/`, `src/benchmarks/`. This is the mechanical rewrite. Start
   with `grep -rn "cc\.utils\.X\b" --include="*.cppm" --include="*.cpp"`.
2. **`module cc.utils.X;`** — the module implementation units (17 `.cpp`
   files: 7 json, 8 swarm_backends, 1 skill_usage, 1 swarm_helpers). The impl
   unit's `module` declaration must match the primary's new name.
3. **`tools/arch/upward_edge_baseline.txt`** — rewrite every line naming `X`
   on either side. Live entries referencing `cc.utils.*`: `cc.utils.json`
   (lines 10, 12, 21 — line 21 is `cc.migrations.concrete -> cc.utils.json`),
   `cc.utils.parse_int` (11), `cc.utils.file_persistence` (22),
   `cc.utils.lockfile` (23). The baseline keys on exact module names; a stale
   entry reads as "edge removed" (allowed) plus "new upward edge" (FAIL).
4. **`tools/arch/dead_imports_baseline.txt`** — rewrite every line naming `X`
   on either side. This is the largest lint coupling: ~40 lines reference
   `cc.utils.*` names (file, process, http, git_filesystem, shell,
   detect_repository, markdown_utils, path_utils, platform_paths,
   string_utils, env_utils, git, tool_helpers, bash_execution,
   swarm_backends, team_helpers, format, debug, hyperlink, path,
   session_storage, and intra-utils lines such as
   `cc.utils.plugin_loader -> cc.utils.plugin_versioning`,
   `cc.utils.settings_manager -> cc.utils.settings_merge`). Both sides of
   each line must be rewritten when either module renames. Note: impl-unit
   entries are keyed on `module [impl:rel/path]` format (`graph_check.py`
   `unit_label`, `:488-497`); moving an impl `.cpp` file changes the path
   suffix. No baseline entries exist for the moving impls today (the only
   `[impl:` entries are `cc.ui.app.app` and `cc.ui.prompt.*`), so this is
   immaterial now — but "rewrite every line naming X" does not account for
   the path suffix.
5. **`tools/arch/inline_def_baseline.txt`** — rewrite lines naming `X`:
   `cc.utils.plugin_loader` (line 10), `cc.utils.team_helpers` (16),
   `cc.utils.swarm_helpers` (20), `cc.utils.hooks_execution` (32),
   `cc.utils.file_edit` (34), `cc.utils.json` (42), `cc.utils.swarm_backends`
   (43) — line numbers as of 2026-09-29 after the `cc.tools.mcp` stale-entry
   removal; match by module NAME, not line number. The ratchet keys on module name; a stale entry is SILENTLY IGNORED —
   the module becomes "untracked" and the ratchet stops enforcing its frozen
   count, with no violation and no report (no code path flags baseline entries
   that match no live module). The fail-closed new-interface gate
   (`inline_def_check.py:394-399`) fires only when inline bodies exceed
   `C2_LIMIT`=100, and the 7 affected modules have frozen counts of
   60/48/42/29/27/22/22 — all under 100, so the gate does not backstop a
   stale entry for them. This failure already occurred once: `cc.tools.mcp 75`
   was a stale baseline entry (the module no longer exists; only
   `cc.tools.mcp_classify` does) and `inline_def_check.py` reported 0
   violations. That entry is removed as part of this plan (B0). The §6.2
   stale-entry diff is the only detection mechanism.
6. **`tools/arch/graph_check.py`** — `TARGET_RANK` (`:64-91`): add the new
   area at rank 2 (or `cc.teams` at 7) in the first batch for that area.
   `MODULE_RANK_OVERRIDE` (`:96-101`): add the five `cc.config.settings_*`
   modules at rank 2 (D5). `CORE8`/`TARGET_AREAS` (`:103-105`): revisit in
   the final batch once `cc.utils` has shrunk to `error` only.
7. **`tools/arch/inline_def_check.py`** — the flat-placement gate
   (`:51-53`, `:383-393`) keys on `module.startswith("cc.utils.")`. Once
   modules rename out of `cc.utils`, this check becomes vacuous for them.
   Generalize it (e.g. key on the file's `src/utils/<area>/` path instead of
   the name prefix) or retire it in the final batch. Two auxiliary files also
   key on module names and must be checked: `flat_utils_exceptions.txt`
   (`inline_def_check.py:54`) and `port_allowlist.txt` (`graph_check.py:36`).
   Both are comment-only (no active entries) today; verify they stay empty of
   `cc.utils.*` names throughout the migration.
8. **CMake** — module names do NOT appear in any cmake file; only file paths
   do. Target moves edit `cc_utils.cmake` (remove path) and the destination
   target's cmake file (add path, preserving FILE_SET vs PRIVATE membership:
   the 17 impl `.cpp` units stay PRIVATE). `cc_teams` is a new file plus one
   `include()` line. No module-name edits.
9. **Namespaces `cc::utils::*`** — DO NOT change (D6). Confirmed decoupled.
10. **String literals** — grep `"cc\.utils` across `src/`: zero hits
    (verified). No reflection-by-name. Per-domain strings that must NOT be
    touched (they are not module names): plugin identifier strings, settings
    keys, env var names, tool names, the `<task_notification>`/`<status>`/
    `<summary>` tag formats (CLAUDE.md hazard — produced by
    `local_agent_task`/`local_shell_task`/`runtime_registry`, consumed by
    `ui/messages/collapse_background_bash.cppm`), the tree-sitter language
    name `"bash"`, hook event names. The rename diff must touch only
    import/module declarations, cmake paths, lint files, and comment
    occurrences updated per item 11.
11. **Comments (dead-import mask)** — `graph_check.py:624`
    (`if not used and imp in body: used = True`) plus the NOTE at
    `:553-557` ("Names in trailing comments or string literals still count
    as use") makes comment/string occurrences of the dotted module name
    load-bearing for the dead-import check. There are ~69 comment
    occurrences of `cc.utils.` in `src/` outside import/module declarations
    (e.g. `src/tools/agent_runtime_builtin_impl.cpp:5`,
    `src/services/mcp/client_protocol.cpp:3,23`,
    `src/utils/swarm/swarm_backends_inprocess.cpp:1,7`). For every renamed
    module, grep `cc\.utils\.<leaf>` (no leading quote) and either update
    comment occurrences to the new name in-batch (preserves the mask) or
    explicitly accept and resolve the resulting `new_dead` graph_check
    failures per batch. Leaving comments stale loses the mask and surfaces
    unexpected dead-import failures mid-batch (loud — the gate backstops it
    — but avoidable).
12. **`__FILE__`** — none in `src/utils/` (verified); path moves are
    behavior-safe.
13. **Docs (non-load-bearing, final batch)** — `CLAUDE.md`,
    `docs/decisions/design-decisions.md` (references `cc.utils.file_index`,
    `cc.utils.plugin_manager`, `cc.utils.plugin_marketplace` at lines 828,
    1939-1940, 2808), `tools/arch/README.md:120-122`. Update for accuracy.
14. **`tests/test_utils.cpp`** — universal importer (53 `cc.utils.*`
    imports); update in every batch.

## 4. Batch breakdown and rollback

Each batch is one commit (target moves: one commit per move). Rollback is
`git revert` of the batch commit; because the lint/baseline edits travel in
the same commit, a revert is atomic and leaves the tree green.

**B0 — prep (no `src/` changes).** Snapshot `graph_check.py --json` output as
the pre-migration baseline (module count, area SCC list, upward-edge set).
Confirm the frozen-exception decisions D1/D2. Remove the pre-existing stale
`cc.tools.mcp 75` entry from `inline_def_baseline.txt` (the module no longer
exists; the entry was silently ignored — see §3.5); this cleanup is part of
this plan and is verified at B0.

**B1 — leaf domains (≈35 modules; includes one target move).** `types` (→
`cc_types` target — the one cross-target move in this batch; everything else
is rename-only), `agent`, `cache`, `prompt`, `media`, `model`, `diagnostics`,
`containers`, `crypto`, `env`, `parsing`, `tree_sitter`. Almost all importers
are `tests/test_utils.cpp` only; the only non-test importers are `effort`
(← `skills/load_skills_dir`), `image_store` (← `ui/messages/message_image`),
`tree_sitter.bash` (← `tools/destructive_command_warning`), `crypto` (12),
`uuid_utils`/`xdg` (1 each). Add the new `TARGET_RANK` areas. Trivial risk.

**B2 — `fs` + `fs.edit` + `text` (20 modules).** Medium fan-in: `parse_int`
15, `file` 7, `atomic_replace` 7, `string_utils` 6, `parse_references` 8,
`path` 5, `file_edit` 4, `file_persistence`/`file_read_cache` 2. Baseline
couplings: `file_persistence`, `lockfile` (upward baseline), `file`,
`string_utils`, `path_utils`, `markdown_utils`, `format`, `path`
(dead-import baseline), `file_edit` (inline baseline).

**B3 — `platform` + `net.http` + `process` + `process.bash` + `process.shell`
+ `scm.git` + `security` + `tools.support` (→ `cc_tools`) (≈35 modules).**
Higher fan-in: `bash_execution` 38, `exec_sync` 12, `env_utils` 12, `git` 10,
`http` 8, `async` 7, `tool_helpers` 6, `permissions_engine` 5, `env` 4,
`http_encoding` 4, `debug` 4, `shell` 2. Baseline couplings: `process`,
`http`, `git_filesystem`, `shell`, `detect_repository`, `platform_paths`,
`env_utils`, `git`, `tool_helpers`, `bash_execution`, `debug`, `hyperlink`.
The `cc_tools` move (tool_helpers, script_tool_enabled) is safe here: no
`cc_utils` module imports either, and `cc_tools` already links `cc_utils`.

**B4 — `serdes`: `json` (+7 impl), `yaml`, `frontmatter_parser`.** The single
biggest mechanical change: `json` has 131 `src/` + 15 `tests/` + 5
`src/benchmarks/pare/` importers. Baseline couplings: `cc.utils.json`
(upward baseline ×2, inline baseline), plus every dead-import line naming
`json` indirectly through other modules. Pure prefix swap; risk is merge
churn, not correctness.

**B5 — target moves (one commit each, in this order):**

- **B5a**: `statusline_runner` → `cc_ui` (`cc.ui.statusline.runner`);
  `collapse_notifications`/`collapse_read_search`/`message_predicates` →
  `cc_ui` (`cc.ui.messages.*`). MUST be first (ordering constraint 1).
- **B5b**: hooks (3) → `cc_hooks`.
- **B5c**: plugins (9) → `cc_plugins`. MUST precede B5g (ordering
  constraint 2). Importer-side link additions: `cc_commands` adds
  `cc_plugins` (imports `plugin_lifecycle`, `plugin_manager`,
  `plugin_marketplace`, `plugin_validation` via `commands/plugin_cmd.cppm`
  and `commands/plugin/plugin_manage.cppm`); `cc_services` adds `cc_plugins`
  (`services/mcp/channel_notification.cppm` imports `plugin_identifier`).
- **B5d**: session (2) → `cc_session`. Importer-side link addition:
  `cc_commands` adds `cc_session` (`commands/insights.cppm` imports
  `list_sessions`).
- **B5e**: skills (2) → `cc_skills`.
- **B5f**: tasks (1) → `cc_tasks`. Importer-side link addition: `cc_tools`
  adds `cc_tasks` (`tools/runtime_registry_team_dispatch.cpp` imports
  `task_utils`).
- **B5g**: settings (5) → `cc_config`, with `MODULE_RANK_OVERRIDE` entries
  and `cc_config` added to `cc_plugins` link libraries. Note: `cc_ui`
  reaches `cc_config` transitively via `cc_ui` → `cc_hooks` (PUBLIC) →
  `cc_config` (PUBLIC) — `src/ui/app/app_settings.cpp` imports
  `cc.utils.settings_manager` (renamed `cc.config.settings_manager` in this
  batch). This works today but is fragile: a future `cc_hooks` PUBLIC→PRIVATE
  change breaks it. Either accept the transitive path (documented here) or
  add `cc_config` to `cc_ui`'s link libraries explicitly.
- (B5h folded into B3: tools.support → `cc_tools`.)

**B6 — `cc_teams` new target (6 primaries + 9 impl units).** New
`src/cmake/targets/cc_teams.cmake`; `include()` in `src/CMakeLists.txt`
between `cc_utils` and `cc_tools`; add `cc_teams` to `cc_tools`,
`cc_orchestration`, `cc_ui`, `loom` link libraries; `TARGET_RANK["cc.teams"]
= 7`. Fan-in: `team_helpers` 14+3, `swarm_backends` 14+2, `swarm_helpers`
3+2, `swarm_pane_observer` 2+1, the two teams modules test-only. Baseline
couplings: `team_helpers`, `swarm_helpers`, `swarm_backends` (inline
baseline); `swarm_backends`, `team_helpers` (dead-import baseline).

**B7 — finalize.** Resolve D1/D2 (`error`); retire or generalize the
flat-placement lint in `inline_def_check.py`; clean up `CORE8`/`TARGET_AREAS`
in `graph_check.py`; docs sweep (§3.13); append the §12 implementation-history
row.

## 5. Risk assessment

**Dangerous batches:**

- **B4 (`json`)** — 152 import sites across src/tests/benchmarks plus three
  lint baselines. Purely mechanical, but the largest diff and the most likely
  to conflict with in-flight work. Mitigation: script the prefix swap, land
  when the tree is quiet, keep it a single commit.
- **B6 (`cc_teams`)** — only batch that creates a new target and edits link
  libraries in four other targets. A missed link addition is a link error
  (not a silent wrong answer), so the build catches it, but the CMake surface
  is the widest. Mitigation: follow the `ide_integration` precedent
  (commit `906e888`), build both presets.
- **B5b (hooks)** — carries the one hard lint ordering constraint (B5a must
  precede it). `query_engine` is an importer; a mistake here is a new upward
  edge, caught by `graph_check.py` before commit.
- **B5g (settings → `cc_config`)** — rank-override semantics (D5) and the
  B5c-before-B5g link-cycle constraint. The `MODULE_RANK_OVERRIDE` entries
  must land in the same commit or the settings modules' `cc.utils.json`
  imports read as new upward edges.

**Medium batches:** B3 (`bash_execution` 38, `exec_sync` 12, `env_utils` 12,
`git` 10) and B2 (`parse_int` 15). Mechanical, but enough importers that a
missed site is a compile error — caught by the build.

**Trivial batches:** B1 (mostly `test_utils.cpp`-only importers) and the B5
moves with test-only importers (messages-collapse, skills `loom_code_hints`,
`types`). B5d/B5e/B5f have 1-2 non-test importers each.

**Cross-cutting risk — partial migration is safe.** Because module names are
decoupled from paths and targets, the tree is green at every batch boundary
even if the effort stops after B1: renamed and unrenamed modules coexist
(they already do — `cc.services.ide_integration` proves it). There is no
flag-day.

**Cross-cutting risk — silent breakage is low.** The CLAUDE.md hazard
(string/shape couplings that break silently) does not apply to module names:
there are no `"cc.utils..."` string literals, no registry keys, and no
reflection-by-name on module names. A missed import is a compile error, not a
wrong answer. The one silent-failure mode is a STALE LINT BASELINE. For
`upward_edge_baseline` and `dead_imports_baseline`, a stale entry surfaces as
a new upward edge / new dead import (loud — the gate fails). For
`inline_def_baseline`, a stale entry is SILENTLY IGNORED: the module becomes
"untracked" and the ratchet stops enforcing its frozen count, with no
violation and no report (the fail-closed gate fires only above 100 inline
bodies; the 7 affected modules are all under 100). The §6.2 stale-entry diff
is the only detection mechanism for this case — which is why constraint 3
(baselines in the same commit) and the §6.2 diff are both load-bearing.

## 6. Verification protocol (per batch)

1. **`graph_check.py`**: run before and after; diff the `--json` output.
   Invariants: same module count (635 total / 124 `cc.utils`, minus none), no
   new area SCCs, no new upward edges, no unranked areas,
   `target_area_sccs` unchanged.
2. **`inline_def_check.py`**: ratchet green with the rewritten baseline (no
   module over its frozen count, no new-interface gate trip). Additionally,
   diff the `inline_def_baseline.txt` module-name set against the live module
   set (from `inline_def_check.py --json`, whose `rows` carry a per-interface
   `module` field) and assert zero stale entries: baseline module names minus
   live module names must be empty. This is the only detection mechanism for
   the silent stale-entry failure (§3.5) — "ratchet green" alone passes
   identically whether the baseline was rewritten or silently dropped.
3. **Dead-import check** (part of `graph_check.py`): no gains vs the rewritten
   `dead_imports_baseline.txt`.
4. **Build**: `cmake --build --preset local-linux -j8` AND
   `cmake --build --preset local-linux-release -j8`, both `-Werror` clean
   (directive 2026-09-29: the local dual preset is the gate; macos CI is not
   required).
5. **`ctest --preset local-linux -j1`** serial, green (1706 tests as of the
   2026-09-26 Phase D row; reconfirm at execution).
6. **Replay / goldens**: the UI truecolor golden suite byte-identical. A
   rename is behavior-neutral, so goldens should not change; any golden diff
   means an accidental content edit slipped in.
7. **Agent review** per project convention (reviews are agent-run, never
   user-run): one independent review per batch, with byte-level audit of the
   rename diff (import/module declarations + cmake paths + lint files +
   comment updates per §3.11).

## 7. Decisions and open questions

- **D0 (resolved)**: verbatim leaf rename; namespaces untouched.
- **D1 — `error`**: KEEP `cc.utils.error` as a frozen exception (OQ-4 says
  "stays"; 47+5 importers make a rename high-churn for zero structural gain).
  Record it in the exceptions file. Alternative: rename to `cc.error.error`
  in B7 if a fully-empty `cc.utils` area is wanted.
- **D2 — `json`**: rename to `cc.serdes.json` in B4 (recommended — it is the
  namesake of the dissolution). Deferrable as a frozen exception if in-flight
  churn is a concern; the rest of the plan does not depend on it.
- **D3 — `session_storage` name**: recommend `cc.session.app_storage` (it is
  the app/UI session storage used by `main.cpp` and `ui/app/*`, distinct from
  the existing `cc.session.storage` conversation-persistence module used by
  `server`/`query`). Verbatim `cc.session.session_storage` works but leaves
  two confusingly-similar names. `plugin_marketplace` needs NO exception:
  verbatim `cc.plugins.plugin_marketplace` coexists with the existing
  `cc.plugins.marketplace` (a small, test-only `PluginStatus` carrier).
- **D4 — messages-collapse (test-only)**: move to `cc_ui` as
  `cc.ui.messages.*` (recommended). OQ-4 maps these to `cc.messages.support`,
  but `cc.ui.messages` is chosen here as a decision: they are test-only leaves
  and `cc_ui` is the natural home (the existing `cc.ui.messages.*` modules
  already live there). They are imported only by `tests/test_utils.cpp`;
  deletion is a separate dead-code decision and is NOT recommended here (they
  survived the c10-b19 dead-code pass).
- **D5 — settings rank**: the five `cc.config.settings_*` modules get
  `MODULE_RANK_OVERRIDE` → 2, per RFC 0001 §3.1 ("cc.config.config / .settings
  rank WITH utils — they import utils.json"). Without the override, their
  `cc.utils.json` imports read as rank-1→2 upward edges.
- **D6 (resolved, out of scope)**: C++ namespace renames
  (`cc::utils::*` → `cc::fs::*` etc.) are a separate, larger change touching
  every qualified use site. Not part of this plan.
- **D7 — `cc_model` / `cc_security` targets**: not created. `model` (5
  modules, mostly test-only) and `security` (7) rename in place in `cc_utils`.
  Per-area static libraries are Phase E work (RFC §3.2 rule 6: one
  responsibility area per library once acyclic); the rename is the
  prerequisite, not the moment, for that split.
- **D8 — `providers`**: zero importers (not re-exported by `model.model`).
  Renamed mechanically in B1; flagged for the next dead-code pass. Same for
  any other test-only module that survives B1.

## Review history

- **2026-09-29 — two adversarial design reviews, verdict request-changes.**
  All required changes applied:
  (1) corrected the `inline_def_baseline` stale-entry failure mode (silently
  ignored, not a fail-closed gate trip) and added a §6.2 stale-entry diff
  sub-step; removed the pre-existing `cc.tools.mcp 75` stale entry;
  (2) added a §3.11 checklist item for comment occurrences of `cc.utils.*`
  names (the `imp in body` dead-import mask) and amended the diff-scope rule;
  (3) corrected the `upward_edge_baseline` json enumeration to include line
  21; (4) documented cc_ui's transitive cc_config dependency in B5g;
  (5) added the four missing importer-side link additions to B5c/B5d/B5f
  (cc_plugins→cc_commands/cc_services, cc_session→cc_commands,
  cc_tasks→cc_tools); (6) corrected six numerical errors (swarm_backends
  impl count, impl-unit count, json benchmark importers, parse_int fan-in,
  env_utils fan-in, test_utils.cpp import count); (7) recategorized B1 to
  acknowledge the types→cc_types target move; (8) reworded D4 to state the
  cc.ui.messages destination as a decision rather than a match to OQ-4;
  plus non-blocking minors: 18→17 impl units, removed dead `cc.messages`
  TARGET_RANK entry, documented flat_utils_exceptions/port_allowlist and the
  dead_imports_baseline `[impl:rel/path]` key format.

- **2026-09-29 — independent design verification (agent:design-verify),
  verdict approved.** All eight required changes confirmed against code and
  tooling (the four importer-side link additions and no-cycle claims, the six
  numerical corrections, ratchet green with zero stale baseline entries).
  Minor citations folded in the same pass: baseline line numbers re-pinned
  after the `cc.tools.mcp` removal (match by module NAME, not line number),
  the §6.2 stale-entry diff now sources module names from
  `inline_def_check.py --json` (`graph_check.py --json` emits no names), the
  §6.1 invariant states 635 total / 124 `cc.utils` modules, and the OQ-4
  deletion arithmetic reconciled — 46 in `56bf7e8` + `cc.utils.tool_management`
  in `f1cae09` = 47; 171 − 47 = 124.
