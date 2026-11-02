# P1 Flow style4 execution plan

Status: complete source authority and revised design/PRR independently approved;
implementation is in progress, with no Flow admission or coverage credit. This
bounded D1c sub-batch follows the separately gated Class D1a/D1b work under
implementable RFC0030 Phase D. Phase D remains incomplete. Obtain independent
source and design/PRR approval before production changes.

## Repository snapshot: 2026-11-02

The implementation and independent oracle prototypes are retained for continued
development, behind a thread-local testing preview that defaults to disabled.
Only scoped prototype tests enable it and restore the previous setting. Preview
rendering bypasses both normal cache reads and writes. The new handDrawn metadata
profile is subject to the same preview guard. This snapshot does not graduate
D1c or admit any Flow originals: accepted coverage remains 663/2145 (30.91%),
with 1482 fallbacks and zero errors; fallbacks and errors count as failures.

Independent reviewers `/root/all_changes_commit_review` and
`/root/style6_design_and_checkpoint_review` approved the snapshot isolation,
conditional on successful Debug and Release builds and regression tests. Source,
producer, consumer, resource, and formal admission gates below remain required
before enabling this feature by default or changing the coverage checkpoint.

Snapshot validation: both all-target builds passed in
`build/mermaid-baseline-debug` and `build/mermaid-baseline-release`. Full CTest
runs covered 4079 enabled tests in each configuration (two existing tests are
disabled). The initial runs had 29 Debug and 32 Release failures: 29 streaming
golden comparisons depended on the machine's configured model, and three
Release clipboard selection tests failed under parallel load. All 43 streaming
and selection tests passed when rerun serially in each configuration with
`LOOM_MODEL=model_api/experimental_0812_256k`; no golden files were regenerated.
All other enabled tests passed in the full runs. The Mermaid privacy check,
31-file RFC lint, and two-build gap-roster verification passed. Both full-run
official matrices retain SHA-256
`85d9020dc4366edc112e08fc575b935f162889bfb52693e9c096c2a576abbdb2`.

## Complete source meaning

Four originals declare three root rectangular nodes and two directed solid
chain edges. Three use LR Lorem/Ipsum/Dolor, assign a dark class to Lorem and
Dolor and leave Ipsum unstyled; the fourth uses TD Start/Process/End, assigns
highlight to B and a direct style to C, leaving A unstyled. All source
properties are fill, stroke, stroke-width and optional color. Values are
hex3 and width2/4px. Models preserve complete frontmatter, including optional
Dagre/handDrawn, htmlLabels and securityLevel declarations, with foreign
browser/CSS/layout assertions explicitly remaining debt.

The immutable `flow_style4_source_models.proposal.v1.json` separately owns four
complete original Models and four complete body-only diagnostic Models. The
body Models have empty frontmatter configuration and independent canonical
pins; they never substitute for complete originals or increase2145. Each row
pins source/frontmatter/body bytes/hashes, all nodes and edges, both complete
Model identities, definition/reference/direct-style statements and effective
property/projection tuples. Admission stays false in this asset.

## Bounded implementation

Add Flow-only, source-witnessed root rectangle classDef, single/multi-target
`class ID[,ID...] NAME` and single-target `style ID`. Keep undefined references,
unknown targets, implicit default classes, groups, nonrectangular targets,
unsupported properties and source forms closed. Do not widen unrelated family
grammars. Definitions resolve after the full parse, class properties cascade
in source assignment order and direct properties apply last. Capture only
actual validated root statements: label/comment/multiline text must never
become definitions. Preserve complete physical records, including the small
fixture's trailing semicolons and source spaces; escaped TAB is disclosed.

Reuse source color parsing, semantic hue tokens and the lazy owned style
channel/cache, with a separate Flow grammar and ownership adapter. Labels on
fill use palette.text and source foreground on fill is visibly unprojected.
Stroke-width>=4 projects to bold border. Unassigned nodes, edges, whitespace
and following text retain their existing attributes. Paint only exact box
interiors and actual border bits; a box character in a label is not a border.

Preserve source/input/Canvas limits and the1M producer Work budget. Prepay all
new owned tables, definitions, references, scans, cascade, style cells, styled
row copies, source legends, cache bytes/debit and final Elements. No phase
reset or cap increase. Bound64 definitions,64 assignments,64 direct statements,
64-byte IDs/names and1024-byte physical records; target lists also have explicit
paid cardinality and duplicate policy. At most seven active legend records
plus the projection key, each240 cells. Retain every original source fact or
preserve source atomically. Cache owns semantic styles and resolves current
theme on each render; ASCII keeps source for applied styles. Preserve the
old style-free Flow glyph/Work path and all shape/routing snapshots, measuring
baseline fees before edits if structure changes could affect them.

## Separate source/screen admission

Use a separate test-private FlowStyle4 contract and independently source-bound
complete graph/properties. Capture and publish all five public physical frames
before source binding or Model decode. The complete graph oracle must prove
all3 node identities/shapes, both directed routes, exact node membership and
full-frame conservation. Independently discover and bind each node's real
contour; check every glyph/foreground/background/eight-flags/hyperlink cell,
complete source/projection legend and footer. No expected geometry or attributes
come from producer parses, layouts or captured output. Check wide/restored
native screens, narrow ordinary source and restoration using the existing
explicit syntax-token foreground policy. Unknown/incomplete/resource results
never count as success or decisive negatives.

Require independent hand papers, at least eight decisive geometry/route/style/
legend/attribute negatives per scene, no-op/crop/domain/oversized-cell/full-scalar
controls, exact/one-short producer and oracle Work, seven palettes with four
accessibility combinations, cache copy/eviction and fit/ASCII checks. Both new
original and separate body domains need strict source/configuration/complete
Model binding, metadata-forgery/consumption/report-identity and real early/late
resource cutoffs. Use one caller-owned paid128M test ledger per scene, including
publication and reserved failure metadata; default producer remains1M.

Integrate complete-original Flow400 and OfficialMatrix separately from body
observations. Body diagnostics may improve if their independently owned complete
body proofs pass, but earn no original credit. Preserve all legacy authorities;
review every extra corpus gain/loss and exact roster change rather than assuming
other observations stay unchanged. Only exact source-reviewed complete originals
can increase drawing coverage. Run independent producer/oracle/integration and
exact-roster reviews, full Debug/Release Mermaid builds/regressions and serial
style/resource/real-PTY/copy gates, privacy/RFC lint and exact dual matrices.
Publish source-by-source dispositions, then update current coverage and all
remaining failure inventories. Continue remaining P1 before P2/P3; no browser,
full-project or RFC graduation claim follows from this sub-batch.

## Revision1: exact limits, disjoint identity and real emission cutoff

### Cardinality, duplicates and ordering

Each assignment has1..64 unique ASCII entity IDs, each1..64 bytes, with the
bare `[A-Za-z_][A-Za-z0-9_]*` subset used by these witnesses. Class names use
the same bounded subset. There are at most64 definitions,64 assignment
statements and64 single-target direct statements. The aggregate assignment
target occurrences plus direct target occurrences is capped at128, including
repeats. Debit every token, target visit, stored byte and bounded lookup before
ownership or matching; limits are checked before reserve/copy. Larger valid
Mermaid forms remain source in this bounded sub-batch.

Reject duplicate targets within one target list, duplicate definition names,
and duplicate normalized property keys. Definitions resolve after parsing;
assignments execute in physical source order with target lists left to right,
then all direct statements execute in their own source order. Repeated
assignment/direct statements still count toward cardinality and Work and apply
their properties on every occurrence. Only consecutive normalized equivalent
style-operation records may collapse in the legend, retaining first verbatim
spelling and a disclosed count. Nonconsecutive repeats retain separate records
so a later reapplication cannot vanish across an intervening override. Preserve
active definition records separately and the chronological order of displayed
source records; the legend key explicitly discloses assignment-order cascade
and direct-last semantics. Seven records plus one key remain the hard legend
cap; overflow is complete source fallback. Add64/65 per-list/definition/statement,
128/129 aggregate, duplicate and reapplication controls, using an explicitly
injected test allowance only where necessary to reach the structural branch;
that does not raise the1M production limit or earn corpus credit.

Only one style statement occupies a physical row. Its optional single trailing
semicolon is syntax and remains verbatim in the displayed source record; a
second statement on the same row stays outside this sub-batch. Root graph and
style statements are lexed with the existing quote/label/comment boundaries.
Capture validated root source spans at parse time; never reconstruct definition
authority by scanning arbitrary label or member lines later. Keep pending state
local and lazy, attaching an owned immutable effective style table only after
complete parse/resolution. No borrowed source or theme storage enters cache.

### Original/body identities and counting

Use two closed contracts: `FlowStyle4OriginalWholeScreen/v1` and
`FlowStyle4BodyDiagnosticWholeScreen/v1`. The trusted input domain is selected
before rendering from the immutable manifest/track, never inferred from a
mutable report or Model hash. A scene key binds asset identity, fixture,
manifest global ordinal, source ordinal, track, complete original bytes/hash/
Git blob, exact frontmatter extent and body bytes/hash, actual input bytes/hash,
and that domain's complete Model bytes/hash. The body is exactly the original
suffix after the pinned frontmatter, independently checked before decoding.

The TD original and body Models deliberately share1671 canonical bytes and
SHA `bb7ca82d840453073f775a7cfc835446654c89120f487e68d46876b0b3072e42`;
Model identity alone is therefore explicitly insufficient. Source and domain
keys remain different. Native Flow400 consumes exactly the four original keys;
the body set is independently consumed only by the diagnostic path. The report
boundary receives independently recorded manifest-derived expected slots for
all eight fixture/track keys before builders run. It rejects missing, duplicate,
swapped or jointly erased provenance before generic emission/counting.

A strict proof retains the trusted original/body enum separately from its
serialized contract/track and binds all source identities, five published
captures, complete decode, two native outcomes, restoration, narrow source,
eight decisive negatives, typed failure causes, monotonic shared Work and final
statuses. Only a complete admitted Original-domain proof adds one to the
2145-original numerator. Every Body-domain proof has immutable original credit0,
even when its complete scene passes. Counter/emitter controls swap domains and
full/actual source hashes, erase owners/metadata jointly, reuse equal Model
hashes and duplicate consumption; all must fail. Body successes never enter the
original counter through per-state or generic aggregation.

### Actual final Element emission boundary

Use a passive test-facade trace, forwarded directly into the production
`render_mermaid_with_budget`/`finish_native_layout` path with the same `Work&`.
It records entry into the styled final-Element branch, remaining allowance at
entry, successful preflight debit, whether final allocation began and whether
a native Element was returned. It exposes no model/layout/expected pixels and
has no allowance of its own. Production default calls leave the optional trace
null; their glyphs, style-free fees, source limits and cache path stay unchanged.
Review this minimal seam before adding it.

For each real styled source, measure the total public rendering debit. Re-run
with exactly that amount: require styled emission entered, allocation begun,
native Element returned and remaining0. With one unit less, require styled
emission entered, the last actual final-emission preflight debit failing,
remaining0, allocation not begun and no returned native or partial Element.
This proves the cutoff happened at the production emission boundary rather
than an earlier parser/oracle stage. Both attempts use one unchanged caller
ledger through parsing, layout, style ownership and final emission. Supply the
complete ordinary source/continuation Element as the fallback, render the
caller's normal `optional_native ? native : source` choice, and verify full
source glyph/background/flags/links and following content with no styled leaks.
Do not manufacture a sentinel-only fallback as source-preservation evidence.
Keep the existing real early/late oracle/proof cutoffs as separate gates.

## Independent review records

Separate read-only Codex invocations; no Claude model execution is claimed.

### Complete source review and design revision requests

**REQUEST_CHANGES — source review passes; the Flow design/PRR is not yet concrete enough to approve production edits.**

The immutable proposal is exactly **31,764 bytes** with SHA-256 `37da945aa0de4196a73ae1b3d46af9d5a71a181abf9295585464c5224fb559f5`. I verified all four original, frontmatter, and body byte counts and hashes against the checked-in sources, and recomputed all eight canonical Model byte counts and hashes.

The source models match the requested graphs, definitions, assignments, direct styles, effective properties, and D1 color roles. The three LR sources share the Lorem → Ipsum → Dolor body; the TD source has the two stated edges and preserves its physical semicolons. All body models have empty configuration, all admission flags remain false, and the asset claims no foreign assertions.

The following design points need explicit resolution before Flow production edits:

1. **Cardinality and duplicates.** The [plan](/Users/zzzode/Develop/Loom/docs/dev/mermaid-flow-style4-plan.md:46) promises a paid target-list limit and duplicate policy without stating either. Specify the per-list and aggregate limits, charging, and behavior for duplicate targets, definitions, properties, assignments, and direct styles, including interactions with assignment-order cascade and physical statement preservation. Include boundary and duplicate controls.

2. **Original/body identity and counting.** The [admission plan](/Users/zzzode/Develop/Loom/docs/dev/mermaid-flow-style4-plan.md:72) requires strict binding but does not define a disjoint domain key or counting proof. This matters concretely: the TD original and its body model both serialize to **1,671 bytes**, SHA-256 `bb7ca82d840453073f775a7cfc835446654c89120f487e68d46876b0b3072e42`. Define source-domain and exact-source binding independent of Model hash, prove separate consumption and report identities, and show that a body success cannot enter the original count.

3. **Actual emission cutoff.** The plan calls for early/late resource cutoffs and prepaid final Elements, but does not identify a test that exhausts the shared producer ledger at the **actual styled final-element emission boundary**. Specify how the test reaches that boundary, proves the cutoff is real, and verifies complete source fallback without partial output; include exact and one-short controls on the same ledger.

The remaining proposed boundaries are appropriate: validated root-statement authority, Flow-specific grammar, atomic source fallback, style-free glyph and Work preservation, theme-resolved cache ownership, an independent three-node/two-route all-attributes screen oracle, and dual-preset plus serial PTY gates. Their execution remains future evidence, not a current pass.

I did not rerun the Class D1b gates or independently verify the reported 889/889, 25 serial passes, or coverage 663; I treat those as the supplied checkpoint while documentation is being updated. No files were edited, and this review grants no coverage credit or phase graduation.

### Revised design and PRR, including minimal passive trace

**APPROVED — bounded FlowStyle4 design and PRR, including the minimal passive trace.** Revision 1 resolves the three points in the prior review.

- The cardinality limits, duplicate rejection, ordering, and legend-collapse rules now preserve repeated operations and their cascade effects. Boundary controls distinguish the injected test allowance from the production limit.
- The original and body contracts bind the input domain and complete source identity independently of the Model hash. The eight expected slots and zero body credit address the equal-hash TD case and prevent diagnostic successes from entering the original count.
- The proposed trace follows the existing test-facade pattern into the actual render and finish path with the same `Work&`. In the current [finish path](/Users/zzzode/Develop/Loom/src/ui/visual/mermaid_impl.cpp:2594), styled preflight debits precede Element allocation; the specified exact and one-short runs can establish that boundary, provided the trace records completion of the **entire** preflight and identifies failure of its last debit. The complete ordinary-source fallback check supplies the missing preservation evidence.

This is approval of the design and its proposed gates, not evidence that they have passed. I did not rerun the reported Class D1b tests or revalidate the previously approved source asset. No files were edited; this review grants no admission, coverage, or phase credit.

## Revision2: exact handDrawn metadata profile

The first actual-source prototype exposed a second blocker: the handDrawn
original's complete metadata is rejected before style parsing. Its body and
the Dagre complete source already reach styled emission. The generic Flow
metadata reader currently supports strict root/nested htmlLabels:true plus
securityLevel:loose, optionally layout:dagre, but has no look field.

Add one explicit additional metadata combination: description present, config
with exactly look:handDrawn, root htmlLabels:true, nested flowchart.htmlLabels:true
and securityLevel:loose. Recognize only the witnessed unquoted handDrawn value,
reject duplicate look and all unknown values/keys, and retain the existing
root/event/type/indent/description checks. Keep look combined with courier,
false HTML, layout or any other profile outside this sub-batch. This is a
configuration-profile boundary, independent of graph content or fixture path.
Do not gate metadata on the presence of styles. HandDrawn browser appearance
remains foreign debt; this supports the declared standard terminal projection.

A diagnostic-only experiment removed look from47 pending source inputs. Nine
altered inputs reached the current native backend; eight use courier profiles
that remain closed here, and only the style4 handDrawn source has the new strict
HTML/security combination. Those altered inputs earn no original credit.
Record the full-source audit and verify all actual observed statuses after the
change rather than treating the diagnostic as admission. Add exact complete
source positive, ordinary strict-look profile positive, duplicate/wrong value/
unknown key/type and courier/false-HTML/combined-layout negative controls.
Keep all original Models and separate body Models immutable; configurations
must be intrinsically decoded, not accepted solely by a Model hash. Any other
corpus gain/loss still requires separate complete source and roster review.
