# B0b batch 3 design — browser-faithful `space:N` edge-row geometry (read-only)

Status: **DESIGN ONLY for Tech Lead approval. No production change.** Produced
2026-11-04 against `9537a0ac` (batch 2). All findings below were verified
read-only against the pinned upstream source
(`mermaid@12.2.0`, commit `1fb7869…`, fetched to /tmp only) and a real
Chromium render of the pinned fixture; nothing in the repo was modified.

## 1. Ground truth (mermaid 12.2.0, Chromium-measured)

Spaces are parsed into invisible full-cell grid children
(`blockDB.populateBlockDatabase`: `space:N` clones N children;
`layout.ts` skips them in max-size but they consume flow positions). They
are never drawn, and the final SVG is cropped to non-space content bounds
(`findBounds`). Net observed semantics:

- **Flow slots are always consumed** — `A space B` keeps B in column 2 with
  a one-column gap; `space:3` still pushes following entities to grid row 1
  (verified: first `blockArrow` lands in row 2 col 2 beside `id1 id2`,
  upstream and ours agree).
- **Leading/trailing all-space ROWS crop to nothing.** Chromium:
  `columns 1 / space / A / space` renders byte-identically to `A` alone
  (A at y=0, viewBox −25..25); `space:3` + id-row has content at the top
  margin with zero blank band.
- **Internal all-space ROWS keep a full row gap.** `A / space / B` gives a
  96 px center distance vs 48 px for `A / B`; leading space + `A / space / B`
  crops only the leading band (−24/72 vs −48/48).
- **Re-declared arrow directions keep the FIRST declaration** (upstream
  `populateBlockDatabase` merges only `type`/`label`, not `directions`):
  `(y)` then `(x, down)` renders as `(y)`. The rendered polygon is
  identical to fixture 73's verified `(y)` node — our existing heads=10
  output is already faithful. No parser change needed.

## 2. Gap and scope (single fixture, single rule)

We render the leading all-space row as a 5-cell band plus the 3-cell gap
(8 blank rows above `ida`); upstream crops it. Among the 58 pins only
`should-handle-block-arrows-and-spece-statements.mmd` has a leading
all-space row. All other space uses are mixed rows or internal rows
(`A space B`, `A space:5`, neo `Start space:2`, issue-5706 internal
`space:2`) and already match.

**Rule (vertical, per group):** space entities keep consuming flow slots;
in `measure`/`place`, all-space rows before the first (and after the last)
content row contribute height 0 and no inter-row gap; internal all-space
rows keep today's 5-cell row + 3 gap. Horizontal edge-column cropping is a
documented non-goal (no pin exercises it; internal column gaps already
work).

- Production: `src/ui/visual/mermaid_block_impl.cpp` `measure`/`place`
  only. Derive per-group first/last content row from existing
  `Placement` rows; zero those `row_heights`; in `place` add the 3-cell
  gap only between nonzero rows. Space entities, parsing, routing, masks,
  and every other family are untouched.
- Independent oracle: the lockstep mirror
  `tests/support/mermaid_block58_projection.hpp` (`natural/measure/place`,
  same fields) gets the identical rule so the five-frame expected model
  re-derives the cropped geometry independently.

## 3. Tests and evidence

- Block58 five-frame planes: the spece pin re-derives ~8 rows shorter with
  content at the top margin; wide_240/restored_240 stay drawn, narrow_40
  stays source_fallback (widths unchanged). The other 57 pins' planes are
  byte-identical (the projection/oracle change is exercised over all 58).
- New focused assertions in `test_mermaid_block_semantics.cpp`: leading +
  leading/trailing all-space rows collapse to zero; an internal all-space
  row adds exactly one row+gap; flow positions are preserved (entity grid
  coordinates unchanged). `total_spaces` (42) and existing space tests
  stay green.
- Matrix: spece stays drawn with changed cell SHAs only; exactly one case
  changes; `mandatory_gaps` and the roster are untouched; debug/release
  reports raw-byte identical; ASan/UBSan; two independent read-only
  reviews (geometry + evidence), upstream Chromium measurements recorded
  in this doc.

## 4. Rollback and isolation from batch 2

No Work, cap, fee, gate, cache, or `PackedMeter` change;
`kBlockPayloadWorkLimit` 224,107,664 and the batch-2 meter path are frozen
and untouched — this is geometry only, in the Block layout TU and its
oracle mirror. Roll back immediately if any of the other 57 pins changes,
spece loses a drawn plane or gains an error, any non-block family changes,
or the dual-preset reports differ. One local commit, not pushed.
