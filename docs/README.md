# Loom documentation

Loom's documentation is organized by audience and purpose. This file is the
index — start here to find what you need.

## Structure

| Directory | Audience | What goes there |
|---|---|---|
| [`user/`](user/) | End users | Getting started, configuration, slash commands, permissions |
| [`dev/`](dev/) | Contributors | Build/test guides, architecture deep dives, conventions |
| [`design/`](design/) | Contributors | Design documents and rationale (why the system is shaped this way) |
| [`rfcs/`](rfcs/) | Contributors | RFCs — proposals for future architecture work (KEP-style, lint-checked) |
| [`decisions/`](decisions/) | Contributors | Decision registers — append-only logs of non-obvious decisions |

## Naming conventions

| Category | Convention | Example |
|---|---|---|
| RFCs | `NNNN-kebab-case.md` (4-digit zero-padded) | `0001-module-architecture-target.md` |
| RFC attachments | `NNNN-kebab-case.md` (parent RFC number) | `0001-cc-sdk-rename-design.md` |
| All other docs | `kebab-case.md` | `markdown-conformance.md` |
| Root files | `UPPERCASE.md` | `README.md`, `AGENTS.md` |

Rules: kebab-case everywhere (no PascalCase, no spaces, no underscores). No
milestone prefixes (`M7-…`) — the milestone is metadata, not a filename.

## Current docs

### Developer guides (`dev/`)

| File | What |
|---|---|
| [`dev/markdown-conformance.md`](dev/markdown-conformance.md) | CommonMark 0.31.2 + GFM conformance suite: architecture, fixtures, HTML serializer, floor system, JSON output + dashboard. Baseline: 1350/1354 (99.7%). |
| [`dev/error-handling-conventions.md`](dev/error-handling-conventions.md) | `std::expected` / `Result<T>` error-handling conventions for the C++ tree. |

### Design documents (`design/`)

| File | What |
|---|---|
| [`design/dialog-system.md`](design/dialog-system.md) | Dialog framework architecture: queue, priority arbitration, two modes, migration. |
| [`design/why-no-di.md`](design/why-no-di.md) | ADR #1: why this tree does not use a runtime DI container. |

### Decision registers (`decisions/`)

| File | What |
|---|---|
| [`decisions/design-decisions.md`](decisions/design-decisions.md) | ~800 extracted comments recording decisions, non-obvious constraints, and cross-module couplings. **Read this first** when changing a wire shape, registry key, or tag format. |

### RFCs (`rfcs/`)

| File | Status |
|---|---|
| [`rfcs/0001-module-architecture-target.md`](rfcs/0001-module-architecture-target.md) | **Implemented.** Target module architecture: declaration-only interfaces, `import std;`, breaking UI9/Core8 SCCs, dissolving `loom.utils`, UI state sharding. |
| [`rfcs/0002-ui-state-sharding-and-ui9-break.md`](rfcs/0002-ui-state-sharding-and-ui9-break.md) | **Implemented.** Sever the 9-area UI SCC via type sinks, registry inversion, and `ReplScreenState` sharding; split `loom_ui` into ~12 acyclic libraries. |
| [`rfcs/0003-streaming-payload-replay-testing.md`](rfcs/0003-streaming-payload-replay-testing.md) | **Implemented.** Streaming payload replay testing: JSONL event fixtures, replay harness, golden snapshots, invariant checker, clock seam for grace-period tests. |
| [`rfcs/0004-session-recording-for-replay.md`](rfcs/0004-session-recording-for-replay.md) | **Implementable.** `loom record` command: capture live API sessions as JSONL replay fixtures compatible with the RFC 0003 harness. |
| [`rfcs/0005-property-based-fuzz-testing.md`](rfcs/0005-property-based-fuzz-testing.md) | **Implementable.** Property-based fuzz testing: generate random valid streaming event sequences, run through the invariant checker (INV-01–07). |

RFC process and stage gates: [`.agents/skills/rfc/SKILL.md`](../.agents/skills/rfc/SKILL.md).
Attachments (phase plans, edge inventories, design docs): [`rfcs/attachments/`](rfcs/attachments/).
