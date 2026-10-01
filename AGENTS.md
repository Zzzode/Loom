# AGENTS.md

Guidance for AI agents (Claude Code, and any tool that reads AGENTS.md) when
working in this repository.

## What this is

**Loom** — a C++23 CLI agent harness. A single self-contained project: no
TypeScript, no Bun, no npm. The tree was ported from a TypeScript codebase that
has since been deleted; `docs/decisions/design-decisions.md` records the design
intent that used to live there.

## Build and test

```bash
cmake --preset debug      # configure (also: release, asan)
cmake --build --preset debug -j8
ctest --preset debug -j$(nproc)

# or, the whole loop at once:
cmake --workflow --preset dev     # configure + build debug
cmake --workflow --preset ci      # configure + build + test release
cmake --workflow --preset check   # configure + build + test asan
```

Configuring without a preset (or an explicit `CMAKE_TOOLCHAIN_FILE`) is a
`FATAL_ERROR` by design: C++23 named modules need a matched `clang++` /
`clang-scan-deps` pair, and a silent fallback would be hard to diagnose.
AppleClang is rejected outright — it ships no `clang-scan-deps`.

Known timing-sensitive flakes: `ServerMain.DirectConnect*` (tests 706/708) use
real TCP sockets with 3-second timeouts and can fail under high parallel load.
If they fail, re-run them in isolation — they pass alone.

### Building on this Linux dev box

The committed presets target the macOS CI runner and will not configure here.
Use the machine-local presets (gitignored, because they hold absolute paths):

```bash
cmake --preset local-linux-clang23            # debug (LLVM 23)
cmake --preset local-linux-clang23-release    # release (LLVM 23, -O2)
cmake --build --preset local-linux-clang23 -j8
ctest --preset local-linux-clang23 -j$(nproc)
```

The `local-linux` / `local-linux-release` presets pin Homebrew LLVM 22 and are
kept for reference; `local-linux-clang23*` use Homebrew LLVM 23.1.2, which
fixes two LLVM 22 defects:

- **LLVM #184957** (operator new ambiguity with textual libc++ + `import std;`)
  — fixed by PR #179178. All 13 impl units that kept textual std headers as a
  workaround now use `import std;`.
- **Optimizer SIGSEGV** — the LLVM 22 optimizer crashed on this tree at `-O2`,
  forcing `-O0 -DNDEBUG` for release. LLVM 23 builds and tests clean at `-O2`.

Both presets point at the offline dependency cache, use system (not brew)
OpenSSL/curl headers, and link against brew's glibc 2.38.

**If configuration fails with a FetchContent download error**, `.deps-cache/`
is missing or incomplete. It holds the six pinned dependency archives; this box
has no github.com access, so they cannot be re-fetched. `CMakeLists.txt` probes
for `.deps-cache/<dep>-src/` automatically — no flags needed when it is present.

## Architecture

**Modules, not headers.** The tree is C++23 named modules (`export module
loom.<area>.<thing>;`), built with `-fmodules-reduced-bmi`. Two consequences
worth internalizing:

- **A module's name does not have to match its path.** `export module
  loom.ui.design.tokens;` can live in `ui/design/tokens.cppm`. Moving a file
  therefore usually needs only a `CMakeLists.txt` path update — not an import
  rewrite. This is what makes directory restructuring cheap here.
- **Importers are found by module name, not filename.** To check whether a
  module is used, grep for `import loom.area.thing;`, and cover `tests/` too.

**Build layout.** `src/CMakeLists.txt` holds global/project setup and, in
dependency order, one `include()` per target pulling a file from
`src/cmake/targets/`. `include()` (not `add_subdirectory()`) is deliberate: it
keeps every target in one CMake scope, so the variables and the tree-sitter
conditional at the top are visible throughout — `add_subdirectory()` would add
a directory scope and change evaluation order. When adding a target, add its
file under `src/cmake/targets/` and an `include()` in the same dependency order.

**`loom_ui` is an INTERFACE aggregate over ~12 area libraries
(`loom_ui_foundation` … `loom_ui_app`), split out of the former single target
by RFC 0002 F4.** The module graph is a DAG (217 ui
modules, zero module-level cycles). At the responsibility-directory level
(`foundation`, `dialogs`, `messages`, …), nine areas *used to* collapse into
one strongly-connected component (`foundation ↔ chrome`, `dialogs ↔ widgets`,
`messages ↔ prompt`), which blocked a library split — but RFC 0002 F2
dissolved those SCCs (`graph_check.py --target-ui9` now PASSES with 12
singleton areas), so the area-level graph is acyclic and the split is
cycle-free. Two caveats from the single-target era still hold:

- **Interface edits fan out via the BMI graph, not the library boundary.**
  Touching `design_tokens` (a `.cppm`) still forces ~62 module recompiles
  regardless of how the archives are split. The split only contains `.cpp`
  *body*-edit fan-out: each target has its own `CXX.dd` dyndep file, so a
  body edit recompiles only that area's objects, not the whole closure. Do
  not split expecting interface-edit speedups.
- **The single FILE_SET let clang-scan-deps resolve intra-`loom.ui.*` imports
  both ways.** The split uses per-target FILE_SETs with cross-area BMI
  propagation via `target_link_libraries`; this must keep clang-scan-deps
  resolving cross-area imports (verified by the dual-preset build).

### Layout

| Path | What |
|---|---|
| `src/query/` | **The engine.** `query_engine.cppm` owns the streaming loop, tool-call loop, thinking mode, retry. Start here for anything about model interaction. |
| `src/query/wire_*.cppm` | The wire-protocol seam. `wire_protocol.cppm` defines `WireBackend`; `wire_anthropic.cppm` and `wire_openai.cppm` implement it. The engine builds a vendor-neutral `RequestInput` and never serializes a wire format itself. |
| `src/tools/` | Tool implementations, each with its input schema, permission model, and execution. |
| `src/commands/` | Slash commands. Registered via `command_registry_init_*.cpp`. |
| `src/ui/` | FTXUI interface. **Not Ink, not React** — do not port React idioms into it. Cut by responsibility: `foundation/` (tokens, theme, figures, primitives), `chrome/` (layout, renderer, terminal I/O), `widgets/` (reusable controls), `visual/` (markdown/diff rendering), `messages/`, `dialogs/`, `permissions/`, `prompt/`, `screens/`, `features/{agents,teams,tasks,plugins,mcp}/`, `tools/` (tool-UI registry), and `app/` (the top-level app orchestrator shards). Twelve `loom_ui_<area>` targets aggregated by the `loom_ui` INTERFACE library — see the build-layout note above. |
| `src/services/` | External integrations: MCP, LSP, API clients, plugins. |
| `src/state/` | AppState store and reducers. |
| `src/constants/paths.cppm` | **The single source for config/memory path resolution.** Both cascades live here; delegate to it rather than hardcoding paths. |
| `benchmarks/pare/` | Benchmark case data. The `pare-benchmark` binary reads the JSON by CWD-relative path. |

### Configuration and data paths

    config dir   $LOOM_CONFIG_DIR > ~/.loom > ~/.agents > ~/.claude   (read)
                 $LOOM_CONFIG_DIR > ~/.loom                          (write)
    memory file  LOOM.md > AGENTS.md > CLAUDE.md   (per directory, walking up)

Read follows the cascade; **write never does.** Writing into another tool's
config directory would interleave two tools' state. Both cascades are
implemented in `src/constants/paths.cppm` — eight call sites previously
reimplemented the lookup and silently drifted.

MCP server config (`loom mcp add/remove/enable/disable`) lives in FOUR
physical JSON files, highest precedence first; same-named entries overlay
per entry across the files, and `--scope` patches exactly one file in place:

    local    <project>/.loom/config.local.json  (gitignored; derived next to the project file)
    project  <project>/.loom/config.json         (VCS-tracked)
    user     $LOOM_CONFIG_DIR/config.json, else ~/.loom/config.json
    global   ~/.config/loom/config.json          (legacy READ tier; written only to remove pollution)

A non-JSON/key=value user/local file is tolerated (zero entries + one
warning); a full save to the project file never copies user/local-only
entries — which may carry Authorization headers — into VCS.

### Wire backends

`wire_api` config key or `LOOM_WIRE_API` env selects the backend; unset ⇒
Anthropic. Credentials reach the wire through the single decision point in
`query/wire_anthropic.cppm` (Bearer when a token is set, else `x-api-key`).
`ANTHROPIC_API_KEY` / `ANTHROPIC_AUTH_TOKEN` are read as user-supplied
credentials for the user's own endpoint — there is no account system and no
login.

## Debug traces

Every session persists two traces, both under the storage dir
(`~/.loom/sessions/`):

- `~/.loom/sessions/<session_id>/messages.jsonl` — the full conversation: every
  message appended to the engine, with tool-use/tool-result/text/thinking
  blocks. One JSON object per line.
- `~/.loom/dump-prompts/<session_id>.jsonl` — the API request body (messages
  array, system prompt, tool schemas) and the parsed response.

Wired in `src/ui/app_constructor.cpp` via `set_session_storage` /
`set_dump_prompts_dir`. Use them to diagnose duplicate tool-call rows, missing
tool output, and wrong tool status. See `.agents/skills/debug-session/SKILL.md`.

## Conventions

- **All code comments and docs in English.**
- **Reviews are agent-run, never user-run.** Every review in this project —
  code review, design review, RFC stage gates, production-readiness review,
  approve/request-changes — is performed by Claude agents, not the user. Do
  not ask the user to read a diff, judge a design, or "sign off"; the user is
  informed of outcomes, never assigned review work. For a consequential gate,
  spawn one or more independent (adversarial where it matters) review agents,
  address or explicitly rebut their findings, and record the agent identity
  as the reviewer. Proceed through gates on the agents' verdict.
- **`-Werror` is on** (`-Wall -Wextra -Wpedantic`). New warnings fail the build.
- **No hardcoded RGB** — use palette/design tokens (`src/ui/design/`).
- **No constant-frequency render ticker** — the FTXUI UI is event-driven.
- **FTXUI components must be held by state**, not reconstructed per render.
- **Every change builds debug *and* release, with ctest green**, before committing.
- Prefer deleting dead code to fixing it.

### A hazard specific to this codebase

Cross-module couplings are often **string- or shape-based**, so a change on one
side silently breaks the other rather than failing to compile. Examples:

- `tools/runtime_registry.cppm`'s `parse_lsp_action` mirrors
  `lsp_action_name()` in `lsp_tool.cppm`; a mismatch silently falls through to
  `LspAction::Symbols` — a *wrong answer*, not an error.
- The `<task_notification>` / `<status>` / `<summary>` tag format is produced by
  three modules (`local_agent_task`, `local_shell_task`, `runtime_registry`) and
  consumed by `ui/messages/collapse_background_bash.cppm`; changing the
  emitters' spelling silently stops the collapsing.

`docs/decisions/design-decisions.md` catalogues these — consult it before
changing a wire shape, a registry key, or a tag format.

## Historical documents

`docs/` holds audit reports and plans written while the TypeScript reference
tree still existed. They contain paths that no longer resolve, and are kept
unedited as records of what was found. `docs/README.md` says which are current.
