---
rfc: 48
title: state concurrent regions, init/accessibility, and labeled composite routing
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0048 — state diagram concurrent regions, in-body init/accessibility, and labeled composite routing

## Summary

D-421 measured the post-RFC-0047 census: the inert-config well is nearly exhausted, and the
largest non-frozen coherent body pool is 27 state-family fallbacks (v1 7, v2 20). Rendering
composite "titled box" containers **already works** (`EntityShape::Group` recursive layout);
the 27 rows are blocked by three distinct capabilities plus one out-of-scope body:

| class | rows | blocker today |
|---|---|---|
| concurrent regions (`--` dividers inside a container) | 11 | no `--` parse branch; no region model; no divider emitter |
| leading in-body `%%{init}%%` + `accTitle:`/`accDescr:` | 8 (bodies also have regions) | the state envelope rejects any `parsed->init`; `accTitle` would parse as a state |
| labeled composite/fan-out edge routing | 7 | State has no label clearance and a free-region prefilter that rejects fan-out |
| `info` directive body | 1 | not a state diagram (explicitly out of scope) |

Target after the three phases: state v1 **12/30 → 5/37**, state v2 **49/54 → 29/74**; global
**608/1537 → 582/1563**; formal `drawn_at_240` **1351 → 1377**.

## Motivation

### Measured selection (D-421, 2026-11-22)

A two-variant strip probe over all 608 fallback rows showed no row is gated by a non-config
frontmatter key alone, and the all-inert-leaf tail is 9 rows — 7 of them inside the frozen
ER107 whole-null accounting and 1 ishikawa row. The state pool below is the largest coherent
body capability outside the excluded surfaces (flowchart exact-bit 222, swimlanes 46, ER107).

### Evidence (renderer survey)

- Composite containers already draw: `EntityShape::Group` entities are measured recursively
  (`mermaid_relations_core_impl.cpp:6710-6711`, `:7533-7539`) and emitted as square titled
  boxes (`:7821-7831`, `:7880-7887`); the state-0038 ledger pins 28 such rows as drawn.
- `--` has no grammar: the statement loop falls through at `:2869-2884`, `identifier` rejects
  `-` (`:83-90`), and the edge operator table keeps `A -- B` invalid for State (`:3001`).
- The state envelope rejects every in-body init (`:1861`, `:1981`) and the body split rejects
  any remaining `%%{` line (`:2202`); `accTitle:`/`accDescr:` would fall into the description
  statement (`:2885`) and become states.
- State layout is one linear `ordered` row per scope (`:7517-7532`) with no rank planner
  (gated to ER/Class at `:6998`); labeled-edge clearance exists only for ER (`:6647-6667`) and
  Class (`:6668-6690`), and the State prefilter requires both ports and both label anchors in
  one free region (`:8389-8391`, `:8496-8498`, `:8082-8084`).

## Goals

- **G1 (P1).** Parse and render concurrent regions: inside a state `Group`, a standalone `--`
  line starts a new region. Each region is laid out as its own state group; regions are placed
  side by side at a common top, separated by a one-column divider drawn inside the container
  box. Regions nest arbitrarily within the existing group depth cap.
- **G2 (P2).** Consume the leading in-body `%%{init: {...}}%%` block for the state family
  through the shared envelope split, admitting only the closed subset `logLevel`
  ∈ {0…5, trace, debug, info, warn, error, fatal} and `theme` ∈ the family's existing closed
  table, disclosed in the footer; also consume scalar `accTitle:` (sets the diagram title) and
  `accDescr:` (validated, not rendered) instead of parsing them as states. Block-form
  `accDescr { … }` stays gated (class precedent).
- **G3 (P3).** Route labeled edges for composite and fan-out bodies: add State label clearance
  to the existing gap/margin computation (mirroring the ER/Class branches) and make the
  free-region prefilter a retry fallback rather than a hard requirement.
- **G4.** Every admitted row must be frame-exact against its reviewed ledger (config-free twin
  + literal footer) and must keep the 28 already-drawn state-0038 rows byte-identical.
- **G5.** No assertion is deleted or relaxed: the E2a/E2b blocker ledgers, the 0046 ledger
  remaining counts, the roster and the matrix all migrate in place with paired positives and
  negatives.

## Non-Goals

- `v2-should-render-a-simple-info` (a non-state `info` body; needs a different renderer).
- Switching the State family to the ranked/layered planner.
- Edges that cross a region divider (the corpus has none; the layout fails closed if one would
  be required).
- Any change to the frontmatter look/theme tables, the journey-family directive helper, ER107,
  flowchart exact-bit, or the three standing debts.

## Proposal

### P1 — concurrent regions

1. **Model.** Append `int region = 0` to `RelationEntity` (`mermaid_detail.cppm:290-299`). Every
   state/control creation site assigns the owner's current region; the region cursor is a
   per-owner counter under a new `kStateRegionLimit = 8`.
2. **Parse.** A trimmed line `--` inside a `state X { ... }` body (State family, non-empty
   scope stack, current owner is a `Group`) increments the owner's region cursor; anything
   else keeps the current fall-through. `A -- B` stays invalid. An empty region or an empty
   composite gates.
3. **Layout.** In `measure`, partition a State owner's children by `region` before
   reach/adjacency (`:6564-6590`); factor the existing ordering + box-measure body
   (`:6591-6829`) into a per-region helper; place region frames left to right at a common top.
   A container's width = Σ region widths + one divider column per boundary; height = max region
   height + the existing title/header rows (`:6948-6953`). Divider x positions are stored on the
   layout; `translate` shifts descendants as today (`:7543-7556`).
4. **Emit.** Draw `│` (and `┴`/`┬` junctions at the top/bottom border) at each stored x inside
   the container, and mark those cells blocked before `route_scope` (`:9186`). `draw_box` and
   `ports_for` are untouched (region bodies share the container's border).
5. **Caps.** Enforce region count, per-region child count, the summed width (`kWidthLimit`) and
   the summed height (`kHeightLimit`) before committing, with a per-`--` prepaid Work charge.

### P2 — init and accessibility

1. **Envelope.** Replace the blanket init rejection (`:1861`, `:1981`) with the shared split
   (`mermaid_envelope_impl.cpp:232-278`): `parsed->init` must be empty or a single directive
   whose payload is a flat object over `logLevel`/`theme` with closed values (the RFC 0047
   grammar, re-expressed in this TU; the journey helper is not importable). Anything else keeps
   the whole source. The effective body starts at the diagram header, so the `%%{` body split
   (`:2202`) no longer sees the admitted block.
2. **Disclosure.** The existing state footer gains `config.logLevel=<v> -> logger only; not
   applied` and `config.theme=<v> -> colour palette; not applied` in canonical order, with the
   frontmatter leaves first (frontmatter leaf + keyed directive stays mutually exclusive, as
   in RFC 0047).
3. **Accessibility.** Before the description statement (`:2885`), consume scalar
   `accTitle: <text>` (sets `diagram_title`, which the emitter already prints at `:9206-9216`
   with its own width guard) and `accDescr: <text>` (validated no-op). Duplicates, empty
   payloads, non-scalar forms and block-form `accDescr {` gate.

### P3 — labeled composite routing

1. Add State to the label-clearance branches (`:6647-6690`) so a labeled edge reserves label
   rows/columns in the gap computation.
2. Keep the free-region prefilter as a fast path; on failure, retry routing without it (still
   bounded by the A* budget and the final width/height caps), then fail closed.
3. Do not enable the ranked planner for State (`:6998` unchanged).

### Gates and ledger footprint

- New reviewed ledger `tools/mermaid/state-composite-0048-cohort.json` + renderer-free
  generator with `--check`, in the state-0038 style: per-row sha, `prior_status`, expected
  footer, and per-width states; the per-row frame oracle is config-free twin + footer.
- Matrix branch `RFC-0048 state composite cohort` placed with the other cohort branches and
  proven reached per row.
- Migrations in the same phase: E2a/E2b blocker ledgers and skip sets (the 26 paths move from
  `state-composite-divider`/`state-initializer-gated`/`state-native-routing-debt` to admitted),
  `layouttuning-0046` remaining counts, roster `--emit`, and the state-0038 ledger's
  `themed_excluded` bookkeeping where the init rows leave it.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | concurrent regions | 11 rows | proposed | red-first; state v2 49 → 39, v1 12 → 11; global 608 → 597; frame-exact ledger; no change to the 28 drawn rows |
| P2 | init + accessibility | 8 rows (needs P1) | proposed | state v1 11 → 7, v2 39 → 35; footer literals pinned; init negatives (nested map, out-of-table theme, duplicate, multi-line) |
| P3 | labeled composite routing | 7 rows | proposed | state v1 7 → 5, v2 35 → 29; global 582; every previously drawn width-240 row byte-identical |

## Production Readiness Review

To be completed at the `implementable` gate.

## Rollout and rollback

Each phase is one atomic commit; reverting a phase restores its ledger counts. No persisted
data, wire, CLI or settings change.

## Drawbacks and risks

- A new region model touches every state creation site; a missed site silently places a state
  in region 0. The per-row frame gate and the E2a/E2b ledgers are the guards.
- Divider cells must be blocked before routing, or edges cross them invisibly.
- The init grammar becomes a fourth local copy (journey-family, flow, class, state); the
  single-source probe artifact pattern (RFC 0047) should be reused for the state copy.
- P3 changes routing heuristics and is the likeliest source of byte-differences in
  already-drawn rows; it is a separate phase for that reason.

## Alternatives considered

1. Do nothing — leaves 26 rows on fallback and keeps the state families' geometry debt.
2. Reuse `CompoundScopeRoute` only (no regions) — cannot express side-by-side regions.
3. Render regions as nested full boxes — doubles borders, changes the corpus look, and the
   terminal has no borderless nested box today.
4. Enable the ranked planner for State in P3 — much larger blast radius; only attempted if the
   label-clearance fix and the prefilter retry prove insufficient.

## Testing and verification plan

- Per phase: the new gate (ledger pins + frame oracle + batteries), the state-0038 gate and
  the smallfam/relations resource tests, `ctest -L mermaid`, and the debug/release dual-preset
  matrix at phase closure, with the full debug ctest logged to a durable path and its exit code
  recorded (TL 2026-11-22 process).
- Roster `--emit` + `--self-test` + `--matrix` after each phase.

## Documentation impact

- [ ] `docs/dev/d421-next-family-state-composite.md` — selection (RFC 0048 phase split)
- [ ] `docs/dev/d422-state-composite-rfc0048-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — region model, init/accessibility, routing
- [ ] `docs/README.md` — RFC index row
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — state rows
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: region field on `RelationEntity` vs a parallel map | design review | proposed: append-only `int region` |
| OQ-2: divider glyphs and blocking | design review | proposed: `│` with `┬`/`┴` junctions; blocked cells; fail closed on cross-divider edges |
| OQ-3: accTitle semantics | design review | proposed: sets `diagram_title`; accDescr validated no-op; block form gates |
| OQ-4: init grammar locus | design review | proposed: a state-local closed copy plus the RFC 0047 single-source probe pattern |
| OQ-5: P3 heuristic | design review | proposed: label clearance + prefilter retry |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review (two lenses).
- `accepted` → `implementable`: independent PRR.
- Each phase: one atomic commit with an independent code review.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | — | — | draft | Created provisional from D-421 and the renderer survey |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | (this commit) |
