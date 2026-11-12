# C4 family renderer design (RFC 0031, D-066 / D-137)

Status: design, 2026-11-12. Precedes implementation. Patterned on the four
shipped greenfield renderers (journey `744c3a24`, timeline `fe3eb520`,
gantt `9e8d2950`, treemap `4deb2144`); same RFC 0031 cohort/ledger/review
ceremony, no new RFC.

## 1. Corpus facts (measured, do not re-census)

32 originals under `cases/c4/{nightly-check,pr-check,visual-check}/`.
Every file has YAML frontmatter. Partition: **24 structural (draw) / 8
style-gated (stay source)**. The gate is the presence of an
`UpdateElementStyle`, `UpdateRelStyle` or `UpdateLayoutConfig` statement
(a color/stroke/layout contract the terminal cannot express). A scan
confirms the 24 draw files contain zero `classDef`/`:::`/theme/look/
fill/color tokens. Brace blocks (`Boundary`/`Deployment_Node`/...`{ }`)
are structural nesting, NOT a visual contract, and all 24 draw files
have balanced braces.

Headers (5): `C4Context` (18), `C4Container` (6), `C4Deployment` (3),
`C4Component` (3), `C4Dynamic` (2). No other first structural token.

Element types (30, all the same call shape): Person, Person_Ext, System,
System_Ext, SystemDb, SystemDb_Ext, SystemQueue, SystemQueue_Ext,
Container, Container_Ext, ContainerDb, ContainerDb_Ext, ContainerQueue,
ContainerQueue_Ext, Component, Component_Ext, ComponentDb, ComponentDb_Ext,
ComponentQueue, ComponentQueue_Ext, Boundary, System_Boundary,
Container_Boundary, Enterprise_Boundary, Deployment_Node, Node, Node_L,
Node_R (plus any `*_Db`/`*_Queue`/`*_Ext` variant already listed).

Relation types (6): Rel, Rel_Back, Rel_U, Rel_D, Rel_L, Rel_R. BiRel also
appears in the corpus and must be accepted as a relation keyword.

## 2. Grammar (line-oriented; every quote is balanced on one line)

- Header line must be one of the five C4* tokens.
- `title <text>` once, bounded.
- Element call: `Type(alias, "Name"[, "Tech"[, "Desc"]][, $attr=...])`
  optionally followed by ` {` opening a nested block. At most 5 args in
  the corpus. `alias` is a bare identifier; Name/Tech/Desc are double
  quoted strings. `$key=value` attributes (`$link`, `$sprite`, `$tags`,
  `$shape`, ...) are non-rendered upstream ("not rendered" fixtures) —
  validate the `$identifier=` prefix shape and transcribe nothing from
  them; they never gate a structural file.
- Relation call: `Rel[_x](from, to, "Label"[, "Tech"])`.
- A lone `}` closes the current block; block depth is tracked explicitly
  and must return to zero at EOF (unbalanced -> fail closed).
- No multi-line strings, no comments beyond `//` if present (verify during
  implementation; the census found none in the draw set).

## 3. Projection decision: structural outline, no layout/shape geometry

The terminal cannot draw C4 container boxes or routed arrows. Emit a
bounded native outline that preserves every semantic element and
relationship as text:

- One line per element with its type, alias, name and (when present)
  technology/description, indented by block depth; a boundary/block
  element introduces a deeper indent for its children.
- One line per relation: `from -> to : label [tech]` with the direction
  keyword preserved (Rel_U etc. transcribed, not interpreted as geometry).
- `$` attributes omitted (upstream itself does not render them).

This is the same "faithful structural listing, not an SVG geometry
claim" doctrine as the other greenfield families; all risk stays inside
one new module unit `mermaid_c4_impl.cpp`.

## 4. Fail-closed and bounds

- Type/relation keyword must be in the closed whitelist; an unknown call
  target returns {}. Aliases must be bare identifiers; quoted args must be
  well-formed double-quoted strings (no unescaped embedded quote unless
  the corpus requires one — census found none); arg count within the
  measured cap; `$` attrs must match `$identifier=`.
- Brace depth tracked, balanced at EOF, capped (corpus max depth small;
  set a generous fixed cap e.g. 32); no element outside the header block.
- Reuse the standard caps (source 65536, output rows 1024, wrap 64, width
  240), overflow-checked Work fees, unicode_text on every emitted string,
  tab/bare-CR rejection, no throwing parse.
- Visual gate: any `UpdateElementStyle`/`UpdateRelStyle`/
  `UpdateLayoutConfig` statement, or a frontmatter theme/look/config key,
  returns {} (whole-source fallback). Key-based so prose mentioning
  "style"/"theme" in a description or string is not a false gate.

## 5. Non-goals

C4 box/boundary geometry, arrow routing/direction rendering, sprite/icon
glyphs, `$link` hyperlinks, style/color/layout config, and any
Update*Style semantics. Revisiting any is a separate reviewed milestone.

## 6. Build and verification checklist

1. New `src/ui/visual/mermaid_c4_impl.cpp` (`c4_layout(string_view,
   Work&)`); declaration in `mermaid_detail.cppm`; register in
   `loom_ui_visual.cmake`.
2. `MermaidProbeFamily::C4` + classifier header whitelist (the five C4*
   tokens, including after a frontmatter fence) + dispatch case.
3. Renderer-free `tools/mermaid/generate_c4_cohort.py` +
   `c4-cohort.json`: drift guard over 24 draw / 8 style, manifest sha
   pins, live-measured NARROW_DRAW, real review block (no placeholder).
4. `tests/ui/test_mermaid_c4_cohort.cpp` CellCapture char-vector gate
   (never Screen::ToString): whole+body per-width states, style fixtures
   stay source, every element/relation keyword, block nesting, `$`
   attribute omission, unknown type/bad alias/unbalanced brace fail
   closed, caps.
5. 1:1 OfficialMatrix hook (loader + earliest-match branch) so the
   observation total stays 2738 and frozen flow/Class285/48-gap/gantt/
   treemap dispositions are unreachable.
6. Ceremony per engineering-efficiency.md: release-primary iteration;
   closing debug+release OfficialMatrix byte-identical; full
   `ctest -L mermaid`; three-family serial gate with SERIAL_RC and
   per-case times; ASan contact; independent fresh-agent zero-build
   review before the ledger flips to approved; local commits, push only
   on explicit user request.

## 7. Expected impact and risk

Whole-original drawn should move 1044 -> 1068 (+24), fallback
1101 -> 1077, errors stay 0, provided live measurement confirms all 24
structural fixtures draw (adjust the ledger partition from evidence if
not; never bend a validator to hit the number). Main parser risk is the
call-argument splitter respecting quoted commas and the brace-depth
stack; both are unit-testable with synthetic cases before the corpus
run. Style-gated fixtures must be proven to stay on default source via
the hook's non-ledger fall-through.
