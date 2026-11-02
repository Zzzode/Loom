# Mermaid unsuccessful-case implementation plan

Updated: 2026-10-30. This is the execution backlog for all unsuccessful pinned
official originals, replacing a two-case-only focus. The existing
[coverage plan](mermaid-coverage-plan.md) retains historical evidence and RFC gates.

## Baseline and complete case list

At 240 columns: **663/2,145 drawn (30.91%)**, **1,482 source fallbacks**, and
**0 matrix errors**. Thus **1,482 files remain unsuccessful**, across 36 corpus
categories. Every fallback and error counts as failure. This denominator does
not include body-only diagnostics or the separate Merman/Termaid corpora.

- [All 1,482 cases grouped by priority and family](mermaid-unsuccessful-cases.md): each row links to its complete original file.
- [Machine-readable inventory](mermaid-failure-inventory.json): source hashes, three width outcomes, exact matrix diagnostic, lexical source features, priority and next work owner.

All original source hashes were checked against the pinned manifest. Priority
buckets are disjoint: **P0 0**, **P1 499**, **P2 447**, **P3 536**. A lexical
frontmatter/style/subgraph indicator is not a diagnosed root cause. Individual
`first_confirmed_blocker` values remain unknown until source or runtime evidence
establishes them. Cross-cutting work packages below overlap these buckets and
must not be summed as additional cases or promised gains.

## Classification and priority by family

P0 means an existing matrix error needs adjudication. P1 means expanding the
existing Flow/Class/ER renderer. P2 deepens the remaining admitted families.
P3 evaluates families and corpus categories without credited drawings.

| Family | Fallback | Error | Total unsuccessful | Assigned priority |
|---|---:|---:|---:|---|
| flowchart | 336 | 0 | 336 | P1 |
| requirement | 160 | 0 | 160 | P2 |
| er-diagram | 138 | 0 | 138 | P1 |
| class-diagram | 25 | 0 | 25 | P1 |
| conf-and-directives | 3 | 0 | 3 | P3 |
| state-diagram-v2 | 102 | 0 | 102 | P2 |
| git | 80 | 0 | 80 | P2 |
| xychart | 46 | 0 | 46 | P2 |
| state-diagram | 42 | 0 | 42 | P2 |
| pie | 17 | 0 | 17 | P2 |
| block | 58 | 0 | 58 | P3 |
| gantt | 52 | 0 | 52 | P3 |
| swimlanes | 46 | 0 | 46 | P3 |
| agentflow | 38 | 0 | 38 | P3 |
| mindmap | 33 | 0 | 33 | P3 |
| c4 | 32 | 0 | 32 | P3 |
| usecase | 32 | 0 | 32 | P3 |
| timeline | 30 | 0 | 30 | P3 |
| treemap | 26 | 0 | 26 | P3 |
| venn | 21 | 0 | 21 | P3 |
| architecture | 20 | 0 | 20 | P3 |
| journey | 18 | 0 | 18 | P3 |
| sankey | 18 | 0 | 18 | P3 |
| tree-view | 15 | 0 | 15 | P3 |
| quadrant-chart | 14 | 0 | 14 | P3 |
| ishikawa | 13 | 0 | 13 | P3 |
| cynefin | 12 | 0 | 12 | P3 |
| kanban | 11 | 0 | 11 | P3 |
| wardley | 10 | 0 | 10 | P3 |
| railroad | 9 | 0 | 9 | P3 |
| eventmodeling | 6 | 0 | 6 | P3 |
| packet | 6 | 0 | 6 | P3 |
| radar | 6 | 0 | 6 | P3 |
| katex | 4 | 0 | 4 | P3 |
| info | 2 | 0 | 2 | P3 |
| zenuml | 1 | 0 | 1 | P3 |

## Ordered implementation packages

### Batch 0 — Complete (P0; all22 original errors resolved)

The final two grouped headless/mixed Flow originals pass full source/geometry,
seven negatives, resource/forgery controls and both presets. See
[executed evidence](mermaid-flow-p0-strands-review.md).

Class13 and defined-Class-style6 are complete; see
[the latest executed evidence](mermaid-class-defined-style6-review.md).
Next P1: [Flow style4](mermaid-flow-style4-plan.md), followed by remaining
existing-family grammar/configuration/routing work. Proposals and design review
alone earn no rendering credit.

### Batch 1 — Shared configuration and labels in existing families (P1/P2)

Scan every failed Flow/Class/ER/Requirement/State/Git/Pie/XY source for its actual
configuration, style and label constructs. Group identical *semantic* blockers,
not merely identical frontmatter labels. Start with profiles already admitted
in RFC 0030/0031: source-owned metadata/layout declarations and bounded label
projection. Then handle meaningful styles/themes/HTML only with explicit
terminal semantics and negative controls. A body with a second blocker stays
unsuccessful until that blocker is resolved too.

Deliverable: exact per-construct source cohorts and overlap counts, parser-stage
or runtime evidence, reviewed terminal projection, full-original before/after
outcomes. The historical 970 frontmatter labels are not a forecast of gains.

### Batch 2 — Flow syntax and complete geometry (P1: 336 fallbacks)

After Batch 1 removes confirmed envelope blockers, process source-backed cohorts
for subgraph containment/directions and group endpoints, shapes/properties,
edge forms/lengths and labels, then orthogonal routing and resource limits.
Start with the shared-target group assignment needed by the P0 group examples,
because it also improves the ability to verify other Flow cases. Preserve every
node, group, edge, marker and label; test duplicate destinations, shared paths,
wrong ownership, missing arrows, orphan ink and width restoration. Resource
failures need bounded algorithm changes rather than an unreviewed cap increase.

### Batch 3 — ER and Class remaining geometry (P1: 138 + 44 fallbacks)

ER: source-defined attribute tables, groups, relationship cardinalities and
multiline/Unicode labels. Class: complete member compartments, stereotypes,
namespace/group ownership, captions and notes. Admit missing complete Models
before counting a picture; independently separate unknown semantics from a
confirmed producer failure. Protect the 26 Class originals repaired previously.
Use small cohorts with one shared, confirmed failure cause.

### Batch 4 — Requirement and State (P2: 160 + 144 fallbacks)

Requirement: complete requirement/element fields and typed relationships, then
relationship routing and label/marker geometry. State: next-line composite
braces, region dividers and scoped directions before deeper composite,
choice/fork/join/history/concurrency geometry. Define the terminal meaning of
new constructs before implementing them. The 102 state-v2 and 42 legacy-state
fallbacks retain separate exact source lists even when an implementation is
shared.

### Batch 5 — GitGraph, Pie and XY (P2: 80 + 17 + 46 fallbacks)

GitGraph: configuration/branch labels/orientation, then bounded dense routing.
Pie: complete slice and percentage-label geometry. XY: domains, axes/ticks,
grids and multiple series. Require exact numeric and label meaning as well as
complete pictures; choose the first cohort by confirmed common blocker and
readiness of an independent oracle, not raw fallback count alone.

### Batch 6 — Additional families/categories (P3: 536 fallbacks)

Initial selection order: **block (58), gantt (52), mindmap (33), architecture
(20)**. This is a proposed user-value/dependency ordering, not evidence that all
of their bodies are unsupported. Audit the complete sources and shared metadata
first. Select one bounded grammar and terminal contract per family, obtain RFC
design/production-readiness review, then implement parser, layout, oracle,
mutations and lifecycle tests. Re-rank after that feasibility audit.

The remaining categories are all listed in the table and per-case appendix:
swimlanes, agentflow, c4, usecase, timeline, treemap, venn, journey, sankey, tree-view,
quadrant-chart, ishikawa, cynefin, kanban, wardley, railroad, eventmodeling,
packet, radar, katex, conf-and-directives, info and zenuml. Evaluate diagnostic
and mathematical categories on their own rendering contract rather than treating
them as ordinary node/edge diagrams. No category is dropped from the backlog.

## Historical execution records

The dated records below preserve the assignments and counts at that checkpoint.
Their words "current", "next" and "remaining" are historical, not active work.
The baseline and ordered packages above are the current assignment.

## Execution checkpoint: P0 default-configuration Flow

The user authorized implementation in P0 → P1 → P2 → P3 order. The first
bounded P0 candidate is `conf-and-directives/pr-check/no-config-use-default.mmd`
(SHA-256 `6f56685a482928f690a4920225dd1e89047586c3d277664cd68e4e84b4b9bb83`).
Its full original is rendered through the public Markdown path against an
independent model: rounded Default node, parallelogram Another node, rectangle
End node, two directed edges, and section group containing Another and End.
`MermaidFlowP0.DefaultConfigurationCompleteSceneHasIndependentGeometry` checks
complete geometry, wrong label/shape/edge/target/group mutations, narrow
fallback-or-geometry, restored wide identity, and following content. It does
not strip the description envelope or approve a picture by its appearance.

The complete-source consumer is integrated in the official matrix. Independent
source review by `agent:default_config_source_review` approved the Model;
`agent:default_config_consumer_review` approved the consumer and removal of this
single error row, conditional on final Debug/Release validation. The initial
review identified missing front-end work prepayment; the consumer now checks
the source-size bound and prepays scans and rendering setup before hashing or
constructing either Markdown path. Five actual captures precede Model binding;
full geometry, semantic/physical mutations, restoration and narrow-source-or-
geometry must all pass before the row becomes drawn. Altered and oversized
sources remain rejected. See [review evidence](mermaid-default-config-review.md).

The first matrix run observed 623/2,145 drawings and only this one error-row
removal. Debug and Release each passed 833/833 Mermaid tests. Their official matrices
are byte-identical (SHA-256 `370e2db075a6e84060e059605069919dee2228a1198c330b8ee30323c28bb5dd`);
both mandatory-gap verifiers passed. Comparing all 2,738 original/diagnostic
rows to the Class checkpoint found exactly this one changed original and no
previously successful case regressed. P0 is still in progress;
P1–P3 remain pending.

## Per-cohort completion criteria

1. Fix the selected complete-source path/SHA list and independent semantics before producer changes. Prefer 10–30 related inputs; use a smaller cohort if the feature requires it. Avoid selecting only images that happen to pass.
2. Record each input's confirmed blocker, competing blockers and baseline at 240 → 40 → 240. Model absence, implementation gaps and fixed-size constraints have different owners.
3. Implement the smallest source-backed change, with actual geometry tests and adversarial mutations. Source hash/provenance tests support these checks but are not drawing success.
4. Run affected tests and full Mermaid Debug/Release regression, compare all 2,145 originals by case and width, and block any previously successful case that regresses. Gains elsewhere do not offset regressions.
5. Update independently reviewed oracle authorities, gap roster, per-family numerator/denominator, remaining failures, RFC implementation records and this backlog together. Passing CTest can still report known capability gaps.

The next implementation milestone is complete physical strand validation for
the two remaining Flow P0 originals: nightly7213 and visual issue4648. The
other eight Flow10 originals have passed full-source geometry, meaningful
mutations and dual-preset regressions. After these two, continue P1/P2/P3.

## ER6 completed batch (2026-10-30)

Six source-pinned ER originals now pass complete geometry and mutations.
Debug/Release each pass 836/836 Mermaid tests, with identical matrices.
Official coverage is 629/2,145 (29.32%), fallback 1,501, error 15.
Remaining disjoint priorities: P0 15, P1 518, P2 447, P3 536.
See [source and implementation reviews](mermaid-er6-review.md) and
[exact matrix comparison](mermaid-er6-matrix-comparison.json).
Next P0: Class4 accessibility declarations, then Flow10 and Requirement1.

## Class4 completed batch (2026-10-30)

Accessibility declarations no longer create phantom Class entities. All four
complete source graphs and their metadata bindings pass. Release841/841;
Debug841 complete outcomes after seven stale-roster retries. Coverage633/2145
(29.51%), fallback1501, errors11. Remaining P0 11, P1 518, P2 447, P3 536.
[Source/design/code review and verification](mermaid-class4-review.md) and
[exact chained observation comparison](mermaid-class4-matrix-comparison.json).
Next P0: Requirement1 and Flow10; continue P1/P2/P3 after the remaining errors.

## RequirementMarkdown1 completed batch (2026-10-30)

The complete source, every literal field,14 meaningful adversaries and strict
resource/proof controls pass. Debug/Release842/842. Official coverage634/2145
(29.56%),fallback1501,errors10. Requirement12/172 (6.98%),fallback160,error0.
Remaining P0 10 (all Flow),P1 518,P2 447,P3 536. Continue through the remaining
priorities. [Executed reviews](mermaid-requirement-markdown-review.md) and
[one-case observation delta](mermaid-requirement-markdown-matrix-comparison.json).

### Flow P0 grouped7 checkpoint (2026-10-30)

The separate repeated-target domain admits six complete originals; source5064
also fixes group-port re-entry, temporary occupancy restoration and permanent
port sealing in both router modes. Seven source-owned Models pass complete
geometry, restoration and six mutations each. Debug/Release852/852. Coverage
641/2145 (29.88%),fallback1501,errors3; Flow61/400 (15.25%),fallback336,errors3.
Strict source/proof/ownership and actual resource controls pass, and both
matrices are identical with exactly seven improved original observations.
See [review and execution](mermaid-flow-p0-grouped7-review.md) and
[comparison](mermaid-flow-p0-grouped7-matrix-comparison.json).
Remaining: P0 3, P1 518, P2 447, P3 536. Continue through P0 and the remaining
priorities without requiring per-case user confirmation.

### Flow P0 length1 checkpoint (2026-10-30)

The nighttime edge-length original binds its independent complete source and
P0 Model to the unchanged reviewed visual294 LinkProfile3 geometry/mutations.
Debug/Release854/854. Coverage642/2145 (29.93%),fallback1501,errors2;
Flow62/400 (15.50%),fallback336,errors2. Both final matrices are identical;
exactly one original improves, with no changed statuses elsewhere. See
[executed evidence](mermaid-flow-p0-length-review.md) and
[comparison](mermaid-flow-p0-length-matrix-comparison.json).
Remaining P0 2,P1 518,P2 447,P3 536. Continue with grouped headless/mixed
physical strands and exact duplicate-edge multiplicity, then the lower priorities.

## Flow P0 strands2 completed batch (2026-10-30)

Debug867/867 (287.50s), Release867/867 (37.32s), plus
final exact-branch control1/1 in both. Coverage644/2145 (30.02%),fallback1501,
errors0; Flow64/400 (16.00%),fallback336,errors0. Exactly two original
observations improve;2736 other observation statuses remain unchanged.
[Executed evidence](mermaid-flow-p0-strands-review.md) and
[comparison](mermaid-flow-p0-strands-matrix-comparison.json).
P0 is complete; remaining P1 518,P2 447,P3 536. Continue with the independently
reviewed Class13 direct-style source/design proposal, then the lower priorities.

## Class direct-style13 completed batch (2026-10-30)

Coverage657/2145 (30.63%), fallback1488, errors0; Class254/285 (89.12%),
pending31. Debug878/878 (287.71s), Release878/878
(37.79s), serial style/resource/real-PTY14/14 each.
Exactly13 original statuses improve;2725 other observations remain unchanged.
[Executed evidence](mermaid-class-style13-review.md) and
[exact comparison](mermaid-class-style13-matrix-comparison.json).
P0 is complete; remaining P1 505, P2 447, P3 536. Continue
[defined-Class-style6](mermaid-class-defined-style6-plan.md), then remaining
existing-family work before P2/P3. No phase graduation is claimed.

## Class defined-style6 completed batch (2026-10-30)

Coverage663/2145 (30.91%), fallback1482, errors0; Class260/285 (91.23%),
pending25. Debug889/889 (315.76s), Release889/889
(38.94s), serial style/resource/real-PTY25/25 each.
Exactly six originals improve;2732 other observed statuses stay unchanged.
[Executed evidence](mermaid-class-defined-style6-review.md) and
[exact comparison](mermaid-class-defined-style6-matrix-comparison.json).
P0 complete; remaining P1 499, P2 447, P3 536. Continue
[Flow style4](mermaid-flow-style4-plan.md), then remaining P1 before P2/P3.
Proposals/design alone earn no credit; no phase graduation.
