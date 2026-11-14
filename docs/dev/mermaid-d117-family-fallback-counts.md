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
| After FS-aws (closed-vocabulary AWS icon shape data) | 2026-11-08 | 9f0ae75a | 967 | 1178 | 0 |
| After FS-spacing (non-terminal nodeSpacing/rankSpacing envelope leaves) | 2026-11-09 | 0606f021 | 971 | 1174 | 0 |
| After FS-7b Slice B (collapsed subgraphs render as one titled node) | 2026-11-09 | a2ea36e4 | 973 | 1172 | 0 |
| After FS-7c Slice C (collapsed endpoint remap; redux bodies) | 2026-11-09 | c0510727 | 975 | 1170 | 0 |
| After FS-elk (non-terminal elk.rectpacking layout engine) | 2026-11-09 | eb3118c8 | 976 | 1169 | 0 |
| After FS-maxwidth (dagre engine + useMaxWidth composite profile) | 2026-11-10 | 2f04cb74 | 978 | 1167 | 0 |
| After journey family renderer (10 theme-free originals) | 2026-11-11 | 744c3a24 | 988 | 1157 | 0 |
| After timeline family renderer (7 theme-free originals) | 2026-11-12 | fe3eb520 | 995 | 1150 | 0 |
| After gantt family renderer (39 valid-date originals) | 2026-11-12 | 9e8d2950 | 1034 | 1111 | 0 |
| After treemap family renderer (10 plain originals) | 2026-11-12 | 4deb2144 | 1044 | 1101 | 0 |
| After c4 family renderer (23 structural originals) | 2026-11-12 | e70e8c34 | 1067 | 1078 | 0 |
| After venn family renderer (11 clean originals) | 2026-11-13 | b53736d8 | 1078 | 1067 | 0 |
| After quadrant family renderer (7 clean originals) | 2026-11-13 | fbb34e13 | 1085 | 1060 | 0 |
| After architecture family renderer (16 clean originals) | 2026-11-13 | 86288941 | 1101 | 1044 | 0 |
| After kanban family renderer (11 clean originals) | 2026-11-13 | d42baf8b | 1112 | 1033 | 0 |
| After usecase family renderer (14 clean originals) | 2026-11-13 | 74506fba | 1126 | 1019 | 0 |
| After tree-view family renderer (8 clean originals) | 2026-11-13 | 2620d828 | 1134 | 1011 | 0 |
| After ishikawa family renderer (7 clean originals) | 2026-11-13 | b921654d | 1141 | 1004 | 0 |
| After cynefin family renderer (7 clean originals) | 2026-11-13 | 3c73ca54 | 1148 | 997 | 0 |
| After wardley family renderer (6 clean originals) | 2026-11-13 | ca676cb5 | 1154 | 991 | 0 |
| After railroad family renderer (8 clean originals) | 2026-11-13 | b02234b3 | 1162 | 983 | 0 |
| After radar family renderer (3 clean originals) | 2026-11-14 | 893835da | 1165 | 980 | 0 |
| After packet family renderer (4 clean originals) | 2026-11-14 | fd6e020b | 1169 | 976 | 0 |
| After eventmodeling family renderer (6 clean originals) | 2026-11-14 | 7d4730bf | 1175 | 970 | 0 |
| After requirement style renderer (4 styled originals) | 2026-11-14 | 6c16fe94 | 1179 | 966 | 0 |
| After agentflow family renderer (27 clean originals) | 2026-11-14 | local | 1206 | 939 | 0 |
| After sankey family renderer (1 clean original) | 2026-11-14 | local | 1207 | 938 | 0 |
| After D223 state reachable-semantics batch one (7 state originals) | 2026-11-14 | local | 1214 | 931 | 0 |
| After D228 state classDef/class/style group (4 representable state style originals) | 2026-11-14 | local | 1218 | 927 | 0 |
| After D235 gitGraph empty quoted-tag group (3 representable git originals) | 2026-11-14 | local | 1221 | 924 | 0 |

Goal: 2145 originals, 0 ordinary fallback, 0 unexpected error at 240
wide; `ctest` green. The frozen 48 body gaps need a final-milestone
disposition (fix or documented infeasibility).

## Remaining fallback by family (whole original, 240 wide)

| Family | Remaining fallback | Drawn | Notes |
|---|---:|---:|---|
| flowchart | 240 | 160 | active track; see breakdown below |
| requirement | 156 | 16 | renderer extended 2026-11-14; classDef/class/style/`:::` body statements now validated and rendered through the shared palette-role style seam (4 new draws); 156 frontmatter config/theme/title/indented-fence rows stay source (no theme/layout/color contract) |
| git | 77 | 58 | D235: +3 explicit empty quoted tag (`tag: ""`) cherry-pick originals; empty string decodes to no label (no glyphs, same frame as an omitted tag). Remaining 77 = 62 non-whitelisted theme/layout frontmatter + 9 `%%{init}%%` + 6 E2a git blockers (4 nine-branch canvas-overflow, 1 unprojected title, 1 multiline quoted branch label) |
| state-diagram-v2 | 70 | 33 | D223 batch one: +4 (HTML-entity fork/join, square choice, note tab break, edge `\n`/`#colon;`); D228: +4 representable classDef/class/style originals (palette-role fill/stroke/color, border shorthand to stroke, bold to pixels); the 1 italic fixture, 1 click, 1 description-tab and the inline `Node:::class` edge form stay gated, plus geometry/concurrency debt |
| gantt | 13 | 39 | renderer added 2026-11-12; 11 themed rows stay source (no color contract), 2 malformed-date fixtures fail closed |
| swimlanes | 46 | 0 | hard wall, no renderer; re-confirmed 2026-11-14 through the production render_markdown seam (0 drawn / 46 fallback): every fixture is swimlane-beta (LR/TD 23 each) and carries a `config.flowchart.htmlLabels` + `useMaxWidth` visual/layout contract (46/46), 18 add theme/look and 5 carry body classDef/class/style; native text rendering cannot honor the lane geometry without silently dropping those contracts, so the whole family stays gated; unblock needs a real lane-geometry renderer |
| xychart | 46 | 2 | partial axes/art only |
| agentflow | 11 | 27 | renderer added 2026-11-14; full agentflow-beta grammar (flow/global/connector containers, @{ } metadata, four edge vocabularies, labeled branches) validated and projected as a bounded native outline, not a flowchart approximation; 11 config theme/look rows stay source (no color/layout contract); live census 27/11/0 from 38; 19 of 27 draw at 40 columns |
| c4 | 9 | 23 | renderer added 2026-11-12; 8 Update*Style/UpdateLayoutConfig + 1 frontmatter config rows stay source (no color/layout contract) |
| usecase | 18 | 14 | renderer added 2026-11-13; 12 frontmatter theme/look/config + 6 body classDef/:::/style/init rows stay source (no placement/color contract) |
| er-diagram | 31 | 13 | |
| timeline | 23 | 7 | renderer added 2026-11-11; 23 themed rows stay source (no color contract) |
| class-diagram | 25 | 260 | |
| treemap | 16 | 10 | renderer added 2026-11-12; 10 classDef/::: style + 6 frontmatter config rows stay source (no color/number-format contract) |
| architecture | 4 | 16 | renderer added 2026-11-13; 4 frontmatter config layout knobs (randomize/seed/idealEdgeLengthMultiplier) stay source (no fcose layout contract) |
| sankey | 17 | 1 | renderer added 2026-11-14; the single description-only `sankey-beta` original has its three-field CSV link rows (`source,target,value`) validated against a closed grammar (printable labels, no quotes/embedded commas/control bytes, bounded positive integer) and transcribed as a bounded native outline rather than an approximate flow graph; the 17 plain-`sankey` dialect originals all carry `config.sankey` layout/color contracts and stay source; live census 1/17/0 from 18, the reachable row draws at 40 columns |
| journey | 10 | 18 | renderer added 2026-11-11; 8 themed rows stay source (no color contract) |
| pie | 17 | 7 | |
| state-diagram | 23 | 19 | D223 batch one: +3 (HTML-entity fork/join top-level and nested, edge literal `\n`); the v1 note twin stays gated by a real tab in its folded description (shared unicode_text contract), plus labels/composite/concurrency geometry debt |
| tree-view | 7 | 8 | renderer added 2026-11-13; 5 frontmatter layout/icon-config + 2 body :::class rows stay source (no icon/color contract); live census 8/5/2 corrected an older 9/4/2 via a trailing-whitespace fence |
| quadrant-chart | 7 | 7 | renderer added 2026-11-13; 5 %%{init}%% visual directives (axis position/sizing/theme colors) + 2 inline point-style rows stay source (no geometry/color contract) |
| ishikawa | 6 | 7 | renderer added 2026-11-13; 6 frontmatter theme/look/config rows stay source (no fishbone geometry/color contract); live census 7/6/0 from a candidate pool of 13 |
| kanban | 0 | 11 | renderer added 2026-11-13; all eleven originals clean (description-only frontmatter; cards with optional @{...} structural metadata, no color contract) |
| mindmap | 19 | 14 | |
| venn | 10 | 11 | renderer added 2026-11-12; 7 frontmatter theme/look/config + 3 body style/classDef rows stay source (no circle/color contract) |
| wardley | 4 | 6 | renderer added 2026-11-13; 4 frontmatter theme-test rows stay source (no map-canvas/color contract); live census 6/4/0 from a candidate pool of 10; the only non-ASCII byte in the family is inside one frontmatter description |
| railroad | 1 | 8 | renderer added 2026-11-13; first multi-dialect family (railroad-beta IR + abnf/ebnf/peg-beta); 1 frontmatter theme row stays source (no track/color contract); live census 8/1/0 from a candidate pool of 9; only non-ASCII byte is in one frontmatter description |
| block | 0 | 58 | complete (B0a/B6) |
| sequence | 0 | 193 | complete |
| cynefin | 5 | 7 | renderer added 2026-11-13; 5 frontmatter config/theme/look rows stay source (no boundary-geometry/color contract); live census 7/5/0 from a candidate pool of 12 |
| eventmodeling | 0 | 6 | renderer added 2026-11-14; first whole-pool-clean family (6/0/0); column-0 frames/data blocks, dotted names, ->> relations; all bodies pure ASCII, no frontmatter gate in the family |
| packet | 2 | 4 | renderer added 2026-11-14; packet/packet-beta alias, single-bit and ranged fields; 2 frontmatter config.packet rows stay source (no bit-cell/color contract); live census 4/2/0 from a candidate pool of 6; all bodies pure ASCII |
| radar | 3 | 3 | renderer added 2026-11-14; 3 frontmatter config/theme/themeVariables rows stay source (no polygon/color contract); live census 3/3/0 from a candidate pool of 6; only non-ASCII bytes are in a gated frontmatter |
| conf-and-directives | 3 | 1 | |
| info | 2 | 0 | |
| katex | 4 | 0 | candidate infeasible, see below |
| zenuml | 1 | 0 | |
| **Total** | **924** | **1221** | |

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
  Remaining subgraph debt is compound: `subGraphTitleMargin`
  config (non-terminal but D-120 blocked for whole-source — see the
  candidate-infeasible section; `nodeSpacing`/`rankSpacing` gained a
  profile under FS-spacing), the FS-1b Work ceiling, collapsed-subgraph
  double-collapse (outer and nested group both collapsed; Slice B/C admit
  single-collapse including endpoint remap), and nested `direction` rows.
- Styling/structure: `classDef`/`class`/`style`/`linkStyle`, `init`
  directives, edge markers/styles, and `A & B` fan-out across the rest.
- AWS icon shape data (`@{ icon: "aws:..." }`): FS-aws admitted a closed
  three-name vocabulary, each projecting to one fixed marker cell with the
  human text retained; unknown icons and non-aws namespaces fail closed.
- Single-quoted shape-data labels (`@{ ..., label: '...' }`): FS-sq admits
  both quote styles through one strict validator; four neo connect-all
  bodies now draw (body-only — the neo frontmatter still gates the whole
  original). Five-plus heavy classic-glyph node graphs hit a separate
  quote-independent Work/layout budget (also fails with double quotes),
  tracked as the remaining connect-all blocker.
- Body `%%{init}%%` directives and accessibility statements (FS-init):
  the directive whitelist gains single-quoted JSON5 normalization and the
  inert `logLevel` key (integer 0..5 or the six named levels; the
  pre-existing double-quoted `flowchart.htmlLabels` shape is unchanged),
  and `accTitle:`/`accDescr:` scalars are consumed as non-visual no-ops
  mirroring the class-family contract (block form, markup content,
  duplicates and subgraph placement fail closed). Four legacy theme
  fixtures draw body only; their frontmatter `theme:` key keeps the whole
  original source. The four new-syntax siblings, whose directive adds a
  `theme` key, stay fail-closed.
- Whole-source envelope spacing leaves (FS-spacing): `flowchart.nodeSpacing`
  and `flowchart.rankSpacing` only position dagre ranks in the upstream SVG
  layout; the terminal uses its own fixed layout, so they are non-terminal.
  A non-negative pixel integer (bare or single/double quoted) is now an
  accepted envelope leaf alongside the bare `flowchart` namespace or a
  courier-font flowchart; signs, fractions, exponents, mismatched quotes,
  structured values and any visual co-key (look/theme/htmlLabels/...) keep
  the source. Four whole-and-body fixtures now draw the full original at
  240/restored (source at 40); the two nightly handDrawn lookalikes stay
  excluded behind the `look` gate. The body track is unchanged.
- Layout-engine envelope leaves (FS-elk): the upstream `config.layout`
  engine choice never reaches the terminal, which runs its own fixed
  layout, so it is non-terminal exactly like the already-accepted
  `layout: dagre`. Only the single bare `layout: elk.rectpacking` value is
  whitelisted (one visual-check-free container fixture, whole-and-body,
  draws at every width). Bare `elk` / `elk.box` stay whole-source for two
  independent, evidence-backed reasons: their corpus fixtures either cross
  the envelope double-parse Work ceiling (the FS-1b debt — the identical
  graph draws on the single-parse body track, and the elk.box/rectpacking
  pair differ only by two extra `%%` comment lines charged twice), or carry
  a subgraph/body shape debt that opening the envelope would surface as a
  new matrix error (verified by a broad-whitelist probe that flipped
  `3-layout-elk-subgraphs` source→error). Quoted engine values, the `elk:`
  option namespace and unknown engines fail closed.
- Body track at 240 after FS-init: 170 drawn / 182 fallback / **48 frozen
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
- **subGraphTitleMargin whole-source admission (1 whole original, D-120,
  BLOCKED on the frozen gap roster; investigated 2026-11-09, no code
  shipped):** `flowchart.subGraphTitleMargin` is a nested `{top, bottom}`
  pixel map and is genuinely non-terminal — the renderer never reads it
  (`grep` zero hits in `src/`) and `group_frame` places every subgraph
  title at the fixed offset `{box.x, box.y + 1}`. Five corpus fixtures
  carry the key. Whitelisting the map makes the renderer natively draw
  three whole originals (self-loops plus the two plain title-margin
  td/lr rows), but only self-loops has an FS-7a ledger branch that can
  authorize the draw; the td/lr rows have no ledger admission and are
  pinned in the E6c roster under the GE15
  `group-edge-endpoint-outside-current-batch` body debt. The envelope
  cannot distinguish self-loops from the lr row — their frontmatter
  values are byte-identical (`top: 10`, `bottom: 5`) — so any whitelist
  also flips td/lr, which then book as three new
  `flow_whole_null_unmet` errors and move the mandatory roster 48 -> 51
  (verified by a full OfficialMatrix run; `flows.errors` 0 -> 3). A
  self-loops-only migration still leaves td/lr at roster 50, so no
  green path exists without first closing the GE15 group-edge-endpoint
  grammar and re-baselining the frozen roster in a separate milestone.
  The other two fixtures stay out independently: the htmlLabels=false
  sibling and the edge-labels row (whose body genuinely falls back on
  cross-boundary labeled group edges).

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
- 2026-11-08 (9f0ae75a, FS-aws): closed-vocabulary AWS icon shape data —
  `@{ icon: "aws:..." }` admits three pinned AWS architecture names,
  each projecting to one fixed U+25A0 marker cell with the human label
  (or node id) retained; unknown icons and non-aws namespaces fail
  closed, repeated icon statements stay idempotent, and bracket-node
  icon metadata is rejected rather than dropped. Whole-original drawn
  964->967, fallback 1181->1178 (Flow pending 254->251,
  admitted/mutated 146->149); flowchart body 240 drawn 159->162 / fb
  193->190 (body ALL 352->355 / 193->190); Class285 260/25/0 and gaps
  48 unchanged; debug/release reports byte-identical (sha256
  fa2f750af20a3181981ebec696382155a9e7f36a5504875a260b14181e1b6f70),
  also byte-identical before/after the idempotency hardening. Two-round
  independent read-only review APPROVE. Non-blocking follow-up R1: a
  mixed icon/label redeclaration can omit the decoration cell (absent
  from the corpus) — future hardening to re-prefix or fail-closed.
  Timing note: #2609 Config110 sits ~287-291s against a 300s ctest cap;
  under sustained host overload (load 53-68 on 56 cores) it can time out
  in ctest but always PASSES run directly without the cap. Matched-load
  A/B timing shows zero FS-aws cost (FS-7a baseline 291s vs FS-aws 287s);
  #2726 Class285 passes isolated (233s). This is environmental test
  fragility, not a code regression.
- 2026-11-08 (FS-sq): single-quoted shape-data labels — `@{ ..., label:
  '...' }` now decodes through one strict helper
  (`read_shape_data_label`) shared by both quote styles; the AWS icon
  *name* stays double-quoted and closed-vocabulary. No backslash, no
  embedded double quote, same-quote matching, and the existing `label()`
  validator keep the accept/reject set identical to the old double-quote
  path plus the intended single-quote forms; malformed quoting fails
  closed. Four neo shape-theme connect-all bodies draw as a body-only
  cohort (the neo look/theme frontmatter still gates the whole
  original). Five-node heavy classic-glyph graphs stay source under the
  quote-independent routing/Work budget, so the fs5g6 ledger migrates in
  place: 3 whole-and-body + 4 body-only draw, 8 blocked (the stale
  curved-trapezoid/neo-quoted-only blocker vocabulary is retired into a
  single `flow-g6-classic-glyph-budget`; fixture sha pins untouched).
  Whole originals unchanged at 967/1178/0 (Flow pending 251,
  admitted/mutated 149); flowchart body 240 drawn 162->166 / fb 190->186
  (body ALL 355->359 / 190->186); Class285 260/25/0 and the mandatory 48
  gaps unchanged; integrity empty. Debug/release serial reports
  byte-identical (sha256
  7a26ea36e2100c62229bc23fcf1f7334e035ab7432d4d6d6ae7cb9fac8ebf180).
  Independent read-only adversarial review (fresh general-purpose agent,
  task ad678f3037149e883, zero builds) APPROVE; both MINOR findings
  resolved (helper comment no longer claims byte-identical double-quote
  trimming; blocker vocabulary reclassified). Full mermaid label suite
  1164/1164 green; ASan contact surface 21/21 with zero sanitizer hits.
  Three-family serial timing gate written to the independent log
  `/tmp/g4probe/serial-fs-sq.log` (SERIAL_RC=0, per-case times): #2609
  250.03s, #2726 197.67s, #3511 28.77s, total 519.40s — all inside the
  300s cap. Non-blocking follow-up: the G5/G7 ledgers still carry
  semantically-stale `neo-quoted-only` blocker labels (states remain
  source; no test asserts the kind string) for a future ledger pass.
- 2026-11-08 (FS-init): body `%%{init}%%` logLevel directive plus
  accTitle/accDescr accessibility statements — a quote normalizer
  (mirroring the class family's class_init_meta) rewrites single-quoted
  directive strings to strict JSON (backslash inside single quotes
  rejected, unicode validated), and the directive whitelist gains a
  second one-key shape `logLevel` (integer 0..5 or
  trace/debug/info/warn/error/fatal) disclosed via the state-family
  notice wording; the existing double-quoted flowchart.htmlLabels shape
  is byte-for-byte unchanged. accTitle/accDescr scalar statements are
  consumed as validated non-visual no-ops (mirroring the class
  accessibility contract: <=256 bytes, no markup/backtick, no
  duplicates, block form and subgraph placement fail closed). Four
  legacy theme fixtures draw body only; their frontmatter `theme:` key
  keeps the whole original source, and the four new-syntax siblings
  (two-key logLevel+theme directive) stay fail-closed. The same four
  rows were already pinned in the FS-6 FontAwesome ledger, which
  migrates in place (fixture shas untouched): 34/20/17 -> 38 body draw
  (17 whole-and-body + 21 body-only scope) / 16 blocked, blocker
  vocabulary unchanged (12 unknown-icon, 4 theme-config). Whole
  originals unchanged at 967/1178/0 (Flow pending 251,
  admitted/mutated 149); flowchart body 240 drawn 166->170 / fb
  186->182 (body ALL 359->363 / 186->182); Class285 260/25/0 and the
  mandatory 48 gaps unchanged; integrity empty. Debug/release serial
  reports byte-identical (sha256
  8100368c224b754e8e9879b3a52a7b49b6969376a31e8266bd5e2ec317a9dea0).
  Independent read-only adversarial review (fresh general-purpose
  agent, task a84b30aa96dc983db, zero builds): first round
  REQUEST-CHANGES caught two cross-cohort MAJORs (the FS-6 durable gate
  would go red; the first matrix hook was FS-6-shadowed dead code that
  also threatened the 48-gap roster) — both closed by the in-place FS-6
  migration and hook removal, then re-review APPROVE. Full mermaid
  label suite 1173/1173 green; ASan contact surface 14/14 with zero
  sanitizer hits. Three-family serial timing gate written to the
  independent log `/tmp/g4probe/serial-fs-init.log` (SERIAL_RC=0,
  per-case times): #2609 227.53s, #2726 202.96s, #3520 28.25s, total
  500.05s — all inside the 300s cap.
- 2026-11-09 (FS-spacing, 0606f021): whole-source envelope spacing leaves
  — `flowchart.nodeSpacing`/`rankSpacing` only position dagre ranks in the
  upstream SVG layout, and the terminal uses its own fixed layout, so they
  are non-terminal. A non-negative pixel integer (bare or single/double
  quoted) is now an accepted indent-4 envelope leaf via two new ConfigBits
  (NodeSpacing=2048, RankSpacing=4096) and a narrow `spacing_profile`
  predicate that admits only the bare-flowchart or courier-font inert
  cores; signs, fractions, exponents, mismatched/single-sided/empty quotes,
  structured values, indent-2 placement and any visual co-key
  (look/theme/htmlLabels/...) fail closed. Four pinned visual-check
  fixtures draw the full frontmatter original at 240/restored while
  keeping ordinary source at 40 columns (22 nodespacing, 23 rankspacing,
  two 3258 subgraph spacing rows); the two nightly handDrawn lookalikes
  stay excluded behind the `look` gate and the body track is unchanged.
  All four already belong to earlier ledgers, which migrate in place with
  fixture sha pins byte-untouched: FS-6 whole originals 17 -> 19
  (body-only scope 21 -> 19; the 38/16 body partition is unchanged) and
  FS-7a whole originals 3 -> 5 (its 7827 Work-ceiling row and the
  subGraphTitleMargin self-loops row stay body-only). A new sha-pinned
  spacing ledger with a corpus re-scan drift guard and a durable pixel
  cohort gate accompany the change; the shadowed defensive matrix
  branches are retained with explicit comments and were proven inert (no
  run_case, so they cannot emit gap errors). Whole originals move
  967 -> 971 drawn / 1178 -> 1174 fallback / 0 error (Flow pending
  251 -> 247, admitted/mutated 149 -> 153); Class285 260/25/0, the
  mandatory 48 gaps, and the body track 170/182/48 are unmoved;
  integrity empty. Debug/release serial reports byte-identical (sha256
  0fe22b004a364aceb3cd28131d0cd2430cfe7bfd7317c1722f67c9f5637803ec,
  415 rows). Two independent read-only adversarial reviews (fresh
  general-purpose agents, zero builds): the cohort review (task
  aee66c1df9b510318) APPROVE after two MINORs and four NITs were
  resolved; a final cross-cohort-migration re-review (task
  a91f43e0c0b606c1d) APPROVE with zero MAJOR and zero functional MINOR —
  its three NITs were closed by correcting the stale-count comments and
  documenting the inert shadowed branches. Full mermaid label suite
  1177/1177 green; ASan contact surface 35/35 (the original 30 plus the
  migrated FS-6/FS-7a gates) with zero sanitizer hits. Three-family serial
  timing gate written to the independent log
  `/tmp/g4probe/serial-fs-spacing.log` (SERIAL_RC=0, per-case times):
  #2610 206.89s, #2727 166.36s, #3525 23.02s, total 396.42s — all inside
  the 300s cap.
- 2026-11-09 (FS-7b Slice B `a2ea36e4`; design 804bdcc6 + findings 241bb0ad):
  the `<id>@{ view: collapsed }` directive collapses a subgraph to one
  titled rectangle — descendants, nested frames and same-leaf internal
  edges are hidden; external edges keep terminating on the collapsed
  node. Five-site change in `mermaid_impl.cpp`: a trailing
  `Group.collapsed` (sizeof Group stays 80, static_assert), a fail-closed
  parser (bare/double-quoted `collapsed`, empty group stack, non-deferred
  target, sole view key), a `place_region` leaf short-circuit, a
  `flow_layout` visibility/projection layer gated entirely on
  `any_group_collapsed` (non-collapsed graphs take the original path),
  and node-box painting of the collapsed leaf. Edges joining vertices in
  two DIFFERENT collapsed groups, or any single hidden endpoint, fail
  closed pending the Slice C endpoint-remap milestone. Three body rows
  admitted: two visual-check whole-and-body plus one handDrawn body-only
  (the `look: handDrawn` frontmatter keeps that whole original source).
  The four redux collapsed rows and two Slice C rows stay out via a
  generator drift guard over all nine collapsed fixtures. Narrow_40 is
  measured per row, not assumed source. Two-round independent review
  (task acf9dfcbd2cb83b20): r1 caught a cross-leaf silent edge-drop and
  the missing parser test matrix, both fixed before r2 APPROVE. Whole
  originals move 971 -> 973 drawn / 1174 -> 1172 fallback / 0 error; Flow
  pending 247 -> 245, admitted/mutated 153 -> 155. Class285 260/25/0,
  the mandatory 48-gap roster, and body 170/182/48 are unmoved;
  debug/release reports byte-identical (sha c4e7cd7e). Full mermaid
  1183/1183; ASan contact surface (fs7b_cohort + fs7b_directive +
  fs7_subgraph + fs7a_cohort, 16 tests) zero sanitizer. Serial gate log
  `/tmp/g4probe/serial-fs7b-collapsed.log` (SERIAL_RC=0): #2610 209.39s,
  #2727 170.54s, #3531 24.10s — all inside the 300s cap.
- 2026-11-09 (FS-7c Slice C `c0510727`): collapsed-subgraph endpoint
  remap. A hidden vertex or nested-group endpoint is rewritten to the
  collapsed ancestor's Group endpoint, so an edge into a collapsed group
  terminates on its single rectangle instead of a hidden node. Same-leaf
  internal edges and remap-only self-connections are dropped (an authored
  self-loop on a visible vertex is preserved, regression-locked); edges
  spanning two different collapsed groups, and double-nested collapse,
  stay fail-closed. The tagged router routes a synthetic Edge so Group-
  kind port sealing applies while the rich-label binding keeps the
  original edge index. Six ledger rows: two description-only
  whole-and-body visual-check fixtures, plus four redux-theme
  body-only fixtures — the same remap unlocks their bodies, while their
  whole originals stay source because the flow envelope whitelist has no
  theme key and the terminal has no redux color contract (no loss of
  expressible semantics). Two-round independent review (task
  ae53da6338bbf34d2): r1 approved the two-fixture remap, r2 approved the
  redux body-only expansion and caught an over-broad self-loop drop,
  narrowed before approval. Whole originals move 973 -> 975 drawn /
  1170 fallback (the four redux rows are body-only and do not change
  whole counts); Flow pending 245 -> 243, admitted/mutated 155 -> 157.
  Class285 260/25/0, the mandatory 48-gap roster, and body 170/182/48
  are unmoved; debug/release Flow400 reports byte-identical (sha
  04c72166). Full mermaid 1188/1188; ASan contact surface (fs7c_cohort +
  fs7b_directive + fs7b_cohort + fs7_subgraph, 19 tests) zero sanitizer.
  Serial gate `/tmp/g4probe/serial-fs7c-remap.log` (SERIAL_RC=0): #2610
  220.53s, #2727 166.82s, #3536 23.20s — all inside the 300s cap.
- 2026-11-09 (FS-elk `eb3118c8`): non-terminal layout-engine envelope
  leaf. The terminal's fixed layout ignores the upstream engine, so bare
  `layout: elk.rectpacking` joins the accepted `layout: dagre` profile via a
  fresh `ElkEngine` ConfigBit and the exact predicate
  `fields == (Layout | ElkEngine)`; the `Layout` bit stays set so the
  untrusted-compound guard still closes subgraph-bearing graphs. Only one
  fixture (the rectpacking container row) admits, whole-and-body, drawing at
  every width. An initial broader whitelist (bare elk/elk.box + the
  keepEntryNodeOnTop option) was rejected after a full matrix showed it
  added a `3-layout-elk-subgraphs` source->error regression and changed
  nothing for two FS-1b Work-ceiling fixtures; bare elk/elk.box, quoted
  engines, the `elk:` option namespace and unknown engines are explicit
  fail-closed Flow6/cohort controls. Whole originals move 975 -> 976 drawn /
  1170 -> 1169 fallback / 0 error; Flow pending 243 -> 242,
  admitted/mutated 157 -> 158 (242+158=400). Class285 260/25/0, the
  mandatory 48-gap roster, and body 179/173/48 are unmoved; the full
  debug/release OfficialMatrix JSON is byte-identical (sha256
  2f41e510976905cbed7113e154f1ec58aa563e1e016c4ec7936b23619f528a85).
  Independent read-only adversarial review (fresh general-purpose agent,
  task a66265d289108d9b1, zero builds) APPROVE with zero MAJOR; its three
  MINOR/NIT items were resolved (drift-guard regex now sees quoted elk,
  namespace branch returned to the original one-liner, folded control
  restored to bare elk). Full mermaid 1191/1191; ASan contact surface
  (fselk_cohort + Flow6 envelope/folded controls, 7 tests) zero sanitizer.
  Serial gate `/tmp/g4probe/serial-fselk-layout.log` (SERIAL_RC=0): #2610
  205.83s, #2727 166.74s, #3539 22.97s — all inside the 300s cap.
- 2026-11-10 (FS-maxwidth `2f04cb74`): composite envelope profile.
  `flowchart.useMaxWidth` (responsive SVG sizing) and `layout: dagre` are each
  already accepted non-terminal leaves, but no predicate admitted their
  conjunction. The `terminal_max_width` profile now also matches the exact
  `Layout | Flowchart | MaxWidth` bit set alongside the no-layout form; exact
  equality keeps every visual co-key (look/theme/...) fail-closed and the
  Layout bit still routes subgraph graphs through the untrusted-compound
  guard. Two dagre useMaxWidth fixtures, already FS-6 body-only rows, migrate
  in place to whole-and-body (240/restored draw, 40 stays source); they
  authorize through the existing generic FS-6 Flow400 branch, so no new
  authority string or matrix branch is added. A handDrawn useMaxWidth sibling
  stays source behind its Look bit. Whole originals move 976 -> 978 drawn /
  1169 -> 1167 fallback / 0 error; Flow pending 242 -> 240,
  admitted/mutated 158 -> 160 (240+160=400). Class285 260/25/0, the
  48-gap roster, and body 179/173/48 are unmoved; the full debug/release
  OfficialMatrix JSON is byte-identical (sha256
  adefc5c4c8c9397924231d568c2feebcdca9b2ff7772ec3615d2b184dff15237). Full
  mermaid 1191/1191; ASan contact (fs6 cohort + Flow6 envelope/folded, 7
  tests) zero sanitizer. Serial gate `/tmp/g4probe/serial-fsmaxwidth.log`
  (SERIAL_RC=0): #2610 221.88s, #2727 171.30s, #3539 24.47s.
- 2026-11-11 (journey family renderer `744c3a24`): first greenfield family.
  A new `mermaid_journey_impl.cpp` renders the journey grammar (title /
  section / `label: score: actors`) as a bounded native text table; 10 of 18
  originals are theme-free and now draw whole-and-body, the other 8 carry a
  theme/look visual gate and stay source (the terminal has no color contract).
  Scores parse via non-throwing from_chars (0..5, overflow fails closed), all
  emitted strings pass unicode_text, and the visual gate is key-based so a
  "theme" word in a task label is not falsely rejected. The classifier adds
  MermaidProbeFamily::Journey on the exact header and a dispatch case; the
  matrix hook replaces the run_case observation 1:1 (observation total stays
  2738). Independent review a9c9dfef3711bc7f6 initially REQUEST-CHANGES (one
  MAJOR: throwing std::stoi; plus text-safety/theme-precision/body-track
  MINORs), all resolved before approval. Whole originals 978 -> 988 drawn /
  1167 -> 1157 fallback / 0 error; journey 10/8; flow 160/240 and body
  179/173/48, the 48-gap roster and Class285 260/25/0 are unmoved; full
  debug/release OfficialMatrix JSON byte-identical (sha256
  3ea1bfdfcfe0c9b6f3238cfc7a420689e09596b1ca33f26ad1756676828dbf0b). Full
  mermaid 1197/1197; ASan contact (6 journey tests) zero sanitizer. Serial
  gate `/tmp/g4probe/serial-journey.log` (SERIAL_RC=0): #2610 209.62s,
  #2727 169.10s, #3545 23.13s.
- 2026-11-12 (timeline family renderer `fe3eb520`): second greenfield family. A new
  `mermaid_timeline_impl.cpp` renders the timeline grammar
  (`timeline [TD]`, title, sections, `period : event [: event]` plus colon
  continuation lines) as a bounded native linear text table; 7 of 30
  originals are theme-free and now draw whole-and-body, the other 23 carry a
  theme/look visual gate and stay source (the terminal has no color contract).
  The classifier adds MermaidProbeFamily::Timeline on the exact header
  (including `timeline TD`) and a dispatch case; the matrix hook replaces the
  run_case observation 1:1 (observation total stays 2738). Independent review
  a7f26a8ecbe8991e6 APPROVE WITH CONDITIONS; all conditions resolved before
  commit: M1 continuation lines now colon-split through the shared
  append_events path (one corpus fixture's narrow-40 state flipped
  source->drawn as a measured geometry consequence, NARROW_DRAW re-pinned),
  N1 `<br>` markers matched case-insensitively via a single longest-first
  scan, M3 durable fail-closed tests for orphan continuations, unknown
  statements, all three caps, 65-cell hard words and whitespace events, B1
  real review id in the ledger. A second independent read-only zero-build
  review on the final diff returned APPROVE with zero MAJOR/MINOR; its four
  NITs are all fail-closed-safe/corpus-absent, the section-cap semantic note
  is documented in-code. Whole originals 988 -> 995 drawn /
  1157 -> 1150 fallback / 0 error; timeline 7/23; flow 160/240 and body
  372/173/48, the 48-gap roster and Class285 260/25/0 are unmoved;
  debug/release OfficialMatrix JSON byte-identical (sha256
  da43018998c11b96323ce0719bdbde9d67336e9ce20148b69e264a2ef0cfcce3).
  Full mermaid 1207/1207; ASan contact (10 timeline + 6 journey tests) zero
  sanitizer. Serial gate `/tmp/g4probe/serial-timeline.log` (SERIAL_RC=0).
- 2026-11-12 (gantt family renderer `9e8d2950`, local): third greenfield family and
  the largest single pool so far. A new `mermaid_gantt_impl.cpp` projects the
  gantt grammar (sections; done/active/crit/milestone tags; ids including the
  two-id vertical-marker form; calendar/clock/epoch points; durations in
  ms/s/m/h/d/w; after/until dependency lists; zero-span markers) to a bounded
  native structural text table. It validates every token's shape but performs
  NO calendar math, exclude resolution or dependency scheduling — an explicit
  documented non-goal, since the terminal has no time axis. 39 of 52 originals
  are theme-free with valid dates and now draw whole-and-body; 11 theme/look
  fixtures stay source (no color contract); the two deliberately malformed
  fixtures (3-digit year, 1-digit day) fail closed through strict date-shape
  validation. Non-terminal directives (dateFormat/axisFormat/tickInterval/
  excludes/includes/todayMarker/weekend/weekday) are bounded text, not
  interpreted. The classifier adds MermaidProbeFamily::Gantt on the exact
  header (including after a frontmatter fence) and a dispatch case; the matrix
  hook replaces run_case observations 1:1 for the 39 ledger paths only
  (observation total stays 2738; the 13 non-ledger gantt fixtures keep their
  default source disposition). Independent read-only zero-build review
  a5f5a7ca0477aa5e0 APPROVE with zero MAJOR; its one MINOR (zero-span
  milestone rendered as "end 0" instead of the milestone marker) and four
  NITs (id-cap ordering, after/until case folding, section-cap comment,
  clock-validation wording) were all resolved before the ledger flipped to
  approved, with regression assertions for the milestone span and
  upper-case dependency keyword. Whole originals 995 -> 1034 drawn /
  1150 -> 1111 fallback / 0 error; gantt 39/13; flow 160/240, body
  372/173/48, the 48-gap roster and Class285 260/25/0 are unmoved;
  debug/release OfficialMatrix JSON byte-identical (sha256
  f62ffcf150c5a9f0b9b1d4b9fb009a896a6bd3500e974a315dff8080d3738cf3).
  Full mermaid 1217/1217; ASan contact (10 gantt + adjacent journey/timeline
  tests) zero sanitizer. Serial gate `/tmp/g4probe/serial-gantt.log`
  (SERIAL_RC=0).
- 2026-11-12 (treemap family renderer `4deb2144`, local; D-137 first self-selected
  family): fourth greenfield family, chosen over agentflow/c4/usecase because
  it is a self-contained indentation tree language with no flowchart
  shape/subgraph debt, and its 26 originals have real source-backed coverage.
  A new `mermaid_treemap_impl.cpp` projects the quoted container/leaf
  hierarchy (2-space indentation; containers carry no value, leaves carry one
  numeric value transcribed verbatim) to a bounded native indented outline; it
  performs no rectangle geometry and no number formatting. 10 of 26 originals
  are plain (no classDef/::: style, no frontmatter config) and now draw
  whole-and-body; 10 stay source behind the classDef/::: color/stroke contract
  and 6 behind the frontmatter treemap.valueFormat config (the formatter
  changes visible leaf text); the key-based gate also keeps theme/look keys,
  while the words "theme"/"config" inside descriptions or node names are not
  false gates (three "theme"-named corpus fixtures draw). Both header
  spellings `treemap` and `treemap-beta` are accepted. The classifier adds
  MermaidProbeFamily::Treemap and a dispatch case; the matrix hook replaces
  run_case observations 1:1 for the 10 ledger paths only (observation total
  stays 2738; the 16 non-ledger treemap fixtures keep their default source
  disposition). Independent read-only zero-build review a85ecf9e80f2a47e7
  APPROVE with zero MAJOR; its two MINORs were resolved before the ledger
  flipped to approved — M1 enforces a value node as a leaf (a node nested
  deeper than an open leaf fails closed), M2 added direct depth/node cap
  tests; the five NITs are intentional sibling parity or safe over-rejection.
  Whole originals 1034 -> 1044 drawn / 1111 -> 1101 fallback / 0 error;
  treemap 10/16; flow 160/240, body 372/173/48, the 48-gap roster and
  Class285 260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  6aaeb5b7b143c426f27e63b672e1956f3e8c72d57fa2a76e6b95d17350bbb280).
  Full mermaid 1225/1225; ASan contact (8 treemap + adjacent gantt/timeline/
  journey tests) zero sanitizer. Serial gate `/tmp/g4probe/serial-treemap.log`
  (SERIAL_RC=0).
- 2026-11-12 (c4 family renderer `e70e8c34`, local; D-137 second self-selected
  family): fifth greenfield family, chosen over usecase because its
  function-call grammar is a closed vocabulary and 23 originals are
  source-backed. A new `mermaid_c4_impl.cpp` projects the C4 model
  (C4Context/Container/Component/Deployment/Dynamic headers; 28 typed element
  keywords across person/system/container/component/db/queue/boundary/
  deployment-node; seven Rel*/BiRel relation keywords; brace-delimited
  nesting) to a bounded native indented outline: elements render as
  `keyword alias - Name [| tech | desc]`, relations as
  `keyword from -> to: label (tech)`, with block depth driving indentation.
  No box/arrow geometry, sprites or links; `$attributes` are prefix-validated
  and omitted. 23 of 32 originals are structural (no Update*Style/
  UpdateLayoutConfig statement and no gated frontmatter key) and now draw
  whole-and-body; 9 stay source — eight color/stroke/layout style statements
  and one frontmatter `config: wrap` layout key. The classifier adds
  MermaidProbeFamily::C4 on the five exact headers (including after a
  frontmatter fence) and a dispatch case; the matrix hook replaces run_case
  observations 1:1 for the 23 ledger paths only (observation total stays
  2738; the nine non-ledger c4 fixtures keep their default source
  disposition). Independent read-only zero-build review a1ee3ba2e69a1f2c3
  initially REQUEST-CHANGES, then APPROVE after fixes: MINOR-1 routed the
  title through the same protected emission path with duplicate-title
  rejection (a follow-up unified the row cap over the combined title plus
  statement output); NIT-1/NIT-2 corrected the partition comment and added
  depth/statement cap tests; NIT-3 made relation extra arguments strictly
  quoted/`$` to match the element path; NIT-4 corrected comment syntax to
  `%%`; NIT-5 gate parity kept under the durable gtest. The generator drift
  guard caught a manual census error before commit (24/8 -> actual 23/9, the
  extra gate being the frontmatter wrap config). Whole originals
  1044 -> 1067 drawn / 1101 -> 1078 fallback / 0 error; c4 23/9; flow
  160/240, body 372/173/48, the 48-gap roster and Class285 260/25/0 are
  unmoved; debug/release OfficialMatrix JSON byte-identical (sha256
  75d0d7ff119feb3fcb5cda783562dc6207dc176233591d5fca194031606bba2e).
  Full mermaid 1232/1232; ASan contact (7 c4 + adjacent treemap/gantt/
  timeline/journey tests) zero sanitizer. Serial gate
  `/tmp/g4probe/serial-c4.log` (SERIAL_RC=0).
- 2026-11-12 (venn family renderer `b53736d8`, local; D-148 self-selected
  family): sixth greenfield family. A new `mermaid_venn_impl.cpp` projects
  the venn-beta grammar — `set`/`union` declarations with optional `:size`
  or `["label"]` suffixes and nested `text` nodes — to a bounded native
  indented outline, preserving every set, union, size, label and text node
  as text. No circle/overlap geometry and no color are attempted. 11 of 21
  originals are clean and now draw whole-and-body; 10 stay source: seven
  carry a frontmatter theme/look/config key and three an in-body
  `style`/classDef/`:::` color statement. The source-driven census
  corrected an initial 14-draw estimate to the actual 11 (the three body
  style assignments had to be gated, not just frontmatter keys). Grammar
  role invariants are enforced fail-closed: sets/unions sit at the base
  indent level and a text node is exactly one level under one, so a nested
  set or a deeper text node returns the whole source. The classifier adds
  MermaidProbeFamily::Venn on the exact `venn-beta` header (including after
  a frontmatter fence) and a dispatch case; the matrix hook replaces
  run_case observations 1:1 for the 11 ledger paths only (observation
  total stays 2738; the ten non-ledger venn fixtures keep their default
  source disposition). Independent read-only zero-build review
  ae64a3c38c96a9a8e APPROVE with zero MAJOR; its M2 (the title bypassed the
  guarded emission path) was fixed by routing the title through the shared
  wrap+guarded push with a combined row cap, and N1 (loose nesting) was
  tightened to the two-level role invariant with regression tests; M1
  (quote-aware suffix scan) and M3 (gate parity on hypothetical inputs)
  are fail-closed with zero corpus impact and deferred to a later
  hardening batch. Whole originals 1067 -> 1078 drawn / 1078 -> 1067
  fallback / 0 error; venn 11/10; flow 160/240, body 372/173/48, the
  48-gap roster and Class285 260/25/0 are unmoved; debug/release
  OfficialMatrix JSON byte-identical (sha256
  e9686dd1fecdf186899d5c09d2357dc9010984d3bdb6543ea923c468639ebf10).
  Full mermaid 1238/1238; ASan contact (6 venn + adjacent c4/treemap/gantt/
  timeline/journey tests) zero sanitizer. Serial gate
  `/tmp/g4probe/serial-venn.log` (SERIAL_RC=0).
- 2026-11-13 (quadrant family renderer `fbb34e13`, local; D-157
  self-selected family): seventh greenfield family. A new `mermaid_quadrant_impl.cpp`
  projects the quadrantChart grammar — one title, x-axis/y-axis labels
  (bare, `label -->`, `low --> high`), `quadrant-1..4` labels and named
  `Name: [x, y]` data points — to a bounded native outline preserving
  every axis, quadrant, point name and coordinate as text. No axis
  geometry, plotting or color is attempted. 7 of 14 originals are clean
  and now draw whole-and-body; 7 stay source: five carry an
  `%%{init}%%` visual directive (axis position, chart sizing/padding,
  themeVariables fill colors) and two carry inline per-point styles
  (radius/color/stroke-color/stroke-width, one also with classDef/`:::`).
  The source-driven live census corrected the initial 13/1 estimate to
  the actual 7/7. The header-only fixture (an official empty-frame case)
  draws as the single verbatim keyword row, through the same guarded
  push as every other row; its one-line body is surface-identical to
  plain text at every width, so the ledger pins distinct whole/body
  states rather than one shared narrow flag. The classifier adds
  MermaidProbeFamily::Quadrant on the exact `quadrantChart` header
  (including after a frontmatter fence) and a dispatch case; the matrix
  hook replaces run_case observations 1:1 for the 7 ledger paths only
  (observation total stays 2738; the seven gated fixtures keep their
  default source disposition). Independent read-only zero-build review
  ac14877d1a0f3b761 APPROVE with zero MAJOR: it independently reproduced
  the exact 7/5/2 partition with two from-scratch analyses, ported the
  parser line by line and passed 50+ adversarial probes. Its N3/N4
  (coordinate-shape/CRLF regression cases, trim_view lifetime comment)
  were applied; N1/N2 are hypothetical-input-only, fail-closed with zero
  corpus impact and deferred to a later hardening batch alongside the
  venn M1/M3 items. Whole originals 1078 -> 1085 drawn / 1067 -> 1060
  fallback / 0 error; quadrant 7/7; flow 160/240, body 372/173/48, the
  48-gap roster and Class285 260/25/0 are unmoved; debug/release
  OfficialMatrix JSON byte-identical (sha256
  7dad26c2ef78f8c2b76415dde4142445391eaf6a81c3a3e4a0c05ce223472e6f;
  release 28s, debug 242s). Full mermaid label suite 1245/1245 green
  (the 1238 venn-era count plus the seven quadrant cohort cases).
  ASan contact across the seven greenfield family cohorts (54 tests:
  quadrant/venn/c4/treemap/gantt/timeline/journey) zero sanitizer
  reports, logged at `/tmp/g4probe/quadrant/asan-contact.log`. Serial
  gate `/tmp/g4probe/quadrant/serial-quadrant.log` — the Config110
  whole-source, Class285, Block58 and ER107 admission tests run one at a
  time — 4/4 passed (SERIAL_RC=0, ~402s).
- 2026-11-13 (architecture family renderer `86288941`, local; D-161
  handoff-resumed family): eighth greenfield family. A new
  `mermaid_architecture_impl.cpp` projects the architecture-beta
  grammar — group/service/junction declarations with independently
  optional (icon)/[label] parts and `in` containment, L/R/T/B port
  edges in directed (`-->`), undirected (`--`) and labeled
  (`-[Label]-`) forms including `{group}` boundary endpoints, and
  `align row|column` directives — to a bounded native outline that
  validates every statement and transcribes the accepted DSL lines
  verbatim. No fcose layout, edge routing, icon or color is attempted.
  16 of 20 originals are clean and now draw whole-and-body; 4 stay
  source, all carrying a frontmatter `config:` layout knob
  (randomize/seed/idealEdgeLengthMultiplier) the terminal cannot
  express. There is no %%{init}%%, classDef/::: or body style anywhere
  in the family. accTitle/accDescr accessibility metadata is accepted
  but not emitted as a frame row. The classifier adds
  MermaidProbeFamily::Architecture on the exact `architecture-beta`
  header (including after a frontmatter fence) and a dispatch case; the
  matrix hook replaces run_case observations 1:1 for the 16 ledger
  paths only (observation total stays 2738; the four gated fixtures
  keep their default source disposition). Independent read-only
  zero-build adversarial review a5602134897c4359d APPROVE with zero
  MAJOR: it independently reproduced the 16/4/0 partition, enumerated 18
  statement shapes (187 statements, zero false rejects), ported the
  validators and passed 64 + 22 probes and 60000 fuzz cases with no
  fail-open. Its MINOR 1 (endpoint-order/missing-port/empty-label
  regression pins), MINOR 2 (icon-only declaration coverage) and the
  NIT 6 arrow-in-label case were applied; MINOR 3 (conservative
  classDef/::: prefix gating inherited from quadrant) only forces a safe
  source fallback and is deferred to a shared hardening batch. Whole
  originals 1085 -> 1101 drawn / 1060 -> 1044 fallback / 0 error;
  architecture 16/4; flow 160/240, body 372/173/48, the 48-gap roster
  and Class285 260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  16ae106a867d557d6fb284d018e8a1d2c57fcc283beb731dac69d4696b531b02;
  release 28s, debug 251s). Full mermaid label suite 1252/1252 green
  (the 1245 quadrant-era count plus the seven architecture cohort
  cases). ASan contact across all eight greenfield family cohorts (61
  tests: architecture/quadrant/venn/c4/treemap/gantt/timeline/journey)
  zero sanitizer reports, logged at
  `/tmp/g4probe/architecture/asan-contact.log`. Serial gate
  `/tmp/g4probe/architecture/serial-architecture.log` — the Config110
  whole-source, Class285, Block58 and ER107 admission tests run one at a
  time — 4/4 passed (SERIAL_RC=0, ~408s).
- 2026-11-13 (kanban family renderer `d42baf8b`, local; D-161 continuous
  family after architecture): ninth greenfield family. A new
  `mermaid_kanban_impl.cpp` projects the kanban grammar — flat card
  declarations `id[Label]` or a bare id, each with an optional flat
  `@{ key: value }` annotation block (priority/assigned/ticket with
  quoted or bare values) — to a bounded native outline that validates
  every card and transcribes the accepted DSL lines verbatim. No board
  or column geometry and no color is attempted. All eleven originals are
  clean (description-only frontmatter; no %%{init}%%, classDef, ::: or
  body style anywhere in the family) and now draw whole-and-body; there
  is no gated subset. The `@{...}` annotation is structural card
  metadata, not a color statement, so it is transcribed rather than
  treated as a visual gate. The classifier adds
  MermaidProbeFamily::Kanban on the exact `kanban` header (including
  after a frontmatter fence) and a dispatch case; the matrix hook
  replaces run_case observations 1:1 for the 11 ledger paths
  (observation total stays 2738). Independent read-only zero-build
  adversarial review a112dbeca598240ec APPROVE with zero MAJOR: it
  independently reproduced the 11/0/0 partition over all 50 cards,
  matched every manifest sha and the live narrow set, and probed every
  malformed card shape with no fail-open. MINOR 1 (full visual-gate
  branch tests), MINOR 2 (header-only frame and bare-CR cases) and the
  NIT 5 empty-label/empty-annotation/nested-brace boundaries were
  applied. MINOR 3 disclosed a cross-suite effect: two merman vendor
  kanban inputs (one a label-less `id@{...}` card) flip from
  Charts/source to native draw through the classifier;
  MermaidTerminal.CompleteImportedInputBaseline still passes and no test
  pins their disposition, so the flip is safe. Whole originals
  1101 -> 1112 drawn / 1044 -> 1033 fallback / 0 error; kanban 11/0;
  flow 160/240, body 372/173/48, the 48-gap roster and Class285
  260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  a7c8530eaab56878df7764b327b6931cc27030617bc14b1bab10d434594561c5;
  release 29s, debug 252s). Full mermaid label suite 1259/1259 green
  (the 1252 architecture-era count plus the seven kanban cohort cases).
  ASan contact across all nine greenfield family cohorts (68 tests:
  kanban/architecture/quadrant/venn/c4/treemap/gantt/timeline/journey)
  zero sanitizer reports, logged at
  `/tmp/g4probe/kanban/asan-contact.log`. Serial gate
  `/tmp/g4probe/kanban/serial-kanban.log` — the Config110 whole-source,
  Class285, Block58 and ER107 admission tests run one at a time — 4/4
  passed (SERIAL_RC=0, ~414s).
- 2026-11-13 (usecase family renderer `74506fba`, local; D-176): tenth
  greenfield family and the grammar-richest. A new
  `mermaid_usecase_impl.cpp` projects the usecase-beta grammar — one
  direction, actors, parenthesized/bracketed use cases and anonymous
  quoted use cases, balanced systemBoundary/end containers, notes, the
  full UML edge set (`-->`, `<--`, `--`, `--o`/`o--`, `--x`/`x--`,
  `--|>`, `..> : include|extend`, labeled and multi-dash edges, named
  edges with their own @{...} annotation), flat @{...} metadata and
  `<<stereotype>>` / literal `&lt;..&gt;` suffixes — to a bounded native
  outline. Physical line breaks inside a quoted label are joined into
  one logical statement and transcribed verbatim; no placement, edge
  routing or color is attempted. 14 of 32 originals are clean and now
  draw whole-and-body; 18 stay source: twelve carry a frontmatter
  theme/look/config key and six carry a body classDef/:::/style/init
  visual statement (eight files contain ::: but two are already
  frontmatter-gated). The classifier adds
  MermaidProbeFamily::Usecase on the exact `usecase-beta` header
  (including after a frontmatter fence) and a dispatch case; the matrix
  hook replaces run_case observations 1:1 for the 14 ledger paths
  (observation total stays 2738; the 18 gated fixtures keep their
  default source disposition). Independent read-only zero-build
  adversarial review a37954a81a81ed523 APPROVE with zero MAJOR: it
  reproduced the exact 14/12/6 partition with a from-scratch splitter,
  ported every validator and passed 60+ probes with no fail-open. MINOR
  1 (a quote in a %% comment could swallow later statements) was fixed
  by treating comment lines as inert outside an open quote, with a
  regression; MINOR 2 added the corpus-safe semicolon reject and the
  `..>` include/extend requirement (a single-connector count was
  rejected because a labeled edge has two dash runs); an unreachable
  quoted-first branch was deleted. Whole
  originals 1112 -> 1126 drawn / 1033 -> 1019 fallback / 0 error;
  usecase 14/18; flow 160/240, body 372/173/48, the 48-gap roster and
  Class285 260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  c4ae6667e736392c6b9db61e65bed60340057078d303eed6c6eb6730d9345151;
  release 30s, debug 253s). Full mermaid label suite 1266/1266 green
  (the 1259 kanban-era count plus the seven usecase cohort cases).
  ASan contact across all ten greenfield family cohorts (75 tests:
  usecase/kanban/architecture/quadrant/venn/c4/treemap/gantt/timeline/
  journey) zero sanitizer reports, logged at
  `/tmp/g4probe/usecase/asan-contact.log`. Serial gate
  `/tmp/g4probe/usecase/serial-usecase.log` — the Config110
  whole-source, Class285, Block58 and ER107 admission tests run one at a
  time — 4/4 passed (SERIAL_RC=0, ~399s).
- 2026-11-13 (tree-view family renderer `2620d828`, local; D-180):
  eleventh greenfield family. A new `mermaid_treeview_impl.cpp` projects
  the treeView-beta grammar — an indentation tree on a 2+4k space grid
  whose nodes are quoted or bare labels (paths, emoji, unicode,
  consecutive spaces, trailing-slash directories) with optional
  `icon(...)` and `## description` suffixes — to a bounded native outline
  that validates the grid and suffix shapes and transcribes each node
  line verbatim, preserving its source indentation. No icons, tree guide
  lines or color are rendered. The live source gives 8 clean / 5
  frontmatter-gated / 2 body-gated, correcting the brief's older 9/4/2
  census: one fixture closes its frontmatter with a trailing-whitespace
  fence ("---      ") and carries config/themeVariables, so it is a
  frontmatter gate once the fence is parsed correctly; the two
  body-gated fixtures use `:::highlight` class annotations. Both the C++
  fence scan and the ledger generator tolerate trailing whitespace on a
  closing fence. The classifier adds MermaidProbeFamily::Treeview on the
  exact `treeView-beta` header (including after a frontmatter fence) and a
  dispatch case; the matrix hook replaces run_case observations 1:1 for
  the 8 ledger paths (observation total stays 2738; the seven gated
  fixtures keep their default source disposition). Independent read-only
  zero-build adversarial review ac919e19482fdb6ff APPROVE with zero
  MAJOR: it independently confirmed the 8/5/2 partition (refuting the
  brief's older 9/4/2 via the trailing-whitespace fence), ported every
  validator and passed 22 boundary probes with no fail-open. MINOR 1
  (free text after a closing quoted label) was tightened to suffix-only
  and MINOR 2 added the bare-CR and node-suffix/indent/gate coverage.
  The shared-classifier exact-fence peek and the combined-header push
  are safe-direction inherited behavior left unchanged. Whole originals
  1126 -> 1134 drawn / 1019 -> 1011 fallback / 0 error; tree-view 8/7;
  flow 160/240, body 372/173/48, the 48-gap roster and Class285
  260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  63dd65802a4cbd1f14f05f2560224f1e08ce03bcc3da3549b5b822e2804c6c94;
  release 27s, debug 248s). Full mermaid label suite 1273/1273 green
  (the 1266 usecase-era count plus the seven tree-view cohort cases).
  ASan contact across all eleven greenfield family cohorts (82 tests:
  tree-view/usecase/kanban/architecture/quadrant/venn/c4/treemap/gantt/
  timeline/journey) zero sanitizer reports, logged at
  `/tmp/g4probe/treeview/asan-contact.log`. Serial gate
  `/tmp/g4probe/treeview/serial-treeview.log` — the Config110
  whole-source, Class285, Block58 and ER107 admission tests run one at a
  time — 4/4 passed (SERIAL_RC=0, ~398s).
- 2026-11-13 (ishikawa family renderer `b921654d`, local; D-187):
  twelfth greenfield family. A new `mermaid_ishikawa_impl.cpp` projects
  the ishikawa-beta grammar — a relative-indentation tree whose nodes are
  bare labels (an effect root followed by cause categories and nested
  sub-causes, character set [A-Za-z0-9 _.-]) — to a bounded native outline
  that validates each node and transcribes the line verbatim with its
  source indentation preserved. No fishbone spine, branch geometry or
  color is rendered. The live source gives 7 clean / 6 frontmatter-gated /
  0 body-gated (the brief's 13 was a candidate count, not a clean
  promise): the six gated fixtures carry frontmatter theme/look/config
  keys (themes, handDrawn look, diagram padding, useMaxWidth) the
  terminal cannot express, and there is no classDef/:::/style/init
  statement anywhere in the family. Indentation is relative rather than a
  fixed grid, so a node needs only a >=2-space indent; blank and
  whitespace-only lines are skipped. The classifier adds
  MermaidProbeFamily::Ishikawa on the exact `ishikawa-beta` header
  (including after a frontmatter fence) and a dispatch case; the matrix
  hook replaces run_case observations 1:1 for the 7 ledger paths
  (observation total stays 2738; the six gated fixtures keep their
  default source disposition). Independent read-only zero-build
  adversarial review aa67aef2934adf864 APPROVE with zero MAJOR: it
  independently reproduced the 7/6/0 partition, ported the renderer to
  a simulator and passed 32 adversarial probes with no fail-open, and
  confirmed the relative-indent design and the single guarded push. The
  four MINOR test/comment items were applied (non-ASCII/emoji fail-closed
  pins, header-only frame test, init/style gate coverage, indent-1
  boundary). Whole originals 1134 -> 1141 drawn / 1011 -> 1004
  fallback / 0 error; ishikawa 7/6; flow 160/240, body 372/173/48, the
  48-gap roster and Class285 260/25/0 are unmoved; debug/release
  OfficialMatrix JSON byte-identical (sha256
  897aebab2c5dd8e30065354f540645d2b17bcf42ea15717f27a2a7cd9466a5d7;
  release 27s, debug 243s). Full mermaid label suite 1280/1280 green
  (the 1273 tree-view-era count plus the seven ishikawa cohort cases).
  ASan contact across all twelve greenfield family cohorts (89 tests:
  ishikawa/tree-view/usecase/kanban/architecture/quadrant/venn/c4/
  treemap/gantt/timeline/journey) zero sanitizer reports, logged at
  `/tmp/g4probe/ishikawa/asan-contact.log`. Serial gate 4/4 green
  (`-j1`, 405.4s wall: EveryWholeSourceIsObserved 204.91s,
  CapabilityReport.Class285 174.77s, Block58AdmissionJoin 23.43s,
  Er107AdmissionJoin 2.29s), RC=0, logged at
  `/tmp/g4probe/ishikawa/serial-ishikawa.log`.
- 2026-11-13 (cynefin family renderer `3c73ca54`, local; D-187):
  thirteenth greenfield family. A new `mermaid_cynefin_impl.cpp` projects
  the cynefin-beta grammar — a strict two-space indented frame of the five
  domain keywords (complex, complicated, clear, chaotic, confusion),
  quoted items, title/accessibility directives and labeled domain
  transitions — to a bounded native outline that validates each statement
  and transcribes the line verbatim, preserving its source indentation.
  No boundary-curve geometry, transition arrows or color is rendered. The
  live source gives 7 clean / 5 frontmatter-gated / 0 body-gated (the
  brief's 12 was the candidate pool, not a clean promise): the five gated
  fixtures carry frontmatter config/theme/look keys (cynefin
  seed/amplitude/width/height/showDomainDescriptions, a base theme with
  per-domain themeVariables) the terminal cannot express, and there is no
  classDef/:::/style/init statement anywhere in the family. Indentation
  is a strict 0/2 grid; blank and whitespace-only lines are skipped. The
  classifier adds MermaidProbeFamily::Cynefin on the exact `cynefin-beta`
  header (including after a frontmatter fence) and a dispatch case; the
  matrix hook replaces run_case observations 1:1 for the 7 ledger paths
  (observation total stays 2738; the five gated fixtures keep their
  default source disposition). Independent read-only zero-build
  adversarial review task ad43d9261d381626d reached final APPROVE after one
  re-review: the first pass returned REQUEST CHANGES on one MAJOR (a
  frontmatter key with whitespace before the YAML mapping colon,
  `config : 42`, bypassed the key gate because the key tail was not
  trimmed — a fail-open). The fix trims the key after the colon cut in the
  cynefin renderer; three RED-then-GREEN pins cover the spacing variants
  and the re-review confirmed 21 spacing/near-lookalike probes and 300,000
  fuzzed inputs with no fail-open, plus an independent 7/5/0 reproduction.
  The same untrimmed-key pattern is inherited by twelve earlier greenfield
  renderers (ishikawa, treeview, journey, kanban, gantt, timeline,
  quadrant, venn, treemap, c4, architecture, usecase) with no live trigger;
  it is recorded as a separate cross-family hardening follow-up and left
  untouched here per the blast-radius rule. Whole
  originals 1141 -> 1148 drawn / 1004 -> 997 fallback / 0 error;
  cynefin 7/5; flow 160/240, body 372/173/48, the 48-gap roster and
  Class285 260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  9f18a4dbbe3cdbcbec01adf2edb99418ebde975aa43d74589662b562e12d48e9;
  release 27s, debug 242s). Full mermaid label suite 1287/1287 green
  (the 1280 ishikawa-era count plus the seven cynefin cohort cases).
  ASan contact across all thirteen greenfield family cohorts (96 tests:
  cynefin/ishikawa/tree-view/usecase/kanban/architecture/quadrant/venn/c4/
  treemap/gantt/timeline/journey) zero sanitizer reports, logged at
  `/tmp/g4probe/cynefin/asan-contact.log`. Serial gate 4/4 green
  (`-j1`, 397.0s wall: EveryWholeSourceIsObserved 203.49s,
  CapabilityReport.Class285 168.39s, Block58AdmissionJoin 22.93s,
  Er107AdmissionJoin 2.17s), RC=0, logged at
  `/tmp/g4probe/cynefin/serial-cynefin.log`.
- 2026-11-13 (wardley family renderer `ca676cb5`, local; D-187):
  fourteenth greenfield family and the richest grammar so far. A new
  `mermaid_wardley_impl.cpp` projects the wardley-beta map — a strict
  two-space indented frame of anchors and components positioned by
  normalized coordinates, value-chain and flow links, evolve/note/
  annotation directives, an evolution-axis rename and pipeline blocks — to
  a bounded native outline that validates each statement shape and
  transcribes the line verbatim, preserving its source indentation. No map
  canvas, coordinate geometry, link arrows or color is rendered. The live
  source gives 6 clean / 4 frontmatter-gated / 0 body-gated (the brief's 10
  was the candidate pool): the four gated fixtures are the theme-test maps
  (base/dark/forest/neutral) and no body carries a
  classDef/:::/style/init statement. Indentation is a strict 0/2 grid;
  blank lines are skipped; pipeline blocks must balance, must not nest and
  may contain only single-coordinate components. The classifier adds
  MermaidProbeFamily::Wardley on the exact `wardley-beta` header
  (including after a frontmatter fence) and a dispatch case; the matrix
  hook replaces run_case observations 1:1 for the 6 ledger paths
  (observation total stays 2738; the four gated fixtures keep their
  default source disposition). Independent read-only zero-build
  adversarial review task aef27af8b85e12728 APPROVE with zero MAJOR: it
  independently reproduced the 6/4/0 partition with two separate parsers,
  ported every validator to a simulator plus an inverse checker and ran
  167 deterministic probes, about 800,000 fuzzed/mutation inputs and a
  300,000+ visual-gate fuzz with no fail-open, and re-attacked the
  `config : 42` spacing seam (this renderer trims the key from the first
  draft). One MINOR was applied: size and the annotation grid used the
  signed-integer pair and so accepted a negative value outside the
  grammar (a leading minus is legal only for label offsets); they now use
  an unsigned pair with two regression pins. The frontmatter key scanner
  already trims before comparing, so the cynefin-era `config : 42`
  spacing bypass is closed here from the start. Whole
  originals 1148 -> 1154 drawn / 997 -> 991 fallback / 0 error;
  wardley 6/4; flow 160/240, body 372/173/48, the 48-gap roster and
  Class285 260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  cb47782c23819d3471c37ab086d22a6b078b8e54beceace46fc3965b611f3108;
  release 27s, debug 241s). Full mermaid label suite 1294/1294 green
  (the 1287 cynefin-era count plus the seven wardley cohort cases).
  ASan contact across all fourteen greenfield family cohorts (103 tests:
  wardley/cynefin/ishikawa/tree-view/usecase/kanban/architecture/quadrant/
  venn/c4/treemap/gantt/timeline/journey) zero sanitizer reports, logged at
  `/tmp/g4probe/wardley/asan-contact.log`. Serial gate 4/4 green
  (`-j1`, 397.3s wall: EveryWholeSourceIsObserved 203.95s,
  CapabilityReport.Class285 168.21s, Block58AdmissionJoin 22.97s,
  Er107AdmissionJoin 2.18s), RC=0, logged at
  `/tmp/g4probe/wardley/serial-wardley.log`.
- 2026-11-13 (railroad family renderer `b02234b3`, local; D-193):
  fifteenth greenfield family and the first multi-dialect grammar. A new
  `mermaid_railroad_impl.cpp` handles the four corpus headers —
  railroad-beta (IR nested function calls) and the railroad-abnf/ebnf/
  peg-beta textual notations — by validating each grammar rule with a
  dialect-specific recursive-descent parser and transcribing the physical
  lines verbatim, preserving continuation indentation. No track geometry,
  branch shapes or color is rendered. The live source gives 8 clean / 1
  frontmatter-gated / 0 body-gated from a candidate pool of 9 (the single
  gated ir-dark-theme carries a config theme key); the only non-ASCII byte
  in the family is an em-dash inside one frontmatter description, so all
  bodies are pure ASCII. The IR dialect parses a closed seven-function set
  and buffers a multi-line rule until its ';' terminator; the three text
  dialects share an expression parser parameterized by their operators and
  reject operator crossing. A rule starts on column 0 and an indented
  continuation is legal only inside an open IR rule. The classifier gains
  MermaidProbeFamily::Railroad on the four exact headers (including after
  a frontmatter fence) and a dispatch case; the matrix hook replaces
  run_case observations 1:1 for the 8 ledger paths (observation total
  stays 2738; the gated fixture keeps its default disposition).
  Independent read-only zero-build adversarial review task
  a1214613ac79ed6d6 APPROVE with zero MAJOR: it independently reproduced
  the 8/1/0 partition across the four dialects, ported both parsers to a
  simulator with an inverse oracle and ran about 158,000
  fuzzed/mutation/random inputs plus cap boundary checks with no
  fail-open, crash, hang, non-verbatim emission or quadratic blowup. Two
  MINORs were applied: a quoted literal now requires printable ASCII
  (C0 control bytes were previously invisible cells) with three
  regression pins, and the ABNF repetition is documented as the RFC 5234
  prefix form only (a postfix '*' stays fail-closed). Whole
  originals 1154 -> 1162 drawn / 991 -> 983 fallback / 0 error;
  railroad 8/1; flow 160/240, body 372/173/48, the 48-gap roster and
  Class285 260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  e99eef2c4d3964600f112191b4f0cb0041e217bdd26589460d6ca0068d073b82;
  release 28s, debug 242s). Full mermaid label suite 1301/1301 green
  (the 1294 wardley-era count plus the seven railroad cohort cases).
  ASan contact across all fifteen greenfield family cohorts (110 tests:
  railroad/wardley/cynefin/ishikawa/tree-view/usecase/kanban/architecture/
  quadrant/venn/c4/treemap/gantt/timeline/journey) zero sanitizer reports,
  logged at `/tmp/g4probe/railroad/asan-contact.log`. Serial gate 4/4 green
  (`-j1`, 398.4s wall: EveryWholeSourceIsObserved 204.26s,
  CapabilityReport.Class285 168.74s, Block58AdmissionJoin 23.19s,
  Er107AdmissionJoin 2.19s), RC=0, logged at
  `/tmp/g4probe/railroad/serial-railroad.log`.
- 2026-11-14 (radar family renderer `893835da`, local; D-196):
  sixteenth greenfield family. A new `mermaid_radar_impl.cpp` projects the
  radar-beta grammar — axes with optional aliases, curves as inline
  numeric arrays or braced named blocks, and scalar display directives —
  to a bounded native outline that validates each statement shape and
  transcribes the physical lines verbatim, preserving block and
  statement indentation. No polygon geometry, radial scale or color is
  rendered. The live source gives 3 clean / 3 frontmatter-gated / 0
  body-gated from a candidate pool of 6 (the three gated fixtures carry
  config/theme/themeVariables keys for radar colors and layout); the
  family's only non-ASCII bytes live in a gated frontmatter, so all clean
  bodies are pure ASCII. A header sits at column 0, a top-level statement
  at exactly two spaces, and a curve block's value lines sit deeper while
  its closing brace returns to two spaces. The classifier gains
  MermaidProbeFamily::Radar on the exact radar-beta header (including
  after a frontmatter fence) and a dispatch case; the matrix hook
  replaces run_case observations 1:1 for the 3 ledger paths (observation
  total stays 2738; the gated fixtures keep their default disposition).
  Independent read-only zero-build adversarial review task
  a78aba37d54c5d977 reached final APPROVE after one re-review: the first
  pass returned REQUEST CHANGES on one MAJOR (the value-list parser
  accepted an empty comma slot anywhere, so leading/internal/repeated
  commas drew — 4,980 distinct fail-open shapes across 26,334
  combinations) and one MINOR (an empty multi-line block was accepted
  while an inline '{}' was rejected). Both are fixed: an empty slot is
  legal only as the final piece after a real element, and a block must
  contain a value line before it can close, each with regression pins.
  Whole originals 1162 -> 1165 drawn / 983 -> 980 fallback / 0 error;
  radar 3/3; flow 160/240, body 372/173/48, the 48-gap roster and
  Class285 260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  1638604c3fe0908e3482dc1880e325bd37bc2d3b74f4a109f19f4013d0e00d47;
  release 27s, debug 243s). Full mermaid label suite 1308/1308 green
  (the 1301 railroad-era count plus the seven radar cohort cases).
  ASan contact across all sixteen greenfield family cohorts (117 tests:
  radar/railroad/wardley/cynefin/ishikawa/tree-view/usecase/kanban/
  architecture/quadrant/venn/c4/treemap/gantt/timeline/journey) zero
  sanitizer reports, logged at
  `/tmp/g4probe/radar/asan-contact.log`. Serial gate 4/4 green
  (`-j1`, 414.2s wall: EveryWholeSourceIsObserved 213.68s,
  CapabilityReport.Class285 174.20s, Block58AdmissionJoin 24.12s,
  Er107AdmissionJoin 2.23s), RC=0, logged at
  `/tmp/g4probe/radar/serial-radar.log`.
- 2026-11-14 (packet family renderer `fd6e020b`, local; D-196):
  seventeenth greenfield family. A new `mermaid_packet_impl.cpp` handles
  both the `packet` header and the `packet-beta` alias, projecting the
  bit-field grammar — a title plus single-bit and ranged fields — to a
  bounded native outline that validates each statement shape and
  transcribes the physical lines verbatim, preserving the two-space
  indentation. No bit-cell geometry or color is rendered. The live source
  gives 4 clean / 2 frontmatter-gated / 0 body-gated from a candidate pool
  of 6 (the two gated fixtures carry config.packet showBits/bitOrder/
  bitsPerRow keys); the only non-ASCII bytes are em-dashes in two
  frontmatter descriptions (one clean, one gated), so every body is pure
  ASCII. The header sits at column 0 and every statement at exactly two
  spaces. The classifier gains MermaidProbeFamily::Packet on the two exact
  headers (including after a frontmatter fence) and a dispatch case; the
  matrix hook replaces run_case observations 1:1 for the 4 ledger paths
  (observation total stays 2738; the gated fixtures keep their default
  disposition). Independent read-only zero-build adversarial review task
  a5c4e116b2dbc1841 APPROVE with zero MAJOR: it independently reproduced
  the 4/2/0 partition, ported the validators to a simulator against a
  separate spec oracle and ran 58 attack vectors, 103 boundary cases and
  900,000 fuzzed/mutation inputs with zero divergence, crash or
  non-verbatim emission. Whole originals 1165 -> 1169 drawn / 980 -> 976
  fallback / 0 error; packet 4/2; flow 160/240, body 372/173/48, the
  48-gap roster and Class285 260/25/0 are unmoved; debug/release
  OfficialMatrix JSON byte-identical (sha256
  11d813cdb9299b639aa1ef76cab193dadafba464298090a4d5147c74d7c05dac;
  release 28s, debug 246s). Full mermaid label suite 1315/1315 green
  (the 1308 radar-era count plus the seven packet cohort cases).
  ASan contact across all seventeen greenfield family cohorts (124 tests:
  packet/radar/railroad/wardley/cynefin/ishikawa/tree-view/usecase/kanban/
  architecture/quadrant/venn/c4/treemap/gantt/timeline/journey) zero
  sanitizer reports, logged at
  `/tmp/g4probe/packet/asan-contact.log`. Serial gate 4/4 green
  (`-j1`, 403.0s wall: EveryWholeSourceIsObserved 208.92s,
  CapabilityReport.Class285 168.98s, Block58AdmissionJoin 22.77s,
  Er107AdmissionJoin 2.28s), RC=0, logged at
  `/tmp/g4probe/packet/serial-packet.log`.
- 2026-11-14 (eventmodeling family renderer `7d4730bf`, local; D-196):
  eighteenth greenfield family and the first whose whole candidate pool is
  clean. A new `mermaid_eventmodeling_impl.cpp` projects the event-model
  grammar — an ordered list of `tf` time frames with inline data,
  data-block references and source relations, plus named data blocks — to a
  bounded native outline that validates each statement shape and
  transcribes the physical lines verbatim, preserving the column layout. No
  event-stream geometry or color is rendered. The live source gives 6 clean
  / 0 frontmatter-gated / 0 body-gated: the whole family draws. The only
  non-ASCII bytes are em-dashes and arrows in frontmatter descriptions;
  every body is pure ASCII. Top-level statements sit at column 0 and data
  block entries at indent two, a distinct column layout. The classifier
  gains MermaidProbeFamily::EventModeling on the exact eventmodeling
  header (including after a frontmatter fence) and a dispatch case; the
  matrix hook replaces run_case observations 1:1 for all six ledger paths
  (observation total stays 2738). Independent read-only zero-build
  adversarial review task a1616614c326e9b96 reached final APPROVE after one
  re-review: the first pass returned REQUEST CHANGES on two MAJORs. M1
  was a dotted-name validator that accepted a digit-led segment; it now
  requires an identifier-start character on every segment. M2 was an empty
  data block being drawn; a block now needs at least one indent-two entry
  before it can close. Three MINORs were tightened in the same pass (one
  inline block and one reference per frame, a single optional trailing
  comma on data entries, and no orphan data header); across 680,000+
  fuzz/mutation cases the change is tightening-only with zero new accepts.
  Whole originals 1169 -> 1175 drawn / 976 -> 970 fallback / 0 error;
  eventmodeling 6/0; flow 160/240, body 372/173/48, the 48-gap roster and
  Class285 260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  ac473df5cc4b62a3ce466575812bf11db61641c2184536604c2c68f83d99a081;
  release 27s, debug 242s). Full mermaid label suite 1321/1321 green
  (the 1315 packet-era count plus the six eventmodeling cohort cases; the
  family has no gated fixture, so its cohort carries six tests).
  ASan contact across all eighteen greenfield family cohorts (130 tests:
  eventmodeling/packet/radar/railroad/wardley/cynefin/ishikawa/tree-view/
  usecase/kanban/architecture/quadrant/venn/c4/treemap/gantt/timeline/
  journey) zero sanitizer reports, logged at
  `/tmp/g4probe/eventmodeling/asan-contact.log`. Serial gate 4/4 green
  (`-j1`, 399.5s wall: EveryWholeSourceIsObserved 205.06s,
  CapabilityReport.Class285 169.19s, Block58AdmissionJoin 23.02s,
  Er107AdmissionJoin 2.19s), RC=0, logged at
  `/tmp/g4probe/eventmodeling/serial-em.log`.
- 2026-11-14 (requirement style renderer, local; D-204): the first extension
  of an existing greenfield renderer rather than a new family TU. A
  production-seam census (classify_mermaid + layout_succeeds, not text
  matching) partitioned the 172 requirement originals into 12 already
  native, 4 reachable, and 156 genuinely frontmatter-gated (config/theme/
  title keys, an indented YAML fence, or a leading-blank theme fixture); the
  19 residual styled fixtures all carry a config/theme gate, so the hard wall
  has source evidence. The reachable four are description-only-frontmatter
  sources whose bodies use the common Mermaid style vocabulary — classDef,
  `class`, direct `style`, and the `name:::class` shorthand. The requirement
  parser now validates every property (fill/stroke/color/stroke-width/
  font-weight), a five-name corpus-bounded CSS named-color profile plus hex,
  every class and node reference, and rejects unknown/malformed shapes as a
  whole source (nothing is ignored). The styles project onto the shared
  ClassDirectStyles seam already used by Class/ER/State: the hex color parser
  and hue-role classifier were extracted into exported single-source helpers,
  and ClassStyleProperties gained an append-only `bold` field the Class
  grammar never sets, so those callers stay byte-identical. Independent
  read-only adversarial review reached APPROVE after one re-review. Its MAJOR
  found that font-weight bold was dropped at the effective-property merge and
  the paint guard skipped bold-only nodes, so the legend claimed bold while
  zero pixels were bold; both merge loops now propagate bold (normal can
  override), the guard admits bold-only boxes, and a pixel-level
  FontWeightReachesBoldPixels gate counts real terminal bold cells (336/120/
  336/0 across the four corpus fixtures). Two MINORs closed trailing-comma
  target lists and generator/envelope fidelity; a write-only verbatim field
  was removed. Whole originals 1175 -> 1179 drawn / 970 -> 966 fallback / 0
  error; requirement 16/156; flow 160/240, body 372/173/48, the 48-gap roster
  and Class285 260/25/0 are unmoved, and the observation total stays 2738.
  Debug/release OfficialMatrix JSON byte-identical (sha256
  ee11f8bbe47ac1b155911b473ce4470d55487d2d36e0e895912f406f197afb7b). Full
  mermaid label suite 1329/1329 green (the 1321 eventmodeling-era count plus
  the eight requirement cohort cases). ASan: the requirement cohort 8/8 and
  the full relations-semantics binary 256/256 (the shared style seam's
  Class/ER/State contact) zero sanitizer reports, logged under
  `/tmp/g4probe/requirement/`. Serial gate 4/4 green (`-j1`, release,
  47.14s wall: EveryWholeSourceIsObserved 13.30s, CapabilityReport.Class285
  8.79s, Block58AdmissionJoin 22.72s, Er107AdmissionJoin 2.16s), RC=0,
  logged at `/tmp/g4probe/requirement/serial-em.log`.
- 2026-11-14 (agentflow family renderer, local; D-211): nineteenth greenfield
  family and the first flowchart-syntax-variant pool recovered with its own
  grammar rather than an approximate flowchart draw. A production-seam census
  (classify_mermaid + layout_succeeds, not text grep) partitioned the 38
  agentflow-beta originals into 27 native and 11 genuinely config/theme-gated;
  the new `mermaid_agentflow_impl.cpp` parses the real grammar — `flow`/
  `global`/`connector` declarations under a strict two-space indent, node
  `id["label"]`/`id("label")` targets, `@{ }` metadata over a closed key
  vocabulary (the six v0.8 shapes, view/value/description/params/returns/
  protocol/endpoint/token_required/connectorRef/instruction/model), and the
  four edge forms `-->`, `-.-`, `--x` plus bare and pipe-quoted labels — and
  projects a bounded native outline. Every edge endpoint and metadata owner
  resolves to a declared element; an unrecognized shape/key/arrow/endpoint
  rejects the whole source, so no attribute is silently ignored and no
  invalid agentflow source is drawn as a generic flowchart. The classifier
  gains MermaidProbeFamily::Agentflow on the exclusive `agentflow-beta`
  header (including after a frontmatter fence) and a dispatch case; the
  matrix hook replaces run_case observations 1:1 for all 27 ledger paths, so
  the observation total stays 2738. Independent read-only adversarial review
  reached final APPROVE after one re-review. The first pass returned REQUEST
  CHANGES on one MAJOR, six MINORs and five NITs, all resolved. The MAJOR was
  a duplicate metadata key supplied across separate `@{ }` blocks being
  silently accepted because the seen-bitmask was block-local; an
  AfElement-persistent `meta_seen` mask now rejects in-block and cross-block
  duplicates uniformly. MINORs aligned the C++/Python frontmatter gate with
  parse_envelope, required an explicit `end` (no implicit dedent container
  close), mandated non-empty connector/flow labels, forbade edges terminating
  at a connector, made metadata comma splitting quote-aware, turned
  `%%{init}%%` into an explicit rejection, restricted `shape` to nodes, and
  expanded fail-closed coverage past 30 cases. Whole originals 1179 -> 1206
  drawn / 966 -> 939 fallback / 0 error; agentflow 27/11 (19 draw at 40
  columns, 8 fall back); flow 160/240, body 372/173/48, the 48-gap roster and
  Class285 260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  b6a1cd49b2ba23b9fb4c445154006f378763567a11bdcdfa167fc29db217fa1c;
  release 63/63). Full mermaid label suite 1334/1334 green (the 1329
  requirement-era count plus the five agentflow cohort cases). ASan cohort
  5/5 (the owning contact surface) zero sanitizer reports, logged at
  `/tmp/g4probe/agentflow/asan-contact.log`. Serial gate 4/4 green
  (`-j1`, release, 47.93s wall: EveryWholeSourceIsObserved 13.29s,
  CapabilityReport.Class285 8.86s, Block58AdmissionJoin 23.45s,
  Er107AdmissionJoin 2.18s), RC=0, logged at
  `/tmp/g4probe/agentflow/serial-em.log`.
- 2026-11-14 (sankey family renderer, local; D-211): twentieth greenfield
  family and the smallest single-family draw of the program (+1), closing a
  pool whose other 17 originals are a genuine config-contract hard wall. A
  production-seam census (render_markdown CellCapture, not text grep)
  partitioned the 18 originals into 1 native and 17 gated; the new
  `mermaid_sankey_impl.cpp` handles only the `sankey-beta` dialect — a
  header followed by strict two-space indented three-field CSV link rows
  (`source,target,value`) — validating printable free-text labels (no
  quotes, embedded commas or control bytes) and a bounded positive integer
  value (1..999,999,999, at most nine digits), capped at 512 links, and
  transcribes each accepted row verbatim as `from -> to: value` rather than
  drawing an approximate flow graph. Any malformed row, a `%%{init ...}%%`
  directive, an unclosed/indented frontmatter fence, or any column-0
  frontmatter key besides `description` rejects the whole source, so no
  attribute is silently ignored. The 17 gated fixtures are all the plain
  `sankey` dialect carrying `config.sankey` nodeWidth/labelStyle/colors/
  useMaxWidth/layout contracts that a text surface cannot honor; they keep
  their whole-source fallback. The classifier gains
  MermaidProbeFamily::Sankey on the exclusive `sankey-beta` header
  (including after a frontmatter fence) and a dispatch case; the matrix hook
  replaces run_case observations 1:1 for the single ledger path, so the
  observation total stays 2738. Independent read-only adversarial review
  reached final APPROVE after one re-review. The first pass returned
  REQUEST CHANGES on one MAJOR and one MINOR, both resolved. The MAJOR was
  an engine `%%{init ...}%%` directive silently swallowed as a plain
  comment; comment lines beginning `%%{` now reject outright while plain
  `%%` comments still pass, matching the
  agentflow/relations/packet/eventmodeling convention. The MINOR aligned
  indented/non-exact `---` fence handling with the renderer-free
  generator's malformed-fence classification. Two residual C++/Python
  divergences were reviewed as fail-safe NITs and documented in code: any
  non-exact line containing `---` is gated (stricter than
  `strip()=="---"`), and any tab anywhere rejects (the generator checks
  leading tabs only); both can only reject to fallback, never mis-draw, and
  both are unreachable on the pinned corpus. Whole originals 1206 -> 1207
  drawn / 939 -> 938 fallback / 0 error; sankey 1/17 (the one reachable row
  draws at 40 columns); flow 160/240, body 372/173/48, the 48-gap roster
  and Class285 260/25/0 are unmoved; debug/release OfficialMatrix JSON
  byte-identical (sha256
  5167f087f20c892429b89b0a7d7e4bc59a4eea96087edac8a824353976bb1d7f;
  release 63/63). Full mermaid label suite 1339/1339 green (the 1334
  agentflow-era count plus the five sankey cohort cases). ASan cohort 5/5
  (the owning contact surface) zero sanitizer reports, logged at
  `/tmp/g4probe/sankey/asan-contact.log`. Serial gate 4/4 green (`-j1`,
  release, 47.29s wall: EveryWholeSourceIsObserved 13.22s,
  CapabilityReport.Class285 8.85s, Block58AdmissionJoin 22.87s,
  Er107AdmissionJoin 2.19s), RC=0, logged at
  `/tmp/g4probe/sankey/serial-em.log`.
- 2026-11-14 (swimlanes hard wall re-confirmed, local; D-211): no renderer
  built. A fresh production-seam probe (the prior scratch evidence had been
  cleaned) measured 0 drawn / 46 fallback over the whole pool: every
  fixture is `swimlane-beta` (LR and TD, 23 each — an earlier text-only
  census claiming a second `swimlane:` dialect was wrong) and 46/46 carry
  both `config.flowchart.htmlLabels` and `useMaxWidth` visual/layout
  contracts; 18 add theme/look and 5 carry body classDef/class/style
  statements. A text renderer cannot model lane geometry without silently
  dropping those contracts, which the D-211 rules forbid, so the family
  stays gated. Unblock condition: a native lane-geometry renderer that
  honors the htmlLabels/useMaxWidth/theme contracts. Evidence:
  `/tmp/g4probe/swimlanes/hard-wall-probe.log`.
- 2026-11-14 (D223 state reachable-semantics batch one, local): first
  in-family extension of the existing shared state/relations renderer rather
  than a new greenfield family, recovering +7 whole originals (v1 +3, v2 +4).
  A production-seam census (testing::layout_succeeds == thin forwarder to
  layout_mermaid_content, plus render_markdown CellCapture; never text grep)
  partitioned the 145 state originals (41 drawn / 104 fallback at start):
  84 are genuine frontmatter visual gates (theme/look/layout/nested
  state.useMaxWidth; 35 pure-FM plus 49 double-gated with a body debt), 1 is
  a lone `info` directive that is not a state diagram, and 20 are body-
  reachable. Of those 20, this batch closes the 7 whose grammar the text
  surface can represent fully; the rest stay honestly gated (see below).
  Three state-local, family-mutually-exclusive changes, all re-entering the
  shared closed validators and adding no Work fee: (1) pseudo-state markers
  `state F &lt;&lt;fork&gt;&gt;`/join now decode by reusing the existing PURE
  class_encoded_annotation at a new State-only call site, and
  `state C [[choice]]` is accepted as an alias of `<<choice>>`; unknown
  decoded markers still fail closed, and the entity frame is byte-identical
  to the raw-marker frame; (2) block notes canonicalize only the corpus
  spelling `<br\t/>` -> `<br/>` before the shared decoder, any other tab
  still rejected; (3) a new pure `state_transition_label` decodes only the
  two corpus-attested edge-label escapes, literal `\n` -> the existing
  `<br>` break and `#colon;` -> literal colon, then re-enters
  relation_literal_label; a predecessor guard rejects nested introducers
  (`\\n`, `##colon;`, `&#colon;`). `relation_literal_label` and
  mermaid_cells_impl.cpp are untouched, so Class/ER/requirement schedules and
  glyphs are unchanged (a transient +80 Work drift was caught against
  MermaidClassStyleBaseline and removed by keeping the decoder fee-free).
  The renderer-free e2b ledger generator retired the five now-native lexical
  blockers (their durable regression guard moved into MermaidStateEnvelope
  native gtests) and added one source-derived
  state-envelope-description-tab detector mirroring the production folded-
  description unicode_text rule; the ledger moved 40/21 -> 47 predicted-draw
  /14 blocked and was re-approved by an independent adversarial agent after
  one MINOR (the nested-introducer guard) was fixed and verified with 27
  fail-closed probes. Independent review APPROVE; an overlay experiment
  proved the only OfficialMatrix redness while verdict=pending was the
  designed review gate. Gated, not silently approximated: 5 classDef/class/
  style fixtures (4 representable but a D204-sized style subsystem deferred;
  1 uses font-style:italic which FTXUI cannot represent - SemanticCellStyle
  has only color/bold/underline), 1 `click` binding pinned whole-source by
  the sha-reviewed state_bare_alias control (needs a reviewed Model
  re-baseline, so reverted to keep the tree green), 1 v1 note whose folded
  DESCRIPTION carries a real tab rejected by the shared unicode_text
  validator, 2 labels-congestion and 2 composite long-label geometry limits
  of the 240x1024 RelationLayout envelope, and 2 concurrency `--` fixtures
  (no Region shape plus a frozen CompositeDividerStaysSource test). Whole
  originals 1207 -> 1214 drawn / 938 -> 931 fallback / 0 error; state family
  41 -> 48 drawn (state-diagram 16->19, state-diagram-v2 25->29), 97
  fallback; flow 160/240, body 372/173/48, Class285 260/25/0, the 48-gap
  roster (zero state rows) and the 2738 observation total are unmoved;
  integrity_errors empty. debug/release OfficialMatrix JSON byte-identical
  (sha256
  f71f9da04345884b5a49cf760fe53fb46ebcccbe900b3b66b99830c344d33feb).
  Full mermaid label suite 1345/1345 green (270.33s wall). ASan+UBSan
  contact (e2b cohort 6/6 plus the 13 MermaidStateEnvelope tests) zero
  sanitizer reports. Serial gates `-j1` release RC=0:
  EveryWholeSourceIsObserved, CapabilityReport.Class285 (in
  test_mermaid_relations_semantics, one non-vacuous test), Block58 30/30,
  Er107 4/4. Evidence under `/tmp/g4probe/state/`.

## D228 state classDef/class/style group closure (2026-11-14)

  Four of the five state style originals that D223 left gated now draw
  natively through a state-owned classDef/class/style parser and resolver;
  the fifth stays whole-source because its classDef sets
  `font-style:italic`, which the terminal `SemanticCellStyle` cannot
  represent (only color/bold/underline; FTXUI has no italic cell attribute).
  State nodes are Rounded (including nested composite children), so the
  state path resolves its own entities instead of the Box/top-level Class
  seam; colors route through `source_style_role` palette roles (no hardcoded
  RGB), the `border:<width> solid <color>` shorthand maps to stroke, and
  `font-weight:bold` reaches the pixels. The e2b ledger moved 47/14 -> 51
  predicted-draw / 10 blocked and was re-approved by an independent
  adversarial agent (104 reviewer assertions plus a 27-assertion dual-seam
  probe).

  Two independent fail-closed root causes were fixed during review. (1)
  Inline endpoint class syntax `Node:::class` on a state edge previously let
  `identifier()` stop at the first ':' so `::class` became a literal edge
  label and the dropped style mis-drew a must-stay-fallback source; a
  State-only guard now rejects a `::`-prefixed remainder before the label
  branch. (2) A state-local named `red` in the direct color table made the
  SHA-pinned StateScoped `unknown_style` closed profile (`style A
  fill:red`) draw; the named table is now split by context so direct
  fill/stroke/color admits only the corpus-attested black/blue (+ shared
  hex/white) and `red` is admitted only inside the border shorthand, its
  sole reachable spelling (`border:1px solid red`). A same-probe HEAD vs
  working-tree census over all 145 state originals proved the change set is
  exactly the four authorized fixtures with zero regressions. A legend TAB
  is now projected as the two-character `\t` (mirroring Class), and
  malformed target lists (trailing/empty comma components) reject.

  Whole originals 1214 -> 1218 drawn / 931 -> 927 fallback / 0 error; state
  family 48 -> 52 drawn (state-diagram 19 unchanged, state-diagram-v2
  29->33), 93 fallback; flow 160/240, body 372/173/48, Class285 260/25/0,
  the 48-gap roster (zero state rows) and the 2738 observation total are
  unmoved; integrity_errors empty. debug/release OfficialMatrix 63/63 each,
  JSON byte-identical (sha256
  48ffcbc5fbbe23415e1ab9bef43c003d8f4e61456222fca261c7f99e3a0dfc6c).
  Full mermaid label suite 1007/1007 green (374.49s wall). ASan+UBSan
  contact (e2b cohort 6/6 plus 17 state tests: MermaidStateStyles,
  MermaidStateEnvelope, MermaidStateScopedDirection, State85) zero
  sanitizer reports. Serial gates `-j1` release RC=0:
  EveryWholeSourceIsObserved, CapabilityReport.Class285, Block58 9/9,
  Er107 4/4. Evidence under `/tmp/d228_review/`.

## D235 gitGraph empty quoted-tag group closure (2026-11-14)

  A same-semantic-family group of three config-free gitGraph originals whose
  only blocker was an explicitly empty quoted tag (`cherry-pick id:"X"
  tag: ""`). The empty string carries zero glyphs, so the change is lossless:
  a new file-local `Cursor::take_tag` accepts the empty tag without pushing a
  label or consuming a tag slot, and it projects identically to an omitted tag
  (no `[]` bracket; the commit node and all parent edges are unchanged). The
  non-empty tag path is behaviorally identical to the prior `quoted()` path —
  same `git_label` validation, backslash rejection and per-commit 8-tag cap —
  proven by the SHA-pinned MermaidGitGraph30 models and the 60/60 gitgraph
  binary. All three tag call sites (commit/merge/cherry-pick) share the one
  helper; malformed quotes, missing values and unknown fields still reject the
  whole statement.

  The E2a ledger retired the `gitgraph-empty-quoted-tag` detector (fixed
  blockers five -> four, vocabulary seven -> six) and moved the three rows to
  predicted-draw, 56/13 -> 59/10, re-approved by an independent adversarial
  agent. A same-probe HEAD-vs-working census over all 135 git originals shows
  the transition set is exactly these three fixtures with an empty regression
  set; the git census absolute count matches the OfficialMatrix basis (no
  capture-geometry offset for this family). Reachable-but-not-lossless
  candidates were deliberately excluded: nine-branch canvas capacity (hard
  wall), the single multiline quoted branch label (cross-line parser), the
  unprojected gitGraph title (dropping it loses information), and next-line-
  brace composite states (routing risk).

  Whole originals 1218 -> 1221 drawn / 927 -> 924 fallback / 0 error; git
  family 55 -> 58 drawn (77 fallback); flow 160/240, body 372/173/48,
  Class285 260/25/0, the 48-gap roster and the 2738 observation total are
  unmoved; integrity_errors empty. debug/release OfficialMatrix 63/63 each,
  JSON byte-identical (sha256
  5bcc065116a3e69a64325ca786aa0eaaccd2ba04d7290b743371779f1ca7f5f7).
  Full mermaid label suite 1357/1357 green (507.50s wall; the complete gtest
  discovery denominator — an earlier 1007/1075 count under-counted because
  libssl discovery failures silently un-registered binaries, see efficiency
  rule 27). ASan+UBSan contact (gitgraph 60/60 plus e2a 5/5) zero sanitizer
  reports. Serial gates `-j1` release RC=0: EveryWholeSourceIsObserved,
  CapabilityReport.Class285, Block58 9/9, Er107 4/4. Evidence under
  `/tmp/d235_probe/`.
