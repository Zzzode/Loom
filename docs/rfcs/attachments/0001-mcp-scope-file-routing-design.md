# RFC 0001 Phase B follow-up — `loom mcp --scope` file routing design

Status: **v2, 2026-09-27 — independent adversarial design review returned
APPROVE WITH CHANGES; all nine required edits are folded into this revision.**
v1 was reviewed by a separate agent (citations re-verified in full); two
blocking findings shaped §A (user-file collision with the registered `config`
tool) and §B (full-save leakage of higher-tier entries). No code yet; C6
implementation may start from THIS document under the normal adversarial
code-review gate.

## Problem

- `loom mcp add --scope local|user|project` (`src/commands/mcp_cmd.cppm`,
  default `"local"` :495, validation :544-551) only pastes a label onto the
  entry (`config.config_scope = scope;` :662) and then calls
  `config_manager_.save()` with no argument (:689) — always
  `ConfigSource::ProjectConfig`.
- `ConfigManager::save` (`src/config/config.cppm:210`) chooses only between
  `global_path_` (~/.config/loom/config.json, hardcoded $HOME) and
  `project_path_` ($CWD/.loom/config.json), and rewrites the ENTIRE merged
  settings by hand-rolled string concat. It is also non-atomic (plain
  ofstream :228-238).
- Provenance is destroyed at load: the `mcpServers` block does
  `settings_.mcp_servers.clear()` then appends (:436, inside the per-file
  `if (mcpServers.is_obj())`). `ProjectMcpServersReplaceGlobal` pins this:
  a project file containing ANY mcpServers key — even `{}` — makes all
  global servers vanish.
- Net defects: `--scope user` writes the project file; every save copies
  global servers down into the project file; `local` is an unconsumed label;
  `configScope` (persisted since C4) has zero routing consumers.
- Adjacent: `serialize_settings` always emits default model/display/features
  sections (:564-581), so the first `mcp add` in a fresh project creates
  .loom/config.json full of defaults that override global settings.
- Every mutating mcp subcommand full-saves: add :689, remove :733 (parses
  NO flags at all), enable :883, disable :925, xaa setup :1055 / clear
  :1167. The `xaaIdp` shape has no parser/serializer anywhere — those saves
  persist nothing but still rewrite every section.
- Other save() callers: `commands/config.cppm:192` (/config set),
  `ui/dialogs/settings_dialog.cppm:1199` (Ctrl-S; working copy round-trips
  mcp_servers :320/:350 and can delete entries :1506). No production caller
  ever passes GlobalConfig.

## Canonical scope semantics already in the tree

- `src/config/settings.cppm:199-216` — SettingsScope read paths (the READ
  path uses config_home_read_under and can land in `.agents`/`.claude`).
- `src/utils/settings/settings_manager.cppm:94-107` — settings WRITE
  hardcodes `$HOME/.loom/settings.json` (does NOT honor $LOOM_CONFIG_DIR);
  its atomic tmp+rename and gitignore appliers (:556-589) are PRIVATE,
  shape-specific templates to MIRROR, not call. The tmp name is a fixed
  `target + ".tmp"` with no fsync — concurrent CLIs in one CWD can collide;
  C6 copies this known limitation and documents it.
- `src/services/mcp/config_impl.cpp:141-167` — four MCP paths; its User
  tier hardcodes ~/.loom (no env var).
- `src/constants/paths.cppm:100-114` — `config_home_write()` =
  $LOOM_CONFIG_DIR > ~/.loom, deliberately excluding the read cascade. Its
  only non-paths caller today is memdir/paths.cppm:164.
- Reusable JSON machinery (verified): `JsonMutDoc::raw_json`
  (src/utils/serdes/json.cppm:274; impl json_mut_doc.cpp:41-48 — strict
  yyjson_read, deep-copies the fragment into the target doc), `copy_val`
  (yyjson_val_mut_copy), `ensure_object` (json_mut_val.cpp:84-90), `remove`
  (:48-51), `set(k,bool)` (:72), `to_pretty_string` (YYJSON_WRITE_PRETTY,
  2-space, no trailing newline). `yyjson_mut_obj_put` REPLACES in place
  preserving key order — but with a NULL value it DELETES the key
  (yyjson.h:6766; json_mut_val.cpp:40-44 has no null guard).

## Runtime consumption chain (why files must not be unified)

1. `main.cpp:1790` install_core_settings_mcp_loader
   (`src/commands/mcp/core_settings_loader.cppm:28-41`): fresh core
   ConfigManager, two config.json files, map via to_native_mcp_server; a
   load error suppresses ALL MCP servers (mcp_tool.cppm:961).
2. `NativeMcpRuntime::ensure_loaded_from_config`
   (src/orchestration/tools/mcp_tool.cppm:950-992): core list, then
   services ConfigLoader four-file overlay (:964-970), then plugins;
   merge_native_mcp_servers (:608-618): precedence
   plugins > services 4-file > core 2-file on name collision.
3. `McpConnectionManager::initialize` (internal ConfigLoader caller) is
   never invoked by the runtime (tests only).
4. After mcp add/remove, sync (:899-910) REPLACES live configuration with
   the command manager's core list for the rest of the process.

Core writes must NOT use the mcp_servers.json file names: the services
overlay would double-load them and win on collisions. (The services WRITE
path that motivated an older version of this warning was deleted in C5; the
double-load/win-on-collision rationale itself stands.)

## Decisions

- **D1 (explicit product decision)** `user` →
  `cc::constants::paths::config_home_write()/"config.json"` — i.e.
  $LOOM_CONFIG_DIR/config.json else ~/.loom/config.json. This is a NEW
  application of config_home_write() per the write policy at
  paths.cppm:100-114; neither the settings writer (hardcoded ~/.loom) nor
  the services User tier (hardcoded ~/.loom) honors the env var today.
  Legacy ~/.config/loom/config.json remains a READ tier (global).
- **D2** mcpServers load merge becomes per-entry name overlay: erase
  same-named entries from the merged vector, then append the file's
  entries. An empty `mcpServers: {}` overrides NOTHING (today it clears).
  Flips `ProjectMcpServersReplaceGlobal`. Non-MCP section merges unchanged.
- **D3** `mcp remove <name>` (no --scope) is BEST-EFFORT across every
  scope file containing the name, and is re-runnable/idempotent:
  - Result distinguishes three outcomes: name absent from every file
    → existing NotFound text; ≥1 write succeeded → success, listing files
    touched; present but ZERO writes succeeded (all unwritable) → error
    listing the files that could not be written (never "not found").
  - `--scope/-s local|user|project|global` restricts to one file; not-in
    that-scope names the scope in the message.
  - Global legacy file IS written when it contains the name (required to
    clean polluted installs); an unwritable global does not abort writable
    tiers — failures aggregate.
- **D4** No automatic migration rewrite. Pre-existing global→project copies
  stay on disk; precedence makes them harmless shadows; `remove` cleans
  them on demand.
- **D5** enable/disable: with no --scope, patch the highest-precedence file
  that physically contains the effective entry. If the entry exists ONLY in
  the legacy global tier, patch the global file in place (same write policy
  as D3) rather than failing; a write failure returns an error naming the
  file. With --scope, patch only that file and error if absent there.
  Known limitation (pre-existing, NOT changed by C6): to_native_mcp_server
  drops `disabled` (mcp_tool.cppm:249-269) and the runtime signature
  excludes it, so enable/disable do not change LIVE connections in the
  running process — effects apply on next start. Help text must not claim
  otherwise.
- **Same-name add**: `add name --scope X` when the name exists in other
  tiers writes only X and creates a physical shadow; lower copies persist
  until an explicit `remove` (default remove clears them all). `list`
  shows one merged row; an owner column is deferred.

## Design — targeted per-entry JSON patching

### src/config/config.cppm — additive API

```cpp
/// Same-name resolution, highest first: Local > Project > User > Global.
enum class McpStorageScope : std::uint8_t { Global, User, Project, Local };

[[nodiscard]] static std::optional<McpStorageScope>
mcp_scope_from_label(std::string_view);  // "local"|"user"|"project";
                                         // "global" accepted internally

[[nodiscard]] std::vector<std::pair<McpStorageScope, std::filesystem::path>>
mcp_scope_paths() const;  // lowest→highest precedence; missing files ok

[[nodiscard]] VoidResult upsert_mcp_server(McpStorageScope,
                                           const McpServerConfig&);

[[nodiscard]] Result<McpRemoveOutcome>
remove_mcp_server(std::string_view name,
                  std::optional<McpStorageScope> = std::nullopt);

[[nodiscard]] VoidResult
set_mcp_server_disabled(std::string_view name, bool disabled,
                        std::optional<McpStorageScope> = std::nullopt);

[[nodiscard]] std::vector<std::pair<McpStorageScope, std::filesystem::path>>
find_mcp_server_files(std::string_view name) const;
```

`McpRemoveOutcome { std::vector<path> touched, failed; }` (names/types at
implementation discretion), so the command can render D3's three outcomes.

**Paths / constructors.** New members `user_path_`, `local_path_`.
Local-path rule (ONE rule, resolves production and tests):

    local_path_ = project_path_.parent_path() /
                  (project_path_.stem().string() + ".local.json")

→ production `.loom/config.local.json`; 2-arg test ctor with
`root/project.json` → `root/project.local.json`.
Default ctor: user_path_ = config_home_write()/"config.json"
($LOOM_CONFIG_DIR honored). Existing 2-arg ctor stays two-file:
`user_path_` EMPTY and skipped (never an exists("") syscall); local derived
by the rule above and read only if present. Add a 4-arg ctor
(global, user, project, local) for hermetic four-file tests.

**Load.** Iterate global→user→project→local. In the mcpServers block
replace clear()+append with name-keyed overlay (erase same-named entries
from settings_.mcp_servers as the file's entries are applied). While
loading, also record the per-source data needed by §B:

- `std::unordered_map<std::string, McpStorageScope> mcp_owner_` —
  highest-precedence owner per name (overwritten as higher tiers load);
- per-file name sets for the target files, so §B can test "does the target
  file itself contain this name?" without a second disk read.

### §A — User/local tiers vs the non-JSON `config` tool (blocking fix)

A REGISTERED tool (`config`, src/tools/runtime_registry_register.cpp:277,
ToolPermission::Write) appends `key=value\n` lines to
`$HOME/.loom/config.json` (src/tools/runtime_registry_executors.cpp
`execute_config_tool`, ~:248-273) — D1's exact user path, and it ignores
$LOOM_CONFIG_DIR. Real installs can therefore hold a file that is not a
JSON object. Strict parse there must not suppress all MCP at startup.

Policy in C6:
- global/project parse errors remain HARD failures (today's behavior; those
  files are only ever written as JSON by Loom).
- user/local parse errors (missing file tolerable as today; JSON parse
  failure / root-not-object): the tier contributes ZERO entries and a
  single diagnostic is recorded — stderr `warning: ignoring <path>: not
  valid JSON (use 'loom mcp' to edit MCP configuration)` for CLI commands
  and the existing core-loader error/trace channel for the startup path;
  loading continues with lower tiers. The tier is NOT marked writable for
  in-place patch: an `add --scope user` against an unparseable user file
  writes via the patcher only after explicit handling — C6 makes upsert
  FAIL with an actionable error ("<path> is not valid JSON; move it aside
  first") rather than overwriting user data. (Removing/patching a
  parseable file is unaffected.)
- Tier-2 follow-up (recorded, not in C6): repoint the `config` tool to a
  non-JSON file (or make it JSON-aware) so the collision disappears;
  candidate: reuse settings.json or `~/.loom/config.env`.

### §B — Full-save must not leak higher-tier entries (blocking fix)

After D2, settings_.mcp_servers is the four-file overlay and may contain
user/local entries carrying Authorization headers / OAuth data. The two
remaining full-save paths (/config set; settings dialog Ctrl-S; also xaa
setup/clear) write the tracked project file and would otherwise copy those
entries downward into VCS.

Rule in serialize_settings' mcpServers emission when writing the PROJECT
file: emit a merged entry E only if

    E.name is physically present in the project file BEFORE this save
    OR E.name's highest owner is global/project (not user/local)

i.e. entries added solely at user/local never enter the tracked file, and
names the project file already contains are not destructively dropped
 merely because a higher tier shadows them. The global tier is left to the
 existing behavior (the pre-existing global→project copy class is a Tier-2
 cleanup, explicitly not widened here). Full-save to any non-project target
 follows the analogous "at or below this tier, or already physically there"
 rule (no current callers, implemented for symmetry). This also closes
 scoped-remove resurrection: a name removed from user/local can't be
 rewritten there by a later full save.

### Patch mechanics

- Factor the per-entry text out of append_mcp_servers (:636-716) into
  `[[nodiscard]] static std::string serialize_server_object(const McpServerConfig&)`
  returning a BARE `{ … }` object fragment (no quoted key, no indent
  prefix, no trailing comma), strict-JSON-parseable, key set/order exactly
  what C4 pinned (type, command, args, env, url, headers, headersHelper,
  disabled, configScope, oauth{authServerMetadataUrl, callbackPort,
  clientId, xaa, issuer}). append_mcp_servers becomes its only pretty-print
  caller — one logical wire shape.
- Private `patch_mcp_file(path, fn)`: read (missing → `{}`); strict-parse
  via JsonMutDoc; ensure_object("mcpServers"); apply fn; atomic write via
  fixed-name tmp + fs::rename, mirroring settings_manager:556-572 (no fsync,
  documented concurrent-CLI limitation).
  - upsert: fragment = raw_json(serialize_server_object(cfg)); CHECK the
    returned mut val is valid BEFORE `servers.add(name, fragment)` — adding
    a null/invalid val DELETES the key (yyjson.h:6766); on invalid fragment
    return an internal error, never touch disk.
  - remove: servers.remove(name); when mcpServers becomes `{}`, drop the
    key but keep every other top-level section.
  - disable: re-read owner file, navigate the entry object, `set` only
    "disabled" true/false (preserves siblings and unknown keys).
- FORMATTING: patched files are re-emitted yyjson-PRETTY, so the first
  patch reformats the WHOLE file (arrays/oauth become multi-line vs the
  hand-serializer's inline style). Key set and key order are preserved.
  Tests compare patched entries STRUCTURALLY (parse + field compare), never
  by byte equality. Whitespace divergence accepted; key divergence is a bug.
- First Local write appends `config.local.json` (the resolved basename) to
  the ./.gitignore using the settings_manager:574-589 algorithm; idempotent
  (must handle an existing entry and a file with no trailing newline).

### src/commands/mcp_cmd.cppm

- add (:680-694): replace merged-vector mutate + save() with
  upsert_mcp_server(mcp_scope_from_label(scope), config); keep
  config.config_scope = scope (:662) as a truthful label. Then:
  1. force a reload by resetting the `config_loaded_` one-shot guard
     (:230/:235-241) and re-running ensure_config_loaded() — plain
     sync_native_runtime() would no-op against the cached manager;
  2. if reload FAILS, RETURN THE ERROR WITHOUT syncing — a failed load()
     blanks settings_ (config.cppm:188) and an unconditional sync would
     push an EMPTY server list into the live runtime;
  3. on success sync_native_runtime().
- remove (:721-741): parse optional --scope/-s; remove_mcp_server; render
  the D3 three-way outcome; reload-with-abort + sync only when ≥1 file
  changed.
- enable/disable (:853-934): set_mcp_server_disabled per effective name
  (nullopt → highest owner); "all" resolves each name's owner and patches
  each distinct file once; aggregate failures; reload-abort-sync. Help text
  notes the next-start effect (D5 limitation).
- list/show/restart/reconnect unchanged (merged view). XAA setup/clear,
  /config set, settings dialog stay on full save() — now safe for
  user/local entries via §B.

## Backward compatibility

- Polluted installs untouched on disk; precedence resolves them; remove
  cleans every copy on demand.
- configScope stays written (C4 tests) but never drives routing; physical
  file is the truth.
- C4 round-trip tests unchanged (single-file save() path kept).
- `ProjectMcpServersReplaceGlobal` (tests/test_services.cpp:5892) edited to
  expect overlay {g1,g2,p1}, renamed/commented, AND extended to pin that
  project `"mcpServers": {}` no longer erases globals.
- `ProjectWithoutMcpServersKeepsGlobal` and
  `EnvironmentLayerLeavesMcpServersUntouched` stay valid as-is.

## Shape hazards

- All MCP JSON keys emitted through serialize_server_object only; C4
  substring/order assertions keep applying to the hand-serializer output.
- New persistent strings: config.local.json (dynamic basename rule),
  user config.json under $LOOM_CONFIG_DIR/~/.loom, gitignore entry, the new
  warning text. Zero golden/fixture references to config.json (verified).
- No wire/trace exposure: to_native_mcp_server drops config_scope and
  disabled; runtime signature() excludes both.

## Test plan (tests/test_services.cpp; 1715 → ~1726)

1. UPDATE ReplaceGlobal → overlay {g1,g2,p1} + empty-object no-erase case.
2. Four-file precedence: user/project/local rows; same name in all four →
   local wins; legacy global still read.
3. upsert(User) writes only the user file; other files byte-identical;
   file contains exactly mcpServers (no model/display/features sections).
4. Default-scope Local: creates config.local.json; appends gitignore once
   (idempotent on 2nd write; copes with no trailing newline); no config.json.
5. Duplication guard (headline regression): global {g1} + local upsert
   {l1} ⇒ global untouched, no project file, reload {g1,l1}.
6. Remove: name in global + polluted project copy ⇒ default removes both;
   --scope touches one; unknown name ⇒ NotFound naming scope when scoped.
7. enable/disable patches only "disabled" on owner; unknown keys survive;
   lower tiers untouched; "all" patches multiple owners; global-only entry
   patches the global file.
8. Patched entry compared STRUCTURALLY against serialize_server_object;
   whole-file reformat tolerated; key order preserved.
9. **§A garbage files**: non-JSON (key=value lines) and malformed root in
   user AND local files → those tiers skipped with zero entries, lower/higher
   tiers load, no hard failure; upsert into an unparseable user file fails
   with the actionable error and leaves bytes untouched.
10. **$LOOM_CONFIG_DIR**: with the var set, upsert(User) lands there and is
    read back (4-arg-ctor tests can't cover this).
11. **Unwritable files**: read-only global containing the name + writable
    project copy: default remove still touches project and reports global as
    failed (not NotFound); all-unwritable-present → error, not NotFound.
12. **§B save-leak pin**: local entry with an Authorization header +
    /config-style full save to project ⇒ tracked file contains no trace of
    the local name/header; a name physically in project AND user stays in
    the project rewrite.
13. Services mcp_servers.json files are byte/mtime-untouched by core
    commands (double-load invariant).
Command-level --scope parsing in McpCommand stays uncovered (zero existing
test constructs McpCommand; no injection seam) — noted as pre-existing.

## Sizing / risk / gate

~200-250 LOC net config.cppm, ~80-100 mcp_cmd.cppm, ~450-550 test code
(~11 new cases + 1 flipped/extended). No CMake/module-rank changes. Medium
risk: persisted semantics, one deliberately flipped test, a new user-file
read with a compatibility policy (§A), and a secret-boundary rule (§B).

Gates: dual-preset -Werror, serial ctest -j1 (never concurrent with ninja),
graph_check --target-core8, inline_def_check, independent adversarial code
review, macos-14 decisive. No truecolor/UI golden impact (CLI text only;
`mcp list` rendering unchanged). Docs on landing: §12 row C6 (decisions
D1-D5, §A/§B policies), one bullet in docs/decisions/design-decisions.md
near the scope/path entries, and a CLAUDE.md configuration-paths addition
naming ~/.loom/config.json (user, $LOOM_CONFIG_DIR), .loom/config.local.json
(local, gitignored), and ~/.config/loom/config.json (legacy read tier).
