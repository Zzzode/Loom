---
rfc: 40
title: journey-family look and theme admission (timeline, gantt, journey)
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0040 — journey-family look and theme admission (timeline, gantt, journey)

## Summary

Three renderers — `timeline`, `gantt` and `journey` — share one byte-identical frontmatter
gate whose comment states that they "must not drift apart". D-404 measured their remaining
reachable slice under the reachable-increment-per-cost criterion: **16 timeline + 11 gantt
+ 4 journey = 31 rows** are blocked only by `config.theme` and/or `config.look` in the
frontmatter. This RFC admits those two keys as per-family, corpus-bounded closed tables,
discloses them through a family footer below the art, and rewrites the shared contract as
one rule implemented per TU. Target: timeline **7 → 23** drawn / **23 → 7** fallback,
gantt **39 → 50** / **13 → 2**, journey **10 → 14** / **8 → 4**; global **1431 / 714 →
1462 / 683**.

## Motivation

### Family selection (D-404, measured 2026-11-22)

The selection criterion is reachable drawn increment per unit cost, not raw fallback count
(D-402). The remaining ranking was: timeline 16, gantt 11, journey 4 (all three share the
colour gate), flowchart ≤ 11, agentflow 9, er 5, pie 5.

| family | originals | drawn / fallback today | clean look/theme rows | after | remaining fallback cause |
|---|---|---|---|---|---|
| timeline | 30 | 7 / 23 | **16** | 23 / 7 | 7 use an in-body `%%{init}%%` theme directive |
| gantt | 52 | 39 / 13 | **11** | 50 / 2 | 2 pre-existing malformed dates |
| journey | 18 | 10 / 8 | **4** | 14 / 4 | 4 use an in-body `%%{init}%%` theme directive |

All 31 rows have the identical shape: top-level `description` (folded `>-`) plus `config`.

### Why the three together

`mermaid_timeline_impl.cpp`, `mermaid_gantt_impl.cpp` and `mermaid_journey_impl.cpp` each
carry the same key-based gate and a comment demanding they not drift. Admitting in one
family and not the others would create exactly the drift the comment forbids; the
architecturally correct unit is one RFC covering all three. Following the established
pattern (state, requirement and git each own a TU-local envelope), the admission logic is
implemented **per TU**, not as a shared module: the "same contract" property is enforced by
one cross-family test, not by a shared implementation. This keeps the change out of the
`mermaid_detail` interface and therefore out of the BMI fan-out.

### The pass-through rule must not change

The existing gate only rejects `theme`/`look` keys; every other frontmatter key is ignored
and the body is rendered. **11 rows that draw today rely on this**: 3 gantt rows carry
`gantt.topAxis` / `gantt.useMaxWidth`, 8 journey rows carry `journey.*` keys. A strict
"only admitted leaves" validator would turn them into fallbacks — a census regression this
RFC forbids (G4). The new rule is therefore: `theme`/`look` become *validated and
disclosed*; every other key keeps today's pass-through.

### Why admission is testable

None of the three renderers reads `look` or `theme`; each gate exists only because a themed
source would otherwise be drawn with the theme silently dropped. The proof is a per-row
twin gate (frame equals the config-free twin plus the recorded footer delta) plus a
drawn-row frame-identity oracle over the three families' 56 already-drawn rows.

## Goals

- **G1.** Promote exactly the measured 31 rows (16 timeline, 11 gantt, 4 journey).
- **G2.** Fail closed for every out-of-table value: `look` values other than `classic`/`neo`
  in timeline (including `handDrawn`), **any** `look` in gantt and journey (no corpus row),
  palettes outside each family's table, duplicates, malformed scalars, and an in-body
  `%%{…}%%` initializer that mentions `theme` (unchanged).
- **G3.** Preserve the pass-through rule exactly (no census shrinkage).
- **G4.** No regression: the 56 drawn rows keep their frames byte-for-byte, the three
  `themed_excluded` lists shrink to the remaining rows, `ThemeWordIn…` negatives stay
  green, dual-preset matrix byte-identical.

## Non-Goals

- The 7 timeline and 4 journey rows whose theme arrives through an in-body
  `%%{init: …}%%` directive, and the 2 malformed-date gantt rows.
- `themeVariables.*`, `gantt.*`/`journey.*`/`timeline.*` keys: they keep the pass-through
  (ignored), exactly as today. This RFC does not validate them.
- Painting the upstream palettes. A future policy RFC owns that.
- narrow_40 drawing; the ER107 B3 gates; other families.

## Proposal

### 1. Closed value tables (per family, corpus-bounded)

| family | admitted `look` | admitted `theme` (rejects everything else) |
|---|---|---|
| timeline | `classic`, `neo` | `dark`, `default`, `forest`, `neo`, `neo-dark`, `neutral`, `redux`, `redux-dark`, `redux-color`, `redux-dark-color` |
| gantt | — (none) | `dark`, `default`, `forest`, `neutral`, `redux`, `redux-dark`, `redux-color`, `redux-dark-color` |
| journey | — (none) | `redux`, `redux-dark`, `redux-color`, `redux-dark-color` |

The scalar reader strips a trailing YAML comment outside quotes and dequotes a matched
pair; an unmatched quote is a mismatch, never a value. A visual key may appear at most
once. Every other frontmatter line is ignored (the measured pass-through).

### 2. Disclosure footer

Per family, emitted below the art when at least one leaf was accepted, with one blank
separator line first:

```
Timeline native projection v1        (or Gantt … / Journey …)
config.look=<value> -> source node style; not applied
config.theme=<value> -> colour palette; not applied
```

Fixed order `look` then `theme`; each line capped at 96 bytes; emission charges
`(1 + n) * (4 * sizeof(std::string) + 256) + bytes * 2 + 256`; the total must satisfy
`art_lines + 1 + n <= 1024`. This is the state/flow/git budget, restated so the three
implementations cannot differ.

### 3. Reviewed cohort, twin gate and wiring

- Sibling ledger `tools/mermaid/journeyfam-0040-cohort.json` (schema
  `loom_mermaid_journeyfam_0040_cohort/v1`) with 31 rows in three family sections and one
  phase review record; the three existing cohort ledgers stay read-only for the row sets
  they now describe (their `themed_excluded` lists are regenerated separately, see §3).
- Generator `tools/mermaid/generate_journeyfam_0040_cohort.py`, `--check` reproducible:
  three pinned literal path sets each with a digest, source-derivable counts (timeline 30
  originals / 16 clean, gantt 52 / 11, journey 18 / 4) and per-row shape validation
  (description + config, leaves inside the family's tables). The roster and the matrix
  artifact are never read.
- **Twin rule** (RFC 0037/0038/0039 restated): the frame equals the synthesized
  config-free twin's frame plus the recorded footer delta, line by line, both sides through
  the same sentinel-bearing wrapper. No drawn row of the three families is body-equivalent
  to a cohort row, so all 31 use the config-free twin.
- Gate `tests/ui/test_mermaid_journeyfam_0040_cohort.cpp`: ledger pins; per-row twin
  inertness; three closed-table boundary tests (one per family, including the `handDrawn`
  rejection for timeline and the no-`look` rejection for gantt/journey); a **cross-family
  agreement test** feeding one probe battery to all three and comparing against each
  family's table; and the **drawn-row frame-identity oracle** over the 7 + 39 + 10 = 56
  already-drawn rows (frontmatter-stripped twin, full untrimmed surfaces, no footer
  markers). Plus a hermetic ctest running the generator `--check`.
- The three existing generators (`generate_timeline_cohort.py`,
  `generate_gantt_cohort.py`, `generate_journey_cohort.py`) classify the promoted rows as
  admitted and shrink their `themed_excluded` lists: timeline 23 → 7, gantt 11 → 0,
  journey 8 → 4. Their ledgers are regenerated, never hand-edited; their gates' pinned
  counts follow. Because gantt's list empties, its `ThemedGanttsStayWholeSource` becomes
  vacuous and is replaced by an explicit out-of-table negative (`theme: neo` — no gantt
  corpus row — stays whole source).
- Matrix branch: one Journey-style map keyed by path covering all three roots, placed
  before the generic fallback, reason `RFC-0040 journey-family look/theme reviewed cohort`.
- Roster migrated with `fallback_attribution.py --emit` in the phase commit.

### 4. Named cross-module couplings

| coupling | files that must change together |
|---|---|
| three gates and three renderers | the three `*_layout` implementation units and their tests |
| existing cohort ledgers and gates | `generate_{timeline,gantt,journey}_cohort.py`, `{timeline,gantt,journey}-cohort.json`, `test_mermaid_{timeline,gantt,journey}_cohort.cpp` (themed_excluded lists, pinned counts, gantt's replaced negative) |
| `FrontmatterLookIsAVisualGate` (journey `:182`, timeline `:148`, gantt) | uses `look: handDrawn`, which **stays** out-of-table for all three — the negatives survive because the tables are corpus-bounded; a future widening must revisit them |
| `ThemeWordIn…IsNotAVisualGate` (three tests) | unchanged: the gate stays key-based, so the word "theme" in a label is not a gate |
| census `expected`/`reason` | `manifest.json` is SHA-pinned and never edited; promotion is a ledger-driven branch |
| frozen partitions | `e6c-mandatory-gap-roster.json` has no rows in these three families |
| reason string | `test_mermaid_official.cpp`, generator, closure doc |

Dependency direction is unchanged. No shared module, no BMI change: each renderer owns its
TU-local validator, and one cross-family test enforces that the three verdicts agree.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `look` + `theme` admission | 31 rows (16 timeline + 11 gantt + 4 journey) | proposed | timeline 7/23 → **23/7**, gantt 39/13 → **50/2**, journey 10/8 → **14/4**; global 1431/714 → **1462/683**; formal `drawn_at_240` **1245 → 1276**; gaps 48, integrity 0; three `themed_excluded` lists at 7/0/4; dual-preset sha recorded |

Closure: full debug ctest once (D-401 §7 discipline), dual-preset byte gate, RFC
`implemented`, closure doc, design-decisions entry, efficiency retro.

Phase-start re-derivations (never assume): the gate's test count and the
`ctest --preset debug -N` total; every row's `narrow_40` disposition measured per row.

## Production Readiness Review

Completed at the `implementable` gate. Current answers:

- **Correctness & tests** — per-row twin gate, three boundary suites, a cross-family
  agreement test and a 56-row frame-identity oracle; known flakes are pre-existing.
- **Build system** — no new module, target or import for production code (one new test
  executable target); three implementation units recompile, then archive and executable
  relinks. BMI PSS N/A with reason (no interface change). Fan-out to be measured by the
  PRR review, not asserted.
- **Rollback** — one atomic commit (three renderers, one ledger, three regenerated
  ledgers, gates); one revert restores the previous census exactly.
- **Observability** — the family footer is the user-visible trace.
- **Documentation** — design-decisions entry, closure doc, README index, retro.
- **Deletion** — the three "no color contract" comments are rewritten to state the new
  rule in the same phase.
- **Deprecation** — none. **Wire / persisted shapes** — N/A. **Golden suites** — N/A.
- **Platform readiness** — N/A-with-reason. **CLAUDE.md** — no change.
  **Architecture-graph lint** — N/A: no new module/import/edge.

## Rollout and rollback

One phase lands P1. Rollback is one revert. No persisted format changes; the new ledger is
additive; the three existing ledgers and the roster are regenerated, never hand edited.

## Drawbacks and risks

- **Larger fan-in than a single-family slice.** Three renderers, three regenerated ledgers
  and three existing gates change together; the RFC's coupling table is the checklist.
- **Pass-through stays lenient.** Unknown config keys remain ignored in these three
  families. Tightening it is a separate, census-shrinking decision with its own RFC.
- **No drawn corpus sibling** for any cohort row; faithfulness rests on the reviewed
  renderer contract, the twin gate and the 56-row frame-identity oracle.

## Alternatives considered

- **Do nothing.** 31 rows remain fallback.
- **Timeline only (16 rows).** Rejected: it would split the explicit "must not drift"
  colour contract that the three renderers share.
- **One shared validator in `mermaid_detail`.** Rejected: it is an interface (BMI) change
  with a much larger rebuild fan-out for a 31-row slice; the established pattern is a
  TU-local envelope per family plus a cross-family test.
- **Tighten to a strict closed-leaf validator.** Rejected: measured 11 currently drawn rows
  (3 gantt + 8 journey) carry other config keys and would regress.
- **Select flowchart (≤ 11) or agentflow (9).** Rejected on increment/cost: both are
  smaller, and flowchart touches the riskiest exact-bit surface.

## Testing and verification plan

- New `tests/ui/test_mermaid_journeyfam_0040_cohort.cpp` (registered in
  `tests/ui/CMakeLists.txt`) plus one hermetic generator `--check` test; the exact count is
  re-derived at phase start (baseline `ctest --preset debug -N` = 4633) and recorded.
- Edited `tests/ui/test_mermaid_official.cpp` (matrix branch + reason string) and the three
  existing cohort tests (pinned counts; gantt's replaced negative).
- Targeted `ctest -L mermaid` in the phase; the three existing generators' `--check` plus
  the new one; `fallback_attribution.py --emit` followed by `--self-test` and `--matrix`;
  release-preset matrix in the phase; dual-preset gate at closure.
- **False-green guardrails.** The matrix branch is a ledger echo; the render proof is the
  cohort gate. Roster correctness comes from the phase procedure (full build → matrix →
  `--emit`), not from `--self-test`. The full ctest is redirected to a log with
  `CTEST_EXIT=$?` and a red run is handled per the D-401 §7 discipline.

## Documentation impact

- [ ] `docs/dev/d404-next-family-journey-trio.md` — records the selection
- [ ] `docs/dev/d405-journey-family-rfc0040-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — journey-family config admission entry
- [ ] `docs/README.md` — RFC index
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: whether the three gates should stay per-TU or become one shared helper | design review | design review rules on the no-drift invariant vs the BMI cost |
| OQ-2: whether the cross-family agreement test is sufficient to enforce the invariant | design review | design review rules on the test's discriminating power |
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
| 2026-11-22 | — | — | draft | Created provisional from the D-404 reachability measurement |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | — |
