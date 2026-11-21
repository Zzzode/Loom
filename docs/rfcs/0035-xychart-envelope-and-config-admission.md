---
rfc: 35
title: xychart family envelope and config admission
status: implementable
owners: "@Zzzode"
reviewers:
  - "agent:ad3f7286f2f4af8be (independent design review, own measurement; r1 REQUEST-CHANGES → r2 APPROVE-TO-ACCEPT)"
created: 2026-11-21
last-reviewed: 2026-11-21
supersedes: ""
tracking: local
---

# RFC 0035 — xychart family envelope and config admission

## Summary

The `xychart-beta` family falls back to plain source for **35 of its 48**
official original cases at 240 columns (13 drawn, 0 errors). All 35 are
family-scoped debt, not terminal hard limits. This RFC proposes closing up to
**24** of them with **no new layout algorithm**, through one closed frontmatter
grammar, a narrow config-value widening, and one renderer-fidelity change
(axis-element suppression), in Wave A (three phases) and Wave B (one phase).
The remaining eleven
rows need genuinely new chart capabilities and are explicit non-goals.
Promotion follows the established reviewed-cohort ledger pattern
(journey/timeline/E2c precedent): an exact enumerated row list, per-row frame
gates through the public renderer, and the dual-preset byte-identical matrix.

Status is **accepted**: the agent-run design gate passed (r2
APPROVE-TO-ACCEPT, see Review History). No production code may merge before
the `implementable` gate (production-readiness review).

## Motivation

### Evidence

Measured on the certified matrix
`build/debug/tests/ui/mermaid-official-matrix.json`, generation
`008d5ca1fd23ddbb4913cd9f329d64cb404e797427ae8440399ede2a31ca32b5`
(re-verified after D-395 F0; the xychart numbers are identical to the
pre-F0 generation `ee743515…`, byte-identical across debug and release;
2738 cases = 2145 original + 593 body_diagnostic).

| Metric | Current | Target | How measured |
|---|---|---|---|
| xychart original wide_240 | 13 drawn / **35 source_fallback** / 0 error | 13 + up to 24 drawn; fallback does not grow | `MermaidCapabilityReport.OfficialMatrix` census |
| xychart original narrow_40 | 0 drawn / 48 | unchanged (no promise) | same |
| xychart restored_240 | equals wide_240 | equals wide_240 | same |
| mandatory_gaps | 48 (flowchart body_diagnostic only) | 48 unchanged | `tools/mermaid/e6c-mandatory-gap-roster.json` |
| integrity_errors | 0 | 0 | matrix header |

Fallback composition, re-derived row by row from the 35 sources (sums to 35):

| Bucket | Rows |
|---|---|
| Envelope coexistence (description + `config`, all config leaves already inside the current profile) | **17** |
| Config-value admission | **6** |
| Description + body `%%{init}%%` coexistence (Wave B) | **1** |
| Capability (non-goals) | **11** |

The exact row sets (all paths relative to
`tests/fixtures/mermaid_official/cases/`; `n/` = `xychart/nightly-check/`,
`p/` = `xychart/pr-check/`):

- **Envelope, 17:** `n/data-labels-single-bar-horizontal.mmd`,
  `n/data-labels-single-bar-vertical.mmd`,
  `n/data-labels-within-bars-horizontal-many-bars.mmd`,
  `n/data-labels-within-bars-horizontal.mmd`,
  `n/data-labels-within-bars-vertical-many-bars.mmd`,
  `n/data-labels-within-bars-vertical.mmd`,
  `n/render-spark-line-with-plotreservedspacepercent.mmd`,
  `n/should-render-a-single-bar-with-label-for-a-horizontal-xy-chart.mmd`,
  `n/should-render-a-single-bar-with-label-for-a-vertical-xy-chart.mmd`,
  `n/should-render-horizontal-bar-chart-with-labels-outside-bar.mmd`,
  `n/should-render-horizontal-bar-chart-without-labels-by-default.mmd`,
  `n/should-render-multiple-bar-plots-horizontally-with-labels-correctly.mmd`,
  `n/should-render-vertical-bar-chart-with-labels-outside-bar.mmd`,
  `n/should-use-all-the-config-from-yaml.mmd`,
  `p/should-render-horizontal-bar-chart-with-labels.mmd`,
  `p/should-render-multiple-bar-plots-vertically-with-labels-correctly.mmd`,
  `p/should-render-vertical-bar-chart-with-labels.mmd`.
- **Config-value, 6:** `show*: false` 5 —
  `n/render-spark-bar-without-displaying-other-property.mmd`,
  `n/render-with-show-axis-label-false.mmd`,
  `n/render-with-show-axis-tick-false.mmd`,
  `n/render-with-show-axis-title-false.mmd`,
  `p/render-with-show-axis-line-false.mmd`; `dataLabelColor` 1 —
  `p/render-all-the-theme-color.mmd`.
- **Wave B, 1:** `n/should-use-all-the-config-from-directive.mmd`.
- **Capability, 11:** per-point labels 4 —
  `n/should-render-a-horizontal-line-chart-with-point-labels.mmd`,
  `n/should-render-a-line-chart-with-mixed-labels-some-points-labeled-some-not.mmd`,
  `n/should-render-a-line-chart-with-point-labels.mmd`,
  `n/should-render-multiple-lines-where-only-one-has-labels.mmd`;
  signed-bar labels 2 —
  `n/should-render-negative-and-decimal-values-with-correct-labels-for-vertical-xy-chart.mmd`,
  `p/should-render-negative-and-decimal-values-with-correct-labels-for-horizontal-xy-chart.mmd`;
  label rotation 3 — `n/rotated-label-45-degrees.mmd`,
  `n/label-rotation-clamped-above-limit.mmd`,
  `n/label-rotation-clamped-below-limit.mmd` (OQ-1 ruling: geometry not
  applied; 45° differs from upstream, 120/−100 coincidentally clamp to 0°);
  zero-width axis 1 — `n/x-axis-range-same-value.mmd`; >3 line series 1 —
  `p/multiple-plots-can-be-rendered.mmd`.

Promotable total = 17 + 6 + 1 = **24** (35 − 11). Wave A = 23 (envelope 17 +
config-value 6); Wave B = 1 (the init row). The D-394 spike counted 27
(17 envelope + 10 config-ish); this RFC moves the init row out of the
config-value bucket because it needs a sibling validator, and moves the three
`labelRotation` rows into the capability bucket by owner ruling (OQ-1). The
promoted set is disjoint from the E2c `predicted-blocked` capability rows,
which stay E2c-governed.

`fallback_roster.json` assigns all 35 to `family_residual:xychart`
(causality `family-scoped-debt`); there is no cross-family shared blocker to
demote, and `design-decisions.md` records no xychart hard limit.

Prior analysis: `docs/dev/mermaid-d066-zero-fallback-plan.md`, the D-394
selection spike (`docs/dev/mermaid-next-family-spike-d394.md`), and its
adversarial verification, which corrected the `dataLabelColor` role, the
`x-axis-range-same-value` double-count, and the "envelope is one flag"
assumption. Design review r1 (2026-11-21) additionally corrected the
promotable count, the code-change inventory and the `show*: false` fidelity
requirement; see Review History.

## Goals

- **G1.** Promote an exact, enumerated reviewed cohort of up to 24 rows where
  every promoted row is measured to draw natively and its frame is pinned by
  a per-row gate.
- **G2.** Fail closed: any frontmatter key, config key, value, duplicate,
  nesting depth or malformed scalar outside the documented grammar keeps the
  whole source on fallback.
- **G3.** No new layout algorithm. The only renderer change is suppressing
  axis elements the source explicitly hides (`show*: false`); no geometry,
  collision or layout work.
- **G4.** No regression: fallback count does not grow for any family, errors
  stay 0, mandatory gaps stay 48, and the dual-preset matrix remains
  byte-identical.

## Non-Goals

- The eleven capability rows, enumerated:
  - signed-bar data-label placement, 2 rows (negative values + `showDataLabel`,
    vertical and horizontal);
  - degenerate zero-width axis, 1 row (`x-axis 0 --> 0`);
  - per-point inline label projection, 4 rows (`line [v "label", ...]`);
  - more than 3 overlaid line series, 1 row (4 lines + 2 bars);
  - label rotation, 3 rows (45°, 120°, −100°). **OQ-1 ruling:** geometry is
    not applied. 45° differs visibly from upstream; 120°/−100° happen to
    clamp to 0° in Mermaid, which is coincidence, not an implementation.
    Recording these as a "documented projection" would book an unimplemented
    geometry as drawn (quality laundering); they stay visible capability
    debt, schedulable on their own, and can be earned back by implementing
    rotation.
- **Frontmatter `title:`.** No promotable row needs it (the only title row,
  `x-axis-range-same-value`, is a capability row). Title-bearing frontmatter
  keeps whole source, and the existing E2c boundary assertion for it stays.
- **True label rotation.** The terminal has no faithful rotated-glyph
  projection; `labelRotation` is not admitted (see the capability list).
- Widened theme/look/layout admission of any kind. The existing `theme` value
  set and ten color roles stay as they are; the only addition is
  `themeVariables.xyChart.dataLabelColor` as an eleventh validated-and-ignored
  color role. D-066 F1 remains its own RFC.
- Chart width/height/font/padding/tick metrics: already validated-and-ignored
  by the terminal projection; not re-litigated here (`plotBorderWidth` joins
  that list only for the Wave B row).
- The requirement-family F0 inert-config cohort (landed in D-395); no shared
  envelope generalization beyond the charts dispatch.
- Manifest edits: the manifest SHA is pinned across tests, support headers and
  verifiers (several inside the ten protected files); promotion uses the
  code-side reviewed cohort ledger, never the manifest.
- narrow_40 drawing for xychart: all 48 rows stay narrow-fallback in the
  current renderer; this RFC does not promise narrow geometry.

## Proposal

### 1. Closed frontmatter grammar (xychart only)

An xychart source with `description` is admitted only when its frontmatter
matches this grammar exactly:

```
description: >-            # literal folded header, first key, exactly once
  <payload line>           # every payload line indented by exactly two spaces
  <payload line>
config:                    # zero or one config block, immediately after
  <existing xy_visual_envelope profile, unchanged>
```

- The description is validated by the shared `parse_envelope` reader
  (`EnvelopeScalar`, form `Folded`, header `>-`, ≤ `kDescriptionLimit`).
- A new sibling validator `xy_description_envelope` in
  `src/ui/visual/mermaid_charts_impl.cpp` enforces the raw shape (first key
  `description: >-`, payload exactly two spaces, non-empty payload, no blank
  line inside the block, no indented comment) and then validates any
  following `config` block with the **existing** `xy_visual_envelope` rules.
  The sibling skips the `description` event itself (already validated by the
  raw-shape check), so `xy_visual_envelope` never sees a top-level
  `description`; `pie_description_frontmatter` is **not edited** and the pie
  path is byte-unchanged.
- The dispatch in `chart_source_lines` consults `chart_body_family` **before**
  the pie validator: a description-bearing xychart body that also carries a
  `config` block routes to the new validator, a description-only xychart body
  keeps the D236 `XyDescription` path, and a pie body keeps today's path.
- `description` alone remains admitted exactly as today (RFC 0031 D236).
  `config` alone remains admitted exactly as today. `title`, any second
  top-level key, a second `description`/`config`, a tab indent, or a
  payload line with one or three spaces rejects. Trailing CR is normalized
  before validation, matching the D236 pie folded-description path: a CRLF
  source projects identically to its LF twin and is admitted.

Wave B adds one shape: a description-bearing frontmatter whose body starts
with exactly one `%%{init: ...}%%` directive before the first statement
(today rejected outright by `chart_source_lines`). The directive JSON is
validated by a **sibling** walker with the same closed leaf set and is
applied for `chartOrientation` and `showDataLabel`/`showDataLabelOutsideBar`
only; the existing `xy_init_json` (plain-init path) is left byte-unchanged,
so orientation-only plain-init sources keep rejecting
(`tests/ui/test_mermaid_charts_semantics.cpp:5637` stays valid). The sibling
walker rejects an empty `xyChart: {}`, so the description+init source at
`:12201` also stays on fallback.
Numeric strings (`"20"`) are accepted exactly as the YAML profile accepts
them through `number()`.

Rejection remains exact for every other combination.

**Change inventory** (all sites in `src/ui/visual/mermaid_charts_impl.cpp`
unless stated):

1. **C1** — `chart_source_lines` dispatch: body-family-first routing for
   description-bearing sources; frontmatter+init gate relaxed for Wave B.
2. **C2** — new `xy_description_envelope` sibling validator (raw shape +
   config rules via the existing profile; skips the description event).
3. **C3** — `xy_visual_envelope` gains an `allow_description` parameter used
   only by the sibling path; the plain XY path keeps rejecting `description`.
4. **C4** — eight per-axis suppression flags on the TU-local `Chart`,
   set from the validated `show*` values; the renderer skips the
   corresponding draw calls **without reflow** (the `top`/`left`/`footer`
   reservations are unchanged, so only the element's cells disappear).
5. **C5 (withdrawn by OQ-1)** — no `labelRotation` admission; the three rows
   stay capability debt and no code changes.
6. **C6** — Wave B: a new sibling `xy_description_init_json` walker with the
   same leaf set (nested `xAxis`/`yAxis` objects, numeric or numeric-string
   scalars) that applies `chartOrientation` and the two `showDataLabel*`
   booleans and ignores metrics; `plotBorderWidth` joins the metric keys
   **for this walker only**. `xy_init_json` is not edited; both walkers share
   one leaf/value table so they cannot drift.
7. **C7** — `tests/ui/CMakeLists.txt`: register the new cohort test target
   exactly like the E2c block (`add_executable` + `target_link_libraries(...
   loom_ui loom_utils GTest::gtest loom_test_main)` + `gtest_discover_tests`
   with `TIMEOUT 300` and `LABELS "mermaid"`); `LOOM_TESTS_DIR` is global.
8. **C8** — `tools/mermaid/generate_xychart_0035_cohort.py`: the ledger
   generator (mirrors `generate_e2c_xychart_cohort.py`), with `--check`
   reproducibility; the ledger is pre-registered by the generator, then
   proven by the gate, never hand-edited afterwards.
9. **C9** — per-phase `tools/mermaid/fallback_roster.json` regeneration via
   `fallback_attribution.py --emit` in the same commit (D-395 precedent), so
   the `--matrix` drift gate stays green between phases.

### 2. Config-value widening

- `show*` flags: `xAxis`/`yAxis` `showLabel`, `showTick`, `showTitle`,
  `showAxisLine` accept `false` as well as `true`. Top-level
  `showDataLabel`/`showDataLabelOutsideBar` keep their bool domain;
  `xyChart.showTitle`/`showLegend` keep `true`-only (not in scope).
- `show*: false` is **applied**, not merely accepted: `Chart` gains eight
  per-axis suppression flags and the renderer skips the corresponding draw
  calls (axis titles, tick marks, tick labels, axis line). The chart title is
  not in scope: no promoted row hides it and `xyChart.showTitle` stays
  `true`-only. Without suppression the frame would show content the source
  explicitly hides — the r1 BLOCKER. Each affected row's gate asserts the
  hidden element is absent while the rest of the frame equals the same
  fixture with the flag set to `true` (or unset), so suppression is proven,
  not assumed. Suppression must not reflow: layout reservations are
  unchanged, only the element's cells disappear.
- `labelRotation` is **not** admitted: any occurrence keeps the whole source
  on fallback. The three rows are capability debt (OQ-1 ruling), not a
  documented projection.
- `themeVariables.xyChart.dataLabelColor` joins the validated-and-ignored
  color-role list (10 → 11 roles) with the same `#rgb`/`#rrggbb`/`#rrggbbaa`
  validation. Like the ten existing roles, color application is SVG-only and
  not projected; this is recorded as a documented projection.
- Wave B: `plotBorderWidth` joins the validated-and-ignored metric keys for
  the sibling init walker only (non-negative number); the YAML profile still
  rejects it, so `tests/ui/test_mermaid_charts_semantics.cpp:4408` is
  unchanged.

### 3. Reviewed cohort ledger and gates

- A **sibling** ledger `tools/mermaid/xychart-0035-cohort.json` (schema
  `loom_mermaid_xychart_0035_cohort/v1`), never an extension of
  `e2c-xychart-cohort.json`: the E2c ledger's reviewed rule excludes
  config/title/init, its counts are hard-asserted, and the E2c boundary test
  asserts that every title/init row keeps whole source. Wave B's init row is
  therefore removed from that boundary assertion by an explicit, documented
  test edit in the same commit that promotes it.
- The ledger carries the exact promoted rows per phase, the per-width
  expected states (wide/restored drawn, narrow source), and the pre-registered
  prediction proven by the gate — not a post-hoc measurement.
- A new per-row gate `tests/ui/test_mermaid_xychart_0035_cohort.cpp` renders
  each promoted fixture through the public interface (`render_markdown` on a
  fenced block) and asserts: native frame, determinism across repeated
  captures, description text never rendered, suppression semantics for
  `show*: false` rows, and a boundary set that stays fallback (unknown
  keys/values, theme/look/layout values outside the existing profile,
  duplicate description/config, title, blank
  line or indented comment in the block, tab, two config blocks, two
  init directives, non-leading init, empty `xyChart: {}` init,
  orientation-only plain-init (non-description path), depth beyond
  `kConfigDepthLimit`, any `labelRotation` occurrence, negative
  `plotBorderWidth`).
- The matrix is wired through the established cohort-branch pattern
  (`tests/ui/test_mermaid_official.cpp`), ordering after the E2c branch; the
  five E2c predicted-blocked rows stay E2c-governed and disjoint.
- Each phase lands atomically: admission code + generator + ledger rows +
  matrix branch + per-row gates + regenerated `fallback_roster.json` in one
  commit. `classify()` turns any unregistered promotion into an error and a
  new mandatory gap (the D-395 failure mode), so phases are never split
  across commits.
- Roster and census gates: `fallback_attribution.py --matrix` stays green in
  every phase because each phase commit regenerates the roster with
  `fallback_attribution.py --emit` (never hand-edited); mandatory gaps stay
  48; integrity errors stay 0.

### 4. Invariants catalog

- Closed key set and closed value set at every config level; duplicate paths
  and depth beyond `kConfigDepthLimit` reject.
- No theme/look/layout value is expanded; `theme` values and the ten
  existing color roles are unchanged.
- The description is inert metadata and is never rendered; no footer is
  added (unlike the requirement F0 disclosure).
- `show*: false` semantics: the corresponding element is absent from the
  frame; all other elements are unchanged.
- Work metering: every new path spends before it mutates, mirroring the
  existing envelope charges.
- No persisted, wire, settings or session shape is touched.
- Dependency direction: every change is inside the charts owning TU
  (`src/ui/visual/mermaid_charts_impl.cpp`; `struct Chart` is TU-local);
  `parse_envelope` is reused unmodified; no module interface, import or
  public API edge is introduced.

## Alternatives

- **Full xychart config system including theme/look.** Rejected: overlaps
  D-066 F1, inflates scope far beyond 24 rows, and re-opens the contested
  theme-disposition policy.
- **Capability-first (per-point labels, signed-bar labels, >3 series).**
  Rejected as the first step: only the 8 geometry-capability rows, each needs new geometry/collision
  work, and the proven admission path would still be required afterwards.
- **Admission-only `show*: false` (draw content the source hides).**
  Rejected by review r1: the frame would contradict the fixture's asserted
  subject, and no per-row gate as originally specified could detect it.
- **Do nothing.** 35 fallbacks remain; rejected because 24 are gate-only debt
  with an established, safe admission pattern.
- **Piggyback on the requirement F0 envelope widening.** Rejected: different
  family dispatch and config profile; sharing the code would couple two
  unrelated admission surfaces.

## Risks and open questions

- **OQ-1 (resolved, owner ruling 2026-11-21):** `labelRotation` is **not
  admitted**; the three rows are recorded in the capability bucket with the
  reason "geometry not applied; 45° differs from upstream, 120/−100
  coincidentally clamp to 0°". Target is 24, not 27. Rationale: rotation
  changes label geometry, so accepting it would book an unimplemented
  capability as drawn; if rotation is later implemented it earns the three
  rows back through the normal census.
- **OQ-2 (resolved by design review):** `dataLabelColor` belongs in the
  validated-and-ignored color-role list; it is the same class as the ten
  existing roles, and color application is SVG-only. Owner: agent design
  review (accept).
- **OQ-3 (implementation gate):** any row whose body does not draw after
  admission moves to the capability bucket with its measured reason recorded.
  Owner: implementation agent.
- **OQ-4 (reviewer note):** narrow_40 stays source fallback; the wide-column
  gain is not a narrow-column promise. Owner: RFC owner.
- **Risk:** over-admission could admit bodies that draw unfaithfully.
  Mitigation: per-row frame gates including suppression semantics, the
  boundary set, and a census after every phase with fallback non-growth.
- **Risk (Wave B):** the sibling init walker duplicates leaf/value rules that
  the YAML profile also encodes. Mitigation: both walkers read one shared
  leaf/value table, and the boundary set pins the plain-init path unchanged
  (`xy_init_json` is not edited). Wave B is independently revertible and
  last.

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review against
  SKILL §3.1. The TechLead is **not** a design gate (project rule §0); the
  only owner decision in this RFC is OQ-1, now ruled (rotation rejected,
  target 24).
- Repo-wide `rfc_lint.py` is red only on pre-existing 0033 duplicate-number
  attachments (not introduced here); 0035 contributes zero findings. That
  debt is tracked separately and must not be read as a 0035 failure.
- `accepted` → `implementable`: agent-run production-readiness review (tests,
  rollback, observability, documentation, roster drift) with a different
  lens, plus the per-phase measurable criteria below.
- Phases (each independently revertible, each atomic across code + ledger +
  matrix branch + gates):
  1. **P1** — closed frontmatter grammar + 17 envelope rows.
  2. **P2** — `show*: false` suppression + 5 rows.
  3. **P3** — `dataLabelColor` role + 1 row.
  4. **P4 (Wave B)** — description + init coexistence + sibling init walker
     + `plotBorderWidth` + 1 row.
- Graduation per phase: targeted xychart census (drawn/fallback/error deltas
  match the phase's row list exactly), targeted tests green. Family closure:
  full matrix dual-preset byte-identical, mandatory gaps 48, integrity errors
  0, roster migrated via `--emit`, full serial ctest with only the documented
  TCP flake.

## Production Readiness Review

### Testing and verification plan

- New behaviour → new tests: `tests/ui/test_mermaid_xychart_0035_cohort.cpp`
  (per-row frame gates, suppression twins, boundary set) plus the
  cohort branch in `tests/ui/test_mermaid_official.cpp`.
- Failure paths: the boundary set is the failure-path suite — every rejected
  combination asserts `source_fallback`, not an error.
- Goldens: no existing golden file changes. The 13 already-drawn xychart rows
  and all other families keep their frames. **Two** reviewed negative
  assertions flip, one per phase, and both are re-asserted in the same atomic
  commit:
  - P1 — `tests/ui/test_mermaid_charts_semantics.cpp:12187-12192`
    (`ConfigTitleAndInitStillFailClosed`): the `with_config` case is
    description + `config.xyChart.width`, an in-profile metric, so P1 admits
    it; it is split out and asserted drawn, while `with_title` (no title
    projection) and `with_init` (empty `xyChart: {}`) stay negative and stay
    valid.
  - P2 — `tests/ui/test_mermaid_charts_semantics.cpp:4408-4421`: the
    `xAxis.showLabel: false` entry is re-asserted as drawn with the label
    suppressed. Its neighbours stay negative and stay valid:
    `xyChart.showTitle: false` (top-level remains `true`-only) and
    `plotBorderWidth: 5` in the YAML profile (Wave B admits it only in the
    sibling init walker).
  - P4 (Wave B) flips no `test_mermaid_charts_semantics.cpp` assertion:
    plain-init orientation-only remains rejected because `xy_init_json` is
    untouched (`:5637`), and the description+init empty-object source stays
    rejected by the sibling walker (`:12201`). P4 does require the E2c
    boundary-test edit named in §3.
  - NIT-3 (test comment): `tests/ui/test_mermaid_charts_semantics.cpp:10637-10639`
    says "config/title/init and malformed envelopes still fail closed"; the
    `config` clause becomes false at P1, so P1 updates that comment.
- Flakes: no new wall-clock assertion; the only timing-sensitive suite is
  untouched.
- Serial ctest total: baseline 4608 (D-395 closure). Each phase adds its
  cohort tests and the delta must reconcile exactly at family closure.
- Verification commands: per phase `cmake --build --preset debug -j8` +
  `ctest --preset debug -R MermaidXychart0035|MermaidXychartCohort|MermaidOfficialMatrix`;
  family closure `cmake --workflow --preset ci` plus the release matrix run and
  `python3 -I tools/mermaid/fallback_attribution.py --matrix`.
- Shape couplings: the matrix `expected`/`reason` shape, the cohort-ledger
  schema, and the roster classification are all string-shaped; the ledger
  name is new (no consumer drift), and the roster is migrated only through
  `fallback_attribution.py --emit`.

### Build system and module discipline

- No module interface change: `struct Chart` is TU-local and every code
  change is a `.cpp` body edit in one TU. BMI PSS and recompile fan-out are
  unchanged; no measurement is required for a body-only change.
- The new cohort test target is registered per C7 (explicit per-file block,
  same shape as the E2c target); `LOOM_TESTS_DIR` is global.
- No concurrency caps, no new textual includes, no new modules or imports;
  the module graph and directory SCCs are unchanged.

### Rollout and rollback

- Rollout order: P1 → P2 → P3 → P4, each phase gated by its targeted census
  row matching the phase table before the next starts.
- One commit per phase; reverting a phase restores its rows to
  `source_fallback` and removes its ledger rows atomically. Rollback is
  LIFO: P1 (C1–C3) is a hard prerequisite of P2/P3 because all their rows are
  description+config sources, and of P4 because the same dispatch work
  relaxes frontmatter+init.
- No flag-day interface change; internal-only.
- No persisted data, settings, wire, session or debug-trace shape is touched.

### Observability

- Session traces (`messages.jsonl`, `dump-prompts/`) are unaffected —
  rendering-only changes.
- No background worker, thread or render ticker is introduced.
- Diagnostics: the matrix JSON and the cohort ledger are the evidence
  surfaces; no new ad-hoc print path.

### Documentation and deletion

- `docs/decisions/design-decisions.md` gains the projection records:
  `dataLabelColor` as a validated-and-ignored color role and the
  axis-element suppression semantics; it also records the `labelRotation`
  capability-bucket ruling.
- `CLAUDE.md` needs no change (no convention, path or build-layout change).
- No dead code is created: `xy_init_json` is untouched, and the new
  `xy_description_init_json` is added as a sibling sharing one leaf/value
  table.

### Platform readiness

- Debug + release presets build `-Werror` clean on the Linux dev box; the
  matrix byte gate runs on both.
- CI is not a gate in this project (standing rule); macos-14 numbers are
  historical only.
- No termios/signal/platform guard is touched; the truecolor rendering path
  is unchanged apart from suppression; no new network fetch.

### Sign-off

| Role | Reviewer | Date | Verdict |
|---|---|---|---|
| Design | `agent:ad3f7286f2f4af8be` | 2026-11-21 | approve (r2) |
| Production readiness | `agent:a3cfba9621c06cef4` | 2026-11-21 | approve (r2; MINOR-1/MINOR-2 folded in) |
| Code (per phase) | pending | — | — |

## Phases and graduation criteria

Baseline after D-395: wide_240 drawn 1247 / fallback 898; xychart 13 / 35;
formal `drawn_at_240` 1061; mandatory gaps 48; integrity errors 0. Each
phase's xychart delta must equal its row list exactly, and the global drawn
delta must be the same number (no other family moves).

| Phase | Status | Rows | xychart drawn/fallback | global drawn/fallback | formal |
|---|---|---|---|---|---|
| P1 envelope grammar | done | 17 | 30 / 18 | 1264 / 881 | 1078 |
| P2 `show*: false` suppression | done | 5 | 35 / 13 | 1269 / 876 | 1083 |
| P3 `dataLabelColor` role | done | 1 | 36 / 12 | 1270 / 875 | 1084 |
| P4 Wave B init | proposed | 1 | 37 / 11 | 1271 / 874 | 1085 |

P1 evidence (measured): debug and release matrix sha256
`7047d8a19335dd8367f5254c0df2f3d994b9bd47551b4c8680d260dde4171b2f`
(byte-identical), xychart 30/18, global 1264/881, formal `drawn_at_240`
1078, mandatory gaps 48, integrity errors 0, roster 881 rows / 44 categories
migrated with `--emit`; cohort gate 6/6 debug and release.

P2 evidence (measured): debug and release matrix sha256
`5096d1a93a1ddc3f5b92f4861a902c2017d9576bfa10ba419d4ec95717550bf9`
(byte-identical), xychart 35/13, global 1269/876, formal `drawn_at_240`
1083, mandatory gaps 48, integrity errors 0, roster 876 rows / 44 categories
migrated with `--emit`; cohort gate 8/8 debug and release. The per-axis
suppression gate is proven by flag-flip twins (frame changes, no reflow,
axis cells only) and by horizontal-orientation synthetics.

P3 evidence (measured): debug and release matrix sha256
`240eec9e631555eda9038c4c032c9016118ff78c6baed3225e70b65bdd8790aa`
(byte-identical), xychart 36/12, global 1270/875, formal `drawn_at_240`
1084, mandatory gaps 48, integrity errors 0, roster 875 rows / 44 categories
migrated with `--emit`; cohort gate 8/8 debug and release. The role shares
`chart_color_value` with the existing ten and is discarded after validation.

The eleven capability rows (8 geometry + 3 rotation) stay
`source_fallback`; there is no OQ-1 fallback branch anymore — the ruling is
final for this RFC.

Family closure: dual-preset byte-identical matrix, mandatory gaps 48,
integrity errors 0, roster final state re-verified (each phase already
migrated it), close-out record in `docs/dev/`.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-21 | r1 | `agent:ad3f7286f2f4af8be` (independent general-purpose, own-measurement adversarial design review) | REQUEST-CHANGES | B1 `show*: false` admission-only → **accepted**: suppression implemented (P2). M1 26 vs 27 contradiction → **accepted**: promotable stated as 27 (17+9+1). M2 three-code-change undercount / pie validator reuse → **accepted**: six-change inventory, new sibling validator, pie untouched. M3 envelope grammar ambiguous → **accepted**: closed grammar + acceptance table; `title` dropped from scope. M4 E2c extension collision → **accepted**: sibling ledger + new test file; boundary test edit named. M5 phase order recreates D-395 pitfall → **accepted**: phases atomic. m1 evidence pin → **accepted**: re-pinned to `008d5ca1`. m2–m8, NITs → **accepted**: owners named, TL-not-a-gate stated, dependency direction stated, attribution clarified, conditional targets added, non-goal wording fixed, projection doc named. |
| 2026-11-21 | r2 | same design-review agent, independent re-measurement of the revised file | APPROVE-TO-ACCEPT | m-A sibling/`allow_description` mechanics → **accepted**: stated in the change inventory (C2/C3). m-B Wave B risk wording → **accepted**. m-C boundary theme wording → **accepted**. m-D chart-title suppression removed from scope → **accepted**. m-E description-only keeps D236 path → **accepted**. m-F numeric-string JSON scalars → **accepted** (C6). m-G full 27-row enumeration → **accepted**. NITs (wave wording, `pr-check/` path, E2c disjointness, reviewers frontmatter, OQ-2 accept) → **accepted**. No BLOCKER/MAJOR. |
| 2026-11-21 | prr-r1 | `agent:a3cfba9621c06cef4` (independent production-readiness review, build/rollback/test-infrastructure lens) | REQUEST-CHANGES | M1 P1 flips `ConfigTitleAndInitStillFailClosed.with_config` → **accepted**: enumerated, split out and re-asserted drawn in P1. M2 P5 would flip the plain-init orientation assertion → **accepted**: sibling `xy_description_init_json`, `xy_init_json` untouched, no flip. M3 roster timing self-contradiction → **accepted**: every phase commit regenerates the roster via `--emit` (C9); closure wording fixed. m1 test target registration → **accepted** (C7). m2 rollback overclaim → **accepted**: LIFO, P1 prerequisite. m3 empty init object → **accepted**: stays rejected, boundary-pinned. m4 generator → **accepted** (C8). NIT ledger name convention → recorded. Phase arithmetic and census re-verified correct by the reviewer. |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-21 | — | Draft created (provisional), no code | — |
| 2026-11-21 | — | r1 review dispositions applied (provisional), no code | — |
| 2026-11-21 | — | r2 APPROVE-TO-ACCEPT; promoted to `accepted`; residual MINOR/NIT folded in | — |
| 2026-11-21 | — | OQ-1 owner ruling: `labelRotation` rejected (capability bucket), target 27 → 24; phases renumbered; PRR r2 MINOR-1/MINOR-2 and NIT-1..3 applied | — |
| 2026-11-21 | — | prr-r2 APPROVE-TO-IMPLEMENTABLE; production-readiness sign-off recorded; promoted to `implementable` | — |
| 2026-11-21 | P1 | Closed description+config grammar; 17 rows promoted; census and dual-preset sha recorded in the phase table | — |
| 2026-11-21 | P2 | Applied axis-element suppression for show*: false; 5 rows promoted; per-phase review gating tightened | — |
| 2026-11-21 | P3 | dataLabelColor joins the validated-and-ignored color roles; 1 row promoted | — |
