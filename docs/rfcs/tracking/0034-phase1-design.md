# RFC 0034 — Phase 1 design: bounded layout-Work allowance for the active-ER envelope path

Status: **Design under review (no `src/` change yet).** This is the Phase-1
design gate for `docs/rfc/0034-er-handdrawn-nested-whole-source-admission.md`.
It (1) itemizes the ~1.06M non-parse layout Work for ER206 ordinal 198 with a
recomputable method, (2) specifies a fail-closed bounded-cap mechanism on the
**layout Work pool** (Route B) and assesses whether Route A is still worth
doing, (3) proves byte conservation for the other 2,737 rows, and (4) states
the interim-red boundary and rollback. It does not implement anything; the
route is a recommendation pending this adversarial review and the user's go
for Phase 1 implementation.

All numbers below were captured with temporary test-only probes driven
through existing seams; every probe was removed and the tree restored. No
production file was edited.

## 1. Cost itemization (recomputable)

### 1.1 Measurement method

Each fee in the renderer is an explicit `Work::spend(n)` call
(`mermaid_cells_impl.cpp:71-78`) against one `testing::Work` pool
(`kWorkLimit = 1'000'000`, `mermaid.cppm:94`). Two orthogonal techniques were
used, both reproducible from existing seams (no production instrumentation):

- **Input-variant ablation.** Run the same immutable fixture through
  `class_relations_rows` (forwards to `detail::relations_layout`) and
  `render_mermaid_with_work` with a 128M budget under four inputs that toggle
  one structural driver at a time. Cost differences isolate each driver.
- **Static fee-formula enumeration.** Every `work.spend(...)` site in
  `relations_layout`, `RelationLayout::prepare` and `ErPaint` was read and its
  argument evaluated for this source's concrete dimensions
  (5 entities, 2 edges, 2 groups with depth 2, canvas 240×34 = 8,160 cells).

Reproduction: a temporary
`MermaidErSubgraphSourceProfile.TemporaryRfc0034Phase1CostProbe` calling
`er_whole_scene_json_for_testing`, `class_relations_rows`,
`render_mermaid_with_work` and reading `Work.remaining` / `FinalEmissionTrace`;
the formulas are the literal `spend` expressions cited per row below.

### 1.2 Ablation readings (128M budget, debug; release identical)

| Input variant | parser-only cost | `relations_layout` cost | full render cost | ElementWork meter |
|---|---:|---:|---:|---|
| body, no envelope | 211,622 | 685,227 | 685,419 | inactive |
| title-only envelope (projection NOT activated) | 216,795 | 690,443 | 691,271 | inactive |
| **full, `look: handDrawn` (active)** | 224,715 | **1,289,582** | **1,290,694** | active, 71,840 / 102,564,352 |
| full, `look: neo` (active) | 224,433 | 1,286,828 | 1,287,928 | active, 71,104 / 102,564,352 |

Derived deltas:

- **Envelope metadata without projection: only +5,216** layout Work
  (690,443 − 685,227). The YAML frontmatter parse and config notices are
  cheap.
- **Activating the ER projection (`diagram->er_presentation` present):
  +599,139** layout Work (1,289,582 − 690,443) for the same body. This is the
  entire gap. handDrawn vs neo differ by only 2,754, confirming the value of
  `look` is not the driver — activation is.
- The 1M-cap failure (Phase 0) is fully explained: body path 685,419 < 1M
  draws; active path 1,290,694 > 1M fails at 290,694 over the cap.

### 1.3 Where the +599,139 active premium is incurred

The active branch diverges from the legacy ER path at exactly one switch in
`relations_layout` (`mermaid_relations_core_impl.cpp:8989-8993`): when
`diagram->er_presentation` is set it constructs an `ErPaint` and calls
`ErPaint::prepare` / `finish`; the legacy path never does. The premium is the
work of that branch, separated into cell-scaled (`O(cells)`) and
count-bounded (`O(entities+edges)`) fees for this source (cells = 8,160;
5 entities, 2 groups, 2 edges).

Cell-scaled and plane-emission fees for this source (cells = 8,160; the
`ErPaint` finish bounding box `body_cells` and the byte/run totals are not
individually probed, so the variable entries are given as formulas and the
total is closed only by the measured ablation in §1.2):

| Site | Formula | Fixed part for this source | Active-only? |
|---|---|---:|---|
| `ErPaint::prepare` plane prepay (`:8095`) | `cells*10 + 4*sizeof(ErPaint) + 128` (sizeof ErPaint = 48) | 81,920 | yes (ErPaint exists only when active) |
| `ErPaint::finish` entry (`:8183`) | `cells*8 + 128` | 65,408 | yes |
| `ErPaint::finish` body-cell prepay (`:8193`) | `body_cells*8 + notices*32` | formula; depends on painted bounding box | yes |
| `ErPaint::finish` emission composite (`:8221-8224`) | `48*bytes + 12*cells + runs*(512+8*sizeof(string)) + rows*(256+4*sizeof(Element)) + 4*sizeof(string)*rows + 2*bytes + 256` = with libc++ sizeof(string)=24 and Element=shared_ptr=16: `… + runs*704 + rows*320 + rows*96 + …` | formula; depends on emitted bytes/runs/rows | yes |
| `ErPaint::put` per painted cell (`:8076`, via `box`/`label`/`path`) | 10 each over every painted rectangle/label/route cell | variable, count of painted cells | yes |
| `route_scope` base (`:8765`) | cells × 3 | 24,480 | **no — body and active both pay** |
| `route_scope` ER compound (`:8777`) | cells × 24 | 195,840 | **no — gated on ER + compound_scope, not er_paint** |
| `route_scope` painted-plane (`:8784`) | cells × 12 | skipped here | active-only but only when `!compound_scope`; row 198 is compound, so it never runs |

The two fixed `ErPaint` cell fees alone (`:8095` + `:8183`) total 147,328; the
`:8193` and `:8221-8224` emission fees and the per-`put` traffic make up the
rest of the premium. No per-put count was instrumented, so this document does
not claim a closed formulaic sum for the variable portion — the +599,139
active premium is the measured ablation total, and the table explains its
shape and bounds. Count-bounded active fees are small constants for this
scene: the `ErPaint` construction (`:8990`), the er_paint-only `route_scope`
charges at `:8793-8807` (edge-count and constant terms; the cell-scaled
`:8784` is skipped for a compound source), and `packed_payload_prepaid`
bookkeeping (`:9089-9092`). The non-ErPaint formulaic grouped fees are
likewise small and identical on both paths: ER grouped entry (`:8958`) = 68;
grouped prepay (`:8970`) for E=5/edges=2 = **7,736**; canvas area fee
(`:8985`) = 1/cell = **8,160**.

**Structural conclusion (no per-put instrumentation needed).** With only
5 entities, 2 groups and 2 edges, every count-bounded fee is necessarily a
small constant. The only `O(cells)` work exclusive to the active branch is
the `ErPaint` plane prepay, finish entry, and per-`put` painting; the
large `route_scope` cell fees (`:8765`, `:8777`) are shared with the body path
and therefore cannot contribute to the active premium, and the one
er_paint-gated cell fee in `route_scope` (`:8784`) is dead for this compound
source. Hence the ~599k premium is concentrated in `ErPaint`'s parallel
16-bit style plane (147,328 in the two fixed plane/finish fees plus the
`:8193`/`:8221` emission fees and per-cell `put` traffic over the painted
rectangles), i.e. the cost of maintaining a
painted style plane that the legacy text-grid renderer never builds — not
parsing and not edge routing.

### 1.4 Recomputation recipe (no build needed to audit the formulas)

`sizeof` measured in the probe: bool=1, int=4, `vector<int>`=24, size_t=8;
RelationBox=16 by definition (four ints). Re-evaluate each cited `spend`
expression with E=5, edges=2, cells=240×34=8,160; the ablation totals in 1.2
tie out to the binary-search threshold in Phase 0 (need 1,290,694; cap
1,000,000; shortfall 290,694).

## 2. Mechanism design

### 2.1 Decision: Route B is required; Route A is optional and insufficient alone

Phase 0 already proved Route A cannot fit the source: the parser-only cost is
224,715, and even a full refund of it leaves 1,290,694 − 224,715 = 1,065,979,
still 65,979 over the 1M cap. The cost itemization strengthens this: the
premium is post-parse painting, not parsing, so parse reuse attacks only the
~224k parse component and none of the ~599k `ErPaint` component.

**Recommendation: implement Route B as the gating capability.** Evaluate Route
A separately on its own merits:

- *Route A (envelope parse reuse) — recommend NOT bundling, keep as optional
  follow-up.* It would remove at most the ~5,216 envelope overhead plus part of
  the 224,715 parse charge, but it restructures the envelope→body parse
  contract and risks the byte-pinned fee sequence for every envelope row
  (97 ER look-envelope rows alone). Its saving does not unlock row 198, so it
  adds review/regression surface without changing the outcome. If a later
  effort wants the general cost reduction, it deserves its own byte-gated RFC;
  A+B is allowed but A is not a prerequisite for B.

### 2.2 Route B: two-tier, scoped, default-off layout Work

The design mirrors the existing secondary-meter precedent
(`NativeLayout::payload_work` for Block, `kBlockPayloadWorkLimit`,
`mermaid_detail.cppm:111-116`, and the caller-injected `element_work` in
`FinalEmissionTrace`): a bounded second meter that only one code path can draw.

- **Implementation shape: a gated supplemental balance inside the existing
  `Work` struct, not a new interface.** `RelationLayout` and `Canvas` both bind
  a concrete `Work&` (`mermaid_detail.cppm:654`, `:236`; constructed at
  `relations_layout:8979,8987`), so a forwarding wrapper cannot be installed
  without an interface refactor. The byte-safe shape is one new field on
  `Work` — a supplemental balance plus an enable flag, both zero/inactive by
  default — and a split-routing `spend`. When the flag is off (every current
  caller), `spend` takes the identical single-balance path, so every existing
  fee sequence and embedded budget field is unchanged. The flag is switched on
  only after parse confirms an ER diagram with non-null `er_presentation`
  (knowable at `:8934`, before the ER fee section at `:8957`), so the
  supplemental also covers the early active-branch fees (the 7,736 grouped
  prepay and 8,160 canvas fee), not just the `ErPaint` calls.
- **Split-routing `spend` semantics (the load-bearing rule).** Today's
  `Work::spend` clamps `remaining` to zero when a fee exceeds it
  (`mermaid_cells_impl.cpp:71-78`). A naive "primary first, then supplemental
  pays the whole fee" would discard the primary residual `r` and make the
  supplemental draw `shortfall + r`, so the measured 290,694 single-pool
  shortfall would not be a valid uniform floor. The scoped `spend` must instead
  split the fee: on a primary refusal, let the primary debit its residual `r`
  to zero and draw only `n − r` from the supplemental. This reproduces exactly
  the behaviour of one combined pool of `1,000,000 + allowance` for every
  fixture, independent of where the boundary falls, and makes **290,694 the
  true uniform supplemental floor** (the exact amount by which row 198's
  1,290,694 need exceeds the 1M primary). An exact-budget and a
  one-unit-short test must assert both the split arithmetic and that the
  combined effective ceiling is constant regardless of the boundary position.
- **Cap derivation, not a fixture magic number.** The supplemental cap
  `kErEnvelopeLayoutAllowance` is a named constant justified from the bounded
  canvas contract (`kWidthLimit*kHeightLimit = 240*1024 = 245,760` max cells)
  and the explicit per-cell active fees, the same way `kErElementWorkLimit` is
  a statically asserted function of the canvas limits
  (`mermaid_impl.cpp:6464-6468`). Under split routing its hard floor is the
  measured supplemental draw for row 198, **290,694**. The final constant is
  the smallest formula-derived bound ≥ that floor that admits this fixture
  class, pinned by a `static_assert`, and validated by the 1M cohort sweep
  (§3) so it cannot over-admit any other row.
- **Fail-closed / drain semantics.** When both the primary and the
  supplemental balance are exhausted, the split `spend` still returns false,
  the enclosing `relations_layout` returns `nullopt`, `finish_native_layout`
  hits its `if (!layout) return {}` guard (`mermaid_impl.cpp:5992`), and the
  markdown renderer returns the plain-text source
  (`markdown_render_code_impl.cpp:106`). There is no exception, no partial
  frame, and no over-draw: the split never debits more than the requested fee,
  and a refused fee leaves the source on the same fail-closed path as today.
  The exact/one-unit-short boundary test (the established
  `MermaidFinalEmissionTrace` pattern,
  `test_mermaid_class_styles.cpp:1241-1255`) proves both the split arithmetic
  and refusal behaviour.
- **Default-off / additive.** No new flag, config, or wire field. The
  supplemental meter is reachable solely through the existing
  `er_presentation` activation, which itself only occurs for the already
  reviewed YAML frontmatter profile; the `%%{init:}` JSON path keeps
  projection default-false and never reaches it (asymmetry preserved, RFC 0034
  NIT-1). The change is a new meter plus a conditional branch — existing
  callers and the default `render_mermaid(source, fallback)` signature are
  unchanged.
- **The ElementWork meter is deliberately left alone.** Phase 0 measured it at
  71,840 of 102,564,352 with zero failed debits; it is not binding and needs no
  change.
- **Solvency accessor for direct `remaining` reads (required, not optional).**
  Changing only `spend` is insufficient: the active ER routing path also reads
  `work.remaining` directly as an insolvency predicate, and those reads would
  fire while the primary is clamped to zero even though the supplemental is
  still funded — defeating the combined-pool invariant and, at one site,
  diverting geometry rather than merely failing. A full audit of
  `mermaid_relations_core_impl.cpp` finds exactly **two ER-reachable direct
  reads**, both on the active path:
  - `:8268` `if (er_paint && canvas.work.remaining == 0u) return false;` in
    `connect_relation` (reached per edge from `route_scope`; live for row 198's
    two edges under active paint);
  - `:7600` `return work.remaining != 0;` at the tail of `RouteSearch::free`,
    reached per A* node on the compound routing path row 198 uses.
  Every other direct read is unreachable while the supplemental is enabled:
  `:303,:345,:2477,:2527,:3001` are parse-time checks before the `:8934`
  enable point (flag off); `:8701` hard-gates `family != Class`; `:8529` is
  inside `if (edge.endpoint_texts)`, a Class-only construct that ER rejects at
  `:6164`. `ErCursor::spend` only forwards to `Work::spend`, so no other debit
  primitive exists. The audit was extended across the whole visual tree to make
  it airtight: the other `mermaid_impl.cpp` reads are Flow-family only
  (`:5031` `connect`, `:5647` `flow_layout`, `:6085-6149`
  `flow_group_border_probe`), ElementWork-meter writes (`:6481`), or trace/
  measurement observations that are not gates (`:6004,:6500,:6671,:6844,:6846`);
  `mermaid_cache_impl.cpp:166` (`if (!computed && work.remaining == 0)`) is
  safe because a split-funded success yields a non-null `computed`, and when
  both pools are exhausted the primary is genuinely zero so the fail-closed
  return is correct, while the cache-hit re-debit at `:134` goes through
  `spend` and is therefore split-aware. The design therefore adds a `Work`
  solvency/availability accessor (primary `remaining` plus the supplemental
  balance when enabled; exactly the primary balance otherwise) and routes the
  two ER-reachable predicates through it. The default-off accessor returns the
  identical single-balance value, so no other call site changes. A dedicated
  test must prove that moving the primary-exhaustion boundary across different
  fee positions leaves the emitted geometry (the captured wide grid) unchanged
  — i.e. the supplemental is pure budget, never a routing input.
- **`Work` layout constraint.** The struct currently has one data member
  (`remaining`). Many test sites positionally construct a single-argument
  `Work{...}` (across 16 test files, on the order of 263 call sites; e.g.
  `Work{kReverse3SceneLimit}` and `Work short_work{spent-1u}`), and a repo-wide
  grep finds no `sizeof(Work)`, no Work memcpy, and no by-value `Work`
  parameter. The supplemental balance and enable flag must therefore be
  appended **after** `remaining` with default member initializers, never
  inserted before it; appending keeps every existing single-argument aggregate
  initializer valid and is the only layout requirement.

### 2.3 Why this does not weaken any invariant

The allowance buys Work budget, never a relaxed semantic check. The drawn row
must still pass the full owned-proof in-function contract (live
four-tuple identity rebind and `!wide && restored && a && b && c` geometry
re-validation) before it can classify as drawn; an under-budget or
non-rendering source still fails closed exactly as row 198 does today.

## 3. Byte conservation argument and the production-budget sweep

The dual-preset OfficialMatrix and a separate production-budget sweep answer
different questions and are both required; neither substitutes for the other.

### 3.1 Why the matrix byte gate cannot by itself observe the new meter

The matrix harness drives every ER block with a 128M `Work`
(`initial_budget: 128,000,000` on all 206 ER rows; the literal `1000000` does
not appear in the matrix). Because the supplemental draws only after primary
exhaustion and row 198 needs 1,290,694 ≪ 128M, the supplemental is inert under
the matrix harness. The matrix byte gate therefore proves output stability but
cannot prove production-budget non-perturbation or that row 198 is the unique
1M-crosser. That evidence requires an explicit sweep.

### 3.2 First-class acceptance artifact: 1M-budget sweep over the envelope cohort

Add a Phase-1 graduation test that reuses the existing
`rows_at(budget)` + binary-search pattern already in
`test_mermaid_envelope.cpp:346-364` (which drives `class_relations_rows` with a
caller-set `Work.remaining`). It runs over every ER fixture whose frontmatter
carries an ER `config` leaf that reaches `er_presentation` — the operational
definition of "envelope-active" (frontmatter presence alone is not it, since
2,139 of the originals carry a `---` description fence; 97 ER rows carry a
look envelope). For each of those 97 fixtures it records the minimum
succeeding budget and asserts:

- row 198 is the **sole** row whose minimum succeeding budget exceeds
  `kWorkLimit = 1'000'000`;
- the other 96 rows have a minimum succeeding budget ≤ 1M (the 86 currently
  drawn plus the other envelope fallbacks whose refusal is construct/authority
  gated, not budget gated — confirmed by their `resource_exhausted: false`
  at 128M), so the supplemental never activates for them;
- the pinned table is reproducible on both debug and release.

This sweep, not the matrix, is the evidence that the bounded allowance cannot
over-admit another row and that 290,694 is the only supplemental draw.

**Upper-bound caveat.** The binary-search pattern in
`test_mermaid_envelope.cpp` fixes its high bound at `kWorkLimit` (1M); that
cannot produce row 198's table cell because its minimum succeeding budget is
1,290,694 > 1M. The sweep must widen the high bound to the scene limit (128M,
as Phase 0 already used) so the search can converge above the cap, while still
using 1M as the decision threshold for "crosser" vs "fits".

### 3.2.1 Layout-cache miss/hit parity (N-4)

Production renders go through `LayoutRowsCache`. On a miss,
`layout_mermaid_content` records
`measured_layout_work = starting_work − work.remaining`
(`mermaid_impl.cpp:6844`); under split routing this delta sees only the primary
and therefore saturates at 1,000,000 for an enabled row even though 1,290,694
was debited in total — so for supplemental-enabled rows the field means
"primary Work consumed", not "total Work consumed", and no current test pins
its exact value. On a hit, the cache re-debits the stored amount against a
fresh flag-off `Work` (`mermaid_cache_impl.cpp:134`), which remains safe and
fail-closed. The graduation plan must add an explicit miss-then-hit parity test
for the admitted row asserting identical rendered output on both paths and
document the primary-only meaning of `measured_layout_work`; no cache key,
eviction, or serialization shape changes.

Two related observations for the implementable-gate audit trail:

- The passive trace writes `trace->entry_remaining = work.remaining`
  (`mermaid_impl.cpp:6004` and `:6500`) likewise observe only the primary and
  will record 0 on an enabled production-budget finish path. They are
  observability fields, never predicates, and no test asserts their value for
  this row; document them alongside `measured_layout_work` rather than treating
  the 0 as a failure.
- `mermaid_cache_impl.cpp:166` (`if (!computed && work.remaining == 0u)`) is an
  ER-reachable direct read but stays correct: under split routing a
  spend-refused nullopt means both pools drained (so primary 0 is truthful),
  while a semantic rejection leaves a positive balance. The only theoretical
  effect — a semantic reject occurring exactly after the primary drains but
  while the supplemental is funded — would skip negative caching, a
  conservative performance nuance with no output change; the §3.2 sweep
  proving row 198 is the sole 1M-crosser is the empirical guard.

### 3.3 Matrix-level conservation (output and derived census only)

Because the matrix runs at 128M, **no embedded `initial_budget` /
`remaining` / `phase_remaining` field changes** for any row. What the closing
OfficialMatrix may change is confined to row 198's case-level disposition and
authority fields (status, whole-null/admission counters, the new contract
block), together with the derived `aggregates` census entries that recompute
from them; every other case and every budget field stays byte-identical. The
re-derived headline census is drawn 1238→1239, fallback 907→906, error 0,
mandatory gaps 48, formal `drawn_at_240` 1053. Acceptance = dual-preset
(debug/release) full-matrix byte identity plus a case-level diff showing only
row 198's disposition/authority fields and the derived aggregates changed; any
other case diff rejects the change.

### 3.4 Frozen assets and determinism

- **Frozen assets are not edited.** The ER206 prior, ER107 `array<Pin,107>`
  and three proposals, and the 48-row mandatory gap roster stay byte-identical
  (ten-file sha/byte loop). Admission routes through new files and a new
  contract id per the RFC 0034 change-surface table; the six-case RFC 0033
  proposal and its `PROPOSAL_SHA` are never edited. The separately-locked
  `fallback_roster.json` moves 907→906 via `fallback_attribution.py`, never by
  hand. All 2,737 other matrix cases (the exact total: 2,738 cases minus the
  one admitted row) are untouched.
- **Determinism of the guarantee.** Work accounting is integer and
  preset-independent; Phase 0/1 readings were identical on debug and release,
  so the dual-preset byte gate is a strong, reproducible invariant rather than
  a timing-dependent check.

## 4. Risk, interim-red boundary, and rollback

- **Interim red state (end of Phase 1 → end of Phase 3).** Once the allowance
  makes the whole source emit art but no owned proof is wired,
  `run_official_er206_case` can no longer take the whole-null downgrade and
  books one `er206_whole_null_unmet`, which fails the hard
  `gap_diff.ok()` / `EXPECT_EQ(gap_rows, 48)` gate
  (`test_mermaid_official.cpp:11474-11477`). This is the pre-announced
  transient; during the window only targeted tests must be green and no full
  matrix is treated as a passing gate, exactly as RFC 0033 slice-2 §4.1. The
  tree must not sit on a red full-matrix commit without an explicit reason.
- **Cap-sizing risk.** An allowance derived too loosely could mask a future
  genuine regression by letting an over-budget source draw instead of failing
  closed. Mitigations: formula-derived (not fixture-derived) cap with a
  `static_assert` on its canvas-contract derivation and split-routing giving a
  uniform 290,694 floor; exact/one-unit-short fail-closed tests; the §3.2
  97-fixture 1M sweep that proves only row 198 crosses the cap and rejects any
  allowance change that flips another row; and the matrix byte gate on output.
- **Scope-creep risk (ErPaint efficiency).** A tempting alternative is to make
  `ErPaint` stop pre-paying the full 10/cell plane or to reuse parse results;
  either changes fee balances for all 86 drawn envelope rows and breaks
  byte-identity. Such a refactor is explicitly out of scope and would need its
  own re-baselining RFC; the bounded allowance is the only single-row-safe
  mechanism.
- **Rollback.** Phase 1 lands as one self-contained commit adding the meter
  and the conditional branch. Reverting it restores the single 1M pool; row
  198 returns to plain-text fallback with zero errors and the matrix returns
  to 1238/907/0/48. Test-only phases 2-3 remain harmless because their proof
  never reaches `ready` without the capability. No data migration, no
  flag-day, all new parameters defaulted.
- **Stop condition.** If Phase 1 cannot make row 198 draw while keeping the
  other 2,737 rows byte-identical on both presets, the RFC terminates with row
  198 retained as a documented `source_fallback` and a negative-result record;
  a permanently red gate is never landed.

## 5. Open items for the implementable gate

- Finalize `kErEnvelopeLayoutAllowance` from the canvas-contract formula
  (240×1024 max cells and the per-cell active fees) with the split-routing
  290,694 floor, and pin it with a `static_assert`; prove the split arithmetic
  and the uniform combined ceiling with exact-budget and one-unit-short tests.
- Implement the gated supplemental balance inside `Work` (inactive by
  default, members appended after `remaining`), enabled after parse and before
  the `:8957` ER fees; confirm the inactive path is byte-identical to today's
  single-balance `spend`.
- Add the `Work` solvency accessor and reroute the two ER-reachable direct
  reads (`:8268`, `:7600`); keep the enumerated parse-time/Class-only reads on
  the primary; add the boundary-invariance geometry test.
- Land the §3.2 1M-budget sweep over the 97 ER look-envelope fixtures (high
  bound widened to 128M) as a durable gtest producing the per-row
  minimum-budget table, proving row 198 is the sole 1M-crosser on both presets.
- Add the §3.2.1 layout-cache miss/hit parity test and document the
  primary-only meaning of `measured_layout_work` for enabled rows.
- Tighten the cap toward the painted bounding-box contract rather than the full
  240×1024 cell count if a smaller formula-derived bound still covers row 198
  (PRR scrutiny; the sweep remains the over-admission guard for current
  fixtures).
- Paste and answer the full PRR checklist; add the design-decisions and
  engineering-efficiency entries.
- Confirm the scoped meter does not alter BMI/compile fan-out beyond the
  touched TUs (Tarjan graph check; no new module edge).

## 6. Review history

| Round | Reviewer | Verdict | Outcome |
|---|---|---|---|
| design r1 | agent a6fa7b4d09aabb828 (fresh-context read-only) | REQUEST-CHANGES | 2 MAJOR (split-routing residual loss; matrix gate blind to the meter → 1M sweep), 3 MINOR (finish-fee itemization, Work-field seam, matrix-diff wording), 2 NIT (240 typo, envelope cohort definition) — all addressed in the r2 revision |
| design r2 | agent a8eb11a69d2897267 (fresh-context read-only) | REQUEST-CHANGES | all seven r1 findings verified CLOSED; new MAJOR N-1 (direct `remaining==0` solvency reads at `:8268`/`:7600` defeat spend-only split routing → solvency accessor + enumerated audit), MINOR N-2/N-3 (composite constants 704/320; fixed-fee total 147,328), MINOR N-4 (layout-cache miss/hit), LOW N-5 (sweep high bound >1M), LOW N-6 (append Work members after `remaining`) — addressed in the r3 revision |
| design r3 | agent af36aef49e546c29b (fresh-context read-only) | **APPROVE** | all six r2 findings verified CLOSED against code; independent whole-tree `.remaining` audit confirms `:8268`/`:7600` are the only ER-active solvency predicates and every numeric claim recomputes. Three non-blocking LOW polish items (L1 entry_remaining trace semantics, L2 cache:166 audit sentence, L3 Work aggregate-init count) folded into this revision. Ready for Phase-1 implementation pending the user's go; PRR remains the next gate. |
