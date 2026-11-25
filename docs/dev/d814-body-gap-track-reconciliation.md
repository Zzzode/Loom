# D-814 — the manifest, the E6c roster and the closure sentence name three different tracks

TL round 814 read three artifacts as contradicting each other on the same
fixtures:

1. `docs/dev/d470-rebaseline-manifest.json` — `01/05/09/13-should-render-
   subgraphs-subgraph-containers.mmd` changed `before=source_fallback ->
   after=drawn`;
2. `tools/mermaid/e6c-mandatory-gap-roster.json` (`gates.official_matrix`) —
   the same four fixtures are still listed as
   `{"track":"body_diagnostic","gaps":["ordinary_source_fallback_expected"]}`;
3. the D-470 closure note — "the four `user-styled-subgraph-containers`
   siblings of the group … keep ordinary fallback".

Measurement shows all three statements are true, but they are about **three
different (fixture, track) pairs**, and statement 3 is about a *different
four* fixtures (`04/08/12/16-should-render-user-styled-subgraph-containers`,
not `01/05/09/13-should-render-subgraphs-subgraph-containers`). The defect is
the wording of statement 3, not the data. This note records the measurement
and the corrected wording; no roster row, no protected pin and no renderer
behaviour changes.

## Per-row measurement

Authority: the published official matrix, `build/release/tests/ui/
mermaid-official-matrix.json` (`cases[]` and `mandatory_gaps[]`, 2738 cases,
48 gap rows), produced by `MermaidCapabilityReport.OfficialMatrix`.

| Fixture | corpus `diagnostic.expected` (manifest) | `original` track (240 / 40 / 240) | `body_diagnostic` track |
|---|---|---|---|
| `redux-color-subgraphs/01-should-render-subgraphs-subgraph-containers.mmd` | `source`, reason `unsupported-node-shape:cylinder` | drawn / source_fallback / drawn, 0 errors | error ×3 |
| `…/05-…-subgraphs-subgraph-containers.mmd` | same | drawn / source_fallback / drawn | error ×3 |
| `…/09-…-subgraphs-subgraph-containers.mmd` | same | drawn / source_fallback / drawn | error ×3 |
| `…/13-…-subgraphs-subgraph-containers.mmd` | same | drawn / source_fallback / drawn | error ×3 |
| `visual-check/issue-4648-sibling-subgraphs-…-external-connections.mmd` | `source`, reason `unsupported-arrow:---` | drawn / source_fallback / drawn | error / source_fallback / error |
| `visual-check/v2-18-…-extractable-subgraph.mmd` | `source`, reason `group-edge-endpoint-outside-current-batch` | drawn / source_fallback / drawn | error / source_fallback / error |

These six are exactly the intersection of the 18-row D-470 projection-draw
cohort with the 48-row E6c `official_matrix` gate (4 + 2). The remaining 12
cohort rows are not in the gate.

`04/08/12/16-should-render-user-styled-subgraph-containers.mmd` — the class
the closure note meant — measure `original expected=pending`, observed
`source_fallback` at all three widths, and are in the D-325 roster under
`family_residual:flowchart`. They never enter the E6c gate.

## Why the body rows are `error`, not "fallback"

`classify()` (`tests/ui/test_mermaid_official.cpp`) only emits `Expected exact
ordinary-code source fallback` when `actual.cells != ordinary.cells`; a
renderer refusal produces the ordinary source and books no error. So the four
redux bodies are *drawing* at 240 — the corpus reason `unsupported-node-shape:
cylinder` has been obsolete since the database shape was admitted, and the
unreviewed-drawing diagnostic the matrix writes
(`build/<preset>/tests/ui/mermaid-unreviewed-drawing-<body sha>.txt`) shows the
five subgraph containers with the cylinder contour.

The four redux bodies also draw at 40 columns, and that drawing is clipped:
the layout keeps its 55-column canvas inside a 40-column viewport, so borders
are cut mid-glyph and successive rows restart at increasing offsets (the
captured frame shows `Ingest`, `Fetch`, `Validate` boxes sliced at column 40).
The `issue-4648` and `v2-18` bodies, by contrast, fall back exactly at 40. So
the body track is not merely stale bookkeeping for those four: at narrow width
their body output is art clipped mid-glyph, which is exactly what the corpus
declares `source` and the E6c gate freezes.

## The gate is one class, mechanically

All 48 `gates.official_matrix` rows carry `track=body_diagnostic` and
`gaps=["ordinary_source_fallback_expected"]` — verified over both the roster
and the published matrix (`all(gaps) == True`). The gate is the frozen census
of *corpus body expectations the renderer no longer meets*, not a statement
about whole-source fallback; `docs/dev/mermaid-d117-family-fallback-counts.md`
already states it ("`mandatory_gaps` = 48 (frozen; the 48 body-diagnostic
errors are unchanged)").

## The six entries are not D-470 fallout

Reproduction used a temporary env-gated build that forced every flow envelope
call site off (`LOOM_FLOW_ENV_OFF`; patch reverted, tree clean):

| Measurement | Envelope ON | Envelope OFF |
|---|---|---|
| `mandatory_gaps` rows | 48 | 48 (identical set) |
| Cases whose `observed` changed | — | exactly the 18 cohort `original` rows (all regress to `error` on the five-frame protocol) |
| The 6 body rows above | error (4× also at 40) | error, unchanged |

The four redux bodies drew before the D-470 envelope existed, and the D-461
commit (`02953b44`) carried the same four roster rows. The entries are older
than D-470 and independent of it.

## Decision

- The E6c roster stays as is: retiring a row means declaring that body's
  current art admissible, and for the four redux bodies that would pin clipped
  40-column output. Removing the rows would also be a capability claim that
  needs its own reviewed body-art cohort, not a documentation repair.
- The manifest is unchanged: its rows are whole-source `wide_240` transitions
  derived from the fallback-roster diff.
- The closure wording is corrected in
  `docs/dev/d470-flow-envelope-and-admission-scope.md`: the four siblings are
  named by path, and the body-track relationship of the six overlaps is
  stated explicitly.

## Reproduce

```
python3 -I tools/mermaid/fallback_attribution.py --self-test
LD_LIBRARY_PATH=… ./build/release/tests/ui/test_mermaid_official \
    --gtest_filter=MermaidCapabilityReport.OfficialMatrix:MermaidCapabilityRoster.*
python3 -I tools/mermaid/verify_e6c_gap_roster.py
```

Exit codes and the identity checks are recorded in the round-814 hand-back.
