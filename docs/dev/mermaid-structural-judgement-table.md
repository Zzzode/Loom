# Mermaid fallback structural judgement table (D-464, 2026-11-24)

Purpose: one auditable verdict per large fallback category, so a category that
is not being worked right now still carries evidence instead of a blank.
Baseline: 376 fallback rows (`tools/mermaid/fallback_roster.json`, 26
categories) at commit `14ac3184`.

Method (all of it reproducible):

- **Category composition** — `paths_by_category` from the emitted roster.
- **Capability probe** — every one of the 376 rows was rendered through the
  production path with a 500M Work budget (`/tmp/d461/bigwork-all.log`,
  probe built from `render_mermaid_with_work`). A row that draws only under
  the larger budget has a *budget* blocker, not a semantic one; 18 fallback
  rows land in that class.
- **Blocker isolation** — for a row suspected to be blocked only by a
  frontmatter key, the body was rendered with the frontmatter stripped
  (probes inline below).
- **Contract evidence** — `docs/dev/d439-d441-batch-closure.md` §4 (Flow400),
  the D-438 sankey dispatch record, and the swimlanes hard-wall record.

Classification: **C** = contract-frozen (protected milestone; drawing is
forbidden before re-baseline/authorization), **S** = structurally
inexpressible in a terminal cell grid, **I** = implementable with an
identified, sized change, **D** = dispatch/authorization gap.

## The eight largest categories

| Category | Rows | Composition | Verdict | Evidence | Fallback path | Change surface if pursued |
|---|---:|---|---|---|---|---|
| `edge_animation_metadata` | 63 | flowchart 55, swimlanes 8 | **C** (55) + **S** (8) | Flow400: paragraph under `flowchart/` are pending-model rows; drawing one fails `flow_whole_null_unmet` (8 errors, gaps 48→57, d439 §4). Swimlanes are the htmlLabels+useMaxWidth hard wall (46/46 rows, unsupported diagram type). 2 flowchart rows draw under a 500M budget but stay pending. | whole source, reviewed | Flow400 re-baseline (protected, TL decision) / swimlanes are a new renderer |
| `htmllabels_false` | 36 | flowchart 9, swimlanes 25, er-diagram 1, mindmap 1 | **C** (34) + **S** (1) + **D** (1) | The ER row's body draws with the frontmatter stripped, but the config-bearing presentation profile enforces an explicitly reviewed ASCII-only label contract (`er_project_fields` / `er_projection_wrap`; "the source authority does not establish arbitrary Unicode/Markdown behavior"), and this row's labels carry emoji/markdown emphasis. Admitting it is a contract decision on that profile, not a config-leaf addition. Mindmap `6-should-centre-svg-text-labels…` fails on the body (shape/text centring). | whole source | ER: decision on the presentation profile's label surface (extend it, or render neutral profiles on the plain path + footer) |
| `securitylevel_directive` | 34 | flowchart 34 | **C** | Flow400 pending rows; 1 row draws under a 500M budget but remains contract-blocked. | whole source | Flow400 re-baseline |
| `subgraph_nesting` | 30 | flowchart 28, conf-and-directives 2 | **C** (28) + **D** (2) | Flow400 (28): 14 draw with the frontmatter stripped but are refused only by the D-440 palette / dagre-compound / title-margin tables, and every `flowchart/` fixture is one of the contract's 400 sources. The 2 `conf-and-directives` rows carry real `graph TD` bodies and fail on a shape re-declaration conflict (`A(Start)` then `A[/Another/]`), a precedence decision in the shared parser, not a directive-only document. Evidence: `docs/dev/d469-subgraph-nesting-evidence.md`. | whole source | Flow400 re-baseline / shape-precedence decision |
| `icon_font_glyphs` | 27 (28 before D-466) | flowchart 27 | **C** (27) | Flow400. The tree-view row that combined `:::class`, `icon(fa:bell)` and a `## description` on one node was admitted by D-466 (the class token is dropped and disclosed; the suffix text stays verbatim). | whole source | Flow400 re-baseline only |
| `look_handdrawn` | 28 | flowchart 19, swimlanes 6, er-diagram 3 | **C** (25) + **D** (3) | Flow400 (19) + swimlanes (6). All 3 ER rows draw with the *entire* frontmatter stripped, but the blocker is again the presentation profile's ASCII-only label contract (these rows carry emoji/markdown/`<br/>` labels), not the `look: handDrawn` leaf itself: a synthetic ASCII body with `look: handDrawn` draws today. A cell grid has no sketch stroke, so the look is inert with disclosure (agentflow D-442 precedent); the open question is the profile's label surface. | whole source | ER: decide the presentation label contract, then the `look` leaf is inert + disclosed |
| `family_residual:sankey` | 17 | sankey 17 | **D** | The classifier dispatches only `sankey-beta`; these bodies use the bare `sankey` header. The shelved patch (`/tmp/d438-sankey-config.patch`) widens the shared predicate; D-438 was a NO-GO pending a TL decision because it changes shared dispatch for a foreign family. | whole source | shared `classify_mermaid` predicate (small diff, shared layer → authorization) |
| `click_interactive_callback` | 16 | flowchart 16 | **C** | Flow400 pending rows; stripping every `click` line still leaves them fallback (probe), so the binding is not even the operative blocker. | whole source | Flow400 re-baseline |

Totals over the eight categories (251 rows after D-466): **C 219 / S 11 /
I 0 / D 21** (63+36+34+30+27+28+17+16 = 251; C = 55+34+34+28+27+25+16,
S = 8+1+2, D = 17 + 3 + 1 (sankey + the ER presentation-profile rows)). The
remaining 125 rows of the 376 baseline sit below the eight largest categories;
the live roster is 345 after D-463's eight, D-464's five, D-465's two,
D-466's one, D-467's nine and D-468's six admissions.

## Implementable-now block (evidence: budget / config probes)

| Family / block | Rows | Blocker | Mechanism | Size |
|---|---:|---|---|---|
| git >8-branch + cherry-pick | 8 | fixed 1M Work primary | bounded canvas-derived git Work envelope (state/ER envelope precedent), primary-first, fail-closed, inert when unused | **done, D-463** (4a4cc36b) |
| ER `look: handDrawn` | 3 | presentation-profile label contract (decision needed) | inert look + disclosure, art = classic twin; per-row admission + twin check | medium (contract decision first) |
| ER `htmlLabels: false` | 1 | presentation-profile label contract (decision needed) | inert leaf + disclosure | medium (contract decision first) |
| tree-view combined annotations | 1 | class placement + suffix form | grammar admission, class token dropped + disclosed; suffix text verbatim | **done, D-466** |

13 rows: 9 admitted (D-463's eight + D-466's tree-view row), 4 open (ER,
decision-gated on the presentation profile's label contract).

## Categories below the top eight (summary)

- `family_residual:flowchart` 13, `special_link_markers` 16 (er-diagram 4,
  flowchart 9, swimlanes 3), `look_neo` 8 (mindmap 1, state 4, swimlanes 3),
  `linkstyle_directive` 12, `classdef_styling` 7, `dagre_layout_tuning` 7,
  `multiline_label` 7, `elk_layout_engine` 5: flowchart-dominated (Flow400)
  plus ER/Class/state body-style rows whose evidence lives in the per-family
  ledgers. Ten rows in this band *do* draw under a 500M budget and are still fallback under the
  fixed primary, so they are still blocked (see the budget note below):
  `edge_animation_metadata` 2, `dagre_layout_tuning` 2,
  `subgraph_nesting` 2, `family_residual:flowchart` 2, `icon_font_glyphs` 1,
  `securitylevel_directive` 1 — all flowchart-family rows under the Flow400
  contract, so the budget is a second blocker, not the operative one.
- `family_residual:xychart` 10: 3 label-rotation rows are the frozen xychart
  rotation debt; the point-label rows need a definition-domain decision.
- `family_residual:mindmap` 0 (D-464's five shape/`::icon` rows plus D-465's
  markdown-string/escaped-break pair close the family body; D-468 admits the
  six closed look/theme rows; the 3 remaining mindmap rows are htmllabels 1,
  look_neo 1 (`depth-colors`) and multiline-label 1). D-465 also
  showed that the wide row was *both* a caption-grammar gap and a Work-budget
  gap: the shaped canvas exhausted the fixed 1M primary, so the prefix bisect
  and the canvas envelope belong together.
- `family_residual:git` 2 (after D-463's eight and D-467's nine; the git
  family total is 3 with the `multiline_label` row): the two TB/BT nine-branch
  capacity rows (29, 64; 276 columns, over the width cap) plus the one
  multi-line quoted branch label (97). D-467 showed that the
  rotate/horizontal and themeVariables rows were init-directive grammar debt
  (multi-line, single-quoted, one trailing comma), not structural; rotation is
  disclosed as always-horizontal, and row 57 draws once its trailing comma is
  dropped.

- `family_residual:gantt` 2: invalid-date issue fixtures; upstream's own
  error path, fallback is the honest projection.
- `family_residual:katex` 2 / `zenuml` 1 / `conf-and-directives` 2:
  directive-only or foreign engines with no terminal model.
- `family_residual:requirement` 1 / `state-diagram` 1: single rows tracked in
  their family ledgers.

## Notes for reuse

- **Derived-container lesson (D-462).** When content is wider than its
  container, first check whether the container dimension is *derived* rather
  than authored: the state composite "could not fit" its caption only because
  `layout_scope` never counted captions. `label_place` needs
  `caption width + 4` free columns, so `width = caption width + 6` (borders)
  and every fitting scope stayed byte-identical.
- **Budget probe first.** Before designing renderer work for a fallback row,
  render it under a 500M Work budget: 18 of 376 rows changed disposition,
  which separates "budget" from "semantics" in one pass.
