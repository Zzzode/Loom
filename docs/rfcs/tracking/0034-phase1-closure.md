# RFC 0034 — Phase 1 closure evidence (bounded layout-Work allowance)

Status: **implementation complete, parked on a documented interim-red tree
(no commit yet).** Phase 1 lands the bounded fail-closed two-pool layout Work
meter that lets ER206 ordinal 198's whole source draw within the production
1,000,000 primary. Two full-suite gates remain red **by design** and can only
turn green in Phases 2-4 (owned proof + matrix wiring + closing matrix). This
doc records the measured evidence and the exact red/green boundary so the
interim red is never misread as a regression (RFC 0034 "Expected interim red
state", mirroring `0033-slice2-implementation.md` §4.1).

Design of record: `0034-phase1-design.md` (r3 APPROVE). Refusal evidence:
`0034-phase0-meter.md`.

## 1. Production change (4 src files, additive and default-valued)

| File | Change |
|---|---|
| `src/ui/visual/mermaid.cppm` | Formula-derived `kErEnvelopeLayoutAllowance = 29,986,816` (240×1024 canvas contract, pinned by `static_assert`); `Work` gains `supplemental_remaining` + `envelope_enabled` **appended after `remaining`** with default member initializers, plus `enable_envelope()` and the `available()` solvency accessor |
| `src/ui/visual/mermaid_cells_impl.cpp` | `Work::spend` split routing: primary first, residual from supplemental; the inactive branch is byte-identical to the historical single-balance clamp-to-zero |
| `src/ui/visual/mermaid_detail.cppm` | One `using` alias for the allowance constant |
| `src/ui/visual/mermaid_relations_core_impl.cpp` | `enable_envelope` only when `diagram->family == Er && diagram->er_presentation`; the only two ER-active direct solvency reads (`RouteSearch::free` ~:7600, `connect_relation` ~:8268) now read `available()` |

No external contract changes. A default `Work` is a single-pool meter exactly
as before; no non-presentation path calls `enable_envelope`. `Work` is not
referenced outside `src/ui/visual` (verified by tree-wide grep), so no other
suite can observe the change.

## 2. Activation surface is the reviewed projection gate

`er_presentation` is constructed only by `er_source_envelope` when the
reviewed frontmatter profile sets `projection` (a non-forest `theme`, or
`consumed` bits 512/1024/2048 = look / htmlLabels / er.useMaxWidth). The
`enable_envelope` branch reuses that exact pre-existing gate; it does not
widen it. All 107 ER107 originals carry such a profile, so all 107 become
two-pool meters, but the supplemental is debited only after the primary is
exhausted — and the §4 sweep proves exactly one fixture (ordinal 198) ever
reaches that branch.

## 3. Budget table (measured against the 1,000,000 primary, both presets)

Sweep `MermaidEnvelopeWork.SupplementalAllowanceIsDrawnByOnlyOrdinal198` over
the 97 ER look-envelope fixtures (a fresh default production `Work` each):

| Outcome | Count | Meaning |
|---|---|---|
| construct/authority rejected | 10 | refuse at any budget (presentation contract gates); not a budget result |
| drawable, primary 1M alone sufficient | 86 | supplemental stays byte-identical to its full post-enable value (zero draw) |
| drawable, crosses the bare 1M primary | **1** | ordinal 198 only |

Ordinal 198 measured draw (pinned as a deterministic budget lock, identical on
debug and release):

- primary consumed: `1,000,000` (exhausted to zero)
- supplemental drawn: **`289,582`**
- total layout cost: **`1,289,582`** (the `class_relations_rows` seam; the
  whole-render Phase-0 threshold was 1,290,694 — the 1,112 difference is the
  finish/entry fees outside this layout seam)
- headroom: `289,582` is under 1% of the `29,986,816` allowance; the
  fail-closed combined ceiling is `kWorkLimit + kErEnvelopeLayoutAllowance`,
  never unbounded

## 4. Targeted tests (red-first, green on both presets)

New in `tests/ui/test_mermaid_envelope.cpp` (suite `MermaidEnvelopeWork`,
7 tests) plus one in `test_mermaid_relations_semantics.cpp`:

- 4 pure meter tests: default-off legacy clamp; split routing; exact combined
  ceiling and one-unit-short fail-closed; fee-decomposition invariance.
- `SupplementalAllowanceIsDrawnByOnlyOrdinal198` — the 97-fixture sweep above.
- `GeometryInvariantAcrossPrimaryExhaustionBoundary` (design N-1) — the same
  grid is emitted with the pool boundary at primary 700k/900k/1M/1.3M/2M; the
  supplemental is pure budget, never a routing input.
- `LayoutCacheMissHitParityForAdmittedRow` (design N-4) — miss and hit through
  the production `LayoutRowsCache` render byte-identical grids; documents that
  `measured_layout_work` / `entry_remaining` observe the **primary-only**
  balance (saturate at 1,000,000) for an enabled row.
- `Rfc0034WholeHandDrawnSourceDrawsAtProductionWorkLimit` in
  relations_semantics — the handDrawn whole source draws with a default 1M
  production Work.

Red-first verification: temporarily neutralizing the single
`enable_envelope` call made all three capability tests fail; restoring it
returned them to green. No temporary probe remains in the tree.

## 5. Design-uncovered frozen tests found and updated (test-only)

The design §4 interim-red enumeration named only the OfficialMatrix
`er206_whole_null_unmet` mechanism. Running the full Mermaid suite surfaced
**two additional frozen gates** the design did not name, both asserting the
old single-pool rule "a `Work{used-1}` layout meter must reject":

- `MermaidER107Runtime.CompleteOriginalsUseSameFactoryOneMillionBudget`
- `MermaidEr107ElementWork.All107PublicRendersExposeExactAndOneShortElementBudget`

Under the two-pool meter a one-unit primary shortfall is funded by the
supplemental, so all 107 projection-active pins flipped these assertions.
These are **meter-state, not output** regressions: the byte-pinning
`AllCompleteOriginals…KeepZeroAdmission` (107/107 fifteen-frame captures and
zero formal admission) and the ElementWork exact/one-short assertions stay
green. Both tests were updated (test-only; neither is among the ten protected
files) to assert the correct two-pool behaviour instead of deleting coverage:
the render succeeds, the primary drains to zero, the supplemental is debited
exactly one unit, and `envelope_enabled` is true. The genuine combined-ceiling
fail-closed boundary remains independently pinned by
`MermaidEnvelopeWork.CombinedCeilingExactAndOneUnitShortFailClosed`, and the
non-presentation zero-budget rejection (`ExistingFamilyDispatch…`) still
passes, proving the inactive single-pool path is unchanged. Both updated
binaries pass on debug and release.

## 6. Byte conservation — four matrices, dual-preset

Captured clean-master and Phase-1 matrices on both presets (stash cycle; the
OfficialMatrix drives every ER row at 128M, where the supplemental is inert
for output, so this gate measures output stability, distinct from the §3
production-budget sweep):

| Tree | sha256 | bytes | debug == release |
|---|---|---|---|
| clean master | `31549cf830661d02a154dbad670723ef5f36aa9919e097d15fe5958b18858ab1` | 8,158,709 | yes (also the RFC 0033 closure sha) |
| Phase 1 | `483b8da490b6c5be1ee1a96c2e4fb119622fcc9a212c47b382ad3409b9014ab4` | 8,159,033 | yes |

Case-level diff clean → Phase 1 over all 2,738 cases: **exactly one case
changes** (ordinal 198); the other **2,737 are byte-identical**. Top-level
diffs are confined to the design §3.3 set `mandatory_gaps`,
`ER_native_whole_source`, and derived `aggregates`. Row 198 field changes:
`actual.height` 30→53 (real native art now emitted), budget/remaining
accounting, `errors` gains the one unmet entry, and wide/restored disposition
source_fallback→error. Census: original/ALL and er-diagram
source_fallback 907→906, error 0→1; drawn stays 1238 (formal drawn credit
awaits the Phase 2-3 owned proof). `mandatory_gaps` adds only the 198 row,
removes none.

## 7. Remaining red gates (planned interim state, Phases 2-4)

Across all 93 Mermaid test binaries, exactly two stay red, both pre-announced
and both un-greenable inside Phase 1 without fabricating authority or editing
a frozen roster:

1. `MermaidCapabilityReport.OfficialMatrix` — the single
   `er206_whole_null_unmet` (gaps 48→49, ER errors 0→1). Turns green only when
   the Phase 2-3 one-row owned proof is wired and Phase 4 re-runs the closing
   matrix. The 48-row gap roster is one of the ten protected files and is not
   edited.
2. `MermaidErGroupEdgeCohort.WholeAndBodyTracksMatchLedgerAtEveryWidth` — the
   blocked-whole guard for 198 in the frozen `er-group-edge-cohort.json`.
   RFC 0034 change-surface table (row for that ledger) schedules its
   regeneration / supersession in the closing change; flipping it early would
   book rendering capability as formal admission before the proof exists. Its
   other three tests stay green.

Per RFC 0034, the tree must not be left at a red gate across a commit without
an explicit reason; Phase 1 is therefore held uncommitted pending the decision
on how to sequence the owned-proof work (or an explicitly-authorized interim
commit referencing this doc).

## 8. Protected assets, architecture, verification

- Ten protected files: sha256/byte loop all `OK` (identical to the RFC 0033
  §0 table); none is in the diff.
- `tools/arch/graph_check.py --target-ui9`: PASS — 0 module cycles, 12
  singleton UI areas unchanged; the diff adds no module or import edge.
- Debug and release presets build clean under `-Werror`; every targeted and
  updated test passes on both.
- Full non-Mermaid ctest was not re-run: the changed symbols are confined to
  the mermaid module with no external consumer, and the inactive path is
  byte-identical, so the residual risk is assessed as zero; a full matrix run
  remains part of the Phase 4 gate.

## 9. Residual risks and open items

- The 29,986,816 allowance is a conservative whole-canvas bound. Design §5
  leaves tightening it toward the painted bounding box for PRR scrutiny; the
  97-fixture sweep is the durable over-admission guard for any cap change.
- `measured_layout_work` / trace `entry_remaining` are primary-only for
  enabled rows (documented in §4 and the N-4 test); no consumer treats them as
  a total today.
- Phases 2-3 (one-row owned proof, new proposal/support header/verifier,
  test-only) and Phase 4 (closing OfficialMatrix, ledger regeneration,
  fallback_roster 907→906, PRR checklist, design-decisions entry) remain.
