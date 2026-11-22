---
rfc: 45
title: gitGraph parallelCommits admission
status: implemented
owners: "@Zzzode"
reviewers: ["agent:design-review", "agent:prr-review"]
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
channel. The 32 content-key rows are excluded **conservatively as one slice**: they carry
`showBranches`/`showCommitLabel`/`rotateCommitLabel`/`mainBranchName`/`mainBranchOrder`,
and all but one default-valued no-op row change what is drawn. The smaller pie (5),
ishikawa (3) and state/venn/mindmap slices are deferred.

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

- Extend the field bitset with `GitGraphNs = 4u` and `Parallel = 8u`; hoist the `mark()`
  lambda above the new branches and never call `git_inert_scalar` on a namespace event (its
  value is provably empty and the call would charge Work).
- `event.namespace_entry && event.path == "gitGraph" && event.indent == 2` marks the
  namespace once; a second namespace rejects. (The envelope parser already rejects
  duplicate paths.)
- `event.path == "gitGraph.parallelCommits" && event.indent == 4` requires the namespace
  to be open, marks once, and admits only the scalars `true` and `false` read through the
  existing `git_inert_scalar` (comment strip, dequote, no whitespace trim beyond the
  parser's).
- Anything else — a different namespace name, a deeper leaf, a colon-less entry — rejects,
  as today.
- The namespace without its leaf rejects (**an explicit gate check**: `parse_envelope`
  *emits* the namespace event for an empty `gitGraph:` block, and marking the namespace
  sets a field bit that defeats the empty-config guard, so the gate must test
  `(fields & GitGraphNs) && !(fields & Parallel)` itself). A colon-less entry is rejected
  earlier by the parser (`colon == npos`).
- The notice list gains, **last**:
  `config.gitGraph.parallelCommits=<value> -> parallel lane compaction; not applied`
  (78 bytes at most — the `false` variant — under the 96-byte cap). Existing sequences are unchanged because
  no already-drawn frame can carry the namespace.

### 2. Reviewed cohort, twin gate and wiring

- Sibling ledger `tools/mermaid/git-0045-cohort.json` (schema
  `loom_mermaid_git_0045_cohort/v1`) with the 12 rows, each with its **synthetic**
  config-free twin (delete the `config:` line **and every following line indented at least
  two spaces, until the next column-0 key** — the RFC 0039 `config_free_twin` helper cannot
  be reused because it only skips flat `look`/`theme` leaves and would leave the namespace
  in place), literal footer lines, and the measured per-width states.
- `generate_git_0045_cohort.py`, renderer-free, `--check`-reproducible; its shape mirror
  **asserts**, never approximates: folded `>-` description with a two-space payload, no
  tab, exactly one `config`, namespace at indent 2 with empty value and the leaf at indent
  4, duplicate/empty-namespace/unknown-key rejection, and scalar normalization identical to
  `git_inert_scalar` (quote-aware `#` cut via `er_strip_comment`'s rule, `trim_relation` on
  the cut prefix, paired dequote, no extra strip beyond the parser's).
- The RFC 0039 generator's `counts.namespaced` stays **44** and its `EXCLUDED`-style pins
  are source-shape; only the human `exclusions` sentence changes to "12 parallelCommits
  rows admitted by RFC 0045, 32 namespaced rows remain". That sentence is generator
  source, so `tools/mermaid/git-0039-cohort.json` is **regenerated with the updated
  exclusions sentence and the new `selection.census_note`** (its 12 rows, counts and pins
  do not change) — editing the sentence without regenerating would turn the
  registered `MermaidGit0039Cohort.GeneratorIsReproducible` check red. The ledger file is
  part of the phase commit. Its machine-readable `excluded[]` per-row reasons predate this
  phase, so `selection.census_note` declares that list the **frozen 0039 census** and names
  the admitted class by count ("the 12 `gitGraph.parallelCommits`-only rows admitted by RFC
  0045") — never a second 12-path literal, which would drift against the 0045 ledger;
  `counts.namespaced == 44` stays the source-shape pin.
- Partition arithmetic is literal against the filesystem: 135 = 70 drawn + 12 admitted + 53
  remaining; the remaining split by roster category (34 `family_residual:git` +
  12 `git_cherry_pick` + 6 `look_neo` + 1 `multiline_label`).
- Gate `tests/ui/test_mermaid_git_0045_cohort.cpp` (5 tests): ledger schema/review/pins and
  the 0039 `namespaced == 44` source-shape agreement; per-row twin inertness + exact footer
  composition; the namespace battery (both booleans admitted; `yes`/`1`/empty/`True`/
  duplicate namespace/`mainBranchName`/`mainBranchOrder`/`showBranches`/nested-too-deep/
  `gitGraph: foo` (a value makes it a leaf, not a namespace)/namespace-without-leaf/
  empty-config/title rejected); the leaf-composition battery
  (`look` + `theme` + namespace emits three lines in order); the literal partition.
- Matrix branch before the RFC 0039 git branch and the generic fallback, reason
  `RFC-0045 gitGraph parallelCommits reviewed cohort`, `raw_active_schema` empty so the
  generic `originals_drawn` counter credits +12.
- Roster and ledgers: regenerate `git-0039-cohort.json` (updated sentence and
  `selection.census_note`), then `--emit`
  640 → 628, `family_residual:git` 46 → 34, categories stay 43.
- **Existing-assertion audit**: no current test names `parallelCommits`; the two namespace
  negatives that exist (`mainBranchName` in `test_mermaid_gitgraph_semantics.cpp` and
  `test_mermaid_gitgraph_0039_cohort.cpp`) stay rejected and unchanged. One whole-source
  scan did pin these rows implicitly: `MermaidE2aCohort.ConfigAndInitRowsKeepWholeSource`
  asserts every config-bearing git row outside the reviewed ledgers renders identically to
  text. It is extended, not relaxed — the RFC 0045 ledger joins its ledger-driven exclusion
  set and the `git_skipped == git_admitted` set equality still holds (the scan drops from
  120 to 108 rows, above its `> 100` floor).
- **The footer contains the literal `gitGraph`**, which three consumers use as a "not
  natively drawn" substring mnemonic (`test_mermaid_e2a_cohort.cpp`, the gitgraph
  semantics gate, `e2e_pty_shutdown.cpp`; `design-decisions.md` records that the RFC 0039
  header was capitalised to avoid exactly this). They stay green only because none renders
  these rows. The new gate therefore uses **line-equality** native-frame predicates (the
  `test_mermaid_gitgraph_0039_cohort.cpp` form), never `find("gitGraph")`, and any row
  carrying this footer must never be judged by the substring mnemonic.

- **Stale "namespaced subtrees stay closed" promises are amended in the same phase**: the
  production comments in `git_frontmatter` (~951-953, ~1002-1003), the
  `design-decisions.md` paragraph that names `parallelCommits` as closed, and RFC 0039's
  testing-plan sentence (the `mainBranchName` negative remains valid).

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `parallelCommits` admission | 12 rows, 1 family | **done** | red-first measured (all 12 manifest `source`/`frontmatter`); git **70/65 → 82/53**; global **1505/640 → 1517/628**; formal **1319 → 1331**; gaps 48, integrity 0; narrow **0/12** live; dual-preset matrix byte-identical sha `ca24369e…`; full debug ctest 4681/4681 executed (two known `ServerMain.DirectConnect*` flakes and one `E2E_Pty_Handoff_HupStress` under load, each clean on 3 isolated reruns); independent P1 code review APPROVE (r2) |

Closure: full debug ctest once, dual-preset byte gate, RFC `implemented`, closure doc,
design-decisions entry, efficiency retro.

**Closure-report format (TL 2026-11-22):** ① the absolute `source_fallback` total (expected
**628**), ② the part beyond a certified profile or frozen/protected authority (expected
**289**, unchanged), ③ this phase's net reduction (**12**).

## Production Readiness Review

Completed at the `implementable` gate by an independent PRR agent.

### 1. Correctness and tests

- Every new behaviour has a new test: the five-test cohort gate and one hermetic
  `--check`; no existing assertion flips (the `mainBranchName` negatives stay rejected).
- The `gitGraph` substring-mnemonic hazard is named and the gate uses line-equality
  native-frame predicates.
- Flakes: none added. Serial ctest total: **4675 → 4681**.

### 2. Build system and module discipline

- N/A for BMI: one `.cpp` body unit; `GitEnvelope` already carries `config_notices`.
  No concurrency, module or import change.

### 3. Rollback and compatibility

- One atomic commit including the roster and both ledgers; a revert restores 640 and every
  pin. No persisted data, wire, CLI or settings change.

### 4. Observability / 5. Documentation / 6. Platform

- The existing git footer channel; no new diagnostics or ticker. The phase amends the stale
  "namespaced subtrees stay closed" comments/decisions/RFC 0039 sentence. Debug + release
  `-Werror` on the Linux box; ASCII output; macos-14 CI is not a gate; no new fetch.

### Review sign-off

| Role | Reviewer | Date | Verdict |
|---|---|---|---|
| Design | agent:design-review | 2026-11-22 | approve (r2) |
| Production readiness | agent:prr-review | 2026-11-22 | approve (2 MAJOR + 4 MINOR + 3 NIT folded) |
| Code (per phase) | agent:code-review | 2026-11-22 | approve (r2) |

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
2. **Admit the 32 content-key rows** — rejected: all but one default-valued no-op row
   change drawn labels/branches/order; the slice is excluded conservatively as a whole.
3. **pie `donutHole` + `highlightSlice` (5) or ishikawa `useMaxWidth` (3)** — smaller;
   deferred as the next slices.

## Testing and verification plan

- New gate (5 gtests) + one hermetic `--check` for the new generator: measured
  `ctest -L mermaid` **+6** (1496 after the registration); full debug ctest **4675 → 4681**
  executed (`ctest -N` reports 4683 including the two `DISABLED_` layout tests). The
  executable is
  registered like its siblings (`add_executable` / `target_link_libraries` /
  `gtest_discover_tests ... LABELS "mermaid"`) with `TIMEOUT 600` (the 0039 gate's
  five-capture × 12-row cost).
- The RFC 0039 git gate stays the drawn-row regression oracle and must stay green.
- Targeted `ctest -L mermaid`; release matrix; dual-preset byte gate at closure; full debug
  ctest once.
- Ordered at closure: review approved → regenerate `git-0039-cohort.json` (updated
  sentence and `selection.census_note`) → full rebuild → both matrices → cohort gate green → `--emit` → `--self-test` →
  `--matrix`.

## Documentation impact

- [x] `docs/dev/d415-next-family-git-parallel-commits.md` — selection (already written)
- [x] `docs/dev/d416-git-parallel-commits-rfc0045-closure.md` — closure record
- [x] `docs/decisions/design-decisions.md` — gitGraph namespace admission entry
- [x] `docs/README.md` — RFC index (0045 row; the RFC 0039 row's "44 namespaced" clause)
- [x] `tools/mermaid/git-0039-cohort.json` — regenerated with the updated exclusions
      sentence and `selection.census_note`
- [x] `docs/dev/mermaid-d117-family-fallback-counts.md` — the git row was stale at 77/58 and
      is corrected to 53/82 with the new headline

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
| 2026-11-22 | r1 | agent:design-review | REQUEST-CHANGES | Scope (exactly 12), arithmetic (135=70+12+53; 53=34+12+6+1), grammar event shapes and the content-vs-layout argument all confirmed; the native projection draws commit identity/order and never reads the key, so the toggle is layout-only *with* the disclosure. 1 MAJOR + 3 MINOR + 2 NIT: the 0039 ledger must be regenerated because the exclusions sentence is generator source and the registered `--check` compares whole-file text; the footer is 78 bytes for `false`, not 77; the 32-row rationale overclaimed (one default-valued no-op row) → softened to a conservative slice exclusion; the empty-namespace rejection is a gate check, not a parser one (`parse_envelope` emits the namespace event), so the explicit `(GitGraphNs && !Parallel)` test is now a mechanism bullet. All folded |
| 2026-11-22 | r2 | agent:design-review | **APPROVE-TO-ACCEPT** | All r1 folds re-verified against the repository (the regenerated 0039 ledger changes only `selection.exclusions`; the footer is 78 bytes; the empty-namespace gate check is explicit; the 32-row exclusion is conservative). Two residual NITs folded: the Alternatives wording now carves out the default-only no-op row, and the closure order names the 0039 regeneration step. Gate: provisional → accepted |
| 2026-11-22 | PRR | agent:prr-review | REQUEST-CHANGES | 2 MAJOR + 4 MINOR + 3 NIT. MAJOR: the footer contains the literal `gitGraph`, which three consumers use as a not-natively-drawn substring mnemonic → the gate is pinned to line-equality predicates and the audit names the hazard; the 0039 ledger's machine-readable `excluded[]` reasons would contradict the new sentence → `census_note` declares the list the frozen census. MINORs: stale "namespaced subtrees stay closed" comments/decisions/RFC 0039 sentence amended in the phase; README status; the twin extraction rule (delete the config line and every following indent≥2 line); the generator mirror enumerated to the RFC 0044 standard. NITs: hoist `mark()` and skip the scalar call on namespace events; `gitGraph: foo` is a leaf; CMake wiring and TIMEOUT 600 named, the stale d117 git row corrected. All folded |
| 2026-11-22 | — | — | draft | Created provisional from D-415 |
| 2026-11-22 | correction | agent:code-review-r1 | — | The r2 row's "changes only `selection.exclusions`" understates the regenerated 0039 ledger diff: it also adds `selection.census_note` (PRR fold). No code or ledger change; the record is corrected here, not rewritten |
| 2026-11-22 | code r1 | agent:code-review | REQUEST-CHANGES | BLOCKER: the ledger gate's twin assertion scanned the whole source for `parallelCommits`, which the folded description prose legitimately contains → 12/12 false failures; replaced with the structural `twin_is_config_free`. Plus: upstream manifest pins missing; the generator mirror lacked tab-indent and body-init rejection; the battery lacked duplicate-namespace/`mainBranchOrder` negatives; the twin rule diverged C++/Python at indent 1; the r2 wording correction. All fixed |
| 2026-11-22 | code r2 | agent:code-review | **APPROVE** | Re-verified against the full gate binary (4/5 green; the only red is the verdict placeholder, written after this approval); all fixes confirmed by code: structural twin predicate, upstream pins, mirror tab/init rejection with the stricter-direction docstring, added negatives, aligned twin rule, E2a scan extension. Four non-blocking NITs (closure record, README/d117 sync, init edge shape, `look`/`theme` residual) dispositioned |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | de989294 |
| 2026-11-22 | — | Folded design r1/r2; accepted after r2; no code | 9752cec5 |
| 2026-11-22 | — | PRR folded (mnemonic audit, frozen census, twin rule, mirror); implementable; no code | 4ad9bb70 |
| 2026-11-22 | P1 | 12 rows admitted: one namespaced `gitGraph` subtree with the boolean `parallelCommits` leaf, footer line appended last, sibling ledger + generator + 5-test gate + hermetic `--check`, matrix branch, E2a scan extension, roster 640→628; git 82/53, global 1517/628, formal 1331; dual-preset sha `ca24369e…`; full debug ctest 4681 executed (3 environmental flake reruns clean) | (this commit) |
