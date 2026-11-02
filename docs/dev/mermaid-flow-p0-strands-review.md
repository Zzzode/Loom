# Flow P0 physical-strands batch verification

2026-10-30: the last two P0 originals now pass complete source-bound native
geometry. Coverage is **644/2145 (30.02%)**,1501 fallbacks, **zero errors**.
Flow is **64/400 (16.00%)**,336 fallbacks and zero errors. Each complete original
at240 columns earns1/1 only after native validation; fallback and error count as
failure. Body diagnostics remain outside2145. P0 is complete for the original
22-error implementation package; this does not imply Mermaid is complete.

## Source meaning and actual production defect

Nightly ordinal54/global715 (7213) is565 bytes, SHA
`a4889b253941d911e7d1b3dae02117f72473a12d10d91bc7febf8288b8c92f53`;
its complete Model is2824 bytes, SHA
`2ccbff0e6e3091db21f7b87b89521cd49c2ae80e8b714e693799706fa38f1f80`.
It has11 nodes,2 groups,14 headless edge occurrences, including five N11--N22
occurrences and isolated N21. Visual ordinal357/global1018 (issue4648) is529
bytes, SHA`403c54cf5f29cc7de0886f4ac363fd8c3229bc3d0d4539023f1a9a15a9a560a8`;
its complete Model is990 bytes, SHA
`6099cc34612fe0780b7466ffdcceac69b7a8b71b3382e514c38e278c7cbcd121`.
It has4 nodes,2 groups and3 edges (two point-arrow and one headless).
Root TD, Wrapper LR and inherited Inner LR remain explicit.

Both come from the unchanged independently reviewed P0 asset (64152 bytes,
SHA`46e19f214ffdf8b4dd5ba25bfcf41db64a6d8f7ba88fe50fe7d2fe65170b7283`).
All ten P0 source Models now have separate reviewed consumers; original fixture
and asset bytes are unchanged. Foreign browser/Dagre assertions remain debt.

The actual pre-fix7213 capture contains nonterminal T junctions at zero-based
(27,24),(28,24),(47,24),(48,24). Relaxed routing allowed subsequent paths to
join or turn on marked prior stems. The [retained actual capture](mermaid-flow7213-before-actual-capture.json)
and [diagnosis](mermaid-flow7213-route-diagnostic.json) are observed evidence,
never expected geometry. Counts alone could not prove five separate strands.

The producer now preserves distinct physical paths for repeated directed pairs
and reversed headless pairs. Opposite directed relations remain distinct.
Only straight two-arm cells allow perpendicular crossings; corners, consumed
crossings, arrowheads and group ports seal. Group ownership checks also inspect
actual routed bits and used ports. Temporary labeled half-path reservations
save/restore blocked and crossing state, including failed search, budget failure
and exception unwinding. Strict free-path reservations retain their old tariff.

Caption changes are restricted to labeled repeated pairs and their own rank
bands; compound hints retain original endpoint identity before projection.
Unrelated labels/ranks and the six historical body snapshots stay unchanged.
A legacy dense labeled graph initially hit the fixed Work limit; packed paid
endpoint analysis and reuse removed redundant caption scans. Saved bytes and
reserve/restore writes are paid before mutation. No canvas or Work cap increased.

## Private oracle and owned proof

A separate restricted test-private strand oracle discovers complete contours,
checks exact labels/interiors, direct membership and independent directions,
then traces deterministic reciprocal paths. Crossings continue straight; T
junctions, shared turns/ports, reused arms, wrong heads and unclaimed ink reject.
Headless endpoint keys are unordered; directed targets and complete occurrence
multiplicities are exact. Source order is established by complete Model/source
binding, never inferred from pixels. Nonmember entity frames cannot overlap.
The entire grid, including duplicate/after-footer text, is conserved.

Path2048 cells,14 strands,11 nodes,2 groups and240x1040 grid bounds are explicit.
Truncation, allocation and exhausted work are inconclusive/resource outcomes,
never negative credit. Complete contour discovery is charged to the same128M
scene ledger, including authenticated setup work; no fresh per-phase allowance
is introduced. Both native and official consumers publish all five captures
before binding/decode, then validate all drawn widths and restoration.

Each source has seven decisive mutations: label, membership, marker, omitted
occurrence, direction, extra ink and broken route. Private hand papers cover
orthogonal crossings, all directions, shared walls,14-strand/2048-cell/grid
boundaries and exact/one-short work. Real-source controls cut work before capture,
after source selection, after wide geometry and at the last mutation. Failures
clear all drawing/admission/mutation credit; the collector rejects contradictory
owned rows independently. Exact23-key proofs bind source, Model, measured setup,
physical outcomes/counts/reasons, widths and seven negatives. Missing, duplicate,
unknown, swapped, forged and resource-inconsistent proof fields all reject.

Review also corrected older Grouped7/GroupEndpoint15 negative handling: opaque
allocation failure remains resource across the inner assignment boundary;
unexpected exceptions remain inconclusive; accepted mutants never become
resource merely because their label contains that word. Cardinality and legacy
LinkProfile candidate bounds are resource. The final control explicitly asserts
`GroupEndpoint profile cardinality bound` using a bounded ordinary graph plus129
tagged edges, then exercises that actual diagnostic through the negative helper.

## Executed regression and exact comparison

Debug **867/867**,287.50s; Release **867/867**,
37.32s. After final review, the exact cardinality-branch
control was corrected, rebuilt and passed1/1 in each preset; production and
classifier code were unchanged from the full runs. Earlier focused34/34 passed.
RFC lint, privacy and both roster-report verifications pass. These are all
Mermaid-labeled tests, not full-project CTest, browser equivalence or LLVM line
coverage. No commit or RFC phase graduation is claimed.

Final matrices are byte-identical, SHA`b4d39048be0674dd0c7fb9179aa1027c0dec756fb91b82a84efb6d767a262e9e`.
Only the two named originals improve; the other2736 observation statuses are
unchanged. [Machine-readable comparison](mermaid-flow-p0-strands-matrix-comparison.json).
The reviewed roster removes exactly their two original rows:268 total,
official48; other gates and body rows unchanged.47989 bytes, SHA
`41d955e47250dfb9a01fbbf45853c42feb6258cdedc3cd6869cb8d2f202bae64`.

P1 remains518, P2 447 and P3 536. The next batch is the reviewed
[Class13 direct-style proposal](mermaid-class-direct-style-plan.md); its proposed
Models/design earn no drawing credit before implementation and validation.

## Independent review records

The available Codex CLI ran independent read-only agents. Review labels identify
the exact invocation artifacts; they are not claims of Claude model execution.

### producer review

**APPROVE**

I found no concrete issue in the reviewed production delta. The parser’s 64-vertex and 64-group limits keep the packed endpoint namespaces distinct, and the bit-32 marker separates normalized headless keys from ordered keys. Classification is funded before allocation and writes, and sort comparisons spend work before proceeding; exhaustion unwinds locally.

The path reservation restores saved blocked and crossing states in reverse order, including during exception unwinding. The authored endpoint check also happens before compound projection.

I did not independently run tests in this read-only review. The supplied 34-test log predates the final sort-unwind change.

### integration review

**APPROVE integration.** The corrected control reaches the intended GroupEndpoint profile cardinality branch and passes in both Debug and Release.

At [the corrected test](/Users/zzzode/Develop/Loom/tests/ui/test_mermaid_flow_group_endpoints.cpp:648), the original model remains bounded, the tagged profile is resized to 129, and the test asserts the single exact cardinality diagnostic before passing the same geometry to the shared negative helper. In [the classifier and helper](/Users/zzzode/Develop/Loom/tests/support/mermaid_flow_oracle.hpp:1192), removing the new cardinality match would leave that diagnostic unclassified; the helper would return normally, causing the `EXPECT_THROW` control to fail.

The updated Debug control passed 1/1, as did Release. The supplied final3 full-suite logs show 867/867 passing in each configuration before the test-only correction. No broader phase graduation is implied.

### oracle review

**APPROVE** for the restricted, test-private strand prototype. I found no remaining blocking issue in the reviewed accounting, geometry, or adversarial controls.

The earlier findings are addressed: unrelated rectangles have explicit strict disjointness checks; each scene is debited for the measured discovery work before capture; and both source models have an order-permutation control rejected by the immutable model binding. The shared-wall test establishes decisive rejection at discovery, though it does not independently exercise the new ownership guard.

The mutation helper requires a matching baseline and seven decisive mismatches; inconclusive or resource-limited results cannot complete the proof. The physical comparison remains a multiset check and does not claim to recover source occurrence order from pixels.

This was a read-only source review. I did not run the tests or independently verify the reported remaining-work figures: no built `test_mermaid_flow_group_endpoints` executable was present, and the environment is read-only. This approval does not authorize formal matrix admission.

### roster review

**APPROVE the exact two-row roster removal**, conditional on the pending producer and final integration reviews passing and the required Debug and Release regressions passing. This verdict does not approve a broader phase gate.

I verified that the baseline roster has the supplied SHA-256. The proposal removes only the original `flow_whole_null_unmet` rows for nighttime 7213 and visual issue 4648. The official gate falls from 50 to 48 rows, and the total from 270 to 268; body-diagnostic rows and all other gates are unchanged. The proposal also appends a review note.

The current Debug matrix has 644/2145 originals drawn at wide width, with 1501 fallbacks and zero errors; Flow admits 64/400. Against the prior matrix, exactly the two named original cases improve from error to drawn at wide and restored widths. Their narrow status and all body-diagnostic observed statuses remain unchanged. The matrix’s mandatory-gap rows match the proposed official gate.

The focused log records 24 passing tests out of 25. The sole failure reports the expected two roster-only gaps and the stale 50-row count pin. The separate physical-strands oracle review approved its restricted test-private scope, and the passing `OfficialFlowStrands` test checks seven rejected mutations per source along with resource and forged-proof controls. The oracle review itself did not authorize formal matrix admission, which remains subject to the stated conditions.

### Final checkpoint documentation review

**APPROVE**

The Class checkpoint wording is historical, the source-feature observations are explicitly superseded by the active checkpoint and backlog, and the ledger link matches the renamed heading’s anchor. No edits were made.
