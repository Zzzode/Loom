---
rfc: 44
title: requirement elk layout admission
status: implemented
owners: "@Zzzode"
reviewers: ["agent:design-review", "agent:prr-review", "agent:code-review"]
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0044 — requirement `layout: elk` admission

## Summary

D-413 selects the next slice from D-410 bucket 5: the requirement family's
`config.layout: elk` rows. The requirement envelope gate already admits five inert
`config` leaves (`logLevel`, `fontFamily`, `htmlLabels`, `look`, `theme`) and discloses each
through a bounded footer; this RFC adds one more — a closed one-value `layout` table —
promoting **23 rows** whose bodies are byte-identical to already-drawn non-ELK rows. The
ELK layout engine is never applied; the footer says so.

One phase, one TU (`mermaid_requirement_impl.cpp`), no shared module, no BMI change.
Target: requirement **144 / 28 → 167 / 5**; global **1482 / 663 → 1505 / 640**; formal
`drawn_at_240` **1296 → 1319**.

## Motivation

### Selection (D-413, measured 2026-11-22)

Bucket 5's 374 rows were ranked by text-inertness proof, mechanism cost and rows per
mechanism (D-413 §1). The requirement ELK slice wins: 24 rows sit in one roster category
(`elk_layout_engine`, 38 total), 23 of them have a **corpus twin** — a drawn, non-ELK row
whose body is byte-identical after frontmatter removal — and the TU already has both the
admission channel and the disclosure footer.

Rejected alternatives: `treemap.valueFormat` (changes displayed numbers), architecture
`seed`/`randomize` (seed shape), xychart `labelRotation` (rotation debt), tree-view icons
(foreign glyphs), and smaller one-off slices (mindmap `layout` 3, state `useMaxWidth` 4,
c4 `wrap` 1). git `gitGraph.parallelCommits` (12) is the runner-up.

### Evidence

- requirement: 172 originals, **144 drawn / 28 fallback**. `config.layout: elk` accounts for
  24; all 24 classify as `elk_layout_engine`.
- 15 of the 24 carry only `layout`; 8 add `htmlLabels: false` (already admitted by D-066
  F0), so the two leaves compose in one envelope. One row carries the nested
  `config.flowchart.htmlLabels` map and has **no** corpus twin; the envelope rejects
  namespaced entries, so that row stays exclusive — **23 rows promoted**.
- The remaining 5 fallbacks stay: 3 nested `config.flowchart.htmlLabels` maps — **one ELK
  row** (which also carries `htmlLabels` and lacks a corpus twin: the 24th ELK row) and
  **two non-ELK rows** (one plain, one also `look`; the gate rejects namespaced entries) —
  plus 1 source whose leading line is blank before the opening fence (the gate requires
  `---\n` at offset 0) and 1 row whose closing fence is indented two spaces
  (`parse_envelope` closes only at column 0, so the envelope is malformed).
- The native requirement projection is a text tree of requirements/elements/connections;
  it draws no geometry, so the layout engine selection cannot change the frame. The 23
  body-identical twins prove it frame by frame.

### Why admission is testable

The renderer never reads `layout`; the proof is the established two-part oracle: per-row
twin inertness (corpus twin frame plus exactly the ledger's footer delta, wide only) and
the existing RFC 0037 gate's 120 drawn-row frames, which stay green only if the new leaf
does not disturb an envelope that carries no `layout`.

## Goals

- **G1.** Admit exactly `config.layout: elk` at envelope indent 2, once, in a closed
  one-value table.
- **G2.** Disclose it as `config.layout=elk -> external layout engine; not applied`,
  appended after every existing notice line — no already-drawn frame can carry a `layout`
  leaf, so the position only fixes the new rows' leaf order (`htmlLabels` then `layout`).
- **G3.** Promote exactly 23 rows; every other verdict is unchanged (the five other leaves
  keep their tables; any other `layout` value rejects; the nested `config.flowchart.*`
  map keeps rejecting).
- **G4.** No regression: the RFC 0037 120-row cohort keeps its frames cell for cell and
  the dual-preset matrix stays byte-identical.

## Non-Goals

- The three nested `config.flowchart.htmlLabels` maps (one ELK without a twin, two
  non-ELK), the leading-blank source, and the indented-fence malformed envelope.
- Other layout engines, `layoutEngine`, ELK geometry.
- flowchart's 222 exact-bit rows and the 67 protected/frozen rows.
- narrow_40 drawing; the ER107 B3 gates.

## Proposal

### 1. Mechanism (one TU, one new leaf)

In `requirement_envelope` (the event-driven gate used by all requirement admission):

- Extend the field bitset with `Layout = 32u`.
- Inside the config block, at indent 2: `layout` is admitted exactly once and only when its
  scalar (the existing `requirement_inert_scalar`: `er_strip_comment`, then trim of leading
  and trailing **spaces** only, then dequoting of an outer same-character pair) is `elk`.
  Tabs are rejected upstream by the envelope parser, so they never reach this scalar. Any
  other value rejects the whole source.
- The notice list gains, **last**:
  `config.layout=elk -> external layout engine; not applied` (56 characters, under the
  96-byte cap). Appending last matters only for the new 23 rows — an `htmlLabels` +
  `layout` row emits html then layout — because no currently-drawn frame can carry a
  `layout` leaf; it is still pinned by the composed-order battery.
- `config_seen && fields == 0` still rejects; a second `layout` leaf rejects; a `layout`
  at any other indent rejects; namespaced entries still reject.

### 2. Reviewed cohort, twin gate and wiring

- Sibling ledger `tools/mermaid/requirement-0044-cohort.json` (schema
  `loom_mermaid_requirement_0044_cohort/v1`) with the 23 rows, each with its corpus twin
  path + sha (body equivalence verified by the generator), the twin's own footer block, the
  literal source footer block, and the measured per-width states.
- **Twin selection rule (measured).** A row's body can be byte-identical to several drawn
  twins carrying different footers, so the rule is pinned: prefer a **footer-free** drawn
  twin (a drawn row whose frontmatter has no `config:` block, so it emits no notices) —
  20 of 23 rows; otherwise use a drawn twin whose footer block is a **byte-prefix** of the
  source footer — the 3 rows whose only twins carry `htmlLabels: false`. The gate composes
  `source frame = twin frame with the twin's recorded footer block stripped, then the
  source footer block appended`, and asserts the prefix relation for every prefix twin.
  Self-twins and non-drawn twins are rejected explicitly; the ledger records the kind and
  both blocks, so the composition is never ambiguous.
- `generate_requirement_0044_cohort.py`, renderer-free, `--check`-reproducible; its scalar
  mirror is quote-aware exactly like `er_strip_comment` (`#` starts a comment only at index
  0 or after a space/tab and outside quotes, returning the trimmed prefix), reused from an
  existing generator helper rather than a naive split. The RFC
  0037 generator is source-shape based: its `EXCLUDED_EXPECTED["layout: elk"] = 24` stays
  24 (the fixture shape has not changed) and only its human-readable `exclusions` sentence
  is updated to say "23 admitted by RFC 0044, 1 remains"; its 120 rows and counts do not
  change. The generator's module docstring no longer calls `config.layout` capability debt.
- Partition arithmetic is literal against the filesystem: 172 = 144 drawn + 23 admitted +
  5 remaining, with the drawn 144 split 120 (0037) + 8 (F0) + 4 (base cohort) + 12
  pre-existing.
- **Existing tests that pin the old verdict and change in the same commit** (the RFC
  0037 gate cannot stay green otherwise):
  `tests/ui/test_mermaid_requirement_cohort.cpp` — the `FrontmatterAndThemeGatesStayClosed`
  array drops the ELK fixture, the single-leaf battery's `layout: elk` negative becomes an
  admitted positive (with its footer line) beside a new out-of-table `layout: dagre`
  reject, and the file's header comment no longer lists `layout` as a fallback cause; `tests/ui/test_mermaid_requirement_0037_cohort.cpp` — the
  `Keys outside the admitted set` battery flips `layout: elk` to the admitted positive and
  keeps a `layout: dagre` reject. Nothing is deleted; each flip is paired with a negative.
- Gate `tests/ui/test_mermaid_requirement_0044_cohort.cpp` (5 tests): ledger schema/review/
  pins and sibling↔0037 agreement — every 0044 path appears in
  `requirement-0037-cohort.json`'s `excluded` list with `class == "layout: elk"`, the 0044
  set is disjoint from the 0037 `rows`, and the 0037 excluded class still has 24 entries;
  per-row twin inertness + exact footer composition (both twin kinds non-vacuously counted
  20/3); the closed table (elk admitted; `dagre`/`elk-layout`/case variants/empty/duplicate/
  indent 4/column-0 `layout` rejected; nested map still rejected; the five existing leaves
  unchanged); the composed-order battery (`htmlLabels` + `layout` emits html then layout;
  the three RFC 0037 leaf batteries keep their expected sequences); the literal six-group
  partition (120 + 8 + 4 + 12 drawn, 23 admitted, 5 remaining).
- Matrix branch inserted **immediately before the D-066 F0 requirement block**
  (`tests/ui/test_mermaid_official.cpp` ~10057), ahead of the F0 (~10059), RFC 0037
  (~10082) and base-cohort (~10572) requirement branches, reason
  `RFC-0044 requirement elk layout reviewed cohort`, `raw_active_schema` empty so the
  generic `originals_drawn` counter credits +23.
- Roster: `--emit` 663 → 640, `elk_layout_engine` 38 → 15, category count unchanged (43).

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `layout` admission | 23 rows, 1 family | **done** | red-first measured; requirement **167/5**; global **1505/640**; formal **1296 → 1319**; gaps 48, integrity 0; narrow **0/23** live; dual-preset sha `0732abe2…`; full debug ctest 4675/4675; independent P1 code review APPROVE (r2) |

Closure: full debug ctest once, dual-preset byte gate, RFC `implemented`, closure doc,
design-decisions entry, efficiency retro.

**Closure-report format (TL 2026-11-22):** every closure report states three numbers side
by side — ① the absolute `source_fallback` total (expected **640**), ② the part beyond a
certified profile or frozen/protected authority (expected **289**, unchanged), and ③ this
phase's net reduction (**23**).

## Production Readiness Review

Completed at the `implementable` gate by an independent PRR agent.

### 1. Correctness and tests

- Every new behaviour has a new test: the five-test cohort gate, the three existing-
  assertion flips (each paired with a `layout: dagre` negative), and three hermetic
  `--check` entries.
- Failure paths covered: the closed table and shape batteries.
- Golden suites: N/A — no rendered-cell golden or truecolor path; the text surface is
  asserted by the cohort gate and by the RFC 0037 120-row oracle, which stays green only if
  the new leaf does not disturb frames without `layout`.
- Flakes: none added; no wall-clock assertion.
- Serial ctest total: **4667 → 4675**.
- Couplings named and both-sided: ledger fields, matrix reason, roster category, C++ gate ↔
  Python classifier (quote-aware scalar mirror), the 0037 `EXCLUDED_EXPECTED` constant, the
  base generator's `superseded_note`.

### 2. Build system and module discipline

- BMI PSS: N/A — one `.cpp` body unit; `RequirementEnvelope` already carries
  `config_notices`, so no interface/module change. No concurrency change; no new module.

### 3. Rollback and compatibility

- One atomic commit including the roster, so one `git revert` restores 663 and every pin;
  splitting the roster out would red the `--self-test` (live vs committed).
- No persisted data, wire, CLI, or settings change.

### 4. Observability

- The disclosure footer is the existing requirement notice channel; no new diagnostics,
  threads, or render ticker.

### 5. Documentation and deletion

- No gate is wrapped or duplicated; the phase updates the 0037 generator's human sentence
  and the base ledger's `superseded_note`, plus the docs listed below.

### 6. Platform readiness

- Debug + release `-Werror` on the Linux box; ASCII cell text, no platform code touched;
  macos-14 CI is not a verification gate (directive 2026-09-29); no new fetch; no colour
  painted.

### Review sign-off

| Role | Reviewer | Date | Verdict |
|---|---|---|---|
| Design | agent:design-review | 2026-11-22 | approve (r3) |
| Production readiness | agent:prr-review | 2026-11-22 | approve (2 MAJOR + 5 MINOR + 5 NIT folded) |
| Code (per phase) | agent:code-review | 2026-11-22 | approve (r2; 1 MAJOR fixed) |

## Rollout and rollback

One phase, one atomic commit; rollback is one revert plus a rebuild/re-measure. No
persisted data, wire protocol, CLI, or settings change.

## Drawbacks and risks

- **A disclosed leaf the renderer cannot paint** — stated by the footer.
- **Ordering sensitivity**: appending the new notice last is what keeps the RFC 0037
  frames byte-identical; the composed-order battery pins it.
- **Category causality**: the roster is fallback-only, so the 23 admitted rows leave
  `elk_layout_engine` and the remaining 15 rows keep the `foreign-engine` label — the
  category is never mixed. The admitted rows are inert *in this family's text projection*
  (proven by twins); other families' ELK rows keep the engine debt. On emit
  `predicate_sufficiency` becomes 79 matches / 62 drawn / 17 fallback and
  `sufficient_blocker` stays false.

## Alternatives considered

1. **Do nothing** — leaves 23 twin-proven rows on fallback.
2. **Admit the 24th (nested `config.flowchart.htmlLabels`) too** — needs namespaced-entry
   support and a synthetic twin; rejected as a second mechanism.
3. **git `parallelCommits` (12)** — smaller and second; RFC 0039 called the namespaced
   `config.gitGraph.*` block semantics debt, so a single-key carve-out needs its own
   argument. Deferred.
4. **treemap `valueFormat` / architecture `seed`** — rejected: content-changing keys.

## Testing and verification plan

- New gate (5 gtests) + three hermetic `--check` entries — the new generator plus
  **newly registered** checks for `generate_requirement_0037_cohort.py` and
  `generate_requirement_cohort.py` (both support `--check` and are green today, but neither
  is registered; the base generator is touched, so its check belongs): expected
  `ctest -L mermaid` delta **+8**; full debug ctest **4667 → 4675**.
- The RFC 0037 gate is the drawn-row regression oracle (120 rows) and must stay green.
- Targeted `ctest -L mermaid`; release matrix; dual-preset byte gate at closure; full
  debug ctest once.
- Roster order: review approved → full rebuild → both matrices → cohort gate green →
  `--emit` → `--self-test` → `--matrix`.

## Documentation impact

- [x] `docs/dev/d413-next-family-requirement-elk.md` — selection
- [x] `docs/dev/d414-requirement-elk-rfc0044-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — requirement layout admission entry
- [ ] `docs/README.md` — RFC 0044 row and the RFC 0037 row's still-current "`layout: elk`
      (24) stay fallback" clause
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — requirement row
- [ ] `tools/mermaid/generate_requirement_cohort.py` + `requirement-cohort.json` — the
      frozen D-204 snapshot keeps its rows and counts; the generator-authored
      `superseded_note` live-partition sentence is refreshed to the post-RFC-0044 numbers
      (edit the generator string, then regenerate, so `--check` stays honest)

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: one ledger or extend the 0037 ledger? | design review | proposed: sibling ledger (0037 is a phase record; its exclusions sentence only) |
| OQ-2: narrow_40 disposition | implementation | measured live, pinned |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR.
- Phase: one atomic phase with its review record.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | r1 | agent:design-review | REQUEST-CHANGES | Scope, twin bytes, arithmetic, inertness and wiring all confirmed. BLOCKER: three existing assertions pin `layout: elk` as fallback (`test_mermaid_requirement_cohort.cpp` gated array + single-leaf battery, `test_mermaid_requirement_0037_cohort.cpp` keys battery) and were outside the plan → named as in-scope edits, each flip paired with a `layout: dagre` negative. MAJOR: the two remaining rows' stated causes were wrong (indented closing fence = malformed envelope; leading blank line = no `---\n` at offset 0), and nested maps total 3, not 2 → rewritten. MINOR: the 0037 generator's `EXCLUDED_EXPECTED["layout: elk"]` stays 24 (source-shape) with only the human sentence changing; the 0037 `--check` is a newly registered entry, not an existing one; NITs: scalar trims spaces only and dequotes an outer same-char pair (tabs rejected upstream), the notice-order claim is vacuous for old frames and pinned only for the new rows, and the roster is fallback-only so the category cannot mix. All folded |
| 2026-11-22 | r2 | agent:design-review | REQUEST-CHANGES | r1 folds confirmed faithful (three assertion edits, 0037 generator constant, +7 registry, scalar/order/roster wording). New MAJOR introduced by the r1 rewrite: the nested-map attribution said "two ELK rows" and "only the ELK one", which is self-contradictory — the correct partition is 1 ELK nested row (also htmlLabels, no twin) + 2 non-ELK nested rows (one plain, one `look`) → fixed. MINORs: Non-Goals still used the retracted descriptors → aligned; the `superseded_note` is generator-authored, so the generator is added to the doc-impact list; the family test's header comment is added to the in-scope edits. All folded |
| 2026-11-22 | r3 | agent:design-review | **APPROVE-TO-ACCEPT** | All r2 fixes verified against fixtures and roster; all recomputed numbers consistent (172/144/28; 24 elk = 15+8+1; 23 promoted; 5 remaining; 38→15; 640/289/23). One optional NIT: G2's causal clause was loose → softened. Gate: provisional → accepted |
| 2026-11-22 | PRR | agent:prr-review | REQUEST-CHANGES | 2 MAJOR + 5 MINOR + 5 NIT. MAJOR: the touched base generator's `--check` was unregistered (+8, total 4675); twin selection was undefined and the footer delta ambiguous (rows have several body-identical drawn twins with different footers) → pinned footer-free-first/prefix-second with both blocks in the ledger and a composed strip-then-append rule. MINORs: ctest total stated; matrix anchor named; sibling↔0037 assertion specified; README status and the 0037 README clause added to doc impact; red-first and the P1 code-review gate added to the phase criteria. NITs: five existing leaves, six-group partition, 0037 docstring, quote-aware scalar mirror, no F0 generator. All folded |
| 2026-11-22 | P1 | agent:code-review | r1 **REQUEST-CHANGES** → r2 **APPROVE** | r1 ran the binaries and confirmed the gate, matrix wiring, three flips, twin composition and ledger arithmetic; one MAJOR: the generator's `admits()`/scalar mirror was naive (no indent-2 check, no quote-aware comment/dequote) and contradicted RFC §2 → fixed to mirror `requirement_inert_scalar` exactly and reject indent≠2/duplicates/namespaced entries, with direct probes matching every C++ battery verdict and all `--check`s reproducing. MINOR/NIT folded: stale d117/README requirement rows, empty-config footer guard, duplicate top-level keys. Residual documented: `admits()` does not mirror the title ink/width rules; no 0044 row carries a title |
| 2026-11-22 | — | — | draft | Created provisional from D-413 |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | e06c993d |
| 2026-11-22 | — | Folded design r1/r2; accepted after r3; no code | 33459cfe |
| 2026-11-22 | — | PRR folded (+8 ctest, twin rule, anchor); implementable; no code | e864a051 |
| 2026-11-22 | P1 | layout admission implemented: one TU leaf + footer, 23 rows, sibling ledger, five-test gate + three hermetic checks, matrix branch, roster 663→640 | 1f1debb5 |
