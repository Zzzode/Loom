---
rfc: 45
title: gitGraph parallelCommits admission
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0045 — gitGraph `parallelCommits` admission

## Summary

D-415 selects the next slice from the remaining capability debt: the gitGraph family's 12
rows whose only unadmitted frontmatter is `config.gitGraph.parallelCommits` (`true` 10,
`false` 2). The git envelope gate already admits `config.look`/`config.theme` and discloses
them through a bounded footer; this RFC admits exactly one namespaced subtree — `gitGraph`
with the single boolean leaf `parallelCommits` — and appends one disclosure line. The
upstream parallel-lane compaction is never applied; the footer says so.

One phase, one TU (`mermaid_gitgraph_impl.cpp`), no shared module, no BMI change.
Target: git **70 / 65 → 82 / 53**; global **1505 / 640 → 1517 / 628**; formal
`drawn_at_240` **1319 → 1331**.

## Motivation

### Selection (D-415, measured 2026-11-22)

git is 135 originals, **70 drawn / 65 fallback**. The 65 fallbacks split by shape:
`gitGraph.*` namespaces 44 (12 carry only `parallelCommits`; 32 also carry content keys —
`showBranches`, `showCommitLabel`, `rotateCommitLabel`, `mainBranchName`,
`mainBranchOrder`), plain-description rows 14 (body capability), look+theme rows 6 (body
density, an RFC 0039 exception), title 1. The 12 `parallelCommits`-only rows are the
largest clean slice in the remaining debt and reuse the family's existing admission
channel; the 32 content-key rows are rejected (they change what is drawn), as are the
smaller pie (5), ishikawa (3) and state/venn/mindmap slices.

### Evidence

- All 12 rows are `description: >-` + `config:` / `gitGraph:` / `parallelCommits: <bool>`;
  with the config block removed the bodies draw natively at 240 columns (probe strip=1).
- The renderer never reads the key; it is the upstream renderer's lane-compaction switch.
  No drawn git fixture is body-equivalent, so all 12 use the **synthetic config-free twin**
  (RFC 0039 precedent).
- The 12 rows classify as `family_residual:git`; after promotion the category is 46 → 34
  and does not disappear, so the category count stays 43.

### Why admission is testable

The proof is the established two-part oracle: per-row twin inertness (config-free twin
frame plus exactly the ledger's footer delta, wide only) and the RFC 0039 gate's drawn-row
frames, which stay green only if the new namespace leaf does not disturb an envelope that
carries none.

## Goals

- **G1.** Admit exactly one namespace entry `gitGraph` (indent 2) containing exactly one
  leaf `parallelCommits` (indent 4) whose scalar is `true` or `false`.
- **G2.** Disclose it as
  `config.gitGraph.parallelCommits=<value> -> parallel lane compaction; not applied`,
  appended after every existing notice line.
- **G3.** Promote exactly 12 rows; every other verdict is unchanged — `look`/`theme` keep
  their tables, every other namespaced key (`mainBranchName`, `mainBranchOrder`,
  `showBranches`, `showCommitLabel`, `rotateCommitLabel`) still rejects, an empty namespace
  or an empty config block still rejects.
- **G4.** No regression: the RFC 0039 12-row cohort keeps its frames and the dual-preset
  matrix stays byte-identical.

## Non-Goals

- The 32 content-bearing `gitGraph.*` rows, the 14 description-only body rows, the 6
  density rows and the title row.
- flowchart's 222 exact-bit rows and the 67 protected/frozen rows.
- narrow_40 drawing; the ER107 B3 gates.

## Proposal

### 1. Mechanism (one TU, one namespaced subtree)

In `git_frontmatter` (the envelope gate used by the git renderer):

- Extend the field bitset with `GitGraphNs = 4u` and `Parallel = 8u`.
- `event.namespace_entry && event.path == "gitGraph" && event.indent == 2` marks the
  namespace once; a second namespace rejects. (The envelope parser already rejects
  duplicate paths.)
- `event.path == "gitGraph.parallelCommits" && event.indent == 4` requires the namespace
  to be open, marks once, and admits only the scalars `true` and `false` read through the
  existing `git_inert_scalar` (comment strip, dequote, no whitespace trim beyond the
  parser's).
- Anything else — a different namespace name, a deeper leaf, a colon-less entry — rejects,
  as today.
- The namespace without its leaf rejects (no empty `gitGraph:` block).
- The notice list gains, **last**:
  `config.gitGraph.parallelCommits=<value> -> parallel lane compaction; not applied`
  (77 characters at most, under the 96-byte cap). Existing sequences are unchanged because
  no already-drawn frame can carry the namespace.

### 2. Reviewed cohort, twin gate and wiring

- Sibling ledger `tools/mermaid/git-0045-cohort.json` (schema
  `loom_mermaid_git_0045_cohort/v1`) with the 12 rows, each with its **synthetic**
  config-free twin (the generation rule is the ledger's own path with the config block
  removed), literal footer lines, and the measured per-width states.
- `generate_git_0045_cohort.py`, renderer-free, `--check`-reproducible; its shape mirror
  follows the live gate (description folded `>-` with a two-space payload, one config, the
  namespace + leaf, scalar normalization).
- The RFC 0039 generator's `counts.namespaced` stays **44** and its `EXCLUDED`-style pins
  are source-shape; only the human `exclusions` sentence changes to "12 parallelCommits
  rows admitted by RFC 0045, 32 namespaced rows remain". Its 12 rows do not change.
- Partition arithmetic is literal against the filesystem: 135 = 70 drawn + 12 admitted + 53
  remaining; the remaining split by roster category (34 `family_residual:git` +
  12 `git_cherry_pick` + 6 `look_neo` + 1 `multiline_label`).
- Gate `tests/ui/test_mermaid_git_0045_cohort.cpp` (5 tests): ledger schema/review/pins and
  the 0039 `namespaced == 44` source-shape agreement; per-row twin inertness + exact footer
  composition; the namespace battery (both booleans admitted; `yes`/`1`/empty/`True`/
  duplicate namespace/`mainBranchName`/`mainBranchOrder`/`showBranches`/nested-too-deep/
  namespace-without-leaf/empty-config/title rejected); the leaf-composition battery
  (`look` + `theme` + namespace emits three lines in order); the literal partition.
- Matrix branch before the RFC 0039 git branch and the generic fallback, reason
  `RFC-0045 gitGraph parallelCommits reviewed cohort`, `raw_active_schema` empty so the
  generic `originals_drawn` counter credits +12.
- Roster: `--emit` 640 → 628, `family_residual:git` 46 → 34, categories stay 43.
- **Existing-assertion audit**: no current test pins `parallelCommits`; the two namespace
  negatives that exist (`mainBranchName: trunk` in `test_mermaid_gitgraph_semantics.cpp` and
  `test_mermaid_gitgraph_0039_cohort.cpp`) stay rejected and unchanged.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `parallelCommits` admission | 12 rows, 1 family | proposed | red-first: the 12 rows are `source_fallback` on wide/narrow/restored before the change; then git 70/65 → **82/53**; global 1505/640 → **1517/628**; formal **1319 → 1331**; gaps 48, integrity 0; narrow measured per row; dual-preset sha recorded; independent P1 code review APPROVE |

Closure: full debug ctest once, dual-preset byte gate, RFC `implemented`, closure doc,
design-decisions entry, efficiency retro.

**Closure-report format (TL 2026-11-22):** ① the absolute `source_fallback` total (expected
**628**), ② the part beyond a certified profile or frozen/protected authority (expected
**289**, unchanged), ③ this phase's net reduction (**12**).

## Production Readiness Review

Completed at the `implementable` gate by an independent PRR agent.

## Rollout and rollback

One phase, one atomic commit; rollback is one revert plus a rebuild/re-measure. No
persisted data, wire protocol, CLI, or settings change.

## Drawbacks and risks

- **A disclosed upstream behaviour the terminal cannot emulate** — the parallel lane
  compaction is not applied; the footer states it. The 32 content-key rows show the line
  between layout toggles and content semantics.
- **A new grammar position** (a namespaced subtree) in a gate that previously admitted only
  flat indent-2 leaves; the namespace battery pins the closure.

## Alternatives considered

1. **Do nothing** — leaves 12 twin-proven rows on fallback.
2. **Admit the 32 content-key rows** — rejected: they change drawn labels/branches/order.
3. **pie `donutHole` + `highlightSlice` (5) or ishikawa `useMaxWidth` (3)** — smaller;
   deferred as the next slices.

## Testing and verification plan

- New gate (5 gtests) + one hermetic `--check` for the new generator: expected
  `ctest -L mermaid` delta **+6**; full debug ctest **4675 → 4681**.
- The RFC 0039 git gate stays the drawn-row regression oracle and must stay green.
- Targeted `ctest -L mermaid`; release matrix; dual-preset byte gate at closure; full debug
  ctest once.
- Roster order: review approved → full rebuild → both matrices → cohort gate green →
  `--emit` → `--self-test` → `--matrix`.

## Documentation impact

- [ ] `docs/dev/d415-next-family-git-parallel-commits.md` — selection (already written)
- [ ] `docs/dev/d416-git-parallel-commits-rfc0045-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — gitGraph namespace admission entry
- [ ] `docs/README.md` — RFC index (0045 row; the RFC 0039 row's "44 namespaced" clause)
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — git row and headline

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: sibling ledger vs extending the 0039 ledger? | design review | proposed: sibling ledger; the 0039 `namespaced: 44` pin stays source-shape |
| OQ-2: is a `true`/`false` scalar table the right closure, or any boolean? | design review | proposed: the two literals only |
| OQ-3: narrow_40 disposition | implementation | measured live |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR.
- Phase: one atomic phase with its review record.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | — | — | draft | Created provisional from D-415 |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | — |
