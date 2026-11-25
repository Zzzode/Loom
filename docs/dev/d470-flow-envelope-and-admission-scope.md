# D-470 — the flow Work envelopes and the admission set they force

Round 806 authorized a canvas-derived, flow-family-only Work envelope
(blocker A), the reviewed `disclosure_footer` cohort path for the four nested
originals (blocker B), and a 12-row manifest with the four collapsed siblings
classified explicitly (side effect). This note records what the envelope
actually does, measured before anything was committed: it admits **two
pending originals beyond the eight-row group**, and that admission cannot be
tuned away.

## The envelope (implemented, uncommitted)

`flow_envelope_layout_allowance(width, height) = cells * 64 + 32768`, enabled
by `flow_layout` once the planned canvas is known (`mermaid.cppm`,
`mermaid_impl.cpp`). The flow render path's `!work.remaining` predicates were
switched to `work.available()` — with the envelope active the primary can
reach zero while the supplemental still funds the render, and the old
predicates aborted exactly at that point (the two-pool rule already documented
on `Work::available`).

Binding measurements (`LOOM_FLOW_ENV_TRACE` probe on the production renderer):

| Row | Canvas | Allowance | Supplemental drawn | Residual/cell |
|---|---|---|---|---|
| redux 01 body (authorized group) | 55x194 | 715,648 | 333,535 | 31.3 |
| visual-check issue-4648 | 87x134 | 778,880 | 160,658 | 13.8 |
| dagre 7827 keep-entry-node | 98x155 | 1,004,928 | 219,868 | 14.5 |

The authorized row is the *densest* of the three, so no formula of the form
`cells * rate + base` that admits it can exclude the other two: the envelope
admits a whole class, not a source list.

## Consequence: two pending originals now draw without authority

With the envelope live, the official matrix reports exactly two new
`flow_whole_null_unmet` rows (errors 0 -> 2, gaps 48 -> 50,
`/tmp/d461/d470-env-matrix2.log`):

- `flowchart/visual-check/issue-4648-sibling-subgraphs-with-different-directions-and-external-connections.mmd`
- `flowchart/nightly-check/dagre/dagre-should-keep-the-entry-node-on-top-of-a-recursive-flow-nested-in-a-subgraph-with-dagre-keepentrynodeontop-true-7827.mmd`

Both are Flow400 pending rows (no Model). Their whole sources now render
natively inside the bounded envelope, which the matrix correctly refuses to
book without a reviewed authority. They are outside the eight-row group and
outside round 806's manifest, so they need an explicit disposition: a
supplement Model each, a reviewed source-pinned cohort entry, or a reviewed
"candidate art" classification. All three touch the protected gap roster
(`tools/mermaid/e6c-mandatory-gap-roster.json` and the byte/SHA pin in
`tests/support/mermaid_gap_roster.hpp`).

## The four collapsed siblings will draw too

The FS-7b collapsed-group grammar is already live: in the current baseline the
collapsed bodies (03/07/11/15) draw at every width as body diagnostics. Once
the redux palettes admit, their whole sources take the same path, so they too
become unreviewed draws rather than "collapsed grammar not modelled". Round
806's preferred classification therefore needs updating: the accurate cause is
"whole source draws through the reviewed collapsed-group projection; pending
Model", not "syntax not modelled".

## Budget gates re-baselined with intent preserved (release green)

- `MermaidFlowRouter.RelaxedRetryIsBoundedAndKeepsWholeSourceWhenUncloseable`:
  the binding row moved K8 -> K12 (K8 is now funded); K12 still exceeds the
  bounded envelope and keeps the whole source.
- `MermaidFlowStyleBaseline.PlainPublicGlyphAndWorkRemainUnchanged`: the four
  exact pins shift by the fixed 40-unit pre-enable charge (329177 -> 329217,
  344485 -> 344525, 323796 -> 323836, 374376 -> 374416); glyph SHAs unchanged.
- `MermaidFlowStyle4Prototype...`: a primary one unit short now succeeds (the
  supplemental funds it, the RFC 0034 ER107 boundary shape); the
  combined-ceiling fail-closed negative is pinned in
  `MermaidEnvelopeWork.FlowEnvelopeIsCanvasDerivedAndFailsClosed`.

## Round 809 outcome (implemented)

Round 809 authorized classification instead of Models ("按类准入"). Measurement
then showed the palette disclosure footer is unclaimed ink for the Flow400
geometry oracle for **both** redux bodies, so none of the eight theme rows can
use the Model path; the phase-1 `flow400_theme8` supplement and its gate were
deleted as superseded (they certified Models the oracle cannot accept). The
oracle itself was not changed and does not know about footers.

A second wall surfaced on the way: the reviewed full-profile parse
prepayment (`flow_parse_prepay_total`) for a 334-byte five-subgraph body is
~1.08M, charged before any canvas exists, so a canvas-derived envelope cannot
fund it. The flow path therefore now runs two bounded pools:

- **parse pool** — `flow_parse_envelope_allowance(prepay_total) =
  prepay_total + 4096`, i.e. exactly the analytic prepayment bound, enabled
  before the prepayment in the frontmatter Flow branch;
- **layout/emission pool** — `flow_envelope_layout_allowance(width, height) =
  cells * 64 + 32768`, enabled once the planned canvas is known and refilled
  there (the same meter, so the parse pool is replaced, not added).
  The flow path's `remaining`-based predicates (`flow_parse_prepay` bounds,
  the router loop and the target loop) read `available()`, the two-pool rule
  `Work` already documents.

The reviewed projection-draw cohort landed with **18 rows** — the 12
redux-color originals (8 theme, 4 collapsed), the two envelope rows and four
sources whose frontmatter prepayment exceeds the fixed primary
(6080/6088-4 diamond intersections, forest-theme-legacy-graph, v2-18 nested
subgraphs). Each row carries its own cause in
`tools/mermaid/d470-projection-draws-cohort.json`; the durable gate is
`test_mermaid_d470_projection_draws_cohort` plus the generator `--check`.

The adversarial review found a latent production hole on the first cut: the
`LayoutRowsCache` hit path re-debited only the cached primary-only layout work
against a flag-off meter, so a future envelope-funded **styled** flow row would
fall back on the next token-driven re-render (today's corpus has no >1M styled
flow row, so this is a defensive fix rather than a reproduced regression).
`NativeLayout` now carries `flow_envelope_allowance` (0 for non-flow layouts),
the cache re-enables the same bounded pool on a hit, and the success check
reads `available()`; a new miss/hit byte-parity test renders the five-subgraph
source (legacy rows plus the palette footer) through the real cache.

The official matrix is green with `flows.pending 240 -> 222`,
`admitted/mutated 160 -> 178`, `decoded` unchanged at 85; the mandatory gap
roster is unchanged (48). The three exact-budget gates were re-baselined with
intent preserved (K8 -> K12 binding row, +40 fixed-charge pins, ER107-shaped
envelope boundary) and a new meter test pins the parse pool.

The D-325 fallback roster is regenerated from this matrix (never hand-edited):
`fallback_attribution.py --emit` reports 345 -> 327 fallback rows over 25
categories, `--self-test` is reproducible with causality on 25/25 categories
and `--matrix` shows no drift. The row-by-row change record is
`docs/dev/d470-rebaseline-manifest.json`, generated mechanically by
`tools/mermaid/d470_rebaseline_manifest.py` from the pre/post roster diff; it
enumerates exactly the 18 flipped rows and their causes.

The round-806 attribution for the four collapsed siblings — "collapsed-subgraph
syntax not modelled" — is **refuted by measurement**: the FS-7b projection
already draws their bodies at every width, and their whole sources draw once
the palettes admit. The accurate cause is now recorded per row.
