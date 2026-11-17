# RFC 0033 — Slice 2/3/4 implementation spec (owned-proof cohort)

Status: **APPROVE-TO-IMPLEMENT (independent pre-implementation review
agent a62fba492df260b37, r2 2026-11-17).** r1 returned REQUEST-CHANGES (4
MAJOR/6 MINOR, all spec-text defects, no architectural change); every finding
closed and independently re-evidenced in r2, verdict APPROVE-TO-IMPLEMENT with
no remaining BLOCKER/MAJOR/MINOR. Reviewer's six load-bearing handoff
conditions are reproduced in §6. Parent:
`docs/rfcs/0033-er-group-edge-owned-proof.md` (approved r3). Slice 1 (parser
admission + §7.0 oracle gate) is committed at `426fcb58`. This spec turns the
approved candidate (b) into concrete, reviewable changes. Hard rule inherited
from the RFC: ER107's `array<Pin,107>`, its three proposals, the ER206 prior
and the gap roster are byte-immutable; all new authority lives in new files
reached only through an additive, defaulted-parameter path.

## 0. Byte baseline (must be reproducibly unchanged at closure)

Captured at HEAD `426fcb58`. The closure verifier recomputes every value and
asserts equality; any drift is a stop, not an edit.

| protected file | sha256 (before) | bytes (before) |
|---|---|---|
| tests/support/mermaid_er107_authority.hpp | 3aeb3312a5d57e740960930ba740c4e4aa2824a529bd7e8d51c247211d5610c2 | 139324 |
| tests/support/mermaid_er107_source.hpp | 1593280ffaf27f9b9115a898557734693f58b181b81c64067ff9c399ed3cae46 | 116500 |
| tests/support/mermaid_er107_official.hpp | 93904c1ad361940d803958b4adc7f2fced74f948b8cdc5bc887859f81f706473 | 30465 |
| …/mermaid_native/er107_complete_source_proposal.v1.json | 5a1c642cc7a3e2cf08e00355be845711520f762fc999dc13fe7dbb4a417297f3 | 3743357 |
| …/mermaid_native/er107_terminal_projection_proposal.v1.json | f8f7dedc651112ac553ea86d200f2562ffb05d54b2b09df7d6477a687a8de7c9 | 2945119 |
| …/mermaid_native/er107_presentation_source_audit.v1.json | 8f35938eb74591a428823aae3c1279c4a333fa8240b7d92181e77e7f22ed18ba | 72136 |
| …/mermaid_native/er206_native_whole_source_authority.config62.v1.json | 76d54ecbc725e59645501f7079f9387b272172d584703f0ea72f143c83c469db | 622036 |
| tools/mermaid/e6c-mandatory-gap-roster.json | 1fe297da6adb4630a015bf33c9730fb0d9d4e6e6e09dd9df8457d4fcf38d1127 | 44740 |
| tests/support/mermaid_relations_oracle.hpp (ER107 reviewed helper; include/reuse only) | ce9d3fd7d54eab66b469d630b77f0f5b7acf9f72524124c758c576fc7808c3cd | 169038 |
| tests/support/mermaid_gap_roster.hpp (hosts kRosterBytes/kRosterSha256) | 3c0135fd91ce21d6a961d10c695123ac7e1c9b89477572d9941703c52ab1a0b0 | 11591 |

These equal the constants already pinned in `verify_er107_admission.py`
(SOURCE/TERMINAL/PRESENTATION/PRIOR sha) and `mermaid_gap_roster.hpp`
(`kRosterBytes=44740`, `kRosterSha256=1fe297…`), and
`SOURCE_HELPER_SHA=1593280f…` for `mermaid_er107_source.hpp`. The closure
check is a shell loop over the same **ten** paths comparing `sha256sum` and
`wc -c` to this table — a mechanical, rerunnable proof, not a narrative claim.
Line numbers cited anywhere in this spec are approximate and may drift; the
named symbols/functions are authoritative (MINOR-6).

## 1. New files only (no edit to any file in §0)

### 1.1 Proposal artifact (new, 6 cases)
`tests/fixtures/mermaid_native/er_groupedge_complete_proposal.v1.json`

One document with the six cases in the prior-ordinal order 26, 27, 29, 39,
196, 197. Each case is produced by the existing Stage A test-only seams over
the immutable fixture bytes (the emitter contract already drives all of these
for arbitrary `.mmd`):

- `whole_scene_model` ← `er_whole_scene_json_for_testing` (production parse
  output; the leg that the cross-check validates);
- `source_model`, `source_witnesses`, `source_binding` ← the D271/D251/D275
  seams via `er_src_collect_all` (the INDEPENDENT leg — zero production-parser
  calls, per the r2 review verification);
- identity fields ← manifest `source_sha256`, `manifest_global_ordinal`, and
  the read-only `prior_ER206_ordinal`; no ER206 prior edit.

Generator: a **record-mode C++ test** (the only viable path — the
`generate_*_cohort.py` family is pure Python with zero subprocess and cannot
call the C++ seams). All six seams are already exposed in
`src/ui/visual/mermaid_testing_seams.cpp` and already driven over these exact
six fixtures by the slice-1 durable test
(`Rfc0033GroupEdgeFullFrameOracleAcceptsSix`), so no new production capability
is needed. The record test emits the canonical proposal JSON; a checked-in
pure-Python `--check` verifier then re-derives and compares every embedded
sha/field count. Output is canonical, byte-stable JSON. The six fixture shas
already match the manifest (verified in D295).

### 1.2 Support headers (new)
- `tests/support/mermaid_er_groupedge_source.hpp` —
  `inline constexpr std::array<Pin,6> kPins` (the Pin mirrors the ER107 shape:
  path, global, case/model/scene/binding/complete shas) plus the
  renderer-free `select(path, raw_sha)` and the source-leg helpers, reusing
  (never editing) `mermaid_relations_oracle.hpp`.
- `tests/support/mermaid_er_groupedge_official.hpp` — namespace
  `loom::test::mermaid_er_groupedge::official` with `Documents`, `run(index)`,
  and `owned(JsonVal)` structurally mirroring `mermaid_er107_official.hpp` but
  bounded to 6 and the new contract id
  `ERGroupEdge/complete-original/v1`. `owned()` enforces the RFC §4.2 closed
  set: identity pins; 15 captures (3 cache modes × 5 widths); captures-before-
  bind; plane equality; the live witness raw-byte/line checks; field-by-field
  decode cross-check (`decode_semantics`-equivalent at
  `mermaid_er107_authority.hpp:994-1039` granularity); per-case
  `mutations_executed` identity including `group_caption`, `group_parent` and
  the group-edge cardinality/label/endpoint controls; closed skips; Work caps;
  width/narrow XOR; explicit error path; ordinal range `< 6`.
- A `constexpr`/static-init assertion in the header that the six-path set is
  disjoint from every path in `mermaid_er107::authority::kPins` (RFC §4.3).

### 1.3 Verifier (new)
`tools/mermaid/verify_er_groupedge_admission.py`, modeled on
`verify_er107_admission.py` but standalone and **read-only plus `--self-test`
only** (no `--write` join flag; the ER107 matrix-join API is dormant via
`MATRIX_API_CONFIRMED=False`, so there is no write behavior to mirror).

- **Two-step cohort assertion (MAJOR-1).** There is no single text predicate
  that robustly yields six. Verified against the ER206 prior:
  - loose superset — whole-null rows whose debt list *contains*
    `raw_entity_group_kind_collision…` = **7** (prior ordinals 26, 27, 29, 39,
    196, 197, 198);
  - exact-set predicate `debts == {raw_entity_group_kind_collision…}` = **6**
    (the cohort above minus 198);
  - ordinal 198 (`…-handdrawn-look`) carries the **two-debt** set
    `{active_config, raw_entity_group_kind_collision}` and is the Work-ceiling
    row. Its exclusion cannot be text-decided (the D295 generator documents
    "budget fit is not textually decidable"), so the verifier must assert:
    (i) the loose 7-row superset; (ii) the compiled cohort pin table equals the
    exact-6 set; (iii) ordinal 198's exclusion is pinned by the reviewed
    `BLOCKED_CAPABILITY_SET` path+sha with the Work-ceiling reason; (iv) the
    exact-6 set and the ER107 `{active_config}` 107 set have empty
    intersection. A hand-rolled heuristic that "happens to return 6" without
    these four checks is rejected.
- recomputes every §0 sha/byte (now ten files, including the two reviewed
  helpers) and fails on drift;
- live witness checks reproducing `verify_er107_admission.py:150-157`
  (bounded span, `raw[a:b]==lexeme`, slice sha, line recompute);
- **field-count recomputation (MINOR-2):** independently derives
  entity/relation/group/direction counts from the independent `source_model`
  and pins them, so the field-by-field cross-check exists on the verifier side
  too, not only in the C++ `owned()`;
- pins per-case mutation identity sets and the aggregate census;
- `REVIEWED_HELPER_SHA` starts empty → authority gate fails closed until an
  independent review fills it;
- `--self-test` wired into `tests/ui/CMakeLists.txt` like the ER107 join test
  (this is the expected additive CMakeLists edit).

### 1.4 Non-authoritative smoke (relabeled D295 gate)
The char-difference `test_mermaid_er_group_edge_cohort.cpp` (in
`/tmp/rfc0033_wip/`, also in `stash@{0}`) enters the tree with an explicit
header comment: regression smoke ONLY, never referenced by
`run_official_er206_case` or any proof path; the new owned cohort is the sole
matrix authority (RFC §7.1 NIT-4).

## 2. Matrix wiring — one defaulted parameter, one additive branch, full in-function contract

Declaration near `test_mermaid_official.cpp:5053` gains a trailing defaulted
parameter AFTER the existing one (the definition at ~19032 keeps no defaults):

```cpp
std::size_t, JsonVal er107_local_proof = JsonVal{},
              JsonVal groupedge_local_proof = JsonVal{});
```

The three call sites (the ER107 matrix branch, the non-selected tail, and the
ER206 pin loop) compile unchanged. In the matrix loop, inside the
`er-diagram` branch, add a sibling block immediately after the existing
`if (auto selected = official_er107::select(...)) { … continue; }` block. A
`groupedge_selected` boolean array mirrors the existing `er107_selected` guard
and `require`s no cohort row is processed twice (MINOR-5).

### 2.1 In-function contract is mandatory, not implied (MAJOR-4)

Inside `run_official_er206_case`, the new proof parameter must reproduce the
ER107 supplemental branch's two independent safety layers verbatim — calling
`groupedge::owned(proof)` alone at the call site is explicitly insufficient:

1. **Live four-tuple identity re-binding** (the ER107 pattern at
   ~19138-19145): `require` that the proof's `path`, `source_sha256`,
   `prior_ER206_ordinal` and `manifest_global_ordinal` equal the row's
   re-read-from-disk values, so a stale or swapped proof cannot match.
2. **Live geometry re-validation** (~19146-19149): recompute
   `!wide && p.restored && a && b && c` on the just-captured frames and the
   width/narrow XOR against `proof.narrow_exact_ordinary`; only then set
   `row.status = {"drawn", narrow ? "source_fallback" : "drawn", "drawn"}`.
3. **Fail-closed:** a `!ready`/absent proof is passed as `JsonVal{}` (the
   existing ~10308 idiom); the function then takes its normal whole-null path
   and, because the parser now renders differently from the text control,
   books the explicit `er206_whole_null_unmet` error at ~19158-19160 — never a
   silent draw and never a silent fallback.

Routing is structurally exclusive: the ER107 block `continue`s on its hits and
the header's constexpr disjointness assertion guarantees no path matches both
cohorts. The non-selected tail is unchanged.

## 3. Phased local commits (each returns to dirty 0; no push)

1. Proposal generator + the 6-case proposal JSON; a self-check that re-derives
   every embedded sha.
2. The two support headers + disjointness assertion; unit-level `owned()`
   tests over the six (and a negative test that a tampered byte fails).
3. The verifier + CMake `--self-test`; empty helper-sha fails closed.
4. Matrix wiring + relabeled smoke; targeted gates green.
5. RFC0033-T1 reclassification of the two SourceProfile literals to
   `mandatory_complete_positive` with complete Models (removes the report-only
   transition), if the owned proofs supply those Models.
6. The single closing OfficialMatrix run.

## 4. Closing evidence (D-311 requirement 3) — measured, with the true red baseline stated

### 4.1 The real baseline is red while the authority is absent (MAJOR-2)

Slice 1 (`426fcb58`) already contains the two production parser admissions but
no owned-proof cohort. Its true full-matrix state — independently re-read from
the built matrix and forced by the hard assertions at
`test_mermaid_official.cpp` (`gap_diff.ok()` and `EXPECT_EQ(gap_rows, 48)`) —
is therefore:

| point | drawn | fallback | error | mandatory_gaps |
|---|---|---|---|---|
| pre-admission green baseline (D298 pre-matrix, parser shelved) | 1232 | 913 | 0 | 48 |
| **slice-1 HEAD (parser in, proof absent) — RED by construction** | 1232 | 907 | **6** | **54** |
| closure target (owned cohort wired, proofs ready) | **1238** | 907 | 0 | 48 |

The gap count is a hard `EXPECT_EQ(…,48)` plus the pinned roster
(size 44740 / sha 1fe297…), so the full OfficialMatrix is expected to FAIL
throughout slices 1–4; only targeted tests are green during those slices.
"Each slice returns to dirty 0" means **git working-tree clean**, never "full
matrix green". The full matrix is run exactly once, at closure.

### 4.2 The single closing run

Run the full OfficialMatrix once after wiring. Report MEASURED, not predicted:

- observed `wide_240` drawn N against the slice-1 red baseline (1232) and the
  green target (1238); fallback, error and `mandatory_gaps`; formal
  `original_drawing_coverage.drawn_at_240` (must stay 1053 — ER206 pending rows
  are skipped in coverage).
- Per-case disposition of all six (path → observed triple + errors). If
  N ≠ 1238, enumerate each non-drawn case with its exact failing
  check/error — that output feeds either a fix or the §5 exclusion decision,
  never a silent re-baseline.
- Re-run the §0 sha/byte loop (now ten files) and attach its output proving all
  protected files are unchanged. The closure `git diff --stat` whitelist is:
  new files (proposal, two headers, verifier), the one additive edit to
  `tests/ui/test_mermaid_official.cpp`, the slice-1 test file, **and the
  additive `tests/ui/CMakeLists.txt` edit** (new test target, verifier ctest,
  smoke registration — this edit is mandatory, not an oversight).
- Gap roster must return to 48 / 44740 bytes / sha 1fe297… at closure (the six
  become clean non-error draws and never enter the gap set). A post-wiring
  closing run that still yields 54 is a §5 stop signal (a proof failed closed),
  not a roster edit.

## 5. Stop conditions and explicit rollback (RFC §6)

### 5.1 Triggers

Any of: an unreviewed-artifact backdoor is found in this design; ER107 frozen
structure must change; the full-frame oracle (6/6 in slice 1) regresses for any
case; an ER107 sha-pinned helper would need editing; or the closing matrix
cannot reach 1238/0/48 without weakening a gate.

### 5.2 Rollback is required, not implied (MAJOR-3)

Because slice 1 already committed the parser hunks and the SourceProfile
5/2 report-only transition, a stop cannot leave the tree on the red
1232/907/6/54 matrix. On trigger, in the same change as the negative result:

1. Revert the two `parse_plain_er` admissions in
   `mermaid_relations_core_impl.cpp` (group endpoint back to
   `EntityShape::Group ? -1 : n`; in-group direction back to
   `if (groups && groups->depth) return {};`), restoring D298 green-parser
   behavior.
2. Revert the `MermaidErSubgraphSourceProfile` 7→5/0→2 transition back to
   `unsupported 7 / geometry 0` and remove the `class35_errors` report-only
   flag, while keeping the Stage A test-only seams (commit `916b9dd9`) and any
   purely additive, non-parser tests that still hold.
3. Do not wire the cohort; keep its files uncommitted or delete them.
4. Re-run targeted gates and, as the exclusion-terminal acceptance evidence,
   confirm the matrix returns to the green 1232/913/0/48 state.
5. Write the same-format negative result (symptom / why infeasible / exit
   criteria) recording the six as RFC-level permanently excluded retained
   `source_fallback`, so the repository never lands a permanently red gate.

## 6. Load-bearing handoff conditions (from the r2 APPROVE review)

The fresh-context implementer must honor these verbatim; they are the
conditions on which APPROVE-TO-IMPLEMENT was granted:

1. Implement the §1.3 four-part cohort assertion as written — no heuristic
   shortcut. Ordinal-198 exclusion is pinned by ledger path+sha
   (`c4891928…`, verified against disk and manifest) plus the Work-ceiling
   reason, not by an ad-hoc predicate.
2. Replicate the §2.1 two-layer in-function contract verbatim; a call-site
   `owned()` check alone is insufficient.
3. At closure run the §0 ten-file sha/byte loop; the `git diff --stat`
   whitelist includes the additive `tests/ui/CMakeLists.txt` edit.
4. The full matrix being red (54 gaps) during slices 1–4 is the expected
   state; never edit the roster or silently re-baseline. One full
   OfficialMatrix run, at closure only.
5. The new verifier's `REVIEWED_HELPER_SHA` stays empty (fail-closed) until an
   independent review fills it.
6. On any §5.1 stop trigger, execute the full §5.2 rollback; the repository
   must never be left on a permanently red full-matrix gate.

Residual (non-blocking) risk noted by the reviewer: `er_src_collect_all` has
not yet been executed on the six fixtures. If it returns nullopt it fails
early at the phase-1 self-check, which is correctly contained by the phase
ordering — phase 1 is therefore the true first implementation gate.
