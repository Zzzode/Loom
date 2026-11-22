---
rfc: 42
title: pie look and theme admission
status: implemented
owners: "@Zzzode"
reviewers: ["agent:a91726bc2e6905718 rfc0042-design-review (r1 REQUEST-CHANGES -> r2 APPROVE-TO-ACCEPT)", "agent:ac41c7a527c6f43f6 rfc0042-prr (r1 REQUEST-CHANGES -> folded; see Review History)"]
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
`description: >-` envelope; the other 12 split cleanly into `pie.*` semantic keys (8)
and body capability (4).

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
- **narrow_40 must be measured live**: the footer's longest line is 60 characters
  (`config.theme=redux-dark-color -> colour palette; not applied`), longer than a
  40-column viewport, so the admitted rows are expected to keep the whole source there.
  The ledger records the measured state and the gate asserts it (rule 34); the
  twin+footer frame comparison itself is wide-only.

### Why admission is testable

The pie renderer never reads `theme`; the text surface has no colour contract. The proof
is the established two-part oracle: per-row twin inertness (the admitted frame equals the
corpus twin's frame, or for the four synthetically twinned rows the config-free twin's
frame, plus exactly the ledger's literal footer delta) and a drawn-row frame-identity
oracle over the 7 Pie7 rows the family already draws.

## Goals

- **G1.** Admit exactly the five reachable-row `theme` values, and only inside the reviewed
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

Five values — exactly the theme values carried by the corpus's *reachable* rows (the
corpus also uses `default`, `forest`, `neutral` on four rows whose bodies cannot be
projected; those values are therefore outside the admitted table).

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
  whose raw text is literally `  theme: <value>` (exactly two leading spaces, no space
  before the colon; `theme : dark` therefore rejects — the literal raw-line check governs,
  `event.path` alone would accept it because the envelope trims keys). The scalar is read exactly like
  `PieConfig` does today: on the raw `event.value`, with `chart_yaml_scalar` and an
  explicit rejection of a leading quote — no comment stripping and no separate dequoting,
  so a quoted value and a value carrying a trailing comment both reject. The resulting
  token must be one of the five table values. A blank line inside the description block,
  an indented comment, a second `config:` line, any other leaf (`pie.textPosition`,
  `layout`, …), a duplicate theme, or an out-of-table/empty value keeps the whole source.
- The description rules are the reviewed `PieTheme` rules, not the old profile's: unlike
  `pie_description_frontmatter` (which tolerates a blank payload line), `PieTheme`
  rejects a blank line inside the description block and an indented comment inside it —
  the stricter `xy_description_shape` rule — so the admitted shape is fully pinned. The
  old description-only profile is untouched. Likewise exactly one space follows the
  `theme:` colon in the leaf line (`  theme: dark`); the scalar's surrounding whitespace is
  then trimmed by `chart_yaml_scalar`, so `  theme:    dark` is not the pinned shape.
- A source carrying a theme leaf **without** the folded description keeps today's
  rejection: `pie_config` is not entered (it requires a `pie.textPosition` event), and the
  fallthrough `xy_visual_envelope` profile rejects the pie body.
- The body must still be a pie body (`chart_body_family == Pie`), and `PieTheme` is added
  to the explicit `lines.front().starts_with("pie")` guard beside `PieDescription`/
  `PieConfig`; the existing profile selection for xychart is untouched.

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
an off-by-one-safe `>` comparison. The footer attaches whenever the gate accepted a leaf
(`notices` non-empty) — including the degenerate title-only chart `pie_chart` returns for
an empty slice list — so an admitted source always discloses (G2). The degenerate probe is
pinned in the battery.

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
  corpus twin, and per-row shape validation. The classifier must not rely on the first
  body statement alone. Here "frontmatter matches" includes **value-table membership**:
  of the four body-blocked theme rows only `dark-theme.mmd` carries an in-table value and
  is excluded solely by its `accTitle:`/`accDescr:` body (which `parse_chart` rejects),
  while `default-/forest-/neutral-theme.mmd` are additionally excluded by the closed
  table. The admissible set is therefore "frontmatter matches (shape **and** value) and
  the body has no `accTitle:`/`accDescr:` statement" — the corpus-exact structural rule,
  pinned by the 5/4/8 split. The roster and the matrix artifact are never read.
- **Twin rule**: the frame equals the twin's frame plus the recorded literal footer delta,
  line by line, both sides through the same sentinel-bearing wrapper.
- Gate `tests/ui/test_mermaid_pie_0042_cohort.cpp`: ledger pins; per-row twin inertness
  with a **live** per-row `narrow_40` assertion; a closed-table battery with pinned
  verdicts (every admitted value with the exact footer block; unknown/case/empty;
  duplicate; **quoted scalar rejects**; **trailing comment rejects**; unmatched quote;
  an indented comment inside the config block rejects;
  `theme : dark` rejects — the raw line must be literally `  theme:`; indent 0 and indent
  4 reject, indent 2 is the admitted position; a payload line that looks like `theme:`;
  a blank line inside the description rejects; theme plus any other leaf; a second config
  block; theme without the folded description; theme plus an in-body `%%{init}%%`); a
  the four body-blocked theme fixtures (`dark-`, `default-`, `forest-`, `neutral-theme.mmd`)
  pinned as explicit whole-source negatives (the body parser rejects `accTitle:`/
  `accDescr:`), so a future body-parser widening cannot silently promote a sixth row; the
  empty-slice degeneracy (`pie title X` with an admitted theme) drawing with the footer; a
  partition test against the Pie7 rows; and the drawn-row frame-identity oracle over the
  seven Pie7 paths.
  The twin comparison is **wide-only** (`kWide`), exactly like the git/agentflow gates;
  narrow is a separate ledger-state assertion.
- **Adjacent guards that must stay green** (they exercise the same profile surface and
  are part of the change set's verification, not its edits):
  `tests/ui/test_mermaid_charts_semantics.cpp` `MermaidPie7.SupplementarySourceControls`
  (`config_root_is_valid_unsupported` with `config.pie.textPosition` and
  `one_space_payload_is_valid_unsupported` must remain whole-source),
  `tests/ui/test_mermaid_xychart_0035_cohort.cpp`'s pie-body + `xyChart` config fallback
  guard, and `tests/ui/test_mermaid_envelope.cpp`'s pie budget probe.
- **Existing matrix support**: the Pie7 rows are hardcoded in
  `tests/ui/test_mermaid_official.cpp` (the "pie formal matrix support is deliberately
  file-local" block); the new sibling-ledger branch is placed **after** that block (so
  the special Pie7 branch can never be shadowed) and the two row sets are provably
  disjoint. **Accounting path is pinned**: the new branch leaves
  `raw_active_schema` empty, so the generic `++originals_drawn` counter credits the five
  rows (1285 → 1290) without a new accumulator and without touching the owned-contract
  skip list. The published `Pie7_native_whole_source` summary keeps its whole-scene
  arithmetic intact (`whole_sources_admitted` 7 + `mandatory_remaining` 17 = `target`
  24) — the five RFC 0042 rows are terminal-track admissions, not whole-scene ones — and
  its `scope` string is extended to name the separate RFC 0042 terminal theme cohort, so
  the artifact neither contradicts the new census nor breaks its own reconciliation.
- Roster migrated with `fallback_attribution.py --emit` in the phase commit
  (674 → 669; `family_residual:pie` 17 → 12, non-empty).

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `theme` admission | 5 rows | done | pie 7/17 → **12/12**; global 1471/674 → **1476/669**; formal `drawn_at_240` **1285 → 1290**; gaps 48, integrity 0; narrow_40 measured **0/5**; dual-preset sha `b00ab2c34b602c2aff4a2cae17e82d8a26dbad7aca35fa9a503d3d7cc4a0db8c` |

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
- `fallback_attribution.py` in the only order that can be green: review approved → full
  rebuild → both preset matrices → **cohort gate green** → `--emit` (write the migrated
  roster) → `--self-test` → `--matrix` (the drift gate compares the live matrix against
  the migrated roster). Running `--matrix` before `--emit` fails by construction (live
  669 vs pinned 674); the same correction applies to RFC 0041's identically worded
  sentence, which this phase fixes as a docs correction.
- **False-green guardrails.** The matrix branch is a ledger echo; the render proof is the
  cohort gate. The full build must precede any matrix verdict, and the report target is
  rebuilt in both presets before the sha compare. A red full gate is handled per D-401 §7.

## Documentation impact

- [ ] `docs/dev/d408-next-family-pie.md` — selection record (already written)
- [x] `docs/dev/d409-pie-rfc0042-closure.md` — closure record
- [x] `docs/decisions/design-decisions.md` — pie config admission entry
- [x] `docs/README.md` — RFC index
- [x] `docs/dev/mermaid-d117-family-fallback-counts.md` — pie row 17/7 → 12/12
- [x] `docs/dev/engineering-efficiency.md` — per-task retro
- [x] `tests/fixtures/mermaid_native/direct_theme_source_audit.v1.json` — unconsumed
      point-in-time census; annotate as superseded by RFC 0042 (it lists pie rows as
      `source_fallback` with a pre-phase `matrix_sha256`)

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: does the new profile share xychart's description-shape helper? | design review | **Resolved (design review r2)**: no — reuse the payload-indent rule but take the stricter blank-line rule (`xy_description_shape`), keep the profiles separate and the old description-only profile byte-unchanged |
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
| 2026-11-22 | r1 | independent design-review agent (own re-derivation; read the charts TU, the envelope reader, the Pie7/precedent gates, the roster, the gap roster and the matrix) | REQUEST-CHANGES | Every partition and endpoint re-derived as correct (24 = 7 + 5 + 8 + 4; five values; 12/12; 1476/669; 1290; gaps 48; integrity 0; roster 669 with `family_residual:pie` 17→12; corpus twin for `dark`, no drawn peer for the four redux rows). **MAJOR**: the scalar rule misattributed behaviour to `chart_yaml_scalar` (it neither trims comments nor dequotes; `event.value` keeps the trailing comment) and its own "trailing comment positive" battery case could not pass without an unstated `strip_comment` that would diverge from `PieConfig` → **fixed by revision**: the leaf is read exactly like `PieConfig` (raw `event.value`, `chart_yaml_scalar`, leading quote rejected), so quoted and comment-bearing values are negatives; the battery pins them as such. **MINORs**: blank-line-inside-description was contradictory → `PieTheme` takes the stricter reject rule (the old profile is untouched); the `drawn_at_240` accounting path was unspecified → the new branch leaves `raw_active_schema` empty so the generic counter credits 1285→1290, and the stale `Pie7_native_whole_source` summary is updated in the same commit; the adjacent `test_mermaid_charts_semantics` supplementary controls and the xychart pie-body guard are named in the change set; the generator cannot classify by the first body statement alone → the corpus-exact rule excludes bodies carrying `accTitle:`/`accDescr:`; the twin delta is wide-only and the footer's longest line is 60 (not 64) characters. **NITs**: "exactly the corpus" reworded to the reachable-row values; the theme-without-description rationale corrected (it falls through to `xy_visual_envelope`, not `pie_config`); `theme : dark` and indent verdicts pinned; `PieTheme` added to the `starts_with("pie")` guard; the empty-slice footer path excluded. |
| 2026-11-22 | r2 | same design-review agent, re-read at 5c697207 against the live code | APPROVE-TO-ACCEPT | All five gating items verified resolved in substance: the scalar rule now matches `PieConfig` exactly (raw `event.value`, `chart_yaml_scalar`, leading quote rejected; quoted/comment/unmatched all negatives), `PieTheme` takes the stricter blank-line rule with the old profile untouched, the 1285→1290 accounting path is the live generic counter with no new accumulator, the classifier's structural rule reproduces 5 of 24, and the twin comparison is wide-only with the footer's longest line at 60 characters. MINOR-4 and NITs 7–11 verified folded in. Two new MINORs folded into this revision before the gate: the `Pie7_native_whole_source` summary must **keep** its whole-scene arithmetic (7 + 17 = 24) and express the RFC 0042 contrast in its scope string — the five rows are terminal-track, not whole-scene admissions — and the generator's "frontmatter matches" must be stated to include value-table membership (`dark-theme.mmd` is the sole value-valid row excluded by its body). NITs also folded: OQ-1 wording, an explicit indented-comment battery case, the raw-line-vs-`event.path` note, the G1 wording, and the Review History wording. Gate to `accepted` passed. |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | — |
| 2026-11-22 | — | Accepted after design review r2 | — |
| 2026-11-22 | — | Implementable after PRR r1 (MAJOR folded) | — |
| 2026-11-22 | P1 | `PieTheme` profile + disclosure footer; 5 rows promoted; corpus/synthetic twin ledger (digest `3caa028a…`); 6 gtest + 1 hermetic `--check`; matrix branch after the Pie7 block; roster 674 → 669; narrow_40 measured 0/5 | `a1c5f93e` |
| 2026-11-22 | — | Closure: full debug ctest, dual-preset byte gate, D-409 record, design-decisions and README entries, d117/audit census updates, efficiency retro | see D-409 |
| 2026-11-22 | p1-code-r1 | independent read-only adversarial code-review agent (grammar/twins/footer/generators/matrix/hidden tests) | APPROVE | Zero BLOCKER/MAJOR. Verified live: the PieTheme profile matches the RFC shape exactly, PieConfig/PieDescription are byte-unchanged, the footer budget is byte-identical to the six sibling paths (emission after `pie_chart`, so the title-only chart discloses), the corpus/synthetic twin gate recomputes sha and body equivalence, the narrow 0/5 state is live-asserted, the matrix branch sits after the Pie7 special branch and credits the generic counter, and no protected file or exported interface changed. MINORs fixed: the RFC placement sentence now says "after the Pie7 block"; the generator strips CR before classifying. NITs fixed: the emission block re-checks the 96-byte cap for sibling parity, and the in-table no-description negative pins the body-family guard. |
| 2026-11-22 | prr-r1 | independent PRR agent (production-readiness lens; graduation measurability, hidden tests, build/Work, false-green, rollback, couplings, precedent) | REQUEST-CHANGES -> folded | Every product-side number re-derived live (1471/674, 1285, gaps 48, integrity 0, pie 7/17; the five rows original-track `family_residual:pie`, absent from the protected gap roster; the +5 accounting path is the generic counter; a simulated promotion gives 669 / pie 12 / formal 1290 / `--self-test` green). Build is body-only, no BMI change; Work formulas byte-identical to five siblings; no shared-contract change; mixed corpus/synthetic twins match RFC 0037. **MAJOR**: the testing plan's fixed order put `--matrix` before `--emit`, which fails by construction (live 669 vs pinned 674) → folded: the only green order is review approved → full rebuild → both matrices → cohort gate green → `--emit` → `--self-test` → `--matrix` (and RFC 0041's identically worded sentence is corrected as a docs fix). MINORs folded: the footer now attaches whenever a leaf was accepted, including the title-only empty-slice chart (G2); `PieTheme` takes the stricter `xy_description_shape` rule for an indented comment inside the payload and pins exactly one space after `theme:`; the four body-blocked theme rows are explicit whole-source negatives in the gate; the point-in-time `direct_theme_source_audit` snapshot joins the documentation impact; D-408's footer length is corrected to 60 characters. |
