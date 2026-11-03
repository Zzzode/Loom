---
rfc: 32
title: Independent Work dimension for ER ftxui Element emission
status: provisional
owners: "@Zzzode"
reviewers: ["agent:/root/b_accounting_review (ADR 0032 accounting, approved with invariants, 2026-11-03)", "agent:/root/b_matrix_regression_review (ADR 0032 matrix/regression, approved with gate conditions, 2026-11-03)"]
created: 2026-11-03
last-reviewed: 2026-11-03
tracking: Tech Lead D-021 design-only; implementation gated on decision D-022 and two read-only reviews
---

# ADR 0032: Independent Work dimension for ER ftxui Element emission

- Status: **Provisional** (design only; implementation needs decision D-022).
- Date: 2026-11-03.
- Owners: `src/ui/visual/` (`loom.ui.visual.mermaid`).
- Supersedes/extends: the single-meter accounting recorded in ADR/RFC 0031 and
  the reverted D-015/D-018 credit experiments
  (`docs/dev/mermaid-er-work-budget-design.md`,
  `docs/dev/mermaid-er-d018-accounting-proof.md`).
- Scope: the **active ER public path only**. Block, Class, State, Flow,
  Requirement, Sequence, GitGraph and the layout-only seam are unchanged.

## Summary

Give the real ftxui Element-tree emission on the active-ER public path its own
bounded Work meter (`ElementWork = 320R + 512 + 416U`) with a worst-case ceiling
derived from the 240×1024 canvas contract (`E_limit = 102,564,352`), instead of
sharing the 1,000,000 layout-gas pool. The frozen layout `Work` contract
(992,611), every predicate, the `4x/6x` coefficients and `er_vector_room` are
untouched; the observed peak across all 107 originals is 119,488 (≪ cap), so
this yields 107/107 on both factory and capture layers without raising any
limit. Design only; implementation needs D-022 and two read-only reviews.

## Motivation

There is one `Work` gas pool of `kWorkLimit = 1,000,000` for the whole render.
On the Linux libc++ port (type sizes and bytes identical to macOS libc++;
ASan/UBSan clean; ordering total), nine then eight dense active-ER originals
fit the **layout** budget but exhaust the shared pool when the public path
builds the real ftxui Element tree in `detail::finish_mermaid_payload`.

D-015 credited the two generic packed gates and D-018 credited the finish
function's duplicate bookkeeping (`8*bytes`, `8*sizeof(StyledRun)`). After both:

- the layout-only factory `MermaidER107Runtime` admits all **107/107 with max
  consumed exactly 992,611** (the frozen ceiling);
- the public capture layers `MermaidEr107Source107` / `MermaidOfficialEr107`
  reach **99/107**; the remaining eight sources overspend solely on the
  **retained, real Element fees** — no further bookkeeping remains to credit.

Per D-018 hard stop those real Element fees may not be waived. This ADR proposes
a separate, independently bounded meter for that real allocation instead of
sharing the 1,000,000 layout-gas pool.

## Goals

- Admit all 107 pinned ER originals on both the layout-only factory and the
  public fifteen-frame capture layers while keeping the frozen layout contract
  (consumed ≤ 992,611) byte-for-byte.
- Meter every real ftxui Element allocation with a conservative,
  worst-case-derived ceiling that an adversarial bounded input cannot exceed.
- Keep Block, Class, State, Flow, Requirement, Sequence and GitGraph consumed
  Work and rejection outcomes identical.

## Non-Goals

- No increase to `kWorkLimit`; no change to existing rejection predicates, the
  `4x/6x` gate coefficients or `er_vector_room`.
- No waiver of real Element/node fees; no credit for an allocation that
  actually occurs.
- No behavior change for non-ER families, the body-diagnostic track, persisted
  formats, public API, CLI or wire.

## Proposal

### Disjoint Element-tree allocation

`ErPaint::finish` (`mermaid_relations_core_impl.cpp`) emits only the packed
terminal model: canvas cell styles (`std::vector<std::uint16_t>`) and row
`std::string`s. It never constructs an ftxui node.

`detail::finish_mermaid_payload` (`mermaid_impl.cpp`) is the single place that
turns the packed model into ftxui objects: one `ftxui::hbox` per row, one
`ftxui::vbox` + `MermaidNode` for the diagram, and per run at least one
`ftxui::text` node plus up to `color`/`bgcolor` decorators (the ER palette
never sets the bold bit, so at most two decorators). The independent accounting
review (APPROVE) verified these allocation sites and node sizes
(`sizeof(ftxui::Element)==16`; real node blocks ≈400/run).

The finish preflight fees split into:

| fee | per | meaning | this ADR |
|---|---|---|---|
| `R*(8*sizeof(Element)+64)+512 = 192R+512` | row | hbox + Elements vector + vbox/node | **Element meter** |
| `128R` | row | vector reserve/growth slack for node children | **Element meter** |
| `U*(8*sizeof(Element)+288) = 416U` | run | text + ≤2 decorator Elements + vector slack | **Element meter** |
| `8B` | bytes | UTF-8 re-scan; ErPaint already wrote the same cells | stays on `Work` (D-018 credit) |
| `8*sizeof(StyledRun)*U = 256U` | run | run re-enumeration; finish allocates no `StyledRun` | stays on `Work` (D-018 credit) |

The Element-meter charge is therefore

```
ElementWork(R,U) = 320*R + 512 + 416*U
```

where `R` is emitted rows and `U` is style runs, each run spanning at least one
display cell. This measures real ftxui allocation only and is disjoint from
every ErPaint/gate charge (trace-verified: the packed producer creates no
Element; the Element finish creates no cell/style buffer beyond the row/run
counts ErPaint already produced).

### Independent worst-case-derived cap

Existing constants are untouched: `kWorkLimit=1,000,000`, all current rejection
predicates, the `4x`/`6x` gate coefficients and `er_vector_room`. The layout
`Work` meter keeps the frozen contract (107/107 at ≤992,611).

A new `ElementWork` meter bounds only the Element tree. Its ceiling is derived
from the already-enforced output bounds (`kWidthLimit=240`,
`kHeightLimit=1024`, checked in layout and `valid_packed_rows`):

- `R <= 1024`;
- each row is at most 240 display cells and every run covers ≥1 cell, so
  `U <= 240*R`;

hence the worst admissible drawing spends at most

```
E_limit = 320*1024 + 512 + 416*(240*1024)
        = 327,680 + 512 + 102,236,160
        = 102,564,352 ElementWork units.
```

Any source that would exceed this cannot have passed the width/height gates; it
is already rejected before the Element meter runs. The new meter therefore never
rejects an admissible bounded drawing and never accepts an unbounded one — it is
a safety ceiling derived from the canvas contract, not a tuned threshold, and it
is not an increase of `kWorkLimit`.

### Per-source predicted peaks vs the cap

Measured on the 107 pinned ER originals at the packed-payload boundary
(temporary trace, reverted). Maximum across all 107: **119,488** ElementWork
(0.12% of `E_limit`). The eight sources that remain after the D-018 narrow
credit (redux-color and redux-dark-color variants share identical geometry and
thus the same R,U,E):

| original (tail) | R | U | predicted ElementWork |
|---|---:|---:|---:|
| er-diagram-with-all-relationship-types … redux-color / … redux-dark-color (×2) | 117 | 196 | **119,488** |
| complex-er-diagram-with-multiple-relationships … redux-color (×2) | 103 | 190 | **112,512** |
| a-not-so-simple … without-htmllabels elk-layout | 80 | 171 | 97,248 |
| a-not-so-simple … without-htmllabels handdrawn | 79 | 170 | 96,512 |
| a-not-so-simple … without-htmllabels default | 77 | 168 | 95,040 |
| a-not-so-simple … handdrawn-look | 76 | 167 | 94,304 |

All other 99 originals: ≤ 90,464. The Element tree is built once per render
regardless of capture width, so the 40-column frames incur no additional
ElementWork (they cannot be treated differently from the 240 frames).

Expected result under this ADR: each public render is independently gated by
layout `Work <= 992,611` (frozen, unchanged) and
`ElementWork <= 102,564,352` (worst-case, never binding for the corpus), giving
**107/107** on factory and capture layers without raising `kWorkLimit` or
relaxing a predicate.

### Wiring (future implementation, D-022)

- Add an independent counter (fresh per render) consumed only by the three real
  Element fees in `finish_mermaid_payload`; the `8B`/`256U` terms remain on
  `Work` with the D-018 prepaid credit.
- Carry the ER prepaid credit through `NativeLayout` and the
  `LayoutRowsCache::Entry` (a cache hit rebuilds fresh Elements and is charged
  ElementWork, but re-debits `layout_work` for the cached packed model); the
  credit defaults to zero for Block and every non-ER family.
- No new abstraction beyond one bounded counter and its explicit cap; no change
  to public API, persisted format, CLI or wire.

### Review gate conditions (both independent reviews APPROVED, 2026-11-03)

Implementation under D-022 must satisfy all of these; the design is not
implementable otherwise.

1. **Explicit ER flag, never a credit-presence test (matrix F1).** Block shares
   `finish_mermaid_payload` with ER and the function has no family argument.
   Carry an explicit `bool er_packed` on `NativeLayout`/`LayoutRowsCache::Entry`
   (set only on the `er_paint` return path). When false (Block), **100% of the
   current gate-3 fees — `320R+512+416U` and `8B+256U` — stay on `Work`**; the
   Element meter and every prepaid credit are no-ops. Do not branch on
   `credit > 0`.
2. **D-022 lands three pieces together (F2).** The D-015 credit for gates 1–2,
   the D-018 credit for finish bookkeeping, and this Element meter are all
   reverted on HEAD today; D-022 must re-add them as one reviewed change, all
   ER-only (Block/Class/Flow/State pass `prepay_styled_payload` too), with
   fresh measured evidence. The frozen layout Work must stay ≤ 992,611.
3. **Lock the ER style-code range (accounting invariant).** The 416U/run bound
   covers at most two decorator nodes; it has zero headroom for bold (a third
   node, +136). ErPaint style codes use bits 0–9 only and never set bit 10
   (bold), even though the generic validation mask permits it. Add a
   static/regression assertion that active-ER packed codes stay within bits
   0–9, plus the ER-only meter wiring guard.
4. **New test seam (F4).** Add a testing entry that injects/observes ElementWork
   so exact-budget (`remaining == 0`) and one-unit-short rejection invariants
   can be asserted for all 107 sources on both presets (observed peak 119,488;
   all others ≤ 90,464), alongside the existing Work seam.
5. malloc-block headers are not counted anywhere in the gas scheme and per-run
   vector growth is covered by the row margin; 416U is therefore not a strict
   byte bound at adversarial run density, but the two safety guarantees (the
   meter never rejects an in-domain bounded drawing; real heap is still bounded
   by the canvas contract) are unaffected.

### Predicted matrix delta (must be verified, not hand-edited)

Implementation should change exactly eight ER original rows: original-track
ER wide/restored drawn **68 → 76**, fallback 138 → 130, errors stay 0; total
original wide drawn **663 → 671 (31.28%)**, fallback 1482 → 1474. Narrow cells
follow each source's existing geometry decision (source_fallback or drawn); the
meter never changes geometry. The frozen `MermaidCapabilityReport.OfficialMatrix`
pin (`test_mermaid_official.cpp:8266-8271`, 68/138 → 76/130) and the reviewed
docs (`mermaid-obligation-ledger.md`, conformance/gap/backlog) update through the
normal reviewed-ledger flow. The mandatory-gap roster
(`tools/mermaid/e6c-mandatory-gap-roster.json`, pinned sha `1fe297da…`, 48 rows,
all `body_diagnostic`, zero ER original rows) is **unchanged**: the eight errors
clear and `unknown_messages` empties, with no `--write`; verifier 0 mismatch on
both presets.

## Testing and verification plan

- Debug and Release `-Werror`; each full `ctest -L mermaid` green.
- `MermaidER107Runtime`, `MermaidEr107Source107`, `MermaidOfficialEr107`:
  107/107; exact-budget and one-unit-short invariants for both meters.
- Matrix row-by-row before/after on both presets, byte-identical across
  presets; only ER `error/fallback -> drawn` transitions; no
  drawn->failure and no new error; `verify_e6c_gap_roster.py` 0 mismatch.
- Block/Class/State/Flow consumed Work and rejection outcomes byte-identical
  (their ElementWork is unused / zero).
- ASan/UBSan clean; two independent read-only reviews:
  `agent:/root/b_accounting_review` (the meter counts real allocation, cap is a
  true worst-case bound, no overlap) and
  `agent:/root/b_matrix_regression_review` (dual-preset rows/lifecycle/roster).
- Single impl-area commit, revertible.

## Alternatives

If the only way to admit the eight sources were to raise `kWorkLimit`, charge
fewer real Element fees than the actual ftxui nodes allocated, or weaken a
rejection predicate, this design is **withdrawn** and the eight remain recorded
blockers. The evidence here is that none is required: the real Element tree has
an independent worst-case bound far above the observed peak, so the existing
frozen layout contract and all predicates stay intact.
