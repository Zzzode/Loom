# D-470 — Flow400 theme-group re-baseline (phase 2 blockers, measured)

Phase 1 (commit `580d1d8b`) certified the eight supplemental models against
the production renderer. Phase 2 was attempted — open the four redux palettes
in `flow_envelope_body`, wire the supplement as a fifth Flow400 source
authority (`immutable_Flow400_8Models_plus_Theme8`) in the matrix and its
parity test — and hit two measured walls. The attempt is shelved in
`stash@{0}` (`d470-phase2-wip`); the tree is clean at phase 1.

## Blocker A — the fixed 1M flow Work primary rejects the larger body

`render_mermaid_with_work` on the whole source (Work probe,
`/tmp/d461/d470-work-probe.log`):

| Body | 1,000,000 (fixed primary) | 8,000,000 | 200,000,000 |
|---|---|---|---|
| A — TB, five sibling subgraphs, one cylinder (01/05/09/13) | **fallback** (uses 1,000,000) | drawn, uses **1,333,535** | drawn, 1,333,535 |
| B — LR, nested subgraphs (02/06/10/14) | drawn, uses 842,063 | drawn | drawn |

Body A needs 1.33M against the fixed primary, so the whole-source render
falls back before any model is considered. The matrix run shows
`Complete mapped Flow original still uses ordinary source at240` for exactly
those four rows (`/tmp/d461/d470-matrix1.log`). Funding it needs a bounded,
canvas-derived flow Work envelope (the git D-463 / state D-461 / ER canvas
precedent) and its byte-equivalence gate — a shared renderer change.

## Blocker B — the required disclosure footer breaks the Flow400 geometry protocol

Body B rows do draw once the palette opens, but `flow400_geometry` reports
`Unclaimed text/route glyph reveals an extra/misowned/duplicated fact` for
each: the admission appends the required
`config.theme=<palette> -> colour palette; not applied` disclosure below the
art, and the Flow400 model oracle claims every glyph in the frame. The
protocol has no footer-aware variant. FS-1 ledger rows exercise a reviewed
`disclosure_footer` path instead of an upstream Model; D-440's forest row is
admitted through that cohort path, not the Flow400 model path.

## Blocker B side effect — the palette opening is not local to the eight rows

The same run shows E6c gap drift: the four **collapsed**-subgraph siblings
(03/07/11/15, not part of the authorized group and not modelled) move from
"config is the whole blocker" to `flow_whole_null_unmet` once the palettes
admit — observed gap rows 48 → 52, four `unexpected (renderer-only)` entries
in `gap_diff` (`/tmp/d461/d470-matrix1.log` lines 83-93). So a group-scoped
palette opening silently reclassifies four rows outside the group's manifest.
Whichever option is chosen, the collapsed siblings need an explicit
classification alongside it (their bodies use collapsed-subgraph syntax the
renderer does not support, so they stay fallback either way).

## Options for the TL

1. **Footer-aware Flow400 protocol** — a reviewed variant of
   `flow400_geometry` that excludes the exact disclosure block from the glyph
   claim (and the Work envelope for Blocker A). Keeps the model directive;
   both are protected-protocol changes.
2. **FS-1-style disclosure cohort** — admit the eight rows on source-byte
   identity plus the inline five-frame proof with `disclosure_footer`
   handling (no upstream Model). Contradicts the round-804 model directive
   but is the established path for config-bearing declarations.
3. **Hold the palette closed** for this group until either path is
   authorized; the group's models stay certified in phase 1.

Nothing in phase 2 is committed; the phase-1 census is unchanged
(1800/345/0, drawn_at_240 1614, gaps 48).
