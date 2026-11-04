# Flowchart 315 closure plan (D-066 Track F)

Status: **PLAN 2026-11-05, main-agent produced; independent read-only
review REQUEST-CHANGES findings incorporated (revision 2).** Baseline
`27bcef9b` (post-F0): flowchart 85 drawn / 315 fallback of 400
originals; zero unexpected failures; frozen 48 body rows include
flowchart-only debts this track also retires.

## 1. What already exists (do not rebuild)

- Edge vocabulary: normal/thick(`==`)/dotted(`-.`) links and arrowheads
  are already parsed (`mermaid_impl.cpp:317-490` link scanner).
- Styling statements are already parsed (`flow_style_collect`,
  mermaid_impl.cpp:557-620): `classDef`, `class` assignment, `style`.
  Debt here is CSS property coverage and projection, not statement
  parsing.
- Node shapes: a large alias set is admitted (shape-alias cohorts);
  residuals are the parenthesized bracket shapes below.
- Dagre-configured nested compounds correctly fall back via B6-A;
  simple dagre flows draw.

## 2. Measured census (315 fallback bodies; rows are multi-label, counts
do NOT add)

Token census: subgraph 87, fa icons 54, classDef 43, style lines 36,
body init directive (34 payloads in 34 files, two directory copies of 10
markdown files counted separately), backtick labels 33, class
assignment 31, markdown bold 31, `<br>` 23, stadium 18, hexagon 17,
click 16, linkStyle 15, thick `==` 14, circle 13, cylinder 12, HTML tags
10, direction 10, subroutine 7, href 6, open-circle arrows 6,
double-circle 5, cross arrows 4, unicode 3, literal `\n` 3, dotted 1,
`A & B` fan-out (adds to the routing debt in FS-4/FS-7). Directory
clusters: visual-check 108, handdrawn 50, dagre 41, shape-themes 40,
shape-alias 26, redux-color-subgraphs 16, pr-check 11, icon 10, v2 7,
classic 3, elk 3.

Body init payloads, classified per file (independently re-verified):
20 files `{"flowchart":{"htmlLabels":true|false}}` (10 markdown-string
fixtures each present in nightly-check/dagre and visual-check), 4 files
bare `{logLevel:0}` legacy-theme graphs, 10 visual payloads (theme/
themeVariables 7, inheritDir 2, elk+minNodeWidth 1).

## 3. Batches (ordered by measured independent delta; each batch's real
delta is whatever the serial full matrix at its close shows)

- **FS-1 Markdown labels (2–3 rounds).** Backtick spans, `**bold**`,
  `<br>`/`<br/>` in label contexts, and the markdown-string semantics
  exercised by the 20 htmlLabels-init files. These 20 files ALL carry
  backtick + bold, so this batch is their prerequisite; the body init
  whitelist for `flowchart.htmlLabels` ships WITH this batch (it has
  zero independent delta before markdown lands). Five-frame
  labels/ordering checks pin every row; htmlLabels true vs false is
  terminal-equivalent (upstream differs only in TEXT anchor vs
  foreignObject wrapping; admission requires the body's markdown to be
  supported). Measured potential: up to 20 rows minus any with
  additional subgraph/style debt (4 also have subgraph, 4 also have
  style — those wait for their dependencies).
- **FS-2 init whitelist completion + click statement (1–2 rounds).**
  After FS-1, accept the 4 bare `{logLevel:0}` body directives
  (expected delta ~0 standalone — all four also carry fa icons,
  subgraphs and thick edges) and parse `click <id> "target" ["tooltip"]`
  / callback statements as non-visual no-ops (loose profile; the
  terminal has no interaction layer). Standalone click delta is ~0: the
  16 click files all carry other debt (shape cohorts, classDef/style/
  linkStyle, or `A & B` fan-out in the two link-anchor files), so this
  batch removes a rejection class rather than moving rows; each newly
  unblocked row is credited when its other batch lands. Visual init
  payloads keep failing closed.
- **FS-3 styling projection (3–4 rounds).** Audit which CSS properties
  in classDef/style/linkStyle are rejected; map the supported subset to
  terminal attributes (bold/dashed/double borders, palette tokens — no
  hardcoded RGB), consume `linkStyle` dash/arrow options the edge
  renderer already expresses. Depends partly on F1 theme tokens for
  color-value rows; structural styling lands first.
- **FS-4 edge residuals + fan-out (2–3 rounds).** Thick edge rendering
  polish (parser exists), open-circle (`--o`) and cross (`--x`)
  arrowheads, pipe `|label|` and free-text edge labels, `A & B` fan-out
  syntax, and the frozen body-track categories extra-arrow-label (6),
  textual/open-arrow labels (2), `---` arrows (3).
- **FS-5 shape completion (2–3 rounds).** stadium 18, hexagon 17,
  circle 13, cylinder 12, subroutine 7, double-circle 5 admitted as
  whole-source cohorts with reviewed snapshots; retires the matching
  frozen body rows (cylinder 6, trapezoid 2, circle 1, extra-node-shape
  2) through roster re-pin.
- **FS-6 fa icons (2 rounds).** `fa:fa-…` glyph substitution via a
  reviewed icon→terminal-glyph table (icon occupies a fixed label cell;
  upstream Chromium comparison per icon class).
- **FS-7 subgraph routing + direction (4–6 rounds, hardest).** 87
  subgraph rows, cross-group endpoints (12 frozen body rows),
  `direction`/`inheritDir`, the dagre 41 cluster. Requires the routing
  investment shared with ER and the F4 layout track; refine rather than
  widen the B6-A gate; layout-engine-grade work goes through the F4
  RFC gate.
- **FS-8 handdrawn 50.** Blocked on the F3 rough-look equivalence
  ruling in the D-066 plan; no code until the ruling.
- Theme-bearing flow rows (shape-themes 40, redux-color-subgraphs 16)
  land with Track A F1, not as a flowchart-specific batch.

Each batch: production change → flowchart corpus subset/five-frame
pins → one full serial OfficialMatrix with row-confined diff → debug/
release byte identity → ASan cohort → independent read-only review →
one local commit with before→after fallback counts, not pushed. Roster
re-pins follow the reviewed path as frozen body rows retire.

## 4. First batch FS-1 scope

Markdown label support (backtick/bold/br in node and edge labels,
plain-text equivalence under both htmlLabels settings) plus the
body-init whitelist for `{"flowchart":{"htmlLabels":…}}` only: parse()
currently rejects any `%%{` line wholesale (mermaid_impl.cpp:1266);
extend the Flow path to consume that exact directive payload with a
JSON parse, exact key/boolean values, Work fail-closed, and a bounded
disclosure line appended after the art (state E2b footer precedent),
visual payloads rejected. Acceptance: only rows whose remaining debt is
within FS-1 change; no drawn→failure; every other family byte-identical.
