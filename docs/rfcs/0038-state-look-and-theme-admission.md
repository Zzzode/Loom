---
rfc: 38
title: state family look and theme admission
status: implemented
owners: "@Zzzode"
reviewers: ["agent:design-review (aa8df56e34f50b615, r1/r2 REQUEST-CHANGES → r3 APPROVE-TO-ACCEPT)", "agent:prr-review (aa4f19113e92b44ae, r1/r2 REQUEST-CHANGES → r3 APPROVE-TO-IMPLEMENT)"]
created: 2026-11-21
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0038 — state family look and theme admission

## Summary

state (`stateDiagram` v1 + `stateDiagram-v2`) is the family with the largest
*reachable* slice under the D-400 criterion (reachable drawn increment over
cost): **28 of its 93 fallbacks** are blocked only by a frontmatter `config`
block carrying `look` and/or `theme`. This RFC admits both as closed,
corpus-bounded value tables, validated and disclosed through the disclosure
footer that `state_source_envelope` already publishes. Target: **28 rows**
promoted in **one atomic phase** (both leaves: 20 `look`+`theme` rows and 8
theme-only rows), family **80 drawn / 65 fallback**, global **1419 / 726**.

The mechanism is the RFC 0037 one, second application: closed tables, per-row
twin gates, a reviewed cohort ledger, and one generator-authored review record. The state
envelope already accepts `description`, `config` (`logLevel`, `fontFamily`,
`securityLevel`), and `title` (projected through the shared relation-title
seam), so this RFC adds two leaves and two footer lines — no new geometry, no
new module.

## Motivation

### Family selection (D-400, measured 2026-11-21)

TL directive 638 replaced the raw-fallback-count ranking with reachable drawn
increment over cost. A throwaway render probe (`docs/dev/d400-next-family-reachability.md`,
log `/tmp/reach_probe.log`) rendered all 754 fallback rows as-is and with the
whole frontmatter stripped. Results (non-gap strip-draws, then the clean
closed-table subset):

| family | fallbacks | non-gap body draws | clean closed-table slice |
|---|---|---|---|
| flowchart | 240 | 49 | **≥8** (design-review correction: the eight nested/collapsed subgraph rows are blocked only by `theme`; the `@{shape}` and icon-font rows are body-blocked; still fewer rows than state and costlier — a new `Theme` profile with exact-bit machinery) |
| git | 77 | 50 | 12 (`gitGraph.*` rows are ordering/label semantics) |
| **state v2 + v1** | **93** | 37 | **28** (19 v2 + 9 v1) |
| timeline | 23 | 16 | 16 (but no envelope exists) |
| gantt | 13 | 11 | 11 (needs the classic palettes dark/forest/neutral) |
| agentflow | 11 | 11 | 9 + 2 (`handDrawnSeed` needs its own ruling) |

state is selected because it is largest *and* cheapest: the envelope, the
disclosure footer and the title projection already exist. flowchart's real
closed-table slice (**≥8**, per the design-review correction) is smaller and
needs its own exact-bit `Theme` profile, so it stays out of scope.

### Evidence

Measured on the certified matrix (`build/debug/tests/ui/mermaid-official-matrix.json`,
generation `7fd2b117…`, byte-identical across presets at RFC 0037 closure) and
the 754-row `tools/mermaid/fallback_roster.json`.

| Metric | Current | Target | How measured |
|---|---|---|---|
| state originals `wide_240` | 52 drawn / **93 fallback** (v2 33/70, v1 19/23) | 80 / 65 | `MermaidCapabilityReport.OfficialMatrix` census |
| global `wide_240` | 1391 / 754 | 1419 / 726 | same |
| formal `drawn_at_240` | 1205 | 1233 | certification census |
| mandatory_gaps | 48 | 48 unchanged | `e6c-mandatory-gap-roster.json` has **no state rows** (48 flowchart + 25 class) |
| integrity_errors | 0 | 0 | matrix header |

Cohort census (28 rows):

| key | values | rows |
|---|---|---|
| `config.look` | `neo` 16, `handDrawn` 2, `classic` 2 | 20 |
| `config.theme` | `redux` 6, `redux-dark` 6, `redux-color` 3, `redux-dark-color` 3, `neo` 4, `neo-dark` 4, `default` 2 | 28 |
| `config.fontFamily: courier` (already admitted by E2b) | — | 8 (v1 rows combining it with look+theme) |

Composition (design-review correction): all 20 rows that carry `look` **also**
carry `theme`; the remaining 8 v2 rows carry `theme` alone; no row carries
`look` alone. A `look`-first phasing would therefore promote **zero** rows
(every look row would still be rejected on `theme`), so this RFC admits both
leaves in **one atomic phase**. Every cohort row uses the folded (`>-`)
description form the envelope already requires.

### Why admission is testable

`look` and `theme` select upstream stroke style and colour palette. The state
renderer never reads either: the shared relations core consults `theme` only in
the ER source-model seam (`er_src_obligation_effect`,
`er_terminal_model_json`) and the ER paint, never on the state path. The
inertness claim is therefore a per-row frame-identity twin, exactly as in RFC
0037: the frame with the leaf must equal the frame without it plus the bounded
footer.

## Goals

- **G1.** Promote exactly the measured 28-row cohort (20 `look`+`theme`, 8 theme-only).
- **G2.** Fail closed: unknown or out-of-table values, duplicates, deeper
  nesting, a namespaced `state.*` subtree, `layout: elk`, non-folded or
  tab-bearing descriptions and every body-level rejection keep the whole
  source on fallback.
- **G3.** No renderer geometry change: the production edits are two leaves and
  two footer lines inside `state_source_envelope`.
- **G4.** No regression: existing E2b footers byte-identical, fallback never
  grows, errors 0, gaps 48, dual-preset matrix byte-identical.

## Non-Goals

- `config.layout: elk` (4 rows) — a different layout engine.
- `config.state.useMaxWidth` (4 rows) — a nested namespaced key; D-066 F0 keeps
  namespaced subtrees closed in the sibling families and this RFC does not open
  that boundary for four rows.
- The one row whose folded description contains a literal tab (`<br<TAB>/>`,
  in the description payload and the body): its blocker is
  `state_source_envelope`'s description scan rejecting `\t`, not config.
- The 56 remaining body-capability rows (v1 11 + v2 45): they do not draw even
  with the frontmatter stripped.
- Theme values with no state-corpus row, `themeVariables.*`, `handDrawnSeed`,
  `curve`.
- Painting upstream's stroke style or palette. A future policy RFC owns that.
- narrow_40 drawing; the ER107 B3 gates; other families.

## Proposal

### 1. Closed value tables (`look` and `theme`, one phase)

`state_source_envelope` (`src/ui/visual/mermaid_relations_core_impl.cpp`)
gains two leaves beside `logLevel`/`fontFamily`/`securityLevel`, at indent 2
under the single `config:` block, at most once each:

| key | admitted values | rejects |
|---|---|---|
| `look` | `classic`, `neo`, `handDrawn` | any other string, duplicate |
| `theme` | `default`, `neo`, `neo-dark`, `redux`, `redux-dark`, `redux-color`, `redux-dark-color` | any other string, duplicate |

`state_scalar_text` dequotes, so a quoted scalar (`theme: "neo"`) is the same
value and is admitted; the boundary suite pins that, as RFC 0037 did.

Both tables are corpus-bounded, matching the requirement tables and this
renderer's convention. The four upstream palettes without a state row
(`base`, `dark`, `forest`, `neutral`) stay rejected and are listed explicitly.

Disclosure footer: the existing order is header, `fontFamily`, `logLevel`,
`securityLevel` (alphabetical). The new lines go **after** `securityLevel` so
the 61 E2b cohort footers stay byte-identical:

```
State native projection v1                     (existing header text)
config.fontFamily="courier" -> ...             (existing)
config.logLevel=N -> ...                       (existing)
config.securityLevel="loose" -> ...            (existing)
config.look=neo -> source node style; not applied      (new)
config.theme=redux-color -> colour palette; not applied (new)
```

The exact header text is whatever `kStateNoticeHeader` already is; this RFC
does not change it.

### 2. Reviewed cohort, twin gates and wiring

- Ledger `tools/mermaid/state-0038-cohort.json` (schema
  `loom_mermaid_state_0038_cohort/v1`), a **sibling** of `e2b-cohort.json`:
  the E2b ledger stays read-only for this RFC (the RFC 0035 E2c lesson). Its
  28 rows carry the single phase's review record.
- Generator `tools/mermaid/generate_state_0038_cohort.py`: `--check`
  reproducible. **The 28/56 split is render-defined, not shape-defined**: the
  PRR review proved that the existing E2b `state_blocker` detector (calibrated
  on 61 rows) over-selects by 16 when applied to the 63 look/theme rows, so the
  cohort is a **reviewed literal path set** pinned in the generator — exactly
  like RFC 0037's `DRAWN_TWINS`. The generator hard-asserts only the
  **source-derivable** counts — family 145, cohort 28, `layout: elk` 4,
  `state.useMaxWidth` 4, description-tab 1 — because the drawn/body split
  (52 / 56) is render-defined too and cannot be derived from config shape (the
  same reason the 28-row set is pinned rather than classified). The 52/56
  split stays a **measured, recorded** figure (D-400 and the closure record),
  not a generator assertion; the roster is never read. One whole-cohort path+sha digest is the `--check` pin (a per-phase
  split digest and a `look`-before-`theme` priority are vestigial with one
  phase). The review record is authored in the generator.
- **Twin rule (RFC 0037 restated, with the state-specific prefix clause).**
  Corpus twins take precedence; the recorded delta is *source footer lines
  minus twin footer lines*, compared line-by-line against the live frame, so a
  wrong delta is red, not green. When a twin already carries an E2b footer
  (e.g. a `-default` sibling with `htmlLabels` or `fontFamily`), only the lines
  the twin lacks are appended — the header and blank separator are never
  re-added. The 8 v1 rows carrying `fontFamily: courier` + `look` + `theme`
  lose three footer lines at once against their config-free twin and must
  record all three.
- Gate `tests/ui/test_mermaid_state_0038_cohort.cpp`: the same anti-false-green
  assertions as RFC 0037 (twin must draw, native predicate, per-width ledger
  equality, determinism, exact footer composition). The frozen controls stay
  **owned by `MermaidCapabilityReport.OfficialMatrix`** (it pins
  `observations.size() == 2145 + 593` and runs the gap-roster diff gate). The
  new gate does **not** read the matrix artifact: that file is written only at
  `OfficialMatrix` run time, there is no ctest dependency ensuring order, and a
  targeted run would see a missing or stale file (false red). Instead the
  `drawn_at_240` delta (1205 → **1233**) and the gaps/integrity invariants are
  verified in the phase census procedure — release matrix run, numbers recorded
  in the RFC and the commit — exactly as the RFC 0037 phases recorded theirs.
  Work accounting: the two new leaves charge only when present, so the 61 E2b
  rows' budgets do not shrink — the E2b suite is the guard.
- Matrix branch in `tests/ui/test_mermaid_official.cpp` reading per-row
  `expected_states` (Journey style), never the xychart hardcoded triple, with
  the reason string `RFC-0038 state look/theme reviewed cohort`. The E2b branch
  is untouched.
- Roster migrated with `fallback_attribution.py --emit` in the phase commit.

### 3. Named cross-module couplings

| coupling | files that must change together |
|---|---|
| envelope validator and footer | `mermaid_relations_core_impl.cpp` (`state_source_envelope`), the gate, the ledger |
| E2b cohort byte-identity | `tools/mermaid/e2b-cohort.json` and its 61 rows are read-only; **`tests/ui/test_mermaid_e2b_cohort.cpp`** holds the E2b footer/byte assertions and must stay green (the existing footers are a prefix of the new ones) |
| **E2b negative** `NonWhitelistedConfigRowsKeepWholeSource` (`:239-284`) | iterates every `config:`-bearing state fixture outside the E2b ledger (**71** today) and asserts whole source. The phase adds the sibling ledger to its `cohort` set, asserts the excluded set **equals** that ledger, and lowers the floor `EXPECT_GT(checked, 50)` to `EXPECT_EQ(checked, 43)` (71 − 28). The test stays meaningful: the 43 rows cover `layout: elk` (4), `state.useMaxWidth` (4) and 35 look/theme rows whose bodies cannot draw; over-listing fails the new cohort gate, under-listing fails this negative. `theme: dark` is owned by `MermaidStateEnvelope.ConfigFrontmatterStaysSource`, the tab row is an E2b blocked ledger row (owned by the E2a negative and the state-envelope description test), and init-only rows carry no `config:` line so they are outside this scan. The stale `:240` comment is corrected |
| **E2a negative** `MermaidE2aCohort.ConfigAndInitRowsKeepWholeSource` (`tests/ui/test_mermaid_e2a_cohort.cpp:243-275`) | iterates `git` + both state roots with only the E2a ledger and the E2b predicted-draw rows excluded (**160** rows today) and asserts whole source. The 28 cohort rows are in neither set, so the phase extends this exclusion set with the sibling ledger too; its floor `EXPECT_GT(checked, 100)` still holds (132 remain) |
| surviving state negative | `tests/ui/test_mermaid_relations_semantics.cpp` `MermaidStateEnvelope.ConfigFrontmatterStaysSource` asserts `config: theme: dark` stays fallback. It survives **only because `dark` is outside the closed table**; a future widening must revisit it |
| census `expected`/`reason` | `tests/fixtures/mermaid_official/manifest.json` is SHA-pinned and never edited; promotion is a ledger-driven branch |
| frozen partitions | `class481`/`state85` gate records in `e6c-mandatory-gap-roster.json` are untouched; `state85` counts are asserted by their own test |
| reason string | `test_mermaid_official.cpp`, generator, closure doc |

Dependency direction is unchanged.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `look` + `theme` admission | 28 rows (20 `look`+`theme`, 8 theme-only; v2 19, v1 9) | done | state 52→**80** drawn / 93→**65** fallback; global 1391→**1419** / 754→**726**; formal `drawn_at_240` **1205→1233**; gaps 48, integrity 0; E2b suite and **both** negatives (E2a, E2b) green; release matrix sha recorded |

Closure: full debug ctest once, dual-preset byte gate, RFC `implemented`,
closure doc, design-decisions entry, efficiency retro.

## Production Readiness Review

Completed at the `implementable` gate. Current answers:

- **Correctness & tests** — per-row twin gate plus a boundary suite; the gate
  is the inertness proof. Known flakes are pre-existing.
- **Build system** — no new module, target or import; the edit is inside a
  module **implementation unit** (`src/ui/visual/mermaid_relations_core_impl.cpp`
  is a `.cpp` in `module loom.ui.visual.mermaid`), so **BMI PSS is N/A with
  reason** (no interface or BMI change). Fan-out measured by the PRR review
  against `build/release/build.ninja`: **1 C++ compile**, one
  `libloom_ui_visual.a` archive relink and **~206 executable relink edges**
  naming the archive — not a BMI closure, and far below RFC 0037's 509-unit
  interface edit. No concurrency caps; no new textual includes.
- **Rollback** — a single atomic commit (both leaves, 28 rows); one revert
  restores the previous census exactly. No ordering constraint exists because
  the two tables are admitted together.
- **Observability** — the disclosure footer is the user-visible trace.
- **Documentation** — design-decisions entry, closure doc, README index, retro.
- **Deletion** — the E2b comment stating that only `logLevel`/`fontFamily`/
  `securityLevel` are accepted is corrected, along with the stale "E2b admits a
  reviewed set of state inert-config originals" comment at
  `tests/ui/test_mermaid_e2a_cohort.cpp:250-252`, both in the phase that
  invalidates them.
- **Deprecation** — none.
- **Wire / persisted shapes** — N/A: rendering-only.
- **Golden suites** — N/A: assertion-based mermaid cohorts only.
- **Platform readiness** — N/A-with-reason: rendering-only, no termios/signal/
  network/cache change, Linux debug+release only.
- **CLAUDE.md** — no change. **Architecture-graph lint** — N/A: no new
  module/import/edge.

## Rollout and rollback

One phase lands P1. Rollback is one revert. No persisted format changes; the
ledger is additive; the roster is regenerated, never hand edited.

## Drawbacks and risks

- **No drawn corpus sibling for part of the cohort.** The 8 v1 rows and several
  v2 rows may have no body-equivalent drawn fixture, so their faithfulness
  rests on the reviewed renderer contract plus the twin gate, not an external
  comparison. The ledger records which rows use corpus twins.
- **The state renderer's frames are ours, not upstream's.** Admitting `look`
  and `theme` does not make the terminal paint rough strokes or palettes; the
  footer says so per row.
- **E2b byte-identity is load-bearing.** A footer inserted before the existing
  lines would change 61 reviewed frames; the order is pinned by the P1 gate.

## Alternatives considered

- **Do nothing.** 93 state fallbacks remain; 28 are frontmatter-only.
- **Extend `e2b-cohort.json` in place.** Rejected: it is a reviewed ledger with
  its own row semantics (the E2c lesson from RFC 0035). A sibling keeps it
  read-only.
- **Also admit `state.useMaxWidth` and `layout: elk`.** Rejected: four rows
  each, and both are boundary decisions (nested namespace key; layout engine)
  that need their own evidence.
- **Pick timeline (16 rows).** Rejected on cost: timeline has no envelope or
  footer channel; state reuses three existing mechanisms.
- **Pick git (12 rows).** Rejected: the same budget buys 12 rows there versus
  28 here, and git's other 38 rows are semantics, not tables.

## Testing and verification plan

- New `tests/ui/test_mermaid_state_0038_cohort.cpp` (registered in
  `tests/ui/CMakeLists.txt`), expected 5 tests; `ctest --preset debug -N` reports **4622** total, so the
  closure total is `4622 + 5 = 4627` with no deletions (re-derive at phase
  start).
- Edited `tests/ui/test_mermaid_official.cpp` (matrix branch + reason string).
- Targeted `ctest -L mermaid` in the phase; `generate_state_0038_cohort.py
  --check`; release-preset matrix in the phase; dual-preset gate at closure.
- **False-green guardrails.** The matrix cohort branch is a *ledger echo* (it
  takes `status` from the ledger, like Journey), so the census is meaningful
  only after a full build (efficiency rule 3), and the render proof is
  `test_mermaid_state_0038_cohort`, never the echo. The full ctest is
  redirected to a log with `CTEST_EXIT=$?` — never piped through `tail`
  (efficiency rule 7).
- Known flakes pre-existing (`ServerMain.DirectConnect*`, `E2E_Pty_*`).

## Documentation impact

- [ ] `docs/dev/d400-next-family-reachability.md` — already records the
      selection (this RFC is its deliverable)
- [ ] `docs/dev/d401-state-rfc0038-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — state config admission entry
- [ ] `docs/README.md` — RFC index
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: corpus-twin vs synthetic-twin split for the 28 rows | implementation | generator pins the per-kind counts; the gate proves both directions |
| OQ-2: narrow_40 disposition per row under the footer budget | implementation | ledger `expected_states`, measured per row |
| OQ-3: whether the v1 rows' `fontFamily: courier` combination needs its own footer-delta assertion | design review | **Resolved (design review)**: no special case — the standard twin rule already emits `config.fontFamily="courier"` because the config-free twin strips `fontFamily` |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR with a different lens.
- Phases: **one** atomic phase, with its review record.
- Graduation: the phase's targeted census; closure runs full ctest once and
  the dual-preset byte gate.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-21 | — | — | draft | Created provisional from the D-400 reachability measurement |
| 2026-11-21 | r1 | `agent:aa8df56e34f50b615` (independent general-purpose, own render probe + static re-derivation) | REQUEST-CHANGES | **B-1** the phase split was arithmetically impossible: every `look` row also carries `theme`, so a look-first P1 would promote 0 rows → **accepted**: rephased to one atomic phase admitting both leaves for all 28 rows; rollback and the PRR row rewritten. **M-1** the flowchart downgrade overclaimed "0 reachable": the eight nested/collapsed subgraph rows are blocked only by `theme` (their bodies render with `config:` removed), only the `@{shape}`/icon rows are body-blocked → **accepted**: RFC and D-400 now state **≥8**, and the downgrade reason is "fewer rows + higher unit cost (new exact-bit `Theme` profile)", not "body capability". **M-2** a pre-existing negative test was missed: `test_mermaid_e2b_cohort.cpp::NonWhitelistedConfigRowsKeepWholeSource` iterates every config-bearing state fixture and asserts fallback, so all 28 rows would break it → **accepted**: added to the coupling table together with the correct owner of the E2b footer assertions and the stale `:240` comment. **M-3** the theme value census summed to 22, not 28 → **accepted**: corrected to `redux 6, redux-dark 6, redux-color 3, redux-dark-color 3, neo 4, neo-dark 4, default 2`. MINOR m-1 body count 55→56; m-2 rollback dependency direction dropped with the rephase; m-3 the surviving `theme: dark` negative named (it stays green only because `dark` is excluded). NIT n-1 OQ-3 closed by the review (the standard twin rule already covers `fontFamily`); n-2 tab wording made precise (a literal tab in the payload and body, rejected by the description scan). Independently verified: the 28-row cohort and its composition, the 145 partition, the matrix census, the envelope/footer facts and the inertness claim. |
| 2026-11-21 | r2 | same design-review agent, independent re-read | REQUEST-CHANGES | All substantive r1 findings verified fixed. **MAJOR-1** residual two-phase wording (G1, the §1 heading, the footer tags and the Gates line that specifies review-record cardinality) → **accepted**: everything now describes one atomic phase and one review record; the generator spec keeps a single whole-cohort digest and notes the per-phase split digest and priority rule as vestigial. **MINOR-1** D-400 §3 still titled "0 行可达" → **accepted**: retitled "≥8 行可达，性价比低" and the intro now points at the eight corrected rows. **MINOR-2** plural "per-phase/each phase" phrasing → **accepted**: singularized. **NIT-1** → **accepted**: recorded in the generator spec. Review also confirmed that one phase does not weaken the review-record safety mechanism (it degenerates to exactly one generator-authored approval plus the `--check` hand-edit guard; only cross-phase anti-drift is lost, which cannot occur with no partial state), and that the midpoint arithmetic left no residue. |
| 2026-11-21 | r3 | same design-review agent, final consistency pass | APPROVE-TO-ACCEPT | All r2 items verified fixed: one-phase wording, endpoint arithmetic, the 5-test count, the theme census, the named e2b negative and the closed OQ-3. Two cosmetic NITs noted (the phase-table label "P1"; a plural "review records"), the second folded in. Gate to `accepted` passed; no owner ruling required. |
| 2026-11-21 | prr-r1 | `agent:aa4f19113e92b44ae` (independent production-readiness lens; static census re-derivation, build-graph fan-out measurement, negative-test count re-derivation, false-green audit) | REQUEST-CHANGES | **MAJOR-1** a second existing negative was missed: `MermaidE2aCohort.ConfigAndInitRowsKeepWholeSource` covers both state roots and excludes only the E2a/E2b ledgers, so all 28 rows would break it → **accepted**: named in the coupling table with its exclusion-set extension (its `>100` floor still holds at 132). **MAJOR-2** the E2b negative's floor breaks after the (correct) exclusion: 71 − 28 = 43 against `EXPECT_GT(checked, 50)` → **accepted**: the phase asserts the excluded set equals the sibling ledger and pins `EXPECT_EQ(checked, 43)`. **MAJOR-3** the renderer-free generator cannot derive the 28/56 split (the E2b detector over-selects by 16) → **accepted**: the cohort is a reviewed literal path set with a whole-cohort path+sha `--check` pin and three hard-asserted counts. **MAJOR-4** the build-system row deferred its measurement → **accepted**: measured fan-out (1 compile, archive relink, ~206 executable relinks) and BMI-PSS N/A-with-reason recorded; "BMI closure" corrected. MINOR-1 ctest total 4622 → closure 4627; MINOR-2 the README index's two-phase wording corrected now; MINOR-3 false-green guardrails (full build, log capture, the cohort gate is the render proof and the matrix branch is an echo); MINOR-4 frozen-control ownership assigned to `OfficialMatrix` with the report-dir read. NIT-1 quoted scalars pinned in the boundary suite; NIT-2 the new leaves' Work charges made conditional. The twin rule now restates corpus-twin precedence and the footer-prefix clause. |
| 2026-11-21 | prr-r2 | same PRR agent, independent re-read | REQUEST-CHANGES | MAJOR-1/2/4, MINOR-1..4 and both NITs verified fixed, with the 71/43, 160/132 and 145 partitions re-derived. **MAJOR-A** the generator still asserted the render-defined 52/56 split → **accepted**: the generator now hard-asserts only source-derivable counts (145, 28, 4, 4, 1) and the 52/56 split is a recorded measurement, not an assertion. **MINOR-B** the new gate's matrix-file read had no ordering guarantee (the artifact exists only after `OfficialMatrix` runs) → **accepted**: the read is removed; the `drawn_at_240` delta (1205→1233) and the gaps/integrity invariants are verified in the phase census procedure, and the frozen controls stay owned by `OfficialMatrix`. **MINOR-C** the "stays meaningful" enumeration overclaimed (the 43 rows do not include `theme: dark`, the tab row or init-only rows) → **accepted**: corrected and re-attributed to their owners. NIT-A `drawn_at_240` baseline stated (1205→1233); NIT-B the PRR agent added to `reviewers:`; NIT-C the E2a stale comment folded into the Deletion row. |
| 2026-11-21 | prr-r3 | same PRR agent, final pass | APPROVE-TO-IMPLEMENT | MAJOR-A, MINOR-B and MINOR-C verified genuinely fixed (source-derivable counts only; no matrix-artifact read; corrected 43-row coverage), NIT-A/B/C verified; counts reconcile end to end and every PRR checklist row is answerable from the RFC text. Gate to `implementable` passed. |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-21 | — | Draft created (provisional), no code | — |
| 2026-11-22 | P1 | Both closed tables land in `state_source_envelope` (two leaves, two disclosure lines appended after every existing leaf); reviewed 28-row literal cohort with one whole-cohort digest; generator + sibling ledger + 5-test twin gate; matrix branch; E2a/E2b negatives extended (43 / 132 floors); roster `--emit` 754->726. Family 52/93 -> 80/65, global 1391/754 -> 1419/726, formal `drawn_at_240` 1205 -> 1233, gaps 48, integrity 0, dual-preset sha `59cf7de8`. Reviewed by `agent:acde9271b5b1810a9` (APPROVE, 2 MINOR + 2 NIT applied test-only) | `648d7035` |
| 2026-11-22 | — | RFC marked `implemented`: the single phase is done, full debug ctest run (4627 declared; two load-timing flakes, both green in isolation), dual-preset byte gate identical, closure record `docs/dev/d401-state-rfc0038-closure.md` | — |
