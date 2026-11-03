# B0b Block58 eleven-source failure — read-only triage and design input

Status: **read-only design input for Tech Lead D-033 B0b. No production
change.** Produced 2026-11-04 against `1ff18075` (B0a landed). All numbers
below were measured with a temporary test-only budget probe through the
existing testing facade (`block_layout_succeeds` / `layout_succeeds` /
`render_mermaid_with_work` with injected `Work`); the probe was reverted and
the tree is clean. Nothing here raises `kWorkLimit` or relaxes a predicate;
implementation needs shared-change-gate approval.

## 1. The eleven sources, measured at 1,000,000 and at 50,000,000 Work

Costs shown are consumed Work at three layers: **block** =
`block_layout_succeeds` (parser/geometry/paint only); **layout** =
`layout_succeeds` (classify probe + dispatch + valid_packed_rows +
prepay_styled_payload + block); **render** = full public pipeline (layout +
packed `finish_mermaid_payload` Element emission).

| source (tail) | block | layout | render @1M | render @50M |
|---|---:|---:|---:|---:|
| …arrows-and-spece-statements | 672,403 | 768,697 | **reject** (finish 7,392 short) | 1,017,865 ✓ |
| …all-arrow-types-in-9-columns-grid | 719,600 | 807,903 | **reject** (12,768 short) | 1,041,431 ✓ |
| look-neo-with-the-neo-dark-theme | 956,546 | **1,115,317** | reject in layout | 1,431,333 ✓ |
| look-neo-with-the-neo-theme | 956,819 | **1,116,027** | reject in layout | 1,432,003 ✓ |
| rows-with-different-heights… | 878,425 | **1,001,150** | reject in layout | 1,213,974 ✓ |
| nested-redux-color-theme | 719,686 | 820,246 | reject (3,360 short) | 1,083,070 ✓ |
| nested-redux-dark-color-theme | 678,772 | 768,916 | reject (336 short) | 1,016,276 ✓ |
| shapes-inside-…-redux-color | 856,441 | 966,665 | reject (6,048 short) | 1,198,089 ✓ |
| shapes-inside-…-redux-dark-color | 849,575 | 957,923 | reject (6,048 short) | 1,186,659 ✓ |
| shapes-inside-…-redux-dark | 821,771 | 923,541 | reject (8,736 short) | 1,142,333 ✓ |
| shapes-inside-…-redux-theme | 821,101 | 922,515 | reject (984 short) | 1,141,267 ✓ |

**Finding 1 — deterministic accounting, not semantic rejection.** With a
50M budget all eleven sources complete the full public pipeline
(`preflight_complete=1`, zero failed debit). The production renderer never
throws a semantic reject for these inputs; every 1M failure is exact budget
exhaustion at a known gate.

## 2. Two distinct overshoot shapes

The finish-only cost is `render − layout`: 213k–316k across the set.

**Group A — finish emission overshoot (8 sources).** Layout is ≤ 967k; the
packed finish fees (192R+512 entry, 128R/row, 416U/run in
`finish_mermaid_payload`) push the total to 1.016M–1.20M. These are the same
real-Element fees ADR 0032 moved to an independent ElementWork meter for ER;
Block reaches the **same shared function with `er_packed=false`**.

**Group B — layout-layer overshoot (3 sources).** look-neo ×2 and
rows-with-different-heights consume 1.001M–1.116M before finish even starts:
the block parser/paint itself is 878k–957k, and the classify probe plus the
two generic packed gates (`valid_packed_rows` 32R+128 / 4B+2S;
`prepay_styled_payload` 512+6·styled_bytes+4·source) add 123k–159k. Moving
finish fees alone cannot save these; the layout path must also be repriced
or credited.

## 3. Geometry is NOT automatically correct just because it accepts

A 50M render is necessary but not sufficient; the Block58 consumer requires
its full five-frame authority plane. Frame dumps showed:

- **9-columns-grid: geometry substantially correct at 240 cols** — the
  inline `space` token, three rows × six boxes, and all nine edge styles
  render (solid `---/-->/<-->`, bold `===/==>/<==>` degraded to the same
  terminal rule set — a possible style-distinction question for the
  authority, not a layout failure), dashed `-.-/-.->/<-.- >`. This one is
  **pure accounting**, pending the source58 plane check.
- **spece-statements: real feature gap remains.** `space:3` (a
  full-row-width spacer declaration) is not honored: after `ida idb idc`
  the first `blockArrow` flows straight into the third column beside
  `id1 id2` instead of onto a new spacer row. The `(y)` and composite
  `(x, down)` block-arrow directions also need parser/geometry verification
  against the authority model. This source needs an actual Block feature
  cohort in addition to any budget change.
- The other six themed sources (nested ×2, shapes ×4) and the three
  pr-check composites (neo ×2, different-heights) still need the source58
  five-frame plane; budget work must not be credited until they pass
  `auth::validate` with the same pinned model.

## 4. Shared-change gate (required before implementation)

Block and ER both terminate in `detail::finish_mermaid_payload`. Options,
in increasing blast radius:

1. **Generalize the ADR 0032 Element meter to the Block packed path** behind
   the explicit family flag pattern (`er_packed` → a packed-family tag or a
   block flag on `NativeLayout`), with its own worst-case-derived cap and
   bit-range guard. Saves Group A (8 sources). Block has **no ErPaint
   prepayment**, so there is no D-015-style gate credit to import: gates are
   genuinely charged once. Must prove non-packed families and the
   Class/State StyledRows path stay byte-identical, and freeze the
   `NativeLayout` gas footprint (already 88) as ADR 0032 did.
2. **Reprice/credit the generic packed gates for Group B (3 sources).**
   Investigate whether `styled_payload_bytes` (capacity-based ×6)
   over-charges themed nested composites the way the ER capacity probe
   showed elsewhere; any change touches every packed family and needs the
   full two-review gate. The honest alternative is a Block-specific meter
   covering gate + element emission with a canvas-derived cap, leaving
   kWorkLimit at 1M for parser/geometry.
3. **Feature cohort for spece-statements** (`space:N` declaration,
   `(y)`/composite arrow directions): parser + placement work, source-bound
   semantics, plane evidence; independent of the meter work.

## 5. Acceptance sequence (proposal for B0b)

1. Design + two independent reviews for the meter generalization (options
   1/2) before code; decide one cap derivation for Block from its enforced
   canvas limits.
2. Implement meter + frozen-footprint guards; Group A sources should reach
   public acceptance but are credited only after their five-frame planes
   match the pinned Block58 models.
3. Group B gate repricing as a separately gated sub-batch.
4. Feature cohort for spece-statements as a separate batch.
5. Each sub-batch: debug + release `ctest -L mermaid`, source58/official
   matrix row-by-row, roster reviewed (the Block unknown-gap rows at
   OfficialMatrix :8508 and the 47/58 pin at :8463 clear only through the
   consumer evidence flow, not by editing counts), ASan/UBSan.

## 6. Non-goals

No `kWorkLimit` change, no predicate relaxation, no paper/roster
regeneration in this phase, no implementation before Tech Lead
confirmation of the design and the shared-change scope.
