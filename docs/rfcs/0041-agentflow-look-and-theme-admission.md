---
rfc: 41
title: agentflow look and theme admission
status: provisional
owners: "@Zzzode"
reviewers: []
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
| pie | 4 | one — a new envelope (the family has no gate) | low but smallest useful increment |
| venn / ishikawa | 2 | one — a new envelope | low, too small |
| git 38 / requirement 26 / state-v2 6 | 0 inert | semantic or geometric capability, not admission | out of scope by construction |

### Evidence

- 38 agentflow originals = 27 drawn + 9 theme-only + 2 `look`+`handDrawnSeed`.
- The 9 rows are two bodies × themes: the "11 every-container-kind" body under
  {dark, base, default, forest, neutral} and the "15 support-triage" body under
  {dark, default, redux-color, redux-dark-color}. Seven distinct theme values.
- All 11 fallbacks sit in the roster's `edge_animation_metadata` category (91 rows;
  the detector is a co-occurrence predicate, drawn 151 / matched 242, not a proven
  sufficient blocker), so no sufficiency pin flips; `proven_compact_cohort_candidates`
  stays {`click_interactive_callback`, `linkstyle_directive`}.
- None of the 9 rows appears in the protected `e6c-mandatory-gap-roster.json`
  (which lists only class/flowchart rows), so `mandatory_gaps` stays 48.
- The strip probe proves the bodies render natively once the frontmatter is removed; the
  per-row proof is the twin gate this RFC adds.

### Why admission is testable

The renderer never reads `theme`; the text surface has no colour contract. The proof is
the same two-part oracle RFC 0038/0039/0040 used: per-row twin inertness (the admitted
frame equals the synthesized config-free twin's frame plus exactly the ledger's literal
footer delta) and a drawn-row frame-identity oracle over the 27 rows the family already
draws (identical untrimmed cell surfaces with and without the frontmatter, no footer
markers).

## Goals

- **G1.** Admit exactly the seven corpus `theme` values as a closed table at the
  admitted position; everything else keeps today's whole-source fallback.
- **G2.** Disclose each admitted leaf through a bounded footer
  (`Agentflow native projection v1` + one line) that names the value the renderer
  actually accepted.
- **G3.** Promote exactly 9 rows; keep the prior contract for every other key (the gate
  remains strict: `description` plus at most one admitted `config.theme`).
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

### 2. Grammar (block-context line scan, TU-local)

`af_frontmatter_gated` keeps its existing envelope scan (exact `---` fences, closing-line
exclusion, unclosed fence rejects) and its `parse_envelope`-mirroring rules (a column-0
non-comment line needs a colon; `description` is the only admitted top-level key), with
one added rule:

- A column-0 non-blank, non-comment line opens a block; its key is the text before the
  first colon. `config:` opens the config block; any other column-0 key closes it.
- A line that contains a colon whose key (text before the first colon, leading spaces
  stripped) is exactly `theme` is a **visual key**; a colon-less line is never one.
- A visual key at **indent 2 inside the config block** is validated against the table
  and, if admitted, disclosed. **At most one** admitted leaf is allowed.
- A visual key **anywhere else** — top level, inside a `description:` payload, or at
  indent 4 or deeper — rejects the whole source (today's behavior).
- A `config:` block that carries no admitted leaf (empty, or only non-visual leaves)
  rejects the whole source. This preserves the current contract for every non-admitted
  frontmatter, so the change is a strict widening for exactly one shape.
- Every other key rejects the whole source. Unlike RFC 0040's three families, agentflow
  has **no pass-through**: it admits only `description` today, and this RFC does not
  widen that.

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
must satisfy `art_lines + 1 + n <= kHeightLimit (1024)`; failing either bound keeps the
whole source. This is byte-for-byte the state/flow/git/journey-family budget. The height
guard reads an off-by-one-safe `>` comparison, so `art + 1 + n == 1024` is admitted.

### 4. Reviewed cohort, twin gate and wiring

- `tools/mermaid/agentflow-cohort.json` keeps its 27 `rows`/`counts` and gains a named
  `admitted` partition (9 paths, digest) and shrinks `gated` to the 2 seed rows; the
  generator asserts the three-way sum equals 38.
- New sibling ledger `tools/mermaid/agentflow-0041-cohort.json` (schema
  `loom_mermaid_agentflow_0041_cohort/v1`) with the 9 rows in one section and one phase
  review record. Each row pins the literal `expected_footer_lines` (blank separator,
  header, theme line) plus `expected_states`, exactly like the RFC 0038/0039/0040
  ledgers. `expected_states.narrow_40` is not source-derivable: like those ledgers it
  comes from a reviewed, measured `NARROW_DRAW` set (empty unless the phase measures a
  row drawing at 40 columns), and the ledger records the per-row measurement.
- Generator `tools/mermaid/generate_agentflow_0041_cohort.py`, renderer-free,
  `--check`-reproducible, with pinned literal path sets and digests, source-derivable
  counts (38 originals / 27 drawn / 9 admitted / 2 still gated), and per-row shape
  validation (description + config with exactly one theme leaf). The roster and the
  matrix artifact are never read.
- **Twin rule**: the frame equals the synthesized config-free twin's frame plus the
  recorded footer delta, line by line, both sides through the same sentinel-bearing
  wrapper. No drawn agentflow row is body-equivalent to an admitted row, so all 9 use the
  config-free twin.
- Gate `tests/ui/test_mermaid_agentflow_0041_cohort.cpp`: ledger pins; per-row twin
  inertness; a closed-table battery covering every admitted value, sibling values
  (`redux`, `redux-dark`), an unknown value, a case variant, an empty value, a duplicate
  leaf, a quoted value, a trailing comment, an unmatched quote, indent 0 / 2 / 4, a
  description payload line that looks like `theme:`, an admitted leaf plus an in-body
  `%%{init … theme …}%%` line, a `config:` carrying a non-visual leaf (rejects), and an
  empty `config:` (rejects) — all with hardcoded verdicts and literal footer strings;
  plus the drawn-row frame-identity oracle over the 27 already-drawn rows (full untrimmed
  character surfaces, footer-marker absence, line-equality native predicate).
- Update `generate_agentflow_cohort.py` to classify structurally (a frontmatter line
  whose key is exactly `theme` at indent 2 inside `config:`, in-table value) instead of
  the value-agnostic substring gate; regenerated ledger, never hand-edited.
- Matrix branch: one Journey-style map keyed by path covering the 9 rows, placed before
  the generic fallback, reason `RFC-0041 agentflow look/theme reviewed cohort`.
- Roster migrated with `fallback_attribution.py --emit` in the phase commit
  (683 → 674; `edge_animation_metadata` 91 → 82, its sufficiency row recomputed, no
  compact-candidate change).

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `theme` admission | 9 rows | proposed | agentflow 27/11 → **36/2**; global 1462/683 → **1471/674**; formal `drawn_at_240` **1276 → 1285**; gaps 48, integrity 0; `gated` 11 → 2; dual-preset sha recorded |

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
  registrations, and the roster; one revert restores the previous census exactly.
- **Observability** — no new diagnostics; session traces unaffected.
- **Documentation** — RFC, closure doc, design-decisions entry, docs index, retro.
- **Deletion** — the old substring/`key != description` gate is replaced, not wrapped.
- **Deprecation** — the family ledger's `gated` list shrinks; no persisted format
  changes (ledgers are tool-generated).

## Rollout and rollback

One phase lands P1. Rollback is one revert. No persisted user data, no wire protocol, no
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

- New gate `tests/ui/test_mermaid_agentflow_0041_cohort.cpp` (7 gtests) registered in
  `tests/ui/CMakeLists.txt`, plus a hermetic `--check` ctest for the new generator and one
  for `generate_agentflow_cohort.py`.
- Targeted `ctest -L mermaid` in the phase; release-preset matrix in the phase; dual-preset
  byte gate at closure; full debug ctest once at closure.
- `fallback_attribution.py --emit` followed by `--self-test` and `--matrix`.
- **False-green guardrails.** The matrix branch is a ledger echo; the render proof is the
  cohort gate. A census command must run from the preset build directory and the artifact
  sha must change, not just the exit code. A red full gate is handled per D-401 §7 —
  fixed, or evidenced by ≥3 clean reruns plus unrelatedness; never laundered by a partial
  rerun.

## Documentation impact

- [ ] `docs/dev/d406-next-family-agentflow.md` — records the selection (already written)
- [ ] `docs/dev/d407-agentflow-rfc0041-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — agentflow config admission entry
- [ ] `docs/README.md` — RFC index
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: does the gate stay TU-local or share RFC 0040's helper? | design review | proposed: TU-local (see Alternatives 2); open to rebuttal |
| OQ-2: is an empty `config:` block a reject or a pass-through? | design review | proposed: reject (preserves today's contract) |
| OQ-3: narrow_40 disposition per row | implementation | ledger `expected_states`, measured per row |

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

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | — |
