---
rfc: 33
title: Owned-proof path for whole-null ER group-edge originals (ER107 cohort amendment)
status: provisional
owners: "@Zzzode"
reviewers: ["agent:/a61c87ba independent architecture/security review — r1 REQUEST-CHANGES (3 MAJOR/4 MINOR, closed); r2 REQUEST-CHANGES narrow (MAJOR-A literal-id + 4 NIT, closed); r3 APPROVE (only non-blocking NIT-5, closed)"]
created: 2026-11-17
last-reviewed: 2026-11-17
tracking: Tech Lead D-306; follows D293/D298 NO-GO (docs/er107-group-edge-admission-nogo.md) and D301 geometry falsification
---

# RFC 0033: Owned-proof path for whole-null ER group-edge originals

## Summary

- Status: **Approved (r3 APPROVE 2026-11-17) — implementation permitted,
  gated by the §7.0 full-frame oracle spike before any frozen asset.**
- Scope: answer exactly one question — *how do the six whole-null ER
  group-edge originals obtain an authority-owned proof so they classify
  `drawn` in the OfficialMatrix?* Nothing in this RFC changes renderer
  geometry (D301 proved none is needed) or the existing ER107 cohort.
- Extends/supersedes: the blocked 107→113 "atomic re-baseline" idea. D298
  (`docs/er107-group-edge-admission-nogo.md`) established that parser-only
  admission yields 6 errors / 0 new drawn; D301 (same doc §8) established
  that the six already satisfy the drawn geometry predicate once parsed. The
  only missing element is the owned proof.

## Motivation

Six whole-null ER group-edge originals satisfy the drawn geometry predicate
once parsed (D301), but a whole-null original reaches `drawn` only through a
supplemental-proof branch: unreviewed render output must book an error, never
a draw. The only missing element is an authority-owned proof; §2 records why
that property may not be weakened.

## Goals

- Obtain an authority-owned proof for the six originals so they classify
  `drawn` in the OfficialMatrix without changing renderer geometry.
- Preserve the ER107 107-case baseline, the ER206 prior and the three
  fail-closed gates byte-identically.
- Keep the new proof cohort mutually disjoint from ER107 with compile-time
  and verifier-recomputed assertions.

## Non-Goals

- Renderer geometry changes (D301 proved none is needed).
- Flipping `MATRIX_API_CONFIRMED`, `REVIEWED_HELPER_SHA`/`kB3ReviewStatus` or
  `WRITE_REVIEW_APPROVED`; those remain owner-gated.
- Editing any sha-frozen ER107 review helper or the ER206 prior.

## Proposal

Build a second, parallel owned-proof slot (candidate (b), §4) validated by a
dual-track Model authority and a full-frame oracle pre-gate (§7.0); §3 rejects
the fold-into-ER107 candidates (a1)/(a2).

## Alternatives

- (a1) rewrite the ER206 prior debt and (a2) widen the ER107 selection
  predicate: both mutate sha-frozen, cross-gate authorities and cannot keep
  the 107 baseline unchanged (§3).
- The explicit-exclusion stop outcome (§6): if the pre-gate spike or review
  fails, the six are recorded as permanently excluded rather than admitted on
  unreviewed art.
- Do nothing: the six stay in error, which the matrix counts as a mandatory
  gap rather than a resolved state.

## 0. Revision history (r1 review findings → disposition)

- **MAJOR-1** SourceProfile terminal-test co-change was unstated and the
  stash version downgrades two assertions to report-only → now an explicit
  §4.1 with a bounded transition and reclassification exit.
- **MAJOR-2** rigor parity relied implicitly on the terminal-model seam,
  risking a self-referential authority → §4.2 now mandates a dual-track
  Model authority (independent source leg + field-by-field decode cross-check).
- **MAJOR-3** D301 validated only the lightweight predicate, not the full
  `check_complete_frame`/`validate_impl` oracle → §7.0 makes that oracle spike
  a hard pre-gate before any frozen asset is generated; failure triggers §6.
- **MINOR-1** per-case mutation identity pinning added (§4.2).
- **MINOR-2** cohort disjointness now requires a compile-time assertion plus
  verifier recomputation of both prior predicates (§4.3).
- **MINOR-3** formal coverage stays 1053 and the ER107 verifier's dormant
  drift are disclosed (§5.1).
- **MINOR-4** "must edit an ER107 sha-pinned helper" is now an explicit §6
  exclusion trigger; the new cohort may only include/reuse, never edit.
- NITs: "15 captures" wording; default-parameter ordering; bring D295 assets
  into the tree as implementation step zero.

## 0.1 r2 review findings → disposition

- **MAJOR-A** §4.1 enumerated the wrong five "remaining terminal" literals.
  Verified against `kErSubgraphLiterals`: the real five are
  `duplicate_group_ID`, `kind_collision`, `in_group_styles`,
  `inline_class_association`, `in_group_accessibility`; `header_missing_separator`
  is one of the four `malformed_source` (out of scope) and no `classDef`
  literal exists. §4.1 corrected with the exact ids and the malformed four
  called out as unchanged.
- **NIT-1** §4.1 now pins the independent second counter
  (`MermaidErGroupedPresentation`, `unsupported_observed == 9`) as unmoved.
- **NIT-2** the report-only transition is anchored to explicit work item
  **RFC0033-T1** with an objective exit, preventing an open-ended time-box.
- **NIT-3** §4.2 now requires live witness raw-byte slice/line checks
  (mirroring verify_er107_admission.py:150-157), not component shas alone.
- **NIT-4** §7.1 step zero states the D295 char gate is a non-authoritative
  smoke only and can never be wired into a matrix proof path.

## 1. The six originals and their fixed facts

Renderer-free, sha-pinned selection (the D295 generator reproduces the exact
set over all ER originals): six draw candidates plus one handDrawn whole
source Work-ceiling row that stays `source_fallback` and is out of scope.

| prior ordinal | fixture (short) | ER206 whole-null debt |
|---|---|---|
| 26 | relationships-between-subgraphs | raw_entity_group_kind_collision_not_representable_without_loss |
| 27 | subgraphs-direction-override | same |
| 29 | subgraphs-quoted-ids | same |
| 39 | subgraph-and-entity (pr-check) | same |
| 196 | nested-direction-default | same |
| 197 | nested-direction-elk-layout | same |

All six are present in the sha-frozen ER206 prior (read-only ordinals above),
have `whole_model = null`, and are **not** in the ER107 kPins cohort of 107,
whose membership is derived by the exact predicate
`whole_model is None AND debts == {active_config}`
(`tools/mermaid/verify_er107_admission.py`).

D301 measured, on the production-faithful path (`er206_observe` →
`note13_capture` single-char Grids → `following_row`, the same helpers
`run_official_er206_case` uses), that all six satisfy
`!wide && restored && a && b && c` with ample Work budget once the two
`parse_plain_er` admissions are applied. The renderer work is therefore
complete; only the authority attestation is missing.

## 2. Why an owned proof is required at all

In `run_official_er206_case`, a whole-null ER original reaches `drawn` only
through the supplemental-proof branch (`test_mermaid_official.cpp` ~19134).
`Observation::status` defaults to `{"error",...}`; without a proof, a source
that now renders differently from the text control cannot be downgraded to
`source_fallback` and books `er206_whole_null_unmet`. The "never admission"
wording is deliberate: art that has no reviewed authority behind it is an
error, not a draw. Any solution MUST preserve this property — there must be no
path by which unreviewed render output earns a draw.

## 3. Candidate (a) — extend the ER206 debt classification / selection predicate

Fold the six into the existing ER107 proof cohort by making their debt
grantable, then let the current `official_er107` machinery prove them.

Two sub-forms, both rejected:

- **(a1) rewrite the six rows' debt in the ER206 prior** from
  `raw_entity_group_kind_collision…` to `active_config`. The prior is a
  sha-frozen authority (`PRIOR_SHA = 76d54ecb…`) also consumed by the Block58
  join and the ER107 lock/identity checks. Editing it invalidates
  `prior_ER206_case_canonical_sha256` across the proposal, the cohort
  `LOCK_SHA`, and every helper pin; it changes a protected, cross-gate asset
  and directly hits the D-306 stop condition ("requires changing the ER107
  sha-frozen prior structure").
- **(a2) keep the prior but widen the selection predicate** to also accept
  group-collision debt. `verify_er107_admission.py` hard-asserts
  `len(selected) == 107`, that the source cases equal the selected set, and a
  fixed mutant census (359 semantic / 734 physical / 841 official) and cohort
  lock. Widening to 113 forces: regenerating the three large `mermaid_native`
  proposal JSONs with the 27-field schema, resizing both
  `std::array<Pin,107>` tables to 113, recomputing every aggregate/component
  hash, and rewriting the frozen counts — touching the very 107-case baseline
  and protected pins the milestone is required to leave byte-identical.

**Costs common to (a):** mutates sha-frozen, cross-gate authorities; cannot
preserve the 107 baseline's zero-change requirement; large, non-additive blast
radius across ER206/ER107/Block58; the structural debt mismatch (the rows are
genuinely a different debt class) makes predicate widening semantically
dishonest — they are not `active_config` cases.

## 4. Candidate (b) — a second, parallel owned-proof slot (RECOMMENDED)

Introduce a separate, independently reviewed cohort authority for the six
group-edge originals, parallel to ER107 and mutually disjoint from it, wired
into `run_official_er206_case` as a second optional proof parameter (defaulted
to null so the three existing call sites compile unchanged; consulted only
when `official_er107::select` returns nullopt).

Components, all additive in new files:

1. `tests/support/mermaid_er_groupedge_*.hpp` — its own
   `std::array<Pin,6>`, a renderer-free `select(path, raw_sha)`, a `run()` that
   performs the captures/checks, and a closed-schema `owned(JsonVal)`.
2. Cohort ledger `tools/mermaid/er-group-edge-cohort.json` (already prototyped
   in the shelved D295 work) regenerated by a `--check`-guarded generator;
   membership and source shas pinned to the immutable manifest.
3. A self-testing `tools/mermaid/verify_er_groupedge_admission.py` mirroring
   the ER107 verifier's fail-closed discipline, with its own reviewed helper
   sha set (left empty until independent review, exactly as ER107 does).
4. Matrix wiring: in the non-107 `er-diagram` branch, look up the group-edge
   proof and pass it through the new parameter; assert the two cohorts are
   disjoint (a path can resolve in at most one — mechanism in §4.3).
5. Re-apply the two ER-local `parse_plain_er` admissions (held in
   `stash@{0}`); they are the renderer-side prerequisite D301 validated and
   do not alter the 107 (emitter 21/21 byte-exact). The new optional parameter
   is appended AFTER the existing defaulted parameters of
   `run_official_er206_case` (definition keeps no defaults; only the
   forward declaration at ~5053 carries them), so all three call sites compile
   unchanged.

### 4.1 Co-located SourceProfile test change (MAJOR-1) — bounded, never silent

Re-applying the parser hunks necessarily changes
`MermaidErSubgraphSourceProfile.CompleteOwnershipTitlesTypedMembersAndRecovery`:
the two admitted-form literals (`Group_business_endpoint`, `local_direction`)
move out of the unsupported bucket, so the terminal expectations change
`unsupported_observed 7→5` and `geometry 0→2`. This is part of this milestone
and must be explicit, not smuggled in.

The literal table (`kErSubgraphLiterals`, 16 entries) is 5
`mandatory_complete_positive`, 4 `malformed_source`
(`missing_end`, `extra_end`, `header_missing_separator`, `unfinished_title`),
and 7 `valid_unimplemented_source_obligation`. After the two admitted forms
move, the **exactly five** literals that must stay on terminal failures are:

1. `duplicate_group_ID`
2. `kind_collision`
3. `in_group_styles` (`style A fill:red`)
4. `inline_class_association` (`A:::red`)
5. `in_group_accessibility`

A test assertion pins those five as still terminal. There is no `classDef`
literal in this table, and the four `malformed_source` literals are out of
scope for this change and keep their existing terminal behaviour
(`malformed_closed == 4` unchanged). The independent second counter in
`MermaidErGroupedPresentation` (`EXPECT_EQ(unsupported_observed, 9)`,
~line 31090, separate `kErPresentationLiterals`) is unaffected by the parser
admission and must stay 9; the change explicitly pins that it does not move
(NIT-1).

The `class35_errors(..., terminal_failures=!geometry_classified)` downgrade
used in the shelved patch is accepted only as a **time-boxed transition
tracked as work item RFC0033-T1** (NIT-2): reclassify the two literals to
`mandatory_complete_positive` with complete Models — the correct path per
NO-GO §8 — so they return to normal terminal geometry/mutation validation.
The RFC cannot close while the report-only state is permanent; RFC0033-T1 is
the explicit exit anchor.

### 4.2 Rigor parity — dual-track Model authority, not a self-referential one (MAJOR-2, MINOR-1)

A sha over the production parser's own output is only self-consistency, not
authority. ER107 avoids self-reference with two independent legs: a parallel
`source_model`/`source_witnesses`/`source_binding` whose witnesses bind
byte-for-byte to the raw source (the D251/D271/D275 seams), plus a
`decode_semantics` field-by-field cross-check of the production
`whole_scene_model` (entity count, relation count, groups, direction,
id/label/owner, headers, attributes — `mermaid_er107_authority.hpp:994-1039`)
and the full sha chain in `bind_case`. The new cohort MUST reproduce both legs:

- **Independent source leg:** each of the six proposals carries its own
  `source_model`, `source_witnesses` (each witness's byte span re-points into
  the immutable raw bytes) and `source_binding`, produced through the existing
  Stage A test-only generator — these already exist as a capability, they are
  newly *curated and sha-pinned* for the six, not invented. The verifier must
  additionally run the **live** witness checks (not merely pin component
  shas), reproducing `verify_er107_admission.py:150-157` for every witness:
  bounded `0 <= start <= end <= len(raw)`, `raw[start:end] ==
  source_lexeme`, `sha256(raw[start:end]) == source_slice_sha256`, and the
  start/end line recomputation — so no "hash matches but the span lands in the
  wrong bytes" shortcut is possible (NIT-3).
- **Cross-check leg:** the cohort's decode equivalent runs a field-by-field
  cross-validation of the production `whole_scene_model` against the
  source_model at the same granularity as authority.hpp:994-1039. The
  verifier pins these component shas and the per-field counts in its closed
  schema; absent either leg the proof cannot be `owned`.
- **Per-case mutation identity (MINOR-1):** beyond the ER107 numeric floors
  (semantic > 0, physical ≥ 7), the verifier pins each of the six
  `mutations_executed` identity sets and the aggregate census, and requires
  the cohort-defining controls to be present — `group_caption`,
  `group_parent`, and the cardinality/label/endpoint mutations of the
  group-addressed edge (the mutation core already contains these kinds,
  `mermaid_relations_oracle.hpp:3403-3413,3494-3501`). The skipped-mutation
  set is also closed/pinned. Feasibility signal: the oracle identity model
  already admits group ids as relation endpoints (relations_oracle.hpp
  ~2407-2412), so "Unknown relation endpoint" will not fire spuriously.

### 4.3 Cohort disjointness mechanism (MINOR-2) and shared-helper boundary (MINOR-4)

- The new support header contains a `constexpr` (or static-init) assertion that
  its six-path set and the ER107 `kPins` path set are disjoint, failing to
  compile/start on any overlap. The matrix routing `if/continue` already makes
  a path reach only one branch; the assertion makes the invariant structural.
- The verifier independently recomputes BOTH prior selection predicates
  (`{active_config}` and the group-collision set) and asserts their union is
  107 + 6 with empty intersection.
- The new cohort may only `#include` the ER107 sha-pinned review helpers
  (including `mermaid_relations_oracle.hpp`, one of the four
  `verify_er107_admission.py` REVIEWED helpers). Editing any of those files is
  an explicit §6 stop trigger; reuse is read-only.

### Why (b) is data-preferred

- Zero touch to the ER206 prior, the ER107 `array<Pin,107>`, the three
  proposals, the selection predicate, and `verify_er107_admission.py` — the
  107 baseline stays byte-identical by construction (new files + one
  defaulted parameter + one additive branch).
- Disjoint cohorts, each sha-pinned and independently reviewed, so no existing
  authority is weakened.
- Reversible: deleting the new files and the additive branch returns the six
  to `source_fallback` with no migration of frozen data.
- Expected ledger effect, to be confirmed by the single closing matrix run:
  six rows move `source_fallback → drawn`, so drawn 1232→**1238**, fallback
  913→907, error stays **0**, and `mandatory_gaps` stays **48** (the six were
  never error rows and become clean draws; the gap roster and its pinned
  size/sha need no edit).

## 5. Reversibility and review visibility

- Every new pin/ledger/proposal artifact carries an explicit sha and is gated
  by a verifier whose `REVIEWED_HELPER_SHA` map is deliberately empty until an
  independent review fills it (the ER107 pattern), so the gate fails closed
  pre-review.
- The gap roster (`tools/mermaid/e6c-mandatory-gap-roster.json`,
  size+sha in `mermaid_gap_roster.hpp`) is expected to be untouched; if the
  closing run shows otherwise, that is a stop signal, not an excuse to edit it
  without review.
- Removal is purely subtractive and needs no data migration.

### 5.1 Two metric/disclosure facts (MINOR-3)

- **Formal coverage does not move.** The six travel the whole-null pending
  supplemental-proof route; `original_drawing_coverage.drawn_at_240`
  deliberately skips ER206/ER107 pending rows and only counts
  `ers.admitted`, which stays at its existing value. So the *formal*
  `drawn_at_240` stays **1053** even though the observed-wide drawn count moves
  1232→1238. Both numbers must be reported, not conflated.
- **The ER107 verifier is dormant, not a live-matrix guard.**
  `MATRIX_API_CONFIRMED = False` in `verify_er107_admission.py` makes the real
  `matrix_check` unreachable; only its `--self-test` (synthetic data) runs in
  CTest, and its pinned 688/`RETAINED_ROWS_SHA` values already differ from the
  live 1232 ledger. Candidate (b) breaks nothing today, but a future revival of
  the ER107 join will have six extra rows to reconcile. This RFC records that
  debt rather than implying the verifier continuously guards the live matrix.

## 6. Explicit-exclusion fallback (the stop outcome)

If independent review or the pre-gate spike finds any of the following, no
proof slot is built and the six are instead recorded at RFC level as
**permanently excluded from whole-source admission**, with their reason
(whole-null group-collision debt with no authority-grade Model path); the
parser admission stays shelved and the ledger documents them as retained
`source_fallback`. This is a legitimate terminal state, not a failure.

Stop triggers:

1. candidate (b), even with the §4.2 dual-track authority, would admit
   unreviewed render products or cannot match ER107's full invariant set;
2. meeting ER107-strength rigor for the six forces a change to the sha-frozen
   ER206 prior or the ER107 array/107/proposal structure (the D-306 condition);
3. **the §7.0 full-frame oracle spike fails** for any of the six — proving the
   geometry is not in fact acceptable to the complete-scene oracle (MAJOR-3),
   however well it passes the lightweight predicate;
4. **the implementation would have to edit an ER107 sha-pinned review helper**
   (e.g. `mermaid_relations_oracle.hpp`) rather than only include/reuse it
   (MINOR-4).

## 7. Acceptance and verification

### 7.0 Pre-gate spike before any frozen asset (MAJOR-3)

Before generating any proposal/pin/verifier artifact, do a throwaway or
early-gated spike with the parser hunks applied: run the ER107-strength full
scene oracle (`check_complete_frame` → `validate_impl`, the same validator
`exercise` invokes per mutation) over all six, not just `following_row`. This
is the single largest technical unknown — D301 only exercised the lightweight
predicate. The six must pass the complete-frame geometry/contact checks
(entity outlines, group outlines, unique assignment, relation label bridges)
on the normalized Grids. If any fails, stop under §6 trigger 3; do not build
authority assets that cannot reach `ready`.

### 7.1 Gates

- One full OfficialMatrix run, only at closure: expect observed drawn
  1232→**1238**, fallback 913→**907**, error **0**, `mandatory_gaps` **48**;
  formal `original_drawing_coverage.drawn_at_240` stays **1053** (§5.1). Any
  deviation needs a measured explanation, never a silent re-baseline, and the
  gap roster sha must remain unchanged.
- ER107: all 107 cases per-case byte-identical — the existing seam
  differentials and `verify_er107_admission.py --self-test` stay green; the
  two `array<Pin,107>` tables, three proposals and ER206 prior show zero diff.
- The new cohort verifier self-test is green and its `--check` ledger is
  stable; the verifier recomputes both prior predicates and the disjointness
  `constexpr` holds. Every new proof satisfies the §4.2 dual-track authority
  (independent source leg + field cross-check) and the §4.2 per-case mutation
  identity/census pins — a verifier that only checks geometry is explicitly
  insufficient.
- §4.1: exactly the two admitted-form literals move to the bounded
  report-only state, with the other five still terminal; reclassification
  exit criteria recorded.
- One clean rebuild confirming baseline binaries (per the efficiency doc's
  stale-object rule, verify via `--gtest_list_tests`, not just a green run).
- Targeted loops during iteration; no full matrix before closure.
- Implementation step zero brings the shelved D295 ledger/generator/gate out
  of `stash@{0}`/`/tmp/d295_shelved` into the tree (GC safety) before building
  on them; capture count language is the ER107 **15 captures**
  (3 cache modes × 5 widths), not "five frames". The char-difference cohort
  gate `test_mermaid_er_group_edge_cohort.cpp` is brought in **only as a
  non-authoritative regression smoke** (NO-GO §3 already classifies it as
  unfit for drawn authority): it must never be referenced by
  `run_official_er206_case` or any matrix proof path, and a header comment plus
  the verifier state that the new owned cohort is the sole matrix authority
  for these paths (NIT-4).
- The independent review chain (r1/r2/r3, APPROVE) is recorded in the RFC
  front-matter; this r3 approval, with the §4.2 dual-track authority and §7.0
  pre-gate, is the gate that permits implementation (NIT-5).
