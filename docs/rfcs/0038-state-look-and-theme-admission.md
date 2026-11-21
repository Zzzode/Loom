---
rfc: 38
title: state family look and theme admission
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-11-21
last-reviewed: 2026-11-21
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
promoted in two phases (P1 `look` 20, P2 `theme` 8 more), family
**80 drawn / 65 fallback**, global **1419 / 726**.

The mechanism is the RFC 0037 one, second application: closed tables, per-row
twin gates, a reviewed cohort ledger, per-phase review records. The state
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
| flowchart | 240 | 49 | **0** (all 16 look/theme-only rows carry subgraph nesting, collapsed `@{view}`, `@{shape}` or icon-font bodies) |
| git | 77 | 50 | 12 (`gitGraph.*` rows are ordering/label semantics) |
| **state v2 + v1** | **93** | 37 | **28** (19 v2 + 9 v1) |
| timeline | 23 | 16 | 16 (but no envelope exists) |
| gantt | 13 | 11 | 11 (needs the classic palettes dark/forest/neutral) |
| agentflow | 11 | 11 | 9 + 2 (`handDrawnSeed` needs its own ruling) |

state is selected because it is largest *and* cheapest: the envelope, the
disclosure footer and the title projection already exist.

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
| `config.theme` | `redux` 4, `redux-dark` 4, `redux-color` 3, `redux-dark-color` 3, `neo` 3, `neo-dark` 3, `default` 2 (v2 19 + v1 9) | 28 |
| `config.fontFamily: courier` (already admitted by E2b) | — | 8 (v1 rows combining it with look+theme) |

Phase split: **P1 `look` 20** (11 v2 + 9 v1 rows carrying `look`), **P2 `theme`
8** (the remaining v2 rows carrying `theme` alone). Every cohort row uses the
folded (`>-`) description form the envelope already requires.

### Why admission is testable

`look` and `theme` select upstream stroke style and colour palette. The state
renderer never reads either: the shared relations core consults `theme` only in
the ER source-model seam (`er_src_obligation_effect`,
`er_terminal_model_json`) and the ER paint, never on the state path. The
inertness claim is therefore a per-row frame-identity twin, exactly as in RFC
0037: the frame with the leaf must equal the frame without it plus the bounded
footer.

## Goals

- **G1.** Promote exactly the measured 28-row cohort (P1 20, P2 8).
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
- The one row whose folded description contains a tab (`<br\t/>`): its blocker
  is the description grammar, not config.
- The 55 remaining body-capability rows (v1 11 + v2 45): they do not draw even
  with the frontmatter stripped.
- Theme values with no state-corpus row, `themeVariables.*`, `handDrawnSeed`,
  `curve`.
- Painting upstream's stroke style or palette. A future policy RFC owns that.
- narrow_40 drawing; the ER107 B3 gates; other families.

## Proposal

### 1. Closed value tables (P1 `look`, P2 `theme`)

`state_source_envelope` (`src/ui/visual/mermaid_relations_core_impl.cpp`)
gains two leaves beside `logLevel`/`fontFamily`/`securityLevel`, at indent 2
under the single `config:` block, at most once each:

| key | admitted values | rejects |
|---|---|---|
| `look` | `classic`, `neo`, `handDrawn` | any other string, duplicate |
| `theme` | `default`, `neo`, `neo-dark`, `redux`, `redux-dark`, `redux-color`, `redux-dark-color` | any other string, duplicate |

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
config.look=neo -> source node style; not applied      (new, P1)
config.theme=redux-color -> colour palette; not applied (new, P2)
```

The exact header text is whatever `kStateNoticeHeader` already is; this RFC
does not change it.

### 2. Reviewed cohort, twin gates and wiring

- Ledger `tools/mermaid/state-0038-cohort.json` (schema
  `loom_mermaid_state_0038_cohort/v1`), a **sibling** of `e2b-cohort.json`:
  the E2b ledger stays read-only for this RFC (the RFC 0035 E2c lesson).
- Generator `tools/mermaid/generate_state_0038_cohort.py`: renderer-free,
  `--check` reproducible, population = the whole state family by source shape
  (never the shrinking roster), `ENABLED_PHASES`, classification priority
  `look` before `theme` (identical to RFC 0037; no title rows exist here),
  per-phase path+sha digests, review records authored in the generator,
  denominators `145 = 52 drawn + 28 cohort + 4 elk + 4 useMaxWidth + 1
  description-tab + 56 body` pinned.
- Gate `tests/ui/test_mermaid_state_0038_cohort.cpp`: the same six
  anti-false-green assertions as RFC 0037 (twin must draw, native predicate,
  per-width ledger equality, determinism, exact footer composition, frozen
  controls: gaps 48, integrity empty, `drawn_at_240` delta equals the phase
  count, `original_files_executed == 2145`). Twin kinds: corpus twins where a
  body-equivalent drawn sibling exists, synthetic config-free twins otherwise
  (counts fixed by the generator at implementation).
- Matrix branch in `tests/ui/test_mermaid_official.cpp` reading per-row
  `expected_states` (Journey style), never the xychart hardcoded triple, with
  the reason string `RFC-0038 state look/theme reviewed cohort`. The E2b branch
  is untouched.
- Roster migrated with `fallback_attribution.py --emit` in each phase commit.

### 3. Named cross-module couplings

| coupling | files that must change together |
|---|---|
| envelope validator and footer | `mermaid_relations_core_impl.cpp` (`state_source_envelope`), the gate, the ledger |
| E2b cohort byte-identity | `tools/mermaid/e2b-cohort.json` and its 61 rows are read-only; `test_mermaid_relations_semantics.cpp`'s E2b assertions must stay green (the existing footers are a prefix of the new ones) |
| census `expected`/`reason` | `tests/fixtures/mermaid_official/manifest.json` is SHA-pinned and never edited; promotion is a ledger-driven branch |
| frozen partitions | `class481`/`state85` gate records in `e6c-mandatory-gap-roster.json` are untouched; `state85` counts are asserted by their own test |
| reason string | `test_mermaid_official.cpp`, generator, closure doc |

Dependency direction is unchanged.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `look` admission | 20 rows (11 v2 + 9 v1) | proposed | state 52→72 drawn / 93→73 fallback; global 1391→1411 / 754→734; formal 1205→1225; E2b suite green; release matrix sha recorded |
| P2 | `theme` admission | 8 rows (v2 theme-only) | proposed | state 72→80 / 73→65; global 1411→1419 / 734→726; formal 1225→1233; same gate set |

Closure: full debug ctest once, dual-preset byte gate, RFC `implemented`,
closure doc, design-decisions entry, efficiency retro.

## Production Readiness Review

Completed at the `implementable` gate. Current answers:

- **Correctness & tests** — per-row twin gate plus a boundary suite; the gate
  is the inertness proof. Known flakes are pre-existing.
- **Build system** — no new module or target; the edit is inside an existing
  non-exported partition function; fan-out is the existing
  `mermaid_relations_core` closure (to be measured at the PRR gate).
- **Rollback** — one atomic commit per phase; LIFO (P1 is a prerequisite of P2,
  since every P2 row carries `theme` only after its `look` siblings land).
- **Observability** — the disclosure footer is the user-visible trace.
- **Documentation** — design-decisions entry, closure doc, README index, retro.
- **Deletion** — the E2b comment stating that only `logLevel`/`fontFamily`/
  `securityLevel` are accepted is corrected in the phase that invalidates it.
- **Deprecation** — none.
- **Wire / persisted shapes** — N/A: rendering-only.
- **Golden suites** — N/A: assertion-based mermaid cohorts only.
- **Platform readiness** — N/A-with-reason: rendering-only, no termios/signal/
  network/cache change, Linux debug+release only.
- **CLAUDE.md** — no change. **Architecture-graph lint** — N/A: no new
  module/import/edge.

## Rollout and rollback

Phases land P1 → P2. Rollback is LIFO. No persisted format changes; the ledger
is additive; the roster is regenerated, never hand edited.

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
  `tests/ui/CMakeLists.txt`), expected 5 tests; full ctest baseline 4620 +
  those tests, no deletions.
- Edited `tests/ui/test_mermaid_official.cpp` (matrix branch + reason string).
- Targeted `ctest -L mermaid` per phase; `generate_state_0038_cohort.py
  --check`; release-preset matrix per phase; dual-preset gate at closure.
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
| OQ-3: whether the v1 rows' `fontFamily: courier` combination needs its own footer delta assertion | design review | design review verdict |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR with a different lens.
- Phases: P1, P2, each atomic with its own review record.
- Graduation: per-phase targeted census; closure runs full ctest once and the
  dual-preset byte gate.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-21 | — | — | draft | Created provisional from the D-400 reachability measurement |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-21 | — | Draft created (provisional), no code | — |
