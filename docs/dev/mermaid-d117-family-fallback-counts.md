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

Goal: 2145 originals, 0 ordinary fallback, 0 unexpected error at 240
wide; `ctest` green. The frozen 48 body gaps need a final-milestone
disposition (fix or documented infeasibility).

## Remaining fallback by family (whole original, 240 wide)

| Family | Remaining fallback | Drawn | Notes |
|---|---:|---:|---|
| flowchart | 240 | 160 | active track; see breakdown below |
| requirement | 160 | 12 | mostly unbuilt renderer surface |
| git | 80 | 55 | |
| state-diagram-v2 | 78 | 25 | |
| gantt | 13 | 39 | renderer added 2026-11-12; 11 themed rows stay source (no color contract), 2 malformed-date fixtures fail closed |
| swimlanes | 46 | 0 | renderer not started |
| xychart | 46 | 2 | partial axes/art only |
| agentflow | 38 | 0 | flowchart-syntax variant |
| c4 | 9 | 23 | renderer added 2026-11-12; 8 Update*Style/UpdateLayoutConfig + 1 frontmatter config rows stay source (no color/layout contract) |
| usecase | 32 | 0 | |
| er-diagram | 31 | 13 | |
| timeline | 23 | 7 | renderer added 2026-11-11; 23 themed rows stay source (no color contract) |
| class-diagram | 25 | 260 | |
| treemap | 16 | 10 | renderer added 2026-11-12; 10 classDef/::: style + 6 frontmatter config rows stay source (no color/number-format contract) |
| architecture | 20 | 0 | |
| sankey | 18 | 0 | |
| journey | 10 | 18 | renderer added 2026-11-11; 8 themed rows stay source (no color contract) |
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
| **Total** | **1078** | **1067** | |

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
