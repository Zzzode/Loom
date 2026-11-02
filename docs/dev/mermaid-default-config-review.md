# P0 DefaultConfig source and consumer review

Date: 2026-10-30. Scope: one complete official original, terminal semantics only.

## Source/model review

**APPROVE — source/model review**, by `agent:default_config_source_review`.

The model in [the P0 test](/Users/zzzode/Develop/Loom/tests/ui/test_mermaid_flow_semantics.cpp:610) can be reused by the official matrix consumer for **terminal original-file admission**, conditional on that consumer binding the complete original file and passing its full geometry and frame checks. This verdict gives no browser, theme, or upstream snapshot equivalence credit.

- **Binding and envelope:** The complete [fixture](/Users/zzzode/Develop/Loom/tests/fixtures/mermaid_official/cases/conf-and-directives/pr-check/no-config-use-default.mmd:1) hashes to `6f56685a482928f690a4920225dd1e89047586c3d277664cd68e4e84b4b9bb83`, matching both the test and manifest. Its frontmatter contains a folded **description**, despite the description’s phrase “no frontmatter”; it has no authored config or directive. The description is metadata, not a diagram label or a theme assertion.
- **Scene:** `graph TD` is correctly modeled as top-to-bottom. A is rounded and labeled `Default`; B is a parallelogram labeled `Another`; C is a rectangle labeled `End`. The root group `section` contains B and C, not A. The two unlabeled solid directed edges are A→B and A→C. No additional source scene elements are omitted.
- **Checks:** The test requires a drawing at 240 columns, validates its geometry, and rejects altered label, shape, edge count, endpoint, and membership models. At 40 columns it accepts exact ordinary-source fallback or requires valid geometry; returning to 240 must reproduce the original wide cells. It checks the following sentinel in the wide and narrow frames.

The oracle has limits relevant to reuse. This test calls `validate`, while the stronger `flow400_geometry` also requires an exact, independent following row; the matrix consumer should use its full protocol rather than treating this test alone as admission evidence. The model describes terminal geometry and does not encode frontmatter, default-theme behavior, or visual styling. The negative checks establish rejection of those particular model mutations, not exhaustive detection of every corrupt drawing; bounded oracle exhaustion is also a failure, not positive geometry evidence. See [the oracle](/Users/zzzode/Develop/Loom/tests/support/mermaid_flow_oracle.hpp:1012).

This was a read-only source review; I did not run the test or infer a snapshot from the renderer.

## Consumer re-review

**APPROVE** for the scoped DefaultConfig changes and removal of the single source-verified `ordinary_source_fallback_expected` roster row, conditional on updating the observed gap count to **69** and passing the final debug and release builds and tests.

The source hash matches the fixture, and the Model captures its nodes, shapes, edges, and group membership. The admission checks wide and restored geometry, narrow geometry or exact ordinary-source fallback, restoration equality, and four semantic or physical mutations across the five captured frames. The shared work budget is established before hashing and rendering setup; the oversized-source test also verifies rejection before hashing.

The supplied debug matrix log supports removal of exactly that one roster row. It is a **failing run**, however: the roster still contains the row and the test expects 70 while observing 69. It does not establish that the final dual-preset tests passed.

Final validation is recorded in the implementation backlog. No other P0 or P1–P3 completion is implied.

## Executed acceptance evidence

Both configurations passed all 833 Mermaid tests, including complete official
matrix execution and the new source/mutation/oversize rejection controls.
Debug ran in 320.60 seconds; Release in 37.38 seconds. The matrix hash is
`370e2db075a6e84060e059605069919dee2228a1198c330b8ee30323c28bb5dd` in both
configurations. The gap verifier passed for both report directories; only the
reviewed DefaultConfig ordinary-source-error row was removed. All 2,738 matrix
rows were compared; this original changed from error to drawn at every width,
and no previously admitted drawing regressed. This satisfies the consumer
review's conditional execution requirements. Whole-project CTest was not run.
