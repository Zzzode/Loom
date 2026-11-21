---
rfc: 42
title: pie look and theme admission
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0042 — pie look and theme admission

## Summary

The pie family has 24 official-corpus originals: 7 draw natively, 17 are fallback. D-408
measured the remaining reachable slice under the reachable-increment-per-cost criterion:
**5 of the 17** are blocked only by a `config.theme` leaf inside the reviewed folded
`description: >-` envelope; the other 12 are either `pie.*` semantic keys (8), body
capability (4), or non-inert layout/look shapes.

This RFC adds one profile — `PieTheme` — beside the existing `PieConfig` and
`PieDescription` profiles in `mermaid_charts_impl.cpp`, validates the leaf against a
closed five-value corpus table, and discloses it through a bounded footer below the art.
Target: **5 rows** in **one atomic phase**: pie **7 → 12** drawn / **17 → 12** fallback;
global **1471 / 674 → 1476 / 669**; formal `drawn_at_240` **1285 → 1290**.

## Motivation

### Family selection (D-408, measured 2026-11-22)

D-408 re-reads the D-402 strip probe against the current 674-row fallback roster (the 52
rows promoted by RFC 0039/0040/0041 excluded) and cross-checks every per-family count
against the live release matrix. Among the remaining inert-theme candidates:

| candidate | reachable inert rows | mechanism surface | risk |
|---|---|---|---|
| **pie** | **5** | **1** — one new chart profile in the existing charts TU | medium-low: the charts TU was safely extended once (RFC 0035); pie's path is small |
| flowchart | 7 | one but huge (exact-bit profile machine) and 4 of its candidate rows are inside the protected 48-row gap roster | high: RFC 0036 precedent |
| ishikawa + venn | 4 | 2 — one gate per family | low but a worse ratio |
| git 38 / requirement 26 / state-v2 6 / treemap 6 | 0 inert | semantic or geometric capability | out of scope |

### Evidence

- 24 originals = 7 drawn + 5 theme-only + 8 `pie.*` semantic + 4 body-blocked.
- The 5 rows are two bodies × themes: a twelve-slice pie under
  {redux, redux-dark, redux-color, redux-dark-color} (four rows sharing one body, no drawn
  body-equivalent) and the "Sports in Sweden" pie under {dark} (body-identical to the
  drawn `pie/pr-check/simple-sports.mmd`, so a corpus twin exists).
- The five schema paths are the reviewed folded `description: >-` envelope followed by a
  single `config:` block with one `theme` leaf — the shape `pie_description_frontmatter`
  already rejects today solely because of the config block.
- All 5 rows sit in the roster's `family_residual:pie` category (17 rows; the category
  stays non-empty) and none is in the protected `e6c-mandatory-gap-roster.json` (which
  lists only class/flowchart rows), so `mandatory_gaps` stays 48.
- **narrow_40 must be measured live**: the footer's longest line is 64 characters, longer
  than a 40-column viewport, so the admitted rows are expected to keep the whole source
  there. The ledger records the measured state and the gate asserts it (rule 34).

### Why admission is testable

The pie renderer never reads `theme`; the text surface has no colour contract. The proof
is the established two-part oracle: per-row twin inertness (the admitted frame equals the
corpus twin's frame, or for the four synthetically twinned rows the config-free twin's
frame, plus exactly the ledger's literal footer delta) and a drawn-row frame-identity
oracle over the 7 Pie7 rows the family already draws.

## Goals

- **G1.** Admit exactly the five corpus `theme` values, and only inside the reviewed
  folded-description envelope with a single `config:` block.
- **G2.** Disclose the accepted leaf through a bounded footer
  (`Pie native projection v1` + one line) naming the value the renderer accepted.
- **G3.** Promote exactly 5 rows; change no other accept/reject verdict — the existing
  `PieConfig` (`config.pie.textPosition`) and `PieDescription` profiles stay
  byte-unchanged, and a theme-bearing source without the folded description stays
  rejected.
- **G4.** No regression: the 7 Pie7 rows keep their frames cell-for-cell and the
  dual-preset matrix stays byte-identical.

## Non-Goals

- The 8 `pie.*` semantic rows (`donutHole`, `highlightSlice`, `legendPosition`,
  `textPosition` combined with a description, `useMaxWidth`, `logLevel`) — geometry or
  interaction semantics, not inert style.
- The 4 body-blocked theme rows (dark/default/forest/neutral over a body the pie renderer
  cannot project).
- A theme-bearing config without the folded description; any second config block; any
  other key — all keep today's whole-source fallback.
- narrow_40 drawing; painting the palettes; the ER107 B3 gates; other families.

## Proposal

### 1. Closed value table

| family | admitted `look` | admitted `theme` (rejects everything else) |
|---|---|---|
| pie | — (none) | `dark`, `redux`, `redux-dark`, `redux-color`, `redux-dark-color` |

Five values, exactly the corpus.

### 2. Grammar (a new chart profile, one TU)

The pie frontmatter is read by the shared bounded `Envelope` reader
(`parse_envelope`). Today `chart_source_lines` routes a pie body with a folded
description and no `pie.textPosition` to `pie_description_frontmatter`, which requires the
raw frontmatter to be exactly `description: >-` plus two-space payload lines — so a
`config:` line rejects the source. This RFC adds `PieTheme`:

- The raw frontmatter must start with the literal `description: >-` header and carry at
  least one payload line, each payload line indented by exactly two spaces with a
  non-space, non-tab third character (the unchanged `pie_description_frontmatter` rules).
- It may then carry **exactly one** `config:` line, followed by exactly one leaf line
  indented by two spaces: `theme: <value>` with a scalar read by `chart_yaml_scalar`
  (trim, strip a trailing comment outside quotes, dequote a matched pair) and a value in
  the table. A blank line inside the description block, an indented comment, a second
  `config:` line, any other leaf (`pie.textPosition`, `layout`, …), a duplicate theme, or
  an out-of-table/empty value keeps the whole source.
- A source carrying a theme leaf **without** the folded description is untouched: the
  standalone `pie_config` profile requires `pie.textPosition` and rejects it.
- The body must still be a pie body (`chart_body_family == Pie`); the existing profile
  selection for xychart is untouched.

The accepted value is recorded in a new `Chart` field (`notices`), never read by
rendering.

### 3. Disclosure footer

Emitted below the art in `charts_layout` when `notices` is non-empty, one blank separator
first:

```
Pie native projection v1
config.theme=<value> -> colour palette; not applied
```

Each line is capped at 96 bytes and charged at `size * 4 + sizeof(std::string) + 32`;
emission charges `(1 + n) * (4 * sizeof(std::string) + 256) + bytes * 2 + 256`; the total
must satisfy `rows + 1 + n <= kHeightLimit (1024)`; failing either bound keeps the whole
source. Byte-for-byte the state/flow/git/journey-family/agentflow budget. The guard uses
an off-by-one-safe `>` comparison.

### 4. Reviewed cohort, twin gate and wiring

- New sibling ledger `tools/mermaid/pie-0042-cohort.json` (schema
  `loom_mermaid_pie_0042_cohort/v1`) with the 5 rows and one phase review record. Each row
  pins the literal `expected_footer_lines` (blank separator, header, theme line),
  `expected_states`, and its twin: `{kind: "corpus", path, sha256}` for the dark row and
  `{kind: "synthetic"}` for the four redux rows (no drawn body-equivalent exists; the
  config-free twin is the same source with its frontmatter removed).
- Generator `tools/mermaid/generate_pie_0042_cohort.py`, renderer-free,
  `--check`-reproducible: pinned literal path set and digest, source-derivable counts
  (24 originals / 7 drawn / 5 admitted / 12 other), body-equivalence verification for the
  corpus twin, and per-row shape validation. The roster and the matrix artifact are never
  read.
- **Twin rule**: the frame equals the twin's frame plus the recorded literal footer delta,
  line by line, both sides through the same sentinel-bearing wrapper.
- Gate `tests/ui/test_mermaid_pie_0042_cohort.cpp`: ledger pins; per-row twin inertness
  with a **live** per-row `narrow_40` assertion; a closed-table battery (every admitted
  value with the exact footer block; unknown/case/empty; duplicate; quoted scalar as a
  positive; trailing comment as a positive; unmatched quote; `theme : dark`; indent 0/2/4;
  a payload line that looks like `theme:`; theme plus any other leaf; a second config
  block; theme without the folded description; theme plus an in-body `%%{init}%%`); a
  partition test against the Pie7 rows; and the drawn-row frame-identity oracle over the
  seven Pie7 paths.
- **Existing matrix support**: the Pie7 rows are hardcoded in
  `tests/ui/test_mermaid_official.cpp` (the "pie formal matrix support is deliberately
  file-local" block); it stays unchanged and the new sibling-ledger branch is placed
  before it, so the two row sets are disjoint.
- Roster migrated with `fallback_attribution.py --emit` in the phase commit
  (674 → 669; `family_residual:pie` 17 → 12, non-empty).

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `theme` admission | 5 rows | proposed | pie 7/17 → **12/12**; global 1471/674 → **1476/669**; formal `drawn_at_240` **1285 → 1290**; gaps 48, integrity 0; narrow_40 measured per row; dual-preset sha recorded |

Closure: full debug ctest once (D-401 §7 discipline), dual-preset byte gate, RFC
`implemented`, closure doc, design-decisions entry, efficiency retro.

## Production Readiness Review

Completed at the `implementable` gate. Answers:

- **Correctness & tests** — per-row twin gate (corpus for one row, synthetic for four), a
  closed-table battery with literal verdicts and footers, a live narrow assertion and a
  7-row frame-identity oracle.
- **Build system** — no new module, target or import for production code (one new test
  executable); one implementation unit recompiles (`mermaid_charts_impl.cpp`), then the
  archive and its dependent executables relink. BMI PSS N/A (no interface change).
- **Rollback** — one atomic commit containing the charts TU change, the new ledger and
  generator, the gate, the matrix branch, the CMake registrations and the roster; one
  revert plus a rebuild/re-measure restores the previous census.
- **Observability** — no new diagnostics; session traces unaffected.
- **Documentation** — RFC, closure doc, design-decisions entry, docs index, d117 row,
  retro.
- **Deletion** — no dead code; the existing two pie profiles are untouched.
- **Deprecation** — none; additive ledger and profile.

## Rollout and rollback

One phase lands P1. Rollback is one revert plus a rebuild/re-measure. No persisted user
data, no wire protocol, no CLI contract changes.

## Drawbacks and risks

- **A disclosed leaf the renderer cannot paint.** Documented in the footer, exactly like
  the five prior admissions.
- **Two twin kinds.** The four redux rows have no drawn body-equivalent, so their proof is
  the synthetic config-free twin; the dark row additionally certifies body equivalence to
  a drawn fixture.
- **The charts TU is shared with xychart.** The new profile is additive and the xychart
  routing is untouched; the gate's xychart probes and the existing chart cohort tests are
  the guard.

## Alternatives considered

1. **Do nothing.** Leaves 5 inert-theme rows on fallback while five sibling families
   already disclose colours; the inconsistency is a maintenance hazard.
2. **Admit the 8 `pie.*` rows too.** Rejected: those keys change geometry (donut hole,
   slice highlight, legend/text positions) — not inert.
3. **Flowchart's 7 theme rows instead.** Rejected: the exact-bit profile machine, and
   4 of the candidate rows are inside the protected gap roster.
4. **A shared admission helper for pie and xychart.** Rejected: the charts TU already
   owns two independent profiles and RFC 0035 established that each family's profile is
   validated separately; a shared helper would couple pie's description envelope to
   xychart's.

## Testing and verification plan

- New gate `tests/ui/test_mermaid_pie_0042_cohort.cpp` (six gtests) registered in
  `tests/ui/CMakeLists.txt`, plus a hermetic `--check` ctest for the new generator.
- Targeted `ctest -L mermaid` in the phase; release-preset matrix in the phase; dual-preset
  byte gate at closure; full debug ctest once at closure.
- `fallback_attribution.py --emit` followed by `--self-test` and `--matrix`, in the fixed
  order: review approved → full rebuild → both matrices → `--self-test` → `--matrix` →
  `--emit`.
- **False-green guardrails.** The matrix branch is a ledger echo; the render proof is the
  cohort gate. The full build must precede any matrix verdict, and the report target is
  rebuilt in both presets before the sha compare. A red full gate is handled per D-401 §7.

## Documentation impact

- [ ] `docs/dev/d408-next-family-pie.md` — selection record (already written)
- [ ] `docs/dev/d409-pie-rfc0042-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — pie config admission entry
- [ ] `docs/README.md` — RFC index
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — pie row 17/7 → 12/12
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: does the new profile share xychart's description-shape helper? | design review | proposed: no — reuse the existing `pie_description_frontmatter` rules inline and keep the profiles separate |
| OQ-2: is a theme leaf without the folded description admitted? | design review | proposed: no — no corpus row, keep the standalone `pie_config` profile byte-unchanged |
| OQ-3: narrow_40 disposition per row | implementation | ledger `expected_states`, measured live per row |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR with a different lens.
- Phases: **one** atomic phase, with its review record.
- Graduation: the phase's targeted census; closure runs full ctest once and the
  dual-preset byte gate.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | — | — | draft | Created provisional from the D-408 reachability measurement |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | — |
