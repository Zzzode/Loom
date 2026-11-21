---
rfc: 41
title: agentflow look and theme admission
status: implemented
owners: "@Zzzode"
reviewers: ["agent:a255f982730d3c06d rfc0041-design-review (r1 REQUEST-CHANGES -> r2 APPROVE-TO-ACCEPT)", "agent:a2a24cca22d776afd rfc0041-prr (r1 APPROVE-TO-IMPLEMENT)"]
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0041 — agentflow look and theme admission

## Summary

The agentflow family has 38 official-corpus originals; 27 draw natively and 11 are
rejected by its own frontmatter gate `af_frontmatter_gated`, which today admits only a
top-level `description`. D-406 measured the remaining reachable slice under the
reachable-increment-per-cost criterion: **9 of the 11** gated rows are blocked *only* by
a `config.theme` leaf; the other two also carry `config.look: handDrawn` **and**
`config.handDrawnSeed`, a geometry seed that is not inert and therefore is not admitted.

This RFC reuses the pattern RFC 0038/0039/0040 validated three times: a closed,
corpus-bounded `theme` table validated at the admitted position and disclosed through a
bounded footer below the art, implemented TU-locally in
`src/ui/visual/mermaid_agentflow_impl.cpp` (no shared module, no BMI change). Target:
**9 rows** promoted in **one atomic phase**: agentflow **27 → 36** drawn / **11 → 2**
fallback; global **1462 / 683 → 1471 / 674**; formal `drawn_at_240` **1276 → 1285**.

## Motivation

### Family selection (D-406, measured 2026-11-22)

The criterion is reachable drawn increment per unit of new mechanism, not raw fallback
count. D-406 re-reads the D-402 strip-probe log against the current 683-row fallback
roster (the 43 rows promoted by RFC 0039/0040 excluded; 683/683 coverage verified) and
cross-checks every per-family fallback count against the current release matrix
`fd852b74…`. Of the 218 remaining strip-draw rows:

| candidate | reachable inert rows | mechanism surface | risk |
|---|---|---|---|
| **agentflow** | **9** | **1** — rewrite its own `af_frontmatter_gated` into a validating gate and append a footer in `agentflow_layout` | low: single file, single family, no shared contract |
| flowchart | 8–11 | one but huge — the 2800-line exact-bit profile machine | high: RFC 0036 was withdrawn after measurement falsified it |
| pie | 4 | one — extend the existing `pie_config_frontmatter` gate | low but smallest useful increment |
| venn / ishikawa | 2 | one — extend their existing key-based frontmatter gates | low, too small |
| git 38 / requirement 26 / state-v2 6 | 0 inert | semantic or geometric capability, not admission | out of scope by construction |

### Evidence

- 38 agentflow originals = 27 drawn + 9 theme-only + 2 `look`+`handDrawnSeed`.
- The 9 rows are two bodies × themes: the "11 every-container-kind" body under
  {dark, base, default, forest, neutral} and the "15 support-triage" body under
  {dark, default, redux-color, redux-dark-color}. Seven distinct theme values.
- **Corpus twins exist**: every one of the 9 bodies is byte-identical (frontmatter
  stripped) to an already-drawn fixture — the five container-kind rows equal
  `agentflow/visual-check/10-every-container-kind.mmd`, the four triage rows equal
  `agentflow/visual-check/15-support-triage.mmd`. Corpus twins take precedence over
  synthetic ones (RFC 0037/0038 rule), so the ledger records `twin.kind = "corpus"` with
  the twin path and the frame oracle compares against the drawn twin's own frame.
- All 11 fallbacks sit in the roster's `edge_animation_metadata` category (91 rows;
  the detector is a co-occurrence predicate, drawn 151 / matched 242, not a proven
  sufficient blocker), so no sufficiency pin flips; after the 9 promotions the same
  category reads matched 242 / drawn 160 / fallback 82 and does not empty, and
  `proven_compact_cohort_candidates` stays {`click_interactive_callback`,
  `linkstyle_directive`}.
- None of the 9 rows appears in the protected `e6c-mandatory-gap-roster.json`
  (which lists only class/flowchart rows), so `mandatory_gaps` stays 48.
- The strip probe proves the bodies render natively once the frontmatter is removed; the
  per-row proof is the twin gate this RFC adds.
- **narrow_40 measured: 0 of 9 draw at 40 columns.** Two causes, both measured: the
  footer's longest line (`config.theme=<value> -> colour palette; not applied`) exceeds
  a 40-column viewport, so the five container-kind rows — whose footerless corpus twin
  draws at 40 — fall back once admitted; the four triage rows were already too wide
  (their twin's narrow state is `source`). The 19 already-drawn rows keep their prior
  narrow dispositions (family narrow 19/19 unchanged). This is the PRR's "measure, never
  transcribe" lesson in action: the pre-implementation corpus-twin prediction (5 draw)
  was falsified by the live gate and the ledger records the measured `source` state for
  all nine.

### Why admission is testable

The renderer never reads `theme`; the text surface has no colour contract. The proof is
the same two-part oracle RFC 0037/0038/0039/0040 used: per-row twin inertness (the
admitted frame equals its **corpus twin's** frame plus exactly the ledger's literal footer
delta — the synthetic config-free twin is only the fallback when no corpus twin exists)
and a drawn-row frame-identity oracle over the 27 rows the family already draws (identical
untrimmed cell surfaces with and without the frontmatter, no footer markers).

## Goals

- **G1.** Admit exactly the seven corpus `theme` values as a closed table at the
  admitted position; everything else keeps today's whole-source fallback.
- **G2.** Disclose each admitted leaf through a bounded footer
  (`Agentflow native projection v1` + one line) that names the value the renderer
  actually accepted.
- **G3.** Promote exactly 9 rows; the change is a strict widening for exactly one shape
  (a column-0 `config:` whose sole leaf is an indent-2 in-table `theme`) and changes no
  other accept/reject verdict — in particular indented lines outside a config block stay
  ignored, as today.
- **G4.** No regression: the 27 drawn rows keep their frames cell-for-cell, the family is
  a three-way partition (`rows` 27 + `admitted` 9 + `gated` 2 = 38), and the dual-preset
  matrix stays byte-identical.

## Non-Goals

- The 2 `look: handDrawn` rows. `look` alone is not admitted (no corpus row carries it
  without a seed) and `handDrawnSeed` is a geometry seed — a disclosed-only treatment
  would misrepresent it.
- Any other key: `title`, namespaced `agentflow.*` maps, `fontFamily`, `config` with any
  other leaf, an empty `config` block. All keep today's rejection.
- narrow_40 drawing; painting the upstream palettes; the ER107 B3 gates; other families.

## Proposal

### 1. Closed value table (family, corpus-bounded)

| family | admitted `look` | admitted `theme` (rejects everything else) |
|---|---|---|
| agentflow | — (none) | `base`, `dark`, `default`, `forest`, `neutral`, `redux-color`, `redux-dark-color` |

Seven values, exactly the corpus. Any other value — including `redux` and `redux-dark`
(a sibling family's values), `neo`, case variants, an empty value, a quoted-unmatched
value — rejects the whole source.

### 2. Grammar (config-block-scoped line scan, TU-local)

`af_frontmatter_gated` keeps every existing envelope rule unchanged: exact column-0
`---` fences (an indented fence cannot close the envelope; an unclosed fence rejects), a
tab anywhere in the source rejects in the parser before the gate, a column-0 non-comment
line needs a colon, and a column-0 key other than `description` (compared after
`af_trim`, so `description : x` is the same key) rejects the whole source. Indented lines
**outside** a config block are still skipped exactly as today, so a `description:` folded
payload that happens to read `theme: dark` is neither a visual key nor a new rejection.
The RFC adds exactly one interpretation, scoped to the config block:

- A column-0 non-blank, non-comment line opens a block; its key is the text before the
  first colon, `af_trim`med. `config:` opens the config block; every other column-0 key
  closes it. **Exactly one `config:` line is permitted; a second `config:` line rejects
  the whole source** (so `config:` carrying the admitted leaf followed by an empty second
  `config:` is a reject, not an accept).
- **Inside the config block** (and only there) an indented line is interpreted:
  - blank lines and YAML comment lines (`#…` after indentation) are skipped;
  - a line that contains a colon whose key (text before the first colon, leading spaces
    stripped, **no trailing trim**) is exactly `theme` is a **visual key**. It is
    admitted only when the indentation is exactly 2 spaces and the scalar (trim, strip a
    trailing comment outside quotes, dequote a matched pair) is in the table;
  - **every other leaf rejects the whole source** — a non-visual key (`look`,
    `handDrawnSeed`, `fontFamily`, …), a visual key at indent 0 (already a column-0
    reject) or indent ≥ 4, a colon-less indented line that is not blank/comment, a
    duplicate `theme`, `theme : dark` written with a space before the colon (the key
    text is `theme `, not `theme`), or an unmatched quote.
- **A config block must carry exactly one admitted leaf.** An empty config block, a block
  whose only leaves are non-visual, and a block with a visual key plus anything else all
  reject the whole source — so `config: {theme: dark, look: handDrawn, handDrawnSeed: 1}`
  rejects rather than silently dropping the non-inert leaves. This preserves today's
  contract for every non-admitted frontmatter, making the change a strict widening for
  exactly one shape (a column-0 `config:` whose sole leaf is an indent-2 in-table
  `theme`).
- Every other column-0 key still rejects. Unlike RFC 0040's three families, agentflow has
  **no pass-through**: it admits only `description` today, and this RFC does not widen
  that.

The scalar reader trims both sides, then strips a trailing YAML comment outside quotes,
then dequotes a matched pair; an unmatched quote is a mismatch, never a value. Each
scalar read charges `work.spend(value.size() + 1)` (accounting parity with the other
families; the entry prepayment already bounds the scan). The in-body `%%{init}%%`
directive continues to reject (the body parser already rejects it; the RFC adds a probe).

### 3. Disclosure footer

Emitted below the art when exactly one leaf was accepted, with one blank separator line
first:

```
Agentflow native projection v1
config.theme=<value> -> colour palette; not applied
```

Each line is capped at 96 bytes and charged at `size * 4 + sizeof(std::string) + 32`;
emission charges `(1 + n) * (4 * sizeof(std::string) + 256) + bytes * 2 + 256`; the total
must satisfy `art_lines + 1 + n <= kAfMaxRows (1024, the same value as the sibling
families' `kHeightLimit`)`; failing either bound keeps the
whole source. This is byte-for-byte the state/flow/git/journey-family budget (their
`kHeightLimit` is the same 1024). The height guard reads an off-by-one-safe `>`
comparison, so `art + 1 + n == 1024` is admitted. The footer is appended inside
`agentflow_layout` after the edge pass; the gate's notices are carried out of the parser
(the parser stores the admitted leaves it accepted) so the layout appends exactly what
the gate validated.

### 4. Reviewed cohort, twin gate and wiring

- `tools/mermaid/agentflow-cohort.json` keeps its 27 `rows` and shrinks `gated_count`
  from 11 to **2** (the seed rows), adding `admitted_count: 9` plus an `admitted` path
  list and digest; `family_total` stays 38, `native_draw_count` stays **27** (it counts
  ledger rows, which do not change), and the generator asserts
  `native_draw_count + admitted_count + gated_count == family_total`. The
  `narrow_40_draw_count`/`narrow_40_fallback_count` pins stay 19/8 for the 27 rows.
- New sibling ledger `tools/mermaid/agentflow-0041-cohort.json` (schema
  `loom_mermaid_agentflow_0041_cohort/v1`) with the 9 rows in one section and one phase
  review record. Each row pins the literal `expected_footer_lines` (blank separator,
  header, theme line), `expected_states`, and its **corpus twin** —
  `twin: {kind: "corpus", path: <drawn fixture>}` — exactly like the RFC 0037/0038
  ledgers. `NARROW_DRAW` is populated only from a **live 40-column render** — never
  transcribed from the corpus twin — and the ledger's `twin` object also pins the twin's
  `sha256` (RFC 0037's corpus-twin shape; RFC 0038/0039/0040 supply the footer and
  `expected_states` shape).
- Generator `tools/mermaid/generate_agentflow_0041_cohort.py`, renderer-free,
  `--check`-reproducible, with pinned literal path sets and digests, source-derivable
  counts (38 originals / 27 drawn / 9 admitted / 2 still gated), body-equivalence
  verification for each corpus twin, and per-row shape validation (description + config
  with exactly one theme leaf). The roster and the matrix artifact are never read.
- **Twin rule**: each admitted frame equals its corpus twin's rendered frame plus the
  recorded literal footer delta, line by line, both sides through the same
  sentinel-bearing wrapper. The corpus twin is body-identical and its own frontmatter is
  description-only, so the delta is exactly the footer; the synthetic config-free twin
  is the fallback only if a corpus twin is unavailable (none here).
- Gate `tests/ui/test_mermaid_agentflow_0041_cohort.cpp`: ledger pins; per-row corpus
  twin inertness; a closed-table battery covering every admitted value, sibling values
  (`redux`, `redux-dark`), an unknown value, a case variant, an empty value, a duplicate
  leaf, a quoted value, a trailing comment, an unmatched quote, `theme : dark` (space
  before the colon), indent 0 / 2 / 4, a description payload line that looks like
  `theme:` (still draws, no footer — today's skip), an admitted leaf plus an in-body
  `%%{init … theme …}%%` line (rejects), `theme` combined with `look` / `handDrawnSeed` /
  `fontFamily` (rejects), a `config:` carrying only a non-visual leaf (rejects), an empty
  `config:` (rejects), and a second `config:` line both with a duplicate theme and with an
  empty second block (both reject) — all with hardcoded
  verdicts and literal footer strings; a **live per-row `narrow_40` assertion** for every
  admitted row (`EXPECT_EQ(draws(source, kNarrow), expected_states.narrow_40 == "drawn")`,
  mirroring `test_mermaid_journeyfam_0040_cohort.cpp`), so a transcribed narrow pin cannot
  survive; plus the drawn-row frame-identity oracle over the 27 already-drawn rows (full
  untrimmed character surfaces, footer-marker absence, line-equality native predicate).
  Planned TEST cases: LedgerSchemaReviewsAndPins, RowsAreInertAgainstCorpusTwins,
  ThemeTableIsClosed, GrammarBattery, FamilyLedgerPartition, DrawnRowsKeepFrameIdentity.
- **Existing family gate** `tests/ui/test_mermaid_agentflow_cohort.cpp` is updated in the
  same change: the `gated_count` pin 11 → 2, a new `admitted_count == 9` pin, and
  `FrontmatterGatesStayClosed` becomes an out-of-table negative battery (`theme: neo`,
  `redux`, `redux-dark`, a bare `look`, the two seed rows) while the synthetic
  `config:\n  theme: dark` probe flips to a positive assertion (it now draws with the
  footer).
- Update `generate_agentflow_cohort.py` to classify structurally (the same config-block
  scan: a `theme` leaf at indent 2 inside `config:`, in-table value, exactly one leaf)
  instead of today's `key != description` column-0 scan; regenerated ledger, never
  hand-edited.
- Matrix branch in `tests/ui/test_mermaid_official.cpp`: one Journey-style map keyed by
  path covering the 9 rows, placed before the existing agentflow cohort branch and the
  generic fallback, reason `RFC-0041 agentflow look/theme reviewed cohort`.
- Roster migrated with `fallback_attribution.py --emit` in the phase commit
  (683 → 674; `edge_animation_metadata` 91 → 82 with matched 242 / drawn 151 → 160 /
  fallback 91 → 82; the category does not empty and the compact candidates are
  unchanged).

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `theme` admission | 9 rows | done | agentflow 27/11 → **36/2**; global 1462/683 → **1471/674**; formal `drawn_at_240` **1276 → 1285**; gaps 48, integrity 0; `gated` 11 → 2; narrow_40 measured **0/9** (footer exceeds the 40-column viewport); dual-preset sha `326fb96f00c73516b7bd3ddb1397563ff7d0aed9208e25c839f1f60c1eeb6853` |

Closure: full debug ctest once (D-401 §7 discipline), dual-preset byte gate, RFC
`implemented`, closure doc, design-decisions entry, efficiency retro.

## Production Readiness Review

Completed at the `implementable` gate. Answers:

- **Correctness & tests** — per-row twin gate, one closed-table battery, and a 27-row
  frame-identity oracle; known flakes are pre-existing.
- **Build system** — no new module, target or import for production code (one new test
  executable); one implementation unit recompiles, then archive and executable relinks.
  BMI PSS N/A (no interface change). Fan-out measured by the PRR, not asserted.
- **Rollback** — one atomic commit containing the renderer, the new ledger and generator,
  the updated family generator/ledger, the gate, the matrix branch, the CMake
  registrations, and the roster; one revert **plus a rebuild and matrix re-run** restores
  the previous census exactly (the census is measured, not committed).
- **Observability** — no new diagnostics; session traces unaffected.
- **Documentation** — RFC, closure doc, design-decisions entry, docs index, retro.
- **Deletion** — the old `key != description` column-0 gate is replaced, not wrapped.
- **Deprecation** — the family ledger's `gated` list shrinks; no persisted format
  changes (ledgers are tool-generated).

## Rollout and rollback

One phase lands P1. Rollback is one revert plus a rebuild/re-measure. No persisted user data, no wire protocol, no
CLI contract changes. The only data artifacts are regenerable ledgers and the roster.

## Drawbacks and risks

- **A disclosed leaf the renderer cannot paint.** Accepted and documented: the footer
  states the palette is not applied, exactly like the three prior admissions.
- **Seven-value table bias.** The table is the corpus, which is the RFC 0038/0039/0040
  rule; a future corpus update must regenerate and re-review.
- **The 2 seed rows remain fallback.** That is the honest disposition of a non-inert
  key, not a gap.

## Alternatives considered

1. **Do nothing.** Leaves 9 rows on fallback whose only blocker is an inert palette leaf,
   at zero cost but zero gain; the surrounding families already admitted this exact
   shape, so the inconsistency is a maintenance hazard.
2. **A shared admission helper across the four families.** Rejected: the mermaid detail
   module is imported by 37 implementation units and a shared declaration is a real BMI
   fan-out for a 9-row slice; the established pattern is a TU-local envelope plus a
   cross-family test.
3. **Admit `look` and `handDrawnSeed` too.** Rejected: the seed changes geometry; a
   disclosed-only footer would misstate a render-affecting input.
4. **Flowchart's theme rows instead.** Rejected: same increment at much higher risk
   (exact-bit profile machine; RFC 0036 precedent).

## Testing and verification plan

- New gate `tests/ui/test_mermaid_agentflow_0041_cohort.cpp` (six gtests as enumerated in
  §4) registered in
  `tests/ui/CMakeLists.txt`, plus a hermetic `--check` ctest for the new generator and one
  for `generate_agentflow_cohort.py`. The existing
  `tests/ui/test_mermaid_agentflow_cohort.cpp` is updated in the same change (count pins
  and the replaced negative battery, as §4 states).
- Targeted `ctest -L mermaid` in the phase; release-preset matrix in the phase; dual-preset
  byte gate at closure; full debug ctest once at closure.
- `fallback_attribution.py --emit` followed by `--self-test` and `--matrix`.
- **False-green guardrails.** The matrix branch is a ledger echo; the render proof is the
  cohort gate. The phase edits both a production body and the byte-pinned report test, so
  before trusting any matrix verdict the **full build must have run** (EE rule 3 — verify
  the binary mtime; a target-only build leaves a stale-linked report) and
  `test_mermaid_official` is rebuilt in both presets before the dual-preset sha compare
  (rule 28). A census command runs from the preset build directory and the artifact sha
  must change, not just the exit code. The ordering is fixed: review approved → full
  rebuild → matrix in both presets → cohort gate green → `--emit` → `--self-test` →
  `--matrix` (a matrix run against a still-pending ledger truncates the report, so the
  roster must never be emitted from it; and `--matrix` before `--emit` fails by
  construction, live 674 vs pinned 683). A red full gate is handled per D-401 §7 — fixed, or evidenced by ≥3 clean
  reruns plus unrelatedness; never laundered by a partial rerun.

## Documentation impact

- [ ] `docs/dev/d406-next-family-agentflow.md` — records the selection (already written)
- [x] `docs/dev/d407-agentflow-rfc0041-closure.md` — closure record
- [x] `docs/decisions/design-decisions.md` — agentflow config admission entry
- [x] `docs/README.md` — RFC index
- [x] `docs/dev/engineering-efficiency.md` — per-task retro
- [x] `docs/dev/mermaid-d117-family-fallback-counts.md` — the agentflow row moves to 36/2
- [x] `tests/fixtures/mermaid_native/direct_theme_source_audit.v1.json` — unconsumed
      point-in-time census; annotate as superseded by RFC 0041 (it lists these 9 rows as
      `source_fallback` with a pre-phase `matrix_sha256`)

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: does the gate stay TU-local or share RFC 0040's helper? | design review | **Resolved (design review r1)**: stay TU-local — the mermaid detail module is imported by 37 implementation units, a shared declaration is a real BMI fan-out for 9 rows, and agentflow's gate is stricter than RFC 0040's (no pass-through), so sharing would couple two different contracts. |
| OQ-2: is an empty `config:` block a reject or a pass-through? | design review | **Resolved (design review r1)**: reject — identical to today's verdict; only a block with exactly one admitted theme leaf widens. |
| OQ-3: narrow_40 disposition per row | implementation | **Resolved (P1 measurement)**: 0 of 9 draw at 40 columns — the footer exceeds the 40-column viewport, so every admitted row keeps the whole source there (the corpus twin, footerless, still draws). Ledger records `source` for all nine; family narrow 19/19 unchanged. |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR with a different lens.
- Phases: **one** atomic phase, with its review record.
- Graduation: the phase's targeted census; closure runs full ctest once and the
  dual-preset byte gate.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | — | — | draft | Created provisional from the D-406 reachability measurement |
| 2026-11-22 | p1-code-r1 | independent read-only adversarial code-review agent (grammar/twin/footer/generators/hidden tests) | APPROVE | Zero BLOCKER/MAJOR. Verified against a run of the focused suites and both generator `--check`s: the config-block-scoped grammar is a strict widening for exactly one shape (no narrowing because no currently-accepted frontmatter has a column-0 `config:`), the closed table is identical in C++ and Python, the footer budget is byte-identical to the five sibling paths, the corpus-twin gate recomputes body equivalence and sha, and the narrow_40 assertion is live (0 of 9 draw at 40 columns). MINORs fixed before the commit: the generator docstring now states the measured 0/9 disposition, the RFC qualifies which rows the footer flips (5 container-kind; the triage twin was already too wide), and the family test header no longer claims 11 gated rows; `expected_prepend_lines` accepted as unused. |
| 2026-11-22 | prr-r1 | independent PRR agent (production-readiness lens; graduation measurability, hidden-test enumeration, build/Work, false-green audit, rollback, string couplings, precedent consistency) | APPROVE-TO-IMPLEMENT | Graduation numbers re-derived from the live matrix (`fd852b74…`: 1462/683/0, agentflow 27/11/0, narrow 19/19, formal 1276, gaps 48, integrity 0; all 9 rows original-track `source_fallback`, zero in the protected gap roster). Build fan-out confirmed relink-only (one body unit, no BMI), and every Work formula proven byte-identical to the five sibling paths. **MAJOR**: the new gate enumeration omitted the live per-row `narrow_40` render, so a transcribed narrow pin could survive while the matrix's agentflow narrow aggregate went silently wrong → **accepted**: §4 and the testing plan now pin the live assertion (`NARROW_DRAW` from a live render, never from the twin). MINORs folded in: the full-build/mtime and both-preset-rebuild requirements (EE rules 3 and 28), the fixed emit ordering (review approved → rebuild → both matrices → `--self-test` → `--matrix` → `--emit`), the two stale census documents added to the documentation-impact list, and the expected `ctest -L mermaid -N` denominator delta stated (+9: six gtests + three hermetic `--check` tests; reconciled at closure). NITs: corpus-twin schema re-cited to RFC 0037 with `twin.sha256` pinned; the "7 gtests" count replaced by six enumerated TEST names; rollback restated as "one revert + rebuild". |
| 2026-11-22 | r2 | same design-review agent, re-read against the revised text and an executable model of both gates | APPROVE-TO-ACCEPT | Both new decision points verified against a run of today's gate and the revised gate over all 38 fixtures: indented lines outside a config block are byte-identical in both directions (payload `theme:` and post-`description` indent-4 draw; post-`title` shapes reject on the column-0 key itself), and the one-leaf rule introduces no narrowing because no currently-accepted frontmatter has a column-0 `config:` at all. Four MINORs and three NITs folded in before the gate: a second `config:` line now rejects outright (the cumulative wording admitted `theme` + an empty second block — an over-widening), the testable section names the corpus twin, venn/ishikawa mechanism corrected to "extend the existing gate", the narrow_40 prediction softened to "predicted, measured at the phase" because a footer can flip a whole-surface 40-column predicate, and three wording/ambiguity fixes. Gate to `accepted` passed. |
| 2026-11-22 | r1 | independent design-review agent (own re-derivation; read the renderer, generators, ledgers, roster, gap roster and matrix) | REQUEST-CHANGES | Every partition and endpoint re-derived as correct (38 = 27 + 9 + 2; seven values; 36/2; 1471/674; 1285; gaps 48; integrity 0; roster 674 with matched 242 / drawn 160 / fallback 82). **BLOCKER**: the pre-existing `tests/ui/test_mermaid_agentflow_cohort.cpp` is not in the change set and its `gated_count == 11` pin plus `FrontmatterGatesStayClosed` negatives (three admitted rows and the synthetic `theme: dark` probe) would go red → **accepted**: named in §4 and the testing plan, with the pin deltas and the replaced battery spelled out. **MAJOR-1**: the "strict widening" claim was false — today all indented lines are skipped, so a description-payload `theme:` line and an indent-4 visual key are *accepted* today and would have been narrowed → **accepted**: the new interpretation is scoped to the config block only; indented lines elsewhere stay ignored (pinned as a probe). **MAJOR-2**: the rule was fail-open for a config carrying an admitted leaf plus other leaves (`look`/`handDrawnSeed` silently dropped) → **accepted**: a config block must carry exactly one admitted leaf; `theme+look` / `theme+seed` / `theme+fontFamily` pin rejection. **MAJOR-3**: "no drawn row is body-equivalent" was false — all 9 bodies are byte-identical to two drawn fixtures (5 → `10-every-container-kind`, 4 → `15-support-triage`) → **accepted**: corpus twins take precedence (`twin.kind = "corpus"`), a stronger oracle. **MINOR**: pie already has `pie_config_frontmatter` (mechanism cell corrected); ledger field deltas spelled out (`native_draw_count` stays 27, new `admitted_count: 9`, `gated_count: 2`); narrow_40 prediction stated (24/14) instead of "empty unless measured"; roster sufficiency numbers written in-text. **NIT**: "substring gate" reworded; `test_mermaid_official.cpp` named; `theme : dark`, second config block and colon-less nested lines pinned. |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | — |
| 2026-11-22 | — | Accepted after design review r2; implementable after PRR r1 | — |
| 2026-11-22 | P1 | One validating config-block gate + disclosure footer; 9 rows promoted; corpus-twin ledger (digest `5874b783…`); three-way family partition; 6 gtest + 2 hermetic `--check`; matrix branch; roster 683 → 674; narrow_40 measured 0/9 | `1d729fe3` |
| 2026-11-22 | — | Closure: full debug ctest, dual-preset byte gate, D-407 record, design-decisions and README entries, d117/audit census updates, efficiency retro | see D-407 |
