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

### ASan timing evidence (Class285 serial run)

The earlier observation that a serial
`MermaidCapabilityReport.Class285` run under ASan exceeds 600 s had **no
on-disk record** (the `build/asan` tree had been deleted by a disk
cleanup) and was treated as unverified until re-measured.

Re-measured 2026-11-05 on the boxed Homebrew LLVM 23.1.2 toolchain with
the fixed asan preset (commit `dd276fad`; no `CXXFLAGS` workaround):

- command: `./test_mermaid_relations_semantics
  --gtest_filter='MermaidCapabilityReport.Class285'`, serial, with
  `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
  `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`;
- result: **PASSED in 850,369 ms (~14 min 10 s), zero ASan/UBSan/Leak
  reports**;
- emitted `mermaid-official-class285-source-scenes.json` is
  byte-identical (sha256
  `2539debd548074b9d15f8ae7950d10aef36064850d19e55a013bb3f21719b578`)
  to the debug-preset report; counts decoded 260 / pending 25 /
  geometry errors 0 / oracle-or-resource errors 0.

Consequence: the 600 s ctest timeout makes Class285 under ASan a
direct-binary-only gate (consistent with gate 4 above); serial wall time
is ~14 min, not merely >600 s.

## 2. Fallback census (2145 originals, wide-frame)

| family | drawn | fallback | dominant root cause |
|---|---:|---:|---|
| flowchart | 85 | 315 | body feature debts (no frontmatter blocker) |
| requirement | 12 | 160 | config-heavy + requirementDiagram feature debts |
| state-diagram-v2 | 1 | 102 | existing renderer; inert frontmatter ≤33, composite/notes/layout debts |
| git | 55 | 80 | gitGraph config semantics + E2a 13 cohort |
| gantt | 0 | 52 | net-new renderer |
| swimlanes | 0 | 46 | net-new renderer (swimlane layout) |
| xychart | 2 | 46 | existing renderer; inert frontmatter ≤10, axes/labels/orientation debts |
| state-diagram | 0 | 42 | existing renderer; inert frontmatter ≤12, state feature debts |
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

Frontmatter census over the fallback set: of the **898** rows whose
reason is `frontmatter`, 897 carry frontmatter and 896 parse; over ALL
1282 fallback files, 1279 carry frontmatter and 1278 parse. `%%{init: …}%%`
directives: 69 fallback files carry one anywhere in the file — 35 in the
frontmatter-reason subset (git/journey/quadrant-chart/state/timeline/
xychart, zero flowchart) and 34 flowchart body-debt rows outside it;
counting only directives after the frontmatter fence gives 68 (the one
difference is a directive inside a frontmatter line). Key/value census across the 1278 parsed
files:

- `config.theme` 288 with values redux 61 / redux-dark 55 / redux-color
  49 / redux-dark-color 45 / neo 42 / neo-dark 42 / dark 19 / forest 13
  / default 12 / base 10 / neutral 10.
- `config.look`: neo 221 / handDrawn 109 / classic 9.
- `config.layout`: dagre 46 / elk 40 / tidy-tree 5 / swimlane 5 /
  elk.box 1 / elk.rectpacking 1 (the layout batch must cover all three
  ELK entry points, not just `elk`).
- `config.logLevel`: "0" 83 / "1" 26 / "2" 2 — all non-visual.
- `config.securityLevel`: loose 54 / sandbox 1 — both non-visual in a
  terminal that never executes HTML/script (sandbox additionally disables
  click callbacks; still non-visual).
- `config.fontFamily`: courier 136/136 — the terminal uses one fixed
  font, so this key is already a zero-effect blocker only where a family
  gate lacks a profile; no value-set work is needed inside flow.
- Other keys: `flowchart.*` 49, `gitGraph.*` 44 (parallelCommits 38,
  showBranches/showCommitLabel 20 each, rotateCommitLabel 20,
  mainBranchName/Order 14), `xyChart.*` 28, sankey/usecase namespaced
  keys, etc. — these change geometry or shown elements and are handled in
  their family feature batches, never as inert config.

Production-removal probe (temporary, since removed): stripping the
frontmatter and rendering the body through the production renderer shows
that of the fallback rows in families that already have ANY renderer,
**338 bodies already produce non-ordinary art**. Static classification of
their config leaves gives:

- **52 rows whose config carries only inert keys** (logLevel 0/2,
  fontFamily courier, htmlLabels false, or a description-only frontmatter):
  state-diagram-v2 21, state-diagram 12, xychart 10, requirement 4,
  er-diagram 2, flowchart 2, git 1.
- 286 rows additionally carry visual/semantic keys (theme, look, layout,
  gitGraph/xyChart/… namespaced options) — they belong to the F1+/family
  feature batches, not F0.

The 52 are an upper bound: body-produces-art is not body-is-faithful.
Spot-checks of dumped body art show the state, note, and xychart renderers
are substantively built (correct node/note/axis/bar geometry on simple
cases) with fidelity defects on some cases (an over-tall self-loop riser,
float tick labels like `148.64999999999998`, braille/box overlap on mixed
line+bar series), so every promoted row still goes through the reviewed
five-frame Model+snapshot cohort; F0 admits only rows whose art is
independently verified against upstream.

## 3. Tracks, batches, estimated rounds

Rounds are rough estimates (one round = implement → family subset → full
matrix → review → local commit). They are forecasts, not commitments;
each batch's real delta decides the next batch.

### Track A — frontmatter/config foundation (unblocks every family)

The production gate is per-family envelope acceptance, not a missing
parser: `parse_envelope` (mermaid_envelope_impl.cpp) already tokenizes
every frontmatter; each family then admits only an exact set of config
profiles (flow's field-bitmask profile is in mermaid_impl.cpp:1548-1758;
ER has a projection-notices profile in mermaid_er_impl.cpp:164-356; class,
mindmap, gitgraph, requirement, charts, and relations-core each have
their own small gate). The F-track work is therefore **value-set
widening inside the existing profile machinery plus extending that
machinery across families**, not a new taxonomy parser.

- **F0 (measure-first, 1–2 rounds) inert-key value widening +
  cross-family profile.** Measure-only claim: the probe's true ceiling is
  the 52 inert-only rows above; the full matrix decides the real delta.
  - Widen accepted value sets where upstream makes the value non-visual:
    `logLevel` accepts "0" and "2" alongside the current "1" (83+2 corpus
    rows); `securityLevel` accepts `sandbox` alongside `loose` (1 row;
    sandbox only removes click callbacks — verify non-visual upstream
    before admitting). `fontFamily` needs NO new values (136/136 corpus
    use courier, which flow already accepts).
  - Extend a shared "inert frontmatter profile" out of the flow/ER
    pattern into class, mindmap, gitgraph, requirement, charts, and
    relations-core gates, accepting description/title plus the inert key
    set only, and letting every visual/semantic key keep failing closed.
  - **Flowchart's expected F0 delta is ~0–2 rows**: 69 flow rows carry
    safe-looking keys but their bodies hold the section-4 feature debts,
    and the 2 probe-hits are dagre/B6-gated; F0 must not be judged on a
    flow delta. The early delta comes from state (≤33), xychart (≤10),
    requirement (≤4), ER (≤2), git (≤1), each only after body art is
    verified faithful and promoted through its reviewed cohort.
- **F1 (3–5 rounds) theme system.** Unknown theme names do NOT silently
  become `base`: upstream mermaid 12.2.0 (mermaid.core.mjs:1429-1441)
  falls back to `defaultConfig.theme`, i.e. the **`default` theme**, then
  merges inline themeVariables. Whether the corpus's redux*/neo/neo-dark
  names (210 rows) are registered by the nightly test harness via
  `registerTheme` is UNVERIFIED — F1 opens with a Chromium ground-truth
  pass per theme name (registered → implement its palette via design
  tokens; unregistered → prove default rendering and admit the
  default-equivalent rows with that evidence). `base/dark/forest/
  neutral/default` map to reviewed token palettes; never hardcoded RGB.
- **F2 (3–4 rounds) `look: neo` per family.** neo changes padding,
  label metrics, corner radius and edge-label backgrounds. Implement per
  family starting with state/xychart/flowchart/class/er where five-frame
  infra exists; neo metrics are geometry, not just color.
- **F3 (2–3 rounds, HIGH RISK) `look: handDrawn`.** rough.js jitter has
  no exact box-drawing equivalent. Needs an explicit terminal-faithful
  ruling (bounded roughness glyph scheme) reviewed against Chromium; if
  no faithful mapping exists, the honest outcome is documented
  equivalence rejection, not a fake admit. 109 originals.
- **F4 (6–10 rounds) non-dagre layouts.** dagre depth beyond the B6
  nested-compound gate, ELK in all three entry forms (`elk`, `elk.box`,
  `elk.rectpacking`, 42 total), tidy-tree (5), swimlane layout (5). These
  are real layout engines; sequence after F-track routing work.

### Track F — flowchart body debts (315)

Token census (multi-label): asymmetric/rare shapes 185 (regex
over-counts label braces; exact clustering in F0), subgraph 87, fa icons
55, classDef 43, style lines 36, init directive 34, class assign 31,
markdown bold 31, backtick labels 33, `<br>` 23, edge text labels 24,
stadium 18, hexagon 17, thick/dotted edges 16, click 16, linkStyle 15,
circle 13, cylinder 12, HTML tags 10, direction 10, subroutine 7, URLs
6, circle/open arrows 6, double-circle 5, cross arrows 4; 8 unmatched
handdrawn/elk files.

- **F-S1 (1–2 rounds) click/URL/directive statements**: `click` has NO
  statement parser today (production keeps `click` only as a node-id
  keyword), so this batch is a new statement parse plus non-visual
  semantics — under `securityLevel: loose` the click target/callback has
  no terminal effect; hrefs and `%%{init: …}%%` directives without visual
  keys are likewise stripped. Fails closed on click handlers that carry
  HTML (`securityLevel: loose` is precisely the loose-HTML profile).
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

### Track E/C/G — existing-renderer feature completion (not greenfield)

Independent code review established that state, xychart, requirement,
gitgraph, mindmap, and pie all have **working parsers/renderers with
small admitted cohorts already** (state in
mermaid_relations_core_impl.cpp, xychart in mermaid_charts_impl.cpp,
etc.). The removal probe confirms substantial faithful body art in each.
These families are therefore feature-completion + admission-cohort
expansion — materially cheaper than the genuinely parser-less families in
Track R — and are sequenced ahead of net-new renderers.

- **STATE 144 (v1 42 + v2 102, 6–9 rounds)**: up to 33 are F0 inert
  frontmatter rows (largest early delta); the rest are composite states,
  concurrency regions, notes, forks, history, direction/layout/neo. Fix
  fidelity defects found in the probe (self-loop riser geometry) and
  promote per reviewed cohort.
- **XYCHART 46 (4–6 rounds)**: ≤10 F0 inert rows first; axes/scales,
  data labels, orientation, line+bar overlap rendering, and tick-number
  formatting (the probe shows `148.64999999999998` where upstream rounds)
  are the feature work; shares primitives with R-CHART families, so
  axis/scale investment here is reused.
- **REQUIREMENT 160 (5–7 rounds)**: ≤4 F0 inert rows; exact feature
  census as its own first batch (element kinds, relationship verbs,
  contains/copies, risk/verdict decorations); renderer exists with 12
  drawn.
- **GIT 80 + E2a 13 (4–6 rounds)**: ≤1 F0 row; semantic gitGraph config
  (parallelCommits, mainBranchName/Order, rotate/show-label options),
  vertical-branch orientation, then admit the RFC-0031 E2a reviewed
  cohort.
- **ER 31 (2–3 rounds)**: ≤2 F0 rows; ER subgraph relationships and
  subgraph↔entity edges, direction override, quoted ids, unicode labels,
  htmlLabels/layout/look copies. Same region-routing work as F-S7.
- **CLASS 25 (2–3 rounds)**: `:::cssClass`/cssClasses assignment, click
  callbacks (non-visual), generics `~T~`, typed members; most are
  -2/-3 frontmatter copies, so one feature batch clears triples.
- **MINDMAP 19 / PIE 17 (2–3 rounds together)**: ≤3 F0 mindmap rows and
  13 theme-blocked pie rows after F1 (pie's fallbacks are dominated by
  redux themes); residual config and body features follow.

### Track B — frozen 48 body roster rows (4–6 rounds, interleaved)

12 group-edge-endpoint (after F-S7/ER routing), 6 extra-arrow-label
(after F-S4), 6 cylinder / 2 trapezoid / 1 circle / 2 extra-node-shape
(after F-S3), 6 unsupported-node-id / group-id / invalid-id (parser
strictness — likely 1 round), 3 `---` arrows + 2 textual/open-arrow
labels (F-S4), 2 nonrectangle multiline (F-S2), 2 inline-config
(different bodies from the admitted 16), plus sub-graphs circle node.
Each sub-batch formally re-pins the roster down on its reviewed path.

### Track R — genuinely net-new renderers (ordered for shared infrastructure)

These families have NO substantive parser/renderer today; each one is a
major feature and **starts with an RFC** (see section 3.0). Shared
primitives are built first and reused:

- **R-CHART (~15–22 rounds)**: shared axis/scale/grid/legend primitives
  (xychart's existing partial implementation in Track E/C/G is the
  reference and first consumer), then quadrant 14, radar 6, treemap 26,
  sankey 18.
- **R-GRAPH-BETA (~18–26 rounds)**: reuse flow/region graph infra for
  architecture 20 → agentflow 38 (shares service/group/junction), c4 32,
  usecase 32, swimlanes 46, kanban 11, eventmodeling 6, cynefin 12.
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

Estimated total program size is on the order of 100–170 batch rounds
(revised down from the initial 120–200 after independent architect
review established that state/xychart/requirement/git/mindmap/pie are
feature completion, not greenfield); this is a forecast to be revised
after every batch, not a schedule.

## 3.0 RFC gates (project convention)

Per AGENTS.md, new feature work is RFC-driven; bug fixes and small
refactors are exempt. Applied to this program:

- **RFC required before implementation**: every Track R net-new renderer
  (program RFC or a per-family RFC), the F1 theme system, F2 neo metrics,
  and the F4 ELK/tidy-tree/swimlane layout engines. These are exactly the
  changes large enough to need motivation, invariant catalogs, and review
  gates on the record.
- **RFC-exempt**: the F-S* flowchart feature batches, ER/class/git
  completion batches, and F0 value widening, which are scoped conformance
  fixes on established renderers — provided each stays a single coherent
  behavior change with the standard five-frame evidence.
- The F3 handDrawn and R-KATEX equivalence rulings are design documents
  whose outcome may be "not renderable in terminals"; such a ruling is
  itself reviewed before any code.

## 4. Sequencing

1. **Phase 0**: F0 inert value widening + cross-family profile (measured
   early delta concentrated in state ≤33 / xychart ≤10), then F1 theme
   foundation behind its RFC.
2. **Phase 1** (existing-renderer completion): state and xychart cohorts
   first (cheapest measured wins), then F-S1…S7 interleaved with ER,
   class, git/E2a, requirement, mindmap/pie, and Track B rows as their
   dependencies land.
3. **Phase 2**: R-CHART shared primitives + remaining chart families.
4. **Phase 3**: R-GRAPH-BETA families.
5. **Phase 4**: R-TABLE, R-TREE, R-SMALL, and the F3/R-KATEX equivalence
   rulings.

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
