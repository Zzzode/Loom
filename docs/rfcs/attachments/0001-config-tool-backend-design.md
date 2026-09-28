# RFC 0001 follow-up c13 — structured `config` runtime tool (design v2, post adversarial review)

Status: v2 incorporates an independent adversarial design review (REQUEST-CHANGES
on v1; blockers B1–B6 resolved below). Implementation split into TWO batches:
**c13a** pure patcher extraction (zero behavior change), **c13b** the tool +
salvage + port. Date 2026-09-28.

## Problem (unchanged)

The agent runtime tool `config` (register `runtime_registry_register.cpp:277`,
dispatch `runtime_registry_dispatch.cpp:70`, body
`runtime_registry_executors.cpp:246-273`) hardcodes `$HOME/.loom/config.json`,
appends raw `key=value` lines into the JSON on `set` (document corruption C6 §A
tolerates), and `get` never parses. No code consumes the appended lines.

## Corrected behavior map (review B1; verified in code)

The file is consumed by exactly ONE engine path: the headless direct-query
route `server_routes.cppm::execute_native_query()` builds a FRESH ConfigManager
per request and reads seven settings:
`model.default_model`, `model.max_output_tokens`, `model.temperature`,
`model.context_window_size`, `network.max_retries`, `model.extended_thinking`,
`model.thinking_budget`, plus `network.api_key`/`base_url` and
`permissions.deny_rules`. The interactive TUI engine (`main.cpp::load_config`)
reads ONLY env/CLI flags — it never reads config.json. `display.*`,
`network.timeout_seconds`, and `permissions.allow_bash/allow_file_write/
allow_network` have ZERO runtime consumers (UI-only/inert today).

Therefore the tool never promises "future sessions" blanket effects.

## Decisions (B1–B6 resolutions)

### D1 — v1 allowlist SPLIT into writable vs read-only metadata

**Writable v1 keys (7; all server-route-consumed scalars; none are secrets):**
| Key | Kind / validation | Consumed by |
|---|---|---|
| model.default_model | trimmed non-empty string | server route (requested_model fallback) |
| model.max_output_tokens | uint ≥1 | server route |
| model.temperature | number, 0 ≤ x ≤ 1, or null to clear (USER tier only; lower-tier value still wins) | server route |
| model.extended_thinking | bool | server route |
| model.thinking_budget | null to clear (USER tier only), else uint ≥1024 (Anthropic min) | server route |
| model.context_window_size | uint ≥1 | server route |
| network.max_retries | uint ≥0 | server route |

**CLOSED projected key set:** get-all and single-key get cover ONLY these 7
writable keys plus the 9 read-only metadata keys below; the set is closed in
both directions.

**Read-only metadata (projected, writable=false, consumes=[]):** the 9 keys
`display.show_thinking`, `display.show_token_usage`, `display.compact_mode`,
`display.theme`, `display.line_width`, `network.timeout_seconds`,
`permissions.allow_bash`, `permissions.allow_file_write`,
`permissions.allow_network` — get projects them; set returns a terminal
"not writable through this tool; no runtime component consumes this setting"
error (no retry bait), never silent success.

**Recognized but BLOCKED (specific guidance; credential bytes never
returned):**
- `network.api_key` / `network.base_url` / `network.proxy`: get returns
  PRESENCE ONLY `{key, set:bool, source:"env"|"file"|"none"}`, never the
  value; set rejected pointing to ANTHROPIC_API_KEY / ANTHROPIC_BASE_URL /
  the HTTPS-proxy env (`HTTPS_PROXY`, falling back to `HTTP_PROXY`). A test asserts the real secret/endpoint bytes appear
  nowhere in any tool response.
- `network.verify_ssl` (no env override; TLS-integrity kill switch),
  `permissions.deny_rules` + the four permissions arrays (managed via the
  mcp/allow permission surfaces), `systemPrompt`/`customInstructions`
  (session-prompt injection), `features` (raw bitmask), `mcpServers` (use
  the mcp tool / `loom mcp`), `xaaIdp` (`/mcp xaa`): set rejected naming
  the right surface; get is the generic unknown-key response (outside the
  closed projected set).
- Anything else: generic unknown-key error pointing to action=list.

Each projected/writable key carries:
`{key, type, writable, consumes: ["server-direct-query"] | [], source:
"env"|"file"|"default", env_var?: string, value?: <token>}`.
Source precedence is per ConfigManager tier overlay plus its
`apply_environment_variables` (LOOM_MODEL→default_model,
LOOM_MAX_TOKENS→max_output_tokens). The interactive resolver additionally
honors `ANTHROPIC_MODEL`/`ANTHROPIC_DEFAULT_SONNET_MODEL`
(`src/config/settings.cppm:55-65`) which ConfigManager does not model: for
model.default_model, when present, report an extra
`source_note: "ANTHROPIC_MODEL/ANTHROPIC_DEFAULT_SONNET_MODEL also override
interactive model resolution"`.

### D2 — env-shadow disclosure (B2)

`get` returns effective values with `source`. A `set` on a key whose effective
value currently comes from `env` still writes the user file (harmless; it
becomes effective when the env is absent) but returns
`{"set":..., "value":..., "path":..., "shadowed": true,
"shadowed_by": "LOOM_MODEL"}`; a subsequent `get` keeps showing the env value
with source=env. Effect wording in the description: "Writes the user
configuration used by the headless direct-query server (re-read on every
request). The interactive TUI resolves model/network settings from environment
variables and command-line flags only."

### D3 — silent load for the agent backend (B3)

Add a silent load entry to ConfigManager: `load_silent()` (or a load()
option) that performs the same tier reads but suppresses the §A
`std::println(stderr, ...)` diagnostics; the soft-failure state is exposed
only via `user_tier_unparseable()` and surfaced in the tool response as
`user_file_valid:false`. No tool-reachable path in the in-process TUI may
emit the §A warning (a per-call manager in the TUI would otherwise paint the
screen once per call). The CLI/startup path (`/mcp`, core loader) keeps the
audible warning. Test: junk user file + `config get` → zero stderr bytes.

### D4 — reload after patched write (B4)

After every successful patched write, `set_user_setting` internally calls the
normal `load()` again (silent variant) against the post-rename file rather
than hand-mutating bookkeeping. This recomputes settings_, owner maps,
physical-name sets, AND clears `mcp_tier_unparseable_[User]` when a salvaged
write made the file valid. Correctness is "identical to a fresh process view".

### D5 — strict-mode error pinning in the generic patcher (B5)

`patch_object_file(...)` takes the strict-failure Error from its caller:
the MCP wrapper passes the exact existing C6 literals/code
(`config.cppm:401-405`/`1378-1382`), guaranteeing message-identical salvage=OFF
behavior; C6 pinned tests are untouched. Blank/missing → `{}` in both modes;
`mcpServers` empty-key drop, local 0600/gitignore stay in the MCP wrapper.

### D6 — salvage state machine honest about total replacement (B6)

`repaired` becomes a tri-state string:
`null` (strict load or fresh/blank), `"trailing_junk_dropped"`
(parse_first found a complete leading **OBJECT**; that object + all its keys
are preserved and only trailing bytes are dropped — yyjson RETAINS duplicate
keys on read, so a duplicate-key object with trailing junk takes this path,
not a failure), `"replaced_unparseable"` (NO leading complete OBJECT — this
covers a leading complete non-object value such as `[1,2]` plus junk,
malformed objects (trailing comma/comments), and pure junk: the whole file is
replaced with `{}` plus the new leaf; non-object content can't be recovered).
`yyjson_mut_obj_put` replaces the first twin and deletes later twins of the
SAME key; canonicalization is therefore guaranteed only for the patched
path — an untouched duplicate-key sibling section may survive (documented;
pinned by a test for the patched-key case). Response includes the state.

### D7 — value coercion details (advisory 2)

Use int64/double JSON accessors (not the int-truncating json_int helper):
uint fields reject negatives/non-digits/overflow; temperature bounds [0,1];
thinking_budget 0 = null/clear else ≥1024; bools exactly true|false; enums
exact match case-sensitive; empty default_model rejected. Native JSON
true/number request values are accepted and normalized. Duplicate-key
on-disk files: yyjson obj_put replaces the first twin (documented); add a
test pinning that we write a canonical single-key result.

### D8 — input schema honesty (advisory 4)

The registration currently declares only `action`; `key`/`value` are NEW
props. Declare all three as in v1 with value described as accepting string
(or native bool/number); replace the tool description with D1/D2 effect
wording + "API keys, endpoints, TLS, permissions, prompts, MCP servers are
not writable with this tool."

## Architecture (unchanged, with precedent correction)

New `config` slot on `cc.tools.runtime_backends.port` (rank-8 contract leaf),
concrete `config_backend` in new PRIVATE impl unit
`src/orchestration/runtime_backends_config.cpp` (`module
cc.orchestration.runtime_backends;`), installed in the existing
`install_runtime_backends()` call_once B15-sextet pattern (NOT the separate
MCP core-settings loader — that lives elsewhere); path/env resolved inside
the per-call lambda; `make_config_backend()` exported inline (like
make_image_codec) so the null-slot test can reinstall. cc_tools gains no
links; cc_orchestration already links cc_config. cc_tools deletes
config_path()/execute_config_tool + <cstdlib>; dispatcher uses the slot with
the standard fail-closed text.

Security note: file-tool writes into ~/.loom are otherwise hard-blocked
(path_validation.cppm:781-796); this tool is the deliberate Write-gated
channel across that boundary, justified by the 7-key scalar allowlist that
excludes credentials/endpoints/TLS/deny-rules/prompts/MCP/xaaIdp.
Concurrency: fixed .tmp no-lock writes are now reachable from concurrent
per-connection server agents as well as CLIs (documented lost-update limit,
no torn writes).

## Batch split (advisory 9)

- **c13a — patcher extraction, zero behavior change**: extract
  `patch_object_file` with salvage parameter (MCP passes salvage=OFF + its
  own Error); `patch_mcp_file` delegates; all C6 tests/bytes unchanged; no
  tool changes. Gates: 1735 green, refactor-only.
- **c13b — the feature**: ConfigManager spec/source/silent-load/
  set_user_setting + reload; slot + orchestration backend + registration +
  schema; salvage=ON. Tests below. Each batch independently reviewed.

## Tests (c13b)

test_services: writable-key round-trips of all 7 kinds incl. temperature
bound/clear and thinking_budget 1024/clear; read-only keys set-rejected and
get-projected with writable=false/consumes=[]; unknown/malformed keys;
env source + LOOM_MODEL shadow bit; silence on junk user file (CaptureStderr
0 bytes); sibling/unknown-key/mcpServers preservation; tri-state salvage
(trailing junk dropped, leading non-object `[1,2]`+junk replaced, trailing-comma total replacement, junk-only replaced); blank; mode
preservation; LOOM_CONFIG_DIR routing; only-user-file-touched; post-salvage
reload clears unparseable flag (subsequent §A behavior matches fresh process);
duplicate-key canonicalization.
test_tools registry end-to-end: get-on-missing creates no directory and
returns structured defaults; set typed values verified by real load;
invalid/unknown/readonly are is_error; env routing; junk repair response
states; list payload; null-slot fail-closed + reinstall via make_config_backend.

## Risks

Moderate agent-facing change: output raw-text→JSON (input shape compatible;
no documented consumer; old scraping never worked); get can surface hard
global/project load errors; server-route per-request reload means writes
affect subsequent requests in the same server process (disclosed, not
claimed as future-only); fixed-tmp concurrent-write lost updates; pretty
formatting normalized on salvage.

## Implementation precision notes (second review round)

- **load_silent is a load() OPTION, not a parallel routine**: e.g.
  `load(std::optional<LoadOptions>)`/a quiet flag, so tier logic never drifts;
  only the agent backend passes quiet. The existing audible MCP core-settings
  loader keeps current behavior (out of scope).
- **`source` provenance mechanism (pinned)**: during the four-tier merge,
  record the set of leaves seen on disk (section/leaf names) into a small
  member set; env wins are the two known apply_environment_variables
  assignments (LOOM_MODEL/LOOM_MAX_TOKENS) plus direct env checks for
  ANTHROPIC_API_KEY/base_url presence; classification = env if an override
  is engaged, else file if the leaf was seen, else default. This avoids
  mislabeling a file value equal to the default.
- **Post-write reload can hard-fail on a corrupt global/project tier even
  though the user write succeeded**: the set RESPONSE still succeeds with
  path/repaired plus a `"reload_warning": "<manager error text>"` field; the
  write is never rolled back.
- **c13a must IMPLEMENT the salvage branch, not stub it** (an unused
  parameter / unbuilt branch trips -Werror or rots); it is simply unreachable
  until c13b lands in the same review window.
- **Null-clear scope**: setting/temperature/thinking_budget JSON null clears
  only the USER-tier leaf; a lower-tier value still wins via merge — stated
  in the list response.
