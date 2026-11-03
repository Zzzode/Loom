---
rfc: 32
title: Independent Work dimension for ER ftxui Element emission
status: provisional
owners: "@Zzzode"
reviewers: ["agent:/root/b_accounting_review (ADR 0032 design approved 2026-11-03; implementation re-review APPROVE after MAJOR-1/MINOR-1/NIT-1 fixes, 2026-11-03)", "agent:/root/b_matrix_regression_review (ADR 0032 design approved 2026-11-03; implementation matrix review APPROVE pending release/ASan gates, 2026-11-03)"]
created: 2026-11-03
last-reviewed: 2026-11-03
tracking: Tech Lead D-021 design-only; D-023 implementation landed 2026-11-03, pending final accounting+matrix reviews and graduation
---

# ADR 0032: Independent Work dimension for ER ftxui Element emission

- Status: **Provisional** (implemented under Tech Lead D-023 on 2026-11-03;
  graduation pending the final two independent reviews — accounting and matrix).
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

Measured on the 107 pinned ER originals at the packed-payload boundary.
**Implementation-verified 2026-11-03** through the ADR 0032 F4 public-path
seam (`MermaidEr107ElementWork`, debug + release): 107/107 admitted, maximum
ElementWork **119,488** (0.12% of `E_limit`), margin **102,444,864**. The F4
run corrected one mislabeled row in the design-stage temporary trace: the
112,512 drawing is `entities-with-aliases-handdrawn-look`, not the complex
redux pair (which measure 94,304), so **nine** (not eight) originals exceed
90,464. Only eight of them exhausted the shared pool before this ADR; the
ninth fit under 1,000,000 on layout Work and never failed.

| original (tail) | ElementWork | layout Work (public) |
|---|---:|---:|
| er-diagram-with-all-relationship-types … redux-color / … redux-dark-color (×2) | **119,488** | 892,145 / 894,870 |
| entities-with-aliases-handdrawn-look | **112,512** | 871,853 |
| a-not-so-simple … without-htmllabels elk-layout | 97,248 | 992,611 |
| a-not-so-simple … without-htmllabels handdrawn | 96,512 | 989,968 |
| a-not-so-simple … without-htmllabels default | 95,040 | 984,266 |
| a-not-so-simple … handdrawn-look | 94,304 | 981,446 |
| complex-er-diagram-with-multiple-relationships … redux-color / … redux-dark-color (×2) | 94,304 | 983,439 / 985,714 |

The other 98 originals: ≤ 90,464 ElementWork. The Element tree is built once
per render regardless of capture width, so the 40-column frames incur no
additional ElementWork (they cannot be treated differently from the 240
frames).

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

### Parser fee relocation carried by the same change

The 992,611 figure presupposes one fee relocation that is part of the same
ER-only restoration and is recorded here explicitly. Commit `4b58709c` (one of
the two post-base commits that added the ER overshoot) added a
`8*sizeof(std::optional<std::string>)+128 = 384` prepay at the entry of
`parse_relations`, charged to **every** family. The two fields it funds are
`RelationDiagram::class_accessibility_title` and
`class_accessibility_description`; their only writer is the Class
accessibility-metadata helper (`mermaid_relations_core_impl.cpp:1926`). Both ER
branches return to `parse_plain_er` before the continuation where those fields
are reachable, and a disengaged `optional<string>` allocates no heap, so the fee
never corresponded to an ER allocation. The implementation moves the debit to
the Class/State continuation (after both ER early returns): Class and State
charges are byte-identical, and ER consumed Work drops by exactly 384, which is
why the public-path layout maximum is exactly 992,611 rather than 992,995. The
frozen base `115070b9` charged the fee nowhere (the fields did not exist yet).
The relocation is placed so every non-ER schedule is byte-identical to HEAD:
plain `classDiagram`/`stateDiagram*`, Class envelopes and Requirement pay it
once on the continuation, and the **frontmatter-State** outer invocation —
which at HEAD paid the entry fee and then recursed into `parse_relations`
(768 total) — keeps a matching charge on its outer dispatch before the
recursive body call, so its total is unchanged (the ER envelope branch next to
it deliberately pays nothing). The only frozen test pin affected is the `er`
row of `MermaidClassStyleBaseline.PlainGlyphsAndMeasuredWorkRemainUnchanged`
(12,964 → 12,580; rendered glyph SHA unchanged); all four Class/State rows in
that test stay byte-identical.

### Frozen NativeLayout gas footprint (implementation-review finding)

Adding `er_packed`/`payload_prepaid` grew `sizeof(NativeLayout)` 88 → 104, and
`styled_payload_bytes` seeds its estimate from that size (charged ×6 by the
gate2 prepay and reused as the cache byte measure). Charging the real new size
would silently raise every styled non-ER render by 96 Work (Block, Class/State,
styled Flow), violating the byte-identical gate. The charge basis is frozen at
`kNativeLayoutGasFootprint = 88` (`mermaid_style_impl.cpp`, guarded by
`static_assert(88 <= sizeof(NativeLayout))`): the two ER-only side fields fund
no allocation on any path, ER's `packed_payload_prepaid` recomputes through the
same function so its credit moves with the charge, and cache entry byte
measurements return exactly to their HEAD values.

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
   can be asserted for all 107 sources on both presets (verified peak 119,488;
   nine sources exceed 90,464 — see the measured table), alongside the existing
   Work seam. The seam also proves a non-ER packed family (Block) never
   activates the meter or touches an injected one.
5. malloc-block headers are not counted anywhere in the gas scheme and per-run
   vector growth is covered by the row margin; 416U is therefore not a strict
   byte bound at adversarial run density, but the two safety guarantees (the
   meter never rejects an in-domain bounded drawing; real heap is still bounded
   by the canvas contract) are unaffected.

### Measured matrix delta (verified 2026-11-03, debug; release must match)

The design-stage estimate compared against a hypothetical D-018 tree (only the
marginal eight). Measured against **HEAD** — where the D-015 gate credits, the
D-018 finish credit and this Element meter are all absent together (F2) — the
change moves exactly **17** ER original rows (9 admitted by the re-added
credits, 8 by the Element meter), row-by-row in the published
`mermaid-official-matrix.json`:

- original-track `er-diagram` wide/restored **drawn 158 → 175 (+17)**,
  `source_fallback` 48 → 31 (−17), error rows stay 0;
- original-track ALL-family wide/restored **drawn 835 → 852 (+17)**,
  fallback 1297 → 1280; errors stay 13 (entirely non-ER);
- ER narrow_40 drawn 113 → 120 (**+7**); the other ten rows keep their
  pre-existing `source_fallback` geometry decision at 40 columns;
- `ER107_native_whole_source.local_ready` 90 → 107; `failed_rows` 17 → 0;
- every transition is `source_fallback → drawn`; there is **no** drawn→failure
  and no new error. The hard aggregate pins at `test_mermaid_official.cpp:8260-8269`
  (recorded diagnostics 206/68/138/0) and the owned-originals pin at :8251-8256
  stay byte-unchanged and now pass; no frozen count pin is edited.

The mandatory-gap roster (`tools/mermaid/e6c-mandatory-gap-roster.json`, 48
rows, zero ER original rows) is **unchanged with no `--write`**: the published
`mandatory_gaps` array is byte-identical before/after (52 rows), and the 17 ER
`unknown_messages` (28 → 11) clear. Two distinct pre-existing baselines remain,
identical before and after this change and out of scope: the four observed gap
rows outside the pinned 48 (the Flow Extra7 set — shape-alias 02/06, dagre
6088-2, sub-graphs) account for the 52-vs-48 roster difference, and the 11
remaining unknown messages are the Block58 rows (nine themed accounting rows +
two structural feature gaps). Reviewed ledger docs
(`mermaid-obligation-ledger.md`, conformance/gap/backlog) update through the
normal reviewed-ledger flow; no paper or roster is regenerated by this change.

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
