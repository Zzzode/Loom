---
rfc: 40
title: journey-family look and theme admission (timeline, gantt, journey)
status: implementable
owners: "@Zzzode"
reviewers: ["agent:aa0ce043b4adde4a9 rfc0040-design-review (r1/r2 REQUEST-CHANGES -> r3 APPROVE-TO-ACCEPT)", "agent:ab976b78b004c884a rfc0040-prr (r1 APPROVE-TO-IMPLEMENT; fold-in confirmation r2 APPROVE-TO-IMPLEMENT)"]
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0040 — journey-family look and theme admission (timeline, gantt, journey)

## Summary

Three renderers — `timeline`, `gantt` and `journey` — share one frontmatter gate (the
same rule shape; gantt runs it on sanitized bytes) whose comment states that they "must
not drift apart". D-404 measured their remaining
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

| family | originals | drawn / fallback today | admitted rows (frontmatter-only) | after | remaining fallback cause |
|---|---|---|---|---|---|
| timeline | 30 | 7 / 23 | **16** | 23 / 7 | 7 use an in-body `%%{init}%%` theme directive |
| gantt | 52 | 39 / 13 | **11** | 50 / 2 | 2 pre-existing malformed dates |
| journey | 18 | 10 / 8 | **4** | 14 / 4 | 4 use an in-body `%%{init}%%` theme directive |

All 31 rows have the identical shape: top-level `description` (folded `>-`) plus `config`.

### Evidence

Measured on the certified matrix (`build/release/tests/ui/mermaid-official-matrix.json`,
generation `9d925815…`, byte-identical across presets at RFC 0039 closure) and the 714-row
`tools/mermaid/fallback_roster.json`.

| Metric | Current | Target | How measured |
|---|---|---|---|
| timeline `wide_240` | 7 drawn / **23 fallback** | 23 / 7 | `MermaidCapabilityReport.OfficialMatrix` census |
| gantt `wide_240` | 39 / **13** | 50 / 2 | same |
| journey `wide_240` | 10 / **8** | 14 / 4 | same |
| global `wide_240` | 1431 / 714 | 1462 / 683 | same |
| formal `drawn_at_240` | 1245 | 1276 | certification census |
| mandatory_gaps | 48 | 48 unchanged | `e6c-mandatory-gap-roster.json` has no rows in these families |
| integrity_errors | 0 | 0 | matrix header |

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
RFC forbids (G4). The new rule is therefore: `theme`/`look` **at the admitted position** (indent 2 inside
`config:`) become *validated and disclosed*; every other key keeps today's pass-through.

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

**Grammar: a block-context line scan, never `detail::parse_envelope`.** The existing
gates are line scans over the leading frontmatter block. The new rule keeps a line scan
but adds the minimum block context the value tables need; it must **not** adopt
`parse_envelope`, because that would reject the 11 drawn rows carrying nested
`config.gantt.*` / `config.journey.*` maps and the tab-bearing folded description of a
drawn gantt fixture, regressing the 56-row set. The rule, stated exactly:

- Walk the frontmatter lines. A column-0 **non-blank, non-comment** line opens a block;
  its key is the text before the first colon. `config:` opens the config block; every other column-0 key closes it.
- A line that **contains a colon** and whose key (text before the first colon, leading
  spaces stripped) is exactly `theme` or `look` is a **visual key**; a colon-less line is
  never one.
- A visual key at **indent 2 inside the config block** is validated against the family's
  table and, if admitted, disclosed.
- A visual key **anywhere else** — top level, inside a `description:` payload, or at
  indent 4 or deeper inside the config block (a namespaced `gantt.theme`-style key) —
  keeps today's behavior: it rejects the whole source. This is the rule that keeps
  `theme: dark` at top level or in a description payload from silently becoming an
  admitted key.
- Every other line is ignored (the measured pass-through). Only non-`theme`/`look`
  `gantt.*` / `journey.*` / `timeline.*` keys are pass-through; a `theme`/`look` key is
  never pass-through.
- The in-body `%%{…}%%` scan (a line starting with `%%{` that mentions `theme`) is
  unchanged.

The same state machine is mirrored in the three existing generators' structural
classifier, so the generators and the renderers classify identically (for gantt the
generator applies the same `sanitize_breaks` first; the corpus has no `<br>` inside a
visual scalar, so this is parity insurance, not a row change).

The scalar reader trims both sides, then strips a trailing YAML comment outside quotes,
then dequotes a matched pair; an unmatched quote is a mismatch, never a value. Each scalar
read charges `work.spend(value.size() + 1)` like `state_scalar_text` (the entry prepayment
already bounds the linear scan, so this is accounting parity, not a DoS control). A visual
key may appear at most once.

**Gantt reads sanitized bytes.** `gantt_layout` runs `sanitize_breaks` over the whole
source before the gate and parses the sanitized body. The gate and the notice derivation
must read that **same** sanitized text. Sanitizing can rewrite a scalar: `theme:
dark<br/>` becomes `theme: dark `, and the pinned reader (trim both sides) then yields
`dark` — **admitted**, with the footer disclosing `config.theme=dark`. That is the pinned
probe expectation; the disclosure always names the value the renderer actually used.

### 2. Disclosure footer

Per family, emitted below the art when at least one leaf was accepted, with one blank
separator line first:

```
Timeline native projection v1        (or Gantt … / Journey …)
config.look=<value> -> source node style; not applied
config.theme=<value> -> colour palette; not applied
```

Fixed order `look` then `theme`; each line capped at 96 bytes and charged per line at
`size * 4 + sizeof(std::string) + 32` while it is built; emission charges
`(1 + n) * (4 * sizeof(std::string) + 256) + bytes * 2 + 256`; the total must satisfy
`art_lines + 1 + n <= 1024`. This is the state/flow/git budget; gantt reads the sanitized
bytes (see §1), and the per-line charge is what the git and state paths use.

### 3. Reviewed cohort, twin gate and wiring

- Sibling ledger `tools/mermaid/journeyfam-0040-cohort.json` (schema
  `loom_mermaid_journeyfam_0040_cohort/v1`) with 31 rows in three family sections and one
  phase review record. Each row pins the literal `expected_footer_lines` (blank
  separator, header, leaves in `look` then `theme` order) plus `expected_states`, exactly
  like the RFC 0038/0039 ledgers — the twin test composes the twin's frame with those
  literals, never with a delta computed by the C++ under test. Those literals are
  generator-derived (the same property RFC 0039 has); the **independent** oracle against a
  wrong table or wrong footer is the hardcoded battery in §3.
- **The existing three ledgers keep their row sets.** Their `rows`/`counts` stay at
  7 / 39 / 10 (the drawn cohorts) and their `themed_excluded` lists shrink to the
  remaining rows (timeline 7, gantt 0, journey 4); each additionally gains a named
  `admitted` partition (paths + count + digest) for its promoted rows. The invariant is a
  four-way partition of each family's originals — `rows` (drawn) + `admitted` +
  `themed_excluded` + `fail_closed` = originals, i.e. timeline 7 + 16 + 7 + 0 = 30,
  gantt 39 + 11 + 0 + 2 = 52, journey 10 + 4 + 4 + 0 = 18 — and it is asserted in each
  generator. The new gate asserts the sibling ledger's rows equal each existing ledger's
  `admitted` partition and are disjoint from its `rows`.
- Generator `tools/mermaid/generate_journeyfam_0040_cohort.py`, `--check` reproducible:
  three pinned literal path sets each with a digest, source-derivable counts (timeline 30
  originals / 16 admitted, gantt 52 / 11, journey 18 / 4) and per-row shape validation
  (description + config, leaves inside the family's tables). The roster and the matrix
  artifact are never read. **`narrow_40` is not source-derivable**: like the three
  existing generators, the ledger's `expected_states` come from a reviewed, measured
  `NARROW_DRAW` set (empty unless the phase measures a row drawing at 40 columns), and
  the phase records the per-row measurement (OQ-3).
- **Twin rule** (RFC 0037/0038/0039 restated): the frame equals the synthesized
  config-free twin's frame plus the recorded footer delta, line by line, both sides through
  the same sentinel-bearing wrapper. No drawn row of the three families is body-equivalent
  to a cohort row, so all 31 use the config-free twin.
- Gate `tests/ui/test_mermaid_journeyfam_0040_cohort.cpp` (surface comparisons use the
  per-cell **character** vectors of the git 0039 helper; background is not compared): ledger pins; per-row twin
  inertness; three closed-table boundary tests (one per family, including the `handDrawn`
  rejection for timeline and the no-`look` rejection for gantt/journey); a **cross-family
  agreement test** over a hardcoded, family-independent probe battery; and the
  **drawn-row frame-identity oracle** over the 7 + 39 + 10 = 56 already-drawn rows
  (frontmatter-stripped twin, full untrimmed character surfaces, no footer markers —
  matched only by the **full** header strings `Timeline`/`Gantt`/`Journey native projection
  v1` and the two `config.` prefixes, never a bare `find("Gantt")`: several drawn bodies
  literally contain a `title … Gantt …` line — and a line-equality native predicate).
  Plus a hermetic ctest running the generator `--check`.

  The cross-family battery must be **discriminating**: its expected verdicts and footer
  strings are literals in the test (never loaded from the ledger, never derived from the
  renderer's tables), and it covers, per family, at minimum — every admitted value of that
  family; **every** value admitted by a sibling but not this family — timeline: any `look`
  in gantt/journey and `look: handDrawn`; gantt: `theme: neo`, `theme: neo-dark`; journey:
  `theme: dark`, `default`, `forest`, `neutral`, `neo`, `neo-dark`; an
  unknown value, a case variant and an empty value; a duplicate leaf; a quoted value; a
  trailing comment; an unmatched quote; indent 0 / 2 / 4 spellings; `themeVariables:`; a
  description payload line starting with `theme:`; an admitted leaf plus an in-body
  `%%{…theme…}%%`; and the exact footer of each family (header text, the two line
  formats, `look`-before-`theme` order, one blank separator). The 96-byte cap is not
  probeable through the public API with in-table values (the longest admitted line is well
  under it, and an over-long value rejects for being out of table); it is pinned by code
  parity with the state/flow/git paths and by review, not by a test.
- The three existing generators (`generate_timeline_cohort.py`,
  `generate_gantt_cohort.py`, `generate_journey_cohort.py`) replace their value-agnostic
  `is_themed` substring/regex matching with a **structural** classifier (a frontmatter
  line whose key is exactly `theme`/`look`, the same grammar §1 pins) and split each
  family four ways: `rows` unchanged, a new `admitted` partition (timeline 16, gantt 11,
  journey 4), `themed_excluded` shrinking to timeline 7, gantt 0, journey 4, and
  `fail_closed` unchanged (0 / 2 / 0). Only the
  frontmatter half of the classifier is replaced; the in-body `%%{…theme…}%%` half is
  retained unchanged, which is what keeps those 7/0/4 counts. Their
  ledgers are regenerated, never hand-edited; their gates' pinned counts and header
  comments follow. Because gantt's list empties, its `ThemedGanttsStayWholeSource`
  becomes vacuous and is replaced by an explicit out-of-table negative (`theme: neo` — no
  gantt corpus row — stays whole source). The closed tables live in one shared **Python**
  constants module loaded by all four generators via
  `importlib.util.spec_from_file_location` with an explicit path (the pattern the existing
  verifiers use; `python3 -I` in the ctest implies `-P`, so a plain sibling import would
  fail), so generator-side tables cannot drift from each other; the TUs stay TU-local and are held to the same literals by
  the cross-family test below.
- Matrix branch: one Journey-style map keyed by path covering all three roots, placed
  before the generic fallback, reason `RFC-0040 journey-family look/theme reviewed cohort`.
- Roster migrated with `fallback_attribution.py --emit` in the phase commit.

### 4. Named cross-module couplings

| coupling | files that must change together |
|---|---|
| three gates and three renderers | the three `*_layout` implementation units and their tests |
| **roster self-test** | `tools/mermaid/fallback_attribution.py`: `look_classic_other` holds exactly the 6 timeline `look: classic` rows and empties when they are promoted, while `emit_roster` drops empty buckets but keeps every detector in `causality` — `--self-test` then fails its key-set invariant. `--emit` must emit every detector category with a possibly empty path list (the key-set invariant is what the self-test pins); the phase's `--self-test` is the gate for this |
| existing cohort ledgers and gates | `generate_{timeline,gantt,journey}_cohort.py`, `{timeline,gantt,journey}-cohort.json`, `test_mermaid_{timeline,gantt,journey}_cohort.cpp` (themed_excluded lists, pinned counts, gantt's replaced negative) |
| `MermaidJourneyCohort.FrontmatterLookIsAVisualGate` (`journey:182`) and the look probes inside `MermaidTimelineCohort.ThemeWordInEventIsNotAVisualGate` (`timeline:148`) / `MermaidGanttCohort.ThemeWordInLabelIsNotAVisualGate` (`gantt:159`) | use `look: handDrawn`, which **stays** out-of-table for all three — the negatives survive because the tables are corpus-bounded (a future widening must revisit them). The journey test is the only one named `FrontmatterLookIsAVisualGate` |
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
- **Rollback** — one atomic commit containing everything the census depends on: the
  three renderers, the new ledger and generator, the shared Python tables module, the
  three existing generators and their regenerated ledgers, the gates,
  `tests/ui/test_mermaid_official.cpp` (matrix branch) and `tests/ui/CMakeLists.txt`
  (registrations), and `fallback_attribution.py` + `fallback_roster.json`; one revert
  restores the previous census exactly.
- **Observability** — the family footer is the user-visible trace.
- **Documentation** — design-decisions entry, closure doc, README index, retro.
- **Deletion** — the three "no color contract" comments are rewritten to state the new
  rule in the same phase; the three cohort tests' header comments ("the twenty-three /
  eleven / eight themed fixtures stay on whole-source fallback") and the three existing
  generators' docstrings are corrected to the new split (7 / 0 / 4 remain). The three
  existing generators also gain hermetic `--check` ctests (the git 0039 pattern), so their
  four-way partitions are enforced without a manual step.
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
  `tests/ui/CMakeLists.txt`) plus a hermetic `--check` ctest for the new generator and one
  for each of the three existing generators (four hermetic script tests); the exact count
  is re-derived at phase start (`ctest --preset debug -N` declared **4633** after RFC 0039,
  of which 4631 run because two are compile-time disabled) and recorded.
- Edited `tests/ui/test_mermaid_official.cpp` (matrix branch + reason string) and the three
  existing cohort tests (pinned counts; gantt's replaced negative).
- Targeted `ctest -L mermaid` in the phase; the three existing generators' `--check` plus
  the new one; `fallback_attribution.py --emit` followed by `--self-test` and `--matrix`;
  release-preset matrix in the phase; dual-preset gate at closure.
- **False-green guardrails.** The matrix branch is a ledger echo; the render proof is the
  cohort gate. The roster procedure (full build → matrix → `--emit`) only keeps the derived
  artifact in sync — cohort rows are ledger echoes, so the census cannot establish render
  correctness; the cohort gate is the render proof, and `--emit` must run after it is
  green. `--self-test` guards hand edits only. The full ctest is redirected to a log with
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
| OQ-1: whether the three gates should stay per-TU or become one shared helper | design review | **Resolved (design review r1)**: stay per-TU — `loom.ui.visual.mermaid:detail` is imported by 37 implementation units, so a shared declaration is a real BMI fan-out for a 31-row slice; the established pattern is a TU-local envelope plus a cross-family test. |
| OQ-2: whether the cross-family agreement test is sufficient to enforce the invariant | design review | **Resolved (design review r1)**: yes, but only with a hardcoded literal probe matrix and literal footer strings (pinned in §3); a test that reads the ledger or the renderer's tables is not discriminating. |
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
| 2026-11-22 | r1 | independent design-review agent (own re-derivation; read the three renderers, gates, generators and ledgers) | REQUEST-CHANGES | Every count re-derived as correct (30/52/18 originals, 7/23, 39/13, 10/8, 16/11/4 admitted with the exact value sets, 11 drawn rows carrying other config keys, endpoints 23/7, 50/2, 14/4, 1462/683, 1276, gaps 48, integrity 0). **BLOCKER** `fallback_attribution.py` `--self-test` breaks after `--emit` because `look_classic_other` (exactly the 6 promoted timeline rows) empties while `causality` keeps every detector -> **accepted**: `--emit` must emit every detector category (possibly empty) and the RFC names it as a coupling. **MAJOR-2** the existing ledgers' three-way partition was ambiguous (rows could grow to 23/50/14) -> **accepted**: rows stay 7/39/10, a named `admitted` partition is added, the new gate asserts disjointness/equality. **MAJOR-3** the cross-family test was not discriminating -> **accepted**: hardcoded literal battery and literal footers pinned. **MAJOR-4** the visual-key grammar/pass-through boundary was undefined and `parse_envelope` would regress nested-config and tab-description rows -> **accepted**: keep the indentation-insensitive line scan; payload/indent behaviour made explicit non-goals. **MAJOR-5** gantt must derive gate+notices from the same `sanitize_breaks` bytes, with a `theme: dark<br/>` probe. **MAJOR-6** the ledger must pin literal `expected_footer_lines`. **MAJOR-7** `narrow_40` is not source-derivable -> reviewed measured `NARROW_DRAW` set. **MINOR-8** the new gate's native predicate must be line equality. **MINOR-9** `--emit` cannot establish render correctness (ledger echo); the cohort gate is the proof. **MINOR-11** a shared Python table module and a structural classifier for the three existing generators. **MINOR-12** terminology and stale cohort-test headers/docstrings in scope. **NIT-13** per-notice charge restated. |
| 2026-11-22 | r2 | same design-review agent, two-round re-read against the live tree | REQUEST-CHANGES | BLOCKER verified resolved (the empty-bucket fix satisfies every self-test invariant; `drift_check` unaffected) and the spot-check confirmed 0 of the 31 admitted and 0 of the 56 drawn rows have a payload line whose key is `theme`/`look`. Four residuals: **MAJOR-4** the grammar paragraph still contradicted itself (an in-table top-level/payload key would have been admitted, not gated) and the indent-4 case was undecided -> **accepted**: the rule is now a block-context line scan — admission only at indent 2 inside `config:`, any other visual key keeps today's whole-source rejection, and the same state machine is mirrored in the generators' classifier. **MAJOR-5** the scalar whitespace policy was unstated and the sanitize probe's expectation was likely wrong under a trim-both-sides reader -> **accepted**: the reader is pinned as trim, strip comment, dequote; `theme: dark<br/>` is therefore **admitted** with `config.theme=dark` in the footer, and the probe now says so. **MAJOR-2 residual** the partition equation was wrong for gantt -> **accepted**: four-way `rows + admitted + themed_excluded + fail_closed` = 30/52/18. **MINOR-11 residual** `python3 -I` implies `-P`, so a plain sibling import fails -> **accepted**: the shared Python table module is loaded via `importlib.util.spec_from_file_location`, the existing verifier pattern. Also folded in: the unprobeable 96-byte cap is documented as review-pinned rather than tested, the rollback inventory names every census-carrying file, and D-404 no longer implies a per-row path table in the RFC. |
| 2026-11-22 | r3 | same design-review agent, final consistency pass | APPROVE-TO-ACCEPT | Every r2 residual verified substantively resolved against the live tree (block-context grammar reproduces the corpus and preserves the three existing negatives; the trim/comment/dequote reader reconciles the 31 rows with gantt's sanitize path; the four-way partition reconciles 30/52/18; the `importlib` loading works under `python3 -I`); no sentence implies payload admission and no "any indentation" wording survives. Five editorial NITs folded in before the gate: "column-0 non-blank, non-comment" block opener; only the frontmatter half of the generator classifier is replaced; gantt generator sanitize parity noted; the Summary says "at the admitted position" and no longer calls the gate byte-identical; D-404 drops the "auditable" adjective. Gate to `accepted` passed. |
| 2026-11-22 | prr-r1 | independent PRR agent (production-readiness lens; hidden-test enumeration, build-graph measurement, false-green audit, Work/DoS bounds, rollback audit) | APPROVE-TO-IMPLEMENT | Independently confirmed: the 30/52/18 four-way partitions, the 31 admitted rows, the BLOCKER fix's self-consistency (`counts`/`paths_by_category` key sets, `recomputed == counts`, `sum == total`, `drift_check` unaffected, no other consumer of the roster schema), the build fan-out (3 compile edges + 1 archive relink + 198 executable relinks + 3 phony; 8 UI static libraries link only order-only; zero BMI change), the 56-row oracle's validity and the matrix `observed_gap_rows == 48` dependency (the new branches must precede the generic fallback or gaps become 79). MINOR/NIT items folded in: the look-probe test names corrected (timeline/gantt carry them inside `ThemeWordIn...`, only journey is `FrontmatterLookIsAVisualGate`); the battery must enumerate the full sibling-only value sets; the ledger footers are generator-derived and the independent oracle is the battery; surface comparisons are character cells and marker-absence uses the full header strings (drawn bodies literally contain "Gantt"/"Journey" in their titles); scalar Work charge stated; rollback inventory names the matrix-branch test and the CMake registrations; hermetic `--check` ctests added for the three existing generators; baseline count clarified; colon-less lines are never visual keys; four-way wording. |
| 2026-11-22 | prr-r2 | same PRR agent, fold-in consistency pass | APPROVE-TO-IMPLEMENT | All twelve folded items verified against the live tree with no new contradiction and no weakened contract: corrected look-probe test names, the full sibling-only battery sets, generator-derived ledger footers vs the battery as independent oracle, character-cell surfaces plus full-header-string marker absence (drawn titles contain "Gantt"/"Journey"), scalar Work charge, the complete rollback inventory, the three hermetic `--check` ctests, the 4633-declared/4631-run baseline wording, colon-less lines never being visual keys, and the four-way wording. Gate to `implementable` passed. |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | — |
