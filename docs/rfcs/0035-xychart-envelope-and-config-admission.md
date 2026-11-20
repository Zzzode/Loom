---
rfc: 35
title: xychart family envelope and config admission
status: accepted
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
**27** of them with **no new layout algorithm**, through one closed frontmatter
grammar, a narrow config-value widening, and one renderer-fidelity change
(axis-element suppression), in Wave A (four phases) and Wave B (one phase).
The remaining eight
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
| xychart original wide_240 | 13 drawn / **35 source_fallback** / 0 error | 13 + up to 27 drawn; fallback does not grow | `MermaidCapabilityReport.OfficialMatrix` census |
| xychart original narrow_40 | 0 drawn / 48 | unchanged (no promise) | same |
| xychart restored_240 | equals wide_240 | equals wide_240 | same |
| mandatory_gaps | 48 (flowchart body_diagnostic only) | 48 unchanged | `tools/mermaid/e6c-mandatory-gap-roster.json` |
| integrity_errors | 0 | 0 | matrix header |

Fallback composition, re-derived row by row from the 35 sources (sums to 35):

| Bucket | Rows |
|---|---|
| Envelope coexistence (description + `config`, all config leaves already inside the current profile) | **17** |
| Config-value admission | **9** |
| Description + body `%%{init}%%` coexistence (Wave B) | **1** |
| Capability (non-goals) | **8** |

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
- **Config-value, 9:** `labelRotation` 3 —
  `n/rotated-label-45-degrees.mmd`, `n/label-rotation-clamped-above-limit.mmd`,
  `n/label-rotation-clamped-below-limit.mmd`; `show*: false` 5 —
  `n/render-spark-bar-without-displaying-other-property.mmd`,
  `n/render-with-show-axis-label-false.mmd`,
  `n/render-with-show-axis-tick-false.mmd`,
  `n/render-with-show-axis-title-false.mmd`,
  `p/render-with-show-axis-line-false.mmd`; `dataLabelColor` 1 —
  `p/render-all-the-theme-color.mmd`.
- **Wave B, 1:** `n/should-use-all-the-config-from-directive.mmd`.
- **Capability, 8:** per-point labels 4 —
  `n/should-render-a-horizontal-line-chart-with-point-labels.mmd`,
  `n/should-render-a-line-chart-with-mixed-labels-some-points-labeled-some-not.mmd`,
  `n/should-render-a-line-chart-with-point-labels.mmd`,
  `n/should-render-multiple-lines-where-only-one-has-labels.mmd`;
  signed-bar labels 2 —
  `n/should-render-negative-and-decimal-values-with-correct-labels-for-vertical-xy-chart.mmd`,
  `p/should-render-negative-and-decimal-values-with-correct-labels-for-horizontal-xy-chart.mmd`;
  zero-width axis 1 — `n/x-axis-range-same-value.mmd`; >3 line series 1 —
  `p/multiple-plots-can-be-rendered.mmd`.

Promotable total = 17 + 9 + 1 = **27** (35 − 8). Wave A = 26 (envelope 17 +
config-value 9); Wave B = 1 (the init row). The D-394 spike independently
counted 27 (17 envelope + 10 config); this RFC moves the init row out of the
config-value bucket because it needs a validator rewrite, not a value
widening. The promoted set is disjoint from the five E2c
`predicted-blocked` capability rows, which stay E2c-governed.

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

- **G1.** Promote an exact, enumerated reviewed cohort of up to 27 rows where
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

- The eight capability rows, enumerated:
  - signed-bar data-label placement, 2 rows (negative values + `showDataLabel`,
    vertical and horizontal);
  - degenerate zero-width axis, 1 row (`x-axis 0 --> 0`);
  - per-point inline label projection, 4 rows (`line [v "label", ...]`);
  - more than 3 overlaid line series, 1 row (4 lines + 2 bars).
- **Frontmatter `title:`.** No promotable row needs it (the only title row,
  `x-axis-range-same-value`, is a capability row). Title-bearing frontmatter
  keeps whole source, and the existing E2c boundary assertion for it stays.
- **True label rotation.** The terminal has no faithful rotated-glyph
  projection; `labelRotation` is accepted as a documented projection (OQ-1),
  not implemented.
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
  top-level key, a second `description`/`config`, a tab indent, CRLF, or a
  payload line with one or three spaces rejects.

Wave B adds one shape: a description-bearing frontmatter whose body starts
with exactly one `%%{init: ...}%%` directive before the first statement
(today rejected outright by `chart_source_lines`). The directive JSON must
validate against the same closed leaf set (below) and is applied for
`chartOrientation` and `showDataLabel`/`showDataLabelOutsideBar` only.
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
5. **C5** — `labelRotation` accepted in `[-360, 360]` as a documented
   non-applied projection (no renderer change).
6. **C6** — Wave B: rewrite `xy_init_json` into a closed JSON walker with
   the same leaf set (nested `xAxis`/`yAxis` objects, numeric or numeric-string
   scalars) that applies `chartOrientation` and the two `showDataLabel*`
   booleans and ignores metrics; `plotBorderWidth` joins the metric keys.
   The two existing boolean keys keep byte-compatible semantics.

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
- `labelRotation` accepts a numeric scalar in `[-360, 360]` (OQ-1). For the
  two clamped rows (120, -100) Mermaid itself clamps to 0°, so drawing the
  unrotated label is faithful; for the 45° row it is a documented,
  non-applied projection recorded in `docs/decisions/design-decisions.md`.
  Values outside the range reject.
- `themeVariables.xyChart.dataLabelColor` joins the validated-and-ignored
  color-role list (10 → 11 roles) with the same `#rgb`/`#rrggbb`/`#rrggbbaa`
  validation. Like the ten existing roles, color application is SVG-only and
  not projected; this is recorded as a documented projection.
- Wave B: `plotBorderWidth` joins the validated-and-ignored metric keys
  (non-negative number).

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
  line or indented comment in the block, tab, CRLF, two config blocks, two
  init directives, non-leading init, depth beyond `kConfigDepthLimit`,
  out-of-range `labelRotation`, negative `plotBorderWidth`).
- The matrix is wired through the established cohort-branch pattern
  (`tests/ui/test_mermaid_official.cpp`), ordering after the E2c branch; the
  five E2c predicted-blocked rows stay E2c-governed and disjoint.
- Each phase lands atomically: admission code + ledger rows + matrix branch +
  per-row gates in one commit. `classify()` turns any unregistered promotion
  into an error and a new mandatory gap (the D-395 failure mode), so phases
  are never split across commits.
- Roster and census gates: `fallback_attribution.py --matrix` drift gate
  stays green; the roster is migrated only with
  `fallback_attribution.py --emit`, never hand-edited; mandatory gaps stay
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
  D-066 F1, inflates scope far beyond 27 rows, and re-opens the contested
  theme-disposition policy.
- **Capability-first (per-point labels, signed-bar labels, >3 series).**
  Rejected as the first step: only 8 rows, each needs new geometry/collision
  work, and the proven admission path would still be required afterwards.
- **Admission-only `show*: false` (draw content the source hides).**
  Rejected by review r1: the frame would contradict the fixture's asserted
  subject, and no per-row gate as originally specified could detect it.
- **Do nothing.** 35 fallbacks remain; rejected because 27 are gate-only debt
  with an established, safe admission pattern.
- **Piggyback on the requirement F0 envelope widening.** Rejected: different
  family dispatch and config profile; sharing the code would couple two
  unrelated admission surfaces.

## Risks and open questions

- **OQ-1 (owner decision):** `labelRotation` — accept-and-ignore with a
  documented projection (proposed; 2 of 3 rows are exactly faithful because
  Mermaid clamps 120/-100 to 0°) versus rejecting the 3 rows. Owner:
  TechLead. Target is 27 with projection, 24 without.
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
- **Risk (Wave B):** rewriting `xy_init_json` touches the only currently
  admitted init path (`{"xyChart": {showDataLabel, showDataLabelOutsideBar}}`).
  The rewrite must accept the reviewed init leaf set (metrics, orientation,
  the two booleans) while preserving rejection of everything outside it; the
  two existing boolean keys keep byte-compatible semantics. Wave B is
  independently revertible and last.

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review against
  SKILL §3.1. The TechLead is **not** a design gate (project rule §0); the
  TechLead only decides OQ-1.
- `accepted` → `implementable`: agent-run production-readiness review (tests,
  rollback, observability, documentation, roster drift) with a different
  lens, plus the per-phase measurable criteria below.
- Phases (each independently revertible, each atomic across code + ledger +
  matrix branch + gates):
  1. **P1** — closed frontmatter grammar + 17 envelope rows.
  2. **P2** — `show*: false` suppression + 5 rows.
  3. **P3** — `labelRotation` projection + 3 rows (conditional on OQ-1).
  4. **P4** — `dataLabelColor` role + 1 row.
  5. **P5 (Wave B)** — description + init coexistence + init validator
     rewrite + `plotBorderWidth` + 1 row.
- Graduation per phase: targeted xychart census (drawn/fallback/error deltas
  match the phase's row list exactly), targeted tests green. Family closure:
  full matrix dual-preset byte-identical, mandatory gaps 48, integrity errors
  0, roster migrated via `--emit`, full serial ctest with only the documented
  TCP flake.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-21 | r1 | `agent:ad3f7286f2f4af8be` (independent general-purpose, own-measurement adversarial design review) | REQUEST-CHANGES | B1 `show*: false` admission-only → **accepted**: suppression implemented (P2). M1 26 vs 27 contradiction → **accepted**: promotable stated as 27 (17+9+1). M2 three-code-change undercount / pie validator reuse → **accepted**: six-change inventory, new sibling validator, pie untouched. M3 envelope grammar ambiguous → **accepted**: closed grammar + acceptance table; `title` dropped from scope. M4 E2c extension collision → **accepted**: sibling ledger + new test file; boundary test edit named. M5 phase order recreates D-395 pitfall → **accepted**: phases atomic. m1 evidence pin → **accepted**: re-pinned to `008d5ca1`. m2–m8, NITs → **accepted**: owners named, TL-not-a-gate stated, dependency direction stated, attribution clarified, conditional targets added, non-goal wording fixed, projection doc named. |
| 2026-11-21 | r2 | same agent, independent re-measurement of the revised file | APPROVE-TO-ACCEPT | m-A sibling/`allow_description` mechanics → **accepted**: stated in the change inventory (C2/C3). m-B Wave B risk wording → **accepted**. m-C boundary theme wording → **accepted**. m-D chart-title suppression removed from scope → **accepted**. m-E description-only keeps D236 path → **accepted**. m-F numeric-string JSON scalars → **accepted** (C6). m-G full 27-row enumeration → **accepted**. NITs (wave wording, `pr-check/` path, E2c disjointness, reviewers frontmatter, OQ-2 accept) → **accepted**. No BLOCKER/MAJOR. |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-21 | — | Draft created (provisional), no code | — |
| 2026-11-21 | — | r1 review dispositions applied (provisional), no code | — |
| 2026-11-21 | — | r2 APPROVE-TO-ACCEPT; promoted to `accepted`; residual MINOR/NIT folded in | — |
