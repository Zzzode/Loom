---
rfc: 48
title: state concurrent regions and in-body init/accessibility
status: accepted
owners: "@Zzzode"
reviewers:
  - agent:design-review-evidence
  - agent:design-review-geometry
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

This RFC delivers the first two classes: **19 rows**. The seven routing rows are **deferred to
a follow-up RFC**: their blocker tags (`state-composite-brace-on-next-line`,
`state-native-routing-debt`) do not establish a layout root cause, and the first review round
showed the proposed retry mechanism cannot be justified without symbolic evidence (see
"Deferred work"). The `info` body is out of scope (it needs a different renderer).

Target: state v1 **12/30 → 7/35**, state v2 **49/54 → 35/68**; global **608/1537 → 589/1556**;
formal `drawn_at_240` **1351 → 1370**. Three-number report: ①589 ②289 ③19.

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
- Composite containers already draw: `EntityShape::Group` entities are measured recursively
  (`mermaid_relations_core_impl.cpp:6710-6711`, `:7533-7539`), emitted as square titled boxes
  (`:7821-7831`, `:7880-7887`); the state-0038 ledger's 28 drawn rows include 14 with
  `state X {` composite bodies.
- `--` has no grammar: the statement loop falls through at `:2863-2884`; `identifier` rejects
  `-` (`:83-90`); the edge table keeps `A -- B` invalid (`:3001`).
- The state envelope rejects every in-body init (`:1861`, `:1981`) and the body split rejects a
  remaining `%%{` line (`:2202`). `accTitle:`/`accDescr:` reach the generic edge parse and fail
  the source (`:3001`/`:3009`) — they never become states.
- State layout is one linear `ordered` row per scope (`:7517-7532`); labels/ports for State are
  constrained by a free-region prefilter (`:8389-8391`, `:8496-8498`, `:8082-8084`).

## Goals

- **G1 (P1, 11 rows).** Parse and render concurrent regions. Inside a state `Group` body, a
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
   A per-owner region cursor array (owner slot ≤ 128) and `kStateRegionLimit = 8` bound the
   parse. The side table's allocation and growth are prepaid in both parse (per inserted
   entity) and layout (per partition), per the project Work discipline.
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
5. **Fail closed before partitioning.** For every State scope, iterate its edges and require
   both endpoints' regions to be equal; a cross-region edge returns the whole source to
   fallback. Independently, make the `reach` bounds guard unconditional (`:6579-6583`) so a
   future partition bug cannot index out of range.
6. **Layout.** After the per-scope child list is built (`:6535`) and **before**
   reach/adjacency (`:6556-6590`), partition a State owner's children by region. Extract the
   whole per-region body — ordering (`:6591-6829`), placement (`:7517-7532`), width/height
   checks (`:7516`, `:7351-7356`) and `boxes[owner].w/h` assignment (`:7533-7535`) — into a
   helper operating on one child list. Each region's height is its **complete** scope height,
   which already includes the single header band (`top` at `:6953`, full-height formula
   `:7352`); the container height is `max(region heights)` and **never adds a header again**.
   Container width = Σ region widths + one divider column per boundary, checked against
   `kWidthLimit`; total height checked against `kHeightLimit` before committing. A title wider
   than the summed regions keeps the regions **left-aligned** (divider offsets unchanged, slack
   on the right) and the widened header width participates in the `kWidthLimit` check.
7. **Dividers.** Store divider positions as **container-local offsets**; the emitter draws them
   at `boxes[owner].x + offset`, starting below the header band (`b.y + label_rows + 1`,
   `:9144-9145`) through the bottom border, with `┬`/`┴` at the border rows. Mark those cells
   blocked **before notes** (`:9153`) and routing (`:9186`) so an overlapping note fails closed
   rather than being overwritten, and exclude divider columns from `ports_for` (`:7966-7973`).
   Do not rely on the compound-header blocking loop (`:9133-9152`), which runs only when
   `compound_scope` is set (it is not for same-owner edges).
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
   A frontmatter `theme`/`look` leaf plus a keyed directive gates (mutual exclusion, RFC 0047).
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
  E2b (`:10916`) and state-0038 (`:10939`) branches — **3** of the 19 overlap E2a/E2b
  (`should-align-dividers-correctly` in E2a; v1/v2 `should-render-concurrency-states` in E2b),
  **16 are out of cohort**, and all 19 sit in state-0038's `excluded` (whose branch matches only
  its 28 `rows` and cannot shadow this cohort). A later branch would still be shadowed by the
  E2a/E2b branches (RFC 0046 r1 defect). Reach is proven per row.
- **Conflicting negatives that must flip with paired new negatives**
  (`tests/ui/test_mermaid_relations_semantics.cpp`): `CompositeDividerStaysSource` (`:33456`)
  and `InitThemeFrontmatterStaysSource` (`:33340`); `ConfigFrontmatterStaysSource` (`:33328`)
  stays negative.
- **E2a**: only the `state_blocker` `--` detection branch
  (`tools/mermaid/generate_e2a_cohort.py:186`) and the one `should-align-dividers-correctly`
  JSON row change; the sha-pinned sets (`STATE_COMPOSITE_EXECUTION_DEBT`, `:118`) stay
  **frozen** because they pin the deferred brace rows. Counts:
  `tests/ui/test_mermaid_e2a_cohort.cpp:148-153` 59/10 → 60/9. **E2b**: only the
  `state_blocker` `--` branch (`tools/mermaid/generate_e2b_cohort.py:190`) and the two
  concurrency JSON rows; `NATIVE_ROUTING_DEBT` (`:101`) stays frozen (deferred routing rows).
  Counts: `tests/ui/test_mermaid_e2b_cohort.cpp:151-155` 51/10 → 53/8, `:311`
  `checked == 39` → 35. The new `state-region-0048-cohort.json` **and the 16 out-of-cohort
  paths** (8 config-bearing region rows + 8 init-bearing theme rows) must be wired into **both**
  whole-source scans (`test_mermaid_e2a_cohort.cpp:223-244`/`:304-320`,
  `test_mermaid_e2b_cohort.cpp:207-213`) as a skip/admitted set, or those rows fail
  `same_source_surface` after flipping.
- **0046 ledger/test**: `tools/mermaid/generate_layouttuning_0046_cohort.py:51-60`
  (`FAMILY_COUNTS`), `:442-454`; `tests/ui/test_mermaid_layouttuning_0046_cohort.cpp:234` (`counts.drawn` 105 → 124),
  `:236` (remaining 62 → 43 after P1+P2), `:263-270` (state drawn 28 → 33, 52 → 66),
  `:587` (`? 12 : 49` → `? 7 : 35`), `:600` (remaining 44 → 25).
- **state-0038 needs no migration**: its `excluded` list is the generator's render-defined
  remainder (108 rows) plus 4 `layout: elk`, 4 namespaced `config.state.*` and 1
  description-tab row — 117 in total, including all 19 — and it has no `themed_excluded`; its
  28 drawn rows are the byte-identity oracle, not a bucket to move. Its test iterates `rows`
  only, so a drawn `excluded` row is not asserted as fallback.
- Roster `--emit` 608 → 589 (categories may shrink); `--self-test` and `--matrix` green.
- Docs: `docs/dev/mermaid-unsuccessful-cases.md`, `docs/dev/mermaid-failure-inventory.json`,
  `tests/fixtures/mermaid_native/direct_theme_source_audit.v1.json` carry the enumerated rows
  and must be refreshed as snapshots.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | concurrent regions | 11 rows (v1 1, v2 10; two carry short region-local labeled edges) | proposed | red-first; state v1 12 → 11, v2 49 → 39; global 608 → 597; the v1/v2 `…concurrency-states` rows (region-local labeled edges) draw; all 28 state-0038 rows byte-identical; dual-preset matrix identical; roster/0046/E2a/E2b migrated |
| P2 | init + accessibility | 8 rows (needs P1) | proposed | state v1 11 → 7, v2 39 → 35; global 589; footer literals and probe artifact pinned; init negatives (nested map, out-of-table theme, duplicate, multi-line, non-leading) |

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

To be completed at the `implementable` gate. Must record: the `accTitle` no-op fidelity
trade-off, the E2a/E2b frozen-set rationale, and the State-only side table's
`sizeof(RelationEntity)` static assertion evidence.

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

- [x] `docs/dev/d421-next-family-state-composite.md` — second revision (deferred routing)
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

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | acc9efea |
| 2026-11-22 | — | r1 revisions: routing deferred, scope 19 rows, footprint and design fixes | (this commit) |
