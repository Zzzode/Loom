---
rfc: 48
title: state concurrent regions and in-body init/accessibility
status: implemented
owners: "@Zzzode"
reviewers:
  - agent:design-review-evidence
  - agent:design-review-geometry
  - agent:prr-review
  - agent:amendment-review
  - agent:code-review-p1p2
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0048 — state diagram concurrent regions and in-body init/accessibility

## Summary

D-421 measured the post-RFC-0047 census: the inert-config well is nearly exhausted and the
largest non-frozen coherent body pool is 27 state-family fallbacks. Composite "titled box"
containers **already draw** (`EntityShape::Group` recursive layout); the pool splits into
concurrent regions (11), leading `%%{init}%%` + `accTitle:`/`accDescr:` (8 — their bodies also
contain regions), labeled composite/fan-out routing (7), and a non-state `info` body (1).

This RFC delivers the first two classes: **25 rows** (17 at P1, 8 at P2). The seven routing
rows are **deferred to a follow-up RFC**: their blocker tags
(`state-composite-brace-on-next-line`, `state-native-routing-debt`) do not establish a layout
root cause, and the first review round showed the proposed retry mechanism cannot be justified
without symbolic evidence (see "Deferred work"). The `info` body is out of scope (it needs a
different renderer).

Target: state v1 **12/30 → 7/35**, state v2 **49/54 → 29/74**; global **608/1537 → 583/1562**;
formal `drawn_at_240` **1351 → 1376**. Three-number report: ①583 ②289 ③25.

> **Amendment 1 (2026-11-22, implementation measurement).** P1 measures **17** rows, not 11:
> six further originals (`concurrency-composite-containers-handdrawn-look-redux-{color,dark-color}-theme`,
> `concurrent-states-neo-look-{neo,neo-dark,redux,redux-dark}-theme`) carry the same standalone
> divider and are unlocked by the same mechanism. The roster had attributed them to
> `look_handdrawn`/`look_neo`, but their `look` leaf is already an admitted inert disclosure
> (RFC 0038); the divider was the real blocker. Keeping them fallback would require a
> bespoke look+region gate with no principled basis, and booking them as unreviewed draws
> would change the protected E6c gap roster. The scope therefore grows to 25 rows and every
> derived number above is restated. The amendment is recorded in the Review History and was
> re-reviewed; see also D-421 revision 3.

## Motivation

### Measured selection (D-421, 2026-11-22)

A two-variant strip probe over all 608 fallback rows showed no row is gated by a non-config
frontmatter key alone, and the all-inert-leaf tail is 9 rows — 7 inside the frozen ER107
whole-null accounting, 1 ishikawa row, 1 non-family root. The state pool is the largest
coherent body capability outside the excluded surfaces (flowchart exact-bit 222, swimlanes 46,
ER107).

### Evidence

- 27 rows = roster `family_residual:state-diagram` (7) + `family_residual:state-diagram-v2`
  (20); classes are disjoint and complete: P1 11 (v1 1, v2 10), P2 8 (v1 4, v2 4), routing 7
  (v1 2, v2 5), `info` 1 (v2). All 8 P2 bodies contain `--` regions, so P2 depends on P1.
- **Amendment 1 measurement.** A corpus-wide `--` scan finds 29 state originals with a
  standalone divider = 19 of the pool above (11 P1 + 8 P2; the seven routing rows and the
  `info` row carry none) + 6 look-leaf rows the roster filed under
  `look_handdrawn`/`look_neo` + 4 `classdef_styling` user-styled rows. Removing the directive-carrying rows
  (8, phase P2) and the four `user-styled-with-regions` rows (state styling, deferred) leaves
  exactly **17** P1 originals. The six carry `look: neo`/`look: handDrawn` and
  `theme: neo`/`neo-dark`/`redux`/`redux-dark`/`redux-color`/`redux-dark-color`, all inside the
  RFC 0038 admission tables, so neither leaf changes a frame byte.
- Composite containers already draw: `EntityShape::Group` entities are measured recursively
  (`mermaid_relations_core_impl.cpp:6710-6711`, `:7533-7539`), emitted as square titled boxes
  (`:7821-7831`, `:7880-7887`); the state-0038 ledger's 28 drawn rows include 14 with
  `state X {` composite bodies.
- `--` has no grammar: the statement loop falls through at `:2863-2884`; `identifier` rejects
  `-` (`:83-90`); the edge table keeps `A -- B` invalid (`:3001`).
- The state envelope rejects every in-body init (`:1861`, `:1981`) and the body split rejects a
  remaining `%%{` line (`:2202`). `accTitle: text` / `accDescr: text` reach the State
  **description** path (`:2885-2897`) and create a state named `accTitle`/`accDescr`, so the
  fix must consume them before that branch (the paired negative targets the description path).
- State layout is one linear `ordered` row per scope (`:7517-7532`); labels/ports for State are
  constrained by a free-region prefilter (`:8389-8391`, `:8496-8498`, `:8082-8084`).

## Goals

- **G1 (P1, 17 rows).** Parse and render concurrent regions. Inside a state `Group` body, a
  standalone `--` line advances the owner's region cursor. Regions are laid out side by side at
  a common top, separated by a one-column divider inside the container border; regions nest
  within the existing group depth cap. Cross-region edges fail closed **before** any layout
  partition (no partition may create an edge whose endpoints sit in different lists).
- **G2 (P2, 8 rows).** Consume the **leading** in-body `%%{init: {...}}%%` block through the
  shared envelope split (`effective_body`), admitting only `logLevel` ∈ {0…5, trace, debug,
  info, warn, error, fatal} and `theme` ∈ `{dark, default, forest, neutral}` (**the journey init
  table, not the state frontmatter table** — the frontmatter tables and their negative tests do
  not change). Consume scalar `accTitle:` and `accDescr:` as **validated no-ops** (never
  rendered), so they cannot be parsed as states and the config-free-twin oracle stays exact.
  Block-form `accDescr { … }` stays gated.
- **G3.** Every admitted row is frame-exact against a reviewed ledger (config-free twin +
  literal footer); the 28 already-drawn state-0038 rows stay byte-identical at all three
  widths; the frontmatter gate and its negative tests are unchanged.
- **G4.** No assertion is deleted or relaxed: the two state negatives in
  `test_mermaid_relations_semantics.cpp` flip to positive with paired new negatives, and the
  E2a/E2b ledgers, 0046 counts, roster and matrix migrate in the same phase.

## Non-Goals

- The 7 labeled composite/fan-out rows and the `info` row (deferred / out of scope).
- Extending the frontmatter look/theme tables (their negatives stay).
- Switching State to the ranked planner; edges crossing a region divider (fail closed).
- Any change to `RelationEntity`'s layout or size (the region model is a State-only side
  table), to ER107, flowchart exact-bit, swimlanes, or the three standing debts.

## Proposal

### P1 — concurrent regions

1. **Model (no shared-struct change).** A State-only side table
   `std::vector<int> state_region_by_entity` (indexed by entity slot, sized with the entity
   list) lives beside the diagram's state data. `RelationEntity` keeps its layout, so the
   shared `sizeof(RelationEntity)` Work charges for Class/ER/Requirement are untouched
   (verified by a `static_assert` on the existing size and by the dual-preset family gates).
   A per-owner region cursor array indexed by `owner + 1` (**`64 + kGroupLimit + 1` slots**,
   since an owner can be entity 127) and `kStateRegionLimit = 8` bound the parse.
   The side table's allocation and growth are prepaid in both parse (both vectors at
   engagement, plus one charge per State insertion through `state_entity`/`state_control`)
   and layout (per partition), and every new charge is **gated to `family == State`** (preferably
   only when a region is actually used), so the byte-identical ER/Class charge sequences
   (`:2100-2112`, `:2150`) do not shift.
2. **Assignment only at insertion.** Region is written when `entity()` *inserts* a new State,
   at exactly these seven creation sites: declaration `:2808`, description-implicit `:2895`,
   bare declaration `:2916`, Start `:2919`, edge source `:2922`, Final `:3028`, edge target
   `:3031`. An existing entity keeps its first region (the region argument is consulted only on
   the insert path). Notes (`:2455-2523`) and classDef/class/style targets
   (`mermaid_state_impl.cpp:447-451`) never create entities and never set a region.
3. **Controls are region-scoped.** `control()` (`:366-386`) dedupes by owner+shape today, which
   would collapse three `[*]` starts inside three regions into one. The dedupe key gains the
   region, so each region has its own `[*]` pair.
4. **Parse `--`.** A trimmed `--` line, State family, non-empty scope stack, current owner is a
   `Group`, increments the owner's region cursor. Top-level `--` and `A -- B` stay invalid. An
   empty region (no state between two dividers, or after the last divider) and an empty
   composite gate explicitly, with a per-`--` prepaid Work charge.
5. **Fail closed before partitioning.** For every State scope, map each edge endpoint to its
   **direct child of that scope** (`scope_child`, `:6210-6220`) and require the two mapped
   children's regions to be equal; a cross-region edge returns the whole source to fallback.
   The mapped-child form is required because a nested `compound_scope` edge's endpoints carry
   regions relative to their own owners. Independently, make the `reach` bounds guard unconditional (`:6579-6583`) so a
   future partition bug cannot index out of range.
6. **Layout.** After the per-scope child list is built (`:6535`) and **before**
   reach/adjacency (`:6556-6590`), partition a State owner's children by region. Extract the
   whole per-region body — the margin/top/header block (`:6830-6995`, including the State
   horizontal-note margin `:6919-6943`), ordering (`:6591-6829`), placement (`:7517-7532`) and
   the width/height checks (`:7516`, `:7351-7356`) — into a helper operating on **one region's
   child and edge subsets** and returning that region's `(w, h)`. `boxes[owner].w/h`
   (`:7533-7535`) stays with the container, which composes the final size (the helper must
   never write the owner's box). Each region's height is its **complete** scope height,
   which already includes the single header band (`top` at `:6953`, full-height formula
   `:7352`); the container height is `max(region heights)` and **never adds a header again**.
   Container width = Σ region widths + one divider column per boundary, checked against
   `kWidthLimit`; total height checked against `kHeightLimit` before committing. A title wider
   than the summed regions keeps the regions **left-aligned** (divider offsets unchanged, slack
   on the right) and the widened header width participates in the `kWidthLimit` check.
7. **Dividers.** Store divider positions as **container-local offsets**; the emitter draws them
   at `boxes[owner].x + offset`, starting at the layout's first body row (`top`, `:6953`)
   through the bottom border, with `┬`/`┴` at the border rows. Mark those cells
   blocked **before notes** (`:9153`) and routing (`:9186`) so an overlapping note fails closed
   rather than being overwritten, and exclude divider columns from `ports_for` (`:7966-7973`).
   Do not rely on the compound-header blocking loop (`:9133-9152`), which runs only when
   `compound_scope` is set (it is not for same-owner edges). Divider cells are ordinary
   `blocked` canvas cells, which `RouteSearch::free` (`:7688-7689`) and the port search
   (`:7943`) already honor.
8. **Region-local direction.** A `direction` statement inside a region rotates the whole owner
   today (`state_scope_directions`, `:2382-2400`). The corpus has no region-local direction; the
   gate rejects a `direction` inside a container that already has a region cursor > 0.

### P2 — init and accessibility

1. **Envelope.** Replace the blanket init rejection (`:1861`, `:1981`) with the shared split
   (`mermaid_envelope_impl.cpp:232-278`): `parsed->init` must be empty or exactly one **leading**
   directive whose payload is a flat object over `logLevel`/`theme` with the closed values
   above. The envelope's returned body becomes `effective_body`, while the existing `:2202`
   non-leading `%%{` rejection, the frontmatter pass-through and the frontmatter tables stay
   untouched (a non-leading directive is still rejected).
2. **State-local closed grammar.** The directive scanner is a state-local copy (the journey
   helper is not importable): quote state machine, no nested objects, one occurrence per key,
   CRLF strip, trailing-text and multi-line gates. It reuses the RFC 0047 single-source probe
   pattern: `tools/mermaid/state-init-0048-probes.json` drives both this gate and its Python
   mirror, with `probes_sha256` bound into the ledger review.
3. **Disclosure.** The state footer gains `config.logLevel=<v> -> logger only; not applied`
   and `config.theme=<v> -> colour palette; not applied`, after any frontmatter lines, with the
   admitted scalar printed verbatim (`trace`…`fatal` as strings, not reformatted to int).
   A frontmatter leaf from the **whole state table** (`theme`, `look`, `logLevel`,
   `fontFamily`, `securityLevel`, `useMaxWidth`) plus a keyed directive gates (mutual
   exclusion), so no config line can be emitted twice.
4. **Accessibility.** Before the description/edge paths (`:2885`, `:3001`), consume scalar
   `accTitle: <text>` and `accDescr: <text>` as validated no-ops: non-empty, bounded, unicode
   text, one occurrence each; duplicates, empty payloads and block-form `accDescr {` gate. They
   never set `diagram_title`, so the frontmatter `title` behaviour and the twin oracle are
   unchanged.

### Gates and ledger footprint

- New reviewed ledger `tools/mermaid/state-region-0048-cohort.json` + renderer-free generator
  with `--check` (state-0038 style): per-row `prior_status`, sha, literal
  `expected_footer_lines` and per-width states; the per-row oracle is config-free twin + footer,
  compared line by line.
- Matrix branch `RFC-0048 state region cohort` must be placed **before** the E2a (`:10892`),
  E2b (`:10916`) and state-0038 (`:10939`) branches — **3** of the 25 overlap E2a/E2b
  (`should-align-dividers-correctly` in E2a; v1/v2 `should-render-concurrency-states` in E2b),
  the other **22 are out of cohort**, and all 25 sit in state-0038's `excluded` (whose branch
  matches only its 28 `rows` and cannot shadow this cohort). A later branch would still be shadowed by the
  E2a/E2b branches (RFC 0046 r1 defect). Reach is proven per row.
- **Conflicting negatives that must flip with paired new negatives**
  (`tests/ui/test_mermaid_relations_semantics.cpp`): `CompositeDividerStaysSource` (`:33456`)
  and `InitThemeFrontmatterStaysSource` (`:33340`); `ConfigFrontmatterStaysSource` (`:33328`)
  stays negative.
- **E2a**: only the `state_blocker` `--` detection branch
  (`tools/mermaid/generate_e2a_cohort.py:186`) and the one `should-align-dividers-correctly`
  JSON row change; the sha-pinned sets (`STATE_COMPOSITE_EXECUTION_DEBT`, `:118`) stay
  **frozen** because they pin the deferred brace rows. Counts:
  `tests/ui/test_mermaid_e2a_cohort.cpp:148-153` 59/10 → 60/9; its whole-source scan
  `:304-320` shrinks by the promoted cohort rows, which enter the skip set through the ledger
  and through `:259-262` (the E2b `predicted-draw`) set. Measured: **24** config/init-bearing
  cohort rows leave the scan (16 P1 config-bearing + 8 P2 init-bearing; 22 of the 25 are
  out-of-cohort and 3 are E2a/E2b members). Therefore `EXPECT_GT(checked, 100)` (`:320`) is **re-pinned to `EXPECT_EQ(checked, 80)`
  = 104 − 24**. The equation is the stronger form *over the post-migration population*; the
  population itself shrinks because 24 rows were admitted, so the literal value moves below
  the old bound — a migration, not a relaxation. **E2b**: only the
  `state_blocker` `--` branch (`tools/mermaid/generate_e2b_cohort.py:190`) and the two
  concurrency JSON rows; `NATIVE_ROUTING_DEBT` (`:101`) stays frozen (deferred routing rows).
  Counts: `tests/ui/test_mermaid_e2b_cohort.cpp:151-155` 51/10 → 53/8; its whole-source scan
  `NonWhitelistedConfigRowsKeepWholeSource` (`:241-318`) loses the 16 config-bearing cohort
  rows, of which **2** (the two `should-render-concurrency-states`) were already E2b ledger
  members and therefore already skipped, so the measured net is 14 and `checked == 39`
  (`:311`) → **25**, with `skipped_by_rfc0048 == 16`. The new `state-region-0048-cohort.json` **and the 22
  out-of-cohort paths** (of the 24 scanned config/init-bearing rows, the 2
  `should-render-concurrency-states` were already E2b members) must be wired into **both**
  whole-source scans (`test_mermaid_e2a_cohort.cpp:223-244`/`:304-320`,
  `test_mermaid_e2b_cohort.cpp:241-318`) as a skip/admitted set, or those rows fail
  `same_source_surface` after flipping. The cardinality differs per scan: the E2a scan visits
  both config- and init-bearing rows and absorbs all **24** (16 at P1 + the 8 init-bearing P2
  rows), while the E2b scan visits only `declares_config` rows and absorbs the **16**
  config-bearing ones (the 8 init rows never enter it). A `skipped_by_rfc0048` equality pin
  must therefore be **over the paths the scan actually visits — measured 24 for E2a and 16 for
  E2b — never over the full 25-row `state-region-0048-cohort.json`**: `should-align-dividers-correctly` declares neither
  `config:` nor an init, so it is not a whole-source-scan candidate and a full-ledger pin
  would demand 25 in both scans and fail.
- **0046 ledger/test: measured unchanged.** Its counts are derived from its own tuning-row
  ledger, and none of the 25 rows carries a tuning namespace; the suite stays 5/5 green with
  no edit. The pre-amendment footprint list (105 → 124 etc.) was a projection, not a measured
  dependency, and is withdrawn.
- **state-0038 needs no migration**: its `excluded` list is the generator's render-defined
  remainder (108 rows) plus 4 `layout: elk`, 4 namespaced `config.state.*` and 1
  description-tab row — 117 in total, including all 25 — and it has no `themed_excluded`; its
  28 drawn rows are the byte-identity oracle, not a bucket to move. Its test iterates `rows`
  only, so a drawn `excluded` row is not asserted as fallback. **One measured migration:** its
  `E2bFootersAreUnchanged` count follows the E2b ledger's drawn set, so the two promoted
  concurrency rows move that pin 51 → 53.
- Roster `--emit` 608 → 583 (measured: `family_residual:state-diagram` 7 → 2,
  `family_residual:state-diagram-v2` 20 → 6, `look_handdrawn` 45 → 43, `look_neo` 32 → 28,
  40 categories); `--self-test` and `--matrix` green.
- Snapshot docs: **disclosed, not refreshed.** `docs/dev/mermaid-unsuccessful-cases.md` and
  `docs/dev/mermaid-failure-inventory.json` are checkpoint snapshots last regenerated at
  RFC 0034 (drawn 663 / fallback 1482, "Checkpoint 2026-10-30") and were not refreshed by
  RFC 0035-0047 either; no test consumes them, and the live credit lives in the roster and
  the matrix artifact. Refreshing them for 583 rows is a separate documentation task and is
  recorded as an open item rather than silently skipped. `direct_theme_source_audit.v1.json`
  likewise self-declares as an unconsumed historical census with zero drawing credit.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | concurrent regions | **17 rows** (v1 1, v2 16; measured) | **done** | state v1 12 → 11, v2 49 → 33; global 608 → 591; drawn_at_240 1351 → 1368; all 28 state-0038 rows byte-identical; E2a/E2b migrated; roster 591 |
| P2 | init + accessibility | 8 rows (needs P1) | **done** | state v1 11 → 7, v2 33 → 29; global 583; drawn_at_240 1376; footer literals and probe artifact pinned; 16 init/accessibility negatives (nested map, out-of-table theme, duplicate, backslash, non-leading, G5, accDescr block, duplicate/empty/bare accTitle) |

### Labeled-transition boundary in P1

P1 includes two rows with **short region-local labeled transitions** (`v1/v2
should-render-concurrency-states`), the same geometry class as the four already-drawn redux
rows; the deferred seven are the *fan-out/composite* labeled cases. These two are a named
red-first risk: if they turn out to need the label-clearance mechanism, P1's count drops to 9
and they join the deferred RFC — the phase gate reports the measured number rather than
assuming.

## Deferred work (explicit)

Seven rows — the three `should-render-edge-labels-correctly*` (blocker tag
`state-composite-brace-on-next-line`, whose own detail says it "does not assert a lexical
rejection or establish a layout root cause"), the two `…composite-states` and the two
`…simple-state-diagrams-with-labels` — and the `info` body stay on fallback. A follow-up RFC
must first establish a symbolic root cause per row (ports/label anchors vs gap capacity vs
fan-out multiplicity), then propose a mechanism; the r1 proposal (State label clearance +
free-region retry) is withdrawn because it was not proven to be the blocker and doubles the
routing Work budget.

## Production Readiness Review

| Checklist row | Answer |
|---|---|
| **Correctness & tests** | Per-phase cohort gate (ledger pins + per-row twin frame oracle + 25-row single-source probe artifact), the state-0038 gate over all 28 drawn rows **byte-identical at all three widths**, the two flipped relations-semantics negatives with paired new negatives, and the E2a/E2b whole-source scans with the 25 ledger paths wired into their skip sets (measured 24 visited in E2a, 16 in E2b). Functional timeouts carry no assertion semantics; `MermaidOfficialConfig110Native…` was calibrated to 600 s in the preceding slice (commit `fef4aff0`). |
| **Build system — units** | The parser, layout and emitter live in `mermaid_relations_core_impl.cpp`; state helpers in `mermaid_state_impl.cpp` (implementation units, no BMI of their own). The region side table is a **member of `RelationDiagram`** and `entity()`/`control()` gain a region argument, so the `loom.ui.visual.mermaid:detail` partition BMI changes and the mermaid units that `import :detail` recompile — in-target fan-out with **no cross-area fan-out**. No new imports, no SCC growth, no concurrency caps. Debug and release `-Werror` both required. |
| **Build system — shared structs** | `RelationEntity`/`Relationship` keep their layout (the side table is a separate `std::vector<int>` member of `RelationDiagram`, indexed by entity slot); `sizeof(RelationEntity)` charges are untouched and a `static_assert` pins the measured value together with the dual-preset Class/ER/Requirement family gates. `RelationDiagram`'s only Work charge is `alignof`-based (`mermaid_relations_core_impl.cpp:2112`) and stays 8. |
| **Wire / persisted data** | N/A: no wire format, JSONL trace, settings, config path or CLI change. |
| **Rollback** | Two atomic commits, revertible at hunk level; `mermaid_relations_core_impl.cpp` changes in **both** phases (P1 parser/layout/emitter; P2 envelope `:1861/:1981/:2029` and accessibility `:2885`). P1: relations-core, `mermaid_detail.cppm` (side table + caps + signatures), the new state-region ledger + generator + gate, `test_mermaid_relations_semantics.cpp` (`CompositeDividerStaysSource` flip + paired negatives), E2a/E2b generators/cohorts/tests, the 0046 generator/test constants, `fallback_roster.json`, the matrix branch, CMake registration, docs. P2: relations-core only (envelope/accessibility/footer and the state-local init scanner live in `mermaid_relations_core_impl.cpp`; `mermaid_state_impl.cpp` is untouched), the state-init probe artifact, the ledger's directive rows and footers, the `InitThemeFrontmatterStaysSource` flip + paired negatives, E2a/E2b skip-set extension and roster/matrix re-pin. No 0046 file is edited (measured unchanged). |
| **Protected pins** | None of the 10 protected files is touched. `tests/ui/test_mermaid_official.cpp` changes additively (loader + branch before the E2a/E2b/state-0038 branches + reach assertion); the 25 rows carry no manifest `diagnostic`, so each replaces one `run_case` observation 1:1 and 2738/48/107/206/285 stay unchanged. |
| **Observability** | The footer lines are the user-visible disclosure; no new diagnostics; session traces untouched. |
| **Work meter / budgets** | Region limit 8 per owner, entity total 128, per-`--` and per-region prepaid charges, width/height checked at container composition, and the side table's growth prepaid in parse and layout. Fixture-scale bodies (≤ 64 states, ≤ 128 edges) stay far under `kWorkLimit = 1,000,000`. |
| **Documentation** | design-decisions entry (side table, dividers, init/accessibility, the accTitle no-op trade-off), README, d117, d421 revision 3, d422 closure, three snapshot docs, efficiency retro. |
| **Deletion / deprecation** | No dead code or shape replacement; the deferred routing rows keep their blocker tags unchanged. |
| **Fidelity trade-offs recorded** | (a) `accTitle` is a validated no-op, matching upstream's non-inked accessibility name and keeping the twin oracle exact; (b) the E2a `STATE_COMPOSITE_EXECUTION_DEBT` and E2b `NATIVE_ROUTING_DEBT` sha-pinned sets stay frozen because they pin deferred rows. |

## Rollout and rollback

One atomic commit per phase; reverting a phase restores its ledger counts. No persisted data,
wire, CLI or settings change.

## Drawbacks and risks

- A missed insertion site silently places a state in region 0; the per-row frame gate and the
  E2a/E2b ledgers are the guards, plus a debug `static_assert`-style invariant check in the
  gate.
- Divider cells must be blocked before notes/routing, and dividers must start below the header
  band.
- The state init scanner is a fourth local directive grammar; the single-source probe artifact
  is the rule-37 guard.
- The E2a/E2b whole-source scans fail if any flipped row is not migrated in the same commit.
- `accTitle:` becoming a validated no-op is a deliberate fidelity trade-off: upstream treats it
  as an accessibility name (not rendered ink), and the config-free-twin oracle stays exact only
  if the frame is unchanged. The PRR records it explicitly so it is not read as a rendering
  defect.

## Alternatives considered

1. Do nothing — leaves the state geometry debt.
2. Append `int region` to `RelationEntity` — rejected: changes the shared `sizeof` and ripples
   the Work charges of Class/ER/Requirement, including frozen rows.
3. Render regions as nested full boxes — doubles borders and changes the corpus look.
4. Bundle the routing rows into this RFC — rejected on the r1 evidence: no proven root cause,
   and the retry mechanism doubles the routing Work budget.

## Testing and verification plan

- Per phase: the new gate (ledger pins + frame oracle + batteries), the relations resource
  tests, the two flipped (and paired) relations-semantics negatives, the state-0038 gate, the
  E2a/E2b gates, `ctest -L mermaid`, and the dual-preset matrix at phase closure; the full
  debug ctest logged to a durable path with its exit code (TL 2026-11-22 process).
- Roster `--emit` + `--self-test` + `--matrix` after each phase.

## Documentation impact

- [x] `docs/dev/d421-next-family-state-composite.md` — revision 3 (measured P1 17 rows)
- [ ] `docs/dev/d422-state-region-rfc0048-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — region side table, dividers, init/accessibility
- [ ] `docs/README.md` — RFC index row
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — state rows
- [ ] `docs/dev/mermaid-unsuccessful-cases.md`, `docs/dev/mermaid-failure-inventory.json`,
      `docs/dev/mermaid-e2a-cohort.md`,
      `tests/fixtures/mermaid_native/direct_theme_source_audit.v1.json` — snapshot refresh
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: region model | design review | resolved: State-only side table (no shared struct change) |
| OQ-2: divider glyphs/blocking | design review | resolved: local offsets, `│`+junctions, block before notes/routing, header band start |
| OQ-3: accTitle semantics | design review | resolved: validated no-op |
| OQ-4: init grammar locus | design review | resolved: state-local copy + single-source probe artifact |
| OQ-5: routing rows | design review | resolved: deferred with a written evidence gate |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review (two lenses).
- `accepted` → `implementable`: independent PRR.
- Each phase: one atomic commit with an independent code review.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | r1 | agent:design-review-geometry | REQUEST-CHANGES | B1 cross-region edges reach UB → fail closed before partitioning + unconditional bounds; B2 `control()` dedupe missing region → key gains region; M1 insertion-only assignment (7 sites enumerated); M2 shared `sizeof` ripple → State-only side table; M3 helper extraction range corrected; M4 header double count → region height is complete, header once; M5 divider coordinate space → local offsets; M6 header band/compound interaction → explicit start + blocking; M7 accTitle title → validated no-op; M8 init theme table → journey table; M9–M11 routing mechanism unproven and Work-doubling → routing deferred; m1–m9/n1–n2 folded |
| 2026-11-22 | r2 | agent:design-review-geometry | APPROVE | All r1 dispositions verified against code; arithmetic and ledger constants re-derived and self-consistent. Folded: header formula (region height already includes the header; never add one), the exact seven insertion sites, the 3-overlap/16-out-of-cohort matrix wording, state-0038's real excluded composition, the 16 out-of-cohort paths wired into both whole-source scans, the title-wider-than-regions policy, the labeled-transition boundary with the two P1 rows as a named red-first risk, and the side-table Work prepay. |
| 2026-11-22 | r2 | agent:design-review-evidence | APPROVE | All r1 findings verified resolved; 19-row range and every target number self-consistent. Folded: D-421 stale block, 3-overlap wording, E2a/E2b frozen-set precision with counts, 0046 `counts.drawn` 105→124, the e2a cohort snapshot doc, and the accTitle no-op trade-off recorded for the PRR. |
| 2026-11-22 | r1 | agent:design-review-evidence | REQUEST-CHANGES | BLOCKER initializer theme table → explicit `{dark,default,forest,neutral}`; arithmetic → v2 30/73 (and, with routing deferred, v1 7/35, v2 35/68, global 589/1556, formal 1370); the two `test_mermaid_relations_semantics.cpp` negatives added to the flip set; E2a/E2b generators/tests/constants enumerated; matrix branch ordered before the state branches; state-0038 no-migration fact; 0046 constants enumerated; composite-row count corrected (14 of 28); snapshot docs added |
| 2026-11-22 | r1 | agent:prr-review | REQUEST-CHANGES | B1 `EXPECT_EQ(checked, 88)` wrong — the E2a whole-source scan also absorbs the 2 E2b concurrency rows through `:259-262`, so the re-pin is **86** (104 − 16 − 2); m1 stale E2b scan citation; m2 PRR rows over-stated the build-system and shared-struct footprint; minor missing per-scan cardinalities; NIT wording |
| 2026-11-22 | r2 | agent:prr-review | REQUEST-CHANGES | Only the `86` value remained wrong (still `88` in the re-pin text) plus residuals; all other r1 items verified resolved (E2b 31, 0046 constants, 19-row totals, rollback phases) |
| 2026-11-22 | r3 | agent:prr-review | APPROVE | Faithful replay of `ConfigAndInitRowsKeepWholeSource` reproduces `checked = 104` and post-P1 `86`; no other count depended on the old 88; E2b 31 and 51/10→53/8 re-confirmed; 16/8 per-scan cardinalities correct. NIT N1 folded: the `skipped_by_rfc0048` pin is over the enumerated 16/8 out-of-cohort paths, not the full 19-row ledger. Gate `accepted → implementable` passed. |
| 2026-11-22 | r4 | agent:amendment-review | REQUEST-CHANGES | Independent adversarial review of Amendment 1. Confirmed: the six look-leaf rows' only blocker is the divider (both `look` and `theme` leaves are inside the RFC 0038 tables), the arithmetic reproduces from the artifacts, the P1/P2 split is 17/8, the E6c alternatives are sound, protected pins are untouched and no assertion is weakened. Found (all folded): the gate section still carried the pre-amendment 86/23/19-row/16-out-of-cohort numbers (measured 80 / 25 / 25 / 22); the "recorded in Review History and re-reviewed" claim had no row yet; the 29-row derivation was arithmetically wrong; phase/status wording disagreed with the open closure record; the PRR row cited d421 revision 2; the `counts` names needed a phase-P1 annotation; only the `look` leaf was named where both leaves are admitted. |
| 2026-11-22 | r5 | agent:amendment-review | REQUEST-CHANGES | Residual folds verified against the files: 80/24, 25/16 and the 29-row restatement were correct. Remaining (all folded): the E2a bullet still opened with the stale 16-out-of-cohort + 2-E2b decomposition; the r4 row was separated from the table by a blank line; the PRR documentation row still said d421 revision 2; the closure-pending caveat was absent from the phase table; the ledger `counts.note` was not yet present when read. |
| 2026-11-22 | r6 | agent:amendment-review | APPROVE | Final re-review: the 22-out-of-cohort wiring sentence is in place and consistent with the 24-scanned/16-config split; arithmetic re-derived from the artifacts (25 − 3 = 22; 16 + 8 = 24; 39 − 14 = 25; 104 − 24 = 80); ledger counts note and phase totals verified. Pre-amendment 86/88/19-row strings remain only inside historical Review History rows and are correct as history. Amendment 1 approved. |
| 2026-11-22 | c1 | agent:code-review-p1p2 | REQUEST-CHANGES | B1 the region cursor array was sized `64 + kGroupLimit` but indexed `owner + 1` (an owner can be 127 → OOB read/write); m1 the two side-table vectors' parse growth was under-prepaid (now: both vectors at engagement plus a per-insertion charge on the seven State sites through `state_entity`/`state_control`); m2 the Python mirror trimmed spaces only where C++ trims spaces and tabs; m3 a quoted-empty `accTitle: ""` was admitted (now gated). All fixed; the reviewer's own area checks found the region model, `--` grammar, fail-closed/partitioning, divider emission, P2 envelope, gates, ledgers, generators and scope discipline sound. |
| 2026-11-22 | c2 | agent:code-review-p1p2 | APPROVE | Re-review: B1 (129-slot cursor array, both access paths in range), m1 (both vectors at engagement + per-insertion prepay on the seven sites, ER/Class/Requirement unchanged), m2 (mirror `lstrip(" \t")`), m3 (quoted-empty accessibility gates) and M1 (RFC restated to 80/24 and 25/16) all verified against the code; generator `--check`s reproduce 60/9, 53/8, 25 (P1 17 + P2 8). One comment-arithmetic NIT fixed. |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | acc9efea |
| 2026-11-22 | — | r1 revisions: routing deferred, scope 19 rows, footprint and design fixes | (this commit) |
| 2026-11-22 | — | Gate `accepted` → `implementable`: PRR r3 APPROVE (E2a re-pin 86, out-of-cohort pin clause). No code yet | (this commit) |
| 2026-11-22 | P1 | Concurrent regions: lazy State-only side tables, `--` grammar, region-scoped controls, fail-closed cross-region check, `measure_regions` + per-region `measure_scope`, divider emission/blocking, 17-row cohort ledger + gate, matrix branch + reach proof, E2a 60/9 (`checked` 80, `skipped_by_rfc0048` 24), E2b 53/8 (`checked` 25, 16), state-0038 footer pin 53, `CompositeDividerStaysSource` flip + 6 negatives, roster 591 | (this commit) |
| 2026-11-22 | P2 | Leading `%%{init}%%` + accessibility: state-local closed directive grammar, `effective_body`, G5, directive footers, `accTitle`/`accDescr` validated no-ops, single-source 25-case probe artifact + Python mirror, ledger P2 8 rows, `InitThemeFrontmatterStaysSource` flip + 3 negatives, E2a `checked` 80, roster 583, formal 1376, dual-preset matrix `e1fe4938…`, `ctest -L mermaid` 1514/1514, full debug ctest 4699/4699 (`CTEST_EXIT=0`) | (this commit) |
