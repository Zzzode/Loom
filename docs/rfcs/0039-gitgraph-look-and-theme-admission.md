---
rfc: 39
title: gitGraph look and theme admission
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0039 — gitGraph look and theme admission

## Summary

The git family has the best reachable-increment-per-cost slice among the remaining
families under the D-402 criterion: **12 of its 77 fallbacks** are blocked only by a
frontmatter `config` block carrying `look: neo` and one of six palettes. This RFC admits
those two keys as closed, corpus-bounded tables in the existing `git_frontmatter_body`
envelope, validates them, and discloses them through a footer appended below the art —
the same mechanism RFC 0037 and RFC 0038 used for requirement and state, third
application. Target: **12 rows** promoted in **one atomic phase**: git **58 → 70** drawn
/ **77 → 65** fallback, global **1419 / 726 → 1431 / 714**.

## Motivation

### Family selection (D-402, measured 2026-11-22)

TL directive 638 requires selecting the next family by **reachable drawn increment per
unit cost**, not by raw fallback count. A throwaway render probe
(`docs/dev/d402-next-family-reachability.md`, log `/tmp/reach_probe_d402.log`) rendered
all 726 fallback rows as-is and with the whole frontmatter removed, using the official
matrix's cell-surface predicate (character + background), and classified the surviving
rows by frontmatter shape and body capability markers.

| family | fallbacks | body draws after strip | clean look/theme subset | cost / risk |
|---|---|---|---|---|
| flowchart | 240 | 67 | 19 (faithful ≤ 11) | extend the 2800-line exact-bit profile machine; RFC 0036 was withdrawn for measurement falsification |
| **git** | **77** | **50** | **12** | **one existing 40-line envelope; no shared module** |
| timeline | 23 | 16 | 16 | shares a "must not drift" colour gate with gantt/journey; a single-family change drifts it |
| gantt | 13 | 11 | 11 | same shared gate |
| agentflow | 11 | 11 | 9 (+2 `handDrawnSeed`) | own gate, themes include base/dark/forest/neutral |
| er-diagram | 24 | 9 | 5 | ER107 frozen authority + three B3 gates |
| pie | 17 | 13 | 5 | no envelope |

git is selected: **12 rows for one mechanism surface**, no shared contract and no
cross-family invariant. flowchart's faithful slice (≤ 11) is smaller and its exact-bit
machine is the riskiest surface in the tree; timeline+gantt+journey (31 rows) must be a
single shared-mechanism RFC, not a one-family change.

### Evidence

Measured on the certified matrix (`build/release/tests/ui/mermaid-official-matrix.json`,
generation `59cf7de8…`, byte-identical across presets at RFC 0038 closure) and the
726-row `tools/mermaid/fallback_roster.json`.

| Metric | Current | Target | How measured |
|---|---|---|---|
| git originals `wide_240` | 58 drawn / **77 fallback** | 70 / 65 | `MermaidCapabilityReport.OfficialMatrix` census |
| global `wide_240` | 1419 / 726 | 1431 / 714 | same |
| formal `drawn_at_240` | 1233 | 1245 | certification census |
| mandatory_gaps | 48 | 48 unchanged | `e6c-mandatory-gap-roster.json` has **no git rows** |
| integrity_errors | 0 | 0 | matrix header |

Cohort census (12 rows, two bodies × six palettes):

| key | values | rows |
|---|---|---|
| `config.look` | `neo` | 12 |
| `config.theme` | `neo` 2, `neo-dark` 2, `redux` 2, `redux-dark` 2, `redux-color` 2, `redux-dark-color` 2 | 12 |

Composition: every row carries **both** leaves. The family's shape class has **18**
look/theme rows; the other six (`multiple-branches-cycling-theme-color-limit-…`) do not
draw even with the frontmatter stripped — a body capability (theme-colour cycling limit)
— so the cohort is a **reviewed literal path set**, not a classifier output.

### Why admission is testable

`look` and `theme` select upstream stroke style and colour palette. The gitGraph renderer
never reads either: `gitgraph_layout` renders commits, branches, merges and tags from the
body only. The inertness claim is therefore a per-row frame-identity twin: the frame with
the leaves equals the frame without them plus the bounded footer.

## Goals

- **G1.** Promote exactly the measured 12-row cohort.
- **G2.** Fail closed: unknown or out-of-table values, duplicates, deeper nesting, a
  namespaced `gitGraph.*` subtree, a top-level `title`, a body init directive, and every
  body-level rejection keep the whole source on fallback.
- **G3.** No renderer geometry change: the production edit is one envelope extension plus
  one footer append inside `gitgraph_layout`.
- **G4.** No regression: the E2a cohort gate stays green, fallback never grows, errors 0,
  gaps 48, dual-preset matrix byte-identical.

## Non-Goals

- The other 65 git fallbacks: 44 carry a namespaced `config.gitGraph.*` subtree
  (`mainBranchName`, `parallelCommits`, `showBranches`, `showCommitLabel`, `rotateCommitLabel`,
  `mainBranchOrder`, …) — a semantics debt, not a table; 6 are theme rows whose bodies hit
  the colour-cycling limit; 1 carries no look/theme; the remaining 14 have no body
  capability even with the frontmatter stripped.
- `title:` projection for gitGraph (the existing envelope rejects it and no cohort row
  needs it).
- `look: classic` / `look: handDrawn` and the palettes `default`, `base`, `dark`, `forest`,
  `neutral`: no git-corpus row exists, so the tables stay corpus-bounded.
- Painting the upstream stroke style or palette. A future policy RFC owns that.
- narrow_40 drawing; the ER107 B3 gates; other families.

## Proposal

### 1. Closed value tables

`git_frontmatter_body` grows into `git_frontmatter`, returning the body plus a bounded
notice vector. It accepts, inside at most one `config:` block at indent 2:

| key | admitted values | rejects |
|---|---|---|
| `look` | `neo` | any other string, duplicate |
| `theme` | `neo`, `neo-dark`, `redux`, `redux-dark`, `redux-color`, `redux-dark-color` | any other string, duplicate |

The scalar reader dequotes, so `theme: "neo"` is the same value; an unmatched quote is a
quoting mismatch, never a value. A trailing YAML comment is stripped like the shared
envelope reader does. Everything else keeps the whole source.

Disclosure footer, appended below the art (`gitgraph_layout` output) when the envelope
accepted at least one leaf:

```
gitGraph native projection v1
config.look=neo -> source node style; not applied
config.theme=<value> -> colour palette; not applied
```

Each line is charged against Work with a 96-byte cap, mirroring the state path's
`state_notice_line`.

### 2. Reviewed cohort, twin gate and wiring

- Ledger `tools/mermaid/git-0039-cohort.json` (schema `loom_mermaid_git_0039_cohort/v1`),
  sibling to `e2a-cohort.json` (which stays read-only). Its 12 rows carry the single
  phase's review record.
- Generator `tools/mermaid/generate_git_0039_cohort.py`, `--check` reproducible. The
  12/18 split is render-defined, so the cohort is a **reviewed literal path set** pinned
  in the generator with one whole-cohort path+sha digest; the generator hard-asserts only
  source-derivable counts — family 135, cohort 12, shape class `look-theme` 18, namespaced
  `config.gitGraph.*` 44. The roster and the matrix artifact are never read.
- **Twin rule** (RFC 0037/0038 restated): the recorded delta is the source footer lines
  minus the twin footer lines, compared line-by-line against the live frame. No drawn git
  fixture is body-equivalent to a cohort row (measured: the 58 drawn git rows carry the
  two-branch/merge bodies, the cohort carries the all-commit-types and 5-commit bodies), so
  all 12 rows use the synthesized config-free twin.
- Gate `tests/ui/test_mermaid_gitgraph_0039_cohort.cpp`: twin draws, native-frame
  predicate, per-width ledger equality, determinism, exact footer composition, both
  tables closed (dequote, trailing comment, duplicate, indent, unknown value, namespaced
  subtree, title, init) and the E2a byte-identity guard for the drawn git rows.
- Matrix branch in `tests/ui/test_mermaid_official.cpp` reading per-row `expected_states`
  (Journey style), reason `RFC-0039 gitGraph look/theme reviewed cohort`.
- Roster migrated with `fallback_attribution.py --emit` in the phase commit.
- Census (`drawn_at_240` 1233 → 1245, gaps/integrity invariants) is verified in the phase
  census procedure; the gate does not read the matrix artifact (false-red hazard).

### 3. Named cross-module couplings

| coupling | files that must change together |
|---|---|
| envelope validator and footer | `mermaid_gitgraph_impl.cpp`, the gate, the ledger |
| E2a negative `ConfigAndInitRowsKeepWholeSource` (`tests/ui/test_mermaid_e2a_cohort.cpp`) | iterates `git` + both state roots and excludes only the E2a/E2b/RFC 0038 ledgers (**132** rows today). The phase adds the git sibling ledger and asserts the skipped set equals it; `EXPECT_GT(checked, 100)` still holds at **120** |
| surviving git negative `MermaidGitGraphEnvelope.ConfigFrontmatterStaysSource` (`tests/ui/test_mermaid_gitgraph_semantics.cpp`) | uses a namespaced `config.gitGraph.mainBranchName`; it survives **only because** namespaced subtrees stay closed — a future widening must revisit it |
| git title negative `TitleIsBlockedNotDropped` | unchanged: `title:` stays unadmitted |
| census `expected`/`reason` | `tests/fixtures/mermaid_official/manifest.json` is SHA-pinned and never edited; promotion is a ledger-driven branch |
| frozen partitions | `e6c-mandatory-gap-roster.json` has no git rows and is untouched |
| reason string | `test_mermaid_official.cpp`, generator, closure doc |

Dependency direction is unchanged. No shared module is touched: `detail::parse_envelope`
is reused as-is.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `look` + `theme` admission | 12 rows (2 bodies × 6 palettes) | proposed | git 58→**70** drawn / 77→**65** fallback; global 1419→**1431** / 726→**714**; formal `drawn_at_240` **1233→1245**; gaps 48, integrity 0; E2a cohort gate and its negative green; release matrix sha recorded |

Closure: full debug ctest once (a red run is handled by the D-401 §7 discipline), dual-preset
byte gate, RFC `implemented`, closure doc, design-decisions entry, efficiency retro.

## Production Readiness Review

Completed at the `implementable` gate. Current answers:

- **Correctness & tests** — per-row twin gate plus a boundary suite; known flakes are
  pre-existing and recorded.
- **Build system** — no new module, target or import; the edit is inside the gitGraph
  implementation unit (`src/ui/visual/mermaid_gitgraph_impl.cpp`), so BMI PSS is N/A with
  reason; the fan-out is one implementation-unit compile plus archive and executable
  relinks (to be measured by the PRR review, not asserted).
- **Rollback** — one atomic commit (both leaves, 12 rows); one revert restores the previous
  census exactly.
- **Observability** — the disclosure footer is the user-visible trace.
- **Documentation** — design-decisions entry, closure doc, README index, retro.
- **Deletion** — the envelope's comment stating that any `config` key keeps the whole
  source is corrected in the same phase.
- **Deprecation** — none.
- **Wire / persisted shapes** — N/A: rendering-only.
- **Golden suites** — N/A: assertion-based mermaid cohorts only.
- **Platform readiness** — N/A-with-reason: rendering-only.
- **CLAUDE.md** — no change. **Architecture-graph lint** — N/A: no new module/import/edge.

## Rollout and rollback

One phase lands P1. Rollback is one revert. No persisted format changes; the ledger is
additive; the roster is regenerated, never hand edited.

## Drawbacks and risks

- **No drawn corpus sibling for any cohort row**, so faithfulness rests on the reviewed
  renderer contract plus the twin gate, not an external comparison.
- **The git renderer's frames are ours.** Admitting `look`/`theme` does not make the
  terminal paint upstream's stroke style or palette; the footer says so per row.
- **A one-value `look` table** (`neo` only) is narrower than the requirement/state tables;
  it is what the corpus proves.

## Alternatives considered

- **Do nothing.** 77 git fallbacks remain; 12 are frontmatter-only.
- **Select flowchart (≤ 11 faithful rows).** Rejected: smaller increment, and the exact-bit
  profile machine is the riskiest surface (RFC 0036 was withdrawn after a review falsified
  its measurement).
- **Select timeline+gantt+journey (31 rows).** Rejected for this RFC: they share an
  explicit "must not drift" colour gate, so the correct vehicle is one shared-mechanism
  RFC, not a single-family change. Deferred, not refused.
- **Admit `config.gitGraph.*` semantics instead.** Rejected: those 44 rows change branch
  naming, label visibility and commit ordering — semantics, not inert style tables.
- **Build one shared look/theme validator for every family.** Rejected: shared-module
  change with a much larger blast radius, and each renderer owns its own envelope shape.

## Testing and verification plan

- New `tests/ui/test_mermaid_gitgraph_0039_cohort.cpp` (registered in
  `tests/ui/CMakeLists.txt`), expected 5 tests; `ctest --preset debug -N` reports **4627**
  today, so the closure total is **4632** (re-derive at phase start).
- Edited `tests/ui/test_mermaid_official.cpp` (matrix branch + reason string) and
  `tests/ui/test_mermaid_e2a_cohort.cpp` (exclusion set + skipped-set equality).
- Targeted `ctest -L mermaid` in the phase; `generate_git_0039_cohort.py --check`;
  release-preset matrix in the phase; dual-preset gate at closure.
- **False-green guardrails.** The matrix cohort branch is a ledger echo, so the census is
  meaningful only after a full build (efficiency rule 3), and the render proof is the
  cohort gate, never the echo. The full ctest is redirected to a log with `CTEST_EXIT=$?`
  and a red run is handled per the D-401 §7 discipline (fix, or ≥ 3 clean reruns plus a
  surface-unrelatedness argument).

## Documentation impact

- [ ] `docs/dev/d402-next-family-reachability.md` — records the selection
- [ ] `docs/dev/d403-gitgraph-rfc0039-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — gitGraph config admission entry
- [ ] `docs/README.md` — RFC index
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: whether the one-value `look` table should instead admit the sibling families' three looks | design review | design review rules: corpus-bounded convention says `neo` only |
| OQ-2: whether the footer needs a header line at all (flowchart's htmlLabels notice has none) | design review | design review rules on attribution consistency with requirement/state |
| OQ-3: narrow_40 disposition per row under the footer budget | implementation | ledger `expected_states`, measured per row |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR with a different lens.
- Phases: **one** atomic phase, with its review record.
- Graduation: the phase's targeted census; closure runs full ctest once and the dual-preset
  byte gate.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | — | — | draft | Created provisional from the D-402 reachability measurement |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | — |
