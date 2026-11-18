---
rfc: 34
title: Whole-source admission of the handDrawn nested group-edge ER original (ER206 ordinal 198)
status: accepted
owners: "@Zzzode"
reviewers: ["agent:a360f1f39bbddbbf0 independent design review r1 — REQUEST-CHANGES (1 MAJOR + 3 MINOR + 3 NIT, all prose-only, closed)", "agent:a19d5b125b909dd84 independent design review r2 — APPROVE-to-accept (all seven r1 findings verified CLOSED, two cosmetic NITs folded in)"]
created: 2026-11-18
last-reviewed: 2026-11-18
supersedes: ""
tracking: ""
---

# RFC 0034 — Whole-source admission of the handDrawn nested group-edge ER original (ER206 ordinal 198)

## Summary

One ER original remains just outside the RFC 0033 six-case owned-proof cohort:
ER206 prior ordinal 198,
`er-diagram/visual-check/nested-er-subgraphs-with-direction-override-handdrawn-look.mmd`
(source sha `c48919288f56b2cb5bae9050ff75afa41faca478d0d77c0f6cc442e86465954b`,
manifest global ordinal 647). It is the seventh row of the loose
group-collision superset and was sha-pinned out of RFC 0033 by a reviewed
`er-envelope-double-parse-work-ceiling` blocker because, unlike the other six,
its whole source was believed to exhaust the ER scene Work budget. This RFC
proposes to give that one row an authority-owned `drawn` proof. New measured
evidence shows the historical "Work ceiling" framing needs refinement: the
whole source now **parses** within budget, but its whole-source **native
layout still emits no art and falls back to plain text**, while the
frontmatter-stripped body draws with zero geometry errors. The work therefore
has two coupled parts — a narrowly-scoped production render/Work capability,
then a parallel owned-proof cohort built exactly like RFC 0033 — and must not
weaken any frozen ER107/ER206 asset.

## Motivation

### Why the status quo is inadequate

Ordinal 198 is retained as `source_fallback`, so the matrix under-counts one
original that the renderer is structurally capable of drawing. RFC 0033
removed the group-collision half of its two debts for the six simpler rows;
row 198 carries the same group-collision debt plus an `active_config`
(`look: handDrawn`) debt, nested two levels deep with a group-as-edge
relationship and an in-group direction override. Leaving it pinned forever
converts a measurable, single-row capability gap into an unexplained permanent
fallback, which contradicts the D-329 principle that every fallback must be
explained and either closed or explicitly excluded with a current reason.

### Evidence (measured on the current tree, debug preset, 2026-11-18)

A temporary feasibility probe (removed before any commit) drove the existing
test-only seams and `er206_observe` over the exact pinned fixture bytes.

| Measurement | Observed value | How measured |
|---|---|---|
| Full source size / body size | 532 B / 222 B | byte count of fixture and of the substring after the closing `---` fence |
| Whole-source parse (`er_whole_scene_json_for_testing`, production 1,000,000 Work) | succeeds, `remaining = 775285` | direct seam call, read `Work.remaining` |
| Body-only parse | succeeds, `remaining = 788378` | same seam on frontmatter-stripped body |
| Body-only typed Model | 3 entities, 2 groups, 2 relations | `er296_typed_model` |
| Body-only wide geometry (`validate_impl`) | 0 errors | oracle on `er206_observe(body)` wide grid |
| Whole-source native render | plain-text fallback: `wide == ordinary` is true; geometry error `Missing exact entity/header/member/control/group outline 0` | `er206_observe(full)` + same-grid + oracle |
| Same body, `look: handDrawn` | parses, renders plain text | look-variant probe |
| Same body, `look: neo` | parses, renders plain text | look-variant probe |
| Same body, `look` line removed | envelope invalid, parse rejected | look-variant probe (control) |
| ER `handDrawn`-look originals in the live matrix | 43 rows: 32 drawn, 11 source_fallback | OfficialMatrix aggregate query |
| Single-level `er-subgraphs-handdrawn-look.mmd` | drawn via an ER107 owned proof | OfficialMatrix per-row disposition |

Two conclusions follow. First, the blocker is no longer an *exhaustive parse*:
the parser returns a complete scene with roughly 775k of its 1M Work unused,
so later Work fixes (the D-066/D-117 line of work) have already retired the
parse-time exhaustion that motivated the original FS-1b pin. Second, the
failure is on the **whole-source render/layout route under an active
projection envelope**, not on the `handDrawn` value itself: the `neo` look
fails identically, the body without an envelope draws, and most handDrawn ER
originals (including a single-level handDrawn subgraph) already draw. The
discriminating factors are envelope activation plus the deepest nesting
(domain > details) combined with the group-edge and direction override.

The exact render-side meter or gate that refuses the art was not pinpointed
by the read-only probe; identifying it is an explicit phase-0 deliverable, not
an assumption (see Open questions).

## Goals

- G1. Ordinal 198's **whole source** (frontmatter plus body) reaches an
  authority-owned `drawn` disposition in the OfficialMatrix at wide_240 and
  restored_240, with a measured narrow disposition, zero errors, through a
  reviewed owned proof — not through a heuristic or a weakened gate.
- G2. The production change that makes the whole source draw is the smallest
  render/Work-scoped change demonstrably required, and keeps every ER107 (107)
  and ER206 row byte-identical (dual-preset matrix sha unchanged for all other
  rows).
- G3. All frozen authority assets stay byte-immutable; the new authority lives
  in new files reached only through additive, defaulted paths, reusing the
  RFC 0033 supplemental-proof mechanism.
- G4. The `handDrawn` fidelity boundary is unchanged: the target is the native
  orthogonal projection already documented in the ER config notices
  (`look=handDrawn -> native orthogonal; roughness/relief not reproduced`), not
  hand-sketched visual fidelity.
- G5. The decision is falsifiable and reversible: if the required production
  change cannot keep the frozen baselines byte-identical, the row stays a
  documented `source_fallback` and no permanently red gate is landed.

## Non-Goals

- Reactivating the dormant ER107 live matrix join (`MATRIX_API_CONFIRMED=False`);
  that remains a separately-scheduled RFC.
- Reproducing handDrawn roughness/relief, neo styling, or any foreign-engine
  visual effect; native orthogonal output is the only promised art.
- Admitting the other ten handDrawn ER fallbacks; measured against the frozen
  prior they form three distinct construct classes, each with its own
  unadmitted surface, and all stay out of scope: (i) rich header/attribute
  exact-projection-contract rows, (ii) active entity presentation via
  classDef/style application (`active_entity_presentation` /
  `active_class_definition_or_application` debts), and (iii) the rich
  relation-label/line-break exact-projection row (ordinal 200, which carries
  only that debt). The partition is by dominant construct: ordinals 67 and 70
  straddle (i) and (iii) but are grouped under (i) because they also carry the
  rich header/attribute debts; every one of the ten rows is covered by at
  least one class. This list is the observed partition of the current ten and
  is not a claim that no other construct exists.
- Changing the ER206 prior, the ER107 `array<Pin,107>`, its three proposals, or
  the 48-row mandatory gap roster.
- Any change to the formal `original_drawing_coverage.drawn_at_240` semantics;
  pending rows continue to earn no formal credit.

## Proposal

### Shape of the work: capability first, authority second

Unlike RFC 0033 (where the renderer already drew the six bodies and only
authority was missing), row 198 cannot use an owned proof alone: the in-function
geometry re-validation would fail because the whole source currently emits no
native art. The RFC therefore has a strict ordering — the render capability
must exist and be measured before any proof is curated.

Dependency direction: every production change stays inside the existing
`loom.ui.visual.mermaid` area (the Work/envelope fee and meter logic in
`mermaid_er_impl.cpp`, `mermaid_relations_core_impl.cpp`, `mermaid_impl.cpp`
and `mermaid.cppm`); no new module, no moved module and no cross-area edge is
introduced, so the module graph DAG and the named-directory SCC set are
untouched by construction. All authority work is test-only. A Tarjan check at
the implementable gate must confirm no upward edge was added.

```
production render/Work capability (src/)  ──▶  whole source draws natively
        │                                              │
        └─ byte-identical ER107/ER206 gate             ▼
                                          parallel owned-proof cohort (new files,
                                          RFC 0033 supplemental-slot pattern)
                                                          │
                                                          ▼
                                          OfficialMatrix drawn, prior stays frozen
```

### Phase 0 — instrument the exact refusal (read-only spike)

Add a temporary, deleted-before-merge instrument (or extend the existing
`FinalEmissionTrace` meter) to report which Work balance first refuses on the
whole source inside `relations_layout` / ER ElementWork emission, and whether
the refusal is a budget exhaustion or a presentation-specific parse gate
(the `diagram.er_presentation` branches in `parse_plain_er` around
`mermaid_relations_core_impl.cpp:1407,1476`). Output must distinguish:
(a) envelope double-charge on the 1M `kWorkLimit`; (b) the ER ElementWork
meter `kErElementWorkLimit`; (c) a semantic gate unrelated to budget. The
historical FS-1b label is retained only as prior context, not assumed correct.

### Capability design — the two viable routes (to be selected after phase 0)

- **Route A, envelope parse reuse / double-charge removal.** If phase 0 shows
  the envelope frontmatter parse and the body parse are charged separately
  against one Work budget for no extra information, restructure so the
  envelope's parsed facts feed the body parse without a second full charge.
  This is a fee-schedule change in `src/ui/visual/`; every fee is observable
  through the frozen ER matrices, so the change is gated on byte-identity.
- **Route B, bounded envelope-specific budget.** If the refusal is a fixed
  overhead that genuinely cannot be reused, grant the active-projection
  envelope path a bounded, separately-metered allowance sized to this
  fixture class, with an explicit cap and fail-closed behaviour beyond it.
  This is not a new mechanism: the codebase already separates Work by phase —
  the 1M layout meter `kWorkLimit = 1'000'000` (`mermaid.cppm:94`) is distinct
  from the active-ER finish meter `kErElementWorkLimit = 102'564'352`
  (`mermaid.cppm:99`), Block emission owns a dedicated `payload_work` under
  `kBlockPayloadWorkLimit`, and `FinalEmissionTrace` (`mermaid.cppm:180-182`)
  is the existing injection point for a caller-supplied finish meter. Route B
  reuses that proven fail-closed meter shape rather than inventing one. This
  is preferred over raising the global 1M limit (see Alternatives): the matrix
  byte-gate embeds per-row `initial_budget` / `remaining` /
  `phase_remaining`, so a global change would desynchronise frozen output for
  every row, whereas a new isolated meter leaves the existing balances
  untouched.

Both routes are production changes and therefore require the implementable
gate and PRR; neither may silently increase cost for the 2,737 other rows.

### Authority design — a one-row parallel cohort, RFC 0033 mechanics

Once the whole source draws, curate a new owned proof for ordinal 198 in new
files (a one-case proposal JSON and support header) that reproduce the RFC
0033 §4.2 dual-track rigor: an independent source leg
(`source_model`/`source_witnesses`/`source_binding` with live raw-byte witness
checks, zero production-parser calls in that leg), a field-by-field decode
cross-check of the production `whole_scene_model`, the 15-capture
(3 cache modes x 5 widths) closed set, per-case mutation identity, Work caps
and the width/narrow XOR. Route it through a new additive defaulted parameter
and a matrix branch structurally exclusive with both the ER107 and RFC 0033
cohorts, with a compile-time disjointness assertion. The ER206 prior row 198
is never edited; a legacy view keeps it formally pending, mirroring
`dd3e1b47`.

The one-row verifier must carry the same mechanical, non-narrative cohort
discipline as the RFC 0033 four-part assertion, all bound by path plus sha:
(i) ordinal 198 is the UNIQUE prior row carrying the exact two-debt set
`{active_config, raw_entity_group_kind_collision…}` (measured: exactly one of
206), pinned by its fixture path and source sha c4891928…; (ii) a compile-time
disjointness `static_assert` against both the 107 ER107 pins and the six RFC
0033 pins (the existing pattern is `mermaid_er_groupedge_source.hpp:154-161`);
(iii) the verifier asserts the old BLOCKED-set entry is retired by this cohort
rather than merely orphaned; (iv) the new proposal/contract use a new id so
the six-case `PROPOSAL_SHA` and empty-intersection assertions stay valid.

### Protected-area analysis

- **ER206 prior (`...config62.v1.json`, sha `76d54ecb…`, protected).** Row 198
  stays whole-null with its two debts. The RFC does not rewrite the debt or
  the row; admission is expressed entirely through the parallel proof and the
  matrix routing, exactly as RFC 0033 did for its six rows.
- **ER107 authority (three proposals + `array<Pin,107>`, protected).** Not
  edited. A constexpr assertion proves row 198's path is disjoint from every
  ER107 pin and from the six RFC 0033 pins.
- **Mandatory gap roster (48 rows, sha `1fe297…`, protected).** Row 198 is a
  `source_fallback`, not a mandatory gap, so drawing it does not alter the
  48-gap roster; its size and sha stay unchanged.
- **Fallback registry (`tools/mermaid/fallback_roster.json`, D-325 locked, not
  one of the ten RFC 0033 protected files but separately drift-gated).** Moving
  one row from fallback to drawn changes the locked total 907 -> 906. This is
  an expected, separately-reviewed registry update performed in the same
  closing change, verified by `fallback_attribution.py --matrix`; it must never
  be hand-edited to force a green gate.

### RFC 0033 artifacts that already pin row 198's exclusion (change-surface decision)

Row 198 is sha-pinned as *excluded* by several RFC 0033 assets using the exact
`er-envelope-double-parse-work-ceiling` blocker this RFC retires. None is one
of the ten byte-immutable protected files, but each encodes reviewed semantics
that would otherwise drift silently; the closing change must handle all of them.

| Artifact | How it pins 198 | Decision in this RFC |
|---|---|---|
| `tools/mermaid/verify_er_groupedge_admission.py` | `BLOCKED` dict (`blocker_kind` work-ceiling, sha c4891928…), `LOOSE_SUPERSET` ending in 198, four-part assertion (iii); ctest-wired; also `PROPOSAL_SHA` pins the six-case artifact | Retain the prior/debt assertions (the prior is unchanged and 198 stays absent from the six-case proposal). In the closing change, update the `BLOCKED`/blocker comment to cross-reference RFC 0034 and record that the exclusion is retired by the new cohort, and extend the verifier to assert the BLOCKED-set retirement rather than leave a stale "permanent ceiling" claim. |
| `tests/ui/test_mermaid_er_groupedge_proposal.cpp` | emits `excluded_work_ceiling` with the "exhausts the ER scene Work budget before layout" reason and the handDrawn sha | Update the reason text/cross-reference in the same closing change so it no longer asserts a current exhaustion; the emitted six-case proposal must otherwise stay byte-identical (see new-file rule below). |
| `tools/mermaid/er-group-edge-cohort.json` | non-authoritative D295 ledger: `prediction: "predicted-blocked"`, expected states source/source/source, naming the source site | Regenerate or mark the row superseded in the closing change; this ledger is not ctest-wired and is explicitly non-authoritative. |
| `docs/dev/mermaid-failure-inventory.json`, `docs/dev/mermaid-unsuccessful-cases.md` | narrative/ledger documentation of the fallback | Update or add a supersession note at closure so future audits do not read an obsolete Work-ceiling reason. |
| New one-row cohort (to be created) | — | MUST be a new proposal file with a new contract id, never an edit to the six-case `er_groupedge_complete_proposal.v1.json`; amending that file would trip the verifier's four-part empty-intersection assertion and its `PROPOSAL_SHA` pin. |

The hard gates stay green throughout because they bind immutable prior facts
and the unchanged six-case proposal; what is actively maintained here is the
reviewed *exclusion narrative*, which is the string-coupling drift class called
out in `AGENTS.md`.
- **Production Work/envelope code (`src/ui/visual/`, not protected but high
  blast radius).** The only non-test surface touched. Gated on the dual-preset
  full OfficialMatrix remaining byte-identical for all rows except 198.

### String/shape couplings that can break silently

- The `look` leaf values are string-matched (`"handDrawn"`, `"neo"`) in
  `mermaid_er_impl.cpp:42-45`; any route that touches config handling must keep
  those spellings and the bit-mask (`512` for look at `:43,:265`) intact. The
  same values are consumed outside the config parser at
  `mermaid_relations_core_impl.cpp:5566-5569` (stroke-width selection) and
  `:5904` (the fidelity notice), so a spelling change is a multi-site coupling.
- Frontmatter-format asymmetry: the `%%{init:}` JSON envelope calls
  `er_json_config` with `projection` defaulting false
  (`mermaid_er_impl.cpp:174` vs the YAML path's `true` at `:260`), so the
  bit-512 look leaf is reachable ONLY through YAML frontmatter. Any Route A/B
  change must preserve this asymmetry rather than quietly enabling look leaves
  on the init-directive path.
- The matrix provenance and count functions recognize contracts by string id;
  a new contract id must be added to the count/provenance/coverage-skip paths
  in lockstep (the omission that required the `dd3e1b47` follow-up; lockstep
  sites at `test_mermaid_official.cpp:7983-7998, 8452-8464, 10372/10434`).
- The fixture is identified by path plus sha; the cohort assertion must bind
  both, never the path alone, and re-bind the live four-tuple at
  `test_mermaid_official.cpp:19271-19279`.

## Phases and graduation criteria

Required before this leaves provisional; each phase is independently revertible.

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| 0 | Refusal instrumentation | temporary probe + meter readout, no src/ change on master | proposed | Named refusing meter/gate with a captured readout; body draws, whole source does not, reproduced on both presets |
| 1 | Render/Work capability | minimal src/ change from route A or B | proposed | Whole source emits native art; full dual-preset OfficialMatrix byte-identical for all 2,737 other rows; ER107 107/107 unchanged |
| 2 | One-row owned proof | new proposal + support header + verifier, test-only | proposed | Dual-track `owned()` passes with live witness checks; tampered-byte and not-ready negatives fail closed; disjointness static_assert holds |
| 3 | Matrix wiring | additive parameter/branch + legacy view, test-only | proposed | Row 198 drawn wide/restored with measured narrow disposition; zero errors; routing exclusive with both existing cohorts |
| 4 | Registry reconciliation and closing matrix | fallback registry 907 -> 906 update | proposed | Single full OfficialMatrix: drawn 1238 -> 1239, fallback 907 -> 906, error 0, gaps 48, formal drawn_at_240 1053; `--matrix` drift gate green; protected ten-file loop unchanged |

**Expected interim red state (phases 1-3).** Once phase 1 makes the whole
source emit native art but no proof is wired yet, `run_official_er206_case`
can no longer take the whole-null downgrade; the row stays `error` and books
`er206_whole_null_unmet`, which also fails the hard `gap_diff.ok()` /
`EXPECT_EQ(observed_gap_rows.size(), 48)` gate
(`test_mermaid_official.cpp:11474-11477`). Therefore the full OfficialMatrix is
expected to be RED by one row from the end of phase 1 until the phase-3 proof
is attached. During that window only targeted tests are green; exactly one
full matrix is run, at the phase-4 closure, mirroring the discipline in
`0033-slice2-implementation.md` §4.1. A red interim matrix must never be
misread as a regression, and the tree must never be left at a red gate across
a commit boundary without an explicit documented reason.

## Production Readiness Review

To be completed in full at the implementable gate. Headline items already
known: the production Work change needs a before/after dual-preset byte gate
over the entire matrix (not just targeted tests); the new meter must be
fail-closed with a hard cap; session traces and settings are unaffected because
the change is confined to ER mermaid rendering; no persisted data or wire
protocol changes.

## Rollout and rollback

- Phases land as separate local commits; nothing is pushed until the full gate
  set is green.
- Phase 1 (the only src/ change) is independently revertible: reverting it
  returns row 198 to plain-text fallback while leaving phases 2-3 test-only
  assets harmless (their `owned()` proof simply never reaches `ready`).
- If any phase cannot satisfy its byte-identity gate, stop, keep the row as
  documented `source_fallback`, and write a same-format negative-result record;
  the repository must never be left on a permanently red full matrix (the same
  terminal-state discipline as RFC 0033 §5.2).
- No data migration and no flag-day: all new parameters are defaulted and all
  new files are additive.

## Drawbacks

- It requires a production Work/envelope change, unlike the purely test-side
  RFC 0033 cohort, so the review burden and byte-identity risk are higher.
- A budget route, if chosen, can invite future "just raise the cap" pressure;
  the bounded-cap and fail-closed design plus the byte gate are the only
  safeguards.
- Curating a one-row cohort carries fixed authority overhead (proposal,
  verifier, review receipt) for a single drawn row.

## Alternatives considered

1. **Do nothing — keep row 198 a permanent source_fallback.** Loses because the
   body is demonstrably drawable today and the historical parse-exhaustion
   reason no longer holds; retaining the pin without a current cause violates
   the explained-fallback principle. Acceptable only as the documented outcome
   if the byte-identity gate cannot be met.
2. **Raise the global 1M `kWorkLimit`.** Rejected: a global budget increase
   silently changes cost accounting for every relation source and risks
   desynchronising the frozen Work-pinned contracts; it is the blunt
   instrument the bounded, route-specific allowance is meant to avoid.
3. **Owned proof without any production change (copy the RFC 0033 cohort
   only).** Rejected on measurement: the in-function geometry re-validation
   runs against frames that currently equal the plain-text grid, so the proof
   could never reach `ready`; it would be an unreviewed-artifact backdoor.
4. **Edit the ER206 prior row to erase the debts and admit the row directly.**
   Rejected: the prior is a sha-frozen protected authority; mutating it breaks
   the cross-gate pins and the 107/206 byte-identity mandate, the same
   argument that selected candidate (b) in RFC 0033.
5. **Reproduce true handDrawn roughness.** Rejected as scope: the project's
   established ER projection deliberately maps handDrawn to native orthogonal
   and documents the non-reproduction of relief; visual-fidelity work is a
   separate, much larger effort.

## Testing and verification plan

- Phase 0: temporary instrumented test only, deleted before merge.
- Phase 1: targeted ER render tests plus the full dual-preset
  `MermaidCapabilityReport.OfficialMatrix`, asserting every row other than 198
  is byte-identical (matrix sha comparison across presets and against the
  pre-change baseline for the unchanged row set).
- Phases 2-3: new gtest covering the one-row `owned()` proof, tampered-byte and
  absent-proof negatives, cohort disjointness, and the in-function four-tuple
  identity plus geometry re-validation; a standalone read-only Python verifier
  with an empty review-sha map that fails closed until an independent review
  lands.
- Phase 4: single closing OfficialMatrix with the measured census in the phase
  table, the `fallback_attribution.py --matrix` drift gate, the group-edge and
  ER107 verifier self-tests, and the ten-file protected sha/byte loop.
- Both debug and release `-Werror` clean; serial ctest is the signal.

## Documentation impact

- [ ] `docs/decisions/design-decisions.md` — record the envelope Work-charge
  decision and why the chosen route preserves frozen contracts.
- [ ] `docs/dev/engineering-efficiency.md` — record the measured meter and the
  dual-preset byte gate outcome.
- [ ] Code comments at the changed Work/envelope site and in the new verifier.
- [ ] This RFC's Implementation History and, at closure, a closure-evidence
  document mirroring `0033-slice2-closure.md`.

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| Which exact meter/gate refuses the whole-source art (budget vs presentation gate), and what is its measured remaining balance at refusal? | @Zzzode | Phase 0 instrumented spike |
| Route A (parse reuse) or Route B (bounded envelope allowance) — which is the smallest change that holds byte-identity? | @Zzzode | Phase 0 evidence + design review |
| What is row 198's correct narrow_40 disposition (exact-ordinary vs full-geometry source_fallback)? | @Zzzode | Measured in phase 3 from live frames, never assumed |
| Does drawing row 198 require touching any of the ten RFC 0033 protected files? (Expected answer: no.) | @Zzzode | Phase 2-3 file audit |
| Are all five RFC 0033 exclusion-pinning artifacts (verifier, six-case proposal test, D295 ledger, two docs) reconciled in the closing change without editing the six-case proposal bytes or any protected file? | @Zzzode | Phase 2-4 change-surface audit (see "RFC 0033 artifacts" table) |

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-11-18 | — | RFC opened (provisional); feasibility probe measured parse-success vs render-fallback, then removed | d214e039 | see Motivation/Evidence table |
| 2026-11-18 | — | Independent design review r1 (agent a360f1f39bbddbbf0): REQUEST-CHANGES, 1 MAJOR (RFC 0033 exclusion-pin change-surface omitted) + 3 MINOR + 3 NIT, all prose-only; all six measured claims independently verified | — | reviewer ledger; no code change |
| 2026-11-18 | — | r1 findings addressed in prose: exclusion-pin decision table, interim red-matrix statement, three-class Non-Goal, two-meter Route B precedent, extra look/init couplings, dependency-direction statement, four-part one-row verifier discipline | (uncommitted revision) | r1 finding list |
| 2026-11-18 | — | Independent design review r2 (agent a19d5b125b909dd84): APPROVE-to-accept; all seven r1 findings verified CLOSED against code/data, two cosmetic NITs (citation line, 67/70 partition wording) folded in; all checkable measured claims exact, RFC lint-clean | (this revision) | r2 verdict; not implementable until phase-0 meter evidence + PRR |
| 2026-11-18 | — | Stage transition provisional -> accepted on the r2 design-review gate; held below implementable pending phase 0 and the production-readiness review | (this revision) | rfc skill §1/§3.1 |
