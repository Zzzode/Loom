# Loom

A C++23 terminal agent harness.

Loom is a CLI that runs an LLM agent loop against a model endpoint of your
choosing: streaming responses, tool calls, permissions, sub-agents, and an
interactive FTXUI interface. It is a single self-contained C++23 project — no
TypeScript, no Bun, no npm.

## Status

Under active development. It builds and its test suite passes on Linux
(Homebrew LLVM 23) and macOS (CI); it has no releases and no stability promise.

## Features

- **Streaming agent loop** — model responses stream in real time, with a
  tool-call loop, thinking mode, and retry.
- **Interactive FTXUI terminal UI** — markdown rendering, diffs, dialogs,
  vim-style keybindings, themes, and a status line.
- **Headless mode** — run without a UI for daemon, remote, or scripted work
  (`--headless`, `--input-format`, `--output-format`).
- **Direct-connect server** — an HTTP/WebSocket server mode for remote
  sessions (`--server`).
- **MCP client** — connect to Model Context Protocol servers over stdio, SSE,
  and HTTP transports.
- **Sub-agents and teams** — spawn teammate agents, manage agent configs, and
  run multi-agent workflows.
- **Git workflow commands** — `/commit`, `/review`, `/security-review`,
  `/branch`, `/diff`, and PR automation.
- **Session persistence** — conversations are persisted and resumable
  (`--continue`, `--resume`, `/session`).
- **Multi-wire backends** — speaks the Messages API and OpenAI wire formats
  through a vendor-neutral seam; the engine never serializes a wire format
  itself.
- **C++23 named modules** — the entire tree is built as C++23 named modules
  with `import std;`.

## Building

### Prerequisites

- **CMake ≥ 3.28** and **Ninja**.
- **Clang ≥ 23** with a matching `clang-scan-deps`. C++23 named modules need a
  matched `clang++` / `clang-scan-deps` pair; AppleClang does not qualify (it
  ships no `clang-scan-deps`). LLVM 22 is blocked by two bugs: an operator-new
  ambiguity (LLVM #184957) and an optimizer SIGSEGV at `-O2`.
- **Ninja** (the default generator).

Configuring without a preset (or an explicit `CMAKE_TOOLCHAIN_FILE`) is a
deliberate `FATAL_ERROR` — a silent fallback would pick a toolchain that
cannot build this tree.

### On macOS (CI configuration)

The committed presets target the macOS CI runner:

```bash
cmake --preset debug
cmake --build --preset debug -j8
ctest --preset debug -j$(nproc)
```

### On Linux

The same presets work — `AutoToolchain.cmake` auto-detects Homebrew LLVM on
Linux (`/home/linuxbrew/.linuxbrew/opt/llvm`) and configures libc++, glibc,
and system OpenSSL/curl paths automatically. No machine-local presets needed.

```bash
cmake --preset debug
cmake --build --preset debug -j8
ctest --preset debug -j$(nproc)
```

### Offline builds

Dependencies are fetched at configure time. On a machine with no network
access, place the six pinned archives under `.deps-cache/<name>-src/` and
CMake will use them instead — it probes for that directory automatically.

### Workflows

```bash
cmake --workflow --preset dev     # configure + build debug
cmake --workflow --preset ci      # configure + build + test release
cmake --workflow --preset check   # configure + build + test asan
```

## Running

```bash
./build/clang23-debug/bin/loom                    # interactive REPL
./build/clang23-debug/bin/loom --headless "hi"    # one-shot, no UI
./build/clang23-debug/bin/loom --help             # all flags
./build/clang23-debug/bin/loom --version          # version
```

### Common flags

| Flag | Description |
|---|---|
| `--model <model>` | Set the default model |
| `--continue`, `-c` | Continue the active persisted conversation |
| `--resume [id]` | Resume a persisted conversation by ID |
| `--headless` | Run without UI for daemon/remote work |
| `--simple-ui` | Use simple text UI (not interactive) |
| `--debug` | Enable debug logging |
| `--server` | Start the direct-connect HTTP/WebSocket server |
| `--server-port <port>` | Port for `--server` (default 3000) |
| `--permission-mode <mode>` | Parent permission mode for this session |
| `--dangerously-skip-permissions` | Bypass permission prompts |
| `--agents <json>` | JSON object defining custom agents |
| `--settings <file\|json>` | Load settings from a JSON file or inline JSON |
| `--list-runtime-tools` | Print registered runtime tool names and exit |
| `--run-runtime-tool <name>` | Execute one runtime tool non-interactively |

Run `loom --help` for the full list (45 flags).

### Pointing at an endpoint

```bash
LOOM_API_KEY=... LOOM_BASE_URL=... ./build/clang23-debug/bin/loom
```

An OpenAI-compatible endpoint works too — set `LOOM_WIRE_API=openai` (or the
`wire_api` config key) and point `LOOM_BASE_URL` at it. Loom speaks both
wire formats through a seam that `src/query/wire_protocol.cppm` defines; the
engine itself is format-agnostic.

Credentials reach the wire through a single decision point: Bearer auth when a
token is set, else `x-api-key`. `LOOM_API_KEY` / `LOOM_AUTH_TOKEN`
are read as user-supplied credentials for the user's own endpoint — there is
no account system and no login.

## Configuration

### Config and memory paths

```
config dir   $LOOM_CONFIG_DIR > ~/.loom > ~/.agents   (read)
             $LOOM_CONFIG_DIR > ~/.loom                (write)
memory file  LOOM.md > AGENTS.md > CLAUDE.md   (per directory, walking up)
```

Read follows the cascade; **write never does** — Loom creates its own state
under `~/.loom` rather than writing into another tool's directory. Session
transcripts land in `~/.loom/sessions/`, API dumps in
`~/.loom/dump-prompts/`.

### Settings file

The `--settings` flag loads settings from a JSON file path or inline JSON. It
is the highest-priority source and supports `env` (process env vars, e.g.
`LOOM_API_KEY`/`LOOM_BASE_URL`), `apiKey`, `model`, `theme`,
`permissions`, `mcpServers`, `hooks`, and `statusLine`.

### MCP server configuration

MCP servers are configured with `loom mcp add/remove/enable/disable` and live
in three physical JSON files, highest precedence first:

```
local    <project>/.loom/config.local.json  (gitignored)
project  <project>/.loom/config.json         (VCS-tracked)
user     $LOOM_CONFIG_DIR/config.json, else ~/.loom/config.json
```

Same-named entries overlay per entry across the files; `--scope` patches
exactly one file in place. A full save to the project file never copies
user/local-only entries — which may carry Authorization headers — into VCS.

## Slash commands

The REPL has 90+ slash commands. The major ones:

### Core

| Command | Description |
|---|---|
| `/help` | Show available commands and usage |
| `/clear` | Clear the screen and optionally reset conversation |
| `/compact` | Compress conversation context to free token budget |
| `/cost` | Show cost breakdown for the current session |
| `/model` | Switch or display the active model |
| `/config` | View and modify CLI configuration |
| `/doctor` | Run system diagnostics |
| `/exit` | Exit Loom |

### Sessions and context

| Command | Description |
|---|---|
| `/session` | Manage conversation sessions |
| `/resume` | Resume a previous conversation |
| `/add-dir` | Add a working directory |
| `/files` | Show all files currently in context |
| `/context` | Show and manage context window |
| `/memory` | Edit persistent memory files (LOOM.md) |
| `/rewind` | Restore code and/or conversation to a previous point |
| `/export` | Export the current session |

### Git and review

| Command | Description |
|---|---|
| `/commit` | Generate a conventional commit and stage changes |
| `/diff` | Show file changes made in the current session |
| `/branch` | Manage git branches |
| `/review` | Review code changes with AI |
| `/security-review` | Security audit (OWASP Top 10 + secrets) |
| `/pr` | Pull request management |

### Agents and planning

| Command | Description |
|---|---|
| `/agents` | Manage agent configurations |
| `/tasks` | Manage tasks |
| `/plan` | Enter plan mode (read-only) |
| `/ultraplan` | Ultraplan command |
| `/advisor` | Configure the advisor model |

### UI and customization

| Command | Description |
|---|---|
| `/theme` | Switch visual theme |
| `/color` | Set the prompt bar color |
| `/vim` | Toggle vim-style keybindings |
| `/keybindings` | View keybindings |
| `/statusline` | Configure the status line |
| `/permissions` | Manage tool permissions |

### MCP and integrations

| Command | Description |
|---|---|
| `/mcp` | Manage MCP servers |
| `/hooks` | View hook configurations |
| `/skills` | Manage skills |
| `/plugin` | Manage plugins |
| `/ide` | Detect running IDEs and their MCP endpoints |

Run `/help` inside the REPL for the full list, or `loom --list-runtime-commands`.

## Architecture

Loom is C++23 named modules throughout. A module's name need not match its file
path, so moving a file usually needs only a `CMakeLists.txt` update — not an
import rewrite.

```
src/query/       the agent engine and the wire-backend seam
src/tools/       tool implementations (bash, file ops, MCP, agents, …)
src/commands/    slash commands
src/ui/          FTXUI interface (12 area libraries)
src/services/    MCP, LSP, API clients, plugins
src/state/       AppState store and reducers
src/constants/   config/memory path resolution (single source)
benchmarks/pare/ benchmark case data
```

The engine (`src/query/query_engine.cppm`) owns the streaming loop, tool-call
loop, thinking mode, and retry. The wire-protocol seam
(`src/query/wire_protocol.cppm`) defines `WireBackend`; `wire_messages.cppm`
and `wire_openai.cppm` implement it. The engine builds a vendor-neutral
`RequestInput` and never serializes a wire format itself.

For contributors — build details, conventions, the cross-module coupling
hazards this code is unusually prone to, and the debug-trace locations — see
`AGENTS.md`. Design intent inherited from the original TypeScript
implementation is recorded in `docs/decisions/design-decisions.md`.

## Documentation

- [`AGENTS.md`](AGENTS.md) — contributor guide: build, architecture,
  conventions, hazards. (`CLAUDE.md` is a symlink to it.)
- [`docs/README.md`](docs/README.md) — docs index: what is current, what is
  historical.
- [`docs/decisions/design-decisions.md`](docs/decisions/design-decisions.md) —
  design decisions and cross-module couplings. Read this first.
- [`docs/rfcs/0001-module-architecture-target.md`](docs/rfcs/0001-module-architecture-target.md)
  — the module-architecture RFC (implemented).
- [`docs/rfcs/0002-ui-state-sharding-and-ui9-break.md`](docs/rfcs/0002-ui-state-sharding-and-ui9-break.md)
  — the UI state-sharding RFC (implemented).

## License

Not yet chosen.
