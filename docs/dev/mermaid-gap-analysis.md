# Mermaid official-corpus gap analysis

This analysis began with the LLVM-instrumented Debug run on 2026-10-27, at 240
columns, against the pinned Mermaid 12.2.0 official original-source corpus.
The current family table includes subsequent Class26, DefaultConfig, ER6, Class4,
RequirementMarkdown1, Flow P0, Class13 and Class defined-style6 checkpoints.
The latest matrix is `build/mermaid-baseline-release/tests/ui/mermaid-official-matrix.json`;
the build directory is local and may be regenerated or removed. See
[the conformance guide](mermaid-conformance.md) for the test run and counting
rules, and [the obligation ledger](mermaid-obligation-ledger.md) for the
existing source audits and native capability work. The proposed
[coverage improvement plan](mermaid-coverage-plan.md) organizes follow-up work.

Of 2,145 complete originals, **596 drew, 1,501 fell back to source, and 48
were classified as errors**. Fallback is not successful drawing. A matrix
error is a test-oracle outcome, not necessarily a renderer crash or a confirmed
production bug. This snapshot counts files, not independent features: cases
may be duplicates, themed variants, or have several blockers. At 40 columns,
insufficient space can independently trigger fallback; the counts below are
only for 240 columns.

Latest Class defined-style6 checkpoint: **663/2,145 (30.91%)**,1482 fallbacks,
zero errors. Class260/285 (91.23%). Both presets pass889/889 Mermaid tests
and25/25 serial style/resource/real-PTY checks. See
[executed evidence](mermaid-class-defined-style6-review.md).

## Historical Class routing checkpoint (2026-10-29)

After the Class routing fix, the Debug and Release official 240-column matrices
are byte-identical. Of the same 2,145 originals, **622 draw, 1,501 fall back,
and 22 are errors**: a drawing rate of **29.00%**. Class now draws **237/285
(83.16%)**, with 44 fallbacks and four errors. At that historical Class checkpoint, the other families still matched the
2026-10-27 baseline. The current family table below includes subsequent fixes. Fallback and errors both count as failure. The original
2026-10-27 baseline and the reason analysis below remain historical evidence.

The 20 cardinality and six note originals previously classified as errors now
pass complete source-bound geometry and mutation checks. This does not prove
upstream browser/API assertions or the 48 Class whole-source profiles still
awaiting complete Models. See the [coverage plan](mermaid-coverage-plan.md)
for the implementation, reviews and validation.

## What the fallback labels actually tell us

The matrix's first-order labels account for all 1,501 fallback rows:

| Matrix label | Files | What it establishes |
|---|---:|---|
| `frontmatter` | 970 | The source expectation is associated with frontmatter. This label does **not** establish that frontmatter is the sole or actual runtime blocker; the body may also use unsupported syntax or a diagram family with no renderer. |
| Complete Flow geometry | 336 | The Flow whole-source observation fell back. This label states the contract, not the specific failed parser or layout step. |
| Complete ER typed geometry | 138 | The ER observation fell back. The label does not identify the particular grammar, metadata or layout blocker. |
| Whole-null unadmitted | 44 | Class inputs lack an admitted complete source model and produced ordinary source. It does not identify a specific renderer failure. |
| RFC 0031 E2a reviewed cohort | 13 | Reviewed git/state cohort inputs remain source under their recorded disposition. Consult the [cohort ledger](mermaid-e2a-cohort.md) for individual witnesses. |

These labels are mutually exclusive **report classifications**, not mutually
exclusive root causes. They do not provide a per-file root-cause audit for all
1,501 fallbacks. In particular, no claim that all 970 `frontmatter` rows would
draw after metadata support follows from this matrix.

## Observed results by family

Drawing success rate is drawn / total; the denominator includes both source
fallbacks and errors as failures. Matrix errors include unreviewed output and
unmet positive expectations. A zero drawing count for a family does not by
itself prove that its diagram body is unsupported: metadata and the oracle's admission rules can also block credit.

| Official family | Drawn | Source fallback | Error | Total | Drawing success rate |
|---|---:|---:|---:|---:|---:|
| flowchart | 64 | 336 | 0 | 400 | 16.00% |
| requirement | 12 | 160 | 0 | 172 | 6.98% |
| er-diagram | 68 | 138 | 0 | 206 | 33.01% |
| state-diagram-v2 | 1 | 102 | 0 | 103 | 0.97% |
| git | 55 | 80 | 0 | 135 | 40.74% |
| block | 0 | 58 | 0 | 58 | 0.00% |
| gantt | 0 | 52 | 0 | 52 | 0.00% |
| swimlanes | 0 | 46 | 0 | 46 | 0.00% |
| xychart | 2 | 46 | 0 | 48 | 4.17% |
| class-diagram | 260 | 25 | 0 | 285 | 91.23% |
| state-diagram | 0 | 42 | 0 | 42 | 0.00% |
| agentflow | 0 | 38 | 0 | 38 | 0.00% |
| mindmap | 0 | 33 | 0 | 33 | 0.00% |
| c4 | 0 | 32 | 0 | 32 | 0.00% |
| usecase | 0 | 32 | 0 | 32 | 0.00% |
| timeline | 0 | 30 | 0 | 30 | 0.00% |
| treemap | 0 | 26 | 0 | 26 | 0.00% |
| venn | 0 | 21 | 0 | 21 | 0.00% |
| architecture | 0 | 20 | 0 | 20 | 0.00% |
| journey | 0 | 18 | 0 | 18 | 0.00% |
| sankey | 0 | 18 | 0 | 18 | 0.00% |
| pie | 7 | 17 | 0 | 24 | 29.17% |
| tree-view | 0 | 15 | 0 | 15 | 0.00% |
| quadrant-chart | 0 | 14 | 0 | 14 | 0.00% |
| ishikawa | 0 | 13 | 0 | 13 | 0.00% |
| cynefin | 0 | 12 | 0 | 12 | 0.00% |
| kanban | 0 | 11 | 0 | 11 | 0.00% |
| wardley | 0 | 10 | 0 | 10 | 0.00% |
| railroad | 0 | 9 | 0 | 9 | 0.00% |
| eventmodeling | 0 | 6 | 0 | 6 | 0.00% |
| packet | 0 | 6 | 0 | 6 | 0.00% |
| radar | 0 | 6 | 0 | 6 | 0.00% |
| katex | 0 | 4 | 0 | 4 | 0.00% |
| conf-and-directives | 1 | 3 | 0 | 4 | 25.00% |
| info | 0 | 2 | 0 | 2 | 0.00% |
| zenuml | 0 | 1 | 0 | 1 | 0.00% |
| sequence | 193 | 0 | 0 | 193 | 100.00% |
| **ALL** | **663** | **1,482** | **0** | **2,145** | **30.91%** |

## Historical source-feature observations and constraints (2026-10-27)

The following observations retain the2026-10-27 source audit and later Class26
diagnosis. Their counts and unresolved labels are historical; the active
checkpoint and implementation backlog above supersede them. They do not
describe the current failed set. These are feature areas identified by those
source audits and renderer documentation. Their cohorts overlap the table above; their counts
must not be added together. Some are deliberately bounded terminal profiles or
still need source-by-source confirmation, rather than universally absent
features.

- **Configuration, themes, styles and rich labels:** configuration-bearing
  frontmatter and init directives, non-admitted theme/style settings, and
  portions of HTML/icon label projection are barriers. Some narrow metadata
  and configuration profiles are already implemented. The Flow audit estimates
  overlaps among init/configuration, `@{shape}` variants and HTML/icon labels;
  its earlier router estimate was subsequently corrected. See the
  [Flow reconciliation and T1 checkpoints](mermaid-obligation-ledger.md#historical-capability-expansion-task-list-after-rfc-0031-2026-10-24).
- **Flowchart:** additional shape/property forms, rich labels, configuration,
  subgraph and link forms remain outside the admitted profile. Some sources
  also exceed routing work or the 240-column canvas. The audited T1-B checkpoint
  found most observed pending Flow fallbacks blocked before routing; it did not
  prove a unique root cause for every row.
- **Class:** complete layout for some member/style compartments, relationship
  cardinality captions and note connectors remains incomplete. The matrix's 26
  mapped positives that fall back include cardinality and note variants. A
  subsequent runtime diagnostic traced all 26 to either the Class08-to-C2
  caption-aware connector or the Class10 note leader's work-budget prepayment;
  see the [P0 Class26 diagnosis](mermaid-coverage-plan.md#p0-class26-runtime-diagnosis-2026-10-28)
  for the evidence and remaining uncertainties.
- **ER:** richer attribute-table alignment, cardinality labels, grouped tables,
  Unicode and multiline relationship-label cases need further admission or
  verification. The six matrix errors identify affected cases but not a unique
  parser or geometry cause.
- **Requirement:** only 11 of 172 originals receive whole-source admission in
  this matrix. Requirement/element content and relationship geometry still
  have pending cases; the separately investigated `R -> F <<traces>>` route
  failure is one specific unresolved geometry example, not an explanation of
  all 160 fallbacks.
- **State:** next-line composite braces and standalone `--` region dividers
  are known grammar gaps; nested composites and fork/join/choice/history and
  concurrency geometry have pending cases. The matrix reports 1/103
  stateDiagram-v2 originals drawn in this run; this is not a measure of all
  state syntax support.
- **Pie and XY charts:** remaining profiles include pie percentage-label and
  slice geometry, and XY ticks, grids, axes and multi-series layout. Their
  17 and 46 fallback files respectively are not each independently diagnosed
  by the matrix.
- **GitGraph:** configuration and init options that change branch names,
  visibility, labels or orientation need individually reviewed projection;
  dense routes and fixed canvas/work bounds also limit some graphs. Some
  configuration options and 55/135 official originals already draw.
- **Other diagram families:** the table identifies original corpora with zero
  credited drawings, including gantt, block, architecture, mindmap and sankey.
  Establish body syntax, metadata and terminal semantics separately before
  attributing every fallback to a missing family renderer.

## Historical 48 original-source matrix errors (2026-10-27)

This table preserves the2026-10-27 baseline, not the current zero-error checkpoint.
The table retains the report's own diagnostic, not a guessed root cause. In
particular, `unreviewed art or failed ordinary/restore protocol` describes
alternatives; it does not establish which occurred. The per-case matrix holds
all width observations, hashes and original error strings. Paths below are
relative to `tests/fixtures/mermaid_official/cases/`.

| Family | Fixture | Matrix diagnostic at this checkpoint |
|---|---|---|
| class-diagram | `class-diagram/nightly-check/2-1-should-render-a-simple-class-diagrams-with-cardinality-without-htmllabels.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/nightly-check/2-should-render-a-simple-class-diagrams-with-cardinality-2.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/nightly-check/2-should-render-a-simple-class-diagrams-with-cardinality.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/nightly-check/dagre/dagre-2-1-should-render-a-simple-class-diagrams-with-cardinality-without-htmllabels.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/nightly-check/dagre/dagre-2-should-render-a-simple-class-diagrams-with-cardinality.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/nightly-check/handdrawn/hd-2-1-should-render-a-simple-class-diagrams-with-cardinality-without-htmllabels.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/nightly-check/handdrawn/hd-2-should-render-a-simple-class-diagrams-with-cardinality.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/pr-check/2-should-render-a-simple-class-diagrams-with-cardinality-3.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-classic-look-neo-dark-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-classic-look-neo-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-classic-look-redux-color-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-classic-look-redux-dark-color-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-classic-look-redux-dark-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-classic-look-redux-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-neo-look-neo-dark-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-neo-look-neo-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-neo-look-redux-color-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-neo-look-redux-dark-color-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-neo-look-redux-dark-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/cardinality-neo-look-redux-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/dark-theme.mmd` | Whole-null original has unreviewed art or failed ordinary/restore protocol |
| class-diagram | `class-diagram/visual-check/default-theme.mmd` | Whole-null original has unreviewed art or failed ordinary/restore protocol |
| class-diagram | `class-diagram/visual-check/forest-theme.mmd` | Whole-null original has unreviewed art or failed ordinary/restore protocol |
| class-diagram | `class-diagram/visual-check/full-with-notes-neo-look-neo-dark-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/full-with-notes-neo-look-neo-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/full-with-notes-neo-look-redux-color-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/full-with-notes-neo-look-redux-dark-color-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/full-with-notes-neo-look-redux-dark-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/full-with-notes-neo-look-redux-theme.mmd` | Complete mapped original still uses ordinary source at240 |
| class-diagram | `class-diagram/visual-check/neutral-theme.mmd` | Whole-null original has unreviewed art or failed ordinary/restore protocol |
| conf-and-directives | `conf-and-directives/pr-check/no-config-use-default.mmd` | Expected exact ordinary-code source fallback |
| er-diagram | `er-diagram/nightly-check/should-render-relationship-labels-with-line-breaks.mmd` | OfficialER206 whole-null art/ordinary/restore unmet; never admission |
| er-diagram | `er-diagram/visual-check/an-er-diagram-with-relationships-with-unicode-text-default.mmd` | OfficialER206 whole-null art/ordinary/restore unmet; never admission |
| er-diagram | `er-diagram/visual-check/an-er-diagram-with-relationships-with-unicode-text-elk-layout.mmd` | OfficialER206 whole-null art/ordinary/restore unmet; never admission |
| er-diagram | `er-diagram/visual-check/an-er-diagram-with-unicode-text-default.mmd` | OfficialER206 whole-null art/ordinary/restore unmet; never admission |
| er-diagram | `er-diagram/visual-check/an-er-diagram-with-unicode-text-elk-layout.mmd` | OfficialER206 whole-null art/ordinary/restore unmet; never admission |
| er-diagram | `er-diagram/visual-check/relationship-labels-with-line-breaks-elk-layout.mmd` | OfficialER206 whole-null art/ordinary/restore unmet; never admission |
| flowchart | `flowchart/nightly-check/dagre/2050-dagre-handling-of-different-rendering-direction-in-subgraphs.mmd` | Whole-null Flow has unreviewed art or failed ordinary/restore protocol |
| flowchart | `flowchart/nightly-check/dagre/4-dagre-length-of-edges.mmd` | Whole-null Flow has unreviewed art or failed ordinary/restore protocol |
| flowchart | `flowchart/nightly-check/dagre/55-dagre-handle-nested-subgraphs-with-outgoing-links-2.mmd` | Whole-null Flow has unreviewed art or failed ordinary/restore protocol |
| flowchart | `flowchart/nightly-check/dagre/57-dagre-handle-nested-subgraphs-with-outgoing-links-2.mmd` | Whole-null Flow has unreviewed art or failed ordinary/restore protocol |
| flowchart | `flowchart/nightly-check/dagre/7213-should-render-dagre-edges-with-right-angles-not-curves.mmd` | Whole-null Flow has unreviewed art or failed ordinary/restore protocol |
| flowchart | `flowchart/visual-check/2050-handling-of-different-rendering-direction-in-subgraphs.mmd` | Whole-null Flow has unreviewed art or failed ordinary/restore protocol |
| flowchart | `flowchart/visual-check/5064-should-render-when-subgraph-child-has-links-to-outside-node-and-subgraph.mmd` | Whole-null Flow has unreviewed art or failed ordinary/restore protocol |
| flowchart | `flowchart/visual-check/55-handle-nested-subgraphs-with-outgoing-links-2.mmd` | Whole-null Flow has unreviewed art or failed ordinary/restore protocol |
| flowchart | `flowchart/visual-check/57-handle-nested-subgraphs-with-outgoing-links-2.mmd` | Whole-null Flow has unreviewed art or failed ordinary/restore protocol |
| flowchart | `flowchart/visual-check/issue-4648-nested-subgraph-with-external-connection.mmd` | Whole-null Flow has unreviewed art or failed ordinary/restore protocol |
| requirement | `requirement/nightly-check/requirements-and-elements-with-markdown-text-default.mmd` | Expected exact ordinary-code source fallback |

## Follow-up needed for exact fallback causes

For each source still needing diagnosis, bind its whole original bytes and
metadata to a reviewed expected terminal model, then record separately:
metadata/config admission, body grammar and diagram-family admission, layout or
resource limits, observed output, and oracle/restore validation. Do not strip
metadata and count a body-only result as whole-source support. Preserve a
source fallback until the complete source can be rendered faithfully; replace
an error classification only when the relevant oracle and result are reviewed.
