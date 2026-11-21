---
rfc: 36
title: flowchart inert-input admission (theme colors, tuning keys, style statements)
status: withdrawn
owners: "@Zzzode"
reviewers: []
created: 2026-11-21
last-reviewed: 2026-11-21
supersedes: ""
tracking: local
---

# RFC 0036 — flowchart inert-input admission

## Summary

flowchart is the largest remaining fallback family: **240 of 874** fallbacks
(27%), selected by the mechanism in `docs/dev/d397-flowchart-family-selection.md`.
Measured with a throwaway render probe, its blockers split into config
frontmatter 67, body capability 82, annotation metadata 37–65, style
statements 41 (13 stand-alone), icon fonts 5, line breaks 8 and 2 residual.

This RFC admits only the **inert input classes**: (A) flowchart YAML config
keys that the terminal validates and ignores (color themes and tuning
metrics), and (B) body statements whose only effect is styling or
interactivity (`classDef`/`class`/`style`/`linkStyle`/`click`) plus
`%%` comment lines and `@{ animation }` edge annotations. Every geometry-
changing key stays closed: `look` (hand-drawn strokes), `curve` (edge
routing), `elk` (layout engine), `@{ shape }`, collapsed `@{ view }`, icon
fonts and `<br>` multiline labels. Target: the exact measured row set, up to
~40, recorded in a reviewed cohort ledger per phase.

Status is **withdrawn** (2026-11-21): the independent design review falsified
the blocker composition this draft was built on — 18 of the 67 config rows are
frozen E6c mandatory-gap fixtures, styling is already projected by RFC 0031
FS-3a, and the body slice is style-grammar/init-directive debt. The corrected
measurement and the three paths forward are recorded in
`docs/dev/d397-flowchart-family-selection.md` §5. The number is kept and never
reused.

## Motivation

### Evidence

Measured 2026-11-21 on the certified matrix
`build/debug/tests/ui/mermaid-official-matrix.json` (generation `ee75f003…`,
byte-identical across presets) and the 874-row
`tools/mermaid/fallback_roster.json`.

| Metric | Current | Target | How measured |
|---|---|---|---|
| flowchart original wide_240 | 160 drawn / **240 fallback** / 0 error | up to ~40 promoted; fallback does not grow | `MermaidCapabilityReport.OfficialMatrix` census |
| global wide_240 | 1271 / 874 | matches the flowchart delta | same |
| mandatory_gaps | 48 (flowchart body_diagnostic only) | 48 unchanged | `tools/mermaid/e6c-mandatory-gap-roster.json` |
| integrity_errors | 0 | 0 | matrix header |

Blocker composition (probe evidence under `/tmp/flow_probe*.log`, method in
D-397 §2): config frontmatter 67 — of which color-theme/tuning-only **25**
(+4 subgraph-title-margin rows = 29), `look` 26, `handDrawnSeed` 2, elk 10;
body capability 82; annotation metadata 37–65 (animation inert, shape/view
capability); style statements 41 (13 stand-alone); icon fonts 5; `<br>` 8.

The policy-free body slice measured **13** rows (styles + animation +
comments). The color-theme/tuning slice measures **29** candidate rows by
static key audit. The two sets are disjoint by construction (a row needing
both would not draw under either alone).

### Why this scope

`look: handDrawn` changes stroke geometry and `curve` changes edge routing;
admitting them would book unimplemented geometry as drawn (the OQ-1 ruling in
RFC 0035 applies). Color themes and tuning metrics are the same class as the
eleven validated-and-ignored color roles: upstream applies them in SVG, the
terminal never projects color. Styling statements and `click` bindings are
static-output-inert in a terminal.

## Goals

- **G1.** Promote an exact, enumerated reviewed cohort of the inert-input
  rows (up to ~40), pinned by a per-row frame gate.
- **G2.** Fail closed: any geometry-changing key (`look`, `curve`, `elk`,
  `@{shape}`, `@{view}`), any unknown key or value, duplicate keys, depth and
  malformed scalars keep the whole source on fallback.
- **G3.** No renderer geometry change: the only production changes are a
  flowchart config validator and inert-statement/annotation skipping in the
  existing flowchart parser.
- **G4.** No regression: fallback does not grow, errors stay 0, mandatory gaps
  stay 48, dual-preset matrix byte-identical.

## Non-Goals

- `look: handDrawn`/`neo` and `handDrawnSeed` (geometry).
- `curve` values and `layout: elk`/`elk.*` (edge routing / layout engine).
- `@{ shape: ... }`, `@{ view: collapsed }`, icon fonts (`fa:fa-*`) and
  `<br>` multiline labels (capability).
- Subgraph-nesting, special-link-marker and other body capability rows (82).
- narrow_40 drawing for any family.
- The three ER107 fail-closed gates; theme/look policy beyond color-only
  admission (a future F1 RFC).

## Proposal

### 1. Flowchart config admission (phase P1)

A flowchart source may carry YAML frontmatter whose `config` block validates
against a closed leaf table:

- `theme`: any string value is accepted **only as a color palette name** and
  never applied; unknown theme names are still rejected if they also carry
  unknown keys. `themeVariables.*` color roles accept the existing color
  grammar (validated-and-ignored).
- `securityLevel` (`strict`/`loose`/`sandbox`/`antiscript`), `fontFamily`,
  `htmlLabels` (top level and `flowchart.htmlLabels`), `useMaxWidth`,
  `wrappingWidth`, `minNodeWidth`, `nodeSpacing`, `rankSpacing`, `padding`,
  `diagramPadding`, `markdownAutoWrap`, `subGraphTitleMargin.top/bottom`,
  `titleTopMargin`: validated-and-ignored metrics/policy.
- `layout: dagre` is accepted (the terminal's own layout); any other value
  rejects.
- Everything else rejects: `look`, `handDrawnSeed`, `curve`, `elk`, unknown
  keys, duplicates, tabs, CRLF, non-scalar values, depth beyond one level.

The description frontmatter key follows the RFC 0035 grammar (inert,
never rendered); a description may coexist with the admitted config.

### 2. Inert body statements (phase P2)

The flowchart parser skips, without semantic effect:

- `classDef`, `class`, `style`, `linkStyle` statements (styling is not
  projected);
- `click` bindings (interactivity is not projected);
- `%%` comment lines anywhere in the body;
- `@{ animation: ... }` edge annotations (temporal presentation).

Malformed forms of these statements keep the whole source; `@{ shape }`,
`@{ view }` and every other annotation kind reject.

### 3. Reviewed cohort and gates

- Ledger `tools/mermaid/flow-inert-cohort.json` (schema
  `loom_mermaid_flow_inert_cohort/v1`) with per-phase review records and a
  generator `tools/mermaid/generate_flow_inert_cohort.py` (`--check`
  reproducible).
- Per-row gate `tests/ui/test_mermaid_flow_inert_cohort.cpp`: native frame,
  determinism, source-hallmark absence, twin equality (styling/annotation
  removal must not change the frame), and the fail-closed boundary set.
- Matrix branch after the xychart branch; roster migrated with
  `fallback_attribution.py --emit` in each phase commit.
- Each phase is one atomic commit: code + generator + ledger + matrix branch
  + gates + roster.

## Alternatives

- **Admit `look`/`curve`/`elk` now.** Rejected: geometry is not implemented;
  it would repeat the RFC 0035 r1 BLOCKER class.
- **Capability-first (subgraph nesting, icons).** Rejected: 82 rows, each
  needs real geometry; the inert admission is a prerequisite either way.
- **Do nothing.** 240 fallbacks remain; rejected because ~40 are inert-input
  debt with an established pattern.
- **Switch family.** Rejected by the D-397 measurement: no other family has a
  larger policy-free bucket.

## Risks and open questions

- **OQ-1 (design review):** whether `themeVariables` color roles need a
  closed role list (as in xychart) or the flowchart renderer already owns
  one. Owner: design review.
- **OQ-2 (implementation):** whether the 13 body rows include malformed
  statement forms that must stay closed; the per-row gate enumerates.
- **Risk:** color-only theme admission could be read as a color promise.
  Mitigation: no frame may change; twin gates prove styling is inert.
- **Risk:** `%%` comment lines inside labels vs statements. Mitigation: only
  whole-line comments are skipped.

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review (SKILL
  §3.1).
- `accepted` → `implementable`: independent PRR with a different lens.
- Phases: P1 config admission, P2 inert body statements; each atomic.
- Graduation: targeted flowchart census per phase; family closure runs full
  ctest once and the dual-preset byte gate.

## Rationale

Withdrawn on 2026-11-21 after the independent design review:

- 18 of the 67 config-frontmatter rows are frozen E6c `official_matrix`
  mandatory-gap fixtures whose bodies render wrong geometry, so they may not
  be promoted; excluding them and the geometry-bound remainders leaves no
  clean config cohort beyond `theme`/`securityLevel: sandbox`, whose rows all
  carry hidden body capabilities.
- `classDef`/`class`/`style` are already projected to terminal cells by RFC
  0031 FS-3a, so the "inert styling statements" phase would change drawn
  frames rather than skip inert input.
- The 13-row body slice is style-grammar and `%%{init}%%` directive debt, not
  comments or `@{animation}`.

The corrected measurement and the three paths forward are recorded in
`docs/dev/d397-flowchart-family-selection.md` §5.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-21 | — | — | draft | Created provisional from the D-397 measurement |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-21 | — | Draft created (provisional), no code | — |
