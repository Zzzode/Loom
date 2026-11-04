# B6 design — dagre Flow residual drift (read-only)

Status: **DESIGN, pending independent architect review per Tech Lead (no
human gate). No production change yet.** Produced 2026-11-04 against
`e27d5e48`. Investigation was read-only plus temporary, fully-reverted
render probes in `test_mermaid_flow_ascii.cpp` (tree restored clean).

The four OfficialMatrix roster-drift rows (present at clean HEAD, the only
remaining `-L mermaid` failure) split into **two different root causes**
that must not share a fix.

## 1. The four rows, current state (matrix report)

| fixture | track | expected | observed wide/restored | mechanism |
|---|---|---|---|---|
| dagre/6088-2-…diamond-shape-intersections | original | pending | **error** | whole-null accepted, ordinary protocol fails |
| dagre/6088-2… | body_diagnostic | drawn | drawn | already correct (no drift) |
| dagre/sub-graphs | original | pending | **error** | same whole-null over-acceptance |
| dagre/sub-graphs | body_diagnostic | source | error | body has `((…))` double-circle node |
| shape-alias/02-…event-rounded | original | drawn | drawn (narrow also drawn) | already correct |
| shape-alias/02… | body_diagnostic | source | **error** | inline-config `@{shape:event}` accepted |
| shape-alias/06-…diam-decision-diamond | original | drawn | drawn | already correct |
| shape-alias/06… | body_diagnostic | source | **error** | inline-config `@{shape:diam/decision/diamond}` accepted |

Only **4** report rows drift (2 original `flow_whole_null_unmet`, 2 body
`ordinary_source_fallback_expected`). The original shape-alias rows and the
6088-2 body-diagnostic row are already faithful and untouched.

## 2. Root cause A — dagre originals over-accepted (6088-2, sub-graphs)

Both originals carry frontmatter `config.layout: dagre` +
`flowchart.titleTopMargin: 0` (`fields == Layout|Flowchart|TitleTopMargin`,
admitted as `dagre_title_top_margin_zero`, mermaid_impl.cpp:1729). The body
parses and `flow_layout` succeeds, producing **non-faithful connector
geometry**: `place_region` ranks child group frames as atomic quotient
items but routes vertex connectors to the real descendant boxes, so
connectors overpaint nested frame walls/title bands (`┼`/`││` junctions).
Upstream mermaid 12.2.0 in real Chromium with dagre renders both cleanly
(6088-2: 5 nodes/4 clusters 828×562; sub-graphs: 4 nodes/1 cluster
608×282). Two factual corrections over an earlier draft: `E(("…"))` is a
**Circle** (not double-circle; it paints the ring correctly), and
6088-2's frontmatter-stripped body is an already **admitted, snapshot-
pinned drawn** body (`admitted-phase-c-flowchart-semantics`, snapshot
`6d92be5658372bc9.txt`) — the defect is the WHOLE source being admitted
where the frozen manifest expects pending, a connector/frame overpaint
issue, not a wrong node shape.

**Architect-reviewed predicate (positive, structural; gating on the
envelope Layout bit so it applies only to configured whole sources):**
- **Clause D (6088-2):** a vertex→vertex edge whose endpoint owner groups
  are disjoint (neither ancestor of the other) AND both nested below root
  (`group.parent >= 0` on both sides).
- **Clause T (sub-graphs):** vertices u,v outside a group's subtree and m
  inside, with all three vertex edges u→v, u→m, m→v present.
Static analysis of all 400 flow originals: each clause matches exactly its
one failing row and **zero currently-drawn rows** (drawn cross-sibling
flows sit at depth 1; other nested cases are ancestor-scoped or one-
direction). Depth≥2 is documented as a corpus-derived admission proxy,
not a fidelity proof. Evaluated on the parsed Diagram after body parse
succeeds and before `flow_layout`; bare-header/ASCII/non-Flow paths and
the 6088-2 pinned body are untouched.

Why the siblings correctly fall back: the rest of the dagre series is
whole-null (`source_fallback`, accepted as pending) — 6088-1/3/6080/6088-4
contain `fa:fa-…` icon labels the renderer rejects; dozens more are
pending for styling/edge features. The 81 already-drawn dagre-flagged
originals are Class diagrams and a handful of simple flows; dagre config
alone is **not** a reject predicate (it must not be, or those regress).

The two failing rows have **no rejected feature token**; they reach group
placement with a structure our region router paints incorrectly under the
non-dagre layout. So the honest options are:

- **A1 (recommended, minimal): gate the specific untrusted structural
  profile to whole-null fallback.** Detect, at the Flow dispatch, the
  dagre-configured graph whose fidelity we do not implement: a group
  nesting depth / edge-crossing-nested-group / node-declared-in-a-
  different-scope pattern that our `place_region` is known to paint
  wrong, and refuse (return nullopt) so the frame is an *ordinary*
  fallback (matching `expected:pending`), not a distinct art error. The
  predicate must be narrow: positively match only the untrusted shapes
  (never a blanket `layout:dagre` reject — 81 originals prove that would
  over-reject). Each gated pattern gets a focused test proving the
  dagre-series siblings that currently draw still draw.
- **A2 (large, rejected for B6): implement faithful dagre nested-subgraph
  layout.** That is a real engine (rank assignment, order-constraint
  crossing minimization, cluster bounding) — multi-week, well beyond a
  drift fix; the corpus explicitly keeps these `pending`.

Scope A1 is a **tightening** (accept→ordinary-fallback) confined to the
Flow family dispatch; zero effect on Class/Block/other families and on
the 81 drawn dagre rows.

## 3. Root cause B — inline-config shape-alias bodies (02/06)

The body fragment (frontmatter stripped) is a complete, legal flowchart
using the **inline node-config grammar**: `n0@{ shape: event, … }` /
`@{ shape: diam|decision|diamond }`. `flow_shape_data` (mermaid_impl.cpp
:829) already maps all these aliases (added in `cf34725a`) and renders
them correctly — the **original** rows are expected `drawn` and are drawn.

The **body** rows' manifest expectation is `source / inline-config`: they
are deliberately retained placeholders and are **not members** of the 14
pinned `kFlowShapeAliasBodyCases` admission table (process/stadium/cyl/
hex/parallelogram/trapezoid/dbl-circ/sm-circ/fork/delay/triangle). Because
the body falls through the generic `run_case` path and the renderer now
handles the grammar, it produces non-ordinary art → drift.

This is an **admission-policy** mismatch, not a renderer defect; upstream
renders these shapes. Two coherent directions:

- **B1 (recommended, admission promotion): extend the body admission
  cohort** to include event/rounded and diam/decision/diamond the same way
  the 14 aliases were promoted — add reviewed snapshot/Model rows to the
  shape-alias body authority, author the terminal expected glyph rows, and
  flip those two body rows to `drawn` through the same five-frame
  evidence flow. This removes the drift by *finishing* an already-working
  feature honestly. Larger evidence surface (new pinned snapshots), no
  renderer change.
- **B2 (reject, inconsistent): special-case these alias tokens to fall
  back only in the body-only path.** This keeps a working production shape
  half-disabled depending on entry path — contradictory (original draws,
  body falls back) and fragile; not recommended.

## 4. Proposed batch boundary

Treat A and B as independently reviewable commits because they touch
different layers and have opposite polarity (A tightens, B admits):

1. **B6-A** Flow dispatch fallback predicate (production, small) — clears
   the 2 `flow_whole_null_unmet` original rows; must keep every currently
   drawn flowchart original drawn and keep all non-Flow families
   byte-identical.
2. **B6-B** shape-alias body admission cohort (authority/snapshot/test
   evidence, no production geometry change expected) — clears the 2
   `ordinary_source_fallback_expected` body rows.

If the architect review judges B1 too heavy for this milestone, B may stay
explicitly open and only B6-A ships, leaving the 2 body rows documented —
but the matrix cannot reach a clean roster until both land or the roster
is formally re-pinned (a separate reviewed action, not assumed).

## 5. Verification (per sub-batch)

- `-L mermaid` debug+release; row-by-row matrix diff confined to the named
  rows, no drawn→failure anywhere, all other flow originals/body rows
  unchanged; debug/release reports raw-byte identical.
- Roster: the four drift rows disappear from the diff-vs-roster set toward
  the frozen 48; `tools/mermaid/e6c-mandatory-gap-roster.json` is edited
  only through its reviewed re-pin path if expectations formally change,
  never `--write` to mask a regression.
- Focused tests for the A1 predicate boundary (siblings still draw;
  targeted graphs fall back ordinarily) and, for B1, five-frame alias
  planes against new snapshots.
- ASan/UBSan; one independent read-only review each; geometry-only for A,
  evidence-only for B; no Work/cap/gate/cache change.
- Rollback: any unlisted flow row changes, any non-Flow family changes, any
  drawn→failure, or dual-preset divergence reverts the sub-batch.

## 6. Open question for the architect

Confirm A1's positive predicate can be expressed from already-parsed
`Diagram` structure (groups + endpoint scopes + shape kinds) without
re-running routing, and confirm B1 is the intended direction (promotion)
rather than deliberately keeping these aliases unadmitted in terminals.

## 7. Implementation-time probe findings (2026-11-04, read-only probes reverted)

Temporary render probes (since removed; tree restored) sharpened root cause A:

- The two real fixture files (multiline FOLDED `description: >-`) ARE
  accepted by `render_mermaid` (accepted=1), confirming the matrix
  error-over-acceptance.
- Reconstructing the SAME body under a single-line PLAIN
  `description: x` frontmatter is REJECTED (accepted=0); the body alone and
  the folded-description variant are accepted. So the over-acceptance is
  gated not by dagre itself but by the folded-scalar description path: a
  folded (Block) description takes the envelope `flow_envelope_body`
  profile whose body parse admits this nested-group graph, while a plain
  description hits stricter handling. The precise dispatch branch that
  differs (envelope profile flags vs the first-attempt whole-source parse)
  must be pinned down by the implementer before choosing the predicate
  point. The A1 predicate should therefore be evaluated on the parsed
  Diagram at the Flow dispatch (profile-independent), not keyed on the
  description scalar form (which is incidental).
- Upstream Chromium (mermaid 12.2.0, dagre) renders both originals
  faithfully (5 nodes/4 clusters 828x562; 4 nodes/1 cluster 608x282); the
  body diagnostics are already `drawn` (6088-2) / `source` (sub-graphs,
  double-circle witness) and behave independently of the original rows.
