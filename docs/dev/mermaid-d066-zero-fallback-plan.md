# D-066 — official corpus zero-fallback closure plan

Status: **LONG-HORIZON PROGRAM, accepted 2026-11-04 (TechLead D-066, user
budget authorized).** Goal: all 2145 official originals render natively,
**zero ordinary fallback**. Continuous local batches, never pushed; one
full 2145 matrix run per batch closure; independent read-only review per
batch; one-line report per batch (before→after, failures, commit sha).

Baseline at `e7fbd0ef`: 863 drawn / 1282 expected-source fallback / **0
unexpected failures**; body diagnostics carry the frozen 48-row roster
(`official_matrix=48`, roster 44740 bytes / 1fe297da).

## 1. Uniform per-batch acceptance gates

Every batch, regardless of family, must satisfy the same contract (the
established B0b/B6 evidence pattern):

1. Family subset passes the five-frame capture (240/40/240 + two ordinary
   controls) against upstream ground truth (mermaid 12.2.0,
   1fb7869168…, real Chromium when geometry is in doubt); narrow frame
   follows the reviewed-width rule (art wider than 40 → ordinary source
   fallback at that width only).
2. Full serial OfficialMatrix once at batch closure: row diff confined to
   promoted rows, **no drawn→failure anywhere**, no other family changes.
3. Debug and release reports (matrix json/md + every evidence jsonl)
   raw-byte identical.
4. ASan/UBSan clean for the touched family (run the binary directly;
   ctest's 300 s timeout is too short for OfficialMatrix).
5. One independent read-only review (two for snapshot/model batches);
   reviewers never build, write, or commit.
6. One local commit per batch, not pushed. Roster re-pin only via its
   separately reviewed path when the frozen counts intentionally move;
   never `--write` to mask a regression.
7. Report line: `family: before→after fallback, failures, commit`.

Whole originals are promoted through the existing admission-cohort
mechanism (reviewed Model + terminal snapshots, evidence bound after five
captures — the ER107/Block58/shape-alias pattern), never by weakening the
generic runner. Bodies follow `kFlowShapeAliasBodyCases`-style cohorts.

## 2. Fallback census (2145 originals, wide-frame)

| family | drawn | fallback | dominant root cause |
|---|---:|---:|---|
| flowchart | 85 | 315 | body feature debts (no frontmatter blocker) |
| requirement | 12 | 160 | config-heavy + requirementDiagram feature debts |
| state-diagram-v2 | 1 | 102 | renderer effectively net-new |
| git | 55 | 80 | gitGraph config semantics + E2a 13 cohort |
| gantt | 0 | 52 | net-new renderer |
| swimlanes | 0 | 46 | net-new renderer (swimlane layout) |
| xychart | 2 | 46 | chart renderer effectively net-new |
| state-diagram | 0 | 42 | net-new renderer |
| agentflow | 0 | 38 | net-new beta renderer |
| c4 | 0 | 32 | net-new renderer |
| usecase | 0 | 32 | net-new beta renderer |
| er-diagram | 175 | 31 | ER subgraph relations, direction, config copies |
| timeline | 0 | 30 | net-new renderer (table-like) |
| treemap | 0 | 26 | net-new chart renderer |
| class-diagram | 260 | 25 | cssClasses/click/config copies |
| venn | 0 | 21 | net-new renderer |
| architecture | 0 | 20 | net-new beta renderer |
| mindmap | 14 | 19 | config + body feature debts |
| journey | 0 | 18 | net-new renderer |
| sankey | 0 | 18 | net-new chart renderer |
| pie | 7 | 17 | config + pie feature debts |
| tree-view | 0 | 15 | net-new renderer |
| quadrant-chart | 0 | 14 | net-new chart renderer |
| ishikawa | 0 | 13 | net-new renderer (fixed skeleton) |
| cynefin | 0 | 12 | net-new beta renderer |
| kanban | 0 | 11 | net-new beta renderer |
| wardley | 0 | 10 | net-new renderer |
| railroad | 0 | 9 | net-new renderer |
| eventmodeling | 0 | 6 | net-new beta renderer |
| packet | 0 | 6 | net-new renderer (table-like) |
| radar | 0 | 6 | net-new chart renderer |
| katex | 0 | 4 | needs equivalence ruling (math text) |
| conf-and-directives | 1 | 3 | directive semantics |
| info | 0 | 2 | net-new (trivial) |
| zenuml | 0 | 1 | sequence-like, net-new |
| block | 58 | 0 | complete |
| sequence | 193 | 0 | complete |

Frontmatter census over the 1282 fallback files (896 parseable):
`config.theme` 288, `config.look` 221 neo + 109 handDrawn + 9 classic,
`logLevel` 81, `fontFamily` 75, `flowchart.*` 49, `layout` dagre 46 /
elk 40 / tidy-tree 5 / swimlane 5, gitGraph/xyChart/sankey/usecase
namespaced keys; 67 bodies also carry `%%{init: …}%%` directives.

## 3. Tracks, batches, estimated rounds

Rounds are rough estimates (one round = implement → family subset → full
matrix → review → local commit). They are forecasts, not commitments;
each batch's real delta decides the next batch.

### Track A — frontmatter/config foundation (unblocks every family)

- **F0 (1–2 rounds) config-key taxonomy + no-op admissions.** Classify
  every key as terminal-equivalent (admit), visual-equivalent-by-family
  (later batches), or reject-visible. Immediate-admit candidates after
  per-key upstream verification: `logLevel`, `securityLevel` (terminal
  never executes HTML/script), `fontFamily` (fixed terminal font;
  geometry unchanged only when font size is unchanged — verified per
  case), `useMaxWidth` (terminal always renders intrinsic width), top
  `title`, `displayMode`. Measure-only batch: the true delta is whatever
  full matrix shows; the 128 "safe-config" rows in existing-renderer
  families are an upper bound, not a target.
- **F1 (3–5 rounds) theme system.** Map default/base/dark/forest/
  neutral to design-token palettes; establish what unknown theme names
  (the corpus's redux* / neo custom names) do upstream and admit the
  base-equivalent cases; `themeVariables` color subset → tokens. Colors
  map to the existing palette tokens, never hardcoded RGB.
- **F2 (3–4 rounds) `look: neo` per family.** neo changes padding,
  label metrics, corner radius and edge-label backgrounds. Implement per
  family starting with flowchart/class/er where five-frame infra exists;
  neo metrics are geometry, not just color.
- **F3 (2–3 rounds, HIGH RISK) `look: handDrawn`.** rough.js jitter has
  no exact box-drawing equivalent. Needs an explicit terminal-faithful
  ruling (bounded roughness glyph scheme) reviewed against Chromium; if
  no faithful mapping exists, the honest outcome is documented
  equivalence rejection, not a fake admit. 109 originals.
- **F4 (6–10 rounds) non-dagre layouts.** dagre depth beyond the B6
  nested-compound gate, elk/elk.box/elk.rectpacking (40+), tidy-tree
  (5), swimlane layout (5). These are real layout engines; sequence after
  F-track routing work.

### Track F — flowchart body debts (315)

Token census (multi-label): asymmetric/rare shapes 185 (regex
over-counts label braces; exact clustering in F0), subgraph 87, fa icons
55, classDef 43, style lines 36, init directive 34, class assign 31,
markdown bold 31, backtick labels 33, `<br>` 23, edge text labels 24,
stadium 18, hexagon 17, thick/dotted edges 16, click 16, linkStyle 15,
circle 13, cylinder 12, HTML tags 10, direction 10, subroutine 7, URLs
6, circle/open arrows 6, double-circle 5, cross arrows 4; 8 unmatched
handdrawn/elk files.

- **F-S1 (1 round) click/URL/directive neutrality**: `click`, hrefs,
  `%%{init}%%` with no visual keys are non-visual; admit ignoring the
  link target.
- **F-S2 (2–3 rounds) markdown labels**: backticks, `**bold**`,
  `<br>`, escapes — terminal label line geometry.
- **F-S3 (2–3 rounds) remaining shape cohort**: circle, double-circle,
  cylinder, stadium, hexagon, subroutine, asymmetric shapes as reviewed
  whole-source admission cohorts (aliases already prove the glyphs).
- **F-S4 (2–3 rounds) edge vocabulary**: thick/thin/dotted/cross/circle
  arrowheads, pipe and text labels, open arrows, multi-arrows.
- **F-S5 (3–4 rounds) styling**: classDef/style/linkStyle → terminal
  token attributes (bold/dashed/double border, palette colors).
- **F-S6 (2–3 rounds) fa icons**: glyph substitution policy verified
  against upstream (icon occupies a label cell; exact glyphs chosen by
  reviewed table).
- **F-S7 (4–6 rounds) subgraph/edge routing**: nested-group endpoints
  and cross-group edges; shares root cause with the 12 group-edge-endpoint
  body roster rows. Also clears the 8 handdrawn/elk only via F3/F4.

### Track E/C/G — existing-renderer family debts

- **ER 31 (2–3 rounds)**: ER subgraph relationships and
  subgraph↔entity edges, direction override, quoted ids, unicode labels,
  htmlLabels/layout/look copies. Same region-routing work as F-S7.
- **CLASS 25 (2–3 rounds)**: `:::cssClass`/cssClasses assignment, click
  callbacks (non-visual), generics `~T~`, typed members; most are
  -2/-3 frontmatter copies, so one feature batch clears triples.
- **GIT 80 + E2a 13 (4–6 rounds)**: semantic gitGraph config
  (`parallelCommits`, `mainBranchName/Order`, rotate/show label options),
  vertical-branch orientation, then admit the RFC-0031 E2a reviewed
  cohort.
- **REQUIREMENT 160 (4–6 rounds)**: exact feature census as batch 1
  (element kinds, relationship verbs, contains/copies, risk/verdict
  decorations); renderer exists, 12 already drawn.
- **MINDMAP 19 / PIE 17 (2–3 rounds together)**: residual config and
  body features after F0/F1.

### Track B — frozen 48 body roster rows (4–6 rounds, interleaved)

12 group-edge-endpoint (after F-S7/ER routing), 6 extra-arrow-label
(after F-S4), 6 cylinder / 2 trapezoid / 1 circle / 2 extra-node-shape
(after F-S3), 6 unsupported-node-id / group-id / invalid-id (parser
strictness — likely 1 round), 3 `---` arrows + 2 textual/open-arrow
labels (F-S4), 2 nonrectangle multiline (F-S2), 2 inline-config
(different bodies from the admitted 16), plus sub-graphs circle node.
Each sub-batch formally re-pins the roster down on its reviewed path.

### Track R — net-new renderers (ordered for shared infrastructure)

Shared-primitive order first, then families:

- **R-CHART (largest single investment, ~20–30 rounds)**: build shared
  axis/scale/grid/legend primitives, then xychart 46 (bars/lines/axes/
  orientation), quadrant 14, radar 6, treemap 26, sankey 18.
- **R-GRAPH-BETA (~18–26 rounds)**: reuse flow/region graph infra for
  architecture 20 → agentflow 38 (shares service/group/junction), c4 32,
  usecase 32, swimlanes 46, kanban 11, eventmodeling 6, cynefin 12.
- **R-STATE (6–10 rounds)**: stateDiagram v1 42 + v2 102 — composite
  states, concurrency regions, notes, forks, history; graph layout reuse.
- **R-TABLE (~10–14 rounds)**: gantt 52 (axis + task rows + milestones/
  dependencies), timeline 30, journey 18, packet 6.
- **R-TREE (3–4 rounds)**: tree-view 15 (dendrogram).
- **R-SMALL (~12–16 rounds)**: venn 21 (circle geometry), ishikawa 13
  (fixed fishbone skeleton), wardley 10 (anchored map), railroad 9
  (syntax-diagram primitives), conf-and-directives 3, info 2, zenuml 1
  (sequence reuse).
- **R-KATEX (1 round, ruling first)**: math has no faithful box-drawing
  rendering; expected honest outcome is an equivalence ruling (keep the
  source text), identical in spirit to F3.

Estimated total program size is on the order of 120–200 batch rounds;
this is a forecast to be revised after every batch, not a schedule.

## 4. Sequencing

1. **Phase 0**: F0 taxonomy/measure batch, then F1 theme foundation.
2. **Phase 1** (existing-renderer completion): F-S1…S7 in order, ER,
   class, git/E2a, mindmap/pie, Track B rows as their dependencies land;
   requirement/state feature batches.
3. **Phase 2**: R-CHART shared primitives + chart families.
4. **Phase 3**: R-GRAPH-BETA families.
5. **Phase 4**: R-STATE, R-TABLE, R-TREE, R-SMALL, and the F3/R-KATEX
   equivalence rulings.

Each phase ends at zero fallback for its families with the full matrix
green and the roster re-pinned through review. F3 handDrawn and R-KATEX
are the only two places where a faithful-admit outcome is not assumed;
if review rules them unrenderable in terminals, that ruling and its
evidence is the batch deliverable and TechLead is asked for the goal call
at that point rather than silently leaving fallback.

## 5. Program risks

- **Faithfulness vs. approximation pressure.** Neo/handDrawn/theme and
  ELK are the places where "looks similar" is cheapest and most wrong.
  Every visual config batch requires Chromium ground truth and an
  independent review; no token/metric is admitted on resemblance.
- **Layout engines.** ELK/tidy-tree/swimlane and nested subgraph routing
  are the largest technical risks; they gate F-S7, F4, ER, and 12+ body
  rows, so routing investment is front-loaded into Phase 1.
- **Evidence cost grows with cohorts.** Each promoted original pins a
  reviewed Model + snapshot; the admission tooling (proposal/verifier
  scripts, five-frame Work budgets) is extended per family following the
  ER107/Block58 pattern rather than ad hoc harnesses.
- **Roster drift.** Falling below frozen counts is expected and good; it
  must happen only through the reviewed re-pin path, never via `--write`.
