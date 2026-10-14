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
| [`dev/ui-glossary.md`](dev/ui-glossary.md) | Canonical UI vocabulary: framework terms (Element/Component/Box/Screen) vs project terms (Block/Row/Panel/Pane/Store/…), deprecated terms. |

### Design documents (`design/`)

| File | What |
|---|---|
| [`design/dialog-system.md`](design/dialog-system.md) | Dialog framework architecture: queue, priority arbitration, two modes, migration. |
| [`design/why-no-di.md`](design/why-no-di.md) | ADR #1: why this tree does not use a runtime DI container. |
| [`design/design-language.md`](design/design-language.md) | Terminal hierarchy, readable tokens, activity height, brand/status identities and motion boundaries. |

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
| [`rfcs/0006-design-system-governance.md`](rfcs/0006-design-system-governance.md) | **Provisional.** Design system governance: token lint, hard-coded color sweep, transparent canvas + OSC-11 auto theme, surface vocabulary, emoji/motion policy. |
| [`rfcs/0007-native-text-selection-and-clipboard.md`](rfcs/0007-native-text-selection-and-clipboard.md) | **Implementable.** Reliable copy pipeline (four-state feedback, terminable async, SSH-aware), Historical native-selection default revised by RFC 0010; original reliability and lifecycle rationale retained. |
| [`rfcs/0008-virtual-renderer-threshold-hysteresis.md`](rfcs/0008-virtual-renderer-threshold-hysteresis.md) | **Implementable.** Hysteresis band for the static↔virtual message-list renderer threshold (enter > 80, exit < 60) to prevent mode oscillation at the boundary. Field flicker benefit pending PTY verification (§11.4). |
| [`rfcs/0009-optional-interactive-tool-round-budget.md`](rfcs/0009-optional-interactive-tool-round-budget.md) | **Implemented.** Optional interactive tool-round budgets with committed-progress continuation; no implicit interactive limit. |
| [`rfcs/0010-in-app-selection-copy.md`](rfcs/0010-in-app-selection-copy.md) | **Implemented.** Default in-app selection automatically copies with accurate inline feedback; OSC ownership is refined by RFC 0011. |
| [`rfcs/0011-common-osc-protocol.md`](rfcs/0011-common-osc-protocol.md) | **Implemented.** Centralize Loom-owned OSC encoding and separate protocol, terminal output, platform capabilities and UI. |
| [`rfcs/0012-loom-prompt-and-chrome.md`](rfcs/0012-loom-prompt-and-chrome.md) | **Implemented.** Cell-aware prompt wrapping, responsive built-in statusline and Loom-only welcome artwork. |
| [`rfcs/0013-readable-bounded-brand-ui.md`](rfcs/0013-readable-bounded-brand-ui.md) | **Implemented.** Readable secondary text, bounded task/team chrome, duplicate/unused token deletion and Loom brand coordination. |
| [`rfcs/0014-compressed-chain-activity.md`](rfcs/0014-compressed-chain-activity.md) | **Implemented.** Stable stage elapsed time and bounded current activity for compressed chains. |

RFC process and stage gates: [`.agents/skills/rfc/SKILL.md`](../.agents/skills/rfc/SKILL.md).
Attachments (phase plans, edge inventories, design docs): [`rfcs/attachments/`](rfcs/attachments/).
