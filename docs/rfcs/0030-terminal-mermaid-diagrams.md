---
rfc: 30
title: Terminal Mermaid Diagrams
status: implementable
owners: "@Zzzode"
reviewers: ["agent:/root/generation_review (Phase A design, PRR, implementation and graduation)", "agent:/root/settings_audit (Phase A parser admission)", "agent:/root/corpus_review (Phase B design, PRR, implementation and graduation; Phase C2 bounded design/PRR)", "agent:/root/corpus_oracle (Phase C1 inventory/runtime promotion and C2 implementation review)", "agent:/root/er_group_design_review (C2 ER subgraph design and PRR r1)", "agent:/root/requirement_design_review (C2 Source4 identity and mutation-plan review)", "agent:/root (C2 Pie owned admission and Source4 integration review)", "agent:/root/style_design_review (Phase D design draft, two revision rounds, approved)", "agent:/root/style_feasibility_review (Phase D implementation feasibility, two revision rounds, approved)"]
created: 2026-10-19
last-reviewed: 2026-10-23
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
