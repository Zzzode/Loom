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

Goal: 2145 originals, 0 ordinary fallback, 0 unexpected error at 240
wide; `ctest` green. The frozen 48 body gaps need a final-milestone
disposition (fix or documented infeasibility).

## Remaining fallback by family (whole original, 240 wide)

| Family | Remaining fallback | Drawn | Notes |
|---|---:|---:|---|
| flowchart | 247 | 153 | active track; see breakdown below |
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
| **Total** | **1174** | **971** | |

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
  redirect, and nested `direction` rows.
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
