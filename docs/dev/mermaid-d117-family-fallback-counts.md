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
| After FS-5 G7 (misc shapes; all 42 alias fixtures mapped) | 2026-11-07 | bb55b5bc | 961 | 1184 | 0 |
| After FS-7a (subgraph grammar; direction-less headers + forward group refs) | 2026-11-08 | 575fbcbc | 964 | 1181 | 0 |

Goal: 2145 originals, 0 ordinary fallback, 0 unexpected error at 240
wide; `ctest` green. The frozen 48 body gaps need a final-milestone
disposition (fix or documented infeasibility).

## Remaining fallback by family (whole original, 240 wide)

| Family | Remaining fallback | Drawn | Notes |
|---|---:|---:|---|
| flowchart | 254 | 146 | active track; see breakdown below |
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
| **Total** | **1181** | **964** | |

## Flowchart internal breakdown (263 whole-original fallback)

Content census of the 263 fallback bodies (whole original, 240 wide),
grouped by the debt that keeps the source on ordinary fallback. Many
files carry several debts; counts are indicative buckets, not a partition.
Whole-original frontmatter (neo `look`/`theme`, `wrappingWidth`/
`minNodeWidth` layout config) independently keeps originals source even
when the body would draw.

- ALL 42 nightly-check shape-alias smoke fixtures are mapped as of G7.
  Remaining shape debt lives only on compound rows: `text`/`odd`/
  `anchor`/`circle` short-name shapes and brace-r/braces, held in
  practice by neo config + single-quoted shape-data labels. The
  multi-line classic-label capability now exists for the height-scaling
  stadium contour (authored `<br/>` labels, body-only cohort); fixed
  silhouettes (rings, triangles, waves) still fail closed.
- Compound `shape-themes/connect-all` rows: blocked by other unmapped
  short-name shapes (`text`, `odd`, `anchor`, `circle`, plus the G6
  circles and brace-r) on top of neo config + single-quoted labels.
- `subgraph` framing: FS-7a admitted the grammar-clean rows — direction-less
  (`flowchart`/`graph` with no direction, byte-identical to explicit TB)
  headers and forward group references (an edge naming a subgraph before its
  declaration) — moving 7 body rows to native (3 also whole-original via the
  existing dagre/description envelope profiles). Forward-reference plus a
  reopen stays fail-closed pending a separate positive-Model supplement.
  Remaining subgraph debt is compound: `nodeSpacing`/`rankSpacing`/
  `subGraphTitleMargin` config (no whole-source profile), the FS-1b Work
  ceiling, collapsed-subgraph redirect, and nested `direction` rows.
- Styling/structure: `classDef`/`class`/`style`/`linkStyle`, `init`
  directives, edge markers/styles, and `A & B` fan-out across the rest.
- Body track at 240 after FS-7a: 159 drawn / 193 fallback / **48 frozen
  geometry errors**.

## Candidate infeasible (needs evidence before final milestone)

- **katex (4 originals):** TeX math typesetting. A faithful native
  projection would require a math layout engine; options are (a) accept a
  degraded textual projection behind a reviewed whitelist, or (b) keep
  ordinary fallback with a documented rationale. No decision made; do not
  silently force a lossy render.
- **class link/callback (10 class originals, D-120, BLOCKED on frozen
  contract):** the `link`/`callback` directive parser work is complete and
  tests green (stashed in /tmp/g4probe/class-link-stash/), but admitting
  these whole-null rows makes them native-draw, which trips the frozen
  Class285/Class481 gate (`classes.errors` 0->10, gap roster 48->58). The
  pinned facts (`official_class285_source_facts`, sha 9916a1cb) record
  `class_interaction_contract` + `class_CSS_contract` debts that
  explicitly say "no link/tooltip/callback terminal behavior is newly
  approved". No parser-only path can clear this; the only route is the full
  whole-Model supplement flow (new facts + independent interaction-contract
  review + user re-baselining the frozen approval), a separate milestone.
  Do NOT attempt native drawing of these in the current queue. The other 15
  class fallbacks are cssClass/`:::` styling (same frozen style subsystem).

## Closure log

- 2026-11-07 (0b1c22c4): FS-5 G5 wave documents (+4 shapes) — flowchart
  whole-original drawn 133->137, overall 951->955, fallback 1194->1190,
  frozen gaps stay 48.
- 2026-11-07 (9a66bdfb): FS-5 G6 circle family (+3 shapes: filled/junction,
  framed/stop, crossed/summary) — flowchart whole-original drawn
  137->140, overall 955->958, fallback 1190->1187, body 240 drawn
  145->148 / fb 207->204, original narrow +2 (fixtures 28/37 are fixed
  nine-cell glyphs), frozen gaps stay 48.
- 2026-11-07 (bb55b5bc): FS-5 G7 misc shapes (+3: shaded/lined process, curved
  trapezoid/display, paper tape/flag) - this closes ALL 42 nightly-check
  shape-alias smoke fixtures. Flowchart whole-original drawn 140->143,
  overall 958->961, fallback 1187->1184, body 240 drawn 148->151 /
  fb 204->201, original narrow +1 (fixture 35), frozen gaps stay 48;
  debug/release reports byte-identical (sha256
  a85da2f6b9af94a738c3f607043b8bf2cfc74fa23d8571066c1dcf2b948a6f19).
- 2026-11-08 (aba44036): stadium multiline-label cohort (D-120 Plan A,
  body-only) - authored `<br/>` labels admitted for the height-scaling
  stadium contour; fixed-silhouette classics stay fail closed. Whole
  originals unchanged (961/1184, Flow 257/143, Class285 260/25, gaps 48);
  flowchart body 240 drawn 151->152 / fb 201->200 (body ALL 345/200);
  the one unlocked row's whole original stays source behind the
  Font|RootHtml envelope gate. Debug/release reports byte-identical
  (sha256
  029cffd5fa551a9651bb92e8f24ef392fa9aee4f289fe0055dd7a040b7c83dc5).
- 2026-11-08 (575fbcbc): subgraph grammar — direction-less (`flowchart`/
  `graph` with no direction, byte-identical to explicit TB) headers and
  forward group references (an edge naming a subgraph before its
  `subgraph` declaration, via deferred shells) admitted. Seven body rows
  draw; three also whole-original through existing dagre/description
  envelope profiles, four stay whole-source pending (FS-1b Work ceiling,
  node/rank spacing, subGraphTitleMargin). Forward-ref plus reopen stays
  fail-closed pending a positive-Model supplement. Whole-original drawn
  961->964, fallback 1184->1181 (Flow pending 257->254, admitted/mutated
  143->146); flowchart body 240 drawn 152->159 / fb 200->193 (body ALL
  345->352 / 200->193); Class285 260/25/0 and gaps 48 unchanged;
  debug/release reports byte-identical (sha256
  b6dafa286abe663f24199efa4cb44d9ba673de97803a6ba6ed4acb7896c405ac).
  Independent two-round read-only review APPROVE. Validation note: the
  full 4337-suite non-mermaid failures (Streaming golden `/var/...` path,
  E2E PTY sandbox, ServerMain socket load flake, two stale
  MarkdownMermaid assertions) and ASan-only findings (LeakSanitizer in
  AppRuntime/EnterpriseAuth, UBSan in custom_select/cache_paths, the
  debug/release-only Extra7 preset identity guard, ASan 300s timeouts)
  all reproduce at clean HEAD or lie outside the single changed
  production file; #2103 (renamed
  BareFlowchartDefaultsToTBIncludingGroupContainers) passes isolated at
  0.20s and inside the full run — no FS-7a regression.
