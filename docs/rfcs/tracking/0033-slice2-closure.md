# RFC 0033 — Slice 2 closure evidence

Status: **CLOSED — all six phases landed and measured.** This document is the
durable, in-repository record required by slice-2 implementation spec §4.2
(`docs/rfcs/tracking/0033-slice2-implementation.md`). It captures the evidence that
previously existed only in the implementing session, so the result remains
auditable after that session is gone.

Document kind: a numbered companion record of RFC 0033, not a standalone RFC
proposal — it deliberately shares the `0033-` filename prefix with the parent
RFC and the implementation spec and follows the same no-frontmatter support-
document convention as `tracking/0033-slice2-implementation.md`. The repository
`rfc_lint.py` assumes one file per RFC number and therefore reports a
duplicate-number finding for this multi-document RFC; that finding pre-existed
between the parent RFC and the implementation spec and is not a content
defect.

- Closure HEAD: `ee417aac` (`test(mermaid): graduate two ER group-edge
  literals to complete positives (RFC0033-T1)`).
- Branch state at closure: 77 commits ahead of `origin/master`, all local,
  **not pushed** (per instruction).
- Build presets: Homebrew LLVM 23.1.2, `debug` and `release`, both built with
  `LD_LIBRARY_PATH=/home/linuxbrew/.linuxbrew/opt/openssl@3/lib:/home/linuxbrew/.linuxbrew/opt/curl/lib`
  (required on this box; without it test binaries fail at load with
  `libssl.so.3: cannot open shared object`, result 127).
- Full-matrix artifact: `mermaid-official-matrix.json`, 8,158,709 bytes,
  sha256 `31549cf830661d02a154dbad670723ef5f36aa9919e097d15fe5958b18858ab`,
  byte-identical across the debug and release presets.

## 1. Pre-registered baseline versus measured closure (spec §4.1)

The target column is the value pre-registered in the approved spec before
implementation; it is not a post-hoc re-baseline. The observed column is the
measured full-OfficialMatrix result at closure on both presets.

| point | drawn (wide_240) | source_fallback | error | mandatory_gaps |
|---|---|---|---|---|
| pre-admission green baseline (D298, parser shelved) | 1232 | 913 | 0 | 48 |
| slice-1 HEAD (parser admitted, proof absent) — RED by construction | 1232 | 907 | 6 | 54 |
| closure target (spec §4.1, pre-registered) | 1238 | 907 | 0 | 48 |
| **closure observed (debug, measured)** | **1238** | **907** | **0** | **48** |
| **closure observed (release, measured)** | **1238** | **907** | **0** | **48** |

Additional measured aggregates (identical on both presets):

- `original` track, `narrow_40`: drawn 634, source_fallback 1511, error 0.
- `original` track, `restored_240`: drawn 1238, source_fallback 907, error 0.
- Formal `original_drawing_coverage.drawn_at_240` = **1053** (unchanged;
  ER206/ER107 pending rows earn no formal credit).
- `ER107_native_whole_source`: selected 107, local_ready 107.
- `ER_native_whole_source`: failed_rows 0, complete Models decoded 68,
  whole-null pending 138, mandatory positive source fallbacks 0,
  resource errors 0.

Every measured value matches the pre-registered target exactly. The fallback
count 907 is the intended reduction from the pre-admission 913; it is also the
value locked by the D-325 fallback registry.

## 2. Per-case disposition of the six owned-proof cohort

Read live from the closure `mermaid-official-matrix.json`
(`track == "original"`). All six keep `expected = pending` (formally
unadmitted) while being locally drawn through the supplemental owned-proof
slot; every `errors` array is empty.

| fixture path | wide_240 | narrow_40 | restored_240 | errors |
|---|---|---|---|---|
| er-diagram/nightly-check/should-render-relationships-between-subgraphs.mmd | drawn | drawn | drawn | [] |
| er-diagram/nightly-check/should-render-subgraphs-with-direction-override.mmd | drawn | source_fallback | drawn | [] |
| er-diagram/nightly-check/should-render-subgraphs-with-quoted-ids.mmd | drawn | drawn | drawn | [] |
| er-diagram/pr-check/should-render-relationships-between-subgraph-and-entity.mmd | drawn | drawn | drawn | [] |
| er-diagram/visual-check/nested-er-subgraphs-with-direction-override-default.mmd | drawn | source_fallback | drawn | [] |
| er-diagram/visual-check/nested-er-subgraphs-with-direction-override-elk-layout.mmd | drawn | source_fallback | drawn | [] |

Tally: wide_240 drawn 6/6; narrow_40 drawn 3 (exact-ordinary) and
source_fallback 3 (full-geometry); restored_240 drawn 6/6; zero error rows.
This matches the pinned narrow disposition (3 exact-ordinary / 3
full-geometry) recorded at `dd3e1b47`.

## 3. Ten protected-file sha256/byte loop — actual run output

Command: a shell loop over the ten §0 paths comparing `sha256sum` and
`wc -c` against the spec §0 table. Re-run at closure after the phase-5 commit;
verbatim output:

```text
OK   139324  3aeb3312a5d57e74  tests/support/mermaid_er107_authority.hpp
OK   116500  1593280ffaf27f9b  tests/support/mermaid_er107_source.hpp
OK   30465  93904c1ad361940d  tests/support/mermaid_er107_official.hpp
OK 3743357  5a1c642cc7a3e2cf  tests/fixtures/mermaid_native/er107_complete_source_proposal.v1.json
OK 2945119  f8f7dedc651112ac  tests/fixtures/mermaid_native/er107_terminal_projection_proposal.v1.json
OK   72136  8f35938eb74591a4  tests/fixtures/mermaid_native/er107_presentation_source_audit.v1.json
OK  622036  76d54ecbc725e596  tests/fixtures/mermaid_native/er206_native_whole_source_authority.config62.v1.json
OK   44740  1fe297da6adb4630  tools/mermaid/e6c-mandatory-gap-roster.json
OK  169038  ce9d3fd7d54eab66  tests/support/mermaid_relations_oracle.hpp
OK   11591  3c0135fd91ce21d6  tests/support/mermaid_gap_roster.hpp
----
ALL TEN PROTECTED FILES MATCH §0
```

Loop exit code: `0`. No drift, so no §5.1 stop was triggered and no rollback
was performed. The full sha256 values equal the §0 table and the constants
pinned in the verifiers and `mermaid_gap_roster.hpp`.

## 4. Self-tests, drift gates and registered ctest summary

### 4.1 Python gates (re-run at closure, all exit 0)

- `python3 -I tools/mermaid/verify_er_groupedge_admission.py --self-test`
  → `Ran 8 tests ... OK`, exit `0`.
- `python3 -I tools/mermaid/fallback_attribution.py --self-test` →
  `fallback roster self-test OK: 907 fallback rows, 44 categories, causality
  labels on 44/44 categories, zero unclassified, classification reproducible;
  proven sufficient blockers = ['click_interactive_callback',
  'linkstyle_directive']`, exit `0`.
- `python3 -I tools/mermaid/verify_er107_admission.py --self-test` →
  `Ran 15 tests ... OK`, exit `0`.
- Read-only cohort check
  `python3 -I tools/mermaid/verify_er_groupedge_admission.py` →
  `Read-only verified ER group-edge cohort: six owned proofs, four-part
  cohort assertion, ten protected files byte-identical; both helper shas
  match the independent review receipt.`, exit `0`.
- Live fallback drift gate
  `python3 -I tools/mermaid/fallback_attribution.py --matrix
  build/debug/tests/ui/mermaid-official-matrix.json` →
  `fallback drift check OK: 907 fallback rows match the roster; no
  unregistered or reclassified fallback`, exit `0`.

### 4.2 Registered CTest run

Command (from `build/debug`, with the OpenSSL/curl library path):
`ctest -R "GroupEdge|FallbackAttribution|Er107" --output-on-failure`.

Result: **100% tests passed, 60/60**; label time summary `mermaid = 259.14
sec*proc (60 tests)`; total real time 259.34 s. Anchor registrations among the
60: `MermaidEr107AdmissionJoin.RejectsUnreviewedIncompleteOrBorrowedVerdicts`,
`MermaidErGroupEdgeAdmission.RejectsUnreviewedOrDriftedCohort`,
`MermaidFallbackAttribution.RosterIsReproducibleAndTotal`, all Passed.

These binaries were built from the exact source content that became
`ee417aac`; no `src/` or `CMakeLists.txt` change occurred between that run and
the commit, so the result covers the committed tree. The full dual-preset
OfficialMatrix (`MermaidCapabilityReport.OfficialMatrix`) passed standalone on
both presets (debug 242.9 s, release 27.4 s) and produced the byte-identical
matrix sha in the header.

### 4.3 Full relations semantics binary

`test_mermaid_relations_semantics`: **270/270 passed on both presets**
(debug 680.2 s, release 49.5 s), zero failures. This binary contains the
phase-5 RFC0033-T1 reclassification and the §7.0 full-frame oracle test.

## 5. Independent adversarial review receipt

A fresh-context, strictly read-only adversarial review
(agent `a6b8480885053f362`) independently re-hashed all four digests, derived
both mutation censuses from the oracle source, traced each embedded Model
field to the production parser/encoder, and verified the protected surface.

Verdict: **APPROVE**, no BLOCKER / MAJOR / MINOR. Two non-blocking NITs:

1. No direct `er_whole_scene_json_for_testing` seam-equality assertion for the
   two reclassified literals. **Disposition: accepted, not actioned.** This is
   the same architecture as the five pre-existing positive literals, and the
   RFC dual-leg authority is explicitly scoped to the six-fixture cohort. The
   embedded Models were nonetheless generated from the production seam and are
   guarded by render + validate + exact-census checks. Recorded as an optional
   future test-only enhancement, out of RFC0033-T1 scope.
2. Report metadata counters are hardcoded rather than computed from the
   table. **Disposition: accepted, not actioned.** This is the pre-existing
   pattern across all sibling subgraph tests; the values are arithmetically
   consistent (7 positive + 5 obligations + 4 malformed = 16) and changing it
   is an unrelated refactor.

The reviewer's single residual (read-only, could not compile) is covered by
the dual-preset execution evidence above: 270/270 semantics on both presets
and a byte-identical cross-preset matrix.

## 6. Closure `git diff --stat` whitelist conformance

RFC0033 slice-2 owned paths over `426fcb58..ee417aac` (slice-1 baseline through
phase 5). Every changed path is a new authority/test file or an additive edit
on the spec whitelist; there is no `src/` change and no protected-file change.

```text
 docs/rfcs/tracking/0033-slice2-implementation.md            |  291 +
 tests/fixtures/mermaid_native/er_groupedge_complete_proposal.v1.json | 6834 +++
 tests/support/mermaid_er_groupedge_official.hpp    |  476 ++
 tests/support/mermaid_er_groupedge_source.hpp      |  347 ++
 tests/ui/CMakeLists.txt                            |   47 +
 tests/ui/test_mermaid_er_group_edge_cohort.cpp     |  233 +
 tests/ui/test_mermaid_er_groupedge_owned.cpp       |  165 +
 tests/ui/test_mermaid_er_groupedge_proposal.cpp    |  532 ++
 tests/ui/test_mermaid_official.cpp                 |  188 +-
 tests/ui/test_mermaid_relations_semantics.cpp      |  101 +-
 tools/mermaid/er-group-edge-cohort.json            |  242 +
 tools/mermaid/generate_er_group_edge_cohort.py     |  306 +
 tools/mermaid/verify_er_groupedge_admission.py     |  503 ++
 13 files changed, 10223 insertions(+), 42 deletions(-)
```

Whitelist mapping: new proposal artifact; two support headers; verifier plus
the relabeled non-authoritative smoke and its D295 ledger/generator; the
proposal/owned test drivers; the one additive `test_mermaid_official.cpp`
matrix-wiring edit; the slice-1/phase-5 `test_mermaid_relations_semantics.cpp`
edit; and the mandatory additive `tests/ui/CMakeLists.txt` registrations.

A path scan of the same range for `^src/` and every §0 protected name returns
**NONE**. Three other files appear in the commit range but belong to a
distinct, separately-reviewed workstream (TechLead D-325/D-327/D-329/D-333
fallback registry), not to RFC 0033:
`docs/dev/mermaid-d329-fallback-terminal-state.md`,
`tools/mermaid/fallback_attribution.py`, `tools/mermaid/fallback_roster.json`.

## 7. Uncovered area: no ASan re-run for phase 5

The phase-5 change is test-only and data-shaped: `constexpr` mutation
kind-count tables, two literal initializers embedding canonical Model JSON and
sha256 strings, integer counter literals, and comment updates. It introduces
no allocation, ownership, pointer, lifetime, or production code, and no
`src/` file is touched. The code paths it exercises — Grid copies,
`er206_mutations`, `validate_impl`, outline and rejection loops — are fully
covered by the 270/270 semantics run on both debug and release. The owned
cohort gate itself was already ASan-clean in the prior session
(`dd3e1b47`/`ef37b602`). A full ASan sweep of the mermaid label remains
time-infeasible on this box and is documented as such in
`docs/dev/engineering-efficiency.md`. Residual memory-safety risk for this
diff is assessed as negligible; no ASan defect is claimed.

## 8. Slice 3 / slice 4 start-up assessment (not started)

### 8.1 Verified fact: no slice 3 or slice 4 is defined anywhere

The implementation spec is titled "Slice 2/3/4" and its body once says
"slices 1–4", but the approved content is a single six-phase implementation
that fully answers the RFC's one scoped question, and all six phases are now
complete. A repository-wide search finds no definition of separate slice 3 or
slice 4 goals, boundaries, prerequisites, or documents:

- Only two RFC 0033 documents exist (this closure doc excepted): the parent
  RFC and the slice-2 implementation spec. Neither decomposes work into a
  third or fourth slice.
- The parent RFC uses the word "slice" only for witness byte spans
  (`source_slice_sha256`), never for work packaging. Its scope is one
  question — how the six whole-null group-edge originals obtain an
  authority-owned proof — which is now answered and measured.
- `git grep` for `slice 3` / `slice 4` / `slice-3` / `slice-4` across the
  whole tracked tree returns only unrelated Rust fixture filenames
  (`*.slice3.rs`). No commit, TechLead tracking doc, or source file defines
  them.

Consequently there is currently nothing that can be "started" as slice 3 or
slice 4: they have no target, no prerequisite list, and no spec.

### 8.2 Candidate follow-up work actually evidenced in the RFC

Three items are mentioned in the approved materials. None is a defined slice;
each needs its own decision.

| candidate | source / current state | prerequisite | effort | spec needed |
|---|---|---|---|---|
| Revive ordinal 198 (handDrawn nested-direction, Work-ceiling row) | RFC §1.3/§4.1; pinned out of the cohort by reviewed path+sha `c4891928…` because the envelope double-parse exhausts the ER Work budget | Slice-2 owned-proof pattern landed (done); needs new Work-budget/envelope capability and coordination near the frozen ER206 prior | Large, multi-session | Yes — new RFC; touches a protected boundary |
| Reactivate the dormant ER107 live matrix join (`MATRIX_API_CONFIRMED=False`) | RFC §5.1 recorded debt; a revived join must reconcile six extra rows and the retained 688 ledger against the live 1238 census | Slice-2 cohort landed (done); verifier contract and ledger rework | Medium–large | Yes — changes a verification contract |
| Close the two review NITs (16-literal direct seam-equality test; computed report counters) | Section 5 NIT-1/NIT-2 | None beyond current tree | Small (hours, one test file) | No RFC; ordinary test maintenance, separate commits |

The two larger items depend on the slice-2 mechanism as an established pattern
but are neither enabled nor scheduled by it. Per the repository's RFC-driven
development rule, feature/contract work of that kind must not begin
implementation before an RFC passes agent-run review.

### 8.3 Recommendation

Do not open "slice 3/4" directly — there is no defined scope to implement.
Treat RFC 0033 as functionally complete once this document lands. If the
ordinal-198 revival or the ER107 join reactivation is wanted, author a new
RFC (with its own motivation, protected-area analysis and review gates) first.
The two NITs may be handled as routine, separately-committed test maintenance
without an RFC.
