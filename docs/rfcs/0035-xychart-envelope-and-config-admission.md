---
rfc: 35
title: xychart family envelope and config admission
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-11-21
last-reviewed: 2026-11-21
supersedes: ""
tracking: local
---

# RFC 0035 — xychart family envelope and config admission

## Summary

The `xychart-beta` family falls back to plain source for **35 of its 48**
official original cases at 240 columns (13 drawn, 0 errors); all 35 are
family-scoped debt, not terminal hard limits. This RFC proposes closing up to
**26** of them with **no new layout algorithm**, through two scoped admission
changes: (1) allow a validated folded `description` to coexist with an
already-admitted `config` block (plus `title`), and (2) widen the narrow
config-value whitelist (`show*: false`, `labelRotation` as a documented
projection, `themeVariables.xyChart.dataLabelColor` as a validated color role,
full `init` config). The remaining eight rows need genuinely new chart
capabilities and are explicit non-goals. Promotion follows the established
reviewed-cohort ledger pattern (journey/timeline/E2c precedent): an exact
enumerated row list, a per-row frame gate through the public renderer, and the
dual-preset byte-identical matrix.

Status is **provisional**: this document is a draft for agent-run design
review. No production code may merge before the `implementable` gate.

## Motivation

### Evidence

Measured 2026-11-20/21 on the certified matrix
`build/debug/tests/ui/mermaid-official-matrix.json`
(sha256 `ee743515f2e2a68b8595328ed2d101f73430a344e75056707a3e3a59d8dd2ab1`,
byte-identical across debug and release; 2738 cases = 2145 original + 593
body_diagnostic).

| Metric | Current | Target | How measured |
|---|---|---|---|
| xychart original wide_240 | 13 drawn / **35 source_fallback** / 0 error | 13 + up to 26 drawn; fallback does not grow | `MermaidCapabilityReport.OfficialMatrix` census |
| xychart original narrow_40 | 0 drawn / 48 | unchanged (no promise) | same |
| xychart restored_240 | equals wide_240 | equals wide_240 | same |
| mandatory_gaps | 48 (flowchart body_diagnostic only) | 48 unchanged | `tools/mermaid/e6c-mandatory-gap-roster.json` |
| integrity_errors | 0 | 0 | matrix header |

Fallback composition (measured per-fixture source analysis; sums to 35):
envelope admission (description + config/title/init coexistence) **18**;
config-value admission **9** (show*:false 5, labelRotation 3, full init
config 1); per-point inline label projection **4**; signed-bar data labels
**2**; degenerate zero-width axis **1**; more-than-3 line series **1**.
`fallback_roster.json` assigns all 35 to `family_residual:xychart`
(causality `family-scoped-debt`); there is no cross-family shared blocker to
demote, and `design-decisions.md` records no xychart hard limit.

Prior analysis: `docs/dev/mermaid-d066-zero-fallback-plan.md` (xychart ≤10 F0
inert rows; F0 is a separate, scoped conformance batch), the D-394 selection
spike (`docs/dev/mermaid-next-family-spike-d394.md`, which ranked xychart the
most policy-robust next family), and its adversarial verification, which
corrected three details now reflected below: the
`render-all-the-theme-color` row additionally needs `dataLabelColor` role
admission; `x-axis-range-same-value` was double-counted and is a capability
row; and the envelope relief is three concrete code changes, not one flag.

## Goals

- **G1.** Promote an exact, enumerated reviewed cohort of up to 26 rows
  (envelope-only and config-value rows) where every promoted row is measured
  to draw natively and its frame is pinned by a per-row gate.
- **G2.** Fail closed: any config key, value, duplicate, nesting depth or
  malformed scalar outside the documented whitelist keeps the whole source on
  fallback. No theme/look/layout admission.
- **G3.** No new layout algorithm and no production change beyond the
  description/config dispatch and the two admission whitelists.
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
- Theme/look admission of any kind (D-066 F1 is its own RFC and remains open).
- Chart width/height/font/padding/tick metrics: already validated-and-ignored
  by the terminal projection; not re-litigated here.
- The requirement-family F0 inert-config cohort (landed separately in D-395);
  no shared-envelope generalization beyond the charts dispatch.
- Manifest edits: the manifest SHA is pinned across tests, support headers and
  verifiers (several inside the ten protected files); promotion uses the
  code-side reviewed cohort ledger, never the manifest.
- narrow_40 drawing for xychart: all 48 rows stay narrow-fallback in the
  current renderer; this RFC does not promise narrow geometry.

## Proposal

### 1. Envelope coexistence (the highest-leverage change)

Today the charts dispatch routes a description-bearing envelope into
`pie_description_frontmatter` (`src/ui/visual/mermaid_charts_impl.cpp`
~:460-483), which rejects any top-level key after the folded description
block — including `config`. Independently, `xy_visual_envelope` (~:182)
rejects a top-level `description`. For `xychart-beta` sources only, admit the
closed set `{description, title, config}` together:

- (a) the description path accepts a following top-level `config` whose every
  leaf is already admitted by the existing `xy_visual_envelope` profile;
- (b) the XY path accepts a validated folded `description` event beside
  `config`;
- (c) the pie path keeps its current behavior unchanged (no pie admission is
  proposed here).

Rejection remains exact: a comment, a duplicate key, a top-level key outside
the set, an unclosed fence, a tab indent, or a config leaf outside the
profile all keep the whole source on source fallback.

### 2. Config-value widening

- `show*` flags (`showLabel`, `showTick`, `showTitle`, `showAxisLine`)
  accept `false` as well as `true` (5 rows).
- `labelRotation` is accepted as a **documented, non-applied projection**
  (3 rows): a rotated SVG label has no faithful cell-grid rotation, so the
  projection draws the unrotated label and records the omission in the
  family's projection documentation. This needs owner confirmation (OQ-1).
- `themeVariables.xyChart.dataLabelColor` joins the validated-and-ignored
  color-role list (currently 10 roles; `mermaid_charts_impl.cpp` ~:233-239)
  (1 row; OQ-2).
- A full inline `init` config on an otherwise admitted body reuses the same
  closed validation profile (1 row).

### 3. Reviewed cohort ledger and gates

- Extend the existing `tools/mermaid/e2c-xychart-cohort.json` (or add a
  sibling with the same schema) with the exact promoted row list and
  per-width expected states. The ledger is data, not code; its row list must
  be measured, and every row added to it must be justified individually.
- Wire the matrix through the established cohort-branch pattern
  (`tests/ui/test_mermaid_official.cpp`, same shape as journey/timeline/E2c):
  the matrix records the declared state; the rendering proof lives in the
  per-row cohort gate.
- The per-row gate (`tests/ui/test_mermaid_e2c_xychart_cohort.cpp`) must
  render each promoted fixture through the public interface
  (`render_markdown` on a fenced block) and assert: the frame is native (not
  the ordinary source frame), the frame is deterministic across repeated
  captures, and a boundary set stays fallback (theme/look/unknown values/
  malformed scalars/duplicate keys/empty config).
- Roster and census gates: `fallback_attribution.py --matrix` drift gate
  stays green (roster counts move only through the reviewed roster update);
  mandatory gaps stay 48; integrity errors stay 0.

### 4. Invariants catalog

- Closed key set and closed value set at every config level; duplicate paths
  and depth beyond the existing limit reject.
- No theme/look/layout value is expanded.
- Description and title are inert metadata in this family; no footer is
  added (unlike the requirement F0 disclosure) because the description is
  never a rendered contract for xychart.
- Work metering: every new path spends before it mutates, mirroring the
  existing envelope charges.
- No persisted, wire, settings or session shape is touched.

## Alternatives

- **Full xychart config system including theme/look.** Rejected: overlaps
  D-066 F1 (its own RFC), inflates scope far beyond 26 rows, and re-opens a
  contested policy surface (theme disposition) that xychart does not need for
  this cohort.
- **Capability-first (per-point labels, signed-bar labels, >3 series).**
  Rejected as the first step: only 8 rows, each needs new geometry/collision
  work, and the proven admission path would still be required afterwards.
  Listed as the natural follow-up RFC.
- **Do nothing.** 35 fallbacks remain; rejected because 26 of them are
  gate-only debt with an established, safe admission pattern.
- **Piggyback on the requirement F0 envelope widening.** Rejected: different
  family dispatch and config profile; sharing the code would couple two
  unrelated admission surfaces and re-open the requirement review.

## Risks and open questions

- **OQ-1 (owner decision):** `labelRotation` — accept-and-ignore with a
  documented projection (proposed, consistent with colors/metrics) versus
  implementing true rotation capability. Must be decided before
  `implementable`.
- **OQ-2 (design review):** whether `dataLabelColor` belongs in the
  validated-and-ignored color role list (it is currently rejected, unlike the
  other ten roles).
- **OQ-3 (implementation gate):** the D-394 spike found one composition
  double-count and one cross-dependency. The ledger must enumerate rows
  exactly; any row whose body does not draw after admission moves to the
  capability bucket with its measured reason recorded.
- **OQ-4:** narrow_40 behavior stays source fallback; reviewers must not read
  the wide-column gain as a narrow-column promise.
- **Risk:** over-admission of description+config combinations could admit
  bodies that draw unfaithfully. Mitigation: per-row frame gates plus the
  boundary set, and a full-matrix census before/after with fallback
  non-growth.

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review against the
  SKILL §3.1 checklist (measured evidence, falsifiable goals, alternatives,
  shape hazards, open questions). The owner (TechLead) additionally confirms
  OQ-1.
- `accepted` → `implementable`: agent-run production-readiness review
  (tests, rollback, observability, documentation, roster drift) and a phase
  plan with measurable graduation criteria (drawn/fallback/error counts,
  dual-preset byte equality, roster drift gate).
- Draft implementation phases (non-binding at provisional): P1 envelope
  coexistence + acceptance tests; P2 config-value widening + boundary tests;
  P3 cohort ledger + matrix wiring + census closure.

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-21 | — | Draft created (provisional), no code | — |
