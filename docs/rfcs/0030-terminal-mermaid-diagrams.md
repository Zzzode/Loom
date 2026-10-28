---
rfc: 30
title: Terminal Mermaid Diagrams
status: implementable
owners: "@Zzzode"
reviewers: ["agent:/root/generation_review (Phase A design, PRR, implementation and graduation)", "agent:/root/settings_audit (Phase A parser admission)", "agent:/root/corpus_review (Phase B design, PRR, implementation and graduation; Phase C2 bounded design/PRR)", "agent:/root/corpus_oracle (Phase C1 inventory/runtime promotion and C2 implementation review)", "agent:/root/er_group_design_review (C2 ER subgraph design and PRR r1)", "agent:/root/requirement_design_review (C2 Source4 identity and mutation-plan review)", "agent:/root (C2 Pie owned admission and Source4 integration review)", "agent:/root/style_design_review (Phase D design draft, two revision rounds, approved)", "agent:/root/style_feasibility_review (Phase D implementation feasibility, two revision rounds, approved)", "agent:Turing (State brace and ninety-percent milestone)", "agent:Carver (State source/metadata amendment)", "agent:Parfit (Mindmap10 bounded source/design/PRR, three rounds)", "agent:Aristotle (Class endpoint retry code/scope)", "agent:Newton (Class20 original-source/frame preservation and E6c gap amendment)", "agent:Beauvoir (Mindmap10 implementation and dual-consumer admission boundary)", "agent:Galileo (Shapes4 source/design/PRR, private implementation, independent proof ownership and final dual admission)", "agent:Harvey (Block58 B1 complete-source, direct-theme census and partial B2 source-derived facts/provenance only)", "agent:Mill (Block58 owner-local architecture and proportionality only; B2 pending)", "agent:Harvey (Block58 full B2 design/PRR and dormant private payload static review)", "agent:Harvey (Block58 production implementation, B3 independent Native/Official proof ownership and strict dual-join code review)", "agent:Harvey (ER107 B1 source, B2 source/projection semantics, bounded test witness and complete B2 design/PRR)", "agent:Popper (test-facade privacy correction, strict Block marker refresh code, real refresh and postcheck)", "agent:/root/review_er_fix_design (ER107 bounded compaction and ranked-topology limited design/PRR trial)"]
created: 2026-10-19
last-reviewed: 2026-10-28
tracking: local RFC-0030 implementation history; user requested terminal Mermaid rendering
---

# RFC 0030 — Terminal Mermaid Diagrams

## Summary

Render supported Mermaid fenced code blocks as native Unicode terminal diagrams
inside the existing FTXUI Markdown renderer. Begin with common flowchart and
sequence-diagram syntax. Preserve the complete original code for unsupported,
invalid, incomplete, oversized or too-wide diagrams. Do not require an external
runtime, process, browser, network service or terminal image protocol.

### ER107 default-budget repair design (2026-10-28)

This bounded repair replaces the unreviewed standalone rail-planner proposal;
it does not change the immutable terminal contract or admit new drawing credit.
The owner remains `loom.ui.visual.mermaid`. The 1,000,000 Work limit, existing
per-operation/storage/output fees, public factories and source fallback remain
fixed. Work exhaustion always rejects the whole source without reset or refund.

The first branch reuses the final linear placement after the existing ranked
plan cannot fit. It applies only to active ER presentation, root TB, no compound
scope or notes, a complete flat set of plain unaliased boxes without attributes,
members or annotations, an even entity count, degree one at every entity and
exactly half as many relationships as entities. For adjacent ordered entities,
map their IDs into the existing adjacency matrix; either directed adjacency
means one pair and retains the existing gap. Unrelated pairs use one blank row;
the last item has zero following gap. Compute the complete height as
`18 + sum(box.height + boundary_gap)` and validate all bounds before moving
boxes. Allocation and painting continue through the existing Canvas and A*.
The six all-relationship-type originals are the coordinate-change cohort,
including four previously successful sources; both categorical variants must
pay the unchanged output fee from their actually smaller body area.
`agent:/root/review_er_fix_design` approved a limited production trial after
requiring bidirectional pair lookup, a single height/placement gap rule and
explicit whole-source rejection on Work exhaustion.

The second proposed branch reuses the existing ranked-peers layout for active
ER flat root-TB plain DAGs with a rank-skipping edge or repeated directed pair.
It removes only those two existing eligibility exclusions. The adjacency table
continues to represent topology; every original relationship remains present
for independent routing, labels and cardinality. No path, theme, ordinal or
exact entity/edge-count fingerprint selects the branch. A source-model census
finds thirteen newly eligible failed originals. The existing two- and
three-entity parallel controls have no peer rank and retain their linear plan;
the six disjoint-pair originals have neither skip nor repetition.

Only a successful, bounded rank-plan commit sets an active-ER private render
scratch flag. That flag selects stable descending physical edge length in the
existing edge-order copy, with source-order ties. Other scenes retain ascending
order. A rank plan that cannot fit leaves boxes unchanged and falls through to
the existing linear path. A committed plan's route failure remains atomic whole
source rejection, without a second free layout attempt. New scans, comparisons,
scratch initialization and writes are paid by the same live Work counter;
legacy families receive no new storage or Work fees.
`agent:/root/review_er_fix_design` approved this second branch for a limited
B2 production trial after requiring the flag to follow a successful rank-fit
check and preserving every original relationship and source-order sort tie.

The first generic peer-gap contraction was rejected as a universal rule: its
C++ factory budget trial repaired the three repeated-pair sources but rejected
all ten long-span sources. Preserve label-aware peer spacing for long-span-only
graphs; only repeated-pair-only candidates use marker-lane peer spacing.
Label-halo-only and rank-cut-congestion vertical-gap candidates also rejected
the long-span scenes before Work exhaustion. Neither spacing trial remains in
production; no cut-scanning infrastructure was retained.

The selected skip-only repair preserves all body/rank gaps and measured box
sizes. Its local rank candidate uses the existing root minimum margin of 12
instead of edge-count-driven exterior padding, and `2*top` vertical workspace
padding instead of 18 (`top` is 4 for this flat root). Fit and committed canvas
bounds use those same local values. The common linear margin, padding and gap
are untouched. This reduces actual temporary Canvas, semantic-plane and A*
backing storage/scan costs, without discounting any per-cell or output fee.
A smaller exterior can alter the available route choices; complete existing
A*/label validation still proves successful geometry or rejects the whole
source. Mixed repeat-and-skip graphs retain both original gaps and workspace.
`agent:/root/review_er_fix_design` and `agent:/root/review_runtime` independently
approved this narrower workspace trial. The Release factory now accepts all
107 originals, each with exact-used success and one-unit-short rejection;
Native/Official and full-preset gates remain separate requirements.

Acceptance uses the existing native C++ all-107 same-factory budget test,
including exact-used success and one-unit-short rejection, then independent
Native and Official fifteen-frame captures, full geometry/style/cache/mutation
checks, OfficialMatrix, Debug/Release full CTest and real PTY Mermaid execution.
Source census and temporary diagnostics are scope records, not regression
coverage. Formal ER107 admission remains a separate fail-closed review/join.

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
- G8. Port complementary upstream terminal test cases into native Loom tests,
  retain their source-declared semantics, and improve Loom-owned parsing/layout
  without embedding another renderer. Count missing/unsupported drawing cases
  as unmet capability, never as successful drawing.
- G7. Pin official Mermaid fixtures, distinguish faithful terminal drawing from
  intentional source fallback and errors, and measure capability without
  presenting fallback or transformed diagnostic inputs as full compatibility.

## Non-Goals

Full Mermaid grammar parity, SVG/PNG export, image protocols, browser integration,
executing model text, scripts, clickable Mermaid links, remote fetching, new user
settings, graph editing, automatic prompt changes and cross-machine file sync.
The completed Phase A/B grammar does not claim Class/ER/Gantt/pie/mindmap/state/XY
or advanced flowchart/sequence support. Phase C uses independently declared
upstream terminal test obligations to expand that grammar; unsupported cases
remain visible capability gaps until their semantics actually render.
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
  Font Awesome icon introducers and unsupported slash/backslash shape or ellipse
  openers also keep source rather than displaying literal markup or rectangles.
  Sequence alias/message text is plain and unquoted; quotes, backslashes and
  `wrap:`/`nowrap:` prefixes (case-insensitive, including their leading-colon forms) keep source
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

### Phase C continuation: complete frozen-suite obligations

#### Provisional ninety-percent milestone (2026-10-25)

The user's current milestone is more than 90% correct native terminal drawing
of the frozen **2,145 official complete original files**: at least **1,931**
admitted originals (90.02%). The initial baseline was **596/2,145 (27.79%)**,
requiring 1,335 additions. The verified count is now **688/2,145 (32.07%)** after
Block58, leaving **1,243** further admissions. The **881** passing
Mermaid-labelled tests and 93 source-bound assertion-site lower bound are
separate regression/semantic measures; neither is this milestone's numerator.
Source fallback, body-only diagnostics, unreviewed art and verification errors
continue to earn zero drawing credit. The complete upstream semantic target
remains recorded; this milestone does not assert full upstream compatibility.

At milestone creation, ten matrix types across nine implementations contained
1,608 originals (74.97% theoretical ceiling); at least 323 of the other 537
were required. With Mindmap and Block owned, twelve matrix types across eleven implementations
contain **1,699** originals (**79.21% type-based ceiling**). At least **232** of
the other **446** originals are still required, even if every owned-family
original eventually draws; cross-type Flow inputs require separate accounting. Owning a family does not admit its pending inputs. Hard-cap sources can remain whole-source, but must stay in the fixed
denominator and consume the remaining 214-file gap allowance.

Execution stays inside `loom.ui.visual.mermaid`, its native tests and source
facts. First close source-bound grammar/layout cohorts in existing families;
in parallel select complete unsupported-family cohorts with independent typed
Models and faithful terminal projection contracts. Every new-family grammar,
config whitelist or layout projection needs its own source-backed design/PRR
review before implementation; this numerical milestone does not authorize
silently ignoring config, HTML, styles, layout semantics or original options.
RFC 0031 E2b remains deferred until its explicit whitelist/precedence/presentation
amendment passes review. No external renderer or runtime dependency is added.

A cohort advances the milestone only after both owning Native and Official
consumers validate complete original sources at 240 columns, narrow fallback
and restoration, complete geometry and meaningful good-first semantic/physical
mutations in Debug and Release. Recompute the full matrix and reviewed gap
roster in the same change; explain every changed outcome, preserve every old
drawing or resolve the regression, and update the current docs. Work/canvas/
source bounds remain fixed unless a separately reviewed measured design
justifies changing them. Existing dirty work remains uncommitted; no milestone
or RFC stage is marked complete from projections or selected-input counts.

#### Block58 complete-family source and design proposal (2026-10-26)

This is the next unsupported-family workstream toward 1,931 originals. The
scope is all **58** complete frozen Block inputs, not a stripped-body or
configuration-free replacement suite. The verified starting point is
**630/2,145**; this proposal adds zero drawing credit. agent:Ramanujan independently
created `tests/fixtures/mermaid_native/block58_complete_source_proposal.v1.json`:
58 sources/24,239 bytes, 279 unique leaf identities, 60 composites, 18 edges,
31 spacer declarations/42 expanded slots and four implicit endpoints. Its source
lock is `eb887a461f7194023f7622fb823af3d02d5cb17bb2decf19c4b2b22bf204a877`
(relative-to-`block/` UTF-8 path, NUL, lowercase SHA hex ASCII, LF). Source facts,
2,887 byte/line witnesses and canonical hashes passed agent:Harvey
(`01a17482-8709-7f80-b5c3-81928144ac07`) independent B1 source-only review.
The reviewed proposal SHA-256 is
`c8ef03ea9fd3be874f44a946ce2481bf51e9b1f85eda2aff0fdc210c3c2f0c27`;
root-only review metadata produces asset SHA-256
`1d0006ab73cf61951a15599606cde2e46a59865416ac9f43ad244d45b0e4c27f`.
Every nested canonical model/witness and terminal-null field is unchanged.
Nested proposal review statuses remain the immutable proposal snapshot; the root
review qualifies their source-only acceptance. Terminal projection and B2 remain
pending, and this checkpoint authorizes no production implementation.

The Block source inventory includes 22 theme/look-bearing originals, 32 nested
sources, eight edge-bearing sources, 14 spacer-bearing sources, ten span-bearing
sources, 42 explicit-shape sources, seven style-bearing sources and ten repeated-ID
sources. These overlapping categories do not change the denominator. Every
source statement, source config value and first-placement/merge consequence must
be retained. Completing a smaller easy subset is not Block58 graduation.

A separate conservative direct-key audit is
`tests/fixtures/mermaid_native/direct_theme_source_audit.v1.json`
(reviewed proposal SHA-256
`63b48be5b021d9dca8068185dadcc96e8e2133adfd10de073c7333aed1b80168`;
root-only review metadata SHA-256
`ef2c6b4ae5ce1b5defe5e1da99a941bc76bbb73c619db611bcd06f7cfb6964b8`).
It binds the 630 matrix and full manifest to exact byte witnesses for 498
unquoted `config.theme` keys exactly two spaces below a top-level `config:`;
it excludes quoted/inline keys and init directives. Of these, 58 currently draw,
434 retain source and six are verification outcomes. Leaving those **440**
unadmitted caps even optimistic total admission at **1,705**, so at least
**226** require new admission to reach 1,931. agent:Harvey independently approved
this source census and arithmetic only; they provide no rendering or theme credit. Browser appearance
cannot be bypassed merely to inflate this count; the exact Block projection below
needs independent source/design/production-readiness approval.

The independently authored terminal-facts proposal is
`tests/fixtures/mermaid_native/block58_terminal_projection_proposal.v1.json`
(SHA-256 `b7d0bf628af004718df6ab3550dd09bb0a71481bc2f70b0cf3e47b2584870caf`).
It binds all58 current source/model/witness/manifest components and freezes415
raw/display caption records,381 logical placements and549 source-witness
references. Two authored single-space captions retain one cell without trimming;
two entity-bearing captions have four single-pass named-entity replacements.
All actual captions fit32 cells; no runtime wrapping/Unicode success is inferred.
Full per-property source styles,44 metadata notices,47 style-loss notices,
20 slot legends and11 source-style declaration legends are proposed separately
from still-null contours, physical geometry, routes and style planes. Main's
read-only hash/reconstruction and finite-schema consistency checks pass, but are
not native test evidence. agent:Harvey approved the partial B2 source-derived
facts/provenance after correcting a stale whole-RFC snapshot binding and a
subset-versus-full-array digest defect. Eleven current-contract groups/thirteen
exact segments and six quotations now bind substantive RFC sections independently
of mutable review/history paragraphs. The full-array digest is
`9c8c2b608640c284e0a4ea6332f1e0616f6f71309b2cb5ec13e55ab90920ebda`.
All58 source/Model bindings and49 style atoms remain exact. This partial verdict
does not approve contours, base fill, Work/geometry, consumers or production;
agent:Mill's saved terminal design remains UNREVIEWED and full B2 is pending.

##### Source semantics to preserve

Pinned Mermaid commit `1fb7869866dd619168d840291c9c54b4f129758b`, rather than a
fixture's English description, defines Block source meaning. Several descriptions
are materially misleading:

- The actual layout reads `columns ?? -1`. Negative/default columns place
  children horizontally at `(position,0)`; physical newlines are not grid rows.
  Positive N uses `(position % N,floor(position/N))`. After placing a full-span
  child, advance by `min(span,N-position%N)` for positive N, otherwise by span.
  Do not shrink an overflowing child, wrap it before placement or let overflow
  skip later rows. `getColumns()` is a different DB helper and is not called by
  this placement algorithm. Scope defaults are not inherited from a parent.
- A first authored ID owns placement, parent, span and arrow directions.
  A repeated declaration updates only non-`na` type and non-ID caption under
  the pinned DB merge; it does not create a second node or update every field.
  All occurrences stay separate witnesses. `ExampleOther:2` then `:1`, and
  `blockArrowId6` first `(y)` then `(x,down)`, must retain the first span/directions.
- `block:ID[:span]` names a composite; bare `block` opens an anonymous composite.
  Thus `block BL` contains an ordinary child BL, and an edge to BL targets that
  child. `block:2` names composite `2` with default span1. Do not implement the
  contradictory prose interpretation. Composite captions default to empty.
- Spacers expand from their own source width field into ordered blank slots,
  separate from ordinary node `widthInColumns`. Edge endpoints can introduce
  implicit ordinary nodes, but an existing ID keeps its original parent/order.
- Source shape type follows exact delimiter concatenation and pinned
  `typeStr2Type`, including single-slash forms. The witnessed set is `na`, square,
  round, circle, hexagon, diamond, stadium, lean_right, subroutine, trapezoid,
  rect_left_inv_arrow and block_arrow. Do not infer shape from a filename.
- Composite-only global preorder indices exist independently of the theme.
  Categorical palette stamping is enabled only for `redux-color` and
  `redux-dark-color` with their actual source palette, not every redux variant.
  Plain nodes and the synthetic root take no slot; direct style remains last.

##### Closed Block-local metadata and style amendment (proposed)

This proposes a local exception to D4 and RFC0031 E2b, not a global config tier.
Until approved, D4/E2b's existing admission boundaries remain unchanged. Legacy
Class/ER/Sequence whitelists and their already reviewed source contracts are not
broadened or retroactively reclassified by this work.

| Authored Block path/form | Bounded values | Required terminal treatment |
|---|---|---|
| description | Existing reviewed plain/quoted/folded source metadata | Preserve unchanged body and complete raw witness; not an in-diagram title. |
| config.theme | neo, neo-dark, redux, redux-dark, redux-color, redux-dark-color only | Preserve exact selected value. Project categorical enablement and composite slot order for the two color variants; map source palette hue roles to existing Loom tokens. Explicitly disclose browser RGB/brightness not reproduced. Other four variants have no categorical cycle. |
| config.look | neo only | Explicit non-terminal notice: SVG gradients/relief are not applied. It cannot alter authored content, grid placement or shape type. |
| classDef/class/style | Witnessed Block grammar and valid ID resolution only | Resolve complete source first; direct style overrides class properties. No unresolved target/class, implicit default class, unknown property or silently dropped assignment. |
| fill/stroke/color | Witnessed #rgb/#rrggbb hex forms only | Parse source hue and map to the already designed D1 token roles; no hardcoded RGB. Source literal colors and collapsed token distinctions are disclosed. Apply fill/stroke on witnessed node/composite masks; on filled labels use the D1 readable text-token rule and disclose non-projected source color. |
| stroke-width | Witnessed `2px` or `4px` only | `2px` has normal border; `4px` has bold border attribute, with explicit pixel-width loss notice. Other widths retain complete source; zero must never become a visible normal border. |
| all other keys/forms | Unsupported | Complete original source, with no admission. No init/themeVariables/fonts/CSS parser is added. |

All accepted non-terminal settings produce a bounded visible note naming the
source key and value; source-literal loss is not merely a ledger field. Color
variants additionally retain the exact upstream palette slot identity (source
palette length, including wrap) in a bounded container/slot legend so a collapsed
terminal hue cannot conceal a reordered or missing slot. Apply direct style after
palette/class resolution per property. Source RGB/CSS/SVG assertions remain
explicit foreign debt; this proposal does not declare them passed. Independent
review must judge whether the complete source-owned terminal equivalent supports
original-file admission, rather than assuming that a notice alone is sufficient.

##### Terminal grid and drawing contract (proposed)

Use the source logical placements above. For each scope, physical column extent K
is the largest used `(column+span)`, including explicit spacer slots, not an
unconditionally reserved N columns. Measure intrinsic children bottom-up, including
nested containers and their padding. With gap3 and border/interior inset2, a
scope's minimum unit width is the maximum checked ceiling of
`(child_min_width - (span-1)*gap)/span`, at least the witnessed shapes' minimum.
Natural scope width is `K*unit+(K-1)*gap`, plus four for an outlined composite.
Root has no outline. Span overflow remains full width and increases the actual
canvas/container extent; it never clips a child or changes the logical row.

Allocate scopes top-down. An inherited composite width first removes four inset
cells and `(K-1)*gap`; split the remaining width into K integer column widths
with the earliest columns taking any one-cell remainder. Column widths differ
by at most one and remain shared across rows. A span covers the sum of its
columns plus internal gaps. Empty explicit spacer cells preserve exactly the
same allocated width and row height as visible slots. Per-row height is the
maximum child height; center children vertically in that row, separating rows
by gap3. Nested groups get full closed outlines and complete child containment.
This is a terminal sizing equivalent, not upstream browser pixel dimensions.

Do not call the existing cropping `Canvas::lines()` on the final Block grid.
Leading/trailing spacer columns and blank spacer-only rows are semantic and must
survive into full-width row strings and the node's width requirement. A note may
widen the outer element without stretching the grid's columns. Prefix and suffix
paragraph sentinels in Native/Official captures establish the Markdown block
origin and full vertical extent; a checker must not infer the origin by trimming
to the first ink and thereby erase leading spaces. Separate boundary-width
controls prove blank trailing slots affect fallback even when no glyph marks them.

All captions are complete. Freeze raw-to-display entity decoding and literal
rows before production code; escaped `&lt;tagname&gt;` remains literal displayed
angle text and is not parsed again as HTML. Use the reviewed 32-cell word/glyph
wrapping rule where required, preserving explicit joiners, wide glyphs and
combining tails. Source shape masks must be complete and distinguishable.
Stepped circle/ellipse bounds may be even when required by equal grid allocation;
no odd-width rule may silently distort otherwise equal columns. Precise contour,
interior and port tables for every witnessed shape, including directional block
arrows, remain a design gate before implementation.

Edges retain source identity, order, labels, start/end markers, solid/dotted and
normal/bold distinction. Route orthogonally in reserved gaps/spacer lanes without
crossing unrelated leaf nodes or labels. A descendant-to-outside route crosses
container boundaries only through declared reciprocal gates; a gate is not an
edge endpoint and must not retarget the edge to its containing group. Normal,
point, circle and cross endpoint treatments and label ownership need explicit
source-backed tables. Cached palette roles must not turn a normal route into an
unobservable thick-route equivalent; capture and validate the bold attribute.

The concrete geometry proposal is
[the Block58 terminal contract](../design/mermaid-block58-terminal-contract.md),
a saved UNREVIEWED draft. Main's first review required source-equivalent contour
corrections: `na` and `square` both select the source rectangle (an
invented double frame is not justified), the asymmetric source shape needs its
actual inward-notch authority, and directional block arrows need complete head
polygons rather than a rectangular frame with a replaced side glyph. Rounded
rectangle and stadium curvature must be specified wherever the source shapes
are observably distinct. Exact irregular masks/ports, all58 resource feasibility,
annotation sufficiency and cache regressions remain pending. The draft now
corrects the shape topology, normalizes arrow bounds and uses bounded digital
edge tracing/caption-mask search, but it still needs independent fixtures and
feasibility. Its zero-apron, non-sharing route rule cannot serve all three edges
from a top-left A with only two outward ports. A one-cell root routing apron and
a three-route hand fixture are recorded only as a not-adopted next candidate;
source-grid sizing, corridor/gate rules and full58 resource costs still need
B2 approval before any implementation. No production change follows from merely
saving a finite-looking contour table.

##### Private ownership, cache, Work and graduation

The owner remains `loom.ui.visual.mermaid` in `loom_ui_visual`. Add family-local
parser/layout/style code and append the Block probe enum without renumbering old
values. Keep other families' paths, resource fees, glyphs and surfaces unchanged.
No global theme provider, UI component, terminal protocol or external runtime is
modified. Reuse the existing width-aware MermaidNode for whole-source fallback.
Style-aware row assembly may use bounded contiguous text runs with foreground,
background and border-bold decorators; it must retain whole FTXUI glyphs.

A styled Block diagram cannot pass through a cache that retains only strings
and drops attributes. Extend the private immutable layout payload with optional
palette-independent style runs/planes, keeping exported factory and
`LayoutRowsCache::get_or_layout()` signatures and row/sentinel semantics intact.
Cache no Element or concrete active-palette colors. The private cache core must
serve payloads once on misses; both row-only access and fresh-element `render()`
share exact-key lookup/LRU/copy/eviction semantics. Existing row byte_size/capacity
meaning stays unchanged. New style backing must have a separately checked fixed
bound proportional to retained row bytes; no unbounded side map, second cache,
per-render relayout or post-hoc search through rendered captions is acceptable.
A fresh Element maps cached roles through the current palette once, preserving
palette/width flips without relayout. This owner-local shared change is necessary
because strings cannot carry the source's style obligations; bypassing the cache
or embedding ANSI/Elements would violate established contracts.

agent:Mill (`01a17483-b5a1-79f1-92ba-09a2f3b9df7a`) approved the
architecture/proportionality of the following owner-local approach, explicitly
without B2/PRR or implementation clearance. The architecture correction is one
private `LayoutPayload` consumed by
BOTH uncached `render_mermaid` and cached `render()`, never a style-only bypass.
Use `rows` plus an optional packed `uint16_t` per terminal cell. Bits0..4 select
foreground role, bits5..9 background role, bit10 border/route bold; upper bits are
zero and unknown/reserved role codes reject the whole payload. An empty plane is
the exact legacy all-inherited-attributes case. For a styled payload, plane length
is the sum of the rows' FTXUI cell widths, including blank cells and continuation
slots. Retain only this palette-independent plane, not run objects or Colors.

The explicit private used-storage constant is **K=2**: plane bytes must be at most
`2*row_bytes` per entry and at most `2*total_retained_row_bytes` globally, plus
fixed vector headers bounded by entry_capacity. This follows only after verifying
UTF-8/glyph round trips and full blank-cell-preserving rows; the byte/cell bound is
checked before retention. Track style used-bytes with the same LRU lifetime and
subtract/reset on eviction, clear and assignment. Deep copy duplicates rows and
plane and rebuilds the exact-key index. A collision that cannot be retained serves
its OWN newly computed payload uncached; it must never look up the occupied slot
again and borrow its style. Rejection always has empty rows AND empty plane.

`get_or_layout()` projects rows from this one private payload lookup and preserves
the engaged-empty rejection sentinel. `render()` and uncached rendering consume
the full payload through the same finish function, with one classification and
one remaining Work; old family paths do not pay a second probe or new plane fee.
Do not call the public row-only getter then reparse/search for styles. No FTXUI
Element, string view into an LRU entry or concrete theme color survives in cache.

Only at fresh-element construction, derive contiguous runs from whole FTXUI
glyphs and the plane. Each run is nonempty, ordered, source-byte-exact and constant
in all attributes; concatenating its byte strings must reproduce that entire row.
A wide glyph includes both slots, including a nonempty combining tail. All slots
must have identical style or the entire payload rejects; never split or drop one.
This includes background-bearing spaces, not just nonblank glyphs. At most one run
per cell is possible. Prepay a conservative `R*(512+8*sizeof(std::string))` and
`H*(256+4*sizeof(ftxui::Element))` for run strings/vectors/FTXUI nodes, in addition
to actual copied glyph bytes, before returning a newly admitted styled layout.
RLE construction owns every string and maps roles through one current-palette
snapshot per fresh element. Cache hits do not relayout; their bounded node/run
construction has the same proven R/H limits as the admitted immutable payload.
The packed backing is prepaid as two planes (retained data plus return/copy) in
the family raster stage, with checked `cells*2*sizeof(uint16_t)` before allocation.
Precise parser/grid/route/contour fees remain the separate B2 PRR gate.

Atomic style parsing is mandatory: resolve every property/value/class/target and
per-property priority before painting any admitted output. Any unsupported or
ambiguous component rejects the whole source. Layer order is base diagram,
composite categorical properties, class properties, then direct style properties;
source indices are never removed by overrides. Existing D1 hue thresholds and
token mapping are the semantic color table, not `resolve_color()` or literal RGB.
The pinned palettes give the following source-derived candidate role sequences:

| Source palette | Slots0..11 |
|---|---|
| Both categorical border arrays | Purple, Cyan, Orange, Cyan, Green, Purple, Red, Yellow, Blue, Green, Blue, Red |
| redux-color background array | Purple, Cyan, Orange, Cyan, Green, Blue, Red, Yellow, Blue, Green, Blue, Red |
| redux-dark-color background array | Empty: NO categorical fill override; keep base/direct fill. |

Slot5 deliberately has different light background and border hue roles. Do not
invent dark background entries or cycle colors for the other four theme names.
Slot identity remains modulo the actual source border array length12; the visible
slot legend preserves identity where multiple source hues share a Loom token.
These are proposed terminal mappings, not new source-array facts or approval of
foreign RGB assertions. B2 still must approve their exact readable base/label
fill policy and per-shape masks before implementation.

Required cache regressions cover BOTH cache and uncached paths with their own
five public captures and complete glyph/foreground/background/bold comparison;
wide/combining boundaries, styled blank spaces, exact source/ASCII key, rejection,
copy/assignment, eviction, clear, width restoration and palette flips. Existing
collision policy needs either an existing fixed collision fixture or explicit
independent control-flow review plus focused exact-key/collision-path evidence;
no global mutable hash hook may be added merely for this cohort. A control-flow
review is not an executed collision regression: without an actual fixed-collision
input, retain explicit implementation evidence and report that runtime gap.
Preserve every
previous630 source/model/frame disposition and all593 diagnostic rows. Palette
notices alone never substitute for the applicable categorical/style proofs.

Keep the same remaining 1M Work, 64-KiB raw source, 240x1024 canvas, at most64
entities/groups/expanded spacers, depth16 and per-caption 1024-byte/256-cell bounds.
Prepay source/event scans, container/ID/parent resolution, label/glyph/row storage,
all checked grid arithmetic, contour/route visits, full blank-cell-preserving
raster, style backing/runs, label/legend copies and final UTF-8 serialization.
No allocation/write precedes its stage's checked prepayment; resource exceptions
are not geometry negatives. Precise fees and per-shape tables remain PRR gates.

The proposed Block-only Work schedule is allocation-based, with checked add/mul
before every charge and before allocating or mutating its output. S is raw source
bytes, D accepted statement/declaration events, N all placed entities INCLUDING
composites and expanded spacer slots, E edges, P style properties, G caption glyphs,
H serialized rows, A full canvas cells and R actual fresh-element runs. These are
counts, not configurable limits. Source/probe charges already spent by the caller
are never reset or refunded. Failure produces whole-source rejection.

| Stage | Prepayment and subsequent charged visits |
|---|---|
| Lexical bounds | Before scratch storage, `sizeof(BlockScratch)`; before the source scan, `12*S+1024`. A fixed bounded scan identifies exact token spans without copying source strings. Stop at D>1024, N>64, E>64, P>256, classes>64 or nesting>16. Positive spans/columns at most64; zero and unsupported auto-column syntax reject. |
| Owned model | Before each reserve/resize, the checked actual capacity times sizeof(element), doubled where both scratch/final copies coexist. Before each string copy or decode, its counted UTF-8 upper bound; before glyph/row collections, actual count times sizeof(element). No uncharged parser retry, regex backtracking or substring copy. |
| Identity and atomic style | Before each comparison, `1+min(lhs_bytes,rhs_bytes)`; before a table/property visit, 16. Resolve all references/classes/property priorities first. Charge the actual retained style/ID/caption storage including both old and replacement allocations during merges. Invalid or unsupported input cannot leave partially admitted art. |
| Grid measure/place | Before the bounded hierarchy/placement scratch, its actual vector capacities times sizeof(element). Charge32 per child/span arithmetic step and16 per hierarchy traversal or column distribution step; every width/height/index arithmetic operation is checked before use. Never loop across a source count before checking its cap. |
| Captions/contours | Charge copied output caption bytes and `8*G` before decoding/wrapping. Charge8 before each mask/contour/gate cell inspection or write, including a failed candidate. A glyph view may point only at immutable owned caption bytes or static glyph literals whose lifetime exceeds this raster. No source/model string mutation may invalidate it. |
| Raster/backing | Before construction, `A*sizeof(BlockCell)+A*2*sizeof(uint16_t)+H*4*sizeof(string)+1024`, with A computed from complete spacer-preserving extents. BlockCell may retain bounded non-owning glyph views during this layout only. The two packed planes pay for retained and returned backing; existing legacy families do not pay these fees. |
| Routes | Before route buffers/labels/gates, actual capacities times sizeof(element), including predecessor/queue storage and label copies. Charge16 per candidate endpoint pair and8 per examined search neighbor/gate/label cell before its read/write. Cap candidates at the finite source-independent port list and search nodes at A per attempt. A failed route search spends its work permanently; no reset per edge or retry. |
| Serialization | Precount glyph bytes with one charge per cell, validate complete row/glyph round trips and style equality across wide/combining continuations, then prepay output bytes before row allocation. All A cells, including background spaces, survive. Count actual R by a charged row/whole-glyph scan, prepay `R*(512+8*sizeof(string))+H*(256+4*sizeof(ftxui::Element))` plus copied glyph bytes before accepting a styled payload. |

Fixed-size buffers must be initialized only after their fixed prepayment succeeds;
heap capacity, not merely logical size, owns the storage charge. Each backing
allocation is paid once, while every repeated route/contour visit is charged even
when it fails. Cache retention adds no new layout arrays and never reruns the
parser. Cache hits reconstruct fresh owned text runs from the validated bounded
payload; they do not claim an additional measured layout pass. Actual C++ sizeof
values and complete58 A/H/R/Work feasibility must be independently checked before
B2 closes; an optimistic arithmetic estimate is not execution or admission proof.

Stage gates are (B1) independent full58 source/meaning/metadata review; (B2) exact
terminal contours, style/palette/metadata tables, sizing/routing/Work and cache
compatibility design+PRR; (B3) complete Native and Official capture-before-binding
consumers with glyph/foreground/background/bold cells, source/model/projection
locks and meaningful good-first semantic/physical mutants; (B4) full Debug and
Release builds/Mermaid regressions, preserved prior630 originals, untouched
body593, unchanged source hashes and independently reviewed dual admission.
All58 remain the intended increment; any incomplete row stays zero credit and
prevents declaring Block58 complete. The milestone remains 1,931, not this family.

Rollback removes only Block dispatch/parser/layout/style/proofs and its cache
payload extension, restoring the exact previously verified630 surfaces and the
old three-family join sequence. No persisted state, source fixtures or public
factory/cache contract is migrated. Do not mark phaseC or the 90% goal complete
from source inventory, a source-only model, configuration recognition or an
unreviewed drawing. No commit or push.

#### Block58 executable test-only preflight checkpoint (2026-10-26)

agent:Harvey approved a limited TEST-ONLY B2 preflight trial, followed by the
source binder implementation and narrow corrections to three projection defects.
The owning files are `tests/support/mermaid_block58_{source,projection,contours}.hpp`
and `tests/ui/test_mermaid_block_semantics.cpp`, registered as a dedicated GTest
target in `tests/ui/CMakeLists.txt`. These are not production implementations.
The source binder freezes both reviewed fact roots and58 independent literal
source/Model/witness/manifest pins. Exact real-width type is retained only at the
four reviewed2.0/4.0 fields; no shared serdes extension was introduced.

The one-cell apron is now an executable TEST CANDIDATE: all58 plans pass source
schedule, full spans/parents, ownership/containment, mask/caption fit, distinct
fan-out stems, all9 links/3 source rows, ancestor crossings and source style roles.
This is not adoption of production sizing or the unreviewed old contour draft.
The native GTests independently retain literal rectangle/spacer/marker/gate,
notch/arrow-apex, full stadium and full inward-notch coordinate fixtures, and
read all caption/edge-label/note content directly from the immutable fact JSON.
Deleting gate metadata, erasing route cells, reversing an arrow marker,
changing a legal role or thick bit, adding reserved bits, invalid columns/duplicate
placements, empty direction sets and caller-recomputed Model hashes are rejected
without passing off resource exhaustion as a geometry negative.

Both full builds pass. The broader18-test revision passes all860 Mermaid CTests
(Debug156.62s/Release42.04s), then the final20 affected tests pass in Debug13.82s
and Release2.95s. No862-test full rerun is claimed. The first dual trial was5/8
and26/58 because group parents were loaded after root references; a separate pass
corrected ordering. Independent review then found omitted note-row/route-write
fees, erasable gate evidence and unchecked route style. Their concrete controls
and fixes passed narrow review; the old654,410 estimate is superseded. Glyph
review found omitted incident arms at stadium/notch corners, and the first full
stadium golden omitted four horizontal shoulder cells. The implementation retains
those cells; the corrected source-derived full golden passes. Complete glyph/B2
review remains distinct from these narrow fixes.

Final candidate artifacts match byte for byte across presets:
`mermaid-block58-preflight.json` SHA127ca3ac and
`mermaid-block58-glyph-preflight.json` SHA4499d731. All58 plans/row sets pass;
rows total119,295 UTF-8 bytes, candidate cold-stage maximum656,426 and proposed
glyph-inclusive maximum767,762 under1M. These are modeled test costs, not actual
production parser/cache Work or verified allocation prepayment. Matched compiler
ABI probes give Cell24/Point8/Element16/string24/ParserSpan40 bytes in both presets.
Every report retains zero drawing credit/captures and unapproved glyph-oracle and
production-prepayment flags. Temporary probes are diagnostic, not extra native
completion evidence. Old10/4 joins restored the unchanged630/2145 matrix; all423
prior Mermaid source/report hashes remain byte-exact. No shared production module,
public contract, dependency, commit or push changes.

Full B2 still requires independently reviewed complete boundary/port fixtures for
all12 source types, seven arrow headsets including shoulders/core joins, final
metadata/style equivalence and production/cache/prepayment compatibility. B3/B4
still require independent public five-frame captures before binding, meaningful
mutants, real terminal regressions and dual-source admission. The goal remains
at least1,931 complete originals; none of this test-only trial adds coverage.

#### Block58 complete boundary preflight follow-up (2026-10-26)

The earlier20-test checkpoint above is historical. Ramanujan independently authored
`block58_boundary_contract.v1.json` (SHA d5c3ce61332082a75737183533d92553cefe4abb5c571afa05c092442244cb07,
167116 bytes), without reading the glyph candidate or generated reports. Its36
normalized primitive fixtures cover all12 source types and all seven observed
arrow headsets at two widths, with272 full glyph rows, whole masks/interiors,
caption placements and exact ports. They carry zero complete-original credit.
Main added bounded source binding, compiled whole-asset SHA, schema/source-lock
checks and project-native tests of the full positives before glyph/mask/port/asset
mutations. The original sources and both source-fact assets are unchanged.

The first full-boundary run was0/2: thin nearest-segment glyphs disagreed with the
independent full-cell contour proposal, and subroutine rail tees were missing.
The simpler test candidate now uses every nonrectangular exposed mask cell as █
and explicit ┬/┴ rail joins. Both22-test focused runs pass (Debug26.95s,
Release3.83s); final full regressions are recorded in the subsequent checkpoint.
The full58 glyph report is now SHA eb846d6b9853b3af84a03e325f75252df2485ae64495a8fa022eaecdd54d1ca9,
119389 bytes of rows, candidate maximum738642; cold plan maximum656426 remains.
These are test-only worksheet estimates, not production prepayment or captures.

`docs/design/mermaid-block58-terminal-contract.md` replaces the earlier unreviewed
cell-centre/thin-line draft with the actual integer masks, one-cell root apron,
source-grid measurements, finite route/gate/marker rules, full Block-local
D1/D4/E2b amendment, exact roles/notes, current theme/flag policy, same-Work budget
and private single-payload cache dataflow. Independent full B2 design/PRR review is now approved by agent:Harvey against
contract snapshot a0f2976fd5408ede363dd2f8307a282559a887d8a9df4734f91a731d532cc54d.
The two final P2 findings closed explicit connected nonrectangular port precedence
and the generic whole-glyph caption/wrapping profile. A subsequent23rd native test
checks all36 primitives with each of four outward stems against complete literal
rows, retaining pixel ports and rectangle junctions; good-first lost-port/junction
mutants reject. Focused23/23 tests pass Debug35.42s/Release5.46s before the later
payload foundation. Source facts remain frozen; producer/capture/admission gates
are not granted by B2 approval. B3/B4 and the1931-original milestone remain open.

#### Block58 private payload foundation started (2026-10-26)

After full B2 approval, Main changed only the Mermaid module's private layout
partition, factory/finish implementation and cache implementation. One dispatch
retains the same remaining Work and returns owned rows plus an optional packed
semantic plane. One centralized validation precedes either public path; row-only
access and render share one exact-key lookup. Legacy planes stay empty and retain
the original finish behavior, probe fees, row-byte limits and rejection sentinel.
Cache copy, assignment, eviction and clear own their style bytes; mismatch/collision
serves its own newly computed payload uncached. No public signature, shared Canvas,
global theme policy, external dependency, persisted format or commit/push changes.

Harvey approved the dormant foundation's static dataflow after fixing P1: FTXUI
places a wide glyph's combining tail in its continuation slot, so an empty-slot
requirement was incorrect. The final validator uses FTXUI scalar widths without
heap glyph vectors; fresh finish retains base+all zero-width tail bytes in one
owned run with equal continuation styles. Only one fresh palette snapshot is used.
The reviewed frozen58 worksheet additionally prepays actual serialized bytes B by
B*(sizeof(string)+2), plus16*(A+noteCells); maximum962940 remains below1M. This
short-glyph corpus calculation is not a generic long-tail capacity proof or actual
producer prepayment. Long-tail byte scans/string growth and styled public runtime
remain pending the producer, with no fake public payload hook added.

The first integration builds exposed missing FTXUI Element alias declarations in
the private partition and moved cache implementation; both now use the existing
node header in a global module fragment. A new legacy copy/assignment regression
also triggered the deliberate -Werror self-assignment diagnostic; an indirect
same-object reference retains that check without suppressing warnings. Full build
and Mermaid regression results are recorded in the final checkpoint below. The first Debug full suite passed865/866, with
`RoleBodyBatchParsedMutationsReachTheirSpecificTupleAndSchemaGuards` reaching
its unchanged180-second timeout (180.04s), while Release passed866/866 in47.92s.
The Debug run overlapped a Release build/suite; contention is a hypothesis, not
an asserted root cause. No code, test threshold or resource limit was weakened;
the unchanged isolated retry passed1/1 in132.93s (133.22s total); another full
Debug full rerun then passed866/866 in208.67s, with that test completing143.54s.
Release remained866/866 in47.92s. This confirms unchanged isolated and full success
below the existing per-test limit, not proof of the contention hypothesis. No Block dispatch,
parser or production drawings are enabled by this foundation; formal coverage
remains630/2145 and the90% target stays open.

Final payload checkpoint: both full builds and all866 Mermaid-labelled CTests
pass (Debug208.67s after the separately recorded865/866 timeout;
Release47.92s). The actual PTY regression passes Debug1.65s/Release3.40s.
All23 preflights, ten legacy cache controls including new independent copy/LRU
state, and twelve capability reports pass. Graph check reports zero module cycles,
zero new upward edges and the UI12 singleton target passes; RFC lint31/31 passes.
The first final join replay rejected mixed matrix generations: Debug had a fresh
unjoined baseline while Release was already joined, and old markers were older
than refreshed verdicts. Re-executing Release OfficialMatrix and the established
two-preset old10/4 joins restored630 in both matrices, with SHA81366f10 exactly.
No timestamps were forged or validation relaxed. All593 body rows and all419
unchanged baseline source/report files remain exact; only the three expected
private production files plus legacy cache test differ from the423-file baseline.
Plans/glyph report pairs match SHA127ca3ac/c65d6ee1. Both source-fact roots and
independent36-fixture asset remain exact. Formal gain remains zero. No complete
Block producer, styled public proof or admission is claimed; next work is the
private Block parser/layout and B3/B4 independent captures/admission.

#### Block58 production and independent capture checkpoint (2026-10-26)

The earlier dormant-payload checkpoint is historical. Main implemented the closed
Block parser/layout in `mermaid_block_impl.cpp`, appended the test-only Block probe
value without changing prior enum values, and connected exact Block headers to the
same live dispatch Work. The source-only test probe accepts real source and a live
Work, never a fabricated payload. No source filenames, fact JSON, producer snapshot,
new dependency, global Canvas or global palette policy enters production.

Harvey approved the producer implementation at SHA
ad2a35c7355e58ff0dbe7346ee52978402ca13b2adf0b08dda900e5ba414bf25 after
closing label/route collisions, cell-width label placement, full nonrectangular
port styles, paid row-growth/candidate buffers, named composite endpoints, empty
config namespaces and default-class lists. Every typed style resolves before paint.
First-owned placement/span/directions survive partial duplicate declarations.
The final glyph pass updates the same bounded compact cells, then prepays actual
serialized bytes, complete semantic planes, fresh run/row nodes and Unicode scratch
before producing immutable rows/roles. No Work reset or cap increase was introduced.

The first actual Native full-source run was33/58, with25 whole-source factory
rejections. Correcting bare envelope config paths and dotted identifiers produced
57/58; the remaining source failed with898110 Work remaining and also failed the
non-scoring2M diagnostic. Its first arrowId6 owns y; a later x/down declaration is
ignored by the source merge, but had been prematurely rejected as a new head set.
Checking the closed projected headsets only for newly owned identities fixes the
actual root cause. The2M diagnostic grants no coverage and changes no factory cap.
A generic renamed-ID/span regression covers that path.

Ramanujan independently authored a pure whole-source geometry/role authority and
Native consumer without reading producer code. Native captures uncached, cache miss
and cache hit (15 frames) before binding each original. Main corrected missing hit
frame publication and strengthened physical color mutants to other valid palette
roles, not malformed color encodings. All58 originals pass complete glyph, blank
fill, foreground/background/bold, exact fallback and restoration checks, rejecting
384 semantic and437 physical mutants. The entire58 x seven palettes x four flag
groups also passes; the two flag bits are high_contrast and force_monochrome.
Source pin/recomputed-model guards pass. These remain local proofs, not admission.

The project-native live-budget test executes all58 with the same classifier and
remaining1M Work: maximum958107, with zero-budget rejection controls. Other public
controls exercise wide+combining edge labels without placeholders, nonrectangular
Red/bold ports beside ordinary route strokes, cache row getter/render/copy/assignment/
clear/eviction/oversize/disabled/ASCII/rejection and fresh palette mapping, named
composite endpoints, ten17-cell words on ten caption rows, and exact Work versus
short-by-one. An independent multi-route label example reproduces the old route
through a prior label and its right clearance; the corrected public five-frame
uncached/miss/hit positive and six non-vacuous mutants pass.

James independently integrated Official-owned captures and source binding into the
full OfficialMatrix, retaining the immutable manifest expectation and explicit
foreign-assertion debt. The current Release local verdict is58/58 with15 own frames
per original and exact Markdown dim verification. No Native report or producer
pixels supply expectations. The matrix excludes all58 from formal drawing coverage
until the external reviewed dual join. Debug/Release four-consumer freshness,
stronger mutation reruns, complete regressions, B3/B4 and the join are still pending
at this checkpoint. Formal coverage remains630/2145;1931 remains the milestone.

#### Block58 B3 approval and B4 execution (2026-10-26)

agent:Harvey independently approved B3 after fresh actual Debug/Release links and
both focused reruns: Debug2/2 in64.13s and Release2/2 in9.14s. Each consumer owns
870 captures of58 complete originals. Native rejects384 semantic and437 physical
mutants; Official rejects384 and495, including58 exact dim-attribute controls.
Both preset compact pairs match byte for byte and explicitly retain foreign=0,
native_report_input=false and drawing_credit=0. The sole authority-status change
is B3_REVIEW_APPROVED; its reviewed file SHA is
3b609c966bec06bd28b728a0aa94e01463029f51e732b476048a8eb8a1dbecb2.

Harvey also approved strict metadata-only join code SHA
73fa0c99479f610c2a6e0c34ca07b410f94ab32c7f55942f504cadb54779e327.
The19 self-tests include immutable pre-validation snapshots across presets,
actual reviewed-helper hashes, interruption before/after every publication step,
and preserving recovery backups if rollback itself fails. Main and reviewer
independently ran19/19 controls. The first hardening run exposed a tuple/list
construction error; r4 fixes it and passes. No formal write has run at this
checkpoint. Full approved-state rebuilds, B4 regressions and final admission
remain pending; formal coverage is still630/2145 and the1931 goal remains open.

#### Block58 B4 and formal admission completed (2026-10-26)

agent:Harvey approved B4 and the real admission write after independent checks
of four fresh compacts, B3/helper pins, old10+4 prerequisites, guard snapshots
and exact non-selected-row retention. Debug and Release final full builds pass;
final Mermaid suites 881/881 pass in 198.35s/39.58s. Debug first 879/880 in 217.78s
failed because the imported baseline trimmed blank spacer columns. The local
public-intrinsic-width correction and generic 59-cell regression passed independent
review, focused 2/2 in 21.25s and the complete rerun. Both final PTY tests pass.
No production change was required by this final regression finding.

The reviewed metadata-only join published 630+58=688/2145 (32.07%) into both
presets with matching formal markers and matrix SHA
ef0f12e6703ae1a30c6d0f533735d5908ea73b03040b03509fa72db90ce64997.
Only 58 Block original rows change; all 2680 others (including 593 body diagnostics)
remain exact. Harvey independently verified both published markers, exact
approved patches and the single appended Markdown admission section; no remaining
publication finding. All four new capture/verdict files match byte for byte across
presets. Of 423 old code/report files, 410 remain exact;13 are the seven required
source/test integrations and six generated matrices/imported baseline reports.
No shared Canvas/global palette or established public contract changed; the
appended probes are test-only. No commit or push. Source fallback, foreign
RGB/CSS/SVG assertions and terminal font/ANSI behavior receive no inferred credit.

The above 90% milestone remains open: 1,243 admissions still required to reach
1,931. Next is independent ER107 source-only census: 107 originals whose existing
ER authority records only active_config debt. Candidate scope grants no drawing
credit and no configuration whitelist approval. Its own source/design/PRR and
implementation/independent-consumer gates precede any production change.

#### ER107 source-only candidate and architecture preparation (2026-10-26)

Next work remains inside the ER portion of `loom.ui.visual.mermaid`. Source author
Ramanujan independently produced `er107_complete_source_proposal.v1.json`,
3,743,357 bytes, SHA5a1c642cc7a3e2cf08e00355be845711520f762fc999dc13fe7dbb4a417297f3.
Exactly107 selected frozen ER206 whole-null rows record only active_config debt;
all62 prior complete profiles and37 other whole-null profiles remain unchanged.
The proposal binds43,704 raw bytes,6,616 byte/line witnesses,362 entities,
287 attributes,253 relationships and181 configuration leaves. All terminal
Models remain null, status UNREVIEWED and drawing credit0. Harvey independently
approved B1 at this exact SHA after re-parsing all107 raw files with zero
difference,6616 witness checks,181 metadata leaves and5 fixed-upstream files/24
excerpts. This external receipt approves source facts only; immutable proposal
bytes remain unchanged. Configuration portability, terminal geometry and B2
are not approved.

The new [ER107 contract draft](../design/mermaid-er107-terminal-contract.md)
records owner boundaries, exact pins, source uncertainty, integration alternatives
and B1/B2/B3/B4 evidence. Current ER config validation and plain row layout are
insufficient to admit new theme/look behavior. Raw-byte inspection confirms103
folded `>-` descriptions and four plain descriptions, already covered by existing
ER syntax. YAML AST style `>` is a category rather than the complete chomping
header; it does not justify a new syntax path. Main has not changed ER production
or added a whitelist.

Current688/2145 remains unchanged. The local460-file snapshot records preservation
hashes only. Additional fixed-upstream ER presentation research and complete
terminal projection/geometry/Work design must precede B2 design/PRR approval;
then implementation and two independent consumers are still required. Candidate107
is not a promise of107 admissions or a substitute for the1931-original goal.

#### ER107 source-binding preflight checkpoint (2026-10-26)

Ramanujan independently authored test-private `mermaid_er107_source.hpp` and
`test_mermaid_er107_semantics.cpp`, importing only std/JSON/crypto. Main registered
the new GTest target with loom_utils/gtest/shared test main and the Mermaid label.
Both targets build; the four focused native preflight tests pass in Debug5.39s
and Release1.49s. Full107 raw/manifest/prior-row/witness/Model/config binding,
good-first raw/witness/config/recomputed-Model mutations, canonical encoding
strictness and raw description/generic/blank-role retention run in C++.
OracleWork512M belongs to test validation, not the unchanged production1M cap.
No renderer or captured pixels supply expectations. Independent helper review
passed Harvey's independent source-infrastructure review at helper SHA
3b82b9258809cd3874f286bc51aec7feb7650fc54fb9e1c7c885cddfab9cc59b
and test SHA7d9d0f2698b09aa4b4335648ccf90ed2970701fbf0b18a04121789928ed9b6dc.
He independently ran4/4 per preset. This is not B2/B3 or production admission.
The previous881/881 full-regression checkpoint is retained; these four focused
source tests are not a new full-suite result.

Main also authored `er107_presentation_source_audit.v1.json`,72,136 bytes,
SHA8f35938eb74591a428823aae3c1279c4a333fa8240b7d92181e77e7f22ed18ba,
containing17 fixed-commit files/49 byte-bound excerpts for appearance precedence,
ER marker/shape/label pipelines, categorical CSS gates, SVG viewport and themes.
All file Git blobs/sizes match the commit-indexed recursive listing. This
additional source-only evidence passed Harvey's independent provenance review:
17 fixed raw files fetched,49 slices and complete canonical digest verified;
4764 cached recursive-tree entries reconstruct root171013601d2b707e35af79142d4c605a21fcbe82
and the frozen corpus subtree. Online tree API recheck hit403; cached tree
reconstruction is distinguished from fresh raw-file fetches. Immutable source
evidence flags stay unchanged; this does not approve a terminal projection. B2 still needs noncategorical roles, notices, full scene bounds and
cost laws. The formal688 count and1931 goal remain unchanged; no ER production
or shared/public contract change, commit or push occurred.

#### ER107 terminal source semantics and baseline-refresh checkpoint (2026-10-26)

Independent author Ramanujan produced the complete terminal candidate at SHA
f9847bdff797cc59949d3a5f00b5718553bcbd5be3171396e27570f0a69cf605,
2,894,834 bytes, binding contract8e2317437a5c043e0b4ae188f93a102b7db631a5f5917c89e80ee7352a979eb5.
Harvey independently reconstructed all107 models/notices and approved partial B2
source-projection semantics:362 entities,287 attributes,253 relationships,
117 typed tables,71 categorical cases,871 notices/1592 rows. Three P2 findings
closed full FG/BG/port priority,1px/2px quantization and finite generic grammar;
M working cells and A output cells are charged separately. Description remains
validated hidden documentation under the existing decision register. Full B2
still needs executable primitive/capacity/cost evidence, not107 fake producer
scenes or a second test planner. Actual same-factory1M, captures and cache
proofs belong to subsequent implementation/B3; no production change is cleared.

A separate privacy check exposed three existing direct primary-module imports.
Main replaced them with the required `.testing` facade, retaining the same public
render/cache API and expected geometry. The privacy gate passes. Both complete
builds and Mermaid suites885/885 pass: Debug182.11s, Release37.36s, including PTY.
This is the prior881 suite plus four source-binding tests, not107 drawings.

Main also reproduced a real report-refresh bug: a fresh full matrix run leaves
the prior exact Block marker, and the new20th self-test first failed on its
unconditional rejection. Revised verifier SHA
54562671ea8971cbc342f61fdf177750a0bc189bb02fa44af567928538b2a2a8
requires exact expected prior-marker bytes after fresh dual proof/prerequisite
validation, snapshots optional marker state before semantic checks and backs up
existing markers with matrix/Markdown. Interrupts restore bytes/mtimes; failed
recovery retains backups. Already joined matrices still reject.23/23 isolated
controls pass, including forged marker, marker race and all existing-marker
interrupt positions. No manual marker deletion or gate relaxation was used.

Popper independently approved code/imports, real refresh and published six-file
postcheck. After fresh old10+4 joins, Block refresh returns the exact prior688
matrixef0f12e6703ae1a30c6d0f533735d5908ea73b03040b03509fa72db90ce64997,
Markdown/markers and2738 rows. Exactly456/460 old code/report files remain equal;
only the three import files and verifier change. The original red refresh test
and privacy failure remain recorded. No production/shared/public contract change,
commit or push. The goal is still1931; formal688 leaves1243 admissions pending.

#### ER107 inline-label witness and native preflight (2026-10-26)

Harvey approved a bounded shared TEST-ONLY ER successful-label witness as the
smallest way to retain the established inline-label bridge. Main added opaque
borrowed grid/Model receipts to `mermaid_relations_oracle.hpp`; default nullptr
keeps old checks/scans/allocations/Work fees. Only the selected relationship's
actual horizontal label jump with reciprocal arms can be certified. Marker,
entity/group boundary and extra-branch contacts reject; inner trace assignment
continues searching when one candidate fails. Publication follows all full-scene
checks and Work payments, with entry invalidation and no allocating final swap.
This changes shared test infrastructure, not production or a public contract.

Ramanujan re-bound the proposal to the clarified contract8056c56dce627a488af883d20124cd8bbc9b6d8cd7fa5e11bde583a1fbb8cfd9:
terminal SHA bf45c1f6b91834dd6b4a910086a1b44ecd05cb28159382c88509619380ed580d.
Source bindings, whole-scene Models, captions and notices remain exact; only
clearance declarations/receipts change. Both new projection targets and final
old-oracle targets build. First focused runs pass43/44 in both presets: old36
and seven new controls pass; `All107TypedSourceModelsAndNoticePlanesRemainUnreviewed`
fails when one512M TEST ledger accumulates independent full-root/hash phases.
Harvey's independent LLDB diagnosis confirms test-ledger exhaustion while building
the terminal_models component preimage, not production1M failure. The correction
uses separately bounded complete validation phases, retaining every check and
all107 cases. B2 remains pending until focused reruns and independent closure.

The new eight-test group includes source/model/notice binding, full table/port/
gate/blank/FG/BG/bold/dim primitives, all four cardinalities in four directions,
inline ownership, broken arms, branches, wrong relationship/marker controls,
valid-empty receipt, failure/resource clearing and null-output fee retention.
These are preproduction tests, not original drawing admission. Finite worksheet
quantities and actual production1M feasibility remain distinct; no producer or
captured output supplies expectations. The1931 goal and formal688 stay fixed.

#### ER107 B2 design/PRR approved (2026-10-26)

Harvey independently approved the complete B2 combination: contract8056c56d,
terminal bf45c1f6, authority aac5420f, tests25c41c51 and shared oracle29b5f7a8.
Final focused44/44 pass per preset: Debug4.30s, Release1.22s. First43/44 remains
recorded. Independent source-root and projection-component test phases each keep
a512M ledger, aggregate bounded1024M test-oracle units, with no hidden reset or
removed checks. These units do not change the production1M counter. Final privacy
check also passes. Complete107 source binding/31 topologies, executable primitives,
opaque inline witnesses, source capacity and finite M/A/B/R/H worksheet close
B2 without duplicating a test layout planner or fabricating107 producer scenes.

Main may now implement the approved ER profile inside the Mermaid owner. Planned
stages are single envelope/body parse plus source display transforms; private
relation payload/complete role serialization; same-factory live Work probe;
then independent Native/Official captures, runtime/cache/legacy retention and
full B3/B4 review. Global Canvas, palette provider and established public render
signatures remain outside scope. B2 adds no drawing credit; formal688/2145 and
the1931 milestone remain unchanged. Actual107 factory1M, exact/short-one and
public complete-scene/plane/cache evidence are required before admission.

#### ER107 production feasibility in progress (2026-10-26)

The first implementation uses the approved ER-local presentation metadata,
private relation payload, source-owned role plane and committed-route extent.
It adds the exact-source public-factory Work probe without exposing payloads.
First actual107 feasibility was101/107 in both presets, not all-green: six
all-relationship theme variants rejected at about41k units. Debug LLDB stopped
at the existing peer-rank fit rejection:225 columns plus two16-column margins
required257, above240. The failed plan had moved no boxes. Only the new ER
profile now falls through to the same existing linear plan when that optional
rank plan cannot fit; legacy ER retains its previous rejection. No duplicate
layout solver or route retry was added.

After that correction, focused runtime3/3 passes in Debug and Release. All107
complete originals pass the identical initial Flow attempt, classification,
ER kernel and payload validation with the original1M cap; maximum862576 at
this checkpoint. Each source also passes an exactly-measured budget and fails
with one unit less. The earlier compile failures (test namespace ambiguity and
wrap pair-type deduction) and first101/107 runtime failure remain in the local
logs. Subsequent accounting/review edits must rerun this measurement. These are
feasibility facts only: full independent Native/Official capture, complete
planes, useful mutants, cache/palette behavior and broader regression remain
in progress. Formal coverage stays688/2145; no new ER drawing credit is granted.

#### ER107 independent Native capture checkpoint (2026-10-26)

Native's first complete-frame run was106/107. The sole grouped handDrawn source
passed whole-scene geometry but failed a full-style comparison: the producer
neutralized the label's top/bottom routing clearance, which must retain its
containing group surface. Caption rows and their owned left/right padding now
alone receive neutral label style. Complete label-clearance bounds still belong
to the crop. Expectations were not changed to accept the incorrect surface.

The subsequent Native3/3 pass in Debug52.53s and Release6.04s. All107 independently
own15 captures before current-case binding, then validate aggregate components
only after all107 captures. Complete scene/planes, restored widths, exact ordinary
source fallback, uncached/miss/hit equality and fresh nodes pass. Source/header/
cardinality/table/role and physical controls reject359 semantic and734 physical
mutants. A separate seven-application-theme and inherited-dim test passes.
Both compact reports are byte-identical at
6c7b59971cadcf35faf0b91289963931e0006cbbfe5e9a2421d4b2ba258b2bf5.
All107 actual-factory exact/short-one tests still pass in both presets, latest
maximum865308 of the unchanged1M production cap. This remains local readiness,
not formal admission.

The focused legacy ER run first passed67/68. Its one stale closed expectation is
the frozen description30 control13: an otherwise plain A/B relationship with
htmlLabels=false, now covered by the approved new profile. The frozen asset and
historical classification remain unchanged. Its existing five-view consumer now
requires the exact raw source, complete wide/narrow graph, ordered source-derived
notices, separators and following content. The other nine historical positives
and20 remaining closed boundaries retain their checks. Focused description2/2
passes after this source-bound amendment; an independent full-plane control is
also being added. No synthetic control contributes to the2145 denominator.

Official's first independent run is not green: its shared checking ledger was
exhausted by several sources after three full-frame passes plus good-first
adversaries. These are explicit test-oracle resource failures, not production1M
failures or successful semantic negatives. Bounded independent test phases,
Official matrix ownership, complete regression and B3/B4 review remain pending.
Formal coverage stays688/2145 and the milestone remains1931.

#### Mindmap5 shapes source/design preparation (2026-10-26)

This next increment belongs only to the existing private Mindmap owner.
agent:Galileo approved the five-source facts and bounded Shapes4 design/PRR
after correction. Production implementation may start only within the four-source
contract below. At source/design approval the verified runtime baseline was
626/2,145, including the ten Mindmap10 originals. The later Shapes4 dual-admission
checkpoint records completion of its runtime gates and the resulting 630 count. The five candidate complete originals
are `nightly-check/{bang-and-cloud-shape-with-icons,circle-shape,rounded-rect-shape,square-shape,text-should-wrap-with-icon}.mmd`
beneath `cases/mindmap/`. Together they have eight source nodes and three parent
relations. The first and last filenames mention icons but their bodies have no
icon directives. The `circle-shape` body uses a single parenthesis pair (rounded
rectangle); `rounded-rect-shape` uses a double pair (circle). Source syntax,
not a filename or description, defines the shape. Duplicate authored `a` labels
in the Bang/Cloud input remain distinct source-ordinal nodes: the pinned grammar
passes the authored text to `addNode`, and `MindmapDB` allocates a fresh numeric
`id` using `this.count++` while retaining separate `nodeId` text. A repeated
authored ID is not permission to merge nodes. The separate Merman typed-model
duplicate-ID rejection is an internal-ID rule, not an authored-ID merge rule.

The complete source-only asset is
`tests/fixtures/mermaid_native/mindmap5_shapes_source_proposal.v1.json`
(SHA-256 `26f803ffb4f900f81a5742f97671a7c6fd4a3c53a0d55a36a6bbbd2af5d1bd25`; reviewed proposal `b8f434d6dc927b05cdfb9cd4b0c28cb36a55c9fc13371d95615635e3d7996ca4`),
with source lock `0d2c09d4c4091f26ac0d18f5ef86e54ae26bc5367efc05bf19d49bcb04d97091`.
agent:Locke derived five complete raw Models with eight nodes, byte/line witnesses and
manifest bindings independently of the renderer. Main rechecked each complete
raw source, Git blob, witness, Model and manifest record; all five joins pass.
The source semantics explicitly precede sanitizer/visible-label interpretation.
Its 32 mutation obligations are declared, not executed geometry proof; pending
parent/shape/projection obligations cannot be used as runtime negatives.

The source proposal is a separate asset, leaving the previously reviewed
Mindmap10 Models, mutations and all their frame bytes unchanged. It must bind
all five full raw inputs, manifest ordinals, raw/Git hashes, exact multiline
lexemes, complete captions, authored IDs, shapes and parents. Multiline opening
and closing delimiters require a reviewed normalization rule; neither trimming
indiscriminately nor copying a parser's output into the authority is acceptable.
The proposed grammar extends the current description-only envelope and rooted
indentation tree; configuration, icons, Markdown/HTML, unknown delimiters and
partial syntax continue to return complete source. No title/config whitelist,
ASCII factory extension, other family or runtime dependency is included.

The intended implementation preserves the existing default-only and single-line
square-root layouts verbatim. A separate shaped-tree branch is required for new
shapes, multiline roots and a shaped child of a default root. Every visible
shape needs a complete and distinguishable outline, exact caption rows and
ports whose incidence identifies the actual parent. Rounded rectangle may reuse
the existing visual convention of four curved corners and continuous straight
borders. Circle should follow the existing Flow circle's stepped curved-ring
convention rather than being relabelled as a rounded rectangle. Bang and Cloud
still require a concrete reviewed contour contract; a shape name or marker next
to an ordinary box is insufficient. No Bang/Cloud production implementation may start
until its contours and source-to-terminal projection are separately approved.

Variable-height nodes must not overlap text, outlines or ancestor trunks.
A candidate layout places preorder nodes on disjoint vertical intervals and
uses a checked column stride larger than every node width, with each parent
trunk below that parent's bottom port and horizontal entry into the child.
Default markers remain one-cell nodes. Caption wrapping keeps the reviewed
32-cell word/glyph rule and source reconstruction by explicit row joiners.
This candidate must be checked for parent/child and sibling-subtree mixtures,
narrow width and deep trees before its exact coordinate/port rule is frozen.
No successful old output may change as an incidental layout improvement.

Keep the existing 64-node/16-level/256-cell/1,024-byte/64-KiB bounds and one
remaining Work budget. Prepay multiline scanning, all label bytes, glyph/row
storage, shape contour cells, route visits and final UTF-8 copies using checked
arithmetic before allocation or writes. Exhaustion remains a complete-source
result and cannot count as a rejected geometry mutant. No private-helper
promotion into the shared detail interface is needed just to reuse a small
family-local contour convention.

Graduation requires independent source/design/production-readiness review,
followed by independent Native and Official five-frame capture-before-binding
consumers, full-cell shape/caption/parent checks, good-first shape/caption/node/
sibling/reparent and physical contour/branch controls where meaningful, and
Debug/Release parity. Existing Mindmap10 reports, Class/Flow reports and the
626 admitted original identities must remain exact. The new cohort must have
its own versioned dual-consumer admission rooted in the verified 626 baseline;
never edit the frozen Mindmap10 base of 616 or reuse a successful old verdict.
An unreviewed proposal, a source fallback, a new primitive or a standalone test
adds zero official credit. Config/icon/formatting originals remain separate
whole-source debt and the above-90-percent objective stays open.

##### First executable subset: four non-Bang/Cloud originals (approved design/PRR)

The first implementation subset excludes the complete Bang/Cloud original until
its contours are separately approved. It targets exactly four complete sources,
five nodes and one parent relation: the three multiline shape roots and the
rounded-child wrapping input. The five-row source proposal is source authority,
not permission to admit all five. No Bang/Cloud shape may be represented by a
box with a shape-name tag. agent:Galileo approved this bounded design/PRR after SHA/port/control/rollback
corrections. The geometry below is the approved implementation contract.

- Admission grammar adds explicit `id(text)` rounded rectangle and `id((text))`
  circle to `id[text]` square, with the current ASCII authored-ID grammar.
  A multiline explicit shape is admitted only when its opening delimiter ends
  the first physical line, exactly one nonempty plain-caption line follows,
  and the matching closing delimiter is alone (apart from leading ASCII spaces)
  on the third line. All delimiters must match exactly; additional content,
  extra lines, tabs, CR without LF, embedded syntax and partial shapes reject
  the whole source. Authored semantic label bytes, including LF and indentation,
  remain bound as raw facts. The terminal projection removes only the framing
  LF/indentation around that one nonempty line, records the exact removed byte
  slices, and retains its caption bytes unchanged. It does not claim a general
  Markdown/whitespace normalizer. CRLF variants must bind equivalent logical
  line framing in separate non-scoring controls. Rooted-tree indentation still
  uses the opening declaration's indent, never the caption/closing line indent.
- Preserve all existing default-only and single-line-square-root paths and frame
  bytes. New geometry applies only to the newly admitted multiline or shaped-tree
  profile. It uses the reviewed 32-cell word/glyph wrapping rule for square,
  rounded and circle labels; default labels remain unwrapped. Rows and joiners
  reconstruct the projected caption, and separate framing witnesses reconstruct
  the authored raw semantic label. All labels retain the old 256-cell/1,024-byte
  limits; raw label bytes including framing also obey 1,024 bytes.
- Let L be the maximum caption-row cell width, R the caption-row count.
  Square and rounded rectangles have width L+4 and height R+2. Caption row j
  starts at (left+2,top+1+j); every row has one blank cell between text and
  each border, including trailing padding. Square corners are the existing
  sharp box convention; rounded corners are `╭`/`╮` on top and `╰`/`╯`
  on bottom, with continuous `─` horizontal and `│` vertical borders.
  Circle width is the least odd integer >=max(9,L+6), height the least odd
  integer >=max(9,R+4). Use the existing Flow `classic_ring` stepped-ring
  convention: outer curved corners at (left+2,top)/(right-2,top) and the matching
  bottom pair; inner corner pairs at (left+1,top+1)/(right-1,top+1) and their
  bottom reflections, horizontal arcs between each pair, vertical sides from
  top+2 through bottom-2. Caption row j starts at (left+3,top+2+j), with all
  remaining interior cells blank. This is an explicit terminal convention,
  not browser pixel geometry or an upstream circle width requirement.
- Preorder node i receives a disjoint interval: top(0)=0 and
  top(i+1)=top(i)+height(i)+2. Default nodes have height1, width2+caption cells,
  marker at their left/top and caption at left+2/top. Let C=max(node widths)+4;
  left(i)=depth(i)*C with checked arithmetic. Dimensions are the maxima of node
  right/bottom extents, within the unchanged canvas bounds. Every shaped node's
  bottom-center port is (left+floor(width/2),bottom), and its left entry port is
  (left,top+floor(height/2)); default nodes use their marker for both ports.
  Branches start immediately below the parent's bottom port, descend to the
  child entry row, and extend right to immediately before the child's left port.
  Set reciprocal incident bits at both ends. A shaped parent's bottom border
  replaces its center stroke with `┬` (left/right/down incident bits 14); a shaped
  child's left border replaces its mid stroke with `┤` (up/down/left bits 13),
  accepting the incoming connector from the left. `├` has no left incident bit
  and is forbidden at this child-entry port. Default marker glyphs remain
  unchanged. Merge shared trunks by line bits, without unrelated crossings;
  child-entry rows are strictly below their parent's bottom. Draw outlines and
  captions without overwriting paths, then apply only these declared port edits.
- Independent geometry checks must recover every physical outline/default node,
  caption and preorder interval, then follow actual child-left ink upward to
  the first parent bottom port. They must distinguish the three shapes, enforce
  complete arcs/borders/padding, exact caption rows, sibling order and account
  for every nonblank cell. Raw-framing changes and projection/caption changes
  have distinct named binding guards. Shape substitutions must preserve a
  locally valid full Model/projection before geometry rejection. Require caption,
  shape and physical-outline mutants for each original, and missing child,
  root/child shape substitution and severed route controls for the one tree;
  reparenting/sibling exchange is inapplicable to a two-node tree and must not
  be fabricated as invalid-Model failures.
- Budget prepayments extend the existing checked L/N/source/Canvas formulas:
  fixed 64-entry declaration/layout storage plus at most 256 row descriptors
  per admitted label, paid from total raw label bytes before vectors/glyphs or
  label strings; explicit framing scan and normalization-copy costs from S;
  contour and all inclusive route-cell visits paid before Canvas construction.
  Circle raster visits are bounded by 4*(W+H)+8 per node; rectangle visits by
  2*(W+H)+8. Text copies and final Canvas serialization retain actual-byte fees.
  The one-million remaining production Work budget is never reset or raised.
  Exhaustion must escape test oracles as resource errors and returns full source
  in production. Before admission, dual-preset tests must cover budget exhaustion,
  malformed/mismatched framing, deep trees, mixed shaped siblings, width fallback,
  wide/combined glyph boundaries and preservation of every old drawing.
  Before admission, non-scoring controls must individually exercise square,
  rounded and circle as a parent with a bottom port and as a child with a left
  entry port. Include a mixed-shape sibling tree with unequal caption-row counts,
  a deep chain, shared ancestor trunks and Unicode/wide/combined captions.
  Each control needs good-first physical port/route cuts and full ink accounting;
  the four original inputs alone cover only one default-to-rounded relation
  and cannot prove the other port combinations or variable-height traversal.

Shapes4 rollback is local and atomic: remove only its private parser/layout
branch, appended test-private Shapes4 authority, Native/Official Shapes4 capture
and admission records, and its documentation checkpoint. Restore the verified
626 baseline matrix using the unchanged Mindmap10 join workflow. Preserve all
Mindmap10 pins, Models, frames, fixed616 admission base, prior Class/Flow output,
other dirty work and the existing public factory/probe contracts. No persisted
user data, global UI behavior or configuration migration is involved.

The existing test-private authority may append a separate Shapes4 namespace or
facade, reusing only Grid, Work, canonical JSON, UTF-8 and cell-accounting
primitives. Original10 schema, Models, pins and default checker behavior remain
closed. Do not duplicate the whole authority or introduce a general cohort
framework for this increment. Native and Official keep separate captures,
binding, Work, mutations and reports even when sharing these pure functions.

#### Mindmap10 source-first design and PRR (2026-10-25)

This is the first unsupported-family workstream for the ninety-percent target.
agent:Parfit approved its bounded source/design/PRR contract after three review
rounds; production and test implementation may start. Runtime drawing admission
and phase completion remain gated by the execution requirements below. Its owner is the existing private
`loom.ui.visual.mermaid` module. Ten of the 33 frozen official Mindmap originals
have no config, icon, Markdown/HTML label, style or unsupported shape. They
contain 45 nodes and 35 parent-child relationships: seven default-shape trees
and three single square-shaped roots. Other 23 originals remain explicit
whole-source debt, inside the fixed denominator. The selected source lock is
`cecb9535be0ad801effe77c4c76c653a0c1db3c6a6cb21f378a80f957c3320af` (sort paths relative to `cases/mindmap/`, with the `mindmap/` prefix removed;
for each row concatenate the path's UTF-8 bytes, one NUL byte, 64 ASCII bytes
of lowercase hexadecimal raw-source SHA-256 text, and one LF byte, then hash
the concatenation; binary digest bytes are not used in this lock).

| Complete original beneath `cases/mindmap/` | Raw SHA-256 |
|---|---|
| `nightly-check/a-root-with-a-shape.mmd` | `9b32149ab995a307564025da74c6121087ad32d2766593dc0f0df714a47d1971` |
| `nightly-check/a-root-with-wrapping-text-and-a-shape.mmd` | `e99d0046f12700e772a3e810482696a66fe0626de6ce34166eaa536f7fc34ba3` |
| `nightly-check/a-root-with-wrapping-text-and-long-words-that-exceed-width.mmd` | `3579e9f2a114fa57a9ecce818b85e8a0d92f0eead520999e05d3afbff77d3801` |
| `nightly-check/adding-children.mmd` | `ada641a3fbfb454b08fd812acc220c464f8fc8b407a2cf38f20b649ed3278138` |
| `nightly-check/adding-grand-children.mmd` | `d83a24a07df1c28f5825a18108d24ddea44d79b22c104b93f3b27ff6eeeabbf6` |
| `nightly-check/branches.mmd` | `9b65c8f3b85fe1c06f86f5f2b5f7dcbab5933628a1467a9d0994c241423b550f` |
| `nightly-check/default-shape.mmd` | `8180af0e158056e7728a25da391beb182d0ed43ba9aafaaf85f48dd15dcd1bb1` |
| `nightly-check/level-2-nodes-exceeding-11.mmd` | `0a927d017b093b97cd1d1c59e8609165b110adfb1b49064a50112bf894ebc106` |
| `nightly-check/only-a-root.mmd` | `a7fc787d51b6790e2c82e2d19b63a02cd376c49c37016ab246b64f7afe23b2a4` |
| `pr-check/simple-mindmap.mmd` | `7e29a4621891e3f99f0153af718466bc5d14a3d86bf2b238db1c7d3cc3c32725` |

The independent source-only Model proposal is
`tests/fixtures/mermaid_native/mindmap10_complete_source_proposal.v1.json`
(SHA-256 `fa675abb49ce3de96e082b17cad12467296f06db80af28c9edafa8d4b6815627`).
It records independent source/design approval, with no runtime drawing credit;
it binds complete Models,
manifest global ordinals, raw/Git hashes and exact source line/byte witnesses.
Separately pinned proposed terminal projection rows and explicit row joiners
reconstruct every original caption without claiming an upstream width constant.
Geometry consumers remain to be authored after review.

The source-owned grammar accepts the exact `mindmap` header, one rooted tree
of nonempty plain labels indented with ASCII spaces, and `id[label]` only for
a single square-shaped root. Leading indentation of the first node is its
base; a child increases indentation, and dedenting must return to an open
ancestor's indentation. Tabs, forests, unknown directives, config/init,
icons, unsupported delimiter shapes, Markdown/HTML, trailing shape syntax and
partial syntax return the complete original source. Reject shape punctuation
rather than rendering it as a plain caption. Preserve authored sibling order
and duplicate captions as separate node identities by source ordinal. Reuse
validated UTF-8/cell-width helpers; never split a multicolumn glyph.

Only one top-level `description` is permitted as nonterminal metadata, through
an unchanged-body use of the existing envelope parser. Its closed forms are
plain single-line scalar, single-quoted single-line scalar and `>-` folded
scalar, matching all ten originals. Reject duplicate descriptions, `title`,
`config`, init directives, literal blocks and every other key/form. No new
config whitelist or presentation exception is implied. Complete labels and all parent relations
must survive projection. The proposed terminal drawing uses a visible node
marker, a full caption and orthogonal connected branches for default nodes;
it is a diagram whose parent incidence is independently recoverable, not an
indented text dump. Explicit square roots get a complete rectangular outline.
Long square captions wrap greedily at spaces with a 32-cell interior limit;
an overlong word breaks at glyph boundaries. This terminal wrapping equivalent
must be reviewed against the two complete upstream wrapping originals. Label
rows remain separate source-owned facts, not inferred from production output.
The 32-cell width is a proposed Loom terminal adaptation, not a width asserted
by the upstream inputs or Merman's 80-cell structured-text formatter. The frozen
row facts passed independent review before implementation. No color or
browser geometry is claimed. The explicit ASCII-only factory
continues to decline Mindmap until its separate source option/function port.

Use at most 64 nodes, depth 16, 256 cells per label, the existing 64-KiB source,
240-by-1024 canvas and one-million Work limits. Prepay all tree containers,
indent-stack scans, glyph segmentation, wrapping, row/cell storage and copies
before allocation or writes; any exhausted check returns complete source.
A private family implementation can return ordinary canvas rows through the
existing public factory, without a runtime dependency or shared UI change.
Append any probe-family enum value so existing numeric values stay stable;
only the exact new header reaches the new family. Old probes, factory overloads,
resource fees and other families' outputs require regression comparison.

Before production implementation, independent design and production-readiness
review must approve this contract and the source-authority design. Tests then
freeze ten complete Models with raw-source hashes, source byte/line witnesses,
node identity, parent, caption rows and shape. Neither Models nor a semantic
checker may be generated from the production parser, AST or captured picture.
Native and Official own independent five-frame captures before binding these
facts; neither consumes the other's report. The checker must establish exact
node/shape/caption ownership, every parent branch, sibling order, absence of
extra nodes/routes and same-Node restoration. Require good-first meaningful
caption, missing/extra node, valid reparenting, sibling-order and physical
branch-break controls wherever applicable, including duplicate-caption and
Unicode controls. Narrow output is complete geometry or exact whole source.
The pinned Merman `upstream/tests/new_family_models/mindmap.rs` remains the
separate typed-model/option obligation source; its render-model APIs, negative
probes and cycle behavior are not credited by this source-only grammar cohort.

The following precise contract addresses agent:Parfit's review findings and has
passed independent design/PRR re-review. It authorizes only this bounded family
implementation; no runtime admission or full-phase graduation is implied.

- Default geometry: preorder node `i` has marker `●` at `(4*depth, 2*i)`
  and its unwrapped exact caption starts two cells to the right. Root depth is
  zero, with at most 16 levels. Captions have 1..256 visible terminal cells,
  at most 1,024 bytes and no trailing ASCII space. Each child connects vertically from the cell
  below its parent's marker to the child's row, then horizontally to the cell
  immediately left of the child's marker. Shared ancestor trunks merge line
  bits; no arrowheads, diagonal edges or unrelated crossing joins are allowed.
  Draw segments before markers/text so no caption or marker is overwritten.
  The independent checker locates physical markers/captions in preorder,
  follows each child's left branch and upward trunk to the first parent marker,
  verifies incident line bits, and accounts for every nonblank drawing cell.
  Absolute Markdown padding is not an expected coordinate. Extra markers,
  detached strokes, missing branches, caption borrowing and unexplained ink fail.
- Square geometry: only one square root is allowed in this cohort. Outline
  width is maximum caption-row cell width plus four, height is row count plus
  two; captions are left aligned with at least one blank cell between text
  and each vertical border; shorter rows retain trailing blank padding. All four corners and continuous borders are mandatory.
  The raw label must have no leading/trailing or repeated ASCII spaces; tabs
  and other syntax controls are rejected. Other valid UTF-8 follows existing
  `validated_plain_label` round-trip rules, bounded by 1,024 label bytes and
  256 terminal cells. These are new-family bounds, not global cap changes.
- Wrapping: split words only at a single ASCII space. Append a whole word if
  it fits the current 32-cell row, otherwise flush the current row first. A
  word exceeding 32 cells starts on a fresh row and breaks at FTXUI glyph/cell
  boundaries; a wide glyph and its continuation stay together. Keep a final
  partial word chunk available for following words. Do not split at hyphens,
  collapse spaces, trim glyph content or add ellipses. Explicit `row_joiners`
  distinguish an original space from a hard word break and reconstruct the
  exact caption; independent row literals are frozen before producer code.
  No row is empty; at most 256 rows follow from the 256-cell label bound.
  Narrow rendering follows the existing factory: full geometry if it fits,
  otherwise the unchanged caller-owned ordinary source, never clipping.
- Authority: each witness binds the exact five-tuple
  `ordinal/caption/shape/parent/authored_id`; bare labels have null authored IDs,
  and the three explicit square roots bind `root`. Hash `complete_model` and
  `terminal_projection` separately using UTF-8 JSON with sorted object keys,
  compact comma/colon separators, Unicode unescaped and no terminal newline.
  File-level SHA also pins mutation plans and byte witnesses. Schema negatives
  must reach their named source/hash/type/identity guard, not a generic failure.
- Consumer ownership: follow the existing Class285 closed-authority pattern.
  One independently reviewed immutable fact asset and a test-private pure
  decoder/checker may be shared. It takes facts, Model, Grid and local oracle
  Work only; no production parser/layout, FTXUI capture or report API is allowed
  inside it. Each Native/Official consumer owns original-byte/manifest checks,
  its five public captures before local binding/decoding, its own Work/errors,
  good-first mutations and report. No consumer reads another's report or frame.
  Formal admission requires both independent verdicts for identical source,
  Model and projection locks; an offline read-only join may compare verdicts,
  locks and row IDs, but cannot lend a successful frame or waive a failure.
  agent:Parfit accepted this bounded reuse after the existing Class285 precedent
  rebutted the initial request to duplicate Models and checker implementations.
- Mutation responsibility is frozen per original in the proposal. All ten need
  a changed semantic caption, erased physical caption cell and an orphan stroke;
  all five single roots additionally need a valid shape substitution and erased
  outline/marker. Every shape mutation explicitly replaces shape and authored_id
  together (square/root versus default/null), carries consistent replacement
  projection rows and must pass local Model validity before frame rejection.
  Each of the five nontrivial trees additionally needs a removed
  leaf, valid reparenting, swapped sibling subtrees and a severed physical branch.
  IDs and target parents/sibling pairs are explicit, and subtree swaps retain
  source IDs. Semantic mutations update row facts consistently and first prove
  their Model is valid, so a source-binding/schema rejection is not counted as a
  geometry negative. Every mutation first passes its consumer's own good frame;
  a resource failure never substitutes for a semantic/physical rejection.
  Unicode and duplicate-caption sources are separate synthetic, non-scoring
  controls. Do not invent Unicode facts for these ASCII originals or claim an
  unobservable exchange of identical sibling IDs as a geometry failure.

Production budgeting uses the existing remaining Work (including prior dispatch
and envelope charges), without reset. Let `S` be source bytes (at most 65,536),
`B_i` bytes in label i (at most 1,024), `L=sum(B_i)<=S` total label bytes,
`N` nodes (at most 64), `H/W` checked output
dimensions and `R` the sum of actual segment-cell visits. All multiplication and
addition are checked before prepayment, reserve, allocation or writes:

| Stage | Required conservative prepayment and bound |
|---|---|
| Raw scan | `8*S + 256`, before splitting/decoding; existing envelope reader also charges its normal work. Reject bare CR, tabs, malformed metadata and unknown forms. |
| Node/stack backing | Before even initializing span backing, pay `64*(2*sizeof(NodeSpan)+2*sizeof(Node)+32) + 16*4*sizeof(int) + 1024` for at most 64 lexeme spans/Node slots and a fixed 16-level indent stack; also charge 16 units per parent/stack visit. Reject the 65th node or 17th level before append. Keep label lexemes as raw string_view spans during this pass; do not allocate/copy any label string or segment glyphs yet. |
| Label validation/glyphs/wrap | First pre-scan all bounded lexeme spans and checked-sum L without allocating labels, then prepay `L*(8*sizeof(std::string)+64) + N*512` before the first label string/glyph/word/row creation. Each B_i is at most 1,024; total FTXUI glyph slots are at most L. The profile has at most 256 nonempty row slots overall (64 single-row default nodes or one square root with at most 256 rows). Validate byte/cell limits and round trip before drawing. Each further scan/append is metered; reserve bounded capacities before writes. |
| Geometry | Default `W=max(4*depth+2+caption_cells)`, `H=2*N-1`; square `W<=36`, `H<=258`. Reject W>240 or H>1024 before Canvas. Compute segment visits from validated parents/positions; `R<=(N-1)*(2*(N-1)+4)` for default trees. Charge `R*(2*sizeof(Point)+16)` before routing and retain Canvas's normal per-cell charges. |
| Canvas/output | Before Canvas construction, `W*H*(2*sizeof(Cell)+16) + H*(4*sizeof(std::string)+32) + 1024`, covering backing initialization/copies and row storage; retain normal Canvas/text/lines charges too. Pay each returned caption-byte copy before it occurs. |

No new reserve adapter, renderer dependency or global exception/budget API is
introduced. Use checked sizes and standard reserve after prepayment, following
the project's bounded-container pattern. Public regressions must exercise
node/depth/byte/cell/dimension boundaries, invalid UTF-8/leading combining marks,
valid wide/combined glyphs, long-word wrapping, malformed metadata and a valid
large source that exhausts the explicit Work formula despite fitting dimensions.
Resource-only controls are not original drawing/semantic credit. Implementation
review must match every allocation/copy/loop to these fees and the fixed 1M cap;
source/geometry/oracle failures stay separate. Independent checker Work is a
separate bounded test budget, never a reset of production Work.

Graduation requires Debug/Release Native and Official passes for the same ten
complete originals, byte-bound facts and frame parity, reviewed independent
negative controls, preserved previous drawings, refreshed full matrix and gap
roster, full Mermaid CTest and independent implementation review. Failed
attempts remain in the evidence history. Rollback removes only the new private
family dispatch/implementation and its admission; frozen upstream files and
old family behavior remain untouched. This cohort can add at most ten official
drawings, leaving the 1,931 target unchanged; subsequent Mindmap/config and
other family cohorts need their own reviewed source obligations.

On 2026-10-20 the user explicitly requires 100% coverage of the complete test
suite. The target is every original record and applicable assertion/option
branch, not merely running every input or recognizing a header. Keep exact
sources and original upstream assertions immutable. A separate reviewed obligation
ledger reconciles source/model bugs, valid empty diagrams, negative probes,
resource cases and foreign private/profile APIs. Any missing native mapping or
unmet portable positive remains pending and blocks completion. Reclassifying a
source fallback because the current renderer cannot draw is forbidden.

The original 2,990 vendor records / 2,916 distinct inputs and all 2,145 official
originals plus 593 separate diagnostics remain accounted for. Original asserted
SVG/browser-only placement is not a universal terminal layout contract; every
adaptation requires an explicit source-backed equivalent or reviewed
non-applicability, never an unrecorded deletion. Preserve original assertion and
model hashes next to any independent correction. Family-level parser acceptance
alone cannot distinguish an empty diagram from a nonempty drawing requirement.

The next disjoint workstreams freeze complete cohorts before implementation:
Class/ER/State native geometry and compartment/cardinality/control assertions;
XY/pie calibrated plot/proportion assertions; and the complete sequence source
cohort with event, marker, actor, activation, control, note and lifecycle order.
The first relation gate covers all 29 Beautiful Class and five ER inputs and
all 28 Termaid State records, including one source-backed error-resilience
control. The first chart gate covers all 22 Beautiful XY and 14 Termaid pie
records, including one valid-empty input. Other records remain on the full
ledger. Pie follows the pinned terminal suite's explicitly documented
part-to-whole bar projection, with shared total, exact values and proportional
lengths; a circular pie geometry requirement would be an unnecessary new
product contract. SVG radial placement options retain provenance and an explicit
profile mapping, not a false circular-rendering claim. XY still requires an
actual calibrated two-dimensional plot; a table alone is insufficient.

#### Private implementation-unit boundary

The owning module remains `loom.ui.visual.mermaid`. Twenty-three diagram families
cannot be maintained coherently as independent parsers/layouts in one growing
implementation file. Do not add an external backend, dynamic registry, plugin
service or exported extension API. Follow the existing Markdown module's
implementation-unit convention inside `loom_ui_visual`.

A first behavior-preserving extraction moves only the existing budget, point,
line-glyph, label-row and cell-canvas primitives into a private
`mermaid_cells_impl.cpp`. Their declarations live in a non-exported `detail`
namespace in the primary Mermaid module interface, visible only to its own
implementation units. Keep the exported factory signature unchanged. Parsing,
family ASTs, layout/routing and source fallback remain within this owner.
Additional family implementation units are registered in the same visual target
only when their frozen semantic tests justify them.

Keeping all families in one file would entangle independent grammar/layout work;
duplicating cell storage, work accounting and source-width safety in each family
would duplicate invariants. This small local boundary centralizes the existing
mechanism without creating a cross-area component. Do not move the flow-specific
label grammar, graph ranking/routing, parser or FTXUI width-selection node into
a speculative universal graph abstraction. The factory remains the one place
that constructs the width-aware diagram/source pair after a complete layout.

The extraction gate requires unchanged private limits, pixel-identical existing
66-input flow and official/terminal reports, all existing oracle/resource tests,
old Mermaid snapshots and native public-factory behavior. Both presets must
build. Independent review approves the design before production edits and
rechecks the exact extraction diff and frozen hashes afterward. Rollback is the
saved, unchanged C2 files in the scratch evidence directory; no user data,
settings or installed binary migration is involved.

#### C2 ER subgraph continuation: design and PRR approved

Independent agent:/root/er_group_design_review approves this source/design and
production-readiness proposal (canonical review SHA-256
`02ae353191b1c3fe96fd36abaf43ea6f3a251b021339967e8fc9961ff1d113cd`).
Concrete test/producer review and unchanged-producer test-first gates remain
required before production edits. Current-source anchors use producer
`a968c651cd31543d3a57fc081aabf90b5114084f88d6d7f156c7725d9e70385b`;
the proposal's older producer spans are historical evidence only. The complete frozen ER inventory has 206 originals, including 15
sources with 22 raw subgraphs. Seven already approved complete Models cover
empty groups, independent ID/title, nesting and member-to-member relationships
across sibling scopes. The other eight group sources retain real whole-source
debts; they remain part of the 100% target. The source/design lock is
`er206-subgraph-source-design-oracle-r1/HASHES.json` under the research audit
root (SHA-256 `554fbfed105b631a97639dc4bb45b004b35abd8bda70f047cf5b03cc72eadfb4`).

The owner remains `mermaid_relations_impl.cpp`, within the existing Mermaid
module and visual target. Pinned ER grammar and DB code establish that a group
claims members when it closes, after inner groups close, rather than when an
entity is first declared. A bounded parse-local candidate list and claimed map
will retain this order. Preserve global source IDs, separate display titles,
empty containers and every statement result; only the completed child group ID
contributes to its parent's candidates. Missing delimiters or failed work checks
reject the whole source. No fixture-name or ordinal dispatch enters production.

Reuse existing Group shapes, stable indices, recursive boxes, common-ancestor
scope projection and structural frame transit. Directly rename only genuinely
shared private ancestry/route concepts already used by Class and State; retain
their algorithms. Do not masquerade ER as Class, set Class namespace flags or
add an ER routing adapter. Empty ER containers need an explicit family guard;
State empty-scope policy and Class-only captions, inheritance and columns remain
unchanged. Every newly activated allocation, relocation, label scan, scope walk,
route object and header-cell visit has a before-use fee. Keep the existing
combined 64 ER entities/groups, 128 edges, 64 groups, depth 16, label/source,
canvas and cumulative one-million production-work bounds.

The first implementation cohort uses all seven unchanged complete Models plus
five source-derived control Models: inner-first ownership, sibling repeated
references with a cross-scope edge, late ownership of a root declaration, a
quoted empty group with a literal keyword title, and a typed aliased member
connected to a root entity. Eleven closed-profile controls protect malformed
syntax and unimplemented valid profiles separately. Missing/extra end, a
missing header separator and an unfinished title are malformed controls.
Duplicate Group IDs, kind collisions, Group endpoints, local direction, in-group
style/class and accessibility are valid-but-unimplemented source profiles. Closed unsupported profiles
receive no drawing or upstream-invalid credit. The executable tests must first
fail on the unchanged producer, capture complete wide/narrow/restored views,
and validate full source-bound geometry, independent following roles, exact
restoration and every applicable semantic/physical negative before production.

The exact Group producer revision3b764 now has independent controlled-trial
retention (`d9421bd3e08677eb9de5bf5459896ac950dcdeb32b2af570ada557da76084d24`).
Both presets build and run Native205 (198pass/7 existing completion reds) plus
six public targets; the matrix records476/2145 successful original drawings.
Seven whole ER sources now pass; the other199 ER and2731 matrix records remain
exact. Twelve control scenes execute68 semantic/12 orphan/24 Group physical
negatives. All91 Native reports match across presets and all13,651 source/test
hashes are conserved during execution. Root proof7cc14c89 preserves real first
outcomes, remaining red gates and UNKNOWN resource fit. Phase C/full100%,
rich-six consumers, serial CTest and real-terminal graduation remain open.
The following design/TEST-FIRST paragraphs retain the earlier review history;
pre-trial statements below do not supersede this limited actual checkpoint.

The presentation clarification is an additional design gate, not an implemented
capability. Existing presentation behavior is preserved for **nongroup ER**.
In a source that contains any Group, a successful body `style`, `classDef`,
`class`, or inline `:::` association anywhere (including before the first Group
or after `end`) must whole-close as valid-but-unimplemented with zero drawing
credit. A statement outside a Group can still target a Group or its member.
Quoted titles/aliases and other literal owners must not trigger this policy.
Independent review `ac45622a002ac9e4c7488192afc49ce5194dd31c590917892bb64df61b5f803d`
requests a locked persistent parser-state/Work contract and complete Models for
two quoted positives before the concrete gate. Its reviewed input lock is
`df02b3c5f41297267cffa68aa3cffe757b77503a3557169fdf9795c70379e310`.
Seven additional whole-closed sources and two literal positives remain
unexecuted proposals. The original eleven controls remain required; this
clarification neither waives presentation obligations nor changes the corpus.

A separate lexical delta is still proposed, not implementable by assertion.
The pinned generated INITIAL lexer tries case-insensitive greedy direction
rules before quoted names; `direction` has no leading word boundary, its
whitespace can cross physical lines, and its orientation has no trailing word
boundary. Header-only quoted-text treatment therefore needs an owning lexical
check; a raw keyword sweep would be incorrect. Alias priority, NUM-first mixed
names and ECMAScript word boundaries also remain explicit source obligations.
Source-only lock `f8feeca0261a78b8986bebd50930db4973eaf0617e8a9067e10ddbdeb0fc61c7`
retains ten whole-null boundary proposals and seven complete positive Models,
including two shared presentation literals. Unknown source validity is not
upstream-invalid credit. The revised presentation-state lock is
`cd95ecfb017da78566b87e040f02fb8568e13aa8d0f97e3cadf8ed4ce5464335`;
it discloses finite new nongroup dispatch fees rather than promising identical
remaining Work. Both deltas require independent review, exact test-first
controls and later concrete review; old Models, limits and debts stay intact.

Independent presentation revision2 review now approves the bounded state and
finite-fee design (canonical
`c6a6f96aa68c78d62a8c23378111098503b6901cbcfbef67ab45c4c3ac83d2f9`).
A concrete test-only appendix is under review: its eleven sources retain all
nine parent controls and add relationship-target `:::` before a Group and after
`end`. Two complete literal Models remain mandatory; nine valid unsupported
sources require whole ordinary output with zero drawing/invalid credit. No
producer trial is approved by this checkpoint. The prior direction review
requested changes; companion lock
`791c225fd69635fb6af21baf5fbdbb90c75e7fd2416bdab6c66d91a7329dbb5b`
proposes owning INITIAL/EntityName predicates and sticky pre-Group support state
for independent review. It neither authorizes a producer trial nor waives any
unknown lexical source.

Companion revision2 now has independent source/design approval (canonical
`1fc9bb56a3ed44fb0ae5fb1406b3aa5cdf4aca60683ca6927a6064c1c92ceb51`).
Owning INITIAL-row and consumed EntityName-span predicates plus sticky pre-Group
support state close its bounded lexical domain without a full-source scan or
second decode. Resource/sizeof/concrete and actual regression gates remain.
The presentation11 appendix has now completed genuine unchanged-producer dual
first-red: both builds pass, each focused TEST fails on two mandatory literal
fallbacks, nine valid unsupported sources retain complete ordinary protocols,
and all89 old reports/13,651 source paths stay exact. Independent actual review
`06938c9017cb15dcce0fed3dd66908f5bc10258f44edc988614f501b26080226`
permits limited test-only retention; it grants no drawing or producer credit.
The separately approved lexical15 R2 concrete test proposal replaces only a
raw CRLF C++ spelling with an escaped literal, preserving all original bytes
and Models. Its actual run is pending. Original seven/five mandatory scenes,
resource controls and full regressions still gate any producer trial.

The remaining paths are explicit obligations: authored local direction must
reuse/generalize the existing private State scope-direction representation after
its own terminal projection review; same-name entity/group roles and group
business endpoints need kind-safe source/Model binding and genuine group-border
ports; repeated group IDs need source completion/lookup-order semantics; styles,
classes and appearance retain their own source contracts. A structural transit
frame is not a group business endpoint. Browser Dagre/ELK placement is retained
as a separate foreign obligation; State inheritance is not evidence of ER's
browser default direction. No missing profile is removed from the denominator.

Graduation requires independent source/design and concrete-code review, genuine
test-first evidence, all seven original and five new complete positive scenes,
all eleven finite closed protocols, exact source/Model/debt identities, and
Debug/Release focused plus complete native/public regressions. Preserve every
previous ER, Class, State and other-family strong scene individually. Current
native rates or source fallback cannot graduate the full suite. Record actual
bounds, build/log/report hashes, oracle failures and reviewed fixes; final
whole-project and real-terminal gates remain required. Rollback is the exact
individually saved owning-file edits and appended tests; no user data, Home
configuration, external backend or installed executable is changed.

#### C2 ER rich-text source contract: source and finite-domain design approved

Agent:/root/er_group_design_review independently approves the source facts and
complete terminal-Model proposals for originals24/65/66/77/78/199. Source lock
`6266b0cd2829395d34853e516a22e6dcc0166411724d1b0c5c2e6a8d815b4994`
and canonical review
`9b766a2f32df7b9f1206e0de0db090f9e393a80eb428f323a66126cf0074dc69`
retain all206 original identities and raw636 entities/437 attributes/409 ordered
relationships/22 groups. The complete inventory distinguishes75 broad marker or
Unicode candidates,15 rich-text debts, these six without additional debts, and
nine still-unreviewed rich/config siblings. The six retain raw IDs, aliases,
attribute fields/keys/comments/order, all relationships and original metadata,
configuration and foreign assertions. No original is selected from its drawing.

The terminal projection reuses the existing rich13 literal contract: preserve
Markdown punctuation and tilde, convert exactly three admitted BR spellings to
LF, preserve Unicode and complete typed-column roles. The raw source Model and
explicit138 raw/display mappings remain separately bound. Browser Markdown,
generic-type expansion, styling/font metrics and SVG/API behavior have real
consumers and remain foreign obligations. This source approval does not certify
those assertions or approve a renderer, codec, fixture/authority change, Native
admission, Official numerator or100% result.

Two of the six complete Models require1896 encoded bytes,47 beyond the old
1849-byte finite62-input test-private domain. Four sources fit the old domain;
reviewing only those would leave the two boundary debts mandatory. The next
design path is an explicit versioned finite all-six domain, rather than a
partial-graph or deleted-field workaround. Its concrete design/owner/fees gate
is still open. It must preserve the old62 complete Models/144 null obligations,
old calls/default1849 guard and all source/config/foreign debts until separately
approved source-Model consumption replaces exactly the six debts. Account for
actual encoded/storage/clone/canonical/publication increases before use; do not
silently widen shared defaults or production1M/source64KiB/Canvas bounds. Reuse
the canonical typed checker. Preserve genuine first failures, all five actual
views before binding/decode, exact applicable negatives, full restoration and
Debug/Release regression evidence. No implementation or drawing is authorized
by this source-only subsection.

The finite-domain design and PRR now have independent approval, restricted to
test-private source consumption (design lock
`456695e487a3bbee8f5cd2059241783dd0c50b989174cc61d9f42f53074bb56f`,
canonical review
`94c004e97aaab59108610c8622295244823efa22dc916c77422aac7f31468dda`).
The chosen concrete proposal will add one small six-row supplement and its
normal source binder, reuse the existing decoder once, and share the canonical
mutation enumeration through exactly two fixed compile-time domains:
`Legacy1849` and same-ledger-only `Rich1896`. The old guard, calls, wrappers,
fees and62/144 authority remain literal. A new receipt, dynamic maximum,
second identity scan, duplicated enumeration or full206 warehouse is unnecessary.
Source binding/good-first geometry and complete mutation proofs stay with the
existing caller/owned-reader boundary; the storage guard is not a source
authenticator.

No concrete code/asset installation or execution is approved by this design.
Concrete review must establish fresh source anchors after the independent
Native EOF additions, actual sizeof/clone bounds, the proposed supplement's
strict key/node/copy fees within65536 bytes, and before-use setup/source/error/
publication ownership. A later all-six Native test-first run and separate old-
default Debug/Release conservation remain mandatory. An active68/138 or Official
join is a further gate and cannot be inferred from source eligibility. The
design's frozen Native203 anchor is historical; its one current input difference
is the independently reviewed Native204 EOF, not a rewritten source fact.

## Phases and graduation criteria

### Phase B design: pinned official capability matrix

Phase A remains graduated. The RFC is reopened for the separately requested
Phase B; agent:/root/corpus_review approved its design and PRR before implementation.
The baseline is b8286503 with 3175 registered tests. The owner remains
`loom_ui_visual` and its native Markdown tests. No shared configuration, model
wire, session, selection or terminal-protocol change is proposed.

Pin upstream tag `mermaid@12.2.0`, annotated tag object
`ee1fdbcebe057cae346232efb6df066215565bec`, peeled commit
`1fb7869866dd619168d840291c9c54b4f129758b`. Its complete
`e2e/diagrams/**/*.mmd` inventory has 2145 files, 37 diagram directories and
1214349 bytes. Vendor every file unchanged with the upstream MIT license,
original relative paths, Git blob IDs and SHA-256 hashes. Record tag, commit,
fixture-tree identity and an exact file inventory in the lock manifest.
Native tests verify hashes, missing/extra paths and totals offline. Updating the
pin is an explicit maintenance operation, never a network fetch during tests.

There are two distinct input tracks with separate denominators:

1. **Original official files (2145).** Wrap each complete original file in a
   closed Mermaid fence and enter the production Markdown renderer. 2139 files
   start with unsupported YAML frontmatter. The other six contain unsupported
   subgraphs, directives, arrow forms or a leading-blank frontmatter block.
   Their expected source fallback is honest evidence of the existing boundary;
   it is not counted as successful drawing or official compatibility.
2. **Body-only diagnostics (593).** The 400 flowchart and 193 sequence files
   also get an explicitly labeled diagnostic view after their frontmatter span.
   The byte offset and derived-body hash are recorded. No diagram statement is
   rewritten, and configuration/title behavior of the original is not claimed.
   These diagnostic results must never be merged into original-file totals or
   called original official passes. They exercise useful existing syntax which
   would otherwise be hidden behind the frontmatter fallback.

Expected outcomes are declared independently of the renderer, not learned from
its output. Each fallback expectation names a concrete unsupported construct
and a source witness. Each drawing expectation contains manually reviewed
semantic checks: displayed labels, node/participant count where observable,
arrow count/direction and relevant ordering. Manifest changes require semantic
review; reclassifying a failure because current code falls back is forbidden.
Large or ambiguous layouts may expect fallback only with a concrete budget or
topology justification consistent with the existing contract.
Drawing cases additionally have reviewed exact terminal glyph snapshots. The
semantic review checks node/edge identity and connection topology against the
upstream body, beyond counts alone; snapshots cannot be refreshed merely to
erase a failure. This reuses the established native Markdown snapshot pattern.

Each row has one of three observed verdicts:

- **drawn**: the expected supported diagram is rendered and all declared
  semantic checks pass. Merely differing from a code block is insufficient.
- **source_fallback**: source was expected and the visible glyph grid exactly
  matches the ordinary source-code renderer at the same dimensions.
- **error**: unexpected drawing/fallback, missing labels/connections, differing
  source output, exception, missing fixture or integrity violation. A process
  crash/timeout also fails the native CTest; it is never counted as fallback.

Use a dedicated native GTest target following the existing Markdown conformance
pattern, linked only to existing UI/utils/test libraries. Exercise
`render_markdown`, real FTXUI Nodes and Screens; do not export parser internals
or create an alternative production parser for tests. Capture glyph cells rather
than comparing ANSI colors. Use widths 240 and 40, bounded adequate height and
a following-text sentinel; reuse each Node for wide/narrow/wide checks where
appropriate. Existing plain-code clipping remains the fallback contract.
Emit deterministic per-case JSON and a readable Markdown matrix under the
build directory, including provenance, expected/observed status, reason and
separate per-track/per-diagram totals. Allow an explicit test report path using
the same kind of test-only reporting seam as Markdown conformance. No floors
that let one failing case be traded for an unrelated success.

Native runner self-checks must demonstrate that the oracle rejects changed
labels, missing arrows, incorrect fallback and manifest corruption. Representative
negative syntax cases remain source, and any discovered production bug receives
a focused regression before its fix. Do not expand the admitted grammar merely
to improve the drawing count. Keep production fixes local to the Mermaid owner.
The initial independent source audit already identified Font Awesome label
introducers and slash/backslash-delimited parallelogram/trapezoid shapes which
the current parser can misread as plain text/rectangles. These are correctness
fixes to whole-input admission, not support expansions. Retain their official
witnesses and add minimal focused regressions which fail before correction.

The first native matrix exposed three admitted cyclic graphs which exhausted
routing work after a declaration-order root or inward-facing back-edge port
encircled remaining nodes. Keep their independent drawing expectations. Ranking
now prefers graph entries, then source-edge order for cyclic components; back
edges and self loops prefer outside-facing ports. These local deterministic
heuristics preserve every original edge and the existing cumulative work budget;
they do not promise the official layout. The three inputs are native regressions,
and their exact terminal snapshots require independent topology review.

Phase B verification includes complete original and diagnostic matrices, both
Werror builds and native serial suites, registration reconciliation, existing
Markdown/conformance/goldens, graph/inline/RFC checks, source/fixture/executable
hashes, independent code/graduation review and a real CLI PTY with screenshots
of representative official-derived drawing/fallback and narrow/wide recovery.
Actual input provenance must be visible in the evidence. The same Loom process
must receive actual PTY resize ioctls, and screenshots must show its output.
macOS is locally available; hosted CI, Linux and PSS remain unverified unless
actually measured. No commit, push or user settings change is authorized.

Alternatives considered: a small hand-picked corpus conceals coverage gaps;
stripping metadata/configuration and counting resulting drawings as official
passes conceals unsupported semantics; running the official browser screenshot
suite cannot validate Unicode terminal pixels. Retaining the complete raw track
plus a separately named body diagnostic track makes the limitation measurable
without introducing a browser/Node runtime dependency into Loom tests.

Rollback removes Phase B's fixture/import/test/doc additions and any individually
identified Mermaid bug fixes; it does not undo Phase A or migrate user data.

### Phase C: complementary upstream terminal tests

The user explicitly corrected the earlier backend-replacement recommendation:
reuse upstream **tests**, not upstream parser/layout implementations. Select a
complementary union rather than one library's narrow subset. Own all production
changes inside `src/ui/visual/mermaid*`; add only private implementation units
when separately owned diagram families justify them. The existing visual CMake
target and native UI test CMake file may register those units/tests. No shared
terminal, Markdown AST, configuration, session, wire, palette or selection
contract changes are proposed. The public factory remains unchanged.

Corpus selection has five complementary inputs:

- Beautiful-mermaid v1.1.3 (commit
  `65f4e0ab8c289e9691a9e5bc4f9eac7108cf449b`): terminal drawing assertions and
  all 100 source/output golden files, including flowchart, sequence,
  Class, ER and XY coverage. State parser/SVG sources remain inventory only;
  public terminal state coverage comes from the other selected suites.
- Mermaid-ascii 1.6.1 (commit
  `5f00e3d9ac9fc96a19859d502333e35b87d2ffea`): all 321 golden files and public
  drawing tests, emphasizing subgraphs, branching/back edges, Unicode and ER.
- Termaid 0.9.1 (`a10dfd506ec67dc11fce3f399c20a353202586ae`): complementary
  authored terminal inputs for its broader diagram families, including all raw
  fixture bytes and exact dynamically invoked input/option variants.
- Mmdflux 2.6.1 (`c7405b64865938b2df4d33d26389d51691409913`): public
  text-rendering inputs and layout regressions, including observed dynamic inputs.
- Merman v0.9.1 (`f6202f702dbde0a75049b1a62b3124feae709b3d`): public authored
  inputs plus independently translated semantic obligations, with typed-model,
  private helper, viewport/encoding and resource API tests inventoried separately.

Each imported project retains its complete applicable licenses, raw test inputs,
original assertion/golden source, immutable version/commit, per-file hashes and
source locations. Imports never copy renderer implementation code. Deduplicate
execution by exact input identity while preserving every source reference and
option/expectation variant. Do not inflate syntax coverage by counting identical
ASCII and Unicode sources separately. Tests execute offline through production
`render_markdown`/FTXUI; JavaScript, Go, Rust and Python are maintenance tools only.

An upstream golden's cell spacing is not a universal Mermaid layout contract:
two source suites intentionally use different spacing, glyphs and algorithms.
Port all independently declared meaning (nodes, labels, endpoint/marker identity,
line style, attributes, message/control order, groups, chart values) into native
assertions. Preserve literal upstream glyph predicates where they are meaningful
for Unicode Loom output. Retain original goldens for provenance and use reviewed
Loom snapshots for its exact geometry. Every adaptation has an explicit mapping;
missing assertion extraction is an unresolved port, not a pass. Private/model-only
APIs that Loom does not expose are accounted for, not reported as executed tests.

Declare positive/negative semantics and required features before observing Loom.
A positive drawing that returns source is a failing drawing obligation. Unsupported
semantics are never marked expected fallback merely because Loom currently lacks
them. Existing width/resource behavior remains separately tested and reported;
intentional narrow-screen fallback is not general drawing compatibility. Invalid
upstream behavior (such as dropping an incomplete edge) is documented rather than
copied into Loom's whole-input parser. A suite with such an upstream bug needs an
explicit independent correction, not a silent changed expectation.

Implementation order is inventory/provenance and native baseline, then independent
semantic review, then local parser/layout corrections by feature family. Extend
this phase's inventory with exact counts and lock the executable oracle before
production changes. The existing official original/body corpus remains intact;
any superseded Phase B unsupported expectation needs a source-backed new oracle,
not automatic golden regeneration from the new renderer.

Graduation requires all admitted public drawing obligations to pass (zero skips,
TODOs, missing facts, unclassified sources or unexpected fallbacks); complete
inventory accounting and duplicate reconciliation; adversarial oracle mutation
checks; both Werror builds, focused and complete native suites with failure history;
architecture/RFC checks; and a real CLI PTY showing newly covered types and
narrow/wide restoration. Record actual drawing coverage and remaining upstream
API/nonterminal exclusions separately. No minimum aggregate score substitutes
for an individual failing positive. No backend/runtime dependency is permitted.

C1 covers the complete import/accounting/execution baseline only. It records
`unverified_drawing`, `source_fallback` and errors; a successful baseline test
proves execution and resize stability, never diagram correctness. Its final
review requires complete inventory/identity locks and mutation rejection.

C2 proceeds in independently reviewed family batches; each batch freezes its
complete inputs, executable semantic oracle and product-bounds mapping before
production changes. Unknown/unported obligations in other batches remain visible
and block overall graduation rather than being silently excluded. Source, node,
edge, label, canvas and cumulative-work bounds are not raised to improve scores.
Foreign over-bound/profile-only cases need independently declared resource/API
policy categories separate from the positive drawing denominator.

Agent:/root/corpus_review approved the first bounded C2 batch after auditing all
66 Beautiful-mermaid flowchart inputs: at most seven nodes, seven edges, 36-cell
node labels, 148 source bytes and group depth three. All nodes are rectangles;
106 solid, four dotted and three thick edges occur across the inputs. Required
features are ampersand Cartesian fanout, nested/empty groups and first ownership,
local group direction, multiline node/edge/group labels with empty lines, and
dotted/thick strokes. Keep 64-node/128-edge/256-label/240x1024/1m-work bounds.
New group syntax additionally admits at most 64 groups and nesting depth 16.
Multiline labels share the existing aggregate 256-cell/glyph budget across all
rows, including retained blank lines; fanout products are preflighted against
remaining edge capacity. These are private admission limits, not new settings.
Group layout uses recursive compound regions: lay out child extents first, then
rank variable-size children and direct-owned nodes in the inherited/explicit
parent direction. Preserve first group ownership and route every original edge
after placement. Perimeter cells permit perpendicular crossings only; corners
are blocked, and crossing glyphs merge perimeter/path bits. Group labels remain
blocked. No post-hoc relocation into foreign group frames is permitted.
Implementation may begin only after the native semantic validator covers node
identity/shape, every edge path/endpoint/arrow/stroke/label, group containment and
ownership, direction and multiline ordering, and rejects corresponding mutations.
ASCII-only glyph assertions are explicitly inapplicable to Unicode production.
Affected older source-fallback expectations need independent source-backed
revision. This approval is not complete Phase C graduation.

The reviewed runtime promotion contains 2,990 vendor records / 2,916 exact
sources, covering 23 recognized families and an unknown bucket. Preserve original
input bytes, all option/expectation variants and the distinct upstream observed
projection versus native desired behavior. Merman's 1,632 sources include 1,605
family-declared drawing obligations, 18 structured semantic-text obligations,
four explicit unsupported-family source fallbacks and five negative probes.
Unreported String output is not evidence of diagrammatic output. These are
pending obligations, not a declaration of all-family native support.

The first 66-input batch now passes its complete native geometry oracle, with
13 handmade oracle mutation tests and four resource/source-policy tests. The
independent source-membership correction preserves the lossy upstream model and
snapshot unchanged and locks its own input witness/facts hash. Fourteen official
rectangle path records (eight exact bodies) reuse the geometry oracle; two
additional decision records require independent snapshot/shape/topology review.
The official 240→40→240 matrix covers their resize behavior separately.

Routing establishes shorter center-distance connections first, using a stable
index copy without changing original edge/rank/source order. Unlabeled distinct
nodes use a multi-source/multi-goal A* search; an admissible consistent bounding
box L1 heuristic and stable queue ties choose shortest feasible ports. Eligible
target landing cells are terminal-only and entered toward the border, avoiding
repeated terminal cells or self-overlap. This keepout is conservative; fallback
remains possible. All O(canvas) setup/search/reconstruction work consumes the
same one-million budget. Labels/self loops keep the earlier bounded routing.
Independent agent:/root/corpus_oracle approved the frozen implementation after
reviewing array/search bounds, movement masks and the complete parser/layout diff.
This completes the first C2 batch only; remaining families/assertions block full
Phase C graduation.

Historical Phase A/B completion evidence
remains unchanged. Rollback removes only Phase C fixtures/runner/docs and the
separately identified local grammar/layout extensions; no data migration or
installed-binary replacement is involved. User explicitly forbids commits/push.

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Native Mermaid | Visual parser/layout, code-fence integration, tests and docs | done | Flowchart and sequence semantic cases pass native renderer tests; direction/branch/cycle/CJK/unsupported/limits/stream/resize cases retain all data or source; real CLI PTY shows completed diagrams plus streaming/fallback states; 3140 baseline tests preserved with 35 additions; debug/release each cover 3175 tests with 3148 passed, 25 skipped, 2 disabled and zero failures; graph/RFC/inline/conformance gates and independent graduation approved |
| B | Official capability matrix | Pinned original fixtures, separate body diagnostics, native matrix runner and justified local bug fixes | done | All 2145 original and 593 diagnostic inputs accounted for separately with zero unexplained errors; provenance/integrity/oracle gates pass; source fallback is not called drawing; complete debug/release verification and exact test registration reconciled; real PTY drawing/fallback/resize screenshots and independent graduation approved |

| C | Complementary terminal suites | Upstream input/assertion union, native semantic port and Loom-owned capability fixes | in progress | Complete provenance and inventory; zero unresolved admitted drawing obligations; semantic oracle review; dual builds/full native suites; real PTY and independent graduation |
| D | Bounded semantic style projection | D1 semantic hue roles on rectangular/rounded entities, lazy CellStyle, bounded legend and theme policy; D2 inline HTML deferred; D3 remaining projections | proposed | Every admitted D1 construct renders its exact reviewed semantic tuple in all seven palettes and under both accessibility flags; style-free inputs keep byte-identical glyphs and Work ledgers; the per-row ledger re-derives every affected official expectation with zero unexplained changes; dual Werror builds, serial suites, real PTY and independent graduation approved |

### Phase D: bounded semantic style projection (design draft)

This subsection is a design draft. It grants no implementation, drawing or
compatibility credit until independent design and PRR review approve the exact
admitted grammar, projection table and fallback rules. D1 (style projection)
is the first sub-phase. D2 (inline HTML) is designed here but deliberately
deferred: the owned corpus contains no admitted HTML witness beyond `<br>` and
two images, so activating it without new source evidence would add a parser and
security surface with nothing to validate. D3 lists the remaining projections.
Every form outside these cohorts keeps the existing whole-source behavior.

#### Measured scope

The complete pinned corpus has 2145 originals. The 1608 originals in the nine
owned diagram families (ten matrix types; state-diagram and state-diagram-v2
are counted separately) are the only inputs this phase can affect:
flowchart400, class285, ER206, sequence193, requirement172, state145, git135,
xychart48 and pie24.

Using statement-level, body-only detection (frontmatter excluded, state grammar
tokens such as `<fork>` excluded, and prose descriptions not counted):

| Cohort | Files | Source fallback | Drawn | Frontmatter (all constructs) | Frontmatter (style-declaration subset) |
|---|---:|---:|---:|---:|---:|
| All owned families, `cssClass` excluded | 178 | 175 | 3 | 64 | 56 |
| All owned families, `cssClass` included | 186 | 183 | 3 | 64 | 56 |
| D1 families (flow/class/ER/state), `cssClass` included | 143 | 140 | 3 | 21 | 13 |

The three drawings are the class-diagram fixtures that declare unused
`classDef`s and assign none of them. The two frontmatter columns differ because
eight flowchart `<strong>` files are style-related constructs but carry no
`classDef` or `style` declaration; they belong to D2 and are deferred. The
style-declaration subset is requirement43 plus state-v2 13 (56 owned) and the
state-v2 13 alone (D1); the all-construct counts add the eight HTML files
(64 owned, 21 D1).

The actual D1 obligation cohort is the 115 files carrying `classDef` or
`style` statements; 98 use only properties admitted below and 17 use at least
one unsupported property (`font-size`13, `border`/`background`4). Eleven files
use `classDef default`, seven use multi-target direct `style`, and five use
multi-target quoted `cssClass`. The other 28 files in the 143 are
inline/cssClass/linkStyle-only or HTML-only; the ten HTML files are outside D1
obligations. The rectangle/rounded target restriction is an additional
source-side blocker not counted in the 17; the ledger records each styled
classic-shape source that stays fallback for that reason. These are source-side
counts, not drawing claims: non-style debts remain in the same files, and the
per-row ledger below decides every disposition.

D1-family `classDef`/`style` property occurrences are fill194, stroke173,
color158, stroke-width75, font-weight19, font-size13, stroke-dasharray11,
border4, background2 and font-style1. Across all owned families the same
statement-only count is stroke221, fill218, color206, stroke-width75,
font-weight55, font-size13, stroke-dasharray11, border4, background2 and
font-style1; the difference is concentrated in requirement and sequence.
Construct file counts in owned bodies are `classDef`102, `style`79, inline
`:::`49, `linkStyle`15, `cssClass`15, `<strong>`8 and other HTML2 (`<img>`).
`<br>` appears in 64 owned-family bodies (77 across all bodies) and remains the
only admitted break form.

The only non-`<br>` HTML in owned bodies is eight flowchart `<strong>` files
(all frontmatter or `htmlLabels` bearing) and two `<img>` files. No owned body
contains `<b>`, `<i>`, `<em>`, `<u>`, `<code>` or `<span>`. D2 therefore has no
admitted witness today.

Phase D claims no drawing-count increase. A row may move from source to drawn
only after its source model is re-derived and recorded in the ledger; the
renderer's output is never the evidence for the expectation.

#### Goals

- D1. Render the admitted style declarations and properties on flowchart,
  class, ER and state entities as semantic terminal attributes, using existing
  design tokens rather than literal colors.
- D2. Render a bounded inline HTML subset inside entity labels. Designed here,
  deferred until the prerequisites in its subsection are met.
- D3. Preserve the whole diagram or its original source. Unsupported style
  syntax, property, value, target, HTML form or bound returns the complete
  source; no partial styling and no partial label.
- D4. Keep the active Loom theme authoritative. The renderer reads
  `theme_provider` once per element; Mermaid `theme`, `themeVariables` and init
  theme keys remain unsupported and retain source.
- D5. Keep glyph geometry, copying, selection, Markdown source, HTML
  serialization, session traces and wire data unchanged. Styles are cell
  attributes; the admitted dashed border and the bounded legend are the only
  new glyphs.
- D6. Disclose every lossy projection in a bounded legend so a diagram never
  appears more faithful than it is.

#### Non-goals

Literal RGB, CSS or SVG fidelity; Mermaid theme configuration; images, Font
Awesome icons or external fonts; links, `click`, tooltips or interactivity;
Markdown inside labels; arbitrary CSS properties (`padding`, `border`,
`background`, `font-size`, shadows); edge, subgraph and `linkStyle` styling;
sequence participant-box and chart-series color projection; HTML admission in
D1; new user settings; a general HTML or CSS parser; any persisted format
change; implicit `classDef default` application in D1.

#### Ownership and dependency direction

The owner remains `loom.ui.visual.mermaid` in `loom_ui_visual`. D1 adds one
private implementation unit, `mermaid_style_impl.cpp`, for declaration
parsing, the static named-color table, hue classification and projection.
D2, when activated, adds a second unit for the inline label parser; one unit is
insufficient there because the label lexer has a different failure model and
security surface. The public factory signature is unchanged.

The renderer reads `loom.ui.foundation.design_tokens` and
`loom.ui.foundation.theme_provider`; this area edge already exists in
`markdown.cppm`, `markdown_api_impl.cpp` and `code_highlight.cppm`, so Phase D
adds no new cross-area dependency. Do not modify `theme_provider` or
`resolve_color`, and do not add a shared color utility: Mermaid's color
grammar, classification table and rejection rules stay local to this owner.
No wire, session, configuration, selection or terminal-protocol change is
proposed.

#### Per-family admitted grammar (D1)

The four families do not share one grammar. Each family admits only the forms
its own source grammar defines; a construct valid in flowchart is not
automatically admitted in class, ER or state.

| Family | `classDef` | Class assignment | Inline suffix | Direct `style` | `cssClass` | Source witnesses |
|---|---|---|---|---|---|---|
| flowchart | yes | `class A[,B...] NAME` | `A:::NAME` on a node declaration or reference, including edge endpoints | `style A` (single target only) | no | classDef43, class24 single+9 multi, inline5 files/10 occurrences, style36 single |
| classDiagram | yes | `class A[,B...] NAME`; `class A` alone is a declaration | `class A:::NAME` only | `style A` (single target only) | `cssClass "A[,B...]" NAME` | classDef9, class73 single+3 multi, class-suffix13, style13 single, cssClass5 multi |
| erDiagram | yes | `class A[,B...] NAME` | `A:::NAME` | `style A[,B...]` | no | classDef9, class6 multi, inline9, style3 single+6 multi |
| stateDiagram(-v2) | yes | `class A[,B...] NAME` | not admitted | `style A[,B...]` | no | classDef11, class10 single+2 multi, style2 single+1 multi |
| requirement | deferred | — | — | — | — | — |

The witness column is a file count from the pinned corpus; it is the admission
authority, not decoration. A form without at least one witness is not
admitted and returns source even when it looks syntactically plausible.
Adding a form requires a pinned source or fixture, a reviewed witness and a
ledger row. This is why flowchart and class admit only single-target `style`,
why class admits only `class A:::NAME` rather than a bare `A:::NAME`, and why
state does not admit inline suffixes at all. ER inline suffixes are admitted
because nine fixtures use `p[PERSON]:::blue`; flow inline suffixes include the
edge-endpoint form witnessed by `A:::someclass --> B:::someclass`.

Requirement is excluded from D1 because all 43 of its style-bearing files are
frontmatter-blocked; admitting styles there would draw nothing and would add a
fifth grammar to review. It returns in D3 with its own source cohort.

Rules common to the four families:

- Targets are entity IDs, never display labels. An unresolved target or an
  undefined class name returns source.
- A `classDef` definition may appear before or after its assignment.
  Definitions and references resolve after the complete parse; there is no
  single-pass forward-reference assumption.
- `classDef default` is not an unused definition. Mermaid applies it
  implicitly to unstyled entities. D1 does not implement implicit application,
  so any `classDef default` returns the whole source. It is never classified
  as unused.
- An unused non-`default` definition is an admitted no-op, produces no visual
  change and no legend entry. This preserves the three existing class
  drawings.
- Precedence is the terminal projection's documented cascade: class
  definitions apply in assignment order, target lists apply left to right,
  and a direct `style` applies last; a later value replaces an earlier value
  per property. This is not a claim of upstream browser CSS source-order
  semantics.
- Multiple inline suffixes, an unresolved suffix, a style on a group, edge or
  unknown entity, or a `class` declaration mistaken for an assignment returns
  source.
- `class Foo { ... }` is always a declaration; `class Foo name` is an
  assignment and requires `name` to resolve to a defined class.
- D1 styles only entity shapes whose interior is the full rectangular cell
  block: Rectangle and Rounded. A style applied to Decision or to any classic
  shape (stadium, circle, double circle, subroutine, hexagon, cylinder,
  asymmetric, trapezoid or parallelogram) returns the whole source. An
  unstyled classic shape keeps drawing exactly as today. This avoids inventing
  per-shape interior masks and dashed-outline glyph tables before their own
  source review.

#### Property and value grammar

Property lists are split on commas or semicolons at parenthesis depth zero, so
`rgb(r,g,b)` and `hsl(h,s%,l%)` values remain whole. A trailing separator is
admitted. Property names are ASCII case-insensitive; keywords and named colors
are case-insensitive; hexadecimal digits are case-insensitive.

| Property | Admitted value | Projection |
|---|---|---|
| `fill` | color, `none` or `transparent` | Semantic fill role; `none`/`transparent` means no fill. |
| `stroke` | color | Semantic border role. |
| `color` | color | Semantic foreground role when the entity has no fill. With a fill, the label uses the palette's default text token and the legend marks the source color as not projected on the fill. |
| `stroke-width` | finite number, 0..64, optional `px` | `<4` normal; `>=4` bold border attribute. Emphasis never changes a glyph. |
| `stroke-dasharray` | `none`, or one or two finite nonnegative numbers separated by spaces, optional `px` | Any nonempty dash list uses dashed border glyphs; `none` is solid. A comma-separated dash list is not admitted and returns source. |
| `font-weight` | `bold` or `normal` | Bold or normal. |
| `font-style` | `italic` or `normal` | Underline or normal; FTXUI has no italic attribute, and the legend discloses the adaptation. |
| anything else | — | Whole source. |

`pt`, `em`, percentages, `calc()`, `var()`, negative values and values above
64 return source. `font-size`, `border`, `background`, `text-align`,
`line-height` and shadows are not projected and return source.

#### Color grammar and hue roles

Admit `#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa`, the comma forms of
`rgb()`/`rgba()`, `hsl()`/`hsla()`, and the CSS named-color set. The named set
is a static 148-entry name-to-RGB table in `mermaid_style_impl.cpp`, validated
at compile time; the sequence box whitelist is only a spelling list and is not
reused as a color source. Reject `lab`, `lch`, `oklab`, `oklch`, `color()`,
`color-mix()`, `currentColor`, `inherit`, `initial`, `unset`, `revert`,
non-finite components and any alpha strictly between zero and one. Alpha zero
is transparent; alpha one is opaque.

Parse the color to HSL and classify it independently of the palette. Roles are
named after hue, not after an assumed user intent, so a pink fill is not
reported as "danger":

| Condition | Role |
|---|---|
| saturation `<0.15`, or lightness `<0.10` | Neutral |
| hue `[345,360)` or `[0,20)` | Red |
| hue `[20,45)` | Orange |
| hue `[45,70)` | Yellow |
| hue `[70,165)` | Green |
| hue `[165,195)` | Cyan |
| hue `[195,255)` | Blue |
| hue `[255,345)` | Purple |

Saturated pastels keep their hue role; `#f9f` is Purple and `#ccccff` is Blue.
The projection uses only existing palette fields:

| Role | Foreground token | Fill token |
|---|---|---|
| Neutral | `palette.text` | `palette.surface_tint_2` |
| Red | `palette.danger` | `palette.danger_tint_bg` |
| Orange | `palette.warning` | `palette.warning_tint_bg` |
| Yellow | `palette.warning` | `palette.warning_tint_bg` |
| Green | `palette.success` | `palette.success_tint_bg` |
| Cyan | `palette.info` | `palette.info_tint_bg` |
| Blue | `palette.info` | `palette.info_tint_bg` |
| Purple | `palette.accent_purple` | `palette.surface_tint_4` |

Orange and Yellow share the warning token; Cyan and Blue share the info
token; Purple has no dedicated tint and uses a surface step. These shared
mappings are disclosed in the legend, not hidden. No RGB literal is emitted by
the renderer. A role that a palette renders indistinctly (monochrome, ANSI16)
still exists semantically and is disclosed; the diagram does not fall back for
a palette capability alone.

#### Projection invariants

- P1. The projection is deterministic: identical normalized source tuples
  always produce the identical semantic tuple. Distinct source tuples may map
  to one semantic tuple; that collision is recorded in the legend and is not a
  fallback reason.
- P2. Every applied class or direct style appears exactly once in the legend
  with its exact source declaration and the semantic role of each projected
  property. Shared roles and unprojected values are marked explicitly. No
  applied style is silent.
- P3. A style-free input produces byte-identical glyphs, identical Work
  consumption, no style allocation and no additional Work charge. In D1, the
  only glyph changes for a styled diagram are the admitted dashed border and
  the legend.
- P4. A style target must be an admitted entity. Styles on groups, edges,
  unknown IDs, sequence participants or charts are not admitted in D1 and
  return source.
- P5. Input, label and canvas limits are unchanged. Phase D adds finite bounds
  below; exceeding any bound returns source.
- P6. `classDef default` returns source in D1.

Normalization folds case, surrounding whitespace, property order, the admitted
trailing separator, and equivalent spellings of one color (`red`, `#f00` and
`#ff0000`). It invents no perceptual equivalence.

#### Bounded legend

When at least one style is applied, append a plain footer block. One entry
corresponds to one distinct applied declaration after normalization: a
`classDef` that is actually assigned, or one direct `style` statement. A
`classDef` and a direct `style` with identical text are still two entries.
At most eight entries are admitted; a ninth returns the whole source.

```
classDef pink = fill:#f9f -> fill:Purple
classDef bold = stroke:#333, stroke-width:6px, color:#fff -> stroke:Neutral, width:bold, fg:Neutral (color not projected on fill)
key: italic=underline; width>=4=bold; shared tokens named; monochrome/ANSI16 may share shades
```

The legend names semantic roles and the systematic adaptations, not palette
colors, so its text is independent of the active theme. A role that shares a
palette token with another role carries a `(shared <token>)` marker; a source
property that is not projected on a fill is marked `(not projected on fill)`.
Each entry is one line of at most 240 cells; the block is at most eight lines.
It is appended to the diagram rows and counted against the existing static
`kWidthLimit` and `kHeightLimit`; it is never wrapped. The current
`MermaidNode` selects only on width, so Phase D does not add a
terminal-viewport-height decision: the existing scroll container owns viewport
height, and only the static canvas bounds can return source. The legend is
never styled and charges Work.

#### D2: inline HTML subset (designed, deferred)

D2 admits `<b>`, `<strong>`, `<i>`, `<em>`, `<u>`, `<code>`,
`<span style="color:VALUE">` and the family's existing break forms in labels of
the D1 families. `<b>`/`<strong>` map to bold, `<i>`/`<em>` to underline,
`<u>` to double underline and `<code>` to reverse video, so the emphasis forms
remain distinct. The span color goes through the D1 classifier. `<span>` admits
no attribute other than `style`, and `style` admits only one `color:VALUE`
declaration. Decode only `&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;` and
numeric `&#NNN;`/`&#xHH;` references, once, after structural splitting.
Decoded punctuation never becomes grammar. Tags do not count toward cell
width; visible text keeps the 256-cell label bound, with a 1024-byte
per-label source bound and at most 64 tags.

D2 activation requires all of the following; none is currently established:

1. At least one source-derived admitted witness per target family. Today the
   only owned non-`<br>` HTML is eight flow `<strong>` files and two `<img>`
   files, all blocked by frontmatter or rejected image syntax.
2. A per-family decoder-order review that proves the existing class annotation
   path (`&lt;&lt;interface&gt;&gt;` and its encoded pair, 68 currently drawn
   class files) and the ER comment path that preserves literal `<strong>` text
   are unchanged.
3. An explicit rule for edge labels, titles and aliases; D2 does not silently
   widen them.
4. A quoting rule that reconciles the span attribute's double quotes with each
   family's label lexer. Unquoted span syntax is not admitted, and the existing
   embedded-quote rejection is preserved.
5. Security negatives and a bounded parser review.
6. A run-style channel extension for double underline and reverse video. The
   D1 `CellStyle` carries only foreground, fill, bold and underline; D2 cannot
   honestly claim distinct `<u>` and `<code>` projections without it.

Until then, every HTML form other than the existing break forms returns the
whole source exactly as today. D2 is not an implementation obligation created
by this draft.

#### Architecture

`detail::Cell` stays a glyph/bits/blocked/allowed record. `Canvas` owns a
parallel `std::vector<CellStyle>` allocated lazily on the first styled write;
style-free diagrams pay no allocation and no per-cell size growth. `CellStyle`
carries a foreground role, a fill role, bold and underline. `Canvas` gains
style-aware `put`, `text`, `multiline` and `segment` overloads and a
`styled_lines()` projection; the existing plain `lines()` remains for tests,
ASCII and the fallback path. D1 styles the whole entity label, its box border
and its interior fill; it does not introduce per-run label styling. Styled
label runs belong to D2.

`finish_mermaid` gains a private `StyledRows`/`StyledRun` overload that takes
`Work&`; a plain `lines()` result is converted to one default-style run per
row, so the element-construction path is shared. Width is the sum of run text
widths, never a re-measure of a concatenated string; exactly-at-width and
width-minus-one regressions guard the known `hbox` shrink path. The legend is
appended as plain rows before the fit check. The active theme is read once per
element; parsing and layout stay theme-independent and deterministic.

Style parsing is attached only to the token branches that already reject
`style`, `classDef`, `class`, `cssClass` and `:::`; there is no full-source
pre-scan. Entity styles are held in a table keyed by the existing entity index
and consumed by the family's entity/box drawing. The explicit ASCII profile
admits only style-free sources: an ASCII-profile source containing an applied
style returns source, so styles are never silently dropped. The existing
`dim_color` decorator still wraps the finished element.

Work accounting remains the existing one-million budget. New before-use charges
cover declaration parsing, color parsing and classification, tuple
normalization, the first lazy style-vector allocation (a coarse
`max(1, width*height/4)` units), StyledRows and run-container growth (4 units
per row, 8 per run), legend rows and styled Element runs. Style-free inputs
incur none of these charges. The source limit stays 64 KiB, the canvas
240x1024, and every existing node, edge, participant, group and label bound is
unchanged. New finite bounds: 64 class definitions, 8 applied class names, 8
legend entries, 8 lines per legend, 240 cells per legend line, 64 property
components per declaration, and 64 characters per class name.

#### Sub-phases and graduation criteria

| Sub-phase | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|
| D1 | Style declarations, hue roles, lazy CellStyle, StyledRows, legend, theme policy; flowchart, class, ER and state entity styles | proposed; design approved | Every admitted construct renders its exact semantic tuple in all seven palettes (dark, light, dark-daltonized, light-daltonized, monochrome, light-ansi, dark-ansi) and under `high_contrast` and `force_monochrome`; style-free inputs keep byte-identical glyphs and Work ledgers; the per-row ledger re-derives every affected official expectation with zero unexplained changes; new style and negative fixtures pass; dual Werror builds, serial suites and real PTY pass; independent graduation approved |
| D2 | Inline HTML subset and styled label runs in the four families | deferred design | Its six activation prerequisites are met and independently reviewed; then the same conservation, theme, security and PTY gates as D1 |
| D3 | Requirement styles, sequence participant-box colors, chart-series colors, `linkStyle`, subgraph styles, `font-size`, `classDef default` semantics | deferred | Separate source/projection review; no obligation is created by this draft |

Phase D does not advance Phase C. Phase C's remaining families and semantic
obligations keep their own denominators.

#### Per-row official ledger

Before any matrix expectation changes, a source-keyed ledger under the
research audit root records for each affected original: prior expected and
observed status, the style constructs present, the remaining non-style debts,
the D1 disposition (still source or reviewed drawing), and the evidence
source. The matrix test's `expected_per_state` and snapshots change only from
that ledger. Fallback rows not re-derived remain byte-identical. The ledger
covers the 115 D1 style-declaration files (3 drawn, 112 fallback) and
separately classifies the 28 inline/cssClass/linkStyle-only files; the ten
HTML-only files are D2-deferred and outside D1 dispositions. It records which
of the 112 remain blocked by frontmatter, grammar, shape, layout or
configuration after D1; the 178/186 cohort counts are not a substitute for
that per-row disposition.

#### Verification plan

Add native semantic tests for statement admission per family, multi-target
forms, `classDef default` rejection, declaration-versus-assignment
disambiguation, the paren-aware property splitter, value bounds, the named-color
table, hue-classification boundaries, precedence, collisions and shared roles,
legend identity and bounds, all seven palettes plus accessibility flags, and
ASCII rejection of styled sources. Add Work-ledger comparison tests proving a
style-free input consumes exactly the same budget as before and a styled input
charges the style vector, StyledRows growth, legend rows and runs before use.
Render through the production factory and assert semantic cell styles from the
real FTXUI screen, not ANSI snapshots; glyph snapshots remain the existing
grid comparison.
Exercise widths 240 and 40, exactly-at-width and width-minus-one, and
wide/narrow/wide restoration. Re-derive affected official expectations through
the ledger; do not refresh a snapshot to erase a failure. Extend the real PTY
gate to all seven palettes, accessibility flags and copy/selection. Record the
test count, conservation counts, theme matrix, ledger deltas and hashes.

#### Alternatives considered

- **Do nothing.** Zero risk, but 186 owned-family originals contain style,
  class or HTML constructs and the current disclosure is limited to sequence
  boxes and chart markers. The user explicitly requested the bounded
  capability.
- **Literal RGB or ANSI passthrough.** Highest color fidelity, but violates the
  no-hardcoded-RGB rule, breaks theme contrast, makes copy and snapshots
  terminal-dependent and turns a source color into an unverified rendering
  claim. Rejected.
- **Categorical first-use roles.** Assign roles by declaration order so every
  distinct color stays distinct. Preserves distinctions but can render a red
  as an unrelated hue, which is a false claim. Rejected.
- **Strict injectivity with whole-source fallback on any collision.** Rejected
  after review: the projection is deliberately many-to-one (hue buckets,
  shared warning/info tokens, `stroke-width` thresholds), so this would make
  common diagrams fall back while the legend already discloses the collision.
- **Implicit `classDef default` support.** The correct upstream behavior, but
  it needs its own per-family precedence review. D1 returns source instead of
  silently ignoring the default class; it returns in D3.
- **Full HTML/CSS parser.** Larger attack surface, no terminal equivalent for
  most constructs and no bounded source model. Rejected.
- **Text-only style disclosure.** Append the exact source style without any
  visual projection. Safe but does not answer the fidelity need; retained as
  the legend component of D1, not as the whole feature.

#### Open questions

- D-OQ1 (owner: agent design review). Legend wording and placement. A concrete
  format is frozen above; acceptance requires width, height, collision and
  snapshot tests.
- D-OQ2 (resolved in this draft). Monochrome and ANSI16 role collapse is
  disclosed by the theme-independent legend key line, never by source
  fallback. Acceptance still requires an e-ink/monochrome PTY and a copy check.
- D-OQ3 (owner: agent design review). Whether D3 should implement implicit
  `classDef default` application or keep rejecting it. D1 rejects it; the
  decision does not block D1.
- D-OQ4 (owner: agent design review). Whether `font-size`, `border` and
  `background` earn a later terminal projection. D1 rejects them; 17 D1 files
  are blocked by these properties and stay source until a reviewed mapping
  exists.

#### Phase D production-readiness notes

- Correctness/tests: new semantic and negative suites; per-row ledger; glyph
  and Work conservation; no source requirement is removed.
- Build: one implementation unit for D1 and private interface declarations.
  Measure the Mermaid interface recompile fan-out before and after; the
  affected set is expected to be the six implementation units, three test
  translation units and `markdown_render_code_impl.cpp` (about ten units), not
  the unrelated `design_tokens` fan-out. Measure BMI PSS on the Linux dev box;
  macOS cannot provide it. Dual Werror presets; no new third-party textual
  includes.
- Rollback: D1 is one independently revertible change set: the style unit, the
  lazy style vector, the StyledRows/finish overload, per-family admission,
  legend and docs. D2 and D3 are separate change sets. The default path and
  every existing snapshot are unchanged, so rollback does not migrate data or
  settings.
- Observability: UI-only; no session, dump-prompt, wire or metrics change. The
  legend is deterministic and bounded.
- Documentation/deletion: update the user guide's supported-syntax table with
  the admitted per-family forms, the exact role/token table, the shared-token
  disclosures and every adaptation. No dead renderer or obsolete style path
  exists to delete.
- Compatibility: no public API, persisted format or established contract
  change.

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
logs. Phase A was subsequently committed and pushed at b8286503 after explicit
authorization. Phase B must remain local and uncommitted under the current
user instruction; no push, settings change or user-installed binary replacement
is implied by this validation task.
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
Phase D extends this plan with the semantic style/HTML matrices, the
all-palette theme gate, glyph-grid conservation evidence and the affected
official-expectation re-derivation recorded in its own subsection.

## Documentation impact

Markdown renderer guide, RFC index and design decision register. No public CLI,
config paths or platform protocol conventions change. Phase D additionally
requires the user guide's supported-syntax table to state the admitted style
properties, the color-to-role table, the HTML subset and every disclosed
adaptation; the exact source-to-role mapping is part of the feature contract,
not implementation detail.

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

### Phase B validation

Evidence is retained under `build/mermaid-official-validation/`. The independently
locked Mermaid 12.2.0 corpus contains 2145 original files (37 types,
1,214,349 bytes), plus 593 separately identified flowchart/sequence body
projections. The native matrix evaluates 2738 inputs at 240 → 40 → 240 columns:

| Track | State | Drawn | Correct source fallback | Error |
|---|---|---:|---:|---:|
| Original | 240 / restored 240 | 0 | 2145 | 0 |
| Original | 40 | 0 | 2145 | 0 |
| Body diagnostic | 240 / restored 240 | 24 | 569 | 0 |
| Body diagnostic | 40 | 19 | 574 | 0 |

Original drawing coverage remains 0/2145; transformed diagnostics do not improve
that figure. The independent audit declared the 24 positive expectations before
renderer execution. All 24 final terminal snapshots and the sole changed existing
cycle snapshot were reviewed against source topology by agent:/root/corpus_review.

The three minimized grammar regressions failed before the admission corrections.
The first complete matrix failed on three admitted cyclic graphs; expectations
were retained and the local root/port corrections made all three draw. Temporary
reachability/search-budget experiments did not solve the problem and were removed;
no debug logging, added budget or alternate semantic contract remains. Preserve
first-pass failures and the routing frames rather than replacing them with the
later passing run. Debug's final focused suite passed all 37 cases. Seven native
tests were added, with all 3175 baseline test names retained (3182 total).

Both default-Ninja Werror builds passed. Debug's complete serial suite covered
3182 registered names: 3155 passed, 25 skipped, two disabled and zero failures.
Release's first complete serial suite covered the same names: 3153 passed,
25 skipped, two disabled and two scroll/clipboard confirmation failures. Those
two plain-text tests passed unchanged in isolation; the nine-case family also
passed three diagnostic iterations (27 executions). Their initial OSC 52 output
contains the complete selection, while the fake native backend confirmation/file
is missing. The selection/clipboard owner and tests are unchanged, and plain
`text` fences do not call the Mermaid owner. This narrows the failing layer but
does not establish the precise intermittent backend cause or a pre-existing bug.
Keep first-pass failure and rerun outcomes separate. The second complete frozen
Release serial run passed: 3182 names covered, 3155 passed, 25 skipped, two
disabled, zero failed. Both selections are disjoint and their union matches
registration exactly. The initial intermittent clipboard failure remains a
disclosed risk; a passing rerun does not establish its precise cause.

All 2177 source/test/tool/fixture hashes and 222 native executable hashes matched
independently. Debug and release matrix JSON reports are byte-identical: 2738
inputs and 8214 width-state observations each, no matrix errors. Real release CLI
PTY PID 10600 demonstrated 110 → 40 → 110 with two actual resize ioctls and only
one model response during resizing. Separate responses showed complete original
frontmatter fallback and Alice/Bob sequence drawing. Exact pinned inputs were
served by a local deterministic SSE fixture, not a hosted model. Four browser
screenshots show actual PTY bytes; provenance, raw output and hashes remain under
`.codex/visualizations/.../mermaid-official-corpus/` outside the repository.
The test child and fixture servers were cleaned up. Agent:/root/corpus_review
independently approved final implementation and graduation after reconciling all
complete logs, test-name sets, matrix reports, topology snapshots, PTY evidence
and frozen hashes. No shared infrastructure, cross-module API, configuration,
session or wire contract changed. No commit, push, user settings change or
installed-binary replacement has been performed.

## Open questions

Phases A and B have no blocking question. Independent Phase B graduation is
approved with the measured limits above. The intermittent clipboard backend
confirmation cause remains unknown; first-pass failures and the subsequent full
passing run are disclosed separately. Linux, hosted CI and PSS remain unverified.

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-28 | C2 | Default-budget repair implementation and full local regression verified | Local delivery checkpoint | agent:/root/review_er_fix_design and agent:/root/review_runtime approve the retained active-only component/rank/workspace repair; agent:/root/review_evidence verifies source/capture preservation and Fork/Join body binding. Native and Official ER107 are107/107 with independent15-frame/cache/geometry/style/mutant checks; factory max992611 under unchanged1M, exact/short-one pass. Fresh Debug-j4/Release-j1 full suites each register4107:4080pass/25skip/2disabled/0fail;552.47s/784.16s. Initial global-model runs5/6fail and fixture32/34 were corrected before these fresh runs. Flow77/400, body14/14; roster78 and frozen assets unchanged; reports dual byte-identical. ER107 formal admission and Phase C remain open. |
| 2026-10-28 | C2 | Earlier ER107 default-budget design trial; superseded by the final checkpoint above | Historical pre-final trial | agent:/root/review_er_fix_design approved limited component-compaction and existing-ranked-layout trials; all Work/output fees and 1M cap remain fixed. Compaction trial reaches 94/107 factory/geometry positives with exact/one-short boundaries; 13 routing failures remain. At this earlier trial, independent Native/Official, full Debug/Release CTest and final implementation review were still open; no ER107 formal admission. |
| 2026-10-19 | — | Opened provisional | Local | Baseline 1cdec8cf; 3140 tests; production mermaid fences currently render as code |
| 2026-10-19 | — | Design/PRR requested grammar and copy corrections | Local | agent:/root/generation_review required closed admitted grammar and distinction between screen-cell selection and raw message Copy; cumulative routing work and default-false AST closure clarified |
| 2026-10-19 | — | Design and PRR approved after corrections | Local | agent:/root/generation_review approved strict grammar, copy contracts, resource bounds and visual ownership; no unresolved design blocker |
| 2026-10-19 | A | Implementation started | Local | Parser/layout owner separated from Markdown integration/tests; no external runtime or shared selection changes |
| 2026-10-19 | A | Independent semantic review required stricter admission | Local | agent:/root/generation_review and agent:/root/settings_audit identified Mermaid hash entities, bidi marks, reserved flow end, sequence quote/wrap semantics and unsupported flow escape decoding; retain whole source for these constructs and add regressions; no new syntax interpretation introduced |
| 2026-10-19 | A | Long-preface Markdown bypass reproduced and corrected | Local | Native DiagramAfterLongPlainPrefaceStillUsesMarkdownParser failed when the existing marker scan inspected only 500 characters; scan the complete source with early return, preserving the parser and ordinary renderers |
| 2026-10-19 | A | Full-scan numeric complexity correction | Local | agent:/root/generation_review identified repeated digit-run scanning after removal of the 500-character cap; advance past each scanned run and cover 64-KiB digits, numeric prefaces and following delimiters |
| 2026-10-19 | A | Implementation and parser admission approved | Local | agent:/root/generation_review approved the complete implementation after semantic and linear-scan corrections; agent:/root/settings_audit independently approved UTF-8, label, reserved-ID and resource-admission behavior |
| 2026-10-19 | A | Graduated; RFC implemented | Local, uncommitted | agent:/root/generation_review independently approved final evidence: debug/release each 3175 covered = 3148 passed + 25 skipped + 2 disabled, zero failed; 51 focused passed; four exact snapshots; real PTY streaming/resize/copy/global instruction evidence; every frozen source/snapshot/executable hash matched |
| 2026-10-19 | B | Reopened for official corpus design/PRR | Local, no commit | User requested fixed official .mmd corpus and measured drawn/source/error matrix; mermaid@12.2.0 resolves to 1fb7869866dd619168d840291c9c54b4f129758b; 2145 original fixtures include 2139 leading frontmatter blocks; 593 separately labeled body diagnostics proposed without altering original inputs or claiming full compatibility |
| 2026-10-19 | B | Design and PRR approved; implementation started | Local, no commit | agent:/root/corpus_review independently verified pin/tree/inventory and approved separate denominators, per-width outcomes, adequate screenshot height, independent semantic witnesses and reviewed topology goldens; agent:/root/corpus_oracle supplied 24 source-reviewed body candidates and concrete parser-admission traps |
| 2026-10-19 | B | Implementation and semantic snapshots approved | Local, no commit | agent:/root/corpus_review approved 24 official-derived terminal snapshots and the sole changed existing cycle golden; first matrix 21 drawn / 569 fallback / 3 errors corrected to 24 / 569 / 0 without rewriting expectations; grammar regressions reproduced before correction |
| 2026-10-19 | B | Graduated with explicit coverage and failure history | Local, uncommitted | agent:/root/corpus_review approved: debug and second complete release each cover 3182 names = 3155 passed + 25 skipped + 2 disabled, zero failed; release first-pass two unrelated-path clipboard confirmation failures remain disclosed; 2738 inputs / 8214 width states per preset, original drawing 0/2145 and body drawing 24/593, zero matrix errors; 2177 source/fixture and 222 executable hashes stable; real PTY screenshots/provenance verified; no user settings/install/commit/push changes |
| 2026-10-26 | C2 | Block58 complete B2 approved; dormant private payload foundation verified | Local, uncommitted | Ramanujan independently authored36 full primitive boundary fixtures/12 source types/7 headsets/two widths/272 rows; Main added whole-mask/interior/port/source/asset checks and four connected-stem controls. Initial0/2 thin-line mismatch corrected with complete pixel contours and subroutine tees; all23 preflights pass. Harvey approved full B2 design/PRR after caption/port precedence fixes, then dormant payload static review after wide-combining-tail correction. Main implemented one private validated row/role payload for cached+uncached paths; no Block dispatch or public API/shared Canvas change. Full builds and final866/866 Mermaid tests pass Debug208.67s/Release47.92s, preserving first Debug865/866 Timeout180.04s and unchanged isolated132.93s/full143.54s success for the affected pure-data guard. Frozen58 glyph-inclusive estimate962940 is not actual prepayment or long-tail proof. Mixed-generation old-join refusal resolved by real Release matrix refresh and dual joins; both final630 matrices SHA81366f10/body593 and419 baseline source/report hashes remain exact, four expected code/test changes only. RFC31 and UI graph checks pass. Producer/styled runtime/Native+Official Block admission and1931 target remain open. No commit/push. |
| 2026-10-26 | C2 | Block58 executable test-only trial; complete production gate pending | Local, uncommitted | Harvey approved the limited test-first scope, independent source-binder implementation and three projection corrections. Ramanujan authored the frozen-source binder; Main authored source scheduling/masks/routes/Work worksheet and project-native GTests; Mill authored pre-production glyph candidate. Full860/860 Mermaid tests pass both presets, then final20/20 affected tests pass after two added contour goldens/frozen-content assertions. All58 plans/owned row sets pass; final candidate upper767762/cold656426,119295 glyph bytes, zero production captures/drawing credit and no verified production prepayment. First5/8 and26/58 parent-order failure and independent fee/gate/style/glyph/golden defects remain disclosed. Prior423 source/report hashes, full630/2145 matrix and body593 stay exact after fresh old10/4 joins. All12 complete boundaries, seven headsets, style/metadata equivalence, cache and actual production Work still require B2; no producer/public/shared contract/dependency/commit/push change. |
| 2026-10-26 | C2 | Block58 terminal source facts approved; full B2 still pending | Local, uncommitted | Harvey independently approved partial B2 facts/provenance for terminal proposalb7d0bf62:58 exact source/model/witness/manifest joins,415 captions including2 preserved single spaces and4 entity replacements,381 logical placements,49 style atoms and122 required annotation records. Current-contract binding now uses11 groups/13 exact segments plus6 quotations instead of circular whole-RFC equality; a subset/full-array hash defect was corrected to9c8c2b60. Current58 Models/bindings remain exact and authored stroke-width is closed to witnessed2px/4px. Mill saved a corrected complete-contour proposal; Main caught false shape distinctions, notch orientation, marker-only arrow shortcuts, unnormalized bounds, ambiguous digital edges, unbounded caption search and an understated note maximum. Actual maximum notes are14 records/48 rows/1176 cells/appended height62; total122/386/9179/508. A top-left3-edge fan-out disproves the zero-apron disjoint-port route rule. A proposed one-cell apron and three disjoint hand routes are saved but not adopted. Exact coordinates/ports, full58 geometry/Work/sizeof feasibility, styles/cache, Native/Official consumers and independent B2-B4 remain pending. Read-only consistency/negative controls and RFClint31 pass; these are not native/runtime tests. Formal630/2145 and prior842/842 runtime results remain unchanged; no shared module/public contract/commit/push. |
| 2026-10-26 | C2 | Block58 full-source review approved; terminal design pending | Local, uncommitted | Ramanujan independently derived all58 complete Block sources/24,239 bytes/279 leaf identities/60 composites/18 edges/42 spacer slots/2,887 byte-line witnesses. Harvey approved B1 after correcting the RFC witness count; source asset1d0006ab references reviewed proposalc8ef03ea, preserving every nested canonical model/witness and null terminal projection. Harvey also approved the conservative498-source direct-theme census and58draw/434source/6error arithmetic:440 unadmitted cap optimistic total1705, requiring at least226 new admissions from this pool. Mill approved only owner-local architecture/proportionality after single-payload, fixedK=2 storage, whole-glyph, atomic-style and cache collision corrections. Precise contours/ports/routes/base-fill/metadata/Work feasibility remain B2, independent source-derived terminal facts are being authored, and no Block production/test implementation is authorized yet. Baseline423 Mermaid source/report files were hashed; dual matrices remain81366f10 and630/2145(29.37%). Current-turn runtime gain is0, target1931 needs1301. RFC lint31/31 passes; no new build or runtime test claimed. No shared module/public contract change, commit or push. |
| 2026-10-26 | C2 | Shapes4 dual-consumer admission verified | Local, uncommitted | Main integrated the private shaped-tree layout with Bernoulli's independently authored appended pure Shapes4 authority and Feynman's independent Official capture/admission consumer; Original10 authority prefix is byte-exact. Full builds and Mermaid CTest842/842 pass in Debug155.98s/Release31.40s, including PTY4.50s/1.96s. Each consumer closes4 complete originals/5 nodes/1 parent/11 semantic+16 physical controls; non-scoring ports, mixed trees, six-level mixed and budget-fitting16-level trees, CRLF, combined glyphs, source/projection guards and bounded fallback controls pass. First focused5/6 (incorrect text-color ordinary control) and11/12 (oversized16-level positive) are retained; fixes preserve exact foreground/background equality and the fixed1M Work cap. Kuhn implemented the metadata-only Shapes4 join; Galileo independently approved after exact-repository build-path validation, report-publication isolation and all runtime evidence. Twelve join self-tests pass; fresh616+10=626 prerequisite is preserved, then atomic Shapes4 join publishes630/2145(29.37%) in both matrices. Four source rows change, no body/other-original rows;24 priorClass/Flow/MM10reports and the old10marker remain byte-exact, and all new frame/compact/matrix/marker files match across presets. Source26f803ff and roster313/b812af2c remain exact. The five-source32-plan proposal is not a claim of32 executed controls or upstream sites. No shared production module/public contract/dependency change and no commit/push. Target1931 still needs1301; Mindmap14/33 leaves19 and phaseC/full conformance stay open. |
| 2026-10-26 | C2 | Shapes4 private parser/layout implementation started; runtime gates pending | Local, uncommitted | Main added the bounded shaped-tree parser/layout after the unchanged Original10 success path, reusing the same remaining Work without reset. Both visual-library builds pass; no factory/module interface or CMake change. agent:Galileo is independently reviewing production code; agent:Bernoulli is implementing the appended pure Shapes4 authority without reading production code. Native/Official captures and complete-source geometry/mutation integration, old negative-profile updates, full builds/regression and external dual admission remain pending. No new official drawing credit; 626 is the last admitted baseline, not a new runtime claim for this working-tree revision. |
| 2026-10-26 | C2 | Shapes4 complete-source/design/PRR approved; implementation pending | Local, uncommitted | agent:Locke independently derived five complete original raw-source Models (8 nodes/3 parents), pinned parser/DB evidence and exact multiline witnesses. Main verified raw/Git/manifest/Model/projection joins; agent:Galileo independently approved source/design/PRR after asset-lock, exact port, mandatory non-scoring per-shape routing and rollback corrections. Four non-Bang/Cloud originals (5 nodes/1 parent) have reviewed raw-framing/display projections; source asset26f803ff references reviewed proposalb8f434d6 and unchanged source lock0d2c09d4. The child port is correctly UDL/bits13. Pure test-private Shapes4 reuse is approved without relaxing Original10 authority. Production code, geometry authority, captures, runtime mutation controls and dual-preset admission are not implemented at this checkpoint. Existing runtime reports and all20 production/test/infrastructure file hashes remain exact; four target originals still whole source in both preset matrices. Formal coverage stays626/2145; no new credit and target1931 still needs1305. |
| 2026-10-26 | C2 | Mindmap10 implementation and dual-consumer admission verified | Local, uncommitted | Euler independently authored the pure source/geometry authority without reading production code; Main integrated Native/Official local captures, source binding and full semantic/physical controls. agent:Beauvoir approved after exact foreground-aware fallback, external dual-consumer admission, closed compact schemas, manifest/raw/hash/mtime freshness, fixed616 base, narrow correspondence and staged publication/rollback corrections. Debug/Release full builds and Mermaid CTest834/834 pass (158.59s/46.68s); four Python join self-tests pass through each preset. All four consumers independently close10 originals/45 nodes/35 parent relations/60 meaningful mutations, then verified external join publishes626/2145 (29.18%) in both matrices. All frame/compact/final-matrix files are byte-identical across presets; only10 original rows change and prior Class/Flow reports remain exact. Source authorityfa675abb unchanged; global roster313/b812af2c unchanged. No foreign assertion-site credit, ASCII-family claim, runtime dependency or public factory API change; appended test probe enum preserves prior numeric values. Target1931 still needs1305; remaining23 Mindmap originals and full upstream/Phase C graduation remain open. |
| 2026-10-25 | C2 | Class20 complete-original gain verified | Local, uncommitted | Final Debug/Release full builds pass; full Mermaid CTest 826/826 with -j8 (162.59 s/36.54 s), after the separately recorded first Release 814/826 stale-roster-count failure. Independent NativeClass285 and Official pass all 20 whole originals: official616/2145 (28.72%), Class231/285, 28 verification-error outcomes overall, 1501 source fallbacks. Official JSON/MD and changed Native Class285/481/Endpoint29/starvation files are byte-identical across presets. Source/Model facts and old drawing frames stay fixed; only20 original matrix rows change, no diagnostics. Reviewed roster313 rows/53898 bytes/b812af2cc7b3db13dcd3bf51d3921f476b0b8446b7bc0fff1c6ff750b696bad5 verifies in both presets. Existing48 Class Model gaps/6 Note obligations and137 Native Class481 pending remain, including one newly drawn Model-null source with zero credit. Milestone1931 still requires1315 more; no cap, config whitelist or public factory contract changed. |
| 2026-10-25 | C2 | Class caption retry and complete-original gain proposed; independent code/ledger reviews approved | Local, uncommitted | Unchanged default greedy plan remains authoritative when it succeeds; only a failed endpoint-caption edge in a flat nonreciprocal/no-note/no-namespace Class scene gets one label-length-first retry. Canvas and A* backing are reused without budget/generation reset. Occupied-only reset skips writes to unchanged blank storage while retaining explicit scan/write fees; retry port/pair allocation and bounded counting-sort fees are prepaid. agent:Aristotle approved after requiring the failed-edge caption guard. Guarded Debug NativeClass285 231/285 and Official 616/2145 have zero new geometry/resource errors; exactly 20 full originals improve, no body rows change and old drawings stay byte-identical. agent:Newton approved exactly 45 resolved gap removals (358 to 313), no additions; the extra Model-null Native Class481 drawing stays within 137 pending and earns zero credit. First Release full suite is 814/826: 12 failures from omitted kGateRowCounts updates, not geometry; all five count constants were corrected. Final rebuild/dual-preset full regression and publication of 616 remain pending. |
| 2026-10-25 | C2 | State brace regression final verification | Local, uncommitted | Full Debug and Release builds pass; full Mermaid CTest 826/826 (Debug serial 1062.73 s, Release -j8 34.38 s), including E2E_Pty_Mermaid. Expanded both-header alias/CRLF controls pass; E2a generator --check stays 69/56/13 and Release E2a 5/5 passes. Full Official matrix JSON/MD and Native Flow400 reports are byte-identical to the previous checkpoint; both gap-roster checks pass at 358 rows/62087 bytes/8ebea8348439d0b93dc9e50fdc6101ce321968141a149f155d89acb66da33f2b. Official coverage remains 596/2145, not 90%; no source cap, cross-module shared behavior, public factory contract, upstream bytes, commit or push changed. |
| 2026-10-25 | C2 | State next-line composite grammar correction and ninety-percent milestone | Local, uncommitted | agent:Turing approved the bounded State parser correction and fixed 1931/2145 milestone. Five-frame independent geometry covers declaration aliases, comments, LF/CRLF and both State headers; malformed scopes and concurrent-region dividers remain source. Debug State 24/24 and Release envelope 7/7 pass at the first checkpoint; final expanded-header/full-suite evidence follows. No official gain is claimed. agent:Carver follow-up review requested path/SHA-bound classification instead of a generic brace detector and accurate five-detector/two-execution-set provenance; both P2 findings were fixed and the final amendment approved. The generator now pins exactly three still-pending complete originals and rejects unreviewed revisions. Class height and longest-first route experiments were rejected: height padding retained three fallbacks; longest-first fixed one but regressed three, so both production experiments and all diagnostic traces were removed. |
| 2026-10-25 | C2 | Mindmap10 bounded source/design/PRR approved | Local, uncommitted | agent:Parfit approved after three rounds. Corrected description plain/single-quoted/>- grammar, exact default-tree/square/wrap geometry, authored-ID witnesses/canonical hashes, path/NUL/ASCII-hex/LF lock encoding, B_i/total-L allocation prepayment and valid paired shape/ID/projection mutations. Existing Class285 pure-authority reuse avoids duplicated Models/checkers, with independent consumer captures/binding/Work/mutations/reports. Reviewed proposal 8307a8358dfe0574797532f174c2c94605277d6bcbb8b0c0e64b7a969aaa75eb; metadata now records source/design approval without drawing credit. Ten originals/45 nodes/35 edges/60 frozen mutation responsibilities and unchanged source bytes; production/test integration, independent implementation review and dual-preset runtime admission remain open. |
| 2026-10-25 | C2 | Mindmap10 complete source proposal frozen for design/PRR | Local, uncommitted | Ten unchanged original files, 45 independent source nodes and 35 parents, exact manifest ordinals/raw/Git hashes and byte witnesses. Source lock cecb9535be0ad801effe77c4c76c653a0c1db3c6a6cb21f378a80f957c3320af; source-only Models were explicitly unreviewed at this checkpoint. A connected-tree/default-node and wrapped-square terminal contract was proposed; agent:Parfit requested changes: description scalar admission, precise wrapping/route geometry, authored-ID witnesses, concrete Work fees and per-original meaningful mutation/consumer ownership. The description forms, authored-ID witness/canonical hashing and per-source mutation/row facts were corrected; exact geometry and conservative Work formulas were added for re-review. The shared immutable authority/pure-checker pattern was accepted on the existing Class285 precedent, avoiding duplicated Models/checkers. Production implementation remains gated pending the final review. No production Mindmap implementation, config whitelist, upstream-option or official drawing credit. The above-90-percent goal remains active. |
| 2026-10-25 | C2 | Bounded ASCII skip-edge source-function port started | Local, uncommitted | Three complete pinned Grid18 functions, seven unique inputs/eight applications; source-only three-node TD/BT DAG profile, existing literal-aware ASCII projection, unchanged Unicode layout and limits. Initial test build exposed two dangling string-view warnings; corrected ownership before execution. First executed Debug run failed all three tests on ASCII whole-source fallback; Unicode geometry remained good. Five-cell upstream label-row lookup is adapted only for interior space padding with both borders and the full label retained; independent review and dual-preset regression required. Phase C remains in progress. |
| 2026-10-25 | C2 | ASCII skip-edge bounded design/source port approved | Local, uncommitted | agent:Ptolemy approved the three-node acyclic ASCII profile, complete original ASCII option/function/assertion bindings, independent geometry and physical negatives, and the bordered-label-row space-padding adaptation. The new fixed fee is local to the new branch; existing LR accounting and default Unicode output remain unchanged. Three complete functions add eight unique direct source sites to the prior joined lower bound (85 to 93); no new helper-call, Official drawing or full-phase credit. Final dual-preset regression is required before delivery. |
| 2026-10-25 | C2 | ASCII skip-edge bounded batch verified | Local, uncommitted | Final Debug and Release full builds pass; Mermaid-labelled CTest is 825/825 in Debug (164.17 s) and Release (49.75 s), with focused native XML 6/6 per preset. Eight distinct original assertion byte intervals match the same vendored source bytes in both presets; eight applications execute 18 original predicates, 48 semantic/projection negatives, 18 predicate negatives and eight rectangular-helper negatives. All eight seven-frame files are byte-identical across presets. Existing Official matrix JSON/MD and Native Flow400 reports remain byte-identical; official coverage stays 596/2145. The initial three source-fallback failures and intermediate label-row-width failure remain recorded; agent:Ptolemy approved the bounded correction. Gap roster and RFC lint pass; whole-suite migration, Linux PSS and final Phase C graduation remain open. No commit or push. |

| 2026-10-19 | C | Reopened for complementary terminal-suite design | Local, no commit | User requested porting upstream tests and maximal combined syntax/case coverage, explicitly rejecting backend integration; inventories and independent design/PRR pending |
| 2026-10-19 | C2 | First bounded flowchart batch design/PRR approved conditionally | Local, no commit | agent:/root/corpus_review independently audited all66Beautiful flowinputs and unchanged bounds; native topology/group/multiline/stroke oracle is a pre-implementation gate; remaining union assertions unresolved |
| 2026-10-19 | C1 | Import/accounting baseline independently approved | Local, no commit | agent:/root/corpus_oracle verified all3836asset hashes and five case-ID sets;1458vendorrecords deduplicate to1388exactinputs; nativebaseline348unverifieddrawing/1040source/0runtimeerrors; three initial nativechecks passed and expandedasset/symlinkmutator rerun passed; semanticcompatibility not established |
| 2026-10-19 | C2 | Native flow semantic pre-implementation gate approved; bounded implementation starts | Local, no commit | agent:/root/corpus_oracle independently approved revised topology/style/content/direction/bounds oracle and recursivecompound design; all12handmade oracle/mutation tests passed; initial66productioncases have22pass/44fail, retainedbefore-fixlog; oldboundsunchanged plusnew64group/depth16admission |

| 2026-10-20 | C2 | First semantic batch implemented and independently reviewed | Local, uncommitted | Initial 66-input batch had 22 passes and 44 failures; final 66 all pass after Cartesian/multiline/stroke/compound-group fixes, stable short-edge-first and bounded multi-source unlabeled routing; 13 handmade mutation tests and four resource tests pass; agent:/root/corpus_oracle approved frozen production SHA 32d8d59c10dfe9117c16dcde0ef0fffc14547d43691650222749d085819a0190; all 40 official drawing snapshots and the old cycle snapshot reviewed by agent:/root/corpus_review; overall Phase C remains in progress |
| 2026-10-20 | C1 | Reviewed runtime corpus promotion | Local, uncommitted | agent:/root/corpus_oracle verified 4526 runtime assets, 678 original assertion slices, 7483 model/config correlation events and 21 upstream suites with 1376 passes; merged 1532 additional Merman exact inputs retaining 100 original IDs/facts and seven fallback-only classification corrections; union 2990 records/2916 exact inputs, 23 known families; native assertion port outside the first batch remains pending |

| 2026-10-20 | C2 | Expanded official and terminal baselines measured | Local, uncommitted | Official raw 2145 remain whole source; body diagnostic 593 has 40 drawings/553 source at240/restored and25drawings/568source at40, zero errors; terminal union2916has760unverified drawings/2156source, zero runtime/resize errors; these do not certify unported semantics |
| 2026-10-20 | C2 | Real release CLI PTY exercised | Local, uncommitted | Release executable SHA084dee5c47ba984647755546bbe9f79c35f7576115da740a3a2f6b22cb885042, PID12461, /private/tmp, isolated fake SSE; frozen group/fanout/multiline inputs rendered; same process110→40→110kernel sizes with only one response prove local fallback/restoration path; five screenshots, exact input hashes, raw bytes and resize/request ledgers retained; no hosted inference or user install replacement |

| 2026-10-20 | C2 | Dual build and full-name native verification complete | Local, uncommitted | Both Werror builds pass; 3204 same test names per preset ultimately 3177 passed/25 skipped/2 disabled; first debug run had30failures (29 model-name snapshot environment differences and one outdated unsupported-subgraph PTY expectation); revised PTY passes and all34 streaming tests pass with isolated recorded-model env, without changing their snapshots/user settings; release3170nonstreaming names pass plus34isolated streaming; three Mermaid JSON reports byte-identical; architecture/RFC/inline gates pass; first-failure history retained, fullPhaseCstillinprogress |

| 2026-10-20 | C1/C2 | Final bounded evidence review approved | Local, uncommitted | agent:/root/corpus_oracle independently verified all 8370 asset hashes, complete per-case provenance/SHA sets, five XML test-name/status maps, three byte-identical cross-preset reports, frozen source/executable/snapshot hashes and five real-PTY screenshots with the kernel resize/request ledger; approved C1 and the first C2 batch only; remaining 21 families and unported assertions keep overall Phase C in progress |

| 2026-10-20 | C2 continuation S0 | Private canvas extraction approved | Local, uncommitted | agent:/root/corpus_oracle approved design/PRR and final implementation: 18 moved function bodies, fields/defaults/bounds and accounting points match C2; both Werror builds succeed; debug/release each pass all52 focused tests and allthree native JSON reports remain byte-identical to C2; only private implementation units and target sources change, exported factory stays unchanged |
| 2026-10-20 | C2 sequence | First-six test design approved; native oracle under review | Local, uncommitted | agent:/root/corpus_oracle verified the complete six Beautiful sequence inputs against the frozen vendor, within the full683record/667exact sequence inventory; 32actors/128messages/64independent notes and old source/label/canvas/work caps; five handmade/inventory tests pass; initial production gate hasthree semantic passes/three source fallbacks, no expectation updates; six original Unicode predicates execute separately; production extension awaits independent native-oracle approval |

| 2026-10-20 | C2 sequence | Limited first-six implementation approved | Local, uncommitted | agent:/root/corpus_oracle approved multiline aliases/messages and case-insensitive over notes; 9 native tests including7handmade/inventory and2semantic pass, six source positives pass allsix original Unicode predicates; 64notes independent from128messages, unchanged source/label/canvas/work bounds; tab-only break rows and all three-known-actor note cap controls are source-preserving; full667sequence coverage remains pending |
| 2026-10-20 | C2 private text safety | Private invariant separation design/PRR and static implementation approved | Local, uncommitted | agent:/root/corpus_oracle approved scalar/roundtrip/256-label checks in private cells unit, retaining family marker admission in flow/sequence; unlike S0 this is an explicit local responsibility refactor, no exported API; dualbuild/baseline evidence pending |
| 2026-10-20 | C2 official sequence | Source-backed oracle expansion and explicit note ownership approved for test authoring | Local, uncommitted | Four newly admitted body records are three exact sources; independent facts retain trailing/middle blank message rows and long note over Alice; identical left/right-configured bodies do not establish raw alignment support; agent:/root/corpus_oracle requires exact top/bottom target ports because wide note containment cannot identify its owner; native positive/mutations must pass before port rendering or snapshots change |

| 2026-10-20 | C2 sequence/private text | Mechanical family extraction and dual gate approved | Local, uncommitted | agent:/root/corpus_oracle independently approved private sequence AST/parser/layout extraction after a leading-empty-statement regression fix; both Werror builds pass and release 12 sequence +20 charts +23 flow +4 official +42 Markdown tests pass; three semantic reports are byte-identical across presets, including unchanged flow report e719b40b...; first36 charts have35drawings+1empty, not full chart coverage; no Go extension is included in this frozen production gate |
| 2026-10-20 | C2 Go sequence | Complete101 source-fact lock approved; native gate in progress | Local, uncommitted | agent:/root/corpus_oracle approved all101 IDs,96declaration witnesses,17safe-display fields and the sole explicit a-br-b two-row correction in separate native facts; vendor SHA7783a2... unchanged; source grammar establishes100drawings+1valid-empty while preserving the original foreign parser-negative; original27source functions/96golden refs and conditional mappings remain independent unmet obligations; first native baseline31pass/70fail, not compatibility graduation |

| 2026-10-20 | C2 Go message/actor | Independent bounded native gates and implementation approved | Local, uncommitted | agent:/root/corpus_oracle approved exact arrow kinds/central markers/numbering/empty-label routes and actor-role figures after adversarial oracle corrections for foreign lifeline arms, self turns on other participants, repeated identical events and wide-cell continuations; complete Go101 progressed from31pass/70fail to48semantic pass(47draw+1empty)/53source, not full compatibility; source/model facts and first6report remain unchanged; private production source fc5e44ee...; current26 native tests have25pass and the complete101 batch remains red until missing capabilities are ported |
| 2026-10-20 | C2 charts | Complete Termaid XY23 native batch approved | Local, uncommitted | agent:/root/corpus_review approved23geometry/0fallback/0oracleerror and28native tests including original1ULP numeric-domain positive, signed/exponent/numeric-prefix oracle mutations and all-tick affine geometry; 21previously passing wide frames and first36 full report remain byte-identical; actual numeric-token bug fixed rather than replacing the triggering source; native every-sample tick density is an explicit terminal projection, not an upstream requirement; fullMerman102 still pending |
| 2026-10-20 | C2 relations | First62 final debug gate approved after source assertions and production findings | Local, uncommitted | agent:/root/corpus_oracle approved28native tests,61drawings+1source-control and62same-node restorations; imported86records=59executed+19ASCIIconditional+8native_geometry_equivalence, plus59source predicates=51executed+8ASCIIconditional with overlap not double-counted; genuine first7predicate failures exposed rank/literal defects, and final review reproduced note-overwrite/long-header losses before local fixes; multi-line LR/RL containment and old62three-state frames pass; full862records/856sources remain incomplete |
| 2026-10-20 | C2 bounded dual | Same frozen family reports verified across presets | Local, uncommitted | agent:/root/corpus_oracle independently verified all151 native names in each preset:150pass and one genuine CompleteGo101NativeBaseline failure;7reports byte-identical, official55/593wide34/593narrow with0matrixerror, Go48/101 at this freeze; releaseCLI8c465962... real OSPTY uses3exact vendor inputs, samechild160→40→160 withnoextraresponse, restoredPNGbyte-identical, child/server cleanup verified; no full-repository or complete-suite claim |
| 2026-10-20 | C2 side notes and identifiers | Independent native oracle and localized debug implementation approved | Local, uncommitted | agent:/root/corpus_oracle approved owned side-note frame/solid-leader geometry and exact physical wide-character cells, then source-derived extended-ID/note-created-actor tests; oldparser and keyword-prefix regressions fail before correction;33native tests32pass withonlycompleteGo101red,57semanticpass/44source/0bad-drawing; first6report/sourcefacts unchanged; production829a7e2f... keeps private interface/resource limits; broader release and new-featurePTY pending |
| 2026-10-20 | C1/C2 Merman XY | Complete102 observations and source-only codec verified | Local, uncommitted | agent:/root/corpus_review independently approved102source/model/config witnesses and compact native facts eeb3c150... before test-only admission;30native chart tests pass,37candidate-art/65source/0observationerror/all102geometry-profilepending; candidate art is not certifieddrawing,108original applications remain separately pending; concrete geometry contract approved for oracle authoring only, production remains unchanged |

| 2026-10-20 | C2 ordered fragments | Limited native implementation and15 source-body candidates approved | Local, uncommitted | agent:/root/corpus_oracle approved sevenkind strictstack/dividers, native exact physical headers/borders/branch ownership, zeroactor empty scenes, actual64/65 and16/17 controls and old resource caps; Work setup accounting P2 fixed with exact64KiB/65537 and aggregated256/257 controls;45debug tests44pass, only fullGo101red at80semanticpass/21source;15source-only official body records/14exact sources pass strongnative and new snapshots independent review, old63snapshot bytes unchanged; generation still genuinely red for a separate newly drawn originalXY width4000 case, no whole-suite or raw metadata graduation |

| 2026-10-20 | C2 frozen fragment/XY boundary | Bounded dual verification and first raw XY admitted | Local, uncommitted | debug/release each run198same native names:196pass plus genuine completeGo101 and MermanXY102 red gates;11reports byte-identical; Go80semanticpass/21source, XY35activegeometrypass/2activesource/65profilepending; rawXY c6c68096... independently reviewed all20points/configuration and original snapshot, normal official4/4each with reportb79b8054..., raw1/2145/body78/593; old78snapshots unchanged; activation five handmade components only, not production; no full-suite completion or installed replacement |

| 2026-10-20 | C2 activation | Limited implementation and dual59 reviewed | Local, uncommitted | agent:/root/corpus_oracle approved private ordered activation parse/owned heavy depth/role-aware intersections and repaired repeated-width-work P2; debug/release59same names each58pass, soleGo101red at89semanticpass(88draw+1empty)/12source; two reports byte-identical and old6bytes unchanged; actual16draw17source/512draw513source+sentinel pass; originalconditions/named-activate/officialsnapshotadmission/activationPTY remain pending |
| 2026-10-22 | C2 ER subgraph continuation | Source/design proposed; implementation not approved | Local, uncommitted | agent:/root/corpus_oracle audited all206 ER originals:15 group sources/22 raw groups, seven unchanged complete Models and eight real pending sources; proposed close-time first-claim ownership plus five complete control Models/eleven closed profiles; design/PRR and executable test-first gates remain open, no source/Model/cap/public-contract change |
| 2026-10-22 | C2 ER lexical ownership | Limited producer retention independently approved | Local, uncommitted | agent:/root/generation_review verified source-frozen Debug/Release Native201:193 pass/eight existing completion failures; healthy terminal numeric entity repair adds original22 complete geometry with14 typed/one physical negative, ER20655 strong/seven mandatory fallbacks/144 whole-null debts; other205 whole case records,200 old statuses,85 old reports and2532 non-ER matrix rows retained; all87 reports match across presets, formal414/2145 unchanged until the separate Official join; review28482dd1a6d88e0298c00dea8a3887d54af10f2b974b8f40d189c8e5c8b53771, no cap/Model/source/public-contract change |
| 2026-10-22 | C2 ER subgraph continuation | Independent design and PRR approved; concrete and test-first gates open | Local, uncommitted | agent:/root/er_group_design_review independently verified12 package files/four primary sources/16 control source hashes,15 group sources/22 raw groups/seven complete Models/eight whole-null debts; lock554fbfed and canonical review02ae353191b1c3fe96fd36abaf43ea6f3a251b021339967e8fc9961ff1d113cd; current a968c651 source anchors retained separately, malformed and valid unsupported closures classified explicitly; no producer/fixture/Model/cap change or implementation graduation |
| 2026-10-22 | C2 Official ER whole-source join | Test-only first actual dual run and retention independently approved | Local, uncommitted | agent:/root/generation_review verified all206 five-view captures/publications before source binding,62 decoded Models/55 complete admissions/seven mandatory fallbacks/144 whole-null debts; formal469/2145,13 failed rows include six unmodeled drawings,resource0; dual Official24 each23 pass/one genuine completion failure,2532 non-ER whole rows/13 public reports exact,13651 source paths/hashes conserved; independent review7706a2ca9ac23a761e35a7a5184a325f28e471bfa4152dc537adca84039c773d; no production/helper/source/Model/policy/cap/public-contract change, no foreign/full-suite/real-terminal graduation |
| 2026-10-22 | C2 ER subgraph test-first | Main16 concrete gate approved and genuine unchanged-producer first-red recorded | Local, uncommitted | agent:/root/generation_review approved pureEOF Native201-to202 test append; Debug/Release build0 and focused new TEST each0 pass/one true failure, five mandatory complete Models remain fallback, four malformed/seven valid unsupported controls preserve five full views/following/restoration; all87 previous reports and13651 source paths/hashes exact, old201 not rerun; Root proofac47f5283e40af58af79eab96b1fe49a548cb0fa6b7ab85f4179991661ccba23; separate resource actual/concrete producer/retention gates open, production a968 and formal469/2145 unchanged |
| 2026-10-22 | C2 ER rich text | Source and complete terminal-Model review approved; finite-domain design proposed only | Local, uncommitted | agent:/root/er_group_design_review independently verified206 source inventory/138 field mappings/six Models for24,65,66,77,78,199, source lock6266b0cd/canonical review9b766a2f; two1896-byte Models exceed unchanged1849 domain47 bytes, versioned finite six-source design/owner/fees/concrete/test-first gates remain open; old62 complete/144 null/raw/config/foreign debts conserved, no source-asset/codec/production/admission or100% approval |

| 2026-10-22 | C2 ER grouped presentation | Independent delta review requests changes; nongroup-only preservation clarified | Local, uncommitted | agent:/root/er_group_design_review verified locked nine source proposals against primary grammar/DB; reviewac45622a and inputdf02b3c5 require successful-call/persistent Group state, exact Work conservation, seven explicit whole-null controls and two complete quoted Models before concrete approval; any Grouped body presentation anywhere remains valid-unimplemented/whole-source/zero credit, original eleven controls preserved, no implementation or coverage gain |

| 2026-10-22 | C2 ER subgraph resource baseline | Independent limited test-only retention approved | Local, uncommitted | agent:/root/generation_review verified dualbuild0/newfocusedTEST1pass, ten whole sources/five lowerModels/five actual UTF8 views and real Markdown code-content equality; upper5ordinary/lower5fitUNKNOWN and zeroOracle guard1, no drawing/internal-cap/productionWork/foreign credit; old202 not rerun,88 old reports each exact and13651freeze conserved; Root proofc3dca1f2 and independent review1252cf598493e2cf9398fba751cc6c63e35f057d65aa16db29525bca6825213c, producer/fullregression gates remain open |

| 2026-10-22 | C2 ER grouped presentation/lexical delta | Presentation design revision approved; separate lexical companion and concrete tests pending | Local, uncommitted | agent:/root/er_group_design_review approved cd95 state/fees/two complete literalModels with canonical c6a6f96a; direction f8fe reviewe29bed45 requires INITIAL/EntityName companion, proposed791c225f is not implementation approval; Root concrete presentation11 pureEOF proposal adds two relationtarget closures, uninstalled/unexecuted, no drawing/invalid/foreign or full-suite credit |

| 2026-10-22 | C2 ER rich-six finite domain | Independent test-private design and PRR approved; concrete gates open | Local, uncommitted | agent:/root/er_group_design_review approved design456695e4/review94c004e9: smallsix supplement/normalbinder/onceexistingdecode/singlecanonicalcore fixedLegacy1849 or sameledgerRich1896, no receipt/dynamicmax/secondidentityscan; source24/199 stay1896, old62/144/defaults conserved, fresh NativeEOF anchors and actualsizeof/key-node-copy/fees/128M/concrete/native/activejoin gates still open; no code/asset/production/foreign/admission/100% approval |

| 2026-10-22 | C2 ER presentation/lexical TEST-FIRST | Presentation11 dual first-red retained; lexical15 concrete R2 approved | Local, uncommitted | agent:/root/generation_review approved pureEOF204 and verified two mandatoryfallback/ninevalidclosed/all11fullviews/zeroops/old89 reports and13651freeze exact; review06938c90/Root proofce4361d6, no good scenes so mutations unexecuted; lexical r1 CRLF P2 retained and r2 escapedliteral fixes source spelling only under review24778165, source companion review1fc9bb56 approved, actuallex15/producer/coverage remain separate gates |

| 2026-10-22 | C2 ER lexical TEST-FIRST / producer concrete | Lexical15 R2 first-red retained; producer requests one local correction | Local, uncommitted | agent:/root/generation_review verified dualbuild0/newTEST1genuinefailure each, five mandatoryfallback/tenunknownvalidityclosed/all15fullviews+fiveModels/42BfourCRLF exact/zeroops, old90 reports each+13651freeze conserved; review407f7c8d/Root proof860eab86, no semantic orGroup physical negatives executed; producer review380830d6 blocks direction-value use of EntityName wrapper, scratch revision required before any actual trial, productiona968/formal469 unchanged |
| 2026-10-22 | C2 ER Group controlled producer | Independent limited actual retention approved | Local, uncommitted | agent:/root/er_group_design_review verified corrected producer3b764/Native205-89b127, dualbuild0/198pass7existingreds/public6 statuses,91 reports dualexact/old87 only ER changed7/199ER+2731matrix records exact,12controls68semantic12orphan24physical,13651live and23344frozen records exact; Root7cc14c89/reviewd9421bd3/lockeab78b85, formal476/2145 observed but full100/rich/productionfit/finalCTest/terminal gates remain open |
| 2026-10-22 | C2 ER rich-six isolated ABI | Independent compile/API/ODR/sizeof retention approved | Local, uncommitted | agent:/root/corpus_oracle verified twoTUs/dualcompile+link+sizeof0 and identical hostABI, clone253824/133760/151168<=524288/setup6865992<=8M/local4288<=16384, overlay3hunks only/current13651sourcehashes exact; Rootcb5efed6/review1b2d589a/lock9065b105; first Root postlink-argv failure preserved, no actualhelper/header/asset install, runtimefit/consumer/admission gates remain open |


| 2026-10-23 | C2 owned source proofs | Pie7 formal join and Flow Source4 Native checkpoint verified; phase remains in progress | Local, uncommitted | Pie7 independently owns seven original five-color-frame scenes and 55 negatives, formal515/2145; full Charts123/124 and Official26/27 per preset retain completion failures. Source4 Native full Flow47/48 per preset admits33/400 with367 pending, four selected sources pass exact8/9/5/7 negatives; 2,000 frames preserved and other396 rows change only by10,432 paid Work units. agent:/root/requirement_design_review approves identity/plan fixes; Root868d362a records dual/source13791 conservation and earlier compile/setup failures. Separate other-four regression515/524 retains nine Relations failures. Source4 Official execution, full five-suite semantics, full CTest and real-terminal graduation remain open; no fallback drawing credit or commit/push. |

| 2026-10-23 | C2 Flow LinkProfile3 owned admission | Three complete originals independently close Native and Official; phase remains in progress | Local, uncommitted | R10 full-nine first pass789/777pass/12fail/zeroerrors per preset freezes13902sourcepins. R12 reviewed test-only immutable mutation batch then runs allFlow53+Official30:81pass/two mandatory gates/zeroerrors each, original525/2145 andFlow39/400 with361pending. Both consumers retain2000frames and397unselected rows byte-exact;3sources each pass6meaningfulmutants,36four-face handgoods108negatives pass, no resourcecredit/cap increase. agent:/root/flow21_producer_author approves R12 limited trial; Root records independent source/frame/report proof. GroupEndpoint15 R3 rejected beforeinstall and central-oracle R4 design only approved; full five-suite semantics/fullCTest/terminal graduation remain open, no commit/push. |
| 2026-10-23 | D | Bounded style design draft authored; two independent reviews requested changes across three rounds; both approved the revised draft | Local, uncommitted | Construct-defined cohorts: 186 owned-family originals including cssClass (183 fallback, three unused-classDef drawings), 178 excluding cssClass; D1 all-construct 143 (140 fallback, 21 frontmatter), D1 style-declaration 115 (3 drawn, 112 fallback, 13 frontmatter). Constructs classDef102/style79/inline49/linkStyle15/cssClass15/`<strong>`8/other-HTML2; `<br>`64 owned bodies/77 all. D1 properties fill194/stroke173/color158/stroke-width75/font-weight19/font-size13/stroke-dasharray11. 98 declaration files use only admitted properties, 17 an unsupported one, 11 `classDef default`, seven multi-target style, five multi-target cssClass; per-family witness counts govern admission (ER inline9, flow inline5 files/10 occurrences; state inline unwitnessed). Reviews rejected strict injectivity, required `classDef default` rejection, witness-gated per-family forms, an exact token table, D2 deferral, a rectangle/rounded target restriction, Work fees and a per-row ledger; the revised draft adopts them. Both reviews approved; no drawing, implementation or commit credit. |


### Subsequent safe-display and typed-family checkpoints (local)

The bounded safe/default-label stage runs64sequence tests in debug and release,
63passing with fullGo101 genuinely red (89drawings+1valid-empty,11source). The
previous unsupported activation source remains present as a strong native
positive; Markdown's first42-test failures are retained separately from the
corrected43/43 verification. Five root reports match across presets. Official
normal4/4 tests and independently reviewed3 activation snapshots yield81/593
body diagnostics while original configured coverage stays1/2145. The prior79
snapshots and2145 original inputs remain byte-identical.

Typed ER41/40 retains250plain profiles with234matches/16source,46pending profiles
and the full296 gate red. Composite XY50/49 retains37active geometry matches
and65pending profiles, with its full102 and original108 assertion obligations
incomplete. Both families have complete dual-preset report equality. The later
positive-baseline XY test-only profile has additional source first-fail gates;
its active/pending split must not be substituted into this earlier checkpoint.

A copied release executable was exercised in a real OS PTY for four exact frozen
inputs: activation/self-return,2bar/2line composite,unsafe-scalar disclosure and
typed ER columns. The same process completed160→40→160; its two wide screenshots
are byte-identical. A local deterministic SSE fixture supplied the responses;
this is actual CLI/rendering evidence, not hosted-model inference. Six images,
input/binary/request hashes, kernel resize records and process cleanup metadata
were independently reviewed. The installed executable remains unchanged.

The next participant-box stage is test-only:74tests/72passes, with only the
new original-source gate and fullGo101 red. All5 box sources map complete typed
members/title/color facts; the lifecycle-mixed source remains independently
pending. Handmade controls validate full timeline sides, real crossings,
separate color compartments, owned footer and exact fragment-caption occlusion.
Production box implementation and all original conditional mappings are still
pending. Phase C remains in progress; no full-suite100% graduation is claimed.


### Participant-box native implementation checkpoint (local)

The source-first box gate now has 76 native tests, including an independently
transcribed official body model. Debug and release each execute all 76: 75 pass
and the complete Go101 gate remains genuinely red. Go101 has 94 semantic passes
(93 drawings and one valid-empty source) and seven unmet source fallbacks. The
previous 90 passes remain passes. Two complete sequence reports are byte-identical
across presets; the six-case frozen sequence report is also unchanged.

The private implementation preserves whole-timeline group ownership, exact
member lanes, separate source color/title disclosure and independent footers.
All 149 named source colors, 18 reviewed functional/hex positives and 24 bounded
source-policy guards pass. These tests do not establish browser CSS appearance
or every original parser-private assertion. Flow23 and Markdown43 regressions
pass in release; their preceding debug checks also passed.

An independent source-only scan of all 193 official sequence suffixes found nine
with authored boxes: eight still require unsupported participant-type metadata,
and one complete role-only body passes the strong native group/message/note/
fragment oracle. Its raw frontmatter still falls back to source. Before updating
that body's declaration, the ordinary official suite correctly reports one
capability-matrix failure in both presets; those first-failure logs are retained.
Snapshot promotion and a real terminal run of this new implementation remain
separate gates. Phase C and complete upstream-assertion coverage remain open.


The independent official box snapshot review and normal dual-preset gate have
now passed: 4/4 official tests per preset, report27e51114... byte-identical,
manifestbe08f24a..., 83 snapshots with all previous82 unchanged. Body diagnostics
are82/593 wide and35/593 narrow; raw drawing remains1/2145. A separate native
opening-limit control raises the sequence count to77:76pass plus fullGo101red
in each preset. Exactly64 empty boxes preserve the bare scene's full pixels;
65 retain the complete source at all three widths. Agent:/root/corpus_oracle
independently approved these bounded results; Phase C remains open.


The next private XY orientation checkpoint passes62/64 native tests per preset;
only complete102/full15 remain red. The source-only10-record/75-field orientation
cohort has2 eligible geometry matches and8 dependency-pending records. Full102
is50active matches/2active source/50pending, full15 is13/2, all6reports match
across presets, and the original48active complete rows remain byte-identical.
Agent:/root/corpus_review approved the limited dual implementation gate. The
original108 assertion applications and full-suite100% objective remain open.


The box/orientation/positive-baseline/ER-gap freeze was exercised with copied
releaseCLI c492ec66... in a real OS PTY at /private/tmp. Four exact frozen inputs
produced eight actual terminal screenshots. The same child completed two
160-to40-to160 transitions (80rows); response counts stayed1 and2 respectively.
Both full wide/restored PNG pairs are byte-identical. Twelve horizontal month
slots and the4000..11000 axis, the positive10..150 baseline with its empty fourth
bar slot, and the complete ER long label/cardinalities are visible. Input,
request, SSE, binary and screenshot hashes, four kernel resize records and
owned process cleanup were independently verified by agent:/root/corpus_oracle.
Verification SHA3afba7b5... records a deterministic local SSE fixture; no hosted
model inference, installation, commit or later implementation is claimed.

An ER endpoint-approach reservation experiment was rejected after a genuine
43-test first run: no new passing profile and one previous geometry success
regressed (239to238). The exact prior production source was restored and its
43-test results/reports recovered. The rejected scratch candidate and original
failure log remain evidence; no resource limit or original expectation changed.

The next ER port-capacity checkpoint is independently approved in both presets:
44native tests/42pass with the complete296 and8-case approach gates still red;
plain ER is244/250 typed-geometry matches,6source,46additional records pending.
All previous239 successes,57non-ER records and the strong mixed-prefix5b56
regression remain intact. Only header-only ER per-face height capacity changed;
no router budget, shared marker contract or typed-table geometry changed. The
final restored debug report bytes match the approved release reports. A later
experimental deletion of a shared connectivity preflight was rejected despite
one ER gain because two old State drawings fell back. Production was restored
to26963fdb; the candidate and genuine first-failure logs remain evidence.

Lifecycle validation is test-first and still open. The independent source-only
union has24 distinct inputs:14drawing positives and10whole-source policies,
covering all six frozen lifecycle records and nine original functions/21source
bindings. The twelve handmade lifetime/activation/note/frame/group scenes pass.
A genuine new codec failure showed all six frozen create/destroy event sequences
were incomplete. The corrected private native converter now compares every
model field against the independent source facts and passes that test. Actual
full92 execution is90pass with the two full-source geometry gates still red;
all14 lifecycle positives remain source fallback. This is native test admission,
not shipped rendering or original private API/ASCII assertion graduation.


The limited lifecycle production gate is independently approved in debug and
release:96native tests/95pass, only the completeGo101 title gate red. All14
source-positive union inputs draw with full geometry and all10native source
policies preserve complete source. Go101 is100matches (99drawings plus one valid
empty), old94complete successful records remain unchanged, and first6report
bytes remain62ee3654.... A new legitimate nested child-cap/parent-destruction
boundary first failed both a production source control and an independent
handmade scene; only the witnessed parentDestruction/childCap equal-finish
case was admitted, preserving exact caps, lifetime absence and LIFO checks.
The official source-fact number sentinel correction and its original failure
are explicitly retained. Three native reports are byte-identical across presets.
The one official lifecycle body promotion remains a separate snapshot gate.

The local ER-only connectivity-preflight change also passes independent dual
review:44tests/42pass,245/250strong matches,5source,46pending. All old244 semantic
matches and all295other full records remain unchanged; the57non-ER/first62
report stays byte-identical. The earlier global-preflight experiment remains
rejected. The self-loop source cohort and original assertions remain open.

The independent lifecycle single-snapshot promotion is now complete: normal
MermaidOfficial4/4 per preset, byte-identical report7f557a6f..., manifestc966a725...,
84snapshots with all previous83 unchanged. Exactly one body diagnostic changed;
all2145raw and592other diagnostics retain their prior declaration. Body drawing
is83/593 wide/restored and35/593 narrow; original drawing remains1/2145. Reviewer
agent:/root/corpus_oracle read the complete new glyph snapshot against the
source-derived five-actor/ten-message/six-lifetime-event model. A copied local
release executable is being exercised separately in a real PTY; no installation
or change to user settings is included in this checkpoint.

The independent real-PTY checkpoint is complete in
`mermaid-lifecycle-er245`: copied releaseae8e10e4, seven actual JPEG captures,
verification4da34643. Both lifecycle scenes execute in the same child at
200->40->200 columns, show complete source at40 and restore byte-identical wide
captures without another model response. The ER capacity scene preserves all
four edges and endpoint cards. The local SSE source/request/response bytes,
kernel resize records, process cleanup and screenshot hashes are locked. Reviewer
agent:/root/corpus_oracle independently inspected all seven images and the
artifacts. This proves the frozen lifecycle/ER245 executable only; subsequent
title and chart work requires its own verification. No installed executable or
user setting was changed.

The named XY/reference-axis implementation checkpoint passes independent dual
review (agent:/root/corpus_oracle):73tests/72pass per preset, seven actual reports
byte-identical,52active strong matches/50pending in full102. Only two full-source
records change; every other100 complete record and all older cohort reports
remain unchanged. Positive-baseline15 is complete. Native axis-owner spur and
exact near-reference disclosure controls retain their first failures and bounded
fixes. The full102 gate, original108 applications, other profiles and Phase C
remain open. Source, cell, Work and assignment caps are unchanged.

The sequence title checkpoint is independently accepted in both presets:
105/105native tests; completeGo101=100drawings+1validempty, with onlyc3b1 changing
and every other100complete record preserved. Source-only raw-title audit finds
zero newly admitted official originals, and normal4/4official tests per preset
preserve all2738observations. Flow23/Markdown43 pass per preset. The private
factory peek forwards the original whole source to strict sequence metadata
admission; no shared metadata service, Canvas, public API or budget changed.
Original frontmatter/API/ASCII obligations remain pending.

The header-only ER same-face triala5c553 is also independently accepted in both
presets:48/46tests,246active strong matches/4source/46pending, self17=14strong
matches/1source/2pending. All old245matches and284unchanged full records, including
typed/CJK,57non-ER and mandatory5b56, are preserved; fourreports are byte-identical.
The remaining parallel self-loop requires further diagnosis, not a cap increase.

Copied releaseb753d17f then passes independent real-PTY review against exact
titlec3b1/XY93f0/ER0916 inputs. Seven actual JPEGs, verificationec481035 and
source/request/SSE/kernel-resize/cleanup artifacts are locked. The short title
remains complete at40; the XY scene falls back to source at40 and restores at200.
Both wide/restored screenshot pairs are byte-identical. The same child executes
all three responses and four resizes without reinference on resize; ER self and
ordinary successor remain visible with the following sentinel. Temporary
processes exit, installed Loom remains unchanged. Reviewer for source, native,
dual execution and PTY gates is agent:/root/corpus_oracle. Phase C stays open.

The complete sequence source-model checkpoint now executes all683 records while
retaining402 source-selected core members and281 additional-event obligations.
Of the core members,249 independently reviewed complete models match in both
presets:247 drawings and two valid-empty scenes, with zero native or observation
errors. The153 missing core models remain unmet. Native108 passes107 per preset;
the complete-core gate stays red. All four actual reports are byte-identical
across presets, with the older106 ordered RUN/OK results preserved. A retained
first failure exposed79 source-fact note-index translations; the independently
reviewed correction748d2b66 changed only those indices and54 derived model hashes.
All683 three-width frames and all629 unaffected complete records remain exact.
Reviewer agent:/root/corpus_oracle accepted this bounded source-schema/native
checkpoint; no producer, shared interface or resource cap changed. Full original
predicates, remaining source models and Phase C are still open.

The value-label stagee28dd6/300d8d is independently accepted in both presets:
86tests/85pass,70complete XY matches/32pending,18new value-label records and eight
byte-identical reports. Exact strict metadata and full owned readouts pass; two
obsolete source-only controls migrate to strong geometry while retaining their
original bytes and first failures. The existing named-singleton guard is retained
without the rejected redundant addition. Original108 predicate applications and
the negative-reference profile remain open.

Parallel-self extent415decea is also accepted in both presets:50tests/49pass,
247matches/3source/46pending,20complete authored controls and20mutation sets. Only
bc08 changes; the other295 complete frozen records, all old246 matches and first62
report remain exact. Five reports are byte-identical. No shared code, exported
contract, source/canvas/routing-work limit or external backend changed.

Additive sequence factsbb051376 and native110 preserve the base249 models while
filling20null entries. Both presets pass108/110;269models yield249drawings+2empty+
18source failures, with133core and281additional-event facts still pending. All683
three-width frames and the older249 dictionaries remain exact. The original
outside-corpus activate positive and foreign14/native16 obligation remain open.
Independent execution review does not replace the separate source-fact review.

Copied released4bea600 passes real-PTY review using exact valued132/namede99c/
parallelbc08 sources. Five JPEGs and verification6bca3e8f retain whole-image
restoration equality, two kernel resizes with no reinference, complete labels,
values/cardinalities/sentinels,42artifact hashes and nine source snapshots. The
temporary child/server exit and tab closes; installation is unchanged. Reviewers
agent:/root/corpus_review and agent:/root/corpus_oracle close these bounded gates.
Phase C and complete-suite100% remain open.


The next bounded checkpoint closes per-message numbering993c9fab/native119 and
generic chart port soundness9b25c283 in both presets, reviewed independently by
agent:/root/corpus_review. Sequence119/117 yields255 strong drawings+2empty and
12modeled source failures;133+281 facts remain pending. Six source-bound gains
and two unmodeled observation changes preserve the other675 complete records,
the prior249 drawings and two valid-empty scenes and the earlier three reports. True118 first reds
precede source-identical migration of two obsolete fallback assertions.

Generic93/92 preserves70 real drawings/eight reports after93/86 genuine first
reds and a reviewed seven-cell handmade correction. Across the six binaries,
19 reports match byte-for-byte across presets; flow23/23,relations50/49 and
Markdown43/43 retain their prior state. Official4/3 remains genuinely red on one
new decimal body drawing: all other2737 complete records,manifestc966 and84
snapshots remain unchanged. An independent full Model and reviewed additive
expectation are required; no baseline rewrite/global candidate-art allowance is
approved. Full100%, original predicates and PhaseC remain open.


The subsequent source-Model stage executes122/120 in both presets and admits
one source-reviewed fractional terminal Model:256drawings+2empty/270Models,
12positive source failures,132core+281event facts pending. Independent dual
review preserves all683 three-state physical hashes and13 actual report bytes.
Official decimal-body snapshot promotion remains a separate reviewed test-policy
gate; the original4/3 failure and raw metadata obligation remain recorded.

Singleton producer30ed1099/native103 then executes103/102 in both presets. Six
source-bound owner keys close six full geometries:76strong+26pending/102,
0geometry/oracle errors. Genuine102/99 bridge and102/100 producer failures precede
the source-identical migration of one obsolete fallback assertion. Eight exact
238-cell ASCII/CJK keys and larger whole-source boundary controls pass. The other
96 complete rows and all70 earlier drawings remain exact; nine reports match
across presets. Independent agent review closes this bounded stage; full100%, original predicates and PhaseC remain open. No source/canvas/
label/Work cap, shared module or public contract changes in these batches.


The independently reviewed additive official body policy8f5/b6c/1b756 executes
6/5 in both presets with complete2738 report byte equality. Only the fractional
body changes against the pre-install24d76 matrix; all2737 other records and the
84 historical snapshots remain exact. Parsed binding mutations pass. A separately
preserved stale4-test binary run is excluded from graduation evidence; release
was recompiled before the accepted6-test run. The sole remaining matrix failure
is the newly drawn raw XY short-title source. Its full source/native Model and
foreign title-omission predicate remain independent obligations; no fixture-based
production guard or permissive expectation was added. PhaseC remains open.


### Inline sequence titles and copied-release terminal checkpoint

The source-bound title gate executes128 tests/126 passes in each preset. The
remaining two failures are the complete683-record gate and an original reserved
participant-keyword case. The complete sequence report retains270 Models:
261 drawing matches, two valid empty states and seven source-positive inputs
still unmet. Another132 core and281 additional-event records need source facts;
observations alone do not qualify as semantic coverage. All20 cross-family
JSON reports are byte-identical between presets. Five title Models become
complete drawings; two additional box/lifetime inputs change observation only.
The other676 complete sequence dictionaries and all2738 official records remain
unchanged. Independent source, execution and scope review approves this bounded
stage; it does not close Phase C or the complete upstream suite.

Copied release62a96d4c is then exercised in a real OS PTY with three exact frozen
inputs: inline Setup title14f1, named bar38399 and fractional messagesa269.
Six real JPEGs,55 artifact hashes,12 frozen source snapshots and exact
source/SSE/request bindings are independently reviewed. The same child11869
receives200→40→200 kernel sizes between responses two and three, without a new
model request. Narrow source and restored complete owner geometry are visible;
the wide and restored JPEGs are byte-identical. The complete-context final
screenshot retains10.1,10.11 and10.12 with their full routes and sentinel.
The deterministic local SSE proves the terminal path, not hosted inference.
Both processes are cleaned up and the installed executable remains unchanged.
The exact review and runtime evidence are recorded under
`build/mermaid-renderer-research-20261020/relations-audit/title-numbering-singleton-independent-real-pty-review.json`
and the `mermaid-title-numbering-singleton` visualization artifact directory.


### Complete seven-text and five-empty native checkpoint

The private sequence text pipeline now shields complete ASCII hash-reference
atoms before statement delimiters, splits actual raw BR tags, decodes the six
reviewed references once and applies the existing Safe display. Only aliases,
messages and notes opt in; titles and structural IDs retain their own grammar.
All seven complete source Models pass, including literal escaped BR, syntax TAB
versus disclosed data TAB, ordered notes, protected semicolons and the unchanged
256/257 display,64-KiB source and canvas/Work limits. Both presets execute136/134;
only the full683 fact-completion gate and outside683 activation-keyword source
remain red. Existing270 Models now yield268 drawings and two valid empty states,
with zero positive-source or native errors. The other132 core and281 additional-
event records still need their semantic facts. Exactly seven modeled rows gain
geometry; c1b8 changes observation only and stays unmodeled. All prior263 strong
complete dictionaries remain identical.

Charts execute119/118 in both presets. The reviewed five no-plot inputs preserve
all declared fields as explicit empty states, independently counted from76
nonempty geometries. Full102 retains21 pending profiles and zero geometry/oracle
errors; the other97 complete rows remain exact. Title token joining, quoted
trimming, earlier-invalid setter rejection, axis lexer-state guards, Unicode,
233/234 title-prefix width and source-preserving metadata controls pass. Five
DenseSource tests also pass, including all ten official CPU points and the
separate mixed numeric source; they do not silently promote that official row
or the existing default full102 profile. The original foreign API/ASCII/SVG
predicates remain independent.

First failures are retained:113/109 exposed unsafe UTF8/bidi matching-model false
acceptance and an incorrect zero-valued-plot fallback expectation; the repaired
native helper runs113/111 before producer admission. The136 hand first-run
129pass includes an incorrectly centered one-cell row; its literal-coordinate
repair runs130pass, including the previously unexecuted TAB half. The first
broad debug/release Markdown run is43/42 because the exact semicolon source is
now correctly one message. Its source-identical migration adds full decoded-label
and single-arrow integration checks plus the same exact full sequence Model;
subsequent Markdown43/43 and Sequence136/134 pass their intended gates.

All21 report files from the first broad dual execution are byte-identical.
The official2738 matrix retains its independently visible raw CPU failure and
adds one genuine unreviewed line-break body observation; no original manifest,
84 historical snapshot, source denominator or expected-source rule was rewritten.
ER's temporary stderr diagnostic preserves all five reports and is removed;
restored415 executes51/49. Its three remaining ordinary-topology sources still
require real drawings. Both endpoint-search failures and the no-port failure
have remaining Work, so a cap increase is not justified. Source/test/binary/full-
log/report evidence is retained under `full-obligation-audit/empty119-text136-*`
and the semicolon-migration logs. Independent bounded review is recorded
separately; full Phase C and the complete upstream100% target remain open.


### Final text/empty dual and real terminal checkpoint

Independent review verifies122 frozen source/test/binary/log/report hashes and
14 complete logs: each preset executes381 unique tests,375 passes and the same
six genuine completion gates remain red. All22 actual JSON reports are
byte-identical across presets. Terminal's three native tests also execute all
2916 exact inputs against2990 frozen identities and verify8370 assets, with
zero observation errors. Inventory execution and candidate drawing observations
do not certify source semantics or complete upstream assertions. Proof is
`relations-audit/text7-empty5-independent-limited-dual-review.json` under the
research artifact root. The preceding21-report first broad run remains separate
history, including its genuine Markdown43/42 failure.

Copied release2faac4c8 is exercised in a real OS PTY with three exact inputs:
decoded heart/infinity942c, complete explicit empty XYbbbb and four-actor/four-
message/four-note line-break source6579. Seven screenshots and146 artifact
hashes are independently checked. The same child88604 receives actual kernel
sizes200→40→200 after its third response, with no fourth model request. Narrow
source, restored complete graph, literal escaped BR and disclosed data TAB are
visible; the two matching-context wide JPEGs are byte-identical. The empty XY
state remains complete at40 columns. Both processes and the browser tab are
cleaned up; the installed executable is unchanged. This deterministic local SSE
exercise proves terminal rendering, not hosted inference or full100% coverage.
Evidence is `relations-audit/entities-empty-breaks-independent-real-pty-review.json`
and visualization directory `mermaid-entities-empty-xy`. The capture-stage
verification manifest is retained unchanged; its pending-review flag is historical
and the separate independent verdict closes this bounded terminal checkpoint.


### Role, official CPU and layered ER partial checkpoints

The separately frozen strict153 sequence checkpoint passes151 tests per preset:
new-role duplicate-header, crossing and endpoint-weight adversaries are closed,
while complete683 and complete31 source gates remain genuinely red. The next
additive161 first-red checkpoint passes153 per preset; its old153 ordered
outcomes and five complete sequence reports are unchanged. The independent
numbered-gap hand scene and closed metadata controls pass before production
role support; six new source-feature groups remain red. These are native oracle
and source-first test gates, not full31/683 rendering acceptance.

Official native execution is8/8 per preset after a reviewed complete raw CPU
projection. All2738 complete result dictionaries agree between presets; exactly
one original-track CPU dictionary changes relative to the preceding projection
checkpoint, and the2737 others plus84 historical snapshots are preserved. The
fixed-cell title/axes/ten samples/owner key are certified only for the declared
native projection; foreign SVG dimensions, fonts and browser predicates remain
independent obligations.

The first proper-DAG ER producer passes54/58 per preset. All248 old strong typed
ER records remain strong;26 complete row observations change frames without
semantic losses, and the twelve inapplicable layout source controls pass. The
complete twelve-node/twelve-edge target still falls back and is not coverage.
Actual temporary stderr observation establishes103x67 measured layout and a
B2-to-C2 endpoint-path failure with remaining Work after ten earlier routes.
The diagnostic is removed, exact producer restored and the complete58/54 plus
all six ER reports reverified. No budget, canvas or source cap increase follows
from this failure. Native first-red/actual execution evidence is retained under
`sequence-audit/role-producer-design/role161-first-*`,
`full-obligation-audit/CPU/first153-official8-*` and
`relations-audit/er-layered-dag-production-first-*`.

The latest additional real-PTY checkpoint uses copied releaseb10afab5, exact
activation-word endpoints, complete official line-break body and six-entity ER
source. Six screenshots show same-child200-to40-to200 behavior after exactly
three deterministic responses. Pixel/source replay closes an accessibility-text
false inference: retained alternate-buffer cells are clipped visibly at40.
Process/tab cleanup and unchanged installed CLI are independently verified in
`relations-audit/activation-linebreak-er97-independent-real-pty-review.json`.
This terminal snapshot predates the new layered ER producer and role tests.
Full Phase C, all independent source/foreign obligations and the requested100%
acceptance remain open; no partial native test total changes that status.

The later actual layered-label correction passes56/58 relations tests per
preset, preserving all248 prior strong ER records and adding the full four-layer
twelve-node/twelve-edge source. The complete296 partition is249 strong, one
genuine source fallback and46 independent pending profiles. Independent review
checks all seven dual reports, the13 changed whole dictionaries and unchanged
other283, first62/class-state, self17, parallel20 and twelve ineligible controls.

Bounded role production and contextual metadata-word handling subsequently pass
166/167 sequence tests per preset. All31 complete role sources now pass their
whole-scene geometry and meaningful mutation gates; five wrap plus four coupled
profiles and the complete683 gate remain open. Independent execution review is
`sequence-audit/role-contextual-id-design/production-C-independent-runtime-review.json`.
Official8/7 retains51 genuine body-expectation conflicts /38 exact bodies; those
expectations are not waived. Source-only complete Models are under independent
review before any geometry or projection admission. This is a partial checkpoint,
not Phase C graduation or complete upstream100% acceptance.


The complete official Role38 source gate subsequently passes168/169 sequence
checks per preset, with the full683 gate still red. A separately reviewed
private batch projection then passes all11 official native gates on each first
full debug/release execution. Independent review
`sequence-audit/official-role38-projection-design/independent-first11-dual-execution-review.json`
verifies126 frozen artifacts, all51 selected body changes and the other2687
unchanged complete dictionaries. The38 additive native snapshots and policy
preserve the manifest,84 historical snapshots and preceding bindings. Original
raw drawing coverage remains2/2145; raw metadata, original foreign predicates,
complete683 and Phase C remain open. The source-first reciprocal ER checkpoint
passes59/62 checks with both dense positive sources still genuinely unmet;
producer implementation has not yet passed its separate review/execution gate.
No native gate total alone establishes the requested complete100% acceptance.


The complete683 Model join first exposes a genuine reader-contract defect:
ce60 integer facts were decoded as DisplayText. Initial debug170/168 and30
source-decode errors remain frozen; a separate metadata recovery discloses that
run's unavailable native OS exit. A two-argument LegacyInteger correction then
passes169/170 on each first corrected complete run. Independent review
`sequence-audit/complete683-reviewed-model-join/independent-repaired170-dual-review.json`
verifies342 complete Models (340 drawings and2 valid-empty), all72 new source
matches,611 unchanged dictionaries, all683 three-state frames and six report
files across presets. The full683 gate remains red from341 unclassified
obligations, and Phase C remains open. This is a bounded native checkpoint,
not completion of the requested upstream100% scope.

#### Phase C checkpoint: complete Termaid47 native geometry, reciprocal ER

The reviewed reciprocal-page boundary inset fixes the observed dense ER routing
cut without raising a cap, adding retries or changing ordinary connectors. First
actual dual-preset ER execution is65/64 with250 strong ER296 records and46
explicit pending source profiles. Both complete permutations and the independent
renamed mixed-stroke case now pass; the preserved future-anchor counterexample
means this is not a universal routing theorem. Independent actual review:056ac9e9.

The separate exact2-profile supplement preserves immutable Termaid45+2null source
facts and yields47 complete native geometry/mutation matches in first173, with
old45 dictionaries and all47 frame states unchanged. Fresh complete execution of
all seven relevant binaries is439/436 in both presets,29 byte-identical reports;
only the Sequence683, ER296 and XY102 full-coverage gates remain red. Independent
actual review:4a1ff8f3. Full683 still has342 reviewed Models and341 pending at
this checkpoint; the next47 join is a separately reviewed candidate.

An actual copied release CLI OS PTY shows dense ER drawing,40-column source
fallback and200-column restoration within the same process and one local
fixture response. Wide/restored screenshots are identical; sentinel survives.
The installed CLI is unchanged, and no real-provider inference is claimed.
Independent live review:2b5af3d7; its raw hash covers a capture prefix with an
explicit68-byte terminal cleanup tail recorded separately.

Phase C remains open. Neither native TEST pass rate nor safe source fallback
certifies the complete frozen original predicate or drawing denominator.


#### Phase C checkpoint: complete389 sequence Models and strict logging ER

First actual174 debug/release sequence runs each pass173 TESTs, joining all47
Termaid Models directly into the complete683 source gate. The result is389
matches (387 drawing and2 valid empty), with294 explicit source obligations
still pending. Exactly47 dictionaries change; other636, all683 physical state
tuples and six other reports remain exact. Independent actual review:e8c95dd2.

The first strict logging-only ER producer/factory trial retains its443/438
seven-binary result and two genuine new ER oracle/boundary failures. The private
test-only display projection and historical fallback migration then pass68/69
relations TESTs and11/11 official TESTs in both first corrected runs. ER296 now
has260 complete typed matches and36 pending; all10 logging-only sources pass
complete geometry and meaningful mutations. Raw facts and12 historical tuples
remain unchanged, as do all producer limits. Independent review:b4d29c81 also
checks the explicitly preserved metadata-only freeze-wrapper correction and the
remaining uncertified raw whitespace diagnostic. No native rerun hides a failure.

Phase C remains open; these bounded results do not certify the original full
upstream drawing/predicate denominator or complete100% acceptance.

#### Phase C checkpoint: source Model admission versus actual geometry

The first reviewed native178/ER73 seven-binary execution is452/446 in both
presets, with six retained failures and34 byte-identical reports. Sequence683
contains553 admitted Models but534 actual matches (532 drawings,2 valid empty),
19 source-positive rendering failures and130 unmodeled records. ER296 contains
272 drawings,2 valid-empty records and22 pending profiles. The new ER long-role
producer defect and erroneous bare-entity negative control remain separately
visible in those first logs. Independent actual review:de0ec2b6.

The source/model and real geometry denominators remain distinct. Phase C stays
open; neither safe source fallback nor Model admission satisfies full100%.


#### Phase C checkpoint: complete State85/ClassEnvelope22 first source gates and Body22 official admission

The complete corrected native Relations121 source gate has117/121 passing in
both presets, with all27 reports exact. All22 Class envelopes are publicly
observed and source/Model/presentation-bound but still whole-source fallbacks.
State85 has85 observations,77 complete Model decodes and48 complete geometry
and mutation successes;28 modeled fallbacks,7 positive profile gaps, the
hidden-choice geometry/policy hold and original malformed/error API hold remain
mandatory. No source/schema/resource/observation failure is converted to
semantic credit. The initial loader and typed-control failures are preserved.

Body22 official admission is independently reviewed by agent:/root/corpus_review
(code/assets) and root (code/materials/actual first dual run). Corrected full
Debug/Release18 tests pass18/18 with the same2738-row report. Exactly22 body
rows change;2716 other dictionaries/all2145 originals/all193 prior raw rows and
five old bindings remain exact. First compile and zero-matrix reader failures
remain frozen. Evidence locks are native121 dual7866c2e and official18
dual99f8ebf, with full source/binary/build/log/report manifests in the research
full-obligation-audit directory.

The next State bare-ID/quoted-alias design is independently reviewed by
agent:/root/terminal_union and root; Class envelope/title/explicit-empty design
by agent:/root/corpus_review and root. These authorize scratch concrete
authoring only, with unchanged-renderer test-first and independent concrete
review gates before root production trials. Neither predicts new source passes.
The c506 source-bound routing evidence closes all third-edge source clearance
obstructions, while full Canvas/Work DWARF remains unobservable; it is not a
completed producer fix. Phase C remains open. No full-upstream100%, whole ctest,
final terminal screenshot, installation, commit or push is inferred.


#### Phase C checkpoint: Flow Config21 full-source production trial

The preserved unchanged-producer test-first execution captures/binds/decodes
all21 original whole sources but admits zero drawings in both presets. A
source-general local Flow parser change then builds both presets and passes
all21 complete geometry/mutation/restore obligations, including empty groups,
self-loops, parallel edges and complete configuration prefixes. All72 applicable
mutations are rejected. Independent source/Model and concrete producer reviews
precede the trial; the first compile-only JsonVal API error remains preserved.

The full seven native targets pass684/694 TESTs in both presets; the same10
full-completion failures remain. All13,653 source paths/hashes are unchanged
during execution. Ten synthetic complete source controls and13 closed profiles
then pass their two focused TESTs in both presets with byte-identical reports.
Source fallback remains zero drawing credit. The formal matrix still consumes
only the old8 Flow Models until its separately reviewed consumer executes.

The terminal predicate port executes12 original Beautiful expressions from four
complete frozen functions, including seven newly executed nonliteral predicates.
Five previously executed literal predicates are retained without duplicate
assertion credit. All four whole native scenes and good-first predicate negatives
pass both presets; this is no full foreign API/options or glyph-golden claim.

Owning changes are private Flow parsing and native test support/fixtures. No
exported module interface, settings, external backend, production resource cap
or original vendor source changes. Independent Requirement concrete reviews
remain open. Phase C and complete100% acceptance remain open; no final whole
CTest, real terminal screenshot, installation, commit or push is inferred.
Evidence directories: `flow400-config21-native-first-red-dual-r3`,
`flow400-config21-producer-first-dual-r2`,
`flow400-config-source-controls-first-dual-r1` and
`beautiful-flow-expression12-first-dual-r2`.


#### Phase C checkpoint: Flow Config21 complete-consumer admission

Independent concrete review precedes the complete Native400/Official400 union.
The first actual R4 dual execution passes56/59 focused TESTs and exposes a real
new Body2 wrapper-authority rejection. It is preserved rather than labelled an
all-green first pass. The limited R6 correction recognizes only the existing
two complete offset-zero descriptor tuples and rejects all nine identity
mutations per tuple plus inconsistent Config21 supplements. Both corrected
full focused executions pass57/59; only the two ultimate completion gates fail.

All400 complete sources still capture/bind;29 whole Models pass full geometry
and meaningful mutations,371 remain unmodeled. The current formal matrix is
497/2145 (23.17%) with Flow29/400. Ordinary source and unadmitted art remain
zero drawing credit. Original wide outcomes are497 drawings,1611 fallbacks and
37 errors. Debug/Release matrices are byte-identical (74bd82f1). Source-path
conservation is exact during both batches; existing other-five-target producer
results are retained without implying another run. No producer or exported
interface is changed by this consumer repair. Full goal, final CTest and
terminal graduation remain open. Root proof: `root-flow400-config21-formal-admission-r6.json`, SHA b69fcb60f867d90864f4c2083e45780631f8935878d80edcf10f1c90c248e0c1.


#### Phase C checkpoint: Requirement and Pie native qualification

The full Requirement producer R4 batch builds both presets and passes688/700
native TESTs;12 completion failures and zero GTest errors remain. All13,766
source path/hash pairs are conserved. The later focused six-TEST batch passes
three and fails three in both presets, after independently reviewed marker/stroke
and Pie canonical binding corrections. Requirement11 completes9/11 source-bound
full Models with meaningful mutants; two path-ownership errors remain. Four of
nine independent positives pass; P1 overflow, a P2 no-op mutant and three P6
target-normal contracts remain unresolved. Pie7 now genuinely reaches seven
mandatory source fallbacks after full capture/binding/Model decoding; its earlier
canonical protocol failure remains retained with ERRATA. The independent
subcell good-grid/negative TEST passes.

Neither cohort has formal/foreign admission. Formal drawing coverage remains
497/2145 and five-suite atomic coverage remains unknown. Flow shape/port checks,
Requirement marker ownership and overflow/direction layout, and Pie projection
remain active work. No exported contract or resource cap changes in this focused
checkpoint; private Requirement dispatch/relations layout owns its typed input
selection. Final whole CTest, terminal screenshots, installation, commit/push
and the100% goal remain open. Evidence: `requirement11-producer-full-seven-dual-r4`
and `requirement7-pie4-native-first-dual-root`; focused proof SHA59dfa914.

Source4 Official checkpoint (2026-10-23): all 28 native Official tests execute
in both presets (27 pass, complete-matrix gate fails). Independent admission
raises drawing coverage to 519/2145; Flow remains 33/400. All reports and
13,791 source pins retain their focused-run values. Complete five-suite
semantics and real-terminal graduation remain open; work is uncommitted.

### 2026-10-23: reverse-domain XY Native checkpoint

Debug and Release each build and execute all seven related native targets:
729 TESTs, 717 pass, 12 remaining mandatory-capability failures, zero errors.
Three complete reverse-domain XY originals now pass full cell geometry and
five-view lifecycle checks. The final reviewed Y-label boundary correction
retains all prior reports in a subsequent full 128-TEST Charts run per preset
(127 pass, one complete-XY102 failure). Evidence is
`root-xy-reverse3-production-native-full-dual-proof-r10.json`. Official formal
coverage remains 519/2145; Native-only results and source fallback grant no
formal credit. Full five-suite semantics and terminal graduation remain open;
this RFC remains in progress and no commit is made.

### 2026-10-23: classic Flowchart full-nine checkpoint

Debug and Release each execute all nine related targets: 735 TESTs, 720 pass,
15 fail, zero GTest errors. Three complete terminal sources, eleven classic
profiles, and 24 semantic plus 24 physical negatives pass. The two old
fallback assertions are retained failures awaiting source-preserving migration;
new official art requires independent owned-matrix geometry before admission.
Formal coverage remains 519/2145, Flow 33/400. Source pins remain unchanged
during execution. Evidence: `root-classic11-production-full-nine-dual-proof-r2.json`.
The complete five-suite semantic and terminal graduation gates remain open.
No public contract, resource cap, installation, commit or push is changed.

### 2026-10-23: selected GitGraph production checkpoint

The owning Mermaid module adds a private GitGraph implementation unit with no
backend dependency or exported-contract change. Debug and Release each pass
all three GitGraph TESTs: 29 nonempty complete sources plus one empty zero-ink
exception, five captures per input, 206 semantic and 321 physical rejections,
and zero scene resource/verification errors. Both reports are byte-equal and
13,812 source pins remain unchanged during execution. The first partial trial
and its four failures remain recorded. Evidence:
`root-gitgraph30-producer3-native8-full-focused-proof.json`.
No Official credit, full five-suite assertion completion, terminal graduation,
installation, commit or push is inferred. The broader regression gate remains open.

### 2026-10-23: complete pinned Termaid GitGraph assertion port

Within the open Phase C API-option scope, independent reviewer
`/root/flow21_producer_author` approves the bounded ASCII design, original-test
first-red and producer trial. The owning Mermaid module adds an overload
`render_mermaid(source, fallback, bool use_ascii)`, retaining the existing
two-argument symbol and defaults. Explicit ASCII currently admits GitGraph
only; geometry bits are projected before literal text, and original Unicode
labels remain intact. No Markdown option, settings, backend or resource cap
changes. The final-node block is shared privately inside the owning module.

Both presets execute98 related TESTs with zero failures. Unique XML properties
prove all39 original functions and81 original assertion source sites from the
complete pinned Termaid GitGraph file pass, including both ASCII options. The
Native30 compact geometry slice independently passes29 nonempty drawings plus
one empty zero-ink state,150 frames and234 physical negatives; default reports
remain byte-exact. Root independently reviews the test-only complete-ID and
uniform-crop repairs authored by `/root/requirement_design_review`. The genuine
ASCII original-predicate first-red and preceding oracle failures remain recorded.
Evidence: `root-termaid-gitgraph39-ascii-complete-dual-proof-r2.json`.

This closes one pinned original test file. It does not close Phase C, the full
Termaid repository, all five suites, Official135 GitGraph originals, the
Official2145 matrix or real-terminal graduation. No fallback drawing credit,
installation, commit or push is granted.

### 2026-10-23: complete LinkProfile3 source-owned checkpoint

The Flow owner retains complete open/point/bidirectional normal, dotted and
thick links, inline captions and weighted minimum ranks for three pinned
originals. Its bounded scanner skips a failed unquoted dot run once instead
of rescanning every suffix. Fixed source, canvas, graph and Work limits remain
unchanged. The existing default/ASCII factory suffix is retained exactly.

Source-first compile, binding-key, following-protocol and whole-source failures
remain recorded. The independent four-face oracle correction is verified by36
hand goods and108 physical negatives. Reusing prepaid BFS arrays closes Native
geometry, but the first complete nine-target replay still records one Official
resource failure. A test-private immutable mutation batch then checks the full
good once before all six copied negatives; invalid-good-before-callback, no-op
negative and zero-work guards pass. Reviewer `/root/flow21_producer_author`
approves that limited actual trial. It changes no producer, asset or cap.

All53 Flow and30 Official TESTs execute per preset:81 pass, two mandatory
completion gates fail, zero errors. Both consumers independently admit all3
sources with18 meaningful mutations and zero resource failures. Each retains
all2000 captured frames and397 unselected source rows byte-exact. Formal
coverage becomes525/2145 (24.48%), Flow39/400 (9.75%). Evidence:
`root-flow-linkprofile3-complete-native-official-dual-proof-r12.json`.
The preceding all-nine run is789/777pass/12fail/zeroerrors per preset; other7
targets retain that run rather than an unperformed later rerun. This does not
close Phase C, full five-suite semantics, full CTest or real-terminal graduation.
No fallback drawing credit, installation, commit or push is granted.

### 2026-10-23: Requirement routing diagnostic and withdrawn experiment

Current-source fixed counters identify both P1 and the LR overflow control at
`R -> F <<traces>>`: 78 compatible port pairs have free starts and goals, but
zero source-forward cells remain free; no pre-label route or label placement
succeeds. The two controls retain substantial production Work. Debug records
these stages; Release has no DWARF line locations, so its separate two-test run
proves only unchanged failures and report retention.

Reviewer `/root/requirement_design_review` approves a limited trial that skips
side-stem reservations only for markerless Requirement ports. Both presets run
all seven Requirement11 TESTs: five pass and the same two mandatory failures
remain. LLDB confirms the same connector still fails. Root withdraws the
candidate, restores the exact original producer, rebuilds normal binaries and
reruns all seven TESTs in both presets. All four Requirement reports per preset
return byte-exact to the pre-diagnostic state. No producer fix, cap increase,
formal credit, phase completion, installation, commit or push is retained.
Evidence: `root-requirement11-current-route-diagnostic-proof-r2.json` and
`root-requirement11-markerless-failed-trial-and-restore-proof-r2.json`.

### 2026-10-23: source-bound ASCII Flow first red

Reviewer `/root/requirement_design_review` approves only a test-first trial of
four complete Merman Grid18 inputs from two original functions/two direct sites.
The existing Flow target gains one translation unit and immutable parent/subset
assets. Seven actual public captures precede each source/Model bind. Both presets
build and run all55 Flow TESTs:53 pass, two gates fail, zero errors. The four
ASCII art gates all fail on complete ordinary source fallback; the other failure
is the pre-existing complete400 capability gate. Source identities, helper text
identities, existing default-Unicode geometry/mutations and style-negative
controls introduce no additional failure. Every prior report remains byte-exact
and13,905 source pins stay unchanged during the run.

This is a genuine missing-feature first red, not semantic completion. Canonical
row predicates, helper semantics and Native/ASCII projection remain open. Formal
coverage stays525/2145 andFlow39/400. The R3 design provenance defect is preserved
in its R4 successor, which explicitly rejects the earlier unapproved literal
arrow masking proposal. No producer, public contract, cap or phase status
changes. No installation, commit or push is made. Evidence:
`flow-ascii-native-rows-test-first-independent-review-r3/REVIEW.md` and
`flow-ascii-rows-first-red-dual-root-r3/RESULT.json`.
