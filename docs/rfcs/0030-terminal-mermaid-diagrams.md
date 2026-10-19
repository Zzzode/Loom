---
rfc: 30
title: Terminal Mermaid Diagrams
status: implemented
owners: "@Zzzode"
reviewers: ["agent:/root/generation_review (design, PRR, implementation and graduation)", "agent:/root/settings_audit (parser admission)"]
created: 2026-10-19
last-reviewed: 2026-10-19
tracking: local RFC-0030 implementation history; user requested terminal Mermaid rendering
---

# RFC 0030 — Terminal Mermaid Diagrams

## Summary

Render supported Mermaid fenced code blocks as native Unicode terminal diagrams
inside the existing FTXUI Markdown renderer. Begin with common flowchart and
sequence-diagram syntax. Preserve the complete original code for unsupported,
invalid, incomplete, oversized or too-wide diagrams. Do not require an external
runtime, process, browser, network service or terminal image protocol.

## Motivation

At baseline 1cdec8cf, the production code-block dispatcher in
markdown_render_code_impl.cpp always passes mermaid fences to ordinary syntax
highlighting. There is no diagram parser or terminal layout. Its Markdown AST
also does not distinguish a closed fence from a streaming unfinished fence.
Existing FTXUI Markdown and streaming tests provide the native verification seam.
The baseline registers 3140 tests on both presets. The user explicitly requested
that model-returned Mermaid become an inline terminal diagram.

## Goals

- G1. Closed mermaid fences containing supported flowcharts or sequence diagrams
  render actual node boxes, connections, arrows and labels in the conversation.
- G2. Diagram direction, branching, back edges, message direction, node identity,
  edge labels, participant ordering and Unicode labels retain their meaning.
- G3. Unsupported syntax or graph types fail as a whole to the original source;
  never silently omit a declaration, edge, participant, control block or label.
- G4. Unclosed streaming fences keep source visible. Closing the fence renders
  the diagram; repeated updates, replacement, reset and terminal resizing work.
- G5. Width-aware layout never silently clips a rendered graph. A diagram that
  cannot fit switches to its original code with the existing code rendering
  behavior. Narrow terminals remain usable and later Markdown stays visible.
- G6. Keep ordinary code, HTML serialization, Markdown conformance, existing
  palette behavior and session/wire data unchanged.

## Non-Goals

Full Mermaid grammar parity, SVG/PNG export, image protocols, browser integration,
executing model text, scripts, clickable Mermaid links, remote fetching, new user
settings, graph editing, automatic prompt changes and cross-machine file sync.
Class/ER/Gantt/pie/mindmap/state/XY diagrams and advanced flowchart/sequence
features are not claimed as supported; the source fallback remains available.
Global Prompt investigation and any existing instruction-loading bug fixes are
separate work and must not expand this renderer's dependencies.

## Proposal

### Ownership and shared scope

The existing loom_ui_visual owner contains the feature. Add a small
loom.ui.visual.mermaid module with declarations only and an implementation unit
for parsing, bounded layout and an FTXUI Node. A dedicated module is justified
because the Markdown owner has no graph parser, graph layout or width-aware
source fallback. Keep these details out of Markdown's exported BMI. Only the
existing code-block implementation imports the factory, and the existing visual
CMake target owns its files. No global renderer, terminal protocol, theme or
cross-area service is introduced. This is the shared Markdown behavior directly
authorized by the user's request.

### Supported syntax and semantic fallback

Use a strict scanner/parser rather than regular-expression extraction of useful
fragments. Admit only syntax implemented end-to-end. Initial flowchart/graph
coverage includes TD/TB/BT/LR/RL directions, exact node IDs, ordinary rectangular,
rounded and decision labels, chained directed edges, labels and branches.
Sequence coverage includes explicit or implicit participants, aliases, ordinary
solid/dashed directed messages, leftward messages and self messages. Comments
may be ignored; any non-comment unsupported token rejects the entire diagram.
Quoted labels and CJK labels must be covered by tests. Styling, subgraphs,
activation/control blocks, directives and other unsupported constructs remain
source. Resource limits also return source; a best-effort partial graph is
forbidden. Final admitted grammar and limits must be documented before graduation.

The initial admitted grammar is closed as follows:

- IDs are ASCII `[A-Za-z_][A-Za-z0-9_]*`. Flow headers are `flowchart` or
  `graph` followed by exactly one of `TD`, `TB`, `BT`, `LR`, `RL`. Sequence
  headers are exactly `sequenceDiagram`. Require a statement after the header.
- Statements are separated by newlines or semicolons outside labels/quotes.
  Blank lines and full-line `%%` comments are allowed. `%%{` configuration
  directives are unsupported, including when otherwise comment-like.
- A flow statement is a node declaration or `node --> node` with optional
  `|label|` immediately after each arrow. Chains repeat that edge/node pair;
  branches use repeated statements. No `&` node groups, edge IDs or other edge
  operators are admitted. Nodes may be bare IDs, `ID[label]`, `ID(label)` or
  `ID{label}`, preserving rectangular, rounded and decision meaning.
- Labels are single-line plain Unicode. Flow labels may use whole-label double
  quotes, but quoted backslash escaping is unsupported and keeps source. Reject
  HTML delimiters, ampersands, hash/entity introducers, backticks/Markdown strings and control
  characters rather than interpreting rich label syntax incorrectly. Unquoted
  labels cannot contain nested delimiters or quotes. Empty labels are unsupported.
  Sequence alias/message text is plain and unquoted; quotes, backslashes and
  `wrap:`/`nowrap:` prefixes (including their leading-colon forms) keep source
  rather than reusing flowchart quoting semantics.
- Bare flow references may receive a later explicit label/shape. Identical
  explicit declarations are accepted; conflicting explicit declarations return
  source. Node identity is the ID, not its label. All statements must be fully
  consumed, including trailing text. The flow control words `end`, `subgraph`,
  `direction`, `style`, `linkStyle`, `classDef`, `class` and `click` are not
  admitted as node IDs; quoted label text such as `A["end"]` is allowed.
- Sequence declarations are `participant ID` or `participant ID as alias`.
  Messages are `ID->>ID: text` or `ID-->>ID: text`, including reversed endpoints
  and self messages. Participant order is first appearance, including implicit
  message endpoints; a later declaration may add an alias without moving it.
  Identical aliases are accepted; conflicting explicit aliases return source.
  No actor, activation, note, autonumber or control-block tokens are admitted.
  Sequence IDs also reject, case-insensitively, the reserved words
  `sequenceDiagram`, `participant`, `actor`, `as`, `create`, `destroy`, `box`,
  `loop`, `rect`, `opt`, `alt`, `else`, `par`, `par_over`, `and`, `critical`,
  `option`, `break`, `end`, `over`, `note`, `activate`, `deactivate`, `title`,
  `accTitle`, `accDescr`, `autonumber`, `off`, `links`, `link`, `properties`
  and `details`. These words remain allowed in ordinary displayed labels.

### Layout and FTXUI integration

Build a small internal graph representation, then compute a deterministic
terminal-cell canvas with bounded dimensions. Keep all connections, including
cycles and back edges, or return source when routing cannot be represented
unambiguously. Use Unicode cell widths from FTXUI, including wide/combining
labels, rather than byte counts. Allocate and route only within explicit bounds;
reject overlapping/ambiguous paths rather than hiding graph data.

The factory receives the ordinary code element as its fallback. A custom Node
uses the standard ComputeRequirement/SetBox/Check iteration to select a fitting
diagram or fallback and report the correct height. It must handle the same Node
being rendered wide, narrow and wide again. FTXUI owns clipping and screen
output; do not print escape sequences or bypass virtual scrolling. Diagram
colors inherit existing palette tokens and dim mode; no hardcoded RGB. Parsed
layout is owned by the rendered element; no process-global graph cache is added.

Append a code_fence_closed field defaulting to false at the end of BlockToken
and set it only in the
existing fenced-code parser. This is a host-only AST field, never persisted or
serialized onto the model wire. Preserve the CommonMark rule that an unclosed
fence is still a code block. The HTML serializer keeps its exact existing output.
Only a closed fence with the exact mermaid language marker attempts rendering.
There is no new BlockTokenKind and therefore no dispatcher enum-shape drift.

The existing Markdown fast-path must inspect the complete input, retaining its
early return at the first marker. A native regression reproduced that sampling
only the first 500 characters treats a fence after 600 plain characters as plain
text and bypasses the lexer entirely. Fix that local owner rather than adding a
Mermaid-specific detection workaround. Ordinary Markdown snapshots remain the
regression gate for this corrected marker scan.
Each candidate digit run is scanned once and the outer index advances past it
when no ordered-list marker is found, avoiding quadratic work on long numeric
input. Its following delimiter remains available to the next iteration.

### Streaming, errors and budgets

Incomplete syntax and incomplete fences retain original code without throwing.
Limit source to 64 KiB, flow nodes to 64, edges to 128, sequence participants to
32, messages to 128 and individual labels to 256 terminal cells. Limit the
canvas to 240 columns by 1024 rows, and bound routing work to a total of
1000000 visited/search/draw cells per block, not a fresh budget for each edge.
Check all limits before expensive allocation. No recursion proportional
to unbounded model input. Specific limits and native boundary tests are required
before implementation review. Failing parse/layout is an ordinary local render
choice, never a model retry, network request or logged dump of model content.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Native Mermaid | Visual parser/layout, code-fence integration, tests and docs | done | Flowchart and sequence semantic cases pass native renderer tests; direction/branch/cycle/CJK/unsupported/limits/stream/resize cases retain all data or source; real CLI PTY shows completed diagrams plus streaming/fallback states; 3140 baseline tests preserved with 35 additions; debug/release each cover 3175 tests with 3148 passed, 25 skipped, 2 disabled and zero failures; graph/RFC/inline/conformance gates and independent graduation approved |

## Production Readiness Review

- Correctness: new native renderer tests cover full examples and negative
  semantic cases, along with existing Markdown/conformance/streaming/goldens.
  Add a real CLI PTY scenario using the existing deterministic SSE fixture.
  Original Mermaid source remains in messages and request dumps. Existing
  screen-cell drag selection copies the visible Unicode diagram; the message
  Copy action continues to receive original Markdown. Verify both contracts
  without changing shared selection or copy behavior.
- Bounds: source, grammar and canvas/work limits are explicit and independently
  reviewed. No external execution, I/O, background worker or render ticker.
- Build: one existing visual target, declarations only in interfaces, implementation
  bodies in cpp. Default Ninja concurrency; no new third-party textual interface
  dependencies beyond FTXUI's existing allowlisted global-module-fragment headers
  (RFC 0001 OQ-1). The factory interface uses its Node/Element type seam; no new
  standard-library textual includes or runtime dependency. Run graph and inline checks. macOS local debug/release verification
  is available; Linux PSS and hosted CI are explicitly unverified locally.
- Compatibility: additive internal AST closure metadata; no session/config/wire
  schema changes. Ordinary code and HTML goldens should be byte-for-byte stable.
  New diagram snapshots are reviewed for arrows, topology and label visibility.
- Rollback: revert only Phase A's visual, AST, target and test/doc changes;
  leave the already pushed automatic model policy and global Prompt work intact.
- Documentation/deletion: document supported grammar and fallback boundaries in
  the existing Markdown guide; add the decision and RFC index. No dead renderer
  is replaced, no AGENTS/CLAUDE path/build conventions change.
- Metrics: record native test totals, real PTY frames, dual build results and
  independent review. No claims about unavailable CI or Linux PSS measurements.

## Rollout and rollback

Independent design and PRR approval precede feature implementation. Keep parser
and integration ownership disjoint. Reconcile additions and retain full native
logs. Commit/push authorization in this turn applies to the previously completed
model-default work; new work stays local unless the user explicitly extends it.
Revert this phase without migrating user settings or persisted conversations.

## Drawbacks

Unicode diagrams cannot match every Mermaid graphic or style. A strict subset
means valid advanced diagrams continue to appear as code. Dense graphs can be
wider than a terminal and therefore use source fallback. The native parser is
maintenance work, so its grammar must remain explicit and bounded; do not extend
it speculatively to imitate the complete Mermaid JavaScript implementation.

## Alternatives considered

1. Keep code fences: no maintenance but does not provide the requested diagrams.
2. Run Mermaid CLI/JavaScript plus a browser and image protocol: broad grammar
   support, but adds runtime installation, process lifecycle and terminal-specific
   behavior to a self-contained C++ terminal application.
3. Bundle a Go/Rust/TypeScript ASCII renderer: useful comparable layout designs,
   but an additional build/runtime toolchain and broad parser compatibility surface
   are disproportionate. No mature drop-in C++ library has yet been established.
4. Extract only recognized edges and skip the rest: deceptively loses graph
   meaning. Strict whole-input acceptance with source fallback is required.

### Comparable implementation evidence

The primary-source references checked on 2026-10-19 are
[Mermaid flowchart syntax](https://mermaid.js.org/syntax/flowchart.html),
[sequence syntax](https://mermaid.js.org/syntax/sequenceDiagram.html),
[Grok Build's native terminal implementation](https://github.com/xai-org/grok-build/blob/main/crates/codegen/xai-grok-markdown/src/mermaid.rs),
[mermaid-ascii](https://github.com/AlexanderGrooff/mermaid-ascii) and
[beautiful-mermaid](https://github.com/lukilabs/beautiful-mermaid).
The native Grok renderer demonstrates rank-based layout, barycenter ordering,
orthogonal routing with outside back-edge lanes and Unicode cell widths without
a JavaScript runtime. These are established layout techniques suitable for this
owner. Its permissive parser and truncation behavior must not be copied: Loom's
whole-input fallback keeps unsupported semantic constructs and labels intact.
Go and TypeScript implementations provide comparison evidence, not new Loom
dependencies. A small native renderer is justified by the lack of a verified
drop-in C++ library meeting the self-contained terminal constraint.

## Testing and verification plan

Use native Markdown renderer tests at multiple widths and styles; include
negative syntax, advanced features, Unicode labels, graph direction, cycles,
arrow labels, bounds, completed and partial streaming, cache/reset and adjacent
paragraphs. Add new exact snapshots without updating unrelated goldens. Real CLI
PTY captures show open-fence source, completed diagrams and fallback, with a
resize transition and stable following text. Keep CTest serial across presets;
historical streaming snapshots retain their fixture-model environment in a
separate selection. State the actual registration count after additions.

## Documentation impact

Markdown renderer guide, RFC index and design decision register. No public CLI,
config paths or platform protocol conventions change.

## Local validation results

Final frozen-source validation on macOS is recorded under the gitignored
`build/prompt-mermaid-validation-20261019/` directory. `validation.json`
records commands, counts, limitations and failure history;
`source-and-binaries.json` records 23 source/test/fixture hashes, four snapshot
hashes and 220 executable hashes across both presets. Independent review matched
every hash and reconciled both complete test-name sets.

- Debug and release Werror builds passed with default Ninja parallelism. Each
  serial suite covered all 3175 registered tests: 3148 passed, 25 skipped,
  two disabled, zero failed. The historical streaming groups used their fixture
  model environment in a separate 34-test selection; both selections were
  disjoint and their union exactly matched registration. No baseline test was
  removed; 35 tests were added across this feature and the separate global
  instruction fixes.
- Final focused verification passed 51/51 tests, including all 35 additions.
  Four new exact snapshots cover flowcharts, sequences, directions and cycles;
  no existing golden was updated.
- A real CLI PTY demonstrated open-fence source, completed flowchart, narrow
  source fallback, wide restoration, sequence rendering and unsupported-source
  fallback. Screen drag copied visible Unicode cells; message Copy retained raw
  Markdown. All three local-fixture HTTP requests contained exactly one global
  instruction sentinel.
- Architecture graph and inline-definition checks passed. RFC lint and whitespace
  checks passed; they are rerun after this completion record.

The initial new global-instruction test mixed two Result types and failed both
builds; branch-local result handling corrected it. The long-preface regression
failed before the Markdown scan fix. Independent review required stricter
semantic admission and a linear numeric scan. Superseded incremental builds were
interrupted and excluded from evidence; changed compilation inputs were
explicitly retriggered after the final source freeze before both successful
builds. Each final complete suite passed on its first frozen-source run.
Hosted CI, Linux and PSS measurements remain unverified. Supported Mermaid
grammar remains the strict subset above; personal instruction files remain
machine-local and were not created for the user.

## Open questions

No blocking question remains. Independent design, PRR, implementation, parser
admission and final graduation reviews approved the documented grammar,
layout, bounds and fallback behavior.

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-19 | — | Opened provisional | Local | Baseline 1cdec8cf; 3140 tests; production mermaid fences currently render as code |
| 2026-10-19 | — | Design/PRR requested grammar and copy corrections | Local | agent:/root/generation_review required closed admitted grammar and distinction between screen-cell selection and raw message Copy; cumulative routing work and default-false AST closure clarified |
| 2026-10-19 | — | Design and PRR approved after corrections | Local | agent:/root/generation_review approved strict grammar, copy contracts, resource bounds and visual ownership; no unresolved design blocker |
| 2026-10-19 | A | Implementation started | Local | Parser/layout owner separated from Markdown integration/tests; no external runtime or shared selection changes |
| 2026-10-19 | A | Independent semantic review required stricter admission | Local | agent:/root/generation_review and agent:/root/settings_audit identified Mermaid hash entities, bidi marks, reserved flow end, sequence quote/wrap semantics and unsupported flow escape decoding; retain whole source for these constructs and add regressions; no new syntax interpretation introduced |
| 2026-10-19 | A | Long-preface Markdown bypass reproduced and corrected | Local | Native DiagramAfterLongPlainPrefaceStillUsesMarkdownParser failed when the existing marker scan inspected only 500 characters; scan the complete source with early return, preserving the parser and ordinary renderers |
| 2026-10-19 | A | Full-scan numeric complexity correction | Local | agent:/root/generation_review identified repeated digit-run scanning after removal of the 500-character cap; advance past each scanned run and cover 64-KiB digits, numeric prefaces and following delimiters |
| 2026-10-19 | A | Implementation and parser admission approved | Local | agent:/root/generation_review approved the complete implementation after semantic and linear-scan corrections; agent:/root/settings_audit independently approved UTF-8, label, reserved-ID and resource-admission behavior |
| 2026-10-19 | A | Graduated; RFC implemented | Local, uncommitted | agent:/root/generation_review independently approved final evidence: debug/release each 3175 covered = 3148 passed + 25 skipped + 2 disabled, zero failed; 51 focused passed; four exact snapshots; real PTY streaming/resize/copy/global instruction evidence; every frozen source/snapshot/executable hash matched |
