# D-117 Mermaid zero-fallback family ledger

Living per-family count of ordinary fallback across the pinned Mermaid
12.2.0 official corpus (commit recorded in
`tests/fixtures/mermaid_official/manifest.json`), maintained under the
TechLead 19:30 milestone workflow (2026-11-07). Updated at every family
closure from the serial `MermaidCapabilityReport.OfficialMatrix` JSON.

All counts are the **whole-original** track at **240 columns** unless
stated otherwise. `drawn + source_fallback = family size` (the frozen 48
body-geometry gaps are a separate, flowchart-body-only disposition; they
are not whole-original errors).

## Headline

| Snapshot | Date | Commit | Drawn | Fallback | Error |
|---|---|---|---|---|---|
| Baseline (D-066 start) | 2026-11-04 | e7fbd0ef | 863 | 1282 | 0 |
| After FS-5 G5 (wave documents) | 2026-11-07 | 0b1c22c4 | 955 | 1190 | 0 |
| After FS-5 G6 (circle family) | 2026-11-07 | 9a66bdfb | 958 | 1187 | 0 |

Goal: 2145 originals, 0 ordinary fallback, 0 unexpected error at 240
wide; `ctest` green. The frozen 48 body gaps need a final-milestone
disposition (fix or documented infeasibility).

## Remaining fallback by family (whole original, 240 wide)

| Family | Remaining fallback | Drawn | Notes |
|---|---:|---:|---|
| flowchart | 260 | 140 | active track; see breakdown below |
| requirement | 160 | 12 | mostly unbuilt renderer surface |
| git | 80 | 55 | |
| state-diagram-v2 | 78 | 25 | |
| gantt | 52 | 0 | renderer not started |
| swimlanes | 46 | 0 | renderer not started |
| xychart | 46 | 2 | partial axes/art only |
| agentflow | 38 | 0 | flowchart-syntax variant |
| c4 | 32 | 0 | |
| usecase | 32 | 0 | |
| er-diagram | 31 | 13 | |
| timeline | 30 | 0 | |
| class-diagram | 25 | 260 | |
| treemap | 26 | 0 | |
| architecture | 20 | 0 | |
| sankey | 18 | 0 | |
| journey | 18 | 0 | |
| pie | 17 | 7 | |
| state-diagram | 26 | 16 | |
| tree-view | 15 | 0 | |
| quadrant-chart | 14 | 0 | |
| ishikawa | 13 | 0 | |
| kanban | 11 | 0 | |
| mindmap | 19 | 14 | |
| venn | 21 | 0 | |
| wardley | 10 | 0 | |
| railroad | 9 | 0 | |
| block | 0 | 58 | complete (B0a/B6) |
| sequence | 0 | 193 | complete |
| cynefin | 12 | 0 | |
| eventmodeling | 6 | 0 | |
| packet | 6 | 0 | |
| radar | 6 | 0 | |
| conf-and-directives | 3 | 1 | |
| info | 2 | 0 | |
| katex | 4 | 0 | candidate infeasible, see below |
| zenuml | 1 | 0 | |
| **Total** | **1187** | **958** | |

## Flowchart internal breakdown (263 whole-original fallback)

Content census of the 263 fallback bodies (whole original, 240 wide),
grouped by the debt that keeps the source on ordinary fallback. Many
files carry several debts; counts are indicative buckets, not a partition.
Whole-original frontmatter (neo `look`/`theme`, `wrappingWidth`/
`minNodeWidth` layout config) independently keeps originals source even
when the body would draw.

- Shape-alias smoke fixtures still unmapped after G6: fixture 15
  (lined/shaded rectangle), 24 (curved trapezoid/display), 35 (paper
  tape/flag). G6 closed the circle family (16 framed/stop, 28 filled/
  junction, 37 crossed/summary); the brace-r/braces and the `text`/`odd`/
  `anchor`/`circle` short-name shapes remain on the connect-all rows.
- Compound `shape-themes/connect-all` rows: blocked by other unmapped
  short-name shapes (`text`, `odd`, `anchor`, `circle`, plus the G6
  circles and brace-r) on top of neo config + single-quoted labels.
- `subgraph` framing: ~31 body-fallback + 13 body-error rows.
- Styling/structure: `classDef`/`class`/`style`/`linkStyle`, `init`
  directives, edge markers/styles, and `A & B` fan-out across the rest.
- Body track at 240 after G6: 148 drawn / 204 fallback / **48 frozen
  geometry errors**.

## Candidate infeasible (needs evidence before final milestone)

- **katex (4 originals):** TeX math typesetting. A faithful native
  projection would require a math layout engine; options are (a) accept a
  degraded textual projection behind a reviewed whitelist, or (b) keep
  ordinary fallback with a documented rationale. No decision made; do not
  silently force a lossy render.

## Closure log

- 2026-11-07 (0b1c22c4): FS-5 G5 wave documents (+4 shapes) — flowchart
  whole-original drawn 133->137, overall 951->955, fallback 1194->1190,
  frozen gaps stay 48.
- 2026-11-07 (9a66bdfb): FS-5 G6 circle family (+3 shapes: filled/junction,
  framed/stop, crossed/summary) — flowchart whole-original drawn
  137->140, overall 955->958, fallback 1190->1187, body 240 drawn
  145->148 / fb 207->204, original narrow +2 (fixtures 28/37 are fixed
  nine-cell glyphs), frozen gaps stay 48.
