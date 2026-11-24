# D-470 — the flow Work envelope and the admission set it forces

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

## Status

Nothing is committed: with the two pending draws unresolved the matrix is red
(2 errors), and admitting them plus the four collapsed siblings is a census
change beyond round 806's authorized eight rows. The implementation is
shelved in `stash@{0}` for the next round; the phase-1 supplement and its
models are unchanged.
