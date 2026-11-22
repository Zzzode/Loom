---
rfc: 43
title: ishikawa, venn and railroad theme admission
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0043 — ishikawa, venn and railroad theme admission

## Summary

D-410's root-cause bill isolates exactly **5 remaining rows** whose only blocker is an inert
`config.theme` leaf: ishikawa 2 (forest, dark), venn 2 (redux-color, redux-dark-color) and
railroad 1 (dark). Each family's TU-local key-based frontmatter gate rejects any
`theme`/`look`/`config` key outright; this RFC widens each gate for exactly one shape — a
`config:` block whose sole leaf is an indent-2 `theme` in a closed corpus table — and
discloses the accepted value through a bounded footer appended below the art.

One phase, three TU-local implementations (no shared module, no BMI change). Target:
ishikawa **7 → 9** drawn / **6 → 4** fallback; venn **11 → 13** / **10 → 8**;
railroad **8 → 9** / **1 → 0**; global **1476 / 669 → 1481 / 664**; formal
`drawn_at_240` **1290 → 1295**.

## Motivation

### Selection (D-411, measured 2026-11-22)

D-410 buckets the 669 remaining wide fallbacks: 222 exact-bit (flowchart, non-roster),
67 protected/frozen (43 in the protected gap roster + 24 ER107/ER206), 380 capability
debt, of which only **5 are inert theme-only**. Those 5 are the entire remaining
"inert key" vein; three small families carry them behind identical key-based gates.

### Evidence

- All 5 rows strip-draw at 240 columns; the rest of each family's fallbacks are
  `useMaxWidth`/`diagramPadding`/`look: handDrawn`/seed rows or body capability.
- Corpus twins exist for 4 of the 5 rows: ishikawa's two theme rows share the body of the
  drawn `ishikawa/pr-check/1-should-render-a-simple-ishikawa-diagram.mmd`; venn's two
  share the drawn `venn/pr-check/16-should-render-a-venn-diagram-with-a-3-set-union-without-explicit-pairwise-subsets.mmd`.
  The railroad row has no drawn body-equivalent and uses the synthetic config-free twin.
- The 5 rows sit in `family_residual:{ishikawa,venn,railroad}`; after promotion the
  railroad bucket empties, which the RFC 0040 empty-category emission semantics already
  handle (`--self-test` key-set invariant intact).
- None of the 5 is in the protected `e6c-mandatory-gap-roster.json` (class/flowchart
  only), so `mandatory_gaps` stays 48.

### Why admission is testable

None of the three renderers reads `theme`; the text surface has no colour contract. The
proof is the established two-part oracle: per-row twin inertness (corpus twin frame, or
the config-free synthetic twin's frame, plus exactly the ledger's literal footer delta,
wide-only) and a drawn-row frame-identity oracle per family over its already-drawn rows.

## Goals

- **G1.** Admit exactly the closed per-family tables: ishikawa {dark, forest};
  venn {redux-color, redux-dark-color}; railroad {dark}.
- **G2.** Disclose each accepted leaf through a bounded footer
  (`Ishikawa native projection v1` / `Venn native projection v1` /
  `Railroad native projection v1` + one line).
- **G3.** Promote exactly 5 rows; every other accept/reject verdict is unchanged — the
  gates keep rejecting every other visual key, and indented lines outside a config block
  stay ignored exactly as today.
- **G4.** No regression: each family's already-drawn rows keep their frames cell-for-cell
  and the dual-preset matrix stays byte-identical.

## Non-Goals

- `ishikawa.useMaxWidth`/`diagramPadding`, venn's `look`+`handDrawnSeed` rows, and any
  body-capability row.
- flowchart's 222 exact-bit rows and the 67 protected/frozen rows (the D-410 policy fork).
- narrow_40 drawing; painting the palettes; the ER107 B3 gates.

## Proposal

### 1. Closed tables (per family, corpus-bounded)

| family | admitted `look` | admitted `theme` |
|---|---|---|
| ishikawa | — | `dark`, `forest` |
| venn | — | `redux-color`, `redux-dark-color` |
| railroad | — | `dark` |

### 2. Grammar (config-block-scoped, TU-local, three copies)

Each gate keeps its existing envelope scan and its existing rule that any column-0
non-visual key keeps today's pass-through (only `theme`/`look`/`config` are gates). The
added rule, identical in all three TUs:

- A column-0 non-blank, non-comment line opens a block; a column-0 key other than
  `description`/`config` keeps today's gate (visual keys) or pass-through (other keys).
  Exactly one `config:` line is permitted; a second rejects.
- Inside the config block, indented lines are interpreted: blank lines and YAML comments
  are skipped; a line whose key (text before the first colon, leading spaces stripped, no
  trailing trim) is exactly `theme` is admitted only at indent 2 with a scalar read by the
  family's existing scalar path (trim, strip a trailing comment outside quotes, dequote a
  matched pair) in the table; every other leaf, a colon-less indented line, a duplicate, or
  a second `config:` rejects the whole source.
- A config block with no admitted leaf rejects. Indented lines outside a config block stay
  ignored exactly as today.
- The in-body `%%{init}%%`/style/classDef gates stay unchanged.

### 3. Disclosure footer

Blank separator, header, theme line; 96-byte cap; per-line charge
`size * 4 + sizeof(std::string) + 32`; emission
`(1 + n) * (4 * sizeof(std::string) + 256) + bytes * 2 + 256`; the total must satisfy
`art + 1 + n <= kHeightLimit (1024)`; failing either bound keeps the whole source. The
validated leaves travel from each gate to its layout through the parser and are appended
after the art.

### 4. Reviewed cohort, twin gate and wiring

- One sibling ledger `tools/mermaid/smallfam-0043-cohort.json` (schema
  `loom_mermaid_smallfam_0043_cohort/v1`) with the 5 rows (family field), literal
  `expected_footer_lines`/`expected_states`, and `twin` (corpus with path+sha for the four
  body-identical rows, synthetic for the railroad row).
- The three family ledgers gain a four-way partition
  (`rows + admitted + frontmatter_gated(remaining) + body_style_gated` = originals:
  ishikawa 7+2+4+0, venn 11+2+8+0, railroad 8+1+0+0); generators updated structurally and
  regenerated, never hand-edited.
- Generator `tools/mermaid/generate_smallfam_0043_cohort.py`, renderer-free,
  `--check`-reproducible, with pinned path sets/digests and body-equivalence verification.
- **Twin rule**: wide-only frame equality = twin frame + the literal footer delta through
  the same sentinel wrapper; narrow is a separate live ledger-state assertion (rule 34).
- Gate `tests/ui/test_mermaid_smallfam_0043_cohort.cpp`: ledger pins; per-row twin
  inertness; one closed-table battery per family (every admitted value with the exact
  footer; out-of-table/sibling/case/empty/duplicate/quoted/comment/unmatched/`theme :`;
  indent 0/2/4; payload and body-init rejects; a config with any other leaf; a second
  config); per-family partition checks against their ledgers; and a drawn-row
  frame-identity oracle over each family's drawn rows (7 + 11 + 8).
- Matrix branch for the 5 rows before the generic fallback, reason
  `RFC-0043 small-family look/theme reviewed cohort`, `raw_active_schema` empty.
- Roster migrated with `fallback_attribution.py --emit` after the cohort gate is green:
  669 → 664; `family_residual:railroad` empties (supported since RFC 0040).

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `theme` admission | 5 rows, 3 families | proposed | ishikawa 7/6 → **9/4**; venn 11/10 → **13/8**; railroad 8/1 → **9/0**; global 1476/669 → **1481/664**; formal **1290 → 1295**; gaps 48, integrity 0; narrow measured per row; dual-preset sha recorded |

Closure: full debug ctest once (D-401 §7 discipline), dual-preset byte gate, RFC
`implemented`, closure doc, design-decisions entry, efficiency retro.

**Closure-report format (TL 2026-11-22):** every closure report states three numbers side
by side and none may replace another — ① the absolute `source_fallback` total (expected
**664**), ② the part of it beyond a certified profile or frozen/protected authority
(expected **289 = 222 exact-bit + 67 protected/frozen**, unchanged by this phase), and
③ this phase's net reduction (**5**).

## Production Readiness Review

Completed at the `implementable` gate. Answers:

- **Correctness & tests** — per-row twin gate, one battery per family, per-family
  partition checks, and three frame-identity oracles.
- **Build system** — three body units, no module/import/BMI change; one new test executable.
- **Rollback** — one atomic commit; one revert plus a rebuild/re-measure.
- **Observability** — no new diagnostics.
- **Documentation** — RFC, closure doc, design-decisions entry, docs index, d117 rows, retro.
- **Deletion** — the three old flat gates are replaced, not wrapped.
- **Deprecation** — additive; one roster category empties (handled).

## Rollout and rollback

One phase lands P1. Rollback is one revert plus a rebuild/re-measure. No persisted user
data, no wire protocol, no CLI contract changes.

## Drawbacks and risks

- **Three TU-local copies.** A shared helper was rejected for the same BMI-fan-out reason
  as RFC 0040/0041; the per-family batteries keep them from drifting.
- **A disclosed leaf the renderer cannot paint** — stated by the footer.

## Alternatives considered

1. **Do nothing.** Leaves the last 5 inert-key rows behind while six families already
   disclose colours.
2. **Three separate RFCs.** Rejected: near-identical documents and review chains for 1–2
   rows each.
3. **Flowchart's 222 rows instead.** Rejected: exact-bit machine and the protected roster.
4. **Folding in the `useMaxWidth`/`diagramPadding`/seed rows.** Rejected: non-inert
   (layout/geometry) by the D-410 classification.

## Testing and verification plan

- New gate (six gtests) + one hermetic `--check` ctest for the new generator and one per
  existing family generator (three), registered in `tests/ui/CMakeLists.txt`.
- Targeted `ctest -L mermaid` in the phase; release matrix in the phase; dual-preset byte
  gate at closure; full debug ctest once at closure.
- Roster in the only green order: review approved → full rebuild → both matrices → cohort
  gate green → `--emit` → `--self-test` → `--matrix`.
- **False-green guardrails.** The matrix branch is a ledger echo; the render proof is the
  cohort gate; full build before any matrix verdict; both presets rebuilt before the sha
  compare; a red full gate handled per D-401 §7.

## Documentation impact

- [ ] `docs/dev/d411-next-family-ishikawa-venn-railroad.md` — selection (already written)
- [ ] `docs/dev/d412-small-family-rfc0043-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — small-family admission entry
- [ ] `docs/README.md` — RFC index
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — ishikawa/venn/railroad rows
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro
- [ ] `tests/fixtures/mermaid_native/direct_theme_source_audit.v1.json` — annotate superseded

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: one ledger for three families or three sibling ledgers? | design review | proposed: one ledger with a `family` field (5 rows, one phase record) |
| OQ-2: does the railroad row (no corpus twin) use a synthetic twin? | design review | proposed: yes (RFC 0039 precedent) |
| OQ-3: narrow_40 disposition | implementation | ledger `expected_states`, measured live |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR with a different lens.
- Phases: one atomic phase, with its review record.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | — | — | draft | Created provisional from D-410's 5d bucket |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | — |
