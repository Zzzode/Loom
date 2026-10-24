# Complete Mermaid obligation ledger

An earlier separate ER subgraph resource-only checkpoint builds Native203 and
runs one focused TEST per preset, both passing. Ten complete sources/five lower
Models bind after five actual view captures; upper5 are whole ordinary and
lower5 remain ordinary/fitUNKNOWN. No drawing, internal-cap, production-Work or
foreign proof is inferred. Old202 TESTs were not rerun;88 prior report files
per preset and13,651 source paths/hashes remain exact. Independent retention
review1252cf598493e2cf9398fba751cc6c63e35f057d65aa16db29525bca6825213c
closes without a material finding. Producer and full-regression gates remain
open; formal coverage stays469/2145.

This is an acceptance contract and a source audit, not a compatibility score.
The owning production module is `loom.ui.visual.mermaid`; this audit adds no
runtime dependency, exported API, configuration or renderer implementation.
The pinned inputs and original expectations remain immutable. The user requires
complete applicable test coverage, and forbids commits and pushes for this work.

The detailed audit is retained under
`build/mermaid-renderer-research-20261020/full-obligation-audit/`. Its JSON files
link each case and original assertion to the vendored source path, file SHA-256,
line/byte span where available, typed-model reference and original option
provenance. These maintenance artifacts do not execute or replace native tests.
The durable test entrypoints remain those in
[the terminal test guide](mermaid-terminal-tests.md) and
[the official matrix guide](mermaid-conformance.md).

## Latest executed checkpoint and remaining scope

### RFC 0031 E6c checkpoint (2026-10-24): mandatory gates become reviewed capability reports

The 12 deliberately-red mandatory-completion gates were renamed into
permanently non-failing `MermaidCapabilityReport.*` tests (12 renames, zero
tests added or removed; two new `MermaidCapabilityRoster.*` integrity tests
bring the Mermaid label to 810 registered ctests, of which 798 are the strict
green regression subset selected by `-L mermaid -E '^MermaidCapabilityReport\.'`).
Enumeration, JSON/MD report emission, resource/integrity/provenance checks and
zero-non-gap-error assertions are unchanged and stay hard failures; only the
aspiration-tier "100% admission" assertions were removed. The frozen observed
counts are now explicit strict `EXPECT_EQ` pins, and each report serializes a
canonical `mandatory_gaps` array diffed both directions against the reviewed
ledger.

Roster: `tools/mermaid/e6c-mandatory-gap-roster.json`, schema
`e6c_mandatory_gap_roster/v1`, **61620 bytes**, SHA-256
`29611b1dae04a3c5b30d14fa904ece3be6ab37d924ae6621079e7633afcd6f1d`, pins in
`tests/support/mermaid_gap_roster.hpp`; verified across debug and release by
`python3 -I tools/mermaid/verify_e6c_gap_roster.py
--build-dirs build/debug/tests/ui,build/release/tests/ui` (serialized gap sets
are byte-stable across presets; 356 roster rows total).

Per-gate frozen counts and roster rows:

| Gate (new ctest name) | Roster key | Frozen counts | Rows |
|---|---|---|---:|
| `MermaidCapabilityReport.OfficialMatrix` | `official_matrix` | ER errors 6/pending 144/admitted 62; Flow errors 9/decoded 54/pending 346; Class errors 30/decoded 237/pending 48/admitted 211; drawn_at_240 582/2145 | 94 |
| `MermaidCapabilityReport.Flow400CompleteSources` | — (counts-only) | errors 9 at matrix; visited 400, decoded/positive/mutated 54, pending 346, zero non-gap errors | 0 |
| `MermaidCapabilityReport.MermanXY102` | `merman_xy102` | 102 inputs, 85 candidate/17 fallback, partition 3/17/82/0, geometry 77, empty 5, profile pending 20, zero errors | 20 |
| `MermaidCapabilityReport.Class481` | `class481` | matched/mutated 341, pending 137, 3 positive fallbacks (`merman/5974d9abfc8f91652b9b`, `merman/a254b69141639fa8b045`, `merman/c50650cbac88ef419784`) | 140 |
| `MermaidCapabilityReport.Class285` | `class285` | decoded 237, matched/mutated 211, pending 48 | 74 |
| `MermaidCapabilityReport.ClassEndpoint29` | `class_endpoint29` | matched/mutated 26, the same three fallback ids | 3 |
| `MermaidCapabilityReport.State85` | `state85` | matched/mutated 71, geometry_errors 1, positive_pending 7, original_policy 1 (`termaid/17f03f94ca1a30b6bf67`), choice_policy 1 (`merman/2419c8d227ab4bcb368d`) | 14 |
| `MermaidCapabilityReport.ClassCaptionStarvation8` | `class_caption_starvation8` | matched/mutated 6, fallbacks 2 (`unequal_multiline_and_marker`, `foreign_owner_keeps_caption`) | 2 |
| `MermaidCapabilityReport.StateReverseEscape` | `state_reverse_escape` | matched/mutated 8, fallback 1 (`original_b91_whole_immutable`) | 1 |
| `MermaidCapabilityReport.ClassNotes18` | `class_notes18` | original 18/decoded 22, strong 16; fallbacks at application ordinals 272-277 | 6 |
| `MermaidCapabilityReport.Requirement11P1P6` | `requirement11_p1p6` | complete 8, geometry 1 (P1), fallbacks 1 (P3 expected control), P5 negatives 10 | 1 |
| `MermaidCapabilityReport.Requirement11Overflow` | `requirement11_overflow` | complete 1, fallbacks 1, errors 1 (LR case index 0) | 1 |

`drawn_at_240` is 582 rather than the E1-era 540 solely because E2a admitted
the reviewed config-free gitGraph/state cohort (+42); every other frozen count
matches the pre-E2a red baseline. Both presets show 809/810 under
`ctest -L mermaid -j8`, the single failure being the pre-existing real-PTY
timing flake `E2E_Pty_Mermaid` (passes alone on an unloaded machine and is not
a renderer report). Rule for later phases: any renderer-driven outcome change
(gain or loss) must update the roster (via `--write`, followed by independent
review) and the frozen constants **in the same change**; expectation changes
still go only through this ledger, never through renderer output.

### Capability-expansion task list after RFC 0031 (next coverage work)

This is the forward-looking companion to the executed RFC 0031 checkpoints
above. RFC 0031 removed structural/architectural debt (one envelope, five
relations units, an ordered probe table, coarse Work charges, a private
detail partition, non-failing reports, a fuzz harness and a per-instance row
cache); it did **not** add body grammar or layout geometry. The remaining
gap is therefore almost entirely *native layout/protocol capacity*, tracked
row-by-row in `tools/mermaid/e6c-mandatory-gap-roster.json` (356 rows) and
the per-family semantic ledgers. Every task below keeps the standing rule:
a row earns drawing credit only when the bounded native parser AND layout
complete it; a source the router cannot close stays whole-source and stays
registered, never silently degrading to partial art.

Current native admission (full original files, 240 columns) after E2a is
**582 / 2,145**: Flow 54/400, Sequence 193/193, Class 211/285, ER 62/206,
stateDiagram-v2 11/103, stateDiagram v1 0/42, Requirement 11/172, Pie 7/24,
xychart 2/48, gitGraph 55/135. Sequence is the only fully covered family.

Tasks are ordered by leverage (shared root cause first); counts are the
frozen, reviewed gap rows each task must retire.

| Task | Scope | Frozen gap rows | Root cause / work |
|---|---|---:|---|
| T1 bounded edge router | Flow + GitGraph shared layout engine | Flow400 pending 346 (no roster), matrix flowchart 56, gitGraph native-parent-route 16 | `plan_parent_routes` is a greedy 4-connected BFS that cannot back-track or detour in the bounded canvas; long cross-lane edges, backward merges and cherry-pick second parents, and dense multi-branch graphs fail to close geometry even though every body parses. Upgrade to an obstruction-aware channel/backtracking router. Highest leverage: one engine unlocks Flow346 and most of the 16 gitGraph route-debt rows. |
| T2 Class geometry | classDiagram | class481 137+3, class285 48+26, endpoint29 3, caption 2, notes18 6 (225 rows) | Member partition/style compartments, stereotype/direction annotation placement, note connectors and long relationship-cardinality labels do not get complete cells; bodies parse. Complete the per-row cell geometry. |
| T3 ER table layout | erDiagram | ER206 pending 144 + matrix 6 | Multi-attribute column alignment/separation, cross-table cardinal tag (`||--o{`)避让 and multi-table grouping geometry. |
| T4 Requirement graph | requirementDiagram | requirement11 2 + matrix 1, plus ~160 pending | Build on the existing requirement lexer + SCC ordering: complete requirement/element boxes, contains/excludes/copies/verifies edges and cross-layer edge geometry. |
| T5a State grammar (next-line brace/divider) | stateDiagram-v2 | state85 3+1 = 4 small, independent rows | Accept a composite whose `{` opens on its own physical line and a standalone `--` region divider (currently lexical rejects even though bounded parsing exists). Small, self-contained, can land first. |
| T5b State composite geometry | stateDiagram(-v2) | state85 remaining 10 + ~124 pending | Nested composite frames, fork/join/choice/history pseudostates and concurrency regions. |
| T6 Pie geometry | pie | ~17 pending | Percentage-label placement, multi-slice start angles, zero/extreme-value bounds. |
| T7 XY chart geometry | xychart-beta | merman_xy102 profile_pending 20, plus ~26 pending | Ticks/grid/axis titles, stacked bars and multi-series layout over the 82 rows whose value geometry already closes. |
| T8 gitGraph semantic config (RFC 0031 E2b) | gitGraph | the 62 config/init-bearing git originals | `mainBranchName`/`showBranches`/`showCommitLabel`/`rotateCommitLabel`/`parallelCommits` change content, ordering, visibility or orientation, so they are projected profiles, not ignorable config; they need the E2b per-key whitelist plus RFC 0030 Phase D style decisions. |
| T9 Cross-cutting: style/HTML projection (RFC 0030 Phase D) | all families | prerequisite for styled originals | No RGB/HTML exists; design the semantic projection layer and palette-token mapping. Unblocks styled config (with E2b) and richer themed diagrams; independent of the layout routers. |
| T10 Linux execution gate | verification infra | n/a | The only unclosed execution gate from RFC 0031 E1/E5/E6a/E6b: BMI PSS before/after and serial dual-preset ctest on a Linux/Homebrew-LLVM box; macOS has no `/proc`. Required before large layout changes land without a memory/perf gate. |

Recommended order and rationale:

1. **T1 first** — it is the shared root cause of 346 Flow + ~16 GitGraph gaps
   (the largest single block). The gitGraph router is literally a copy of the
   same greedy connector.
2. **T5a can land any time** (four rows, pure lexical, no layout risk) and
   **T10 as early as possible** so subsequent layout/perf work is gated on
   Linux.
3. Then by descending gap size with decreasing coupling:
   T2 (class) -> T3 (ER) -> T4 (requirement) -> T5b (state composites)
   -> T6 (pie) -> T7 (xychart).
4. **T9 (style projection)** runs in parallel with T2-T7 and is the design
   prerequisite for **T8/E2b** (themed/config-bearing originals).

Acceptance for every task is mechanical, not aspirational: the named
`MermaidCapabilityReport.*` gate goes green as its roster rows move to drawn;
the gap roster is regenerated (`verify_e6c_gap_roster.py --write`) and
independently re-reviewed in the SAME change; drawn_at_240 and the official
matrix family counts increase with zero source-fallback regressions on
previously drawn rows. Rows whose only blocker is the envelope were already
cleared in E2a and must not reappear here.


The current source-owned LinkProfile3 checkpoint independently closes three complete originals in both consumers: formal drawing coverage is **525/2145 (24.48%)**, Flow **39/400 (9.75%)**. Each source has five actual public captures and six meaningful negatives; 36 independent four-face goods and 108 physical negatives pass. Complete Flow53 and Official30 execute per preset with 81 pass, two mandatory capability failures and zero errors. The preceding complete nine-target first pass is 789/777 pass/12 fail/zero errors per preset. Keep those runs distinct: the last correction changes only test-private repeated-baseline handling, and the other seven targets were not rerun. Both runs freeze all 13,902 source pins. Source fallback and resource failures earn no drawing credit. Evidence: `root-flow-linkprofile3-complete-native-official-dual-proof-r12.json` and `root-flow-linkprofile3-complete-full-nine-dual-proof-r10.json`.

The preceding R2 AST source index includes 277 complete selected files (22 Beautiful, 32 Go ASCII, 132 Merman, 54 mmdflux, 37 Termaid). This closes the four formerly omitted Merman support roots and 11 complete mmdflux sources behind 126 slices. Root independently verifies the index validator and all 22 package members. Termaid has 25 parametrized function containers and 27 decorator sites; two stacked decorators require explicit Cartesian expansion. These are source facts, not executed assertions or a complete semantic denominator.

That preceding 277-file selector excluded complete context for 2,052 other declared private/non-terminal mmdflux predicates. Root therefore separately recovers all 121 distinct internal source paths declared by the pinned mmdflux provenance, verifying full SHA-256 and Git blob identities, UTF-8, all 2,412 predicate byte/line ranges, and all 126 exact slice contents. This 3,714,466-byte source recovery is not an AST port, runtime result, applicability decision or drawing admission. The five-suite semantic denominator remains open. Evidence: `five-suite-full-ast-scope-r2/FULL_AST_SOURCE_INDEX.json` and `five-suite-mmdflux-all-internal-source-root-r3/SOURCE_RECOVERY.json`.

The later R3/R4 static reconciliation expands complete source context to **387 files**: 22 Beautiful, 32 Go ASCII, 132 Merman, 164 mmdflux and 37 Termaid. All 121 mmdflux internal paths declared by the existing provenance are recovered, and all 2,412 private-predicate ranges bind exactly once to their full AST test functions. The four other suite records remain byte-exact to the preceding index. Within direct test bodies, **15,536 context entries represent 15,390 distinct assertion source locations**; all 146 repeats are Go `t.Run` assertions also present in their enclosing parent test context. These counts exclude unexpanded helper/macro/runtime semantics and do not establish migrated assertion coverage. The full five-suite semantic denominator remains open. Evidence: `five-suite-full-ast-all-declared-internal-root-r3/FULL_AST_SOURCE_INDEX.json` and `five-suite-full-ast-site-identity-root-r4/SOURCE_SITE_COUNTS.json`.

A separate Requirement experiment remains a failed trial. Current-source Debug counters locate both mandatory failures at the same `R -> F <<traces>>` connector: all 78 compatible port pairs fail before label placement, with production Work remaining. Skipping markerless side-stem reservations does not solve either source. That candidate is withdrawn; the exact original producer and normal binaries are rebuilt, all seven Requirement11 TESTs execute per preset (five pass, two mandatory failures), and all four Requirement reports per preset return byte-exact to their pre-experiment state. No production fix or coverage credit is retained. Evidence: `root-requirement11-markerless-failed-trial-and-restore-proof-r2.json`.

### Preserved preceding checkpoints

The latest closed complete nine-target first pass builds Debug and Release and
executes 785 TESTs per preset: 773 pass, 12 fail, zero GTest errors. All 13,900
source pins remain unchanged. The complete Termaid GitGraph file passes its 39
original functions and 81 assertion source sites, including ASCII. Formal
original drawing coverage remains 522/2145, with Flow36/400; source fallback
has zero drawing credit. Evidence:
`root-gitgraph-original39-ascii-full-nine-dual-proof-r2.json`.

A later read-only full-source audit discovers 262 pinned upstream test files
across the five projects. The historic v3 ledger's 1,284 mixed source units,
1,192 function identities and 111 files describe its imported subset. Its
3,060 partial source-site entries and the separate 8,454 input cases are not a
complete semantic denominator. Python AST verifies the Termaid identities,
but TypeScript, Go and Rust extraction and dynamic test expansion still have
explicit unresolved boundaries. A language-AST reconciliation is in progress;
no complete-five-suite percentage is claimed. Evidence:
`five-suite-full-denominator-audit-r1/FULL_DENOMINATOR.json`.

The LinkProfile3 source-first gate subsequently builds and executes all52 Flow
TESTs per preset:51 pass and the complete400 gate fails. The independent hand
goods and source authority checks pass; all three selected complete sources
bind and decode before failing geometry with ordinary whole-source fallback.
The separate Official run executes30 TESTs:29 pass and its complete matrix
fails. Earlier compile, binding-key and protocol-control test defects are
retained as failures before their corrections. These new source expectations
do not grant drawing credit. Evidence:
`root-flow-linkprofile3-source-first-red-proof-r6.json`.

### Preserved preceding checkpoint

The latest complete nine-target first pass is 740 TESTs per preset: 727 pass,
13 fail, zero GTest errors. One new retained-diagnostic regression is repaired
by a changed-code all-29-Official rerun (28 pass, one mandatory failure), with
all reports unchanged; the other eight targets are retained, not rerun. Formal
original drawing coverage is 522/2145 (24.34%), including 36/400 Flow sources.
The five-suite semantic obligation set remains incomplete. See the current
[matrix checkpoint](mermaid-conformance.md) for the separate first-pass and
rerun evidence. Source fallback has zero drawing credit; no commit or push
has been made.

The subsequent focused direct Termaid GitGraph port closes its complete pinned
test file:39/39 unique original functions /81/81 assertion source sites pass,
including both ASCII options. All98 related TESTs pass per preset; every old
report remains exact. Four additional product regressions do not enlarge the
original denominator. Native30 compact geometry also passes separately. This
is not Official admission, a new full-nine pass, complete Termaid repository
coverage, or completion of the five-suite denominator. See the terminal test
guide and `root-termaid-gitgraph39-ascii-complete-dual-proof-r2.json`.

### Preserved preceding classic-shape checkpoint

The newer classic Flowchart checkpoint builds and executes all nine related
native targets in Debug and Release: **735 TESTs, 720 pass, 15 fail, zero
GTest errors** per preset. All 13,811 source pins remain unchanged during
execution. Three complete terminal-source drawings pass full five-view cell
geometry and exactly 24 semantic plus 24 physical negatives; all eleven classic
shape profiles and the midpoint/tangent controls pass. The two stale legacy
fallback assertions remain recorded as failures pending a retained-source
migration. Three newly drawn Flow400 originals and one configuration-folder
original remain unreviewed-art errors until their owning matrix consumers
independently validate complete Models. Formal credit remains **519/2145
(24.20%)**, Flow **33/400 (8.25%)**. GitGraph and the full five-suite semantic
obligations remain incomplete. Source fallback earns zero drawing credit.
Evidence: `root-classic11-production-full-nine-dual-proof-r2.json`.
Work remains uncommitted.

The latest reverse-domain XY checkpoint executes all seven related native targets
in both Debug and Release: **729 TESTs, 717 pass, 12 fail, zero GTest errors**.
The failures remain explicit complete-capability obligations. Three complete
Merman originals, three additional positive scenes, source controls, and direct
oracle mutations now pass; the originals each publish five full cell captures
with zero fallback, resource, or verification errors. The reviewed exclusive
Y-label boundary correction is subsequently verified by all 128 Charts TESTs
per preset: 127 pass and only the complete XY102 gate fails. All reports remain
exact in this subsequent run and all 13,806 source pins remain unchanged during
each execution. Evidence: `root-xy-reverse3-production-native-full-dual-proof-r10.json`.
This Native gain does not grant formal admission: official drawing coverage
remains **519/2145 (24.20%)**, Flow **33/400 (8.25%)**. The complete five-suite
semantic denominator and real-terminal graduation remain open. Source fallback
receives zero drawing credit. Work remains uncommitted.

The latest Source4 Official checkpoint independently admits all four complete
originals after five color-cell captures, full typed geometry and 29 meaningful
negatives. Formal drawing coverage is **519/2145 (24.20%)**, including Flow
**33/400 (8.25%)**; 367 Flow originals remain pending. Debug and Release each
build and run all 28 Official TESTs: 27 pass and the complete capability matrix
remains a failure. Reports remain byte-exact to the focused admission run, and
all 13,791 source pins remain unchanged during execution. Evidence:
`root-flow400-source4-owned-official-full-dual-proof-r3.json` and
`root-flow400-source4-owned-official-focused-proof-r3.json`. Separately, the
following-token helper regression runs all 49 Flow TESTs per preset: 48 pass
and only the complete 400-source gate fails, with all prior reports retained.
No full five-suite semantic or real-terminal completion is claimed. Source
fallback receives zero drawing credit. Work remains uncommitted.

The subsequent Source4 Native checkpoint builds both presets and executes
all 48 Flow TESTs: 47 pass and the complete 400-source gate remains a failure.
All four complete originals (0, 282, 374, 375) pass five owned captures,
full geometry, and exactly 8/9/5/7 meaningful negatives. Native Flow coverage
rises from 29 to 33 of 400; 367 sources remain pending. All 2,000 raw frames
stay exact; the other 396 rows change only by the newly paid 10,432 Work
units and matching phase balances. Both reports are byte-equal, with no
resource/geometry/mutation error and all 13,791 source pins unchanged during
execution. Evidence: `root-flow400-typed-source4-owned-native-dual-proof-r5.json`.
Earlier compile and setup failures are retained. Formal coverage remains
515/2145 until the Official consumer independently executes these four rows;
no fallback or native-only result grants formal credit. Work remains uncommitted.

The subsequent four-target regression builds Debug and Release and executes
all 524 native TESTs per preset: 515 pass, 9 fail, and 0 GTest errors.
Sequence 264/264, Markdown 44/44 and Terminal 4/4 pass; Relations retains
its nine mandatory capability failures out of 212 TESTs. Every existing report
and all 13,789 source pins remain unchanged during execution. Evidence:
`root-source4-pretrial-other-four-dual-proof.json`. This separate batch does
not change the formal drawing count or prove complete five-suite semantics.

The newer Pie7 owned-matrix checkpoint builds both presets, executes all124
Charts TESTs (123 pass/1 XY102 gate failure) and27 Official TESTs (26 pass/1
complete-matrix failure), with zero GTest errors. All7 Pie original sources
independently pass five color-cell captures, full source/Model geometry and
55 meaningful semantic/physical/lifecycle negatives. Formal drawing coverage
is515/2145 (24.01%). Only the seven selected original matrix rows change; the
other2138 originals and every prior Charts report remain exact. Both changed
reports are byte-equal across presets and all13,779 source pins remain exact.
Evidence: `root-pie7-owned-formal-admission-dual-proof-r3.json`; the two earlier
compile failures are preserved there. No full-suite, real-terminal or five-suite
semantic-completion claim follows. Work remains uncommitted.

The newer source-bound Mermaid ASCII caption port passes all3 complete
source/Model scenes,8 portable original conditional groups,8 direct predicate
negatives and9 full-geometry negatives in Debug and Release. The first actual
trial exposed a full-grid test-helper boundary defect: row0 captions were
excluded by an interior rectangle. Its reviewed correction plus five direct
boundary witnesses retain full geometry and fixed budgets. Full Flow now
passes45/46 TESTs; only the complete400-source gate fails. Three changed reports
are byte-equal across presets, and all13,778 source pins remain exact. Evidence:
`root-flow-go-parallel-portable-semantics-dual-proof-r2.json`. These eight groups
exclude still-unported ASCII presentation and Go result/API obligations and do
not establish the complete five-suite semantic denominator. Formal coverage
remains508/2145 pending independent owned matrix joins.

The newer Flow folded-description/shape-data checkpoint builds both presets
and passes43/44 Flow TESTs; only the complete400-source mandatory gate remains
red. Six complete inputs (including full original374/375) pass all30 meaningful
negatives. Strict description-only metadata is independent of body syntax;
closed `@{ shape: rect|rounded|diam }` follows source last-wins semantics.
Two historical copies of the same valid folded-description input are promoted
to mandatory full-Model drawing checks; none is skipped, and the immutable old
control asset remains exact. All23 config controls remain covered:11 mandatory
drawings/12 closed controls. The first producer run's stale N13 fallback
expectation remains recorded as an actual failure before its reviewed migration.
Evidence: `root-flow-folded-shape-data-first-red-proof-r3.json`,
`root-flow-folded-shape-data-production-partial-proof-r2.json`, and
`root-flow-folded-config-control-migration-dual-proof-r5.json`. All13,777 source
hashes remain exact during the last batch. Formal coverage remains508/2145;
Flow29/400 awaits the separately owned typed-source formal join. These are
focused native results, not full-suite or real-terminal graduation.

The latest focused checkpoint builds Debug and Release. Flow passes41/42
TESTs, with only its complete400-source gate failing; the focused Official
checks pass1/2, with the complete matrix still failing. Markdown44/44 and
Terminal4/4 pass. The12 rounded multiline sources pass all76 meaningful
negative mutations after the shared test oracle enforces adjacent authored
label rows; nine direct controls cover retained blanks and rejected gaps.
All13,776 source hashes remain exact during execution, and the changed rounded
report is byte-equal across presets. Evidence:
`root-rounded12-label-row-adjacency-dual-proof-r2.json`.

The Official owner now independently admits all11 Requirement sources after
five-frame capture, complete geometry and45 semantic/physical negative controls.
Formal drawing coverage is508/2145 (23.68%); Flow remains29/400. All2134 other
official rows retain their earlier results. Evidence:
`root-requirement11-official-formal-admission-proof-r5.json`. The complete
five-suite semantic denominator remains unestablished. These focused checks do
not replace full CTest or real-terminal validation; no source fallback receives
drawing credit. Work remains uncommitted.

The following paragraphs retain earlier executed checkpoints.

The latest complete seven-target batch executes713 native TESTs per preset:
701 pass,12 fail,0 GTest errors. Both Debug and Release build, all713 test names
match, all13,776 source pins remain exact, and155 prior reports stay unchanged;
the single changed peer report is byte-equal across presets. The exact restored
Requirement combo retains its one complete Peer RL gain. Evidence:
`root-flow-m4-req11-full-seven-proof-r13-restored.json`.

Merman's exact BT multiline-label assertion now also passes in both presets:
one original callsite/one runtime instance, two direct caption negative controls
and three legacy semantic/physical negatives. Its full five views, source and
complete Model are bound independently; no ASCII-option or whole-foreign-suite
claim follows. Flow's new focused batch passes6/6. Evidence:
`root-flow-m4-atomic-first-dual-proof-r2.json`. The later local-gap routing
experiment regressed Peer RL and exhausted production Work, so it was rejected
and exactly restored; it earns no capability credit. Formal coverage remains
497/2145, and the complete five-suite semantic denominator remains unresolved.

The following paragraphs retain earlier executed checkpoints.

The newer full seven-target batch builds both presets and executes710 TESTs
per preset:698 pass,12 fail,0 GTest errors. Markdown44/44, Sequence264/264 and
Terminal4/4 pass. Flow36/37, Charts123/124, Relations203/212 and Official24/25
retain their explicit complete-coverage failures. All13,776 source hashes are
conserved; six changed reports are byte-equal across presets and147 other prior
report files remain exact. Evidence: `root-flow2-pie3-req9-full-seven-proof.json`.
This is not full CTest or complete Mermaid-suite success.

A subsequent focused batch installs the independently reviewed Termaid R2 port:
four complete original `render()` functions, six original assertion expressions,
ten expanded predicate instances and nine meaningful Model/physical negatives
all pass in Debug and Release. Unicode labels, computed200-character input,
no-edge arrow absence, and the five-label loop retain exact complete function
and input bindings. All five public views precede per-scene source/Model decode;
ordinary wide output is a failed drawing. The whole Flow focused target passes
4/4. Requirement's SCC-stem R10 trial remains5/7, with reports exact to R9 and
no improvement credit;11/11 originals and all four P6 directions still pass,
while P1 and two peer sources remain mandatory failures. Evidence:
`root-termaid4-req10-first-dual-proof.json`. The complete five-suite assertion
denominator remains unestablished; this narrow port grants no parser/API/options
or whole-suite certification. Formal drawing coverage remains497/2145.

The newer original000 focused batch builds both presets and passes all5 Flow
TESTs. The complete official source (six nodes, six edges, two Decisions and
repeated Yes/No captions) passes wide, narrow and restored full geometry, exact
restoration and all8 meaningful Model/physical negatives, with79,756,258 of the
unchanged128M oracle-work units remaining. Its native proof is not yet an
Official-owned admission. Requirement remains5/7:11 original sources and all
four P6 directions pass; the complete nine-node/ten-edge Peer RL source now
passes geometry and negatives, while P1 and Peer LR still fail mandatory
wide drawings. Four-way production attribution shows that both the SCC normal
selection and parallel-caption spacing are needed for this single peer gain.
No general SCC completion is claimed. All13,776 source pins remain exact and
changed reports match across presets. Evidence:
`root-flow-original000-req11-first-dual-proof-r3.json` and
`root-requirement11-parallel-only12-attribution-proof.json`.
Formal coverage is still497/2145; complete five-suite semantic coverage remains
unestablished. The changes are uncommitted; no shared interface or public
contract was changed by this focused addition.

The following focused results are earlier checkpoints.

The latest focused drawing checkpoints now execute in both Debug and Release.
Flow typed shapes: all3 TESTs pass;9 complete synthetic sources pass five views,
full geometry and60 meaningful negative mutations across20 categories, with no
wide source fallback or resource error. The four-line Decision is a mandatory
drawing; its immutable historical first-red marker earns zero expected-failure
credit. Pie: all3 TESTs pass, all7 full official sources pass geometry and
mutants;15 supplemental controls include5 complete drawings and10 explicitly
closed protocol outcomes. Requirement:5/7 TESTs pass, all11 full official sources
and all4 P6 direction controls pass; P1 and2 overflow peer drawings still fail
as mandatory source fallback. Independent positives are8/9, with zero native
resource/protocol errors. These focused batches conserve13,776 source paths and
produce byte-equal changed reports across presets. Evidence: Root proofs
`root-flow-decision2-mandatory1-first-dual-proof.json`,
`root-pie7-producer-first-dual-proof-r3.json`, and
`root-requirement11-forward-cost9-first-dual-proof.json` under the audit directory.

These are focused native results, not full-suite success. Formal drawing
coverage remains497/2145 (Flow29/400); the new Requirement/Pie/shape Models have
not yet been admitted by the formal whole-source consumers. Full five-suite
atomic semantic assertion coverage remains unestablished. No fallback earns
drawing credit. Full regression, remaining types/assertion ports and real-terminal
graduation remain open. No installation, commit or push was performed.

The following paragraphs retain earlier executed checkpoints.

Requirement matcher R8 is the newest focused trial: both presets build and
pass3/5 TESTs, conserving13,774 source hashes. The new full three-node/two-edge
case and P2 meaningful self-edge mutation pass; original11 remains9/11 and
independent positives improve to5/9. P1 and three P6 failures remain, with zero
resource/protocol errors. The three changed reports match byte-for-byte across
presets. Root proof84e11925 records the actual run. Subsequent LLDB identifies
the other-label adjacency predicate ignoring the physical route arm, without
rewriting reports; that diagnosis does not earn drawing admission. A two-helper
native correction is under independent review. The following full700 and
focused six-TEST counts retain their executed historical meanings.

The latest full producer batch is Requirement R4: both presets build and pass
688/700 native TESTs (12 failures, no GTest errors), preserving all13,766 source
path hashes. A subsequent focused six-TEST batch passes3/6 per preset after
native marker/stroke and canonical-binding corrections. Requirement11 now has
9 complete source/Model/geometry/mutation scenes and2 path-ownership errors;
independent controls retain P1/P2 and three P6 failures. Pie7 has seven fully
bound/decoded mandatory wide fallbacks and a passing independent subcell oracle
control. No source fallback receives drawing credit. Formal admission remains
497/2145 (Flow29/400), and complete five-suite atomic assertion coverage remains
unknown. Evidence: `requirement11-producer-full-seven-dual-r4/RESULT.json` and
`root-requirement7-pie4-native-proof.json` (59dfa914). Earlier Pie R3 protocol
errors retain explicit ERRATA and are not counted as valid first-red evidence.
Flow shape assertions, Requirement ownership/layout repair and Pie producer work
remain in progress. No full100%, whole CTest, terminal screenshot, installation,
commit or push is claimed. All following counts are historical checkpoints.

The current ER Group controlled producer trial has independently reviewed
limited retention. Debug/Release both build and run Native205 (198pass/7
pre-existing completion reds) and public6: Markdown44/44, Flow29/30,
Charts120/121, Sequence264/264, Terminal3/3, Official23/24. All91 Native reports
and the official matrix agree across presets. Original drawing coverage is
476/2145 (22.19%); all62 ER complete Models pass and144 whole-null remain.
Only seven ER originals change; the other199 ER and2731 matrix records are
exact. Twelve full control scenes execute68 semantic/12 orphan/24 Group physical
negatives; all13,651 source path hashes and the unrelated e2e edit are conserved.
No new runtime dependency, public API, configuration or limit is introduced;
changes remain inside the existing ER parser/renderer and test/doc owners.

Evidence: Root proof `7cc14c89bb65d43f6f2865a6e1e14df2973de757425683e0813fccf20974e0e0`,
independent review canonical `d9421bd3e08677eb9de5bf5459896ac950dcdeb32b2af570ada557da76084d24`.
This approves the exact producer trial, not100%, rich-six admission, general
production1M fit or final serial CTest/real-terminal verification. The following
paragraphs retain prior checkpoints and their historical counts.

The ER subgraph TEST-FIRST checkpoint now builds Native202 in both presets and
runs only the new main16-source TEST. It genuinely fails in both: all five
mandatory complete Models remain source fallback, while four malformed sources
and seven valid-but-unimplemented source obligations retain complete ordinary
views and following/restoration protocols. All16 sources publish five actual
UTF8 views before source binding; all five Models decode. No resource, protocol,
geometry, mutation or observation error occurs. The complete new reports match
across presets, all87 previous report files remain exact, and the13,651-file
source freeze is unchanged. The old201 TESTs were not rerun in this focused
batch. Production remains `a968c651`; formal coverage remains469/2,145. Independent first-red review closes with no material findings; semantic and
Group physical negatives have not run because the required good scenes are
absent. The separate resource-boundary TEST and production implementation gates
remain open.
Evidence: `root-er206-subgraph-main16-first-dual-conservation.json` (SHA-256
`ac47f5283e40af58af79eab96b1fe49a548cb0fa6b7ab85f4179991661ccba23`).

The new test-private ER whole-source join builds Debug and Release. Both run
all 24 Official TESTs: 23 pass and the complete capability-matrix gate remains
red. All 206 unchanged originals publish five actual UTF8 row arrays before
source binding; 62 complete Models decode, 55 pass complete geometry, following
roles, restoration and exact source-applicable mutation proofs, seven mandatory
positives fall back, and 144 whole-null obligations remain. Formal original
terminal drawing coverage is now 469/2,145 (21.86%); foreign assertions gain no
credit. Thirteen ER rows retain honest failures: seven mandatory positive
fallbacks and six whole-null candidate drawings without complete semantic
proofs. No resource error occurs. The new good-first control rejects partial
mutation counts, same-total wrong kinds, bad restoration and forged admission.

This joins existing production capabilities to the formal matrix; it adds no
renderer feature, shared-helper, exported API, source/Model, fixture, policy or
limit change. First-run build and complete native logs remain archived. All 2,532 non-ER whole case records and 13 public reports remain exact; all
four Official reports match across presets and the 13,651-file source freeze
is unchanged. Independent actual-retention review closes with no material
findings. Full-suite completion, serial full CTest and real-terminal graduation
remain open. Evidence: `root-er206-stage3-r2-first-dual-conservation.json`
(SHA-256 `5c1d1711957d95bcf926aa9d1710c3334363ab52e94b542545ff2446f070357b`)
and independent review `7706a2ca9ac23a761e35a7a5184a325f28e471bfa4152dc537adca84039c773d`.
Real Terminal UI access was refused by the computer-use tool for safety reasons;
no screenshot or UI-success claim follows from native test results.

The latest lexical-owner ER repair compiles Debug and Release and runs all 201
Relations native TESTs in both: 193 pass and the same eight completion gates
remain red. A single insertion in `er_cardinality` distinguishes a healthy
terminal entity named `1` from a relationship marker; the relationship parser,
marker table, edge fees and layout remain literal. Its genuine unchanged-code
first run records two mandatory positive fallbacks, one complete compatibility
positive and four closed sources, with zero operational or resource errors.
The new TEST's initial nonempty-mutation assertion was rejected before execution;
its versioned correction now requires exact source-applicable kinds/counts.

After the repair all three complete source controls pass five actual views,
whole geometry, following roles, restoration and all applicable negatives; the
two newly good scenes each reject four typed mutations plus one physical orphan.
Original ER case 22 also becomes a complete positive: 55 of 62 approved Models
now pass, seven remain source fallback and 144 retain whole-Model debts. All
206 source/Model/metadata/debt identities are exact, as are the other 205 complete
case records. All 87 reports match across presets; 85 previous complete reports
and all 200 previous statuses are unchanged. The four closed controls and old
cross-line compatibility control remain complete-record exact.

Complete Release public logs retain Markdown 44/44, Flow 29/30, Charts 120/121,
Terminal 3/3 and Official 22/23; Debug retains Flow 29/30 and Official 22/23.
Their existing completion failures remain genuine failures. All other 2,737 Official matrix
rows and 14 public reports are exact; formal coverage remains 414/2,145 (19.30%)
until the ER whole-source join is verified. The private bug fix changes no
public contract, original fixture, Model, raw policy or limit. Independent
actual-retention review closes with no material findings; full completion and
real-terminal graduation remain open. Evidence: `root-er-numeric22-test-first-dual-conservation.json`,
`root-er-numeric22-producer-first-dual-conservation.json` and
`root-er-numeric22-producer-public-statuses.json`.

The subsequent test-private explicit-token join compiles both presets and runs
all 200 Relations native TESTs: 192 pass and the same eight completion gates
remain red.
All 199 previous statuses and all 85 previous complete reports, including work
remaining, are exact; all 86 current reports match across presets. The new full
ER source uses the same spelling for a business header, an attribute and the
independent following paragraph. Its five complete views, geometry, restoration,
19 coherent typed rejections, one physical orphan and five additional physical
negatives pass. Two deliberately unavailable-budget controls are checked only
as resource failures and earn no semantic credit. No verification or resource
error occurs in the positive scene. Every new negative uses its actual caller's
ledger and explicit following token; old independent 8M paths remain literal.
Production and Official execution are unchanged, so formal coverage stays
414/2,145. Independent actual-retention reviews of both the single-self repair
and this shared test-private core close with no material findings. Evidence:
`root-er206-stage2-first-dual-conservation.json`.

The latest source-frozen Debug and Release runs both pass 191 of 199 native
TESTs, with eight mandatory completion failures retained. A private ER layout
fix measures the space needed by a single vertical header self-relation before
placement. Original ER cases 94 and 95 now pass complete geometry and meaningful
adversaries: 54 of 62 complete Models pass, eight still fall back, and 144 sources
retain whole-Model debts. Both independent BT direction controls also pass;
the unchanged producer's genuine long-label failure remains recorded.

All 85 native reports match across presets, 83 previous reports are byteexact,
and the other 204 complete ER case records remain exact. The only Official
matrix changes are the two ER observations; the other 2,736 rows are exact.
Formal admission remains 414/2,145 (19.30%) until an explicit source-bound ER
whole-scene join is verified. No source, Model, raw policy, production limit,
old native budget or public contract changed. The preceding mechanical sharing
of the ER authority preserved all 198 existing TEST bodies, fees and reports.
Evidence: `root-er206-mechanical-stage1-dual-conservation.json` and
`root-er-single-self-room-first-dual-conservation.json`. Full completion,
foreign assertion certification, full ctest and real-terminal screenshots remain
open; no commit, push, installation or Home change has been made.

The first actual ER description producer trial compiles Debug and Release and
runs the complete198 native TESTs in both:188 pass and10 remain red. Four private
ER envelope edits admit strict opaque documentation without changing business
parsing, config schema, Models, original sources, production1M or public APIs.
All84 reports match across presets;81 previous reports are byteexact. All206
sources are captured and bound,62 complete Models decode,52 pass whole-scene
geometry and coherent typed adversaries,10 remain mandatory source fallbacks,
and144 retain complete-Model debts. Native resource/protocol/geometry/mutation/
observation error counts arezero; six unmodeled sources also draw and receive
no credit. Every original source, approved Model and metadata/debt identity is
preserved.

The new physical-line TEST first records the genuine unchanged-producer CRLF
positive failure and two closed control blanks. After the producer it passes
all3 complete sources, five rendered UTF8 row-array publications, exact restore,
complete positive geometry and a meaningful role adversary. The description30
trial reaches all9 good scenes and executes82 typed plus9 extra-ink rejections,
but a genuine test-placement error stops the duplicate-footer negative on one
scene:8 strong,1 verification error,0 resources,2 of4 protocol negatives. A
separate historical logLevel TEST still expects the newly supported exact
`description: Authored description` source to fall back. Both first failures
are retained. Four independently reviewed test-only corrections preserve all
sources, Models and caps; their Release rerun passes190/198 and all description
controls (9 strong/21 closed/82 typed/9 ink/4 protocol,0 errors). The complete
Debug correction rerun matches190/198 and all84 reports are byteexact across
presets. The other83 reports remain exact to the first producer trial; only
control0 changes, and controls1..29 remain complete-dictionary exact. Evidence:
`root-er-description-tests-corrected-dual-conservation.json`.

Public verification passes Markdown44/44 and Terminal3/3; Flow29/30 and
Charts120/121 retain their previous completion failures. Official22/23 still
fails the complete gate. Both complete2738-row matrices agree, all2532 non-ER
rows remain exact, and58 ER rows expose the immutable historical source-only
policy conflict. This is not a reason to rewrite the original raw policy.
Formal admission remains414/2145 (19.30%);52 native positives require an explicit
source-bound Official whole-scene join before gaining formal credit. Foreign
certification, full100%, full ctest and current real-terminal graduation remain
open. No commit, push, install or Home modification. Evidence:
`root-er-description-physical-first-dual-conservation.json` and
`root-er-description-producer-first-dual-conservation.json`.

The earlier ER TEST-FIRST checkpoint preserves a genuine unchanged-producer
failure baseline. Both first builds failed on one explicit JsonVal construction;
that one-line correction preserves closed-row semantics. The following full
Debug/Release runs execute197 TESTs:188 pass, the previous seven completion
gates and two new mandatory drawing gates remain red. All83 reports are
byteexact across presets, and all81 previous reports remain exact. All206 ER
originals are captured before their own binding:39 complete Models remain
source fallbacks and167 retain whole-source debts. The additional30 description
controls bind9 positive fallbacks and21 finite unsupported profiles, with zero
operational/verifier/resource errors. Their geometry and negative paths have no
good scene yet and earn no credit.

A separately source-approved, data-only ER superset then activates62 complete
Models and144 whole-null sources. Both focused ER206 runs pass the integrity
TEST and fail the mandatory drawing TEST:all62 remain source fallback, with zero
operational/geometry/mutation/observation errors. All206 five rendered UTF8 row
frames remain byteexact; all39 old case records differ only in the two declared
lineage fields. The other82 reports remain exact. The full197 suite was not
repeated for this data-only update; it and the relevant public paths are required
after a production repair. Reports persist UTF8 rows, not a new generic per-cell
pixel serialization:ER206 saves five frames and the description controls three.
No producer, cap, original source, approved Model value or public contract
changes; formal coverage remains414/2145 (19.30%) and foreign certificationzero.
Evidence: `root-er206-description30-first-dual-conservation.json` and
`root-er206-config62-focused-first-dual-conservation.json`.

A later test-only main481 join verifies all13 complete Note sources in the
original default8M scene ledger. Full Debug/Release Native193 runs retain186
passes and the same seven mandatory completion failures. Main481 now observes
all481, decodes344 complete Models, retains137 pending sources and passes341
complete geometry/mutation scenes, up13 from328. All13 new scenes include full
physical restoration, following roles and meaningful business/Note negatives;
all error counters remainzero. All old331 complete-Model cases and137 other
cases are exact. All81 reports match across presets;79 old reports are exact
before/after and the standalone Note report has only its declared scope-key
rename. Official coverage remains414/2145 (19.30%), foreign certificationzero.
No production, shared/public contract, Model, source fixture or budget changes.
Evidence: `root-note13-main481-first-dual-conservation.json`. The complete100%
target remains open.

The latest full Debug/Release checkpoint admits414/2145 originals (19.30%).
One owning private Class declaration branch now permits same-owner plain body
reopening, preserving identity, rows and annotation limits. Four complete source
positives and five closed controls pass after the genuine unchanged-producer
first red. Native193 passes186 with the same seven mandatory completion failures;
all81 reports are exact across presets and78 old reports are byteexact. Only
original116 changes in the other two reports: Notes22 reaches16 strong/6 source
fallbacks, and Class285 reaches211 strong/237 complete Models/48 nulls, with26
complete positives still unmet and zero errors. Its complete14-class/10-edge/2-Note
scene passes wide/narrow/restored geometry and meaningful mutants. Only that
original changes in the Official matrix; the other2737 rows, all Flow400 physical
scenes and other public reports remain exact. No Model, policy, cap, backend,
exported API or established public contract changes. Evidence:
`root-class-reopen-producer-first-dual-conservation.json`. Full100%, full ctest,
foreign certification and current real-terminal graduation remain open.

### Preserved413 same-run Flow body diagnostic checkpoint

The subsequent full Official23 Debug/Release run passes22 TESTs, with only
CompleteCapabilityMatrix still mandatory red. Two offset-zero Flow diagnostic
bodies now use the strict same-iteration complete original proof; descriptor,
full source/Model identity, all five captures, geometry, restoration and meaningful
mutants remain required. Their old source-only conflicts are resolved without
editing the frozen manifest. The other2734 matrix rows are exact. All400 complete
scene records preserve their five physical grids and source/Model references;
only the selected two originals have honest prepaid fee/remaining changes. Full
reports are byteexact across presets. Official credit remains413/2145 (19.25%);
diagnostic rows add no originals and foreign certification remainszero. Evidence:
`root-flow-body2-first-dual-conservation.json`.

The latest additional test-first checkpoint runs192 native Relations TESTs in
both Debug and Release:185 pass and the same seven mandatory completion gates
remain red. All13 complete Class481 Note sources now pass five public captures,
source/Model binding, geometry and both business/Note adversaries. There are no
fallbacks, resource, protocol, geometry or mutation errors in this cohort. The
old190 ordered statuses and all79 previous reports are byteexact; all80 reports
are byteexact across presets. No producer change, main481/Official admission or
foreign predicate credit follows automatically. Formal official coverage remains
413/2145 (19.25%). Evidence: `root-note13-native-first-dual-conservation.json`.

### Preserved413 repeated-subgraph production checkpoint

The latest complete Debug and Release checkpoint is413/2145 originals (19.25%).
The private Flow parser now supports completed same-parent/same-title group
reopens with stable member union and preserved business-edge multiplicity.
Seven complete positives and ten closed boundaries pass in both presets.
The true unchanged-producer first run preserves all seven source fallbacks;
the pre-execution review's new-scene budget correction does not change existing
8M tests or production1M. Native Flow passes29/30: all eight complete original
Models pass geometry/mutants and392 whole-null sources remain mandatory pending.
Relations remains183/190 with all79 complete reports exact before/after and
across presets. The Official matrix and full400 scene JSONL are byteexact across
presets. Only two original repeat rows and their two body diagnostic rows change;
the other2734 rows are exact. The two body rows retain genuine old source-only
policy errors in all three states; this checkpoint does not certify them or
rewrite their frozen expectations. All other public reports are unchanged,
including the Terminal baseline. Evidence:
`root-flow-repeat-native-first-dual-conservation.json` and
`root-flow-repeat-producer-first-dual-conservation.json`. No exported API/public
contract changes. Full100%, full ctest, current real-terminal graduation and
foreign certification remain open. No commits, pushes, installs or Home changes.

### Preserved411 Official Flow whole-source join

The previous complete Debug and Release checkpoint is411/2145 originals (19.16%).
The Official Flow400 path reuses the existing private capture/Model/whole
geometry/mutant checker. All400 originals complete five captures before their
own source binding and Model decode;2000 full physical cell grids, raw foreign
manifest expectations and source identities are preserved. Eight Models are
mandatory,392 whole-null profiles remain pending, and six sources
10/47/58/60/61/290 earn admission. Originals1/2 retain positive-unmet errors;
there are zero operational/resource errors. Both presets pass21/22 TESTs with
only the mandatory complete matrix red. Full reports and scene JSONL are
byte-identical across presets; all2338 other original/diagnostic rows and
existing non-matrix reports remain exact. The strict admission reader rejects
a forged bit and a good-wide/bad-restored physical control. No producer,
Model, policy, source fixture, production cap or public contract changes in
this join. Evidence: `root-flow-stage2a-token-dual-conservation.json` and
`root-flow-stage2b-official-dual-conservation.json`. Foreign certification is
still zero;100%, full ctest and current real-terminal graduation remain open.

### Preserved405 routing-storage and Notes checkpoint

The previous complete Debug and Release checkpoint is405/2145 originals (18.88%).
The reviewed two-hunk private port-index storage change reduces the compiled
routing Pair from24 to8 bytes without dropping candidates or altering their
scores/order. All667 previously good-and-mutated report rows remain byteexact;
these are report rows, not unique sources. The complete Class285 gate gains
95/154/155/181/202, reaching210 complete geometry/mutation matches and27 remaining
complete positives. All79 native reports and the full Official matrix are
byte-identical across presets. Native remains183/190 and Official20/21, preserving
their mandatory completion failures. Four terminal baseline inputs now draw but
remain unmodeled/unverified and receive zero admission credit. Notes22 separately
exposes five genuine8M oracle failures while preserving all ten previous strong
sources. A phase-only dual run localizes95/155/181 to its repeated restoration
comparison and154/202 to restored geometry; no exact inner-spend cause or
successful fix is claimed. Independent review permits retaining the production
storage change for its real whole-source gain and old-strong conservation. A
subsequent reviewed Notes22 test fix reuses the already paid predecode full
restored-equality proof: equal unchanged inputs under the same complete
Model/presentation need no second comparison or restored validation. Actual
focused5 and fullNative190 dual runs retain their mandatory completion reds,
with Notes22 now15 strong/7 whole fallbacks and zero errors in every domain.
Other78 native reports remain byteexact, and all first failures remain retained.
Evidence: `root-port-index-trial-native-conservation.json`,
`root-port-index-trial-public-conservation.json` and
`root-notes22-first-resource-substage.json` and
`root-notes22-proof-reuse-dual-conservation.json`. Production1M, native8M, original
sources/Models/policies remain unchanged. Seven positive Notes sources stay mandatory;
full ctest, current terminal screenshots and100% remain open. No commit, push,
install or Home settings change occurs.

The subsequent Flowchart prefix repair passes eight complete profile controls
and44 closed boundaries in Debug and Release. Complete official originals
10/47/58/60/61/290 pass native whole geometry, narrow/restored views and semantic
mutants; repeated-subgraph originals1/2 remain mandatory unmet positives.
Native Flow passes26/27. All79 Relations reports and other public reports remain
byteexact. Only those six original Flow rows change in the Official matrix;
their old source-only expectations still fail, and none receives formal credit
until the same complete checker is joined to the Official matrix. Formal
coverage therefore remains405/2145. A subsequent mechanical extraction into
test-private headers preserves all27 statuses, both complete Flow reports,
frames and work remainders. Evidence:
`root-flow6-producer-first-dual-conservation.json` and
`root-flow-stage1-mechanical-dual-conservation.json`. No public contract changes.

### Preserved400 Notes authority checkpoint

The previous complete Debug and Release checkpoint is400/2145 originals (18.65%).
All18 independently source-approved complete Notes Models join the existing
source authority, yielding237 complete Class Models and48 mandatory whole-null
profiles. Complete geometry and meaningful mutants pass for205 Class sources,
including six new Notes originals34/35/36/78/138/195;32 complete positive sources
still fall back. All285 originals retain their five predecode captures. Relations
passes183/190, with the same seven mandatory completion reds, and all79 complete
reports plus ordered statuses match across presets. Official passes20/21, with
only the complete matrix red and byteexact complete dual matrices. All2453
non-Class rows are unchanged. The old267 Class source outcomes/Models/frames are
conserved; disclosed new lookup charges reduce each native source ledger and
existing postdecode allowance by exactly32 units. All source fixtures remain
unchanged and foreign assertions certified remain zero.

The genuine first dual compilation failed on four moved namespace constants;
Root added the existing local using pattern. The subsequent first complete dual
Official21 run passed19/21 because its manually constructed protocol control
omitted Notes18=false. The one-field fixture repair preserves the strict bool
reader and forged-admission counterexample; fresh complete Official21 runs restore
20/21. These first failures are retained. Independent reviews of the all18 join
and both local fixes have no remaining material finding. Evidence:
`root-notes18-authority-join-dual-conservation.json`. Full ctest, current real-terminal
graduation and100% completion remain open. No commits, pushes, installs or Home
settings changes.

### Current bounded follow-ups

A fresh read-only C59/Native190 Debug observation selects the complete1207B
Notes original95. It records the first actual production insufficient spend:
394,592 units at the stable-counting buffer prepayment, after2640 filtered pairs
were built. The actual caller BL and return PC identify that one expression.
The typed Note-leader flag is true. The subsequent first typed connected=false
is edge/index10 at root owner-1, after six actual true connections. Production
numeric remaining stays UNKNOWN because its DWARF is incomplete. This establishes
one leader capacity failure; it does not establish the other12 causes, later
business-edge success or an intrinsically over1M request. All202 postpins match,
there are zero reader errors, and the entire704732-byte Notes22 report remains
byteexact. A two-hunk private index-storage candidate passed independent review;
no new producer trial or drawing gain is claimed yet. Evidence:
`root-notes95-first-actual-budget-attribution.json`.

The Flow400 source oracle captured source semantics for all400 whole originals and six
controls under pinned Mermaid12.2.0, using the stock pre-draw source seam only.
This maintenance oracle supplies source facts and is not a production renderer.
The all400 mapping retained its genuine FontAwesome debt omission; an independently
reviewed correction adds54 sources/124 label fields without changing any other
field or the three existing Models. The config-only set is43, not47. A separately
reviewed trueDagre5 contract supplies five further complete Models, yielding the
next native test-first target8/392. The reviewed native implementation has now
run against unchanged production in both Debug and Release:24/25 tests pass,
with only the mandatory completion gate failing. All400 whole originals complete
five captures and publications;8 complete Models decode and392 remain pending.
All eight mapped positives return whole-source fallback. Protocol, resource,
geometry and mutation-error counts are zero. The old23 test statuses and complete
Flow report remain exact, and both complete reports are byte-identical across
presets. Evidence: `root-flow400-first-dual-conservation.json`.
The proposed128M budget belongs only to that new whole-scene test measurement;
old visitor8M and production1M are unchanged, and H1040 may still exceed it.
Neither source capture nor a Model alone grants drawing coverage.

### Preserved394 and Notes test-first checkpoint

The previous Debug and Release checkpoint is394/2145 originals (18.37%), including199
complete Class contracts from219 Models;20 complete positives and66 whole-source
profiles remain pending. The unused-definition parser passes11 complete controls
and19 boundaries; its three independently source-approved original Models then
join the existing whole-scene authority. Relations passes181/187 and Official20/21;
the same six mandatory Relations failures and complete Official matrix remain red.
All2453 non-Class matrix rows and previous282 Class semantic/frame outcomes are
preserved; declared source-binding Work charges differ. The subsequent Notes58
batch confirms dual parity of all189 Relations statuses/79 reports and the full
Official21 matrix. It passes182/189 Relations and20/21 Official; the seven
Relations failures include the new mandatory Notes18 completion gate. Four full
public Notes source controls pass geometry and mutants. The subsequent C59
leader-only stable counting sort also passes six complete official Notes sources
(34,35,36,78,138,195), with12 original fallbacks and zero geometry/mutant/schema/
protocol/resource errors. Both presets retain identical189 statuses/79 reports
and the full Official matrix; the five public Release targets retain all prior
statuses and all2453 non-Class matrix rows are unchanged. All18 source-approved
Notes Models await the formal authority join, so the six successes currently earn
no additional official drawing coverage. Evidence:
`root-unused3-authority-join-first-release-conservation.json` and
`root-notes-counting-r2-first-dual-public-conservation.json`.

The genuine test-first setup failures (Grid member names, fixture path and strict
Model profile) were corrected before the clean unchanged-producer baseline; none
is presented as a first-pass success. A later longest-edge-first routing trial
provides no Class20 gain and swaps an existing success for a fallback; it was
withdrawn exactly. Notes18 is a separate in-progress native test-first gate and
grants no original coverage yet. Its genuine first four literal-paper failures
were corrected by expanding five manually authored Class bottoms one row; the
checker/source/Model/negative obligations were unchanged. Corrected4 papers,19
physical negatives,10 Model negatives and7 codec negatives pass. All22 whole
source captures/decodes have zero protocol/schema/resource errors. Evidence:
`root-notes58-first-dual-public-conservation.json`. No commits, pushes or Home
settings changes.

A two-hook read-only Debug189 observation of complete434B Notes original34
captures its first production insufficient-spend branch. The actual call asks
2,881,480 units, with candidate product bound2100, sort levels12 and actual
Pair size24. The exact caller/BL proves the new leader sorting prepayment exceeds
the entire production1M ledger; numeric remaining stays UNKNOWN because its
production Work DWARF is incomplete. This is one leader failure, not proof of
all18 failure causes or an already-built2100-pair vector. Native exits at its
expected positive-completion red and the complete Notes22 report remains
byteexact. All202 postpins match; independent review has no material finding.
The leader-only stable counting sort has since passed independent review and
actual dual-preset validation. It retains every filtered candidate and stable
score ordering, pays the actual product allocation/copies and score-span buckets,
and leaves business routing, physical clearance and all capacity limits intact.
Its first review found an omitted temporary move read in the allocation charge;
R2 accounts for that read before the actual trial. Source C59 produces the six
complete official Notes geometries above; at that checkpoint formal source
admission remained a separate pending gate. The current all18 join above closes
that source gate without selecting by drawing outcomes. Evidence:
`root-notes34-first-actual-budget-attribution.json` and
`root-notes-counting-r2-first-dual-public-conservation.json`.

### Preserved earlier dual-preset checkpoint

StageC-R2 is now an executed dual-preset Official checkpoint:20/21 TESTs pass
in each preset, with only the complete matrix red. Both complete2738-row reports
are byteexact (4,928,272 bytes), and all2453 non-Class rows are exactly unchanged.
The formal native original drawing count is391/2145 (18.23%), comprising the
previous195 plus196 complete Class originals. No source fallback, body diagnostic
or foreign assertion grants this credit. All285 Class originals have five
captures and complete bindings;216 Models decode,196 whole contracts pass,
20 complete positives still fall back, and69 whole-source profiles remain pending.
The four additional unknown-art rows remain errors. All source fixtures remain
unchanged. Evidence: `root-stageC-R2-official21-first-dual-conservation.json`,
the StageC independent `first-actual-release` and `first-actual-debug-parity`
reviews, and the frozen full native logs/XML.

Class originals directly reuse the immutable source authority and shared whole
geometry/presentation/following-role checker. They require no new285-tuple policy
or full golden warehouse. Formal Class coverage requires whole-source admission;
per-state Drawn remains separate. The genuine R1 review retained two local P2s
(wide-only coverage and named-resource classification); R2 closes both and
explicitly pays the restored Frame text copy. Its counterexample preserves a
good wide frame while rejecting malformed restoration and a forged admission bit.
One128M source ledger includes prepaid bounded failure/publication, with no reset
or setup loan. This batch has zero resource errors, maximum height323 and minimum
final remaining60,936,100. Default production1M/native8M remain unchanged; no
general capacity or full-suite completion claim follows.

The subsequent96ae ordinary-Class single-face capacity trial has completed its
first Release184 and Debug184 checkpoints:178/184 with the same six mandatory failures.
One terminal source (`merman/6dc0fe75244155cb80cc`) gains complete geometry and
mutants, taking Class481327→328 and Endpoint2925→26. All old strong cases retain
geometry/mutation success;73/75 reports are byteexact. OfficialClass285 remains
196/216, with its entire report byteexact. Identical fallback frames do not prove
the candidate's internal layout or failed branch. Debug matches all184 ordered
statuses and all75 complete report bytes. The five public Release targets retain
their previous statuses, and the complete Official matrix is byteexact against
the StageC391 baseline. The changed Class481 report contains one new strong row,
four previously strong rows with valid changed geometry, and one still-pending
row with changed frames; the last row earns no geometry credit. Independent
runtime review approved retaining this finite improvement with no material
findings; the six mandatory completion failures remain. Evidence:
`root-class-cardinality-single-face-first-release-conservation.json` and
`root-single-face96-first-dual-public-conservation.json` and
`class-cardinality-single-face96-independent-runtime-generation/review.json`.

### Preserved pre-StageC checkpoints

The producer remains17f9 after withdrawing the unsuccessful21eb trial by its
six exact inverse edits. The latest local test integration is StageB-R3:
Release Relations178/184, with the original six mandatory completion gates
still red; Official19/20, with only the existing complete matrix red. Its two
new Official controls and new complete following-role native control pass.
Debug verification also returns178/184 Relations and19/20 Official. Every184
Relations status and75 report bytes match Release; all20 Official statuses, the
complete matrix and the compiled-size/per-source ledger properties also match.
Evidence: `root-stageB-R3-native184-first-dual-parity.json` and
`root-stageB-R3-official20-first-dual-parity.json`.

The genuine StageB-R2 first184 run is177/184. A dependent old restoration
negative expected a global token count from the new physical last-row locator.
The reviewed R3 keeps all five original corruptions and restoration rejection,
requires the unchanged last footer spelling for the duplicate-row case, and
explicitly requires the complete Model to reject the extra earlier token.
Only that old TEST changes to pass; all75 complete reports are byteexact.
Against the unchanged183 baseline,47 reports have1457 leaf changes, all
verification work or allowance fields; every original full Model, physical
frame, outcome and error leaf is unchanged. The four full Class controls and
one finite Source control share one8M ledger per source through five actual
captures and all checks. Measured compiled sizes are Pixel40, PixelRow24 and
string24 bytes; heights are8/8/20/20/6, with final remaining2.01M or more.
This is a bounded test-protocol correction, not original Class admission.
Evidence: `root-stageB-R2-native184-official20-genuine-first-release.json`,
`root-stageB-R3-native184-first-release-conservation.json` and
`root-stageB-R2-official20-first-properties.json`.

The earlier producer17f9 checkpoint has181 Relations TESTs.
Release176/181 and Debug176/181 have identical ordered statuses and all73 fresh
complete reports. Five mandatory source-completion failures remain. The prior
producer160777 mechanical180 checkpoint remains173/180 with72 exact dual-preset
reports; its two namespace-positive completion failures now pass alongside the
new alias test. The preceding65-member complete source passes
geometry, resize/restoration, following-text and meaningful mutation checks;
all69 preceding reports and176 ordered statuses remain exact. No private vector
capacity claim is inferred from the public control. Evidence:
`root-class-member65-native177-first-release-conservation.json` and
`root-class-member65-native177-first-dual-parity.json`.

A genuine unchanged17f9 Release183 first-red preserves every old181 status and
all73 reports. Its new independent reciprocal/mirror papers pass all six
physical negatives; eight of nine complete state sources pass geometry and
mutants, with unchanged full279-byte B91 still mandatory fallback. Release is
177/183. The first reviewed21eb prospective reverse-port witness trial returns
the same177/183 and all75 reports byteexact, with no observed coverage gain or
regression. The candidate is not accepted as a fix; actual probe execution and
remaining failure cause still require observation. No budget-fit conclusion
follows from unchanged frames. Evidence:
`root-state-narrow-reverse-native183-R2-genuine-first-red.json` and
`root-state-narrow-reverse-producer21eb-native183-first-trial-conservation.json`.

The independently source-approved26-case classifier supplement retains the
historical152 complete/133 null authority and38 Dagre bindings. Current Class285
profiles are216 complete/69 unresolved;196 pass complete geometry and meaningful
mutants,20 complete positives still fall back, and geometry/resource/observation
errors are zero. All previous166 strong identities and all285 complete frames
are preserved. Four unchanged historical classifier sources now have mandatory
full positive tests; the old negative loop retains its other eight sources.
The genuine first176 build exposed a string_view concatenation compile error,
retained separately before its corrected first execution. Evidence:
`root-classifier26-legacy4-native176-first-release-conservation.json`. The17
dynamic harness reports differ from175 only in source-binding work remainder;
their leaf differences are retained in
`root-classifier26-native176-dynamic17-report-leaf-diff.json`.

At the earlier175 checkpoint, all19 complete generic/classifier controls passed
full geometry and meaningful mutants, and all9 closed profiles retained source.
Its old Grammar35 negative suite correctly exposed four newly supported sources
before the separately reviewed positive migration above. The mistaken textual
LF integration compiler failure and genuine first native failure remain archived.

Before that producer trial, the native harness correction and its physical
multiline R2 pass all17 new regression TESTs. Complete Release173 is168/173;
the original five completion gates remain red. The first173 run166/173 remains
separate: two manual bad-Model controls exposed a real SPACE-dependent branch
escape. R2 requires exact physical centering for every source-authored multiline
Class label, so deleting all edge SPACE cannot recover the legacy trimmed path.
All281 previously published three-frame sets remain exact; four sources formerly
blocked by the erroneous publication fee now publish complete frames. Class285
strong geometry/mutations rises110 to158, with47 harness errors reduced to0.
Evidence: `root-class285-harness-native173-first-and-r2-conservation.json`.

The generic producer then raises the same Class285 strong count158 to166,
retaining all previous strong identities, eliminating its eight member-generic
geometry mismatches and preserving all190 complete/95 unresolved source profiles.
Wide source fallbacks fall115 to89. All original327 Class481 strong source
identities are preserved, including two source-bound whole-Model revisions for
the corrected generic display; Class481 remains150 whole-null and four positive
fallbacks. The new fees include every actual Class member-vector allocation,
relocation and insertion, including a plain member after classified members.
The rejected producer375b P2 is preserved; only independently corrected R2 ran.
Evidence: `root-class-member-generic-producer-native175-first-release-conservation.json`
and `root-class-member-generic-producer-native175-first-dual-parity.json`.

Public160777 Release replay preserves all five suite statuses: Markdown44/44,
Flow23/23, Charts120/121, Terminal3/3 and Official17/18. Eleven of13 reports stay
exact; the terminal unverified baseline and official matrix expose additional
Class drawings without granting admission. Strict originals remain195/2145
(9.09%): at240 columns1754 source fallbacks and196 unadmitted-drawing errors
remain. All original fixture files are unchanged. Evidence:
`root-class-member-generic160-native175-public-conservation.json`.

Remaining Class positives include four known hierarchicalNamespaces:false
metadata barriers and20 cardinality scenes. The cardinality representative's
first terminal production failure is now localized below; its precise internal
guard and the other19 sources remain unresolved. The independent flat-namespace source design now has a genuine
unchanged-producer first run: native179 Release173/179, with all37 new inputs
observed,17 Models decoded, one old supported control strong,16 positive
fallbacks and20 closed profiles. A separate CRLF/bare-CR addition produces
native180 Release173/180; its CRLF positive remains red and its whole bare-CR
boundary remains intact. All earlier statuses and reports are exact at each
step. Evidence: `root-class-hierarchy-native179-genuine-test-first.json` and
`root-class-hierarchy-CR-native180-genuine-test-first.json`.

Independent review rejected the proposed flat-namespace producer's incomplete
alias STR consumption before any producer trial. The replacement source design
fixes this common private Class namespace grammar directly, including delimiter
overlap, and adds malformed whole inputs for false/true/omitted modes. It does
not alter valid default namespace layout or erase the rejected candidate history.

The separately reviewed mechanical oracle extraction adds two test-private
headers and retains every existing checker body,193 work expressions, source
owner and TEST registration. Complete Release180 and Debug180 remain173/180,
with all72 reports, including work_remaining, byte-identical across presets and
to the pre-move Release baseline. All five public Release binaries preserve their
ordered statuses and13 complete reports. The affected Official Debug binary also
preserves all18 ordered statuses and its complete report against Release. The
other four public Debug binaries were built but not rerun for this test-private
move. No new Official Class consumer, following-token adapter, policy or admission
exists. Evidence: `root-oracle-mechanical-A-native180-first-release-conservation.json`,
`root-oracle-mechanical-A-native180-first-dual-parity.json`,
`root-oracle-mechanical-A-native180-first-public-release-conservation.json` and
`root-oracle-mechanical-A-native180-official-dual-parity.json`.

The next genuine unchanged-producer Release181 run passes173/181: all180 old
statuses and72 full reports remain exact, while three of the nine malformed
alias sources are incorrectly drawn. The independently reviewed final R3 fixes
the common private Class alias owner and adds source-owned flat namespace
projection. Its first complete Release181 passes176/181. All17 complete namespace
controls,20 finite namespace boundaries, the CRLF positive, bare-CR boundary and
all9 malformed aliases pass with zero geometry/resource/protocol errors.
Sixty-nine of73 complete reports remain byte-exact. In Class285 only exact
originals111/176/191/223 change, each from mandatory positive fallback to complete
geometry and meaningful mutants; all192 previous strong rows and frames remain
exact. Current native Class evidence is196/216 complete Models,20 positive
fallbacks and69 unresolved whole sources. Formal admission remains195/2145.
The original five mandatory completion failures remain. Complete Debug181 also
passes176/181, with all181 ordered statuses and73 complete report bytes exact
against Release. Complete five-executable public Release replay preserves every
ordered status and12 of13 complete reports; only four Official original Class
rows change. Strict admission stays195/2145, with1750 source fallbacks and200
unadmitted drawing errors at240 columns. All original fixtures remain exact.
Evidence:
`root-class-alias-native181-genuine-test-first.json`,
`root-class-hierarchy-alias-R3-native181-first-release-conservation.json` and
`root-class-hierarchy-alias-R3-native181-first-dual-parity.json` and
`root-class-hierarchy-alias-R3-native181-public-release-conservation.json`, with
all changed report leaves retained separately. Rejected R2 was never installed.

The first Cardinality20 production observation now binds exact original38's
995 bytes to the independently observed identical factory payload. Twelve actual
read-only events prove parse12 entities/eight edges, completed68x193 layout and
all entity paintings, followed by six successful root edges1/2/4/0/3/5 and
edge7 Class08-to-C2 returning false. The factory line optional is then disengaged.
The full original285 TEST exits1 and its entire report remains byte-exact; all216
stable context pins remain exact. Only this original is observed, not all20.
A separately reviewed exact production Work::spend insufficient pre-zero PC
probe repeats the whole285 TEST with224 stable pins and full report preserved.
It observes zero selected insufficient-branch hits. Typed Work remains unknown;
this does not certify budget sufficiency or exclude proactive guards. Ports,
caption placements and the exact inner rejection still require bounded evidence.
Evidence: `root-class-cardinality20-debug181-first-actual.json` and
`root-class-cardinality20-debug181-insufficient-probe-first-actual.json`, with
independent actual and branch/control-flow review in the diagnostic directory.

The first read-only B91 State observation preserves the full85 TEST failure and
its complete report bytes. All885 endpoint pairs are rejected by the existing
region prefilter before any A*/label attempt. Independent recalculation of the
actual committed paths/label padding proves two small blocked pockets covering
all five source ports, without assuming a Canvas width. Canvas pixels and
production Work remain unobservable; no cap/order/routing fix is inferred.
Evidence: `root-cjk-b91-debug177-painted-route-first-actual.json` and its
`independent-first-actual-review-oracle` source/observation proof.

Complete upstream100%, remaining whole-source mappings, fullctest and final
real-terminal screenshots remain open. No commit, push, installation, Home write
or established public contract change.


The latest controlled Release156 checkpoint passes151/156. Its private Class
annotation delimiter fix preserves the inner source text while accepting one
complete encoded `&lt;&lt;...&gt;&gt;` pair. All12 complete positive controls pass
geometry and meaningful mutants; all11 closed boundaries retain complete source.
The original285 Class cohort gains24 strong scenes (61 to85), with no loss of an
existing strong scene. Previously hidden geometry/resource failures are reported
as failures when their sources start drawing; fallback reduction is not credit.
Evidence: `root-class-encoded-annotation-producer-native156-first-release-conservation.json`
(SHA431d659ea4366836c925ea8a69f8f8b4887d937154c1c002b683279f9c011208).

An independently source-reviewed38-row Dagre-only whole-Model supplement now
executes in the same285 mandatory source cohort. The historical152 complete/133
null authority is immutable; current semantic profiles are190 complete/95 null.
The separate integrity test consumes all38 bindings exactly once. The scene test
captures all285 originals and decodes all190 complete Models:110 pass complete
geometry and meaningful mutants,115 wide frames retain source,47 harness/resource
errors and13 geometry errors remain. Four unresolved sources exhaust publication
before binding, so91 pending plus those four errors remains95 unresolved, rather
than a four-case mapping gain. These counts overlap by source disposition and
must not be summed into a completion percentage. No original foreign predicate
or full official source admission is claimed. All156 ordered TEST statuses and49
other complete reports remain exact against the preceding encoded checkpoint.
Evidence: `root-class285-dagre38-native156-first-release-conservation.json`
(SHA2665728f4b4909b733182f0876d9a2b6e91c56193e39f3a1885255e8f1d7e5d7).

The complete Debug156 run also passes151/156 (180,310ms), with all156 ordered
statuses and all50 fresh complete reports byte-exact against Release. Evidence:
`root-class285-dagre38-native156-first-dual-parity.json`. The five completion
aggregates remain genuine failures in both presets.

The encoded producer also completes the five public Release suites:
Markdown44/44, Flow23/23, Charts120/121 (unchanged XY102 completion failure),
Terminal3/3 over2916 sources, and Official17/18 over2145 original files plus593
diagnostic bodies. All ordered statuses and12 of13 reports remain exact. Only62
original Class matrix rows change; strict official admission remains195/2145
(9.09%), with new drawings retaining explicit source-contract errors. Original
fixture bytes remain unchanged. Evidence:
`root-class-encoded-c23-native156-public-conservation.json`.

Remaining work includes the reviewed native-harness corrections, Class member
generic normalization with complete source-bound Models, the remaining original
positive fallbacks and unsupported whole-source profiles, broader regressions,
and final real-terminal screenshots. Full upstream100% remains open. No commit,
push, installation, Home configuration, public contract or shared production
module change is made by these two checkpoints.


The controlled2b5 producer also passes the broader Release replay:
Markdown44/44, Flow23/23, Charts120/121 with its unchanged XY pending failure,
and Terminal3/3. All11 chart/flow reports stay exact. Exactly one of2916 whole
terminal rows changes since1ae:6eba becomes unverified drawing;2915 rows remain
exact. Current baseline counts are2131 unverified drawings and785 source
fallbacks. This producer observation still does not close its native geometry
debt. Evidence:
`full-obligation-audit/root-class-flood2b5-public-regression-release-conservation.json`.

A reviewed Class-only repeated-flood optimization is now under controlled
trial: the complete6eba LF/CRLF graph draws at wide/narrow/restored widths, but
native full geometry exhausts the remaining assertion budget and remains an
error. No complete-source coverage gain is credited. All289 existing strong
Class and24 endpoint records are exact;19 of22 reports remain byte-identical.
The scope does not change any1M production/8M native cap or retry a failure.
Evidence: `full-obligation-audit/root-class-flood113-first-release-conservation.json`.

The subsequent complete Release115 and Debug115 each pass110/115, with all24
fresh report bytes and ordered statuses equal. Release115 preserves all113 old
statuses and22 old reports. The original-oracle presentation test-first adds two genuine failures:
13 of16 complete goods pass; the three full title scenes expose unclaimed title
ink. Of106 paired bad frames,78 execute after their good baseline,63 reject,
15 wrongly pass, and28 remain uncredited because their mandatory good failed.
All source/schema/resource counters are zero. Original paper bytes and Models
remain immutable; no optional presentation checker or public22 admission exists
yet. Evidence: `full-obligation-audit/root-envelope115-stage2-genuine-first-release.json` and
`root-class-flood115-full-dual-parity.json`.

The current endpoint1ae public Release regression executes Markdown44/44,
Flow23/23, Charts120/121 and Terminal3/3. All11 chart/flow report bytes equal the
previous native262 baseline. The chart failure still exposes20 pending XY
profiles and108 foreign predicate applications. All2916 terminal inputs execute;
exactly25 Class rows move from source fallback to unverified drawing, while2891
whole rows remain exact. The baseline now has2130 unverified drawings and786
source fallbacks with no execution/resize errors. These are execution outcomes,
not an overall semantic coverage score. Evidence:
`full-obligation-audit/root-endpoint1ae-public-regression-release-conservation.json`.

The source-only Envelope22 appendix now passes both new TESTs in complete
Release113 (110 pass/3 unchanged completion failures) and focused Debug2/2.
All old111 ordered statuses and21 report bytes stay exact; the new source report
is byte-identical across presets. It binds all22 sources,24 physical envelopes,
64 authored scalar leaves,4157 primary fields and127/66/72 body entities/edges/
members. Minimum per-source native Work remainder is6121129. Geometry and whole
source admission remain explicitly zero; the original192 null entries remain.
Evidence: `full-obligation-audit/root-envelope113-stage1-first-release-conservation.json`
and `root-envelope113-stage1-focused-debug-parity.json`. Complete113 Debug has
not run; the preceding complete111 dual checkpoint is retained below.

The current source-preserving endpoint oracle and corrected target paper pass
108 of111 complete Relations TESTs in both presets. Ordered statuses and all21
fresh report bytes match; previous report bytes remain exact, including289 strong Class scenes and24 strong
endpoint scenes. The three mandatory source-completion gates remain red:
Class481 retains192 original null Models; Endpoint29 retains5 whole positive
fallbacks; the six physical-angle positives retain2 complete-source fallbacks.
Four tiny angle scenes now pass complete geometry and mutations, and all15
closed physical-source controls remain unchanged. Tiny scenes do not certify
the full427/446-byte LF/CRLF source. Evidence:
`full-obligation-audit/root-endpoint111-target-paper-r1-release-conservation.json`
and `full-obligation-audit/root-endpoint111-target-paper-r1-dual-parity.json`.

The source-only ordinary caption capacity trial was rejected after its first
complete Release execution regressed an existing strong scene without closing
any positive fallback. Its exact producer hunk was reversed before subsequent
runs; the original failure is retained. Single endpoint ownership now uses
strict directed-contour nearness and complete independent good/bad papers.
The symmetric same-node/same-marker owner swap is classified as observationally
equivalent only under that exact condition; unequal nonself/null cases still
execute the owner mutation. A historical target good paper was corrected after
the strengthened check revealed its caption on the source half. No source or
Model was changed. Remaining route failures are under direct read-only Debug
observation; an unobservable Work field is not a budget diagnosis.

The reviewed original193 sequence admission now executes all15 official TESTs
in both presets:14 pass and the mandatory matrix remains red for22 independent
body diagnostics. All2738 report bytes and ordered statuses match across presets.
Exactly193 original whole-source sequence rows change from the preserved first215
expectation conflicts; the other2545 complete row dictionaries remain exact.
All193 wide/restored drawings pass; narrow40 draws30 and preserves full source
for163. No original manifest/snapshot/predicate is rewritten, and no source
fallback is counted as drawing. The first callback compile failure and the
first missing-known-par_over summary failure remain recorded. The complete
source/private-summary repair preserves nonbranching par_over dividers. Evidence:
`full-obligation-audit/root-official-raw193-r2-first-dual-parity.json`.

The first endpoint107 production trial (Release only) decodes all29 complete
Models and records23 whole-scene/mutant matches,5 positive fallbacks and1
symmetric-self owner-swap mutation classification failure. All289 prior strong
Class row dictionaries remain exact. The first native107 test-only trial is
retained separately (105/107; all29 positive source fallbacks); no new source
projection was silently given fallback credit. Two former unsupported single
endpoint-text controls now draw and await source-preserving positive migration.
Remaining ordinary caption row capacity and physical-angle source ownership are
under separate concrete review; no full-suite graduation is claimed. Evidence:
`full-obligation-audit/root-endpoint107-producer-r1-first-release-observed.json`.

The counts below are separate obligations, never an overall compatibility score.
The current sequence262 Release first native execution passes all262 TESTs
with28 fresh reports. Config110 now observes/decodes/matches all110 complete
raw sources, retaining the original109 Models and immutable null provenance via
the reviewed supplementary Model. Union24 now has15 complete terminal positives
and9 retained source policies; the original Go EOF error predicate remains an
explicit foreign API debt. All109 other Config110 report rows and23 other Union24
rows remain exact, as do26 other complete reports. One constness error in the new
move-only test-asset loader was repaired before native execution; its failed
build log is retained. Complete combined Debug262 also passes262/262; all28 fresh report bytes and ordered TEST statuses equal Release.

The latest native-only oracle repair106 records104 pass/2 fail in both presets
with identical ordered statuses and19 fresh report bytes. All five whole
row-band scenes and the distinct-role shared-cell good/bad gate pass. The
original two source completion gates remain red; no producer drawing gain is
claimed. Complete evidence is
`full-obligation-audit/root-endpoint106-oracle-r3-first-dual-parity.json`.

The earlier Release105 test-first execution records101 pass/4 fail: three new
whole row-band scenes pass, while the two unequal-height good scenes reveal a
multi-gap prefix-candidate omission in the unchanged geometry oracle. All old100
TEST statuses and19 report bytes are exact; this is no producer coverage gain.
The real first-red is preserved in
`full-obligation-audit/root-endpoint105-papers-first-release-conservation.json`.

The last combined relations100 first passes record98 pass/2 fail in both Debug and
Release, with19 byte-identical fresh reports. All289 complete Class Models match
and pass mutations;192 original null obligations remain. Newly imported endpoint
Models add28 mandatory public positives that are still unmet plus one separate
projection debt; no new drawing credit is granted. The Group-caption ownership
false positive has a genuine failing control run followed by the corrected
passing run. The full official/terminal corpus and original assertion coverage
remain incomplete; these counts do not establish overall100% coverage.

Earlier checkpoints below retain the source and gate history.

The complete native sequence250 checkpoint passes249/250 TESTs in both Debug and
Release, with26 byte-identical reports. Its terminal683 projection checks681
complete Models (679 drawings plus2 valid empty scenes), plus2 separately checked
non-drawing source-preservation dispositions. Whole280 checks278 whole Models
plus those same2 dispositions; neither source policy certifies a typed parser
error API or observed production-cap branch. Original foreign assertion mappings
remain open.

Complete relations96 passes93/96 TESTs in both presets, with18 byte-identical
reports. The subsequent Class-label spacing-only checkpoint retains93/96 TESTs
in both presets and18 byte-identical reports: Class481 has284/289 whole Model
geometries,5 positive fallbacks and192 explicit null Models. Three prior
positive fallbacks now satisfy complete geometry and mutations, without old
strong regressions. Four remaining flat sources have complete read-only traces
showing disjoint port components after earlier paths and label reservations;
the namespace source still exhausts its original A* budget. A namespace cache
and region trial were reverted; an unguarded Class rank trial was also reverted
because it regressed an existing strong positive. No expected fallback, source,
Model or production cap was changed to close these gates.

The optional Class rank-fit checkpoint then records93/96 TESTs in both Debug
and Release, with18 byte-identical fresh reports. Class481 rises to286/289 whole
Model geometries, with3 mandatory positive fallbacks and the same192 explicit
nulls. Both new gains pass complete geometry and mutations; no old strong source
regresses. The pretranslation fit check keeps an overwide Class rank on the
existing linear plan. No cap, source, Model or golden was refreshed to obtain
these gains. The Class-only historical pixel restriction now uses its existing
whole Model under the reviewed rank behavior; the old tuple remains in history.

The subsequent reciprocal and namespace-entry Release96 checkpoints raise
Class481 geometry first to288/289, then289/289, without old strong regressions.
The latest complete Release96 result is95 pass/1 fail: all289 reviewed whole
Models pass geometry and mutations, with zero oracle/resource/geometry/
observation errors. The192 unmodeled whole-source obligations still keep the
full481 completion gate red. Combined Debug parity remains pending. This is100%
of the existing289-Model subset, not full Class481 or overall suite coverage.

The Config38 test-first checkpoint initially recorded0/38 public geometries.
After the independently reviewed private producer change, all38 complete raw
Models and their geometry/mutation checks pass, alongside the7 independently
authored papers and the CSS/typed-option boundary controls. Full Release256
records252 pass/4 fail: the original lifetime-null gate and3 older raw-fallback
expectations. The latter10 whole sources now draw; a separately reviewed strong
Model migration replaces those stale expectations without changing their source.
The109 previously modeled Config110 sources all pass whole-Model geometry.

The subsequent complete Debug/Release259 checkpoints each record256 pass/3 fail
with28 byte-identical fresh reports and identical ordered TEST statuses.
All27 earlier Release report bytes remain exact, and only the3 deliberately
migrated legacy TEST statuses change to passing. The remaining failures are the
old lifetime-null completion gate and2 new lifetime-supplement gates. Focused
Debug runs all3 supplement TESTs, with1 pass/2 fail and a byte-identical new
report. These are first failures,
not a completion claim or a successful rerun of the full suite.

The lifetime source audit identifies two different upstream contracts: Mermaid
SVG attaches lifecycle events using compatibility-map raw indices, whereas
Merman's terminal renderer consumes resolved per-actor lifecycle sidecars.
Three already modeled R24 sources legitimately differ between these domains.
Repeated actor declarations also update roles differently. The independently
reviewed fixed contract retains primary Mermaid actor/alias precedence and uses
resolved-business lifetime attachment as a terminal adaptation. Existing R24
Models remain valid; the new lifetime supplement requires versioned reconciliation
and native gates under that same contract before producer changes. No
fixture-dependent semantics, positive-to-fallback reclassification or silent
Model replacement is permitted.

The fixed terminal-field v2 test-first checkpoint adds one TEST (260 total)
and reconciles twelve existing expectations against the independently reviewed
actor/alias and resolved-lifetime contract. Full Release records254 pass/6 fail;
all28 report bytes remain identical to the prior259 checkpoint. Exactly two
old TESTs change from pass to fail because they now require the source-backed
positive behavior, and the new declaration-control TEST fails as intended.
Focused Debug runs all4 lifetime supplement TESTs, with1 pass/3 fail and a
byte-identical supplement report. All five independent complete hand scenes and
their corruption checks pass; unresolved public declaration/EOF/original-source
failures remain visible. No producer repair has been admitted at this checkpoint.

The created-role local-gap producer plus native261 wrap-replacement checkpoint
records259 pass/2 fail in complete Release. The entire original8ab ten-actor,
eight-Create/eight-Message source now passes its reviewed whole Model and all
mutations under the unchanged240-column cap. Three new complete wrap-replacement
controls pass. No earlier260 passing TEST loses its status. The remaining gates
are the deliberately unjoined original110 inventory and the historical Go
EOF-destroy source policy; both need explicit native source-profile integration,
with original facts/API witnesses preserved. Current combined Debug graduation
is still pending.

The first complete official Release matrix after native262 executes all2,738
records (2,145 original files and593 body diagnostics), with215 real historical
expectation conflicts:193 original sequence and22 diagnostic sequence. Its twelve
TESTs record11 pass/1 fail. All193 original sequence sources now have separate
same-source complete native geometry/mutation evidence, but the official matrix
still declares historical source fallback for them; reviewed source-bound policy
and snapshot admission remains required. The complete first-red audit is
`full-obligation-audit/root-official-after-native262-first-release-full-audit.json`. The older approved snapshot counts below are historical policy
checkpoints, not the current renderer observation or full-suite graduation.
Newly drawn complete sources require independent semantics/assertion mapping and
explicit snapshot/policy admission; raw drawings alone do not authorize a policy
rewrite. Exact stage logs, hashes and conservation audits are linked from the
terminal test guide. Complete official/terminal source coverage, original
assertion translation, other diagram families and current real-PTY graduation
remain unfinished. No100% completion claim is justified.

## What 100% means

Complete input execution, semantic compatibility, original-assertion translation
and policy coverage are independent denominators. All must be closed before a
complete-suite claim. A green baseline, a nonempty output, a recognized header,
an imported model or a returned source fence closes none of the missing drawing
semantics.

1. Every frozen source record remains in the ledger. Execute duplicate exact
   UTF-8 sources once if useful, but retain every vendor assertion, profile and
   provenance variant. Never normalize whitespace to shrink the denominator.
2. Every applicable positive has independently declared semantic facts and
   executable native cell predicates. All labels, values, owners, endpoints,
   markers, styles, order and hierarchy required by its source/test contract
   must survive. An in-policy positive returning source fails this obligation.
3. Every original assertion and conditional/parameter branch has a durable
   mapping to a native predicate, a source-backed correction, a precise reviewed
   terminal-profile equivalent, or a justified private/nonterminal exclusion.
   Keeping the source text or recording a category is not that mapping.
4. Invalid, valid-empty, cancellation, resource and foreign API/profile cases
   have their own independently reviewed outcomes. Valid-empty output is not
   nonempty drawing coverage. Resource fallback needs an independently measured
   bound; current renderer failure is not evidence of exceeding that bound.
5. Native oracle mutations reject semantic corruption. Both build presets,
   complete native suites and the real PTY path must verify the same frozen
   production and fact versions. Skips, disabled tests, errors, missing models,
   unresolved assertions and policy exclusions remain separately visible.

There is no single honest assertion percentage yet: many imported references
identify an entire function containing several assertions and conditional
branches. Their individual applicable branch denominator is not fully
translated. Do not manufacture a percentage from source-function counts.

## Frozen source denominators

The terminal root inventory SHA-256 is
`7c842c6c5f429d58884ebe290027369b1a1aad6f1f041c543bc34cab47c0fbce`.
It locks 8,370 assets. The initial C2 official manifest SHA-256 was
`e6e51589303fe19bdd564fc2aa554a081ad7aa909f480a0f72199e23d1f26276`.
The source audit JSON retains that historical measurement. The current normally
executed dual-preset official manifest SHA-256 is
`c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b`.
The preceding box checkpoint remains recorded at
`be08f24ad7f570ae9fa40f87dd8b92476748ee0cbfaa216babb2d0e5515b557d`.
The prior activation-only manifest remains recorded at
`38c8a892799a66eaa7d5f7c8a6566c38cf213eb3ffdb1f8e0ed8198360dd9e8f`.
The checkpoints below retain the four, eight, three, eight and fifteen diagnostic
expansions and the first complete-original native-profile drawing without
rewriting the original files or the C2 history.

| Terminal project | Original records | Pin |
|---|---:|---|
| Beautiful Mermaid 1.1.3 | 128 | `65f4e0ab8c289e9691a9e5bc4f9eac7108cf449b` |
| mermaid-ascii 1.6.1 | 299 | `5f00e3d9ac9fc96a19859d502333e35b87d2ffea` |
| Termaid 0.9.1 | 639 | `a10dfd506ec67dc11fce3f399c20a353202586ae` |
| Mmdflux 2.6.1 | 292 | `c7405b64865938b2df4d33d26389d51691409913` |
| Merman 0.9.1 | 1,632 | `f6202f702dbde0a75049b1a62b3124feae709b3d` |
| Total | 2,990 | 2,916 distinct exact UTF-8 inputs |

The terminal union has 23 named families and six unknown-input identities.
Original classification fields contain 2,946 `drawing`, 18 `structured_text`,
four `fallback`, nine `error`, twelve `robustness` and one `private_or_parser`
records. These are preserved upstream/import declarations, not final native
denominators: valid-empty and source/model conflicts below still need independent
reconciliation.

The official corpus is a **separate** 2,145-original-file denominator under
Mermaid 12.2.0 commit `1fb7869866dd619168d840291c9c54b4f129758b`.
Its 593 exact body suffix projections are a second diagnostic denominator,
not 593 additional originals. Original files span 37 directories, including
types absent from the terminal union. Passing all terminal families cannot
establish full official-corpus coverage.

The current official matrix records **1/2,145 complete-original native-profile
drawings**, with 2,144 original source fallbacks. The first raw positive preserves
its entire XY source, title, numeric axes and two named series while mapping its
authored 4000-pixel width to bounded native terminal measurement. This does not
establish SVG pixel equality or original browser-assertion completion. At40,
that original also preserves complete source; restoring240 draws it again.
Supporting further originals still needs independently reviewed metadata and
body semantics. Ignoring metadata or reusing a stripped suffix is insufficient.
The normally verified593-body checkpoint has83 reviewed drawing records and510
source records at240 columns, including47 of193 sequence bodies. At40,35 draw
and558 preserve source; restoring the same Node to240 returns to83 drawings and
510 source cases. The current reviewed lifecycle matrix hashes to
`7f557a6fd88a62b90d22fb0e26b2a29a7ad9c71f5b0262ab7bceccb1ca04d137`,
with zero errors in all three states. Normal debug/release official tests each
pass4/4 with byte-identical matrix reports. The prior82-body box matrix remains
at `27e5111444f5c2cf3a88f60740379f829c746529b9cc9933dfec22da39190547`.
The prior81-body activation report
remains at `0ac0961ba6d78d0ae05eb1c6e6ff98daedbf465bf4b121873cf0ce812a510d2e`.
The prior78-body checkpoint retains
its byte-identical dual-preset reports
at `b79b80545c4398952de068caff9a5fa01d93cc4aecb9bdd50451878353bcd3e3`.
Unsupported witnesses remain useful backlog categories, not permission to call
the511 syntax examples compatible.
Original and diagnostic denominators remain independent; neither currently
establishes complete official compatibility.

### Reviewed official sequence checkpoint, 2026-10-20

The initial C2 checkpoint had 40 reviewed body drawings, 553 source cases and
40 snapshots. The first sequence checkpoint added four body records representing three
distinct exact sources. Independent source facts and the native
`MermaidSequenceSemantics.FrozenOfficialSequenceExpansion` cell oracle precede
the four reviewed snapshots:

| Body source | Additional drawing obligations | Snapshot |
|---|---|---|
| `sequence/nightly-check/should-handle-empty-lines.mmd` | Preserve the trailing blank row after `Hello John`, the middle blank row in `Great` / `day!`, both directions and solid/dashed shafts | `dc14cb03bd6d978c.txt` |
| `sequence/nightly-check/should-render-long-notes-over-actor.mmd` | Retain the entire long note between the two messages; its top/bottom ports bind only Alice even where the frame also spans Bob's column | `d70619f8443f36fd.txt` |
| `sequence/nightly-check/should-render-multi-line-messages-aligned-to-the-left-when-configured.mmd` and its `right` counterpart | Preserve both ordered multiline messages under the default body-only profile; the two suffixes have the same SHA-256 | `20b7dce71c48ea60.txt`, `c752145b278121f8.txt` |

The last two original files contain different `messageAlign` frontmatter. Their
identical suffix drawings do not establish either original configuration's
behavior. All 2,145 raw hashes, Git blob identities, byte counts and raw policy
records remain unchanged; the original 40 snapshots retain identical hashes.
This checkpoint had 44 reviewed snapshots and manifest SHA-256
`d19f40245ca47a13f76d60531f305fb26108186e08fb21b21ed22173dc019d9a`.
The native matrix report identity at that
checkpoint is
`3deab5bef75b3d34ed9c4ac5740fe21124d91e150f43d0889735d3176ff345af`.
Complete original rendering and the original-assertion/conditional-branch
denominators below remain unfinished; this checkpoint is not 100% conformance.

The subsequent message-operator checkpoint adds eight body drawings and eight
snapshots, for 52 total, under manifest SHA-256
`8f934959dfc965174b6cc31b029e9e9845320eba01f3f0d8cccb83655a0742c8`.
Its independently reviewed source facts retain all 29
messages, 23 central endpoint circles, bidirectional/unheaded/cross marker
semantics, numbering, multiline rows and the long participant alias. The native
`MermaidSequenceSemantics.SourceReviewedOfficialMessageOperators` gate passed
before snapshot generation, including wide/narrow/restored following-Markdown
checks. All previous 44 snapshots retain identical SHA-256 values. Normal
`MermaidOfficial.*` execution then passed all four tests; the separate explicit
snapshot-generation run is not substituted for that verification. This
native official matrix report SHA-256 is
`2033368343909bbb79586c2abd70d318a6045b32496641407511f2c10ea54689`.
Raw classic-look, margin and log-level configuration is still unimplemented;
none of these eight body projections changes original-file drawing coverage.

The following actor checkpoint adds three independently source-reviewed body
drawings, for 55 snapshots, under manifest SHA-256
`f60a98f3ca859d6271cc95c1f07eccbfcd3fcd7e903c36b24c3579aa3f73acd6`.
The previous 52 snapshot hashes are unchanged.
Four messages, two source-ordered notes and four declared actor roles retain
complete geometry. In `actor-top-and-bottom-symbols.mmd`, explicitly declared
Bob precedes implicit participant Alice. The native terminal profile draws top
figures only; it does not satisfy that raw description's bottom-mirror visual
assertion. The manifest records this limitation explicitly. All four normal
official native tests passed. That matrix report SHA-256 is
`be23fff3ab8985bfb2f2a5180157d29b12031c832e250438d7174f7e497389a3`.
The separate note oracle also now accepts repeated identical notes by choosing
the earliest complete ordered match, while rejecting same-top ambiguity, missing
or extra notes and changed text. The complete Go101 cohort remains red at the
48-pass/53-fallback checkpoint below.

The side-note checkpoint adds eight further body snapshots after independent
source facts and the strong native `SourceReviewedOfficialSideNotes` gate passed.
The source-fact fixture SHA-256 is
`9a5ce7dc8c5f98badaf5c6a86346d8acbf98f6088b3851c2ef7e6131d188e1c7`:
eight distinct exact bodies retain 17 messages and nine notes, including central
connections, dual-target over-note ports, side leaders, complete long text and
explicit multiline rows. Left/right alignment fixture pairs have different
literal text (`I am left aligned` / `I am right aligned`), while all four use
`note left of Alice`. The default body profile preserves these distinct labels
and does not claim raw font, noteAlign, classic-look or margin configuration.
All prior 55 snapshot hashes remain unchanged; there are now 63 snapshots.
A malformed candidate filename was first rejected by the existing path-digest
identity invariant; that failed run is retained separately from the corrected
generation run and the four normal native official tests, which passed.
The current native matrix report SHA-256 is
`5f25b39ab683f3d888ed7e65f52139f719b34b056dc00fd4373baeba5096f68f`.
All original-file metadata and all 2,145 raw source outcomes remain unchanged.

## Family and feature obligations

Counts below are exact terminal inputs; all original record/option variants
remain attached. First C2 geometry coverage is limited to the complete 66-input
Beautiful flow cohort. It does not discharge other vendors' assertions on
duplicate inputs or an entire family.

| Family | Exact inputs | Native semantic obligations |
|---|---:|---|
| Flowchart | 1,119 | Exact node identities/labels/shapes; every directed edge and two endpoint markers, stroke and label; repeated/self edges; group ownership/nesting/endpoints; direction; rich/multiline labels; source styles/configuration |
| Sequence | 667 | Participant/actor identity, alias and declaration order; every ordered message, endpoint/arrow/stroke/text; activations; create/destroy; nested control alternatives; notes/targets; numbering/title/wrapping |
| Class | 479 | Alias/annotation and compartment content/order; member visibility/types/parameters/returns/static/abstract distinctions; every relation type/endpoint/marker/multiplicity/label; namespace/notes |
| ER | 293 | Entity identity/display; ordered type/name/key/comment attributes; every relationship's endpoint/cardinality/role/identification; self/parallel relations |
| XY | 147 | Calibrated two-dimensional axes and domain/orientation; category/sample order; every bar/line value, baseline and connected geometry; series ownership; exact-value disclosure when cells quantize values |
| State | 84 | State/control identities; start/final/choice/fork/join distinctions; every transition and label; composite containment and group endpoints; concurrency; note ownership/direction |
| GitGraph | 31 | Commit/branch identity and order, parents, merges/cherry-picks, tags/types and direction |
| Block | 21 | Columns, spans, spacers, shapes/labels, nested membership and every link |
| Pie | 14 | Reviewed terminal bar projection: slice order, shared total, proportional lengths, owner-bound labels/swatches, exact values/title/showData; valid-empty separately |
| Mindmap | 13 | Root, parent/child and sibling order, depth, labels/shapes, source icon/class semantics |
| Treemap | 12 | Parent/child identity, labels/values/order and proportional ownership under the reviewed terminal projection |
| Gantt | 6 | Section/task order, dates/durations/dependencies/status and disclosed time-domain correspondence |
| Kanban | 5 | Ordered columns/cards, exact membership/title and metadata |
| Packet | 4 | Field labels, inclusive bit intervals, ordering, width/wrap and overlap/range policy |
| Timeline | 4 | Section/event/detail order and exact attachment |
| Journey | 3 | Ordered sections/tasks, scores and actors |
| Quadrant | 2 | Point labels/coordinates, quadrant and axis labels/domain |
| Architecture | 1 | Services/groups/icons/ports, membership and every directed connection |
| TreeView | 1 | Identity, parent/child/sibling order and depth |
| Railroad | 1 | Production/symbol kinds, sequence/choice/repetition and exact topology |
| Ishikawa | 1 | Cause/category hierarchy, direction and incidence |
| Requirement | 1 | Requirement/element metadata and typed relation endpoints |
| Swimlane | 1 | Lane identity/order, step membership and every edge |
| Unknown | 6 | Invalid/empty/robustness source admission; no invented drawing |

Projection is part of the reviewed contract. Termaid's pie test source explicitly
states “rendering (as bar charts)” and asserts bar proportions; circular sectors
are not required to port those tests. Its radial `textPosition` metadata remains
provenance with explicit non-applicability to the approved bar projection.
XY still requires a genuine calibrated two-dimensional plot. Merman's documented
structured-summary families retain a distinct summary/data-preservation
contract; observing text cannot be relabelled diagrammatic evidence. Stronger
graphical obligations need their own explicit product/design decision.

## Original assertion and branch ledger

The per-case manifests contain 4,557 assertion-reference occurrences, deduplicated
to 1,284 distinct assertion objects. These mix literal predicates and whole
original test-function references; they are **not 1,284 individual assertions**.
`assertion-unit-ledger.json` retains every object, case binding, actual asset
hash, source span and original condition/options. No assertion asset is missing.

| Project | Reference occurrences | Distinct assertion objects | Additional source accounting |
|---|---:|---:|---|
| Beautiful | 151 | 151 | 597 static declarations; 1,393 statically counted assertions; actual expanded suite 711 tests / 1,739 expect calls |
| mermaid-ascii | 46 | 35 | 162 top-level Test/Example/Fuzz declarations; 482 statically counted assertion calls; 862 actual passing events include parents |
| Termaid | 1,195 | 322 | 714 Python test functions / 1,394 syntactic assert statements; actual parameterized suite 1,614 tests |
| Mmdflux | 1,394 | 243 | Separate index of 2,412 private/nonterminal candidate test functions; public-source bindings do not port these private tests |
| Merman | 1,771 | 533 | 647 integration + 729 unit declarations; 678 saved exact function slices, including 145 without authored-source case bindings |

The audit found lexical conditional/loop hints in 573 assertion objects,
encoding/profile hints in 219, negative/empty hints in 200 and resource/viewport
hints in 122. These overlapping counts are navigation aids, **not branch
coverage or approved exclusions**. A native mapping must inspect the exact
function and public invocation option variant. In particular:

- A `contains` predicate applies to the output of a specific render invocation,
  not every case sharing its enclosing function. Preserve owner and callsite.
- Parameter loops, ASCII/Unicode branches, width modes, SVG/HTML/ANSI paths,
  cancellation, warning/error and structured fallback paths need separate IDs.
- Resolve relational predicates such as differing glyphs, ordering, proportional
  lengths and branch counts into native geometry predicates. A model import does
  not execute those assertions.
- Do not add a public ASCII mode merely to reproduce a foreign option. Preserve
  that branch, document the exact Unicode semantic equivalent, and review its
  non-applicable literal glyph/spacing clauses.
- Test names, source slices and execution events use different denominators.
  Static counts and expanded upstream test totals must remain distinct.

In the frozen 66-input batch, 30 unique literal assertion objects execute under
the native `portable_assertions` helper; 15 explicit ASCII-only objects are
accounted separately. Their original enclosing source branches still require
the recorded profile mapping before any full original-suite claim. The other
1,239 assertion objects have no established complete native mapping in this
audit. This is a conservative mapping inventory, not a claim that 1,239
individual predicates all need separate test functions.

A later Beautiful expression port executes12 exact predicates from four frozen
Flow functions, including seven newly executed nonliteral expressions: two
relative-row comparisons in the multiline cases, the remaining ordered-row
predicate, three alternative-label booleans and the Unicode-box alternative.
The five literal predicates already executed in the66-input batch are not new
assertion credit. The native TEST also binds every whole source/complete Model,
checks five physical frames and rejects good-first predicate mutations. Both
Debug and Release pass4/4 Terminal TESTs. Evidence:
`beautiful-flow-expression12-first-dual-r2/RESULT.json` and
`flow21-first-red-expression12-actual-independent-review-r1/REVIEW.md` under the
research audit directory. These atomic expressions are a different denominator
from1,284 heterogeneous reference objects; no automatic subtraction from1,239
pending objects or complete foreign invocation/options certification is claimed.

Private/model-only functions remain accountable. `static-test-catalog.json`
retains every imported index entry. Mmdflux's 2,412 private index entries have
no saved matching public assertion slice: their pinned source hash/byte spans
are reference evidence, not ported checks or an approved blanket exclusion.
Merman's 519 source-free/modified model invocations and 170 ambiguous rendering
bindings remain outside the fabricated-source denominator. A reviewer must
separate true private mechanics from public behavior that can be reproduced
through authored input; never synthesize source from an arbitrary private model
and claim the original test was executed.

An executable mapping record needs the original vendor/function/expression and
source hash, exact input/callsite and option condition, independently declared
expected facts, the named native GoogleTest predicate, positive/corruption
witnesses, and result/source/executable identities. For example, a class
inheritance assertion maps to two distinct matched class frames and a connected
solid relation whose hollow triangle is at the declared parent endpoint; moving
that marker to the child must fail. An ER cardinality assertion maps to both
endpoint ports with owner-bound cardinality footprints; a swapped cardinality
must fail even when both entity names and all glyph kinds remain visible.
An XY point assertion maps to its category anchor and calibrated numeric-axis
coordinate; a correct value in a detached table cannot satisfy it. This is a
test-port record, not a new production protocol or generic renderer framework.

## Source corrections, missing facts and policy work

All original records and hashes stay unchanged. Corrections live in a separately
locked obligation layer, anchored to exact source bytes and reviewed before Loom
execution. They must not be derived from renderer output.

| Issue | Evidence | Required native disposition |
|---|---|---|
| Valid empty ER misclassified as drawing | `erDiagram\n`, SHA `74ae87fd89fe0e9f4cfa8da895ab6527ef4da22c4cd1de2a703dd51149114558`; Go explicitly expects empty, Merman model has no entities and its output reference is zero bytes | Reconcile two original classifications as valid-empty; no invented entity or nonempty drawing score |
| Empty GitGraph misclassified | Termaid `a0655f0ceb954df5ace6`; `upstream/tests/test_gitgraph.py:422-425` explicitly requires `output == ""` | Separately locked valid-empty correction |
| Empty sequence misclassified | Termaid `536f52ac1f79d0918360`; `upstream/tests/test_warnings.py:118-122` explicitly requires empty text | Separately locked empty/admission policy; do not copy a foreign exception as valid diagram syntax |
| Header-only smoke probes | Termaid `mindmap`, `pie`, `treemap-beta` only assert string/no-crash; header-only XY can still imply chart axes | Inspect all 13 exact header-only candidate sources; distinguish empty, robustness and genuine axes obligations individually |
| One positive lacks a typed model | Mmdflux `cb885dfe5bdfc9ab04f6`; foreign custom font-family/font-size metrics policy rejects model export | Read original source and freeze graph/label/shape facts independently; map terminal font profile explicitly; keep the positive pending |
| Nine negatives lack typed models | Three Go, one Mmdflux and five Merman negative records | Freeze authored invalidity/error/whole-source behavior independently; missing models are not drawing failures |
| Existing first-group omission | Beautiful `eead33eba3471656` | Preserve original model/golden and the independently locked first-ownership correction |
| Class/ER quoted multiline defects | Beautiful class alias `MyClass["Long<br>Name"]`; ER `"Entity<br>Name"` | Lock source ID separately from display rows; never retain quote/tag text as model truth |
| State type-confusion probe | Termaid `17f03f94ca1a30b6bf67`; malformed chained source is a returned-string/error control | Preserve source/error behavior; no fabricated transition node containing arrow syntax |
| Cross-schema endpoint polarity | Merman ER `relSpec.cardA` denotes cardinality at entity B | Normalize against the original relationship source and endpoints; names alone are insufficient |
| Lossy model fields | Go graph shapes/actor distinctions and Unicode styles; Mmdflux flattened class compartments; Termaid merged fork/join shape enum | Corroborate every required distinction with source and original assertions; add locked source facts where the upstream model loses them |

Foreign resource/profile errors are not Mermaid grammar errors. Keep original
options, exact errors and observable public behavior. Apply Loom's independently
declared source/node/edge/label/canvas/work policy, with at-limit positive and
over-limit whole-source tests. A current layout/router failure within the policy
remains a capability failure. Do not raise the limits to improve a score.
Raw terminal/bidi controls, leading combining characters and zero-width text
need explicit safe-display policies; neither emitting controls nor silently
dropping authored text can count as compatibility.

## Native execution contract and next cohorts

Each mapped positive needs real `render_markdown`/FTXUI cells, whole-output text
ownership, actual geometry/topology, fail-closed bounded oracle work and
hand-authored positive/negative scenes. Missing nodes/values, swapped endpoints
or marker sides, duplicate labels, extra unknown text, broken group ownership,
wrong proportions or axes, source fallback and oracle exhaustion must fail.
Check the same element at wide/narrow/restored widths and following Markdown.
Private test normalizers may consume frozen facts; a test-side Mermaid parser,
external backend/runtime or input-to-golden lookup is not an oracle.

The highest-count four terminal families contain 2,558/2,916 exact inputs
(87.72%), which is input reach rather than a success target. The recommended
order is:

1. Close source/empty/profile classification conflicts and freeze exact assertion
   mappings while existing 66-flow evidence stays unchanged.
2. Extend sequence's existing public path across all 667 inputs: event/actor/
   activation/control/note/arrow semantics provide more value than header-only
   recognition. A complete independently reviewed first source cohort may stage
   this work, but must retain the full-family backlog.
3. Continue flow shape/marker/group endpoints/rich-label features across all
   1,119, and use the reviewed first Class/ER/State cohorts to establish
   compartment, cardinality and state-control predicates. Reuse cell/work
   invariants inside the owning module; do not introduce a universal renderer
   service or external implementation.
4. Complete Class 479, ER 293, XY 147 and State 84; together with flow/sequence
   these six cover 2,789 exact inputs. Remaining 127 sources still need full
   individual contracts, not a percentage exemption.
5. Continue official original metadata and all remaining directory families as
   their separate complete source denominator; diagnostic suffixes never replace
   the original files.

The current test-only relation plan freezes 862 complete Class/ER/State records
(856 distinct sources) before its 62-record first batch. The chart plan freezes
161 XY/pie sources before its complete 36-input first batch, including one
valid-empty pie. Family designs and executable adversarial oracles require
independent review before production feature work. None of these plans, asset
counts or audit outputs constitute completed 100% coverage.

For sequence cohort selection, mermaid-ascii has 101 sequence records: 100
original drawing declarations and one original header/comments-only negative.
Calling the first 100 a complete positive cohort is precise; calling them the
complete sequence family without retaining that negative is not. The separate
Termaid header-only sequence valid-empty probe also requires the source-backed
policy reconciliation above.

### Go101 pre-production source-fact lock

The native obligation layer is frozen separately from the unchanged vendor:

- `tests/fixtures/mermaid_native/sequence_go_cohort.json` locks all 101 IDs and
  the original vendor identity; SHA-256
  `3df78514fc1e30026f8994f23771a73c64c576711ad2a87916c3aeb8a9c1b79f`.
- `tests/fixtures/mermaid_native/sequence_go_facts.json` records independently
  reviewed role, title/color, safe-display and source-correction facts; SHA-256
  `44ea6d8d0d7d8b90db2e741672ee7e999740ce26860a127bd9e784425268002b`.
- The original Mermaid-ascii cases file remains SHA-256
  `7783a2b6936999a6d8406475a49fe7bd1a258441423bc67bd0d5767bee0575b3`.

The facts preserve the approved source-only candidate
`0a499e0792694119388da79dc6bff5a931e5347fbf8212878a168ff8fcc11eae`.
Their only additional field is `explicit_break_fields` on every case. It is
empty except for `mermaid_ascii_6fe5598c27c6f42e`: the source at UTF-8 byte 42
and model path `/events/1/note/text` both contain `a<br/>b`, independently mapped
to the two adjacent rows `a` and `b`. No vendor source/model/expectation changes.
The safe-display contract explicitly includes DEL U+007F, preserves attached
combining sequences and uses unambiguous escaped disclosure for unsafe or
otherwise unrepresentable label scalars. It does not authorize silent text loss.

The original assertion denominator remains separate: 45 inline function/input
bindings refer to 27 source functions; 14 imported literal references contain
12 distinct objects. There are 96 golden references, 57 Unicode and 39 ASCII,
on 60 source records. Several imported `not_contains` objects are actually
row-selection or interval-boundary guards: their source tests require the
selected message/note text or crossing to exist. The independently reviewed
conditional mappings must replace those incorrect whole-output interpretations
without editing the imported objects. Exact source-function hashes and findings
are retained in `full-obligation-audit/go101-assertion-source-inventory.json`.
Frozen facts and a typed-model baseline are not executable original-assertion
coverage; the complete native101 semantic and conditional-assertion gates are
still required before a completion claim.

### Go101 message-oracle checkpoint

The independently reviewed message-only oracle is frozen at test SHA-256
`5bfb3d2d5fbc24c2931ae7301f7feede656d8afcf7b3574dda3d142a767e80cb`.
Its 18 native tests passed: 14 handmade/inventory tests and four existing
semantic/resource tests. The gate covers filled, unheaded, asynchronous, cross
and bidirectional markers, central endpoint circles, message numbering, empty
labels, complete self returns, stroke consistency and ordered repeated messages.
Self returns cannot borrow another participant's lifeline; unclaimed lifeline
cells require an exact ordinary vertical stroke. Same-row route ambiguity,
extra arms, missing routes and extra messages fail. Candidate searches retain
the existing charged work and assignment limits.

The initial complete Go101 pre-production baseline was red: 31 semantic
passes and 70 failures across all 101 records, including 69 source fallbacks,
recorded in `sequence-audit/go101-reviewed-native-baseline.log`. After the bounded
message implementation and exact wide-cell label correction, the next recorded
checkpoint is 42 semantic passes (41 drawings and one valid-empty document) and
59 source fallbacks. Every non-fallback scene passes the currently ported native
geometry; the complete cohort remains red. The original six-input sequence
report remains SHA-256
`62ee365414fb3c47f2747b27e81c7f529f51c11ffe6fbd48a0ff3737c7b81012`.
This checkpoint permits implementation of the reviewed message capabilities;
it does not approve actor-role/lifetime, activation, fragment, participant-box,
title, general-identifier or safe-display geometry. Original conditional
assertions remain a separate pending gate.

The next actor-role executable gate, test SHA-256
`df611f45d81e0e836e97cc1f6899f76cfbbc0fe10ba9e78a41a074a7c3041f28`,
binds a complete three-row figure to its own participant header using the
independently locked declaration role. Six visible cells and three blank cells
must match; missing strokes, misplaced figures, swapped roles and extra marks
fail. `sequence-audit/go-cells-official-eight-actor-gate-tests.log` records 23
tests: 22 pass, and the complete Go101 batch remains red as described above.
This permits the bounded actor-role implementation; it does not satisfy created
or destroyed participant lifetimes or any other pending ordered event family.

The bounded actor implementation then passed its applicable native gate. The
fixed `sequence-audit/actor-first-native-report.json`, SHA-256
`f1d924763cd66eae5cdcc0daf55957449aea64db98de52fb91044b97286afeac`,
records 48 semantic passes (47 drawings and one empty document) and 53 source
fallbacks. Every non-fallback scene has zero native semantic errors. The complete
101-case test remains red, and this result does not port its original conditional
assertions or close any remaining event family.

### Relations first-62 debug checkpoint

The complete first 62-record Class/ER/State cohort now passes its bounded native
debug gate: 61 drawings and one exact-source control, zero semantic errors, and
all 62 same-element restorations identical. All three captured states remain
byte-identical to the prior passing cohort after the final regression fixes.
The source and test hashes are respectively
`81f5cbc717e4edb8d59dbc15e4eb3fc900a61e447d2f06691010102435073108` and
`ce3056082d94ec9cdf0a07e22b69506b37cd11a8912c4fb369e226a8df384ade`.
The 28-test debug log and final report are recorded in `relations-audit/`;
the report SHA-256 is
`3b1e7fdb22ae8465304699b6ec950e0d2793fdf0e9f760651c7087d0e30a2a3e`.

The original importer catalog's 86 records classify as 59 passed native
predicates, 19 conditional foreign ASCII records and eight passed native
geometry-equivalence ports. Those eight ports retain and hash the exact original
snapshots, fixture preprocessing, function and complete authored obligations;
they do not claim foreign pixel equality. The additional complete source-function
catalog executes 51 Unicode conditions and preserves eight ASCII conditions.
Its 59 statements overlap the importer records and must not be added into a
manufactured unique total.

Independent final review found repeated-note overwrite and composite-header
overflow paths; native source regressions reproduced both before the fixes.
Note collision checks now return complete source when overlapping placement
cannot be represented, and group measurement preserves its header and child
containment, including 20-row LR/RL cases. These checks do not increase resource
limits. Safe source fallback for overlapping notes remains a drawing capability
backlog. This checkpoint approves only the first 62 and these regression guards;
the other relation inputs within the frozen 856-source family denominator and
their applicable original assertions remain unfinished. The bounded release
verification is recorded separately below.

### First bounded dual-preset verification

`sequence-audit/current-dual-verification.json` records six native executables
under both debug and release: sequence, charts, relations, flow, official and
Markdown rendering. Independent review parsed every actual complete log, checked
its recorded SHA-256 and matched the full test-name/status sets across presets.
Each preset passes 150 of 151 tests. The sole failing test in both presets is
`MermaidSequenceSemantics.CompleteGo101NativeBaseline`; its 53 source fallbacks
are genuine unmet drawing obligations. This is not a full-repository test run
or an all-green suite, and an expected remaining failure is not a passing test.

All seven actual native report files are byte-identical across presets: the
first-36 chart report, complete Termaid XY23 report, first-six sequence report,
Go101 report, first-62 relation report, existing flow report and official matrix.
Their exact hashes are retained in that verification record. Go101 remains
48 semantic passes and 53 source fallbacks; official body drawings remain 55
of 593, with zero of 2,145 original files drawing. The release executable at
this checkpoint has SHA-256
`8c4659623b30bc1d8d021e20cc65e4f3f27b03ad0a09e75c18852ada3b265a37`.
Subsequent side-note oracle/production work is outside this frozen verification
and requires separate evidence. The real-terminal validation below uses this
same release executable.

### Limited real-terminal evidence

The `mermaid-native-reviewed-stage/verification.json` artifact under
`/Users/bytedance/.codex/visualizations/2026/10/19/01a15315-ef3b-7953-8e46-ca4aba152240/`
records a real OS PTY child, PID 30480, executing the release binary above from
`/private/tmp`. A browser xterm displays that PTY's live output. Three exact
frozen inputs were delivered by deterministic local SSE responses: Go actor
declarations with unheaded solid/dotted messages, Termaid numeric-x samples,
and Beautiful class members/annotation with inheritance. This validates the
real terminal rendering path, not hosted model inference.

Independent review checked each input against frozen vendor bytes, response/SSE
hashes, the PTY spawn code, process identity, recorded ioctl sizes and all five
screenshot hashes. The same child changed from 160 to 40 to 160 columns while
the response count stayed at two. The narrow XY view shows source fallback;
the restored screenshot is byte-identical to the original wide screenshot.
Actor roles/unheaded shafts and class compartments/upward inheritance are
visible in the other captures, with following Markdown retained. The local
server and child both exited, confirmed by the cleanup record and process check.
These three examples do not discharge other corpus obligations or side-note
features; the 48/53 Go101 and 55/593 official-body boundaries remain unchanged.

### Subsequent side-note debug checkpoint

The next bounded implementation admits `Note left of` and `Note right of` one
existing ASCII participant. It retains source event order and complete compact
multiline labels, with an ordinary solid leader from the note's owned midpoint
port to the target lifeline. Generic frame text now verifies physical terminal
glyph cells, including wide-character continuation cells; concatenated text
alone cannot certify a header or note. Handmade adversarial tests separately
check target/side swaps, complete leader strokes and genuine intermediate
lifeline crossings. The production placement currently uses a short local
leader; its `Across` label does not itself prove an intermediate crossing.

Production SHA-256
`016ad625a48a0f7ea9353ffcc4a85a0d621aa99a418bf54714900743880dfda8`
and test SHA-256
`fcf3f4c449a34ad1632473eacb64fbfd8f3cf4839fd478d0488adcd9be431b4b`
passed the applicable independent debug gate. The complete native log
`sequence-audit/side-notes-positive-tests.log` records 29 of 30 tests passing;
the genuine failing test remains the complete Go101 batch. That cohort now
has 50 semantic passes and 51 complete-source fallbacks, with zero invalid
drawings among non-fallback scenes. The original six-input report is unchanged.
Native positive/negative coverage includes CJK aliases, multiline left/right
notes, resize restoration, malformed targets/grammar and the 64/65 note boundary.
No resource limit or public interface changed. This stage is not covered by
the earlier dual-preset freeze or PTY screenshots; its release and any further
source-admission changes need their own verification.

### Subsequent identifier and note-first-use debug checkpoint

The independently reviewed identifier profile is backed by the pinned official
sequence lexer/participant and note productions and the pinned Go terminal
parser. The official grammar copy has SHA-256
`f3f21ae48e3c334fc259e3f78ac60ef70cf930ac6df2dd3c43f2b0de6f2b24db`;
the inspected Go parser has SHA-256
`cf447b3edda8724c023a59a9da1533cc411f69f213907b215f1cf3616f217e6c`.
Spaced, equal-sign, hyphenated, numeric/dotted and safe Unicode identities retain
their exact identity independently of display aliases. Quoted identifiers with
arrow text are an explicit Go-compatible extension. Their default header is
literal text, including a quoted `<br>`; alias/note/message decoding is unchanged.
Notes can introduce participants in authored first-use order under the original
participant cap, with later explicit role/alias declarations.

Before implementation, the new source-derived native geometry cases failed with
the old parser. A subsequent punctuation regression also failed before the
uniform standalone-keyword fix; `actor-service` and `note.svc` are not statements.
The final bounded debug candidate uses production SHA-256
`829a7e2ff93cec2e24faf0471c0251ef623a47d49c1edd0c96e4b485497909d4`
and test SHA-256
`4a3ce2dca0d1f8b3bbe53bdc4a230c2f14fef747531fa7420ba4e75c646af342`.
`sequence-audit/extended-IDs-boundaries-fixed-full-tests.log` records 32 of 33
tests passing; the complete Go101 batch remains genuinely red. Its observations
are 57 native semantic passes and 44 complete-source fallbacks, with zero invalid
drawings and the original six-input report unchanged. Thirteen source-positive
cases and the whole-source negative controls use the existing strong cell,
identity, endpoint and note oracle; no test-side Mermaid parser was introduced.
Equal display aliases are tested with a third anchor and distinct named edges.

No safety-display encoding, activation or control-fragment implementation is
approved by this checkpoint. Existing resource bounds and all vendor files,
models and original assertions remain unchanged. Further official body snapshot
promotions and release/PTY verification are separate gates.

### Side-note, identifier and eight-body dual-preset checkpoint

`sequence-audit/side-ID-eight-dual-verification.json` freezes the subsequent
side-note/identifier implementation at production SHA-256
`829a7e2ff93cec2e24faf0471c0251ef623a47d49c1edd0c96e4b485497909d4`
and test SHA-256
`4a3ce2dca0d1f8b3bbe53bdc4a230c2f14fef747531fa7420ba4e75c646af342`.
Seven native executables ran under each preset: sequence, charts, relations,
flow, official, Markdown and the complete terminal observation baseline.
Independent review checked all fourteen complete logs and their hashes and
matched every test name/status across presets. Each preset passes 162 of 163
tests; the complete Go101 cohort remains genuinely red, with 57 semantic passes
and 44 exact-source fallbacks. This is a bounded run, not a full-repository run.

Nine report files are archived in `sequence-audit/side-ID-eight-frozen-reports/`.
All nine match their executed release reports. Eight current debug reports also
match release byte-for-byte. The Go57 debug report was subsequently overwritten
by the separate literal-text stage below; its archived Go57 report matches
release, and its 44 failing IDs match the complete frozen debug log. The Go57
report SHA-256 is
`62045ba162e1caaf6cf4411f4d447823caab75ff8e51f963967712eb09de94f7`.
The other reports cover charts36, Termaid XY23, sequence6, relations62, flow,
official63, Merman102 C1 and all 2,916 exact-input terminal observations.
Neither `unverified_draw` nor a Merman C1 candidate is semantic certification.

The official report at this checkpoint is SHA-256
`5f25b39ab683f3d888ed7e65f52139f719b34b056dc00fd4373baeba5096f68f`:
63 of 593 body projections draw at wide/restored width, 34 draw at 40 columns,
and all 2,145 original files retain complete source fallback. The eight new
side-note body promotions preserve the prior 55 snapshots and all original
metadata. They certify the reviewed default terminal projection of body text,
not removed frontmatter font or alignment options. The release executable has
SHA-256 `010bd680b8c6e9ca6c33164f1e6c7fd493a4d647b938fe59757e4d9c5c940290`.
Its independently reviewed new PTY evidence is recorded below; the earlier
screenshots above remain evidence for the earlier binary only.

### Side-note and identifier real-terminal evidence

The `mermaid-sequence-side-identifiers/verification.json` artifact under the
same absolute visualization root records release executable `010bd680...940290`
in a real OS PTY child, PID 1980, from `/private/tmp`. Independent review checked
the PTY spawn/exec and resize code, all three input bytes against their frozen
Go vendor records, complete response/SSE hashes and all five screenshot hashes.
The inputs are `58a05b938f6565a7` (spaced/equal identifiers with distinct aliases),
`ede755696f0a4eda` (left note bound to A), and `4f8c957de514a643` (an arrow inside
a quoted identifier, with an actual message to C).

The screenshots visibly preserve Cron/DS aliases and message directions, the
LEFT note's owned midpoint port and solid leader, and literal header `A->B`
separate from the actual message shaft. Following Markdown sentinels remain
visible. The same child changed from 160 to 40 to 160 columns while both ioctl
records retain exactly two model responses. This note fits at 40 columns and
continues drawing; it is not a fallback example. Wide and restored screenshot
files are byte-identical. The local server PID 1722 and PTY child both exited,
confirmed by cleanup records and a current process check. These captures use
deterministic local SSE, not hosted inference, and do not include the subsequent
literal-`>` implementation or discharge the remaining Go101 obligations.

### Literal-arrow text debug checkpoint

A subsequent sequence-private grammar correction permits literal `>` after
structural field parsing, as allowed by the pinned sequence text grammar.
Production SHA-256
`6c9ae01484b55f812f3c2a7ff2f1b78b252bb67ebc718f6a675a91477aaf2ae0`
differs from the preceding production only by removing `>` from its forbidden
plain-text character set. The restrictions on `<`, entities, hash markers,
backticks, unsafe Unicode, quoting, explicit breaks and label resources remain.
No shared module or established public interface changed.

The source-derived alias/message/over-note/side-note geometry regression first
failed against the preceding parser. After the correction,
`sequence-audit/literal-arrow-text-production-full-tests.log` records 33 of 34
tests passing; the sole failure remains the complete Go101 cohort. Go101 now
has 58 semantic passes and 43 complete-source fallbacks, with zero invalid
non-fallback drawings; exact input `mermaid_ascii_4ee7e2bea5e82f5b` passes its
locked model, and the first-six report remains unchanged. This debug-only stage
is separate from the preceding Go57 dual-preset freeze and does not approve
fragment production or certify the pending original conditional assertions.

### Complete ER296 C1 source/observation admission

Independent source review permits a complete ER observation cohort of 296
records and 293 exact sources: 164 Merman, five Beautiful, 85 Go and 42 Termaid
records. The locked compact facts SHA-256 is
`44911ea88baad20091c642c3c1ca502050d30de46e50ae0185d86285958e4d44`.
The source requirements retain 294 drawing records and two same-source
valid-empty records, with all foreign classifications preserved. Source/model
corrections and presentation obligations are explicit; no current renderer
output sets their expected semantics.

The independent original-function catch list retains 165 complete functions,
361 lexical assertion sites, and separately 269 imported assertion applications
and 296 snapshot applications. These overlapping measures are not a manufactured
unique predicate total. Thirty-four Beautiful SVG/parser/private functions are
accounted for separately from its two terminal functions. All complete ER
presentation and original-condition ports remain pending.

The admitted native C1 tests lock every source/model/expectation identity and
capture the same element at 240, 40 and restored 240 columns, with sentinel and
restoration failures remaining test failures. Candidate art, empty candidates,
source fallback, diagnostic model mismatch and diagnostic oracle errors are
reported separately; even a diagnostic model match certifies neither geometry
nor valid-empty behavior. Both certification counts remain zero. This checkpoint
approves the observation implementation and source inventory only; a complete
ER C2 semantic profile remains a separate gate.

The subsequent native C1 run is recorded in
`relations-audit/er296-C1-budget-classification-tests.log`: both inventory and
complete-source observation tests pass. The diagnostic budget correction uses
the same independent 8M work limit while distinguishing oracle exceptions from
model mismatches. The full test SHA-256 is
`a14f2f49a090de54570919881c2909adb121183837e9b2478230e37b1dd788a6`;
its original 263,821-byte first62 test prefix remains exactly
`ce3056082d94ec9cdf0a07e22b69506b37cd11a8912c4fb369e226a8df384ade`.
The observation report SHA-256 is
`ee7f0096166386cfeaf34edaf6627372b63269cbfa1cafcc91d599d5fab257e2`:
157 candidate scenes and 139 exact-source fallbacks, zero observation errors,
zero certified geometry and zero certified empty cases. All 157 diagnostic
models match, but this is explicitly not a complete presentation/original-
assertion port. The previously approved first62 report remains unchanged.

### Literal box-glyph sequence oracle checkpoint

Frame discovery returns bounded glyph candidates, not a count of semantic
participants and notes: a complete box may be authored literal text inside a
multiline header. The oracle therefore removes the false candidate-count
equality while retaining the 256-candidate, 10K-assignment and 8M-work bounds.
Expected headers and notes still match distinct candidates, complete physical
text and exact borders, followed by complete cell ownership. An unrelated extra
box is not silently accepted. The handmade literal-box positive, wrong-glyph
negative and extra-box negative all execute alongside existing entity/note and
route mutations.

`sequence-audit/literal-box-candidate-fixed-full-tests.log`, SHA-256
`2e983e8123cc78a49418225808ddc144cac24f27aba1bd9a9411738d8612abf1`,
contains 36 distinct native tests: 35 pass, and only the complete Go101 batch
remains red. This test-only correction does not implement fragments or change
any source facts, vendor assertions or production capability.

### Plain typed ER table oracle design admission

The provisional `relations-audit/ER296_C2_TYPED_PROFILE_DRAFT.md` is independently
approved for plain typed-table oracle authoring only. Type, name, ordered keys
and comment remain separate attribute fields; repeated blocks append ordered
rows, duplicate keys remain data, and absent optional columns cannot shift
other fields. The owning private ER representation may correct its raw-member
abstraction only after the new geometric oracle passes. Class/State behavior
and their existing 57 captured scenes remain protected; intentional stronger
table pixels for the five first-batch ER inputs do not alter their source facts.

The corrected source-only metadata ledger SHA-256 is
`8b2915c89d85c5ffb896a40c35000c47f586d0fe2458143711c057b5279000f0`.
Independent review verified all 296 IDs/source hashes, 51 UTF-8 metadata
witnesses, and all seven pinned primary file/blob and span hashes. Its 18
frontmatter records, one init record, 19 configuration leaves, four visible
titles and ten accessibility entries are source accounting, not implemented
presentation behavior. Both review clarifications are closed in draft SHA-256
`6b8b713ea7e9740147a8d19b3b8f1b897cc2853e1dd02be6a397866554c14d11`:
the init witness includes its complete directive delimiter, and the unchanged
256-unit displayed-attribute budget totals all decoded fields, terminal
continuations, line breaks, ordered comma-joined keys and semantic join spaces.
The required boundary mutations reject 257 and four separate 256-unit fields.
Table padding remains separately bounded canvas geometry. Rich text,
metadata/profile treatment, MD_PARENT,
empty output, foreign syntax extensions and complete original predicates
remain separate obligations. No ER C2 production change is approved here.

### Sequence fragment executable-oracle admission

The test-only fragment gate at sequence test SHA-256
`041fb0d5e089290d0f715fd2bfcb6ff4d330e16b2108a303caf9fdf53f1dd08f`
passes all 26 oracle tests in `sequence-audit/fragment-oracle-first-tests.log`,
SHA-256 `9bd565a3c36628790dfac99923512090e948ecf6649d6b91fb637cd5c7efe0c8`.
Production remains `6c9ae014...aaf2ae0` at this gate. Independent adversarial
review approves private fragment parser/layout authoring for loop, opt, alt,
par, critical, break and rect with their matching branch dividers only.

The oracle checks complete ordered stack events and branch ownership; exact
multiline kind/condition cells; closed frames and separators; ordinary solid
strokes and exact four-way crossings at participant columns. Only verified
borders, separators and title cells are claimed. Messages, note leaders and
nested frames retain independent complete bounds, strict branch containment
and event-order checks; remaining ink fails. Native handmades include every
kind, multiple dividers, nested fragments, zero-actor empty drawings, repeated
sibling captions and note events, with text/border/order/ownership corruptions.
Existing 512-event, 64-fragment, depth-16 and shared work/canvas limits remain.

This is permission to implement that bounded feature, not production graduation
or a claim that the complete Go101 cohort passes. Production boundary controls,
unchanged legacy pixels, both presets and source-reviewed official admission
deltas remain required. Activation, participant boxes, lifetimes, safe display
and original conditional assertion ports remain independent unfinished gates.

The extended test-only fragment checkpoint adds actual par/critical dividers,
unowned inner-frame/body-island mutations, a note inside a nested region and
fully drawn 64-sibling/depth-16 scenes. All 28 oracle tests pass in
`sequence-audit/fragment-oracle-extended-tests.log`, SHA-256
`2d3933d5c843319e1a5283529f817eb16d4fd22801c2c0e8225ebf578f7f8885`.
The Go codec maps all frozen 32 starts, 12 dividers and 32 closes into the
independently validated ordered model. Independent inspection confirms all
220 event-message payloads agree with their complete message-list counterparts
apart from numbering, which the complete list retains. Source-only rect colors
are consumed separately and completely; unsupported non-fragment event kinds
remain explicit pending failures.

With production still unchanged, the codec pre-production log
`sequence-audit/fragment-go-codec-preproduction-tests.log`, SHA-256
`c96208d4194a59489ee61e90fb77fbf999e782c3bd69f820230bb689bc6757b0`,
records 38 passes out of 39 tests and 43 unique failing Go case IDs. The sole
failing native test remains the complete Go101 cohort; its 58/43 boundary has
not improved through changing expected data. Production fragment/resource
source regressions are newly authored but their execution and narrow/restored
resource evidence remain subsequent gates.

### First fragment production debug checkpoint

The first private implementation has production SHA-256
`ff6f1dca3d13c60402a8439104fa78a9b54721deaa4ee15ff34c88f1d0e9346b`
and test SHA-256
`866bfdede1e1326191834d8a167a07746ae9acf2177063c04d576ef89ab9f846`.
Both newly authored production controls failed against the preceding parser
before implementation. `sequence-audit/fragment-first-production-debug-tests.log`,
SHA-256 `b48015851086d5986b9265920cc3193c803eccb4d348bd3c80b80990438db2c6`,
then records 42 passes out of 43 distinct native tests. Only the complete Go101
batch fails. Its report SHA-256 is
`18bb8959792c01f18c44b8b84491c516aa5fff90d9ce09e8dbd9a43a64361b63`:
80 semantic passes, 21 whole-source fallbacks, and no errors in non-fallback
scenes. Complete original conditional assertion translation remains pending.

Independent review confirms strict parser stack/divider matching and whole-source
failure, bounded event extents and nested margins, and unchanged shared Canvas
write/segment/scan charging. The 64/65 fragment and 16/17 depth controls now also
exercise the same element at wide, narrow and restored widths with all three
sentinels. The new AST and pre-layout work is hard bounded by source/event/label
limits but still requires explicit shared-Work accounting to match the approved
contract; that P2 accounting correction is a separate final implementation
condition. Legacy no-fragment pixel equivalence, new official admissions,
both presets and PTY evidence are not implied by this first debug result.

Fifteen newly drawing official body candidates have independent source-only
models at `full-obligation-audit/official-fragment-fifteen-source-facts.json`,
SHA-256 `74ead1f2d193632e242fccfe083467d2c47313776849537a2a97fa0fce30e41c`.
They retain 15 raw identities and 14 distinct exact bodies, 79 messages, seven
notes, 34 fragments, 169 ordered events and seven central endpoints. The five
numbered messages belong only to the body with the authored `autonumber`
keyword; removed configuration does not number another body. Models were
authored from source before viewing rendered candidates. These are new
obligations awaiting their strong geometry gate and snapshot review, not
promoted official passes. Original raw files and all prior snapshots remain
independent unchanged requirements.

### Fragment accounting and fifteen-body native source gate

The accounting correction closes the P2 condition above at production SHA-256
`a786a444871e3443d11813fa777c889b2fce119e6e79305017c139df23640447`.
The sequence entry charges source bytes before parsing; layout charges typed
entries, message/caption UTF-8 bytes and event traversal in the same Work before
the existing Canvas charges. No limit increases or budget resets are introduced.
Native source controls exercise an exact 64KiB comment-padded positive and a
65,537-byte complete-source fallback, plus the complete kind/condition 256/257
display boundary. The complete debug regression log passes 43 of 44 tests;
the complete Go101 batch remains red.

The independent fifteen-body facts are promoted byte-for-byte into
`tests/fixtures/mermaid_native/sequence_official_fragments_facts.json`, retaining
SHA-256 `74ead1f2d193632e242fccfe083467d2c47313776849537a2a97fa0fce30e41c`.
The native codec maps every explicit field, arrow/central/number fact, note
binding, fragment parent/divider and ordered event into the strong cell oracle.
It locks every raw/body hash and exact UTF-8 suffix, all 15 record identities,
14 distinct bodies and their complete typed-count totals. All three widths
retain sentinels and exact restoration; original raw files still require source
fallback. No renderer output supplies expected source facts.

`sequence-audit/fragment-official-native-debug-tests.log`, SHA-256
`b970ed98aad2399e6b5a433186e95f8ede79f43e1defee27ae7ee8e5312043f1`,
records 44 passes out of 45 tests at test SHA-256
`637d467d384ba684e7d321c8250f4f2abe31514d6ac49493080191fa722891c9`.
The fifteen-body strong source gate passes; only the complete Go101 test fails.
This permits separately reviewed snapshot candidates, not an automatic snapshot
rewrite. The official manifest, prior 63 snapshots and raw-file denominator
have not been promoted by this checkpoint; no complete-suite graduation is
implied.

### Fifteen sequence body snapshot review

Independent review approves the fifteen sequence-only snapshot candidates at
manifest SHA-256
`ad4550e79d3594a8d7b25413d69f1f34007e7def7693f2e07ccbe3c15195e00e`.
Every complete glyph capture was read against the earlier independently authored
model: actor aliases, complete messages and explicit breaks, numbered/central/
unheaded/cross/dotted endpoints, self returns, note ownership and leaders,
closed nested fragment kinds/conditions/colors, complete authored branch order
and following Markdown. All previous 63 snapshot hashes remain identical; all
15 new file hashes match their candidate ledger and filenames retain the path
digest invariant. Exactly these fifteen diagnostic entries change; all 2,145
original raw metadata and expected-source entries remain unchanged.

The manifest now declares 78 drawing body projections and 515 source body
projections. The simple sequence and the two max-width examples have identical
native glyph projections while preserving their separate immutable raw inputs.
The review record is
`full-obligation-audit/official-fragment-fifteen-snapshot-review.json`.
It certifies default body semantics, not the removed configuration or foreign
background paint. It does not imply that the entire official matrix passes:
the actual generation test is red because a separate original XY width4000
input newly draws. That independent chart admission requires its own source
model, native gate and review; the red result must remain visible until resolved.

### Next test-only component admissions

The scratch ER typed-table component is statically approved for three handmade
native tests only. It verifies each independent field column, ordered row,
blank cell, exact internal junction and aggregate attribute budget before
reporting table evidence. Outward boundary arms deliberately remain the existing
whole-scene endpoint/path/ownership oracle's responsibility. This component
cannot independently certify an ER source or authorize production; integration,
adversarial whole-scene tests and actual native execution remain separate gates.

The next sequence activation design may proceed to test-only oracle authoring.
Heavy center lifelines and offset nested strips match the original Go heavy-line
and continuity requirements more directly than adding another closed-frame
identity model. The frozen Go101 source denominator contains 12 opens and 11
closes across 11 records; other activation source/function obligations remain
separate. The profile must define visible open/close event rows and strip
endpoints, preserve unclosed terminal intervals, and distinguish successive
empty intervals. The original `TestActivationParticipantNamedActivate` source
is not a Go101 frozen input and requires explicit accounting for its unambiguous
message-ID extension or a pending source obligation. No activation production
change or complete original-activation-suite claim is approved at this stage.

### Bounded fragment dual-preset and real-terminal checkpoint

`sequence-audit/fragment-limited-dual-verification.json` covers four executables
under each preset: sequence45, relations30, flow23 and Markdown42. Independent
review checked all eight complete log hashes and matched all 140 test names
and statuses across presets. Each preset passes 139 tests; the only genuine
failure is the complete Go101 cohort, with 80 native semantic passes and 21
source fallbacks. All five actual debug/release report files are byte-identical:
sequence6, Go101, relations62, flow and ER296 C1. This bounded run does not
include chart or whole-official/repository graduation.

The new `mermaid-sequence-fragments/verification.json` under the absolute
visualization root records release snapshot SHA-256
`9277106c7875f13872c856f8219fee8999d09931d62712b5fcdc4fbc83d8eb0b`,
bound to reviewed sequence source `a786a444...40447`. A real OS PTY child,
PID6892, executes it from `/private/tmp` with deterministic local fixture SSE.
Independent review verified three exact frozen Go inputs and their response/SSE
hashes, all six screenshot hashes, PTY process/resize records and cleanup.
The complete nested-alt capture includes its following sentinel; the earlier
cropped screenshot alone does not prove that complete capture. Other screenshots
show the loop with the third lifeline and break/rect structural color disclosure.

The same child resizes 160→40→160 while the response count remains two. At40,
the loop correctly shows ordinary complete-source fallback; at160 it draws
again. The complete wide/restored PNGs are byte-identical, SHA-256
`22b110d1275d7fc4cfdc63d9b6db3498822261187f6a843795df2c9cc1f33869`.
The retained first attempt received a local HTTP409 after duplicate startup
prompts; its raw terminal/error archive remains separate from the successful
attempt. Both attempts' servers/children are confirmed absent and the tab is
closed. This is bounded sequence rendering evidence, not hosted inference,
chart certification or a complete-suite pass.

### ER table component native gate and integration review

The component's three handmade native tests pass; its entire33-test binary also
passes in `relations-audit/er-typed-table-plus-existing-debug-tests.log`, SHA-256
`86b3a3bd9c529d82430a6329c8ea321a70a7c0cb0f08bbaba4223833d5f09b4f`.
Test SHA-256 is
`6b655b016a25a9e7b31971711fc16f09e9a6108a09326573294930ac3804c9bb`;
the previous273106-byte test prefix remains exactly `a14f2f49...788a6`.
The first62 and ER296 C1 reports remain unchanged. This closes the component
gate only; all296 complete ER certifications remain zero.

The scratch whole-scene integration is approved for test-only handmade execution:
typed tables must pass the complete component before candidate selection and
again before claiming; every additional outward arm must belong to a selected
real endpoint normal. Internal table junctions grant no generic route exception.
Original topology, markers, multiplicity, direction and orphan-ink checks remain.
This does not authorize corresponding ER production changes.

Independent source review found a P1 profile-accounting omission in the proposed
257plain/39pending split: seven inputs contain inline style/classDef/class
bindings that were absent from the metadata-based pending classification.
Their complete source paint/style obligations must be recorded explicitly
before locking that cohort split. They cannot disappear because the foreign
diagnostic model omits style fields. The component/helper tests may proceed,
but the proposed cohort denominator/profile admission remains under correction.

The source-only correction subsequently closes that P1 at ledger SHA-256
`966bfe56306c25a19221d1710971fa1c424518785387b187ee1b3598d084005c`.
All296 records and293 exact sources remain, with 250 plain typed profiles and
46 explicit pending profiles. The complete inline-style audit retains 11 records,
38 complete statement/binding witnesses and33 property witnesses; independent
review verifies every UTF-8 source span, raw/value field and hash. All eleven
style-bearing records retain a style-profile obligation, including the seven
previously misclassified plain records. This correction changes no native
result, source or vendor expectation. The whole-scene helper remains test-only
until its adversarial native gate passes; ER production is not approved here.

### ER whole-scene helper and activation component gates

The ER typed-table whole-scene helper initially failed two missing-outward-arm
mutations. Its selected perimeter check now requires actual outward normal bits
to equal selected real endpoint normals, rejecting missing as well as extra
connections. The negative expectations remain intact. At test SHA-256
`7feea425f56d2189c261cd051f93a4cfad90ed9547e595706c514035c16d80d7`,
all five helper tests and all35 existing/component tests pass; the complete
log SHA-256 is
`93329654bc79b3ec439ca70a1ba102aa0a13b508c251b193d9fa100425dc0ed6`.
Whole-table exchanges remain individually legal tables but fail the unchanged
identity/topology gate. The250plain/46pending cohort may now collect its full
296-case true-red native feature baseline. Typed-field matches still do not
certify original predicates or deferred profiles, and production remains
unauthorized by this helper checkpoint.

Independent review of the first activation-only component found owner-strip/cap
overwrites across adjacent identities and collapse of different explicit events
onto one row. The added adversarial cases first failed, then passed after
complete per-owner reserved bands (including inactive identities), globally
unique explicit anchors and a consistent trailing-row rule were enforced.
True sixteen-depth handmade geometry and terminal-unclosed cases retain full
cell checks. Five component tests pass at test SHA-256
`bb51d782517fd3f9e52f4f7aa9ae4e07335c35853564de3cf6039717b0a02c3e`;
the log SHA-256 is
`6e8fabc72cf58f075067360e56a5929d4815c63a5b690d2c5b34398c6b34fe3c`.
Sequence production is unchanged. Only whole-scene activation source-event,
independent cap-discovery, route-weight and verified-occlusion test authoring
may proceed; this isolated component permits no activation production change.

### Official raw drawing snapshot registration design

The official test's temporary assertion that every original expectation must
be source is not part of its declared drawn/source schema. Test-local removal
of that assertion and registration of reviewed raw drawing snapshots is
approved in design. Raw names must be `digest(path)[:16].original.txt`, while
body diagnostics retain `digest(path)[:16].txt`; both tracks must register their
safe files in the same exact inventory and explicit generation-missing checks.
The two independent outputs must not share a snapshot when metadata differs.
This changes no production interface, public configuration or corpus schema.
Only a separately source/geometry-reviewed raw expectation may be promoted;
all2,145 original files and593 diagnostic input identities stay in their own
denominators. Candidate raw drawing generation and subsequent snapshot/normal
native review remain separate gates.

### First complete-original XY native-profile snapshot

The subsequent source, native-profile, local test implementation and snapshot
review closes the raw-wide candidate gate. The complete immutable source hash is
`c6c68096d1acd12f38c5d8c402e2223bd475a716a25c1a36b87b061825a4f0a6`;
its independently authored native model retains title, both axis titles, numeric
x0..10/y0..100 and every one of the two named ten-point series. The new snapshot
`323d5b665900bad0.original.txt` has SHA-256
`d8173d3584193b9788505c7e169fc1a0c9945e255b34b6e88d099f4a7e0935f2`.
Review checked both plotted routes, calibrated ticks, complete CPU/RAM legend
and composite ownership, and following Markdown. The source4000-pixel width
uses explicitly reviewed bounded terminal measurement; no SVG pixel/browser
assertion parity is claimed.

All previous78 snapshot files retain identical hashes. Only this one raw
expectation changes, while the other2,144 raw declarations, all593 diagnostic
declarations and all2,145 original file bytes/metadata remain identical. The
test-local registration enforces separate original/diagnostic suffixes and
unique exact inventory entries, preserving safety, symlink and missing-file
checks. The explicit generation test passes1/1; normal debug and release
official tests each pass4/4. Both reports are byte-identical at the current
manifest and report hashes recorded above. This closes the earlier independent
XY raw admission failure without discarding its archived first-red history.

### Activation whole-scene design admission

The whole-scene activation design is approved for test-only authoring. It first
verifies existing structural owners, then discovers complete unclaimed caps in
the full authored event order, and finally verifies every recorded intersection's
actual mask, active/inactive weight and solid/dashed role before projection into
the isolated component. Failed candidates retain no claims. Authored message
labels cannot occlude strips; only complete independently checked note/title
regions may explain opacity. Actual caps and unobscured strip cells are copied
without repair and global ink ownership remains mandatory.

The complete cap footprint must stay inside its authored fragment branch.
Vertical self-return legs cannot share another owner's active nested column,
even when their glyph bits coincide; only the specified horizontal crossings
and real endpoint junctions may share activation ink. These require concrete
adversarial native witnesses before a production gate. The isolated component
results do not approve activation production or change the Go80/21 count.

### Frozen XY-boundary unified verification checkpoint

`charts-audit/final-xy-boundary-dual-verification.json` and the separate
`final-xy-boundary-frozen-inputs.json` identify the reviewed stage before the
complete ER296 typed feature baseline is appended. Independent review hashes
all14 current production/test/manifest inputs and finds no drift from either
freeze record. The unified debug/release build logs finish successful linking
of the requested executable and test targets. This is incremental native build
evidence, not a claim that every repository test was executed.

All14 complete test logs were independently parsed and hash checked. Each preset
runs the same198 distinct test names: sequence50, charts41, relations35, flow23,
Markdown42, official4 and terminal3. Each passes196. Both genuine red tests remain
visible: `MermaidSequenceSemantics.CompleteGo101NativeBaseline` and
`MermaidChartsSemantics.CompleteMermanXY102Batch`. Every family's complete
run/pass/fail name sets match across presets, not just its aggregate count.

All11 archived reports under
`charts-audit/final-xy-boundary-frozen-reports/{debug,release}/` match their
recorded hashes and each other byte-for-byte. Go101 remains80 semantic passes
and21 whole-source fallbacks. Merman XY102 retains35 active rectangular-oracle
matches, two active-profile source fallbacks and65 unresolved profiles, with
zero complete geometry/empty certifications and all108 original function-case
applications pending. First36 charts, Termaid XY23, sequence6, relations62, flow,
ER296 C1, official and complete terminal observations retain their separate
denominators. Unverified terminal drawing observations are not semantic passes.

This checkpoint records bounded dual-preset evidence only. It does not close
either red cohort, original conditional assertions or the complete upstream
suite. Subsequent ER296 typed-baseline and activation whole-scene changes are
new stages and must not be attributed to these frozen198 test results.

### ER296 typed pre-production feature gate

Independent review verifies the complete first-red native log
`relations-audit/er296-typed-first-red-debug-tests.log` at
`3e7ae4460bfb396ff64d84e72c638ba3588942bc87c3fbfc641eccab983aeb93`:
36 distinct tests run,35 pass, and the new complete typed cohort test fails.
Its177 traced case IDs exactly equal the report's177 cases with unmet
obligations. Ordinary diagnostic `Failure` lines are not case IDs.

The report `er296-typed-preproduction-baseline.json` hashes to
`54f1d2d8e62a50f7da9a883350d307e32a7ab1c44506146fac8d832da277dd69`.
It retains296 records/293 exact inputs: the250 source-declared plain profiles
have119 typed geometry matches,36 geometry mismatches and95 source fallbacks;
the46 deferred profiles have two drawing observations and44 source fallbacks.
All296 restore identically, with no oracle/execution exception and zero complete
geometry/empty certifications. Original165 functions and269 imported
applications remain separate pending assertion work.

Executed test source `ce75b834d67b161c2ef7039c2e517e574f5dd3e491545773817dd2b47b56d125`,
unchanged relations production `81f5cbc717e4edb8d59dbc15e4eb3fc900a61e447d2f06691010102435073108`,
typed source-profile ledger `966bfe56306c25a19221d1710971fa1c424518785387b187ee1b3598d084005c`
and field facts `44911ea88baad20091c642c3c1ca502050d30de46e50ae0185d86285958e4d44`
match independently checked files. This permits the previously reviewed local
plain typed ER implementation. It does not approve deferred profiles, relax
source expectations, certify all296 cases or graduate the complete ER family.

### Activation whole-scene executable oracle gate

The private whole-scene validator now binds complete unclaimed caps to the
ordered activation events, checks the whole cap footprint against fragment
scope, and verifies actual active/inactive intersection bits and weights.
Selected message paths carry horizontal/endpoint/vertical/marker roles;
vertical self-return borrowing is rejected even when its glyph resembles a
strip. Only fully verified note/title cells may be opaque. Authored message
text cannot replace activation ink. Actual cap/body projection, the component
validator and the final global ownership sweep use one cumulative Work budget.

Independent static review finds no material defect in this limited gate. The
complete activation component/scene log
`activation-scene-complete-debug-tests.log` at
`1f27b4207b156cc68e1a68613332e6062cfa52c0f0ad661e4ff13d94a4c00270`
passes10/10 tests. The subsequent complete sequence first-red log
`activation-source-baseline-debug-tests.log` at
`5b31d67e0e5b26044366cb0b733e4313d35886aa81ca57b0b77814683c8d5ad0`
runs57 tests, passes55, and retains two genuine failures: the new authored
activation-source forms and the complete Go101 cohort. Go remains80/21.
Test source is `e83f6efaf4146a23f4fc33122315c676f10fd621d3833ee6c22de64318e9e375`;
production remains `a786a444871e3443d11813fa777c889b2fce119e6e79305017c139df23640447`.

The Go codec retains all12 activation and11 deactivation events in source
order, while other pending box/title/event obligations remain pending. The
bounded private production proposal may proceed to implementation, with the
declared16/17 depth and512/513 event controls and explicit three-state sentinel
checks required for its production evidence. The original named-activate
function remains a distinct pending compatibility obligation. This approval is
for the oracle and implementation design; no activation production or complete
Go assertion suite is yet approved.

The subsequent activation-only row-spacing adjustment is also approved as a
design change: each event has a distinct row in global source order; two caps
of the same participant are at least two rows apart, preserving a body row or
an inactive thin gap. Different participants may have adjacent event rows, and
two trailing rows preserve the closed-interval tail. This allows128 authored
four-event cycles to exercise512 events within the unchanged1024-row canvas;
event513 must still fall back. The source-authored boundary test also requires
depth16 drawing versus depth17 fallback and three-width restoration. These are
test-first implementation requirements, not evidence that production activation
already passes. The draft's former fixed three-row advance must be updated to
the approved spacing before implementation is recorded as conforming.

The expanded pre-production source/resource baseline actually runs58 tests,
passes55 and retains three genuine failures: authored activation forms,
activation depth/event boundaries and complete Go101. This is the complete
`activation-resource-first-red-debug-tests.log`, not a projected count after
production edits. Static review of the first unbuilt private production
candidate finds a local performance/accounting issue: newly added text marking
recomputes `label_width` in its per-cell loop and both text/note marking loops
run even without activation. Cache the measured width and guard these new
traversals with `has_activation` before build verification. Other reviewed AST,
ordered interval, ink-role and heavy post-pass changes remain limited to the
private sequence unit. No final production gate is inferred from static review.

The performance/accounting finding is statically closed in sequence candidate
`9cef6a0361d393ad1074989c6d0a001d4a97d485c8732c30e05fda7d9532d3ea`:
label width is measured once per message and the new text/note/self-vertical
marking traversals execute only for activation scenes. Build, actual cohort
geometry, unchanged non-activation reports and dual-preset evidence remain due.

### First activation production debug checkpoint

Independent review checks every hash in
`sequence-audit/activation-production-first-debug-verification.json` against
the actual files, including sequence production `9cef6a0361d393ad1074989c6d0a001d4a97d485c8732c30e05fda7d9532d3ea`
and test `75a2a861c97f2861b6c34f54a1c09b57ae6364a663b10fa9e7d2bb4ab4d934fc`.
The complete build log finishes all8 steps successfully. The complete test log
at `6023b8689358ba84d52b49e153ef23539c3b4c1b193b1edf67a6717eac667243`
contains59 unique run names and58 successful names, matching the verification
record exactly. Only `CompleteGo101NativeBaseline` remains genuinely red.

The authored activation forms, exact four upstream underflow-source controls,
fully drawn16-depth and512-event boundaries,17-depth and513-event whole-source
controls, three-state sentinels and11 component/scene/codec tests all pass.
The first-six report remains byte-identical at `62ee365414fb3c47f2747b27e81c7f529f51c11ffe6fbd48a0ff3737c7b81012`.
The Go101 report at `045bfe9ab3779ec75fe8d381677465ba9d984b863ba8a90af36167b99d5bddd1`
contains101 distinct IDs:88 drawing passes, one valid-empty pass and12 genuine
source-fallback failures. No non-fallback case has a geometry error. Every
case's original conditional assertion mapping remains explicitly pending.

This closes the first bounded activation implementation debug gate and permits
source/design/test-first work on the next unresolved sequence dimension. It
does not graduate Go101 or authorize an unreviewed next production feature.
Release, official admission/snapshot review, complete original assertion ports
and real PTY validation of activation remain pending and must be recorded as
separate evidence before broader graduation.

### Official activation source-only expansion inventory

Before running a new official admission matrix, independent source review locks
all193 sequence diagnostic raw/suffix hashes and UTF-8 byte offsets against the
current official manifest. Exactly11 bodies contain actual activation syntax.
Three complete bodies fit the reviewed activation profile: the overflowing
nested opt example, nested literal-color rect example and central connections
with activations. The remaining eight retain concrete unsupported source-byte
witnesses: four body-local theme init directives, one wrapping init directive,
one lifecycle/metadata/numbering/wrap example and two typed participant metadata
examples. The182 bodies without activation are retained without a new verdict.

Scratch `official-activation-three-source-facts.json` hashes to
`adb9fca1aac99265a42934c050cf22141da401a58a200585cbc61fb8463db9a6`;
`official-sequence-193-activation-source-audit.json` hashes to
`2e56e5e9236a2bf92b587e02a69b4e23c95987223b246a0520021cbbd0bf1418`.
These manually transcribed independent models contain15 participant applications,
21 messages, six fragments,47 ordered events, nine activation openings and five
closings. The two task examples deliberately retain their authored unclosed B/E
intervals and close C/G after their return messages. The central example retains
all five authored circle endpoints. No positive source contains a note, so this
expansion does not claim new note coverage.

The maintenance script only serializes these literal source facts and checks
immutable source spans; it executes no renderer or Mermaid parser. Existing42
sequence diagnostic drawing declarations, all official snapshots and all raw
expectations remain untouched. This inventory permits a strong source-model
native gate, not snapshot generation, promotion or an original official test
pass. Raw frontmatter/configuration remains a separate obligation.

### Activation dual-preset59 and three-source gate review

`sequence-audit/activation-dual59-verification.json` is independently checked
against every archived file hash and both complete logs. Each preset executes
the same59 unique names, passes58 and retains only the genuine Go101 failure.
Both Go101 reports and both first-six reports are byte-identical at the hashes
recorded in the first debug checkpoint. The test-source hash is checked against
the archived `activation-tested59.cpp`, not the later60th-test working file.
This closes the bounded activation release/debug parity check without claiming
official promotion, full upstream assertion execution or real PTY evidence.

The three-source facts are copied byte-for-byte into the independent native
fixture at hash `adb9fca1aac99265a42934c050cf22141da401a58a200585cbc61fb8463db9a6`.
Static review of `SourceReviewedOfficialActivationBodies` confirms complete
typed model mapping, source/raw/suffix hash and byte checks, full47-event count,
240/40/240 geometry, sentinels and raw-source preservation. This permits running
that source gate; it is not yet execution evidence or snapshot approval.

The next private safe-display design may proceed test-first with two explicit
constraints. Removing global C0 rejection must not turn embedded NUL into a
statement boundary: the existing parser's end-of-line sentinel must be tested
by physical index, not by a source byte equal to zero. Label-internal NUL must
be disclosed inside the same complete label, while structural NUL remains
invalid. Also, strict `validated_plain_label` checks apply per decoded explicit
break row, preserving legal blank rows, with one aggregate256 display limit;
the complete multiline string cannot be passed to a helper that rejects LF.
No new safe-display production is approved by this review.

### Official activation source transcription correction

The first execution of the three-source strong gate exposes a P1 error in the
independent expected facts, not a renderer defect: both task sources end with
`D -->> A: Complete`, but the manually transcribed message index8 incorrectly
declared a solid shaft. Independent review re-reads all21 source arrows and all47
ordered events. Exactly two facts change, `/cases/0/model/messages/8/dashed` and
`/cases/1/model/messages/8/dashed`, from false to true. No other field, original
source, source hash, actor, label, fragment or event changes.

The corrected scratch facts hash is
`4a2d69cb97ec62844d757d7435cef6a10df7751f8e28aa369d10775f4fd494c5`.
The original `adb9fca1aac99265a42934c050cf22141da401a58a200585cbc61fb8463db9a6`
bytes remain in `official-activation-three-source-facts.before-dash-source-review.json`.
The complete21-arrow source-line/UTF-8-span and exact two-field delta review is
`official-activation-source-dash-correction-review.json` at
`016803259e8fd7933f49ddc2c1ff9d9d77c23bc1558364cacc669a9482513090`.
The first-red native and LLDB diagnostics remain archived. Expected facts are
corrected from unchanged source bytes; output glyphs are not the expectation
oracle. The193-source classification audit remains unchanged. A new execution
with the corrected facts is required before snapshot promotion.

Static review also permits running the next three safe-display tests: the
independent canonical/literal escape-grid mutations,14 source/canonical scalar
tuples, and complete NUL/break/expanded256/257/invalid-UTF8 source controls.
These are test-first admission evidence only; current production has no new
safe-display codec. The NUL-boundary and per-decoded-row restrictions above
remain mandatory implementation constraints.

### Corrected official activation source gate and safe-display first red

The complete `activation-official-safe63-first-debug-tests.log` at
`bf704412024067d1a691e61a1c386b4cff4f61dde5784b1a42d432176c8d3544`
actually executes63 unique tests and passes60. Only the two new public
safe-display feature tests and complete Go101 remain genuinely red. The
corrected three-source official activation test passes. Sequence production,
test, executable and corrected facts match their frozen input hashes; later
independent changes to the chart owner are a separate stage. Go remains89/12
with no non-fallback error, and the first-six report remains byte-identical.

This permits updating only the three reviewed diagnostic drawing declarations
and generating three candidate snapshots for subsequent independent source and
topology review. All raw declarations and the prior79 snapshots must remain
unchanged. No complete original official test pass is implied.

The actual safe-display red baseline permits the reviewed sequence-private
implementation: strict scalar validation, canonical unsafe/leading-mark
disclosure, doubled literal backslash, physical-index NUL handling, strict
structural IDs, per-decoded-row validation and one aggregate256 display cap.
Every decode/output scan uses the existing cumulative production Work budget.
Shared Unicode/Canvas contracts, public interfaces and other families stay
outside this feature. Final implementation review, actual geometry and later
dual-preset/PTY evidence remain required.

The first sequence-private safe-display production candidate
`851d1b7e4f32d1cf6247fc35700a457651d562c2e96d53bb20c5ea4a167284bf`
passes independent static review. The strict local UTF-8 walk rejects malformed,
overlong, surrogate and out-of-range inputs; canonical disclosure, literal
backslash doubling and base-context reset preserve distinct source identities.
Decoded rows retain blank-row structure under the existing aggregate cap.
Physical-index statement termination fixes the embedded-NUL boundary issue,
while structural identifiers retain the strict shared Unicode validator.
Parsing and output validation share the same cumulative Work; shared Unicode,
Canvas and factory code are unchanged. This is permission to execute the frozen
native feature gate, not evidence that the feature or entire Go suite passes.

### Safe-display first production63 and obsolete tab expectations

The complete first safe-display production log at
`5cb66e76b1f9e9abd1f7199aaa1bf944d0eacc5332652cc39cebecf552969269`
runs63 tests and passes61. The two actual failures are the complete Go101 test
and an older source-admission test containing three tab-only decoded rows.
All14 scalar source/canonical tuples and the complete NUL, decoded-row,
expanded256/257 and invalid-UTF8 controls pass. Go advances to90 semantic passes
(89 drawings and one valid-empty), with11 source fallbacks and no non-fallback
geometry error; the first-six report remains unchanged.

The three unchanged alias/message/note inputs containing `x<br>\t<br>y` now
have the source-contract canonical label `x\n\\u{0009}\ny`. Their old fallback
expectations predate safe disclosure. Independent review approves replacing
only these expectations with complete typed positive models and three-width
geometry/restoration/sentinel checks. The initial self message remains present
in every model; the alias changes the final header, the message appends a second
self return, and the note remains after the first message. Other note-count,
target, markup and label-limit rejection controls remain unchanged. This is a
source-contract expectation update requiring a fresh run, not retroactive
success for the recorded61/63 first production result.

The reviewed tab update executes successfully in the archived63-test source
`98666a00d1f6418c078216602c033a1490eb82c14db6912a896b79149cfc7873`.
The complete log `143e3d58886b73b152551b184f744b73e763724c24fce46d649e04d6b22c1add`
passes62 and retains only the genuine complete Go101 failure. Its report at
`14aa26b2144537b82e198bb45b5d2ed779ce5451c7b15e278d1f4963d4819b21`
records90 semantic passes/11 source fallbacks; the first-six report remains
unchanged. These evidence hashes precede the next quoted-default-header fix.

A new default-header ambiguity must be tested before the next safe-display
production change: a quoted identifier containing literal backslash-u text must
not display the same header as an explicit alias containing the actual unsafe
scalar. The source-backed64th test distinguishes both complete actor identities
and the message binding. A pure private presentation-row function may serve
default headers while keeping sequence TXT grammar rejection and explicit-break
decoding in `sequence_label`. Raw IDs remain identity keys and keep strict
validation; quoted `<br>` stays literal text. Only actual first-red execution
permits implementation of that local display-path closure.

The quoted-default ambiguity is reproduced by the complete one-test first-red
log at `4504bb72a7d1cdb1eff74d6cba45f997c2538b16550a4c3723cb8787fb68a05c`.
The subsequent private candidate
`cb2c6d1f0ab03cef5f1e7be661e77146657974778abbb99c6d968513ad310189`
passes static review. Default presentation is finalized after all declarations,
only for participants without an explicit alias. The raw ID remains unchanged;
a later short alias must not be rejected because an unused expanded default
would exceed256 cells. Independent source controls include both early and late
aliases for a200-backslash identity, plus the exact literal `<br>` default.
The pure presentation-row decoder is separated locally from sequence TXT
grammar checks. Native64-test execution and later dual evidence remain due.

### ER label witness identity review

Independent review identifies a true oracle false acceptance: separate business
edges can bridge different rows of one printed multiline label and reuse its
text cells. The native handmade first-red log at
`4ab21d0f1f16dd604b01aa2e12a424ba343a8e2f5fb6cc99c75bd6009234d023`
confirms the invalid shared-label scene was accepted, while its distinct-block
positive passed. A separate complete multiline-prefix positive also reveals
the existing BFS parent-cell representation discards another complete label
identity at the same landing point.

The bounded test-only correction is approved for execution: retain alternative
complete label identities only for the same single physical horizontal bridge,
and require selected labels' actual text cells to be independent across business
edges, including overlap between differently indexed prefix candidates. Path,
port, marker, stroke and global orphan ownership checks remain unchanged. Path
copying, label-cell claims/backtracking and initialization retain the existing
cumulative Work and trace caps. No label is selected merely because it is the
longest prefix, and no fixture expectation changes. Full native reruns remain
required; this review does not certify all ER296 or its original assertions.

### Reviewed safe/default64 and official activation snapshots

The complete safe/default debug log at
`d0428d0c4b1270ab3ba22eb1f9c25fd3709f5fe41be225a712b655635185865d`
executes64 unique tests and passes63, retaining only complete Go101 as a genuine
failure. Source `cb2c6d1f0ab03cef5f1e7be661e77146657974778abbb99c6d968513ad310189`
and test `83816caf0d11088bce01a97f15d86bfee55609b5cf545641e0495d4e6646aabb`
are independently checked. The quoted identity collision, both200-backslash
alias orders, literal `<br>`,14 safe scalar tuples and the NUL/row/display-limit
controls pass. Go remains90/11 with zero bad drawings; its report is
`14aa26b2144537b82e198bb45b5d2ed779ce5451c7b15e278d1f4963d4819b21`.
The first-six report remains byte-identical. This closes the bounded debug
implementation gate, not release, original assertion or PTY graduation.

The three activation diagnostic snapshots pass independent full source/model
and glyph-row review. Their hashes are:

| Snapshot | SHA-256 |
|---|---|
| `58f7bd922bc3db88.txt` | `4fcdd77f446f8822065b750e716d144961401935aeb6e5582bde7a580c33e578` |
| `e3ea2424e49c9864.txt` | `10704fe3599d5bc2adb4182de26d3cc92ab35ee288b11c9f9fcca3b3577b0f1a` |
| `c3880a4e1e41ce83.txt` | `e75db39e524d991307ea3b5912ff1e967a15db9e8a6423dc5293af936f12a244` |

All21 messages and47 events agree with corrected source facts, including final
dashed Complete messages, all five central circle endpoints, fragment captions
and nesting, complete self return and intentional unclosed B/E intervals. All79
prior snapshots retain their hashes, all2145 raw source hashes are independently
checked, and only the three reviewed diagnostic declarations change. The other
590 diagnostics and every raw declaration remain unchanged. The resulting82
snapshots and candidate generation4/4 permit normal official verification; they
do not constitute original browser/rendering-assertion completion. The scratch
review at `c1b10e738f86282617e79cb21751090bf4671e42ac9f3b50c10e9c16433ca3d0`
retains this decision.

### Next complete Go sequence source-property cohort

Source-only review partitions the11 remaining Go records into four box-only,
five lifetime-only, one mixed box/lifetime and one frontmatter/title source.
The next complete box cohort contains all five box sources, including the
mixed record that must retain its lifecycle failure. The pinned Mermaid docs
describe vertical participant groups; `sequenceRenderer.ts` measures each box
through the completed timeline. The pinned Go `TestBoxRenderStructure` also
requires closed body-side borders and a genuine shaft crossing. A header-only
box would weaken both source and original-test semantics.

The native oracle design may therefore proceed test-first with full-timeline
closed groups, exact member sets and outside exclusions, complete title and
separate color metadata, actor/figure compatibility and independently owned
crossings. New boundaries cannot swallow message, note, fragment or activation
ink. Include the complete six-function `box_test.go` inventory and both
late-declaration/box-claim functions from `activation_test.go`. Original hex-color
positives remain explicit Go extensions despite primary Mermaid documentation
rejecting that color notation; duplicate-declaration/API error differences need
their own applicability review.

`go-sequence-remaining-eleven-and-box-primary-review.json` at
`c1ec315280a0c3c807e86a5fdb37a5582b2216a0e7d124026f2c6092d7a8df8a`
locks all11 sources, primary full-timeline evidence and complete original
function inventories: box6, lifetime9, activation15 and frontmatter2. These are
function counts, not independent assertion/parameter-application counts.
Imported `not_contains` objects can encode a failure branch of an original
positive condition; for example, note-preservation assertions require keeping
their text, not globally removing it. Full conditional source mapping and
native predicates must accompany further feature work. No box production,
YAML/shared-factory change or complete Go suite is approved here.

The box/fragment interaction review finds a source-backed design conflict:
the leading-combining box fixture contains a full-width loop whose complete
caption compartment necessarily intersects a participant-box side. Merely
allowing checked horizontal border crossings is insufficient if every caption
padding cell still requires a visible vertical box boundary. The accepted
test-only resolution uses a dedicated caption-occlusion bitmap populated only
after a complete fragment frame, kind, caption text and boundaries validate.
Only that exact verified caption interior, including padding, may hide a box
side; it must not gain another text claim. Notes, generic body regions and
unbound text do not provide this exemption. Every remaining visible side cell
and real horizontal crossing stays independently checked. Vertical self-return
or frame legs cannot borrow the box side. Native positives for both actual text
and padding intersections, plus altered text, missing visible side, overlarge
occlusion and foreign-stroke mutations, are required before production.

### ER literal/prefix/shared-label bounded dual verification

The local ER corrections graduate their bounded debug/release gate at source
`e4eb389cee407f317beb2366d2215d9ecba555254563e9d4890e89723932b735`
and test `62a0aa2a5fa6359c32485b544b5bbe51a7b8b81230e0b8141ffebe71705f3f9c`.
Independent full-log parsing verifies41 unique run names and40 passes per
preset, with identical complete status sets and only the full ER296 feature
test remaining genuinely red. Debug log is
`c05c77847ab3782ff17d5da65f7589e5d87a6c11e6459434453c0d81207d9fad`;
release log is `f018b5833584f5e6cd6d8eccf87bb1c70008ef11bcedbd04ad2a58d251155901`.

The three archived reports are byte-identical across presets. Typed report
`1b5ed41e5a0372dfd75fc8d6b6aaa59df77cb9599c576a763f1ae49893cf4263`
retains234 plain typed matches,16 plain source fallbacks and46 deferred profiles,
with296 restores and no geometry/oracle exception. First62 report
`37c5cbae6ff2880208835b44a0e694ec8bceb9a6bc714480ef2d244a9f6b6281`
also matches the first typed-production report byte-for-byte; C1 observation
report is `3ff8e5933ce9a9be28ab56f10e0f6d1b7dd900103356ac3d1b5433c99f43f7c2`.
This closes the specific quoted-field, edge-whitespace, prefix-bridge and shared
text ownership findings. It does not graduate plain250, ER296, deferred profiles
or original complete function conditions.

The next private ER label-clearance proposal is admitted for source/test-first
authoring only. Two42-column LR/RL labels cannot fit the existing38-column
canvas; source-derived inter-node clearance is justified without raising limits.
Class/State measurement must retain its exact old algebra. A TB/BT vertical-gap
change alone does not resolve a too-wide cross-axis label, so such failures
remain unmet unless independently specified and tested. No unexecuted clearance
proposal is counted as another passing source.

### Safe/default64, Markdown43 and official81 dual checkpoint

Independent review parses all complete normal logs for both presets:
sequence64/63 passes, Markdown43/43, official4/4, flow23/23 and terminal3/3.
Each preset therefore executes137 distinct names and passes136, with only the
complete Go101 gate failing. Every group's full run/pass/fail name sets match.
The earlier Markdown42 first run failed on a now-supported activation input;
its exact source is retained in a complete positive after the reviewed
expectation migration. That initial failure remains archived, separately from
the subsequent43-test success.

All five actual debug/release reports are independently checked byte-for-byte
and against `safe64-markdown43-official81-dual-verification.json`. Go90/11,
first-six62ee, official0ac096 and flowe719 remain separately identified. The
terminal observation report is
`08ed56276fa30994565ae69640b31009bd8f4707a5bc41f24e4068c9994c91da`;
its unverified drawing observations are not semantic coverage. The seven frozen
source/manifest and archived root-test hashes match this checkpoint. Normal
official tests, rather than candidate generation alone, now pass4/4 in both
presets with81 diagnostic drawings, including45 sequence bodies; raw originals
remain one native-profile drawing out of2145. Real PTY evidence for this exact
stage remains separate. Whole Go101, XY102, ER296 and complete original
conditional assertion translation have not graduated.

### Activation, safe display, composite XY and typed ER real PTY checkpoint

Independent visual review reads all six screenshots under
`/Users/bytedance/.codex/visualizations/2026/10/19/01a15315-ef3b-7953-8e46-ca4aba152240/mermaid-activation-safe-er-composite/`.
The copied release executable hashes to
`0d3980ba95fd731fd414dda87b19814739dd84df3028d00fc06c2401ea67f73f`
and its source ledger binds sequence cb2c, charts281f and relations e4eb. Four
exact frozen terminal inputs exercise owned activation/self return, composite
two-bar/two-line XY, actual U+0001 label disclosure and typed ER columns/keys.
Each selected input, request, response Markdown and SSE byte hash is verified
against its frozen vendor source and capture records. This uses the real
release CLI in an OS PTY and a deterministic local SSE fixture, not hosted
inference or another backend.

One child PID59125 persists throughout all scenes. Kernel resize records show
160→40→160 with response count fixed at two; the wide and restored composite
PNGs are byte-identical at
`e710b6717e3bbb3337a7c79a80349d9ccba174e42a1c4986b1823256fa1e788b`.
The narrow view visibly returns ordinary source and both wide views show the
same plot and series-owner legends. Every screenshot hash matches. The safe
scene displays complete canonical alias/message text; raw terminal capture has
zero U+0001 bytes and six occurrences of each expected canonical field across
redraws. Final response count is four. Child59125 and server58990 are confirmed
absent; the cleanup record also reports the browser tab closed.

One evidence-integrity issue is corrected without altering terminal bytes:
cleanup appends101 terminal-reset bytes after the original capture. The original
verification remains archived; its1371562-byte capture prefix has the exact
old hash `e37ca58a546acaecfda83fa62b3eede2ccf87d72d6fb9a0798d010557508fbe9`.
The final1371663-byte raw capture hashes to
`fb7f8b2e7644d48f272c0c911e8f68cb19e3f77482d86ce6cfd73810a5e76f6a`;
the appended tail hashes to `28f1b77bc5e1f8c337dcffef462875dc433d2eddf7d97d1954bdefbd1079ac64`.
The finalized verification hashes to
`0d05e716b244e77519e8b02e01f128beb9449d170dc9c647677d0d4517022e61`.
This closes only the named frozen-stage PTY evidence, not entire-family
compatibility, pending source cohorts or complete upstream assertion coverage.

### Participant boxes: executable oracle checkpoint before production

The independent reviewer compares the box test-only changes against the frozen
64-test sequence source. The old no-box assertions remain present. Box source
conversion now supplies exact member sets, separately sourced color metadata
and complete display titles instead of a generic pending marker. The mixed
box/lifecycle source retains its lifecycle obligation and remains pending.
All five frozen box records and six box frames remain in the cohort. The nine
original parsing table inputs, role/sibling cases, both late membership claims,
empty-box behavior and invalid forms are preserved as source-based controls.

The whole-scene oracle verifies member headers and actor figures, excludes
nonmembers, binds separate title/color compartments, and checks a closed frame
through the common independent footer. It claims caption and border cells,
never the group's entire body. Only already verified horizontal message/note
routes or fragment boundaries can add crossing arms to a box side. Self-return
verticals cannot borrow it. A dedicated bitmap permits side occlusion only by
the exact interior of a fully verified fragment caption, including its padding;
generic note opacity cannot authorize this exception. All remaining visible
side cells and the final global ink ownership sweep remain mandatory. The
existing shared Work, 256 candidate frames and 10,000 assignments are retained.

The first complete run genuinely executed73 tests and passed70, failing the two
unimplemented source gates and the fragment-caption handmade positive. Its log
hash is `86b257ebb00659468ee8ee789f23203d8866bca3560a719730da654ababd6d22`.
The discovery correction adds box-only candidates from complete first/last
horizontal borders and exact corners, leaving side validation to the strong
binding and final sweep. Generic actor/note/fragment discovery is unchanged.
The revised complete run executes74 tests and passes72; only the new box source
forms and the complete Go101 gate fail. That log hashes to
`4af255c21aa2fb842c907aca3d074500b77290dcfa997d576c4630f403fbfae9`;
the reviewed test source hashes to
`2ab6e85276a229f1451f26c5489d52959d4f923f77e1073562fce8c730a640bf`.

The suspected collision between a box title and an actor label did not
reproduce: its actual run passes1/1 despite the historical log filename
containing `first-red`. No speculative caption-floor rule is introduced.
This approves only the executable test oracle. It is not production approval,
full Go101 coverage, or proof that every original conditional assertion is
ported.

### ER main-axis clearance: limited implementation admission

The independent reviewer reads the complete42-test pre-fix log:40 pass, with
the complete ER296 gate and the new clearance source gate genuinely failing.
The log hashes to
`d36904fd5aa6fa32fd916ed0081ff6d947698e1dca7554db3d137c8304a2a5c4`.
The six LR/RL variants and the additional forward branch remain in the test;
none is removed to obtain a green result.

The reviewed candidate
`73a12c4652f723d997bef48635ca116e74f879bd079582d67e25434f1728f4d0`
is admitted for limited implementation inside the ER owning unit. It derives
the main-axis inter-item gap from decoded edge-label dimensions and endpoint
marker clearance, uses that same gap in measurement and placement, and keeps
the final routing lane at10 cells. Class/State retain their original gap10
algebra. Source limits, canvas/work caps, rank, router and typed-table protocol
remain unchanged. Post-change native execution is still required; the234
existing plain typed matches may not regress. This does not certify all16
remaining source fallbacks, cross-axis TB/BT width handling, or complete ER296.

### Participant-box production design: local layout and color gate

The candidate stays inside the sequence implementation unit. A private group
record and declaration state use existing participant identity/alias handling;
source-ordered contiguous lane units reuse existing header, note, fragment and
activation geometry. The no-box path must remain unchanged. Packing may retain
or enlarge the original message-derived lane spacing, not compress it. Unit
extents include complete owned-note rectangles, self-return turns and activation
cap bands. A complete common footer remains independent of lifeline ownership.
Cross-unit notes that cannot preserve visible sides remain source obligations.
A bounded complete-label interval scan uses the same Work; no shared renderer,
service, Canvas change, public interface or new setting is needed.

Source review identifies a clarification required before implementation: a
balanced parenthesized list of finite numbers does not by itself establish a
valid CSS color. Pinned Mermaid12.2 `sequenceDb.ts:391–418` validates a candidate
with browser CSS support, reverting an invalid candidate to the entire title.
The pinned Go `parser.go:725–772` instead strips functional/hex prefixes and has
149 case-insensitive named tokens, including transparent, while discarding fill
semantics from its model. Its permissive parser is not a complete CSS validator.
The local profile must therefore lock exact functional arities/units and token
boundaries, the Go hex extension, and treatment of unsupported modern or
context-dependent color syntax. It must preserve the complete source spelling
in a separate bounded disclosure compartment. Unknown ordinary words remain
titles; unsupported recognized color forms must not silently lose their color
obligation. Full CSS/browser rendering compatibility is not inferred.

The independent source review is archived as
`full-obligation-audit/box-production-color-source-review.json`, SHA
`6ea34e9e0f4e03a71fd9098d16e8baf222746f21d2a326fc1e220552a1cfbd3e`.
The complete primary database source hashes to
`55b87176caa3e548be11a1582bd1c30317014f7d135ad34deff66a725b40b8e1`;
the Go parser hashes to
`cf447b3edda8724c023a59a9da1533cc411f69f213907b215f1cf3616f217e6c`.
At this design checkpoint production remains unmodified and the exact color
profile clarification remains open. The executable oracle approval does not
close this design item or authorize a full-family compatibility claim.

### ER main-axis clearance: first production debug evidence

The implemented unit matches the reviewed candidate73a12c and the42-test source
matches77872d. Independent parsing of the full native log confirms42 executions,
41 passes and only the complete ER296 gate failing. The log hashes to
`92fbbe541af097d6c4149195d53f610e363d8459c7a052e2189b7060ef9155d9`.
All new clearance variants and the forward branch now satisfy their unchanged
complete geometry, mutation and restoration controls.

Direct report comparison confirms the exact296-ID set is unchanged. Every
previous234 plain typed match remains a match, with exactly five new matches:
`merman/342a9281d34272028e3b`, `merman/3faead10b2198f929523`,
`merman/41ae8afd51abc086eaec`, `merman/4accb2527f09e89b7392` and
`merman/7660ac6e84a4ad39dc23`. The resulting250 plain cohort has239 geometry
matches and11 source fallbacks; all46 source-profile-pending records remain
separate. All296 restore identically and there are no typed geometry or oracle
errors. The first62 report remains byte-identical at37c5cbae, including all57
non-ER records and the five earlier typed ER records.

The new typed report hashes to
`2fd493f7caa4a385a21b1603c612b495ad9fabd5d5505123aa8c2f4da6356925`.
This closes the limited gap implementation review at the debug checkpoint.
Release verification, the remaining11 plain source fallbacks,46 pending
profiles and the original-function assertion ports remain independent work.

### Participant-box color clarification resolved: source/test authoring gate

The revised design hashes to
`542cfb1b6b39d2adbd501269b709020148720eea8123b5e00f44c81a802813b7`.
Independent review closes the color-grammar clarification: all149 source-derived
case-insensitive named tokens retain original spelling; functional colors use
explicit legacy comma arities, finite complete decimal fields, RGB homogeneous
number/percentage channels, HSL hue units and percentage saturation/lightness,
and number/percentage alpha. Whole tokens require a closing parenthesis and
space/tab or EOF boundary. The Go hex extension is explicitly3/4/6/8 digits.
Recognized malformed or unsupported forms retain the entire source rather than
losing a color prefix. Modern/context-dependent CSS and the Go parser's more
permissive5/7-digit behavior remain separate obligations. Exact visible values
do not imply browser paint/clamping equivalence.

The same revision requires complete owned extents and preservation or expansion
of all existing lane gaps, including across group units. The limited design is
approved for independently locked source-fact promotion and public-interface
native test authoring. Expected tokens must come from the pinned source ledger,
not the prospective production lookup. Native controls must execute before the
private implementation, with genuine failures retained. This design approval
does not certify unimplemented boxes, the mixed lifecycle source, all original
parser conditions or full Go101. No shared module or established public contract
change is authorized or required.

### Participant-box color facts and test-first controls

The native source fixture
`tests/fixtures/mermaid_native/sequence_box_color_facts.json` hashes to
`fd3026f26ac5ae37c89fb3309e3999e93326a23e3edd6a33fc33dd848d910f17`.
Independent review checks its complete149-token array against the earlier
source-only audit, the actual pinned Go checkout commit and parser bytes, and
the primary Mermaid database bytes. Its source provenance,725–772 line span,
primary commit/path/hash and explicit terminal profile are preserved. The
existing vendor directory includes the complete MIT license.

The test-only sequence source hashes to
`a115537207c6f2ec5e6673614c783db5e109868ac5fed1d5c368c0494f74b9fa`
and declares75 native tests. Its new color test requires real drawing geometry
for every149 named token and18 independently declared mixed-case, functional
or hex samples, plus same-node wide/narrow/restored behavior and following-text
sentinels. Another23 malformed or valid-but-unsupported forms enforce the local
whole-source policy. Those guards are not foreign parser-error equivalence or
successful drawing credit. The original nine parsing table cases, all five
frozen box records and complete Go101 remain present; mixed lifecycle still
requires its independent obligation. The149/18/23 sample counts are native
controls, not newly inferred counts of original upstream conditional predicates.

The source facts and test authoring pass static review. Actual first-red
execution and subsequent production verification remain separate gates; this
record does not claim the prospective color implementation already draws them.

### Participant-box complete75 first-red execution

The independently parsed full first-red log hashes to
`6f3ce4d3ca2a357e85e19d667672dc80913cefd6bae90463ec02d085f0f7e3f1`.
It executes75 unique tests and passes72. Exactly the original box source forms,
the new complete color controls and complete Go101 fail. All nine box handmade
and codec tests pass. The167 color failures all originate at the unchanged
positive assertion requiring wide drawing rather than ordinary source: all149
named tokens and18 additional positive samples genuinely execute and remain
unimplemented. No negative whole-source policy control fails.

Current source cb2c, test a1155372 and facts fd3026 match this first-red stage.
Together with the reviewed design and executable oracle this admits the limited
box implementation inside the private sequence unit. It does not change any
snapshot expectation or remove the mixed lifecycle, original parser predicates,
full Go101 or broader CSS obligations. Actual production geometry, regression,
dual-preset and real PTY evidence remain required after implementation.

### Box scratch implementation review and ER gap release checkpoint

The independently reviewed box implementation candidate remains scratch-only at
`sequence-audit/mermaid_sequence_box_candidate.cpp`, SHA
`7860745ececb459f46185be428ee826135132ad2729d374d36fcd4d931774a10`.
Its149 production tokens match the independent fixture exactly. The complete
424-line diff implements private declaration ownership, empty-group dropping,
late identity claims, contiguous lane packing, full owned extents, caption
compartments, bounded label intervals and an independent footer. The no-box
coordinate path is retained. Static review finds no material blocker to placing
this exact candidate in the owning unit for native execution; it does not claim
that the uncompiled candidate already passes production tests. Extremely small
finite CSS decimals outside the local `double` representation may still retain
source; that limitation must not be described as invalid CSS.

The separate ER73a12c release log hashes to
`8781f461b2a823641004ed74daf4ff0213c0c86a70faa5a13d27746f6b4df6c1`.
Independent review confirms42 executions,41 passes and the same single complete
ER296 failure. Full ordered run and pass names match debug. All three archived
reports match their actual debug bytes: first62 at37c5cbae, ER observations at
ae97e08f and typed geometry at2fd493f7. This closes the limited ER gap
debug/release checkpoint with239 plain matches,11 source fallbacks and46 pending
profiles, not complete ER graduation. Later temporary routing diagnostics belong
to a separate, unapproved source state and cannot substitute for this freeze.

### Box first production debug result and official source-only cohort

The exact reviewed7860745e implementation is executed with the75-test native
suite. Independent parsing confirms75 runs and74 passes, with only complete
Go101 still failing. The log hashes to
`84622aaf5efb486afd1fe21ddde460dc4a72051f52f24d2c49db675566ed4f06`.
All149 named and18 additional color positives now satisfy the strong geometry
gate. Relative to the first-red source, the only test edit adds an explicit
whole-source policy for finite decimal underflow outside local `double`
representation; the positive expected models are unchanged. The test hashes to
`4f7991adfdd1062ec6ece439160a01eb72403bdd50f85b78f2518a2daf7c8eb6`.

Direct comparison of all101 Go records preserves every previous90 semantic
success. Exactly four box-only sources become successful; the mixed fifth
removes only its now-satisfied box obligation and still fails lifecycle. The
result is94 native semantic successes, comprising93 drawings and one valid
empty document, with seven source fallbacks and no erroneous actual drawings.
The report hashes to
`137ce8dd4d1737cd4bfb019f0ccc95e289cc7b15b85fce3b5d3bc1846a573203`.
The first-six report remains byte-identical at62ee3654. Complete focused flow
23/23 and Markdown43/43 also pass. Official4 executes3 passes and one genuine
matrix failure: one newly admitted box body still has its original source
expectation. No expectation or snapshot is changed at this checkpoint. This
closes only the limited implementation review at the debug stage; release,
official promotion and PTY remain separate.

The independent official box audit checks all193 sequence body projections and
every raw/body byte suffix, UTF-8 offset and manifest hash. Nine bodies contain
authored box statements, representing six distinct body hashes. All remaining24
occurrences of the word box are inspected inside ordinary alias/message/note
text; there are no semicolon-delimited box statements. The other184 sources
remain outside this property audit, not outside their original obligations.
Eight authored-box bodies contain explicit `@{type}` participant shape metadata
outside the native role-only declaration profile; each has a concrete complete
source-row witness. Their types must not be discarded to obtain a positive.

Only `sequence/visual-check/with-boxes.mmd` is independently positive under the
complete current box profile. Its hand-transcribed model contains three actors,
John's two-row alias, exact LightGrey metadata and Alice/Bob membership, all ten
messages with individually sourced arrow/stroke witnesses, the four-row note
right of John after message3, two alt dividers and the empty par divider. Its
complete ordered event count is18. These facts come from source before any
candidate snapshot review. Raw frontmatter/config/theme and browser fill remain
separate unsupported semantics.

The complete source audit is
`full-obligation-audit/official-sequence-193-box-source-audit.json`, SHA
`9372e5ac6ad1592d15e94fc9a426a1842bbbcac660f5a5e3d991af6292711077`.
The independent positive model is
`full-obligation-audit/official-sequence-box-source-facts.json`, SHA
`0a23c6365124ca3402b2253ac4f0a3e30bc357796f3b904717b536d7bc27a8fe`.
It may be promoted unchanged to an independent native source gate; this does
not itself promote the official manifest or certify a generated snapshot.

### Official box witness correction and strong source gate

The193-source box audit has a corrected extraction-only revision at SHA
`1a5fa975a926f6deebf0c1d072eee20e78a39c6000613b01ee394c71768307da`.
The original9372e5ac bytes remain in the adjacent
`before-witness-row-fix` archive. The original `^\s*box` scan incorrectly included
a preceding blank-line newline in six secondary box-row matches; extracting
through that first newline produced an empty witness. Matching only horizontal
space/tab corrects those offsets and preserves the complete actual box rows.
The delta record hashes to
`1fae3cf847bb576cf273a3ac96eafe05ffaaef0b1eccf6329b5106023d626286`.
No193-source hash, eligibility verdict, unsupported-type witness or positive
model field changes. The independent positive facts remain exactly0a23c636.

The promoted native facts are byte-identical to that source model. Their new
strong test hashes the facts, raw file and exact body suffix, loads every actor,
message marker/stroke, note, fragment, ordered event and participant-box field,
checks all ten nonempty source witnesses and asserts the18-event denominator.
It executes240→40→240 with complete geometry, following-text sentinels and a
separate raw-source guard. The first full debug76 run genuinely passes75, with
only complete Go101 failing; its log hashes to
`92e8b0eb8720a25dae15aa2bfc1fe6afc358995f295ee67f8bd25183cea58a45`.
The release76 run has the identical ordered run/pass sets and the same sole
failure; its log hashes to
`7033855cb320c7b2bc0b53436f0683208f8bff478d365d14268be19f339d2fe0`.
Both actual reports match debug bytes at137ce8dd and62ee3654. Release flow23
and Markdown43 pass; the old official4 expectation still genuinely fails its
matrix test before promotion. This closes the limited box dual checkpoint,
not full Go101 or original-condition translation.

The independently reviewed manifest candidate hashes to
`be08f24ad7f570ae9fa40f87dd8b92476748ee0cbfaa216babb2d0e5515b557d`.
Of2145 fixed records it changes only the with-boxes diagnostic expectation.
All raw declarations/metadata and the other592 diagnostics are identical.
Its six filled arrow checks are source-derived from ten messages minus two
cross heads and two unheaded messages; full source labels and branch ordering
remain. This admits generating exactly one candidate snapshot for independent
glyph review while preserving the old82 snapshots, not accepting the candidate
or claiming raw frontmatter/config support.

### Next source-only lifecycle cohort and original-function denominator

The next smallest complete remaining frozen Go property cohort is all six
lifecycle sources, including the mixed CJK box case. It contains20 actor
applications,26 messages,48 normalized ordered events, five create events and
six destroy events. Existing role/alias/safe-display, note, fragment, activation
and box obligations remain attached. The seventh remaining Go source is a
frontmatter title and stays separate; blindly skipping YAML is not a title
implementation.

Independent source review also inventories the full nine pinned
`pkg/sequence/create_test.go` functions and their exact spans/hashes. They
contain21 distinct literal inputs:11 source positives and10 negative policies;
only three overlap the frozen six, yielding24 distinct lifecycle source
obligations in their union. One original input additionally runs both Unicode
and ASCII configurations. These input/function counts are not predicate counts,
and six source successes would not mean all nine original functions pass.

The primary creation contract requires a new identity and binds it to the next
message's receiver; destruction binds to either endpoint of the next message.
The pinned Go terminal projection retains source-ordered top headers, starts
the created lifeline at its bound event and draws a separate owned end cross
after the complete destroying message. Cross-headed messages without a destroy
statement do not terminate lifetime. Note/fragment text and borders must not be
blanked as if they were lifeline cells. All four original intervening-statement
positives, role/alias distinctions, self-return completion and activation
composition remain explicit oracle requirements. Negative source policies must
not be reported as equivalent Go Parse error strings.

The complete source-only cohort, models, original source supplements and full
function inputs are archived in
`full-obligation-audit/go-lifecycle-six-source-and-original-function-audit.json`,
SHA `41161128b82adc3d14f9a517f36fb091088a3e14148e5028ddbe8acd2b09eb69`.
This is planning evidence only; no lifecycle production change is authorized by
this ledger entry.

### ER future-approach preservation: source/test-only experiment

The local ER reservation proposal targets actual unused endpoint approaches
occupied by earlier unrelated rails. Independent review admits only its
source-based native authoring stage: four complete shapes, each with a mirrored
direction, retain all marker/stroke/label/endpoint expectations and three-width
restoration/sentinels. The concrete controls hash to
`8348c474392e56d44a1dffc867f3f76fdde59d069d204ed74a924a81e1f71eae`.
The complete first-red log hashes to
`dbd7d7c776383e940c63713407e370e50c1b5e1bab8e87cb3cb00647a0c1e8e9`:
43 run,41 pass, with the full ER296 and new approach controls genuinely failing.
Six shape/direction positives retain source; the other two draw and execute
their adversarial mutations. No positive is downgraded to source success.

Any subsequent bitmap implementation still requires separate review. Selected
endpoint exceptions may relax only virtual reservation, never actual occupied
marker/text/path cells. Every pair/failure must reset those exceptions; overlap
with another entity's marker may not become ownership. Class/State must have no
bitmap allocation or additional Work. A common coarse free region remains only
a necessary condition, never sufficient drawing proof. Endpoint reservation is
not a promise to solve all eleven ER fallbacks or avoid global face isolation.

### Official box candidate glyph review

Independent review reads the complete generated with-boxes snapshot and compares
it against the separately hand-authored source model. The candidate
`3e7f0869e915c760.txt` hashes to
`a40ea7b5179356e7e695b115bf4de8595c5c15656d9397608305548a75c9e56f`.
All82 previous snapshot files retain their actual hashes. The installed
candidate manifest remains exactlybe08f24a, with only the single diagnostic
change already reviewed.

The complete diagram retains Alice/Bob membership and John's outside two-row
alias; exact LightGrey metadata is in a separate color compartment above the
complete title. All ten messages have their source-derived direction, stroke,
label and markers: six filled, two cross-headed and two unheaded, including the
unheaded checking and Maybe routes. The four-row right-of-John note has its
target-bound leader after message3. Both alt dividers, all three branch messages,
the par empty divider and both ordered parallel messages preserve all18 event
anchors. Box sides are hidden only inside the fully checked fragment captions;
visible side cells and real horizontal crossings survive through the independent
footer, after all participant lifelines end. Color disclosure is not browser
fill parity.

Generation executes4/4 tests and hashes to
`8778eb1aaa506b4d6336cccb440491555fc74a5bb0972c6ed71c092e5a9df86e`.
Generation success alone is not certification. Candidate glyph/source review
passes for normal debug/release execution with snapshot updates disabled. The
independent review record hashes to
`f20e9c0101fb5a5abe0d3d55d51521323443b51af587a4aac62dc0597ef52d9d`
at `full-obligation-audit/official-box-one-snapshot-review.json`. Normal matrix
execution and the separate64/65 empty-box opening regression are not counted
as completed by this candidate record.

### Official box normal dual and empty-box count boundary

The subsequent normal official debug/release logs execute4/4 each, with identical
ordered names. Their hashes are
`24d6a4477267f42966b0033999474011ed824f16bda169c62e4a7452e6ab2190`
and `de38fcdeeca0384643f61107b2383b23394bea2f3adc02a1ee8622bf9ca985a9`.
Independent comparison reads the actual matrix reports, not only their summary:
both hash to27e51114 and match byte-for-byte. All2738 rows have no case or
integrity error. Original2145 remain one native-profile drawing and2144 source
fallbacks; all originals preserve source at40. The separate593 bodies contain82
drawings and511 fallbacks at240,35 drawings and558 fallbacks at40, and the same
82/511 restored. Those82 are46 sequence and36 flow bodies. Raw frontmatter and
browser fill compatibility remain unproved.

The independently read77-test debug/release logs each run77 and pass76, retaining
only the genuine full Go101 failure. Their ordered run/pass sets are identical.
The logs hash to
`e57966c64c3dd8e7176e6c82e2d0287894c9ec46cfaf6b8c6464b8409c86479e`
and `87af158e9b0505c2d36a9fa494d304512e74c8dd940a08713b770a3a88ea795d`.
The new boundary test draws64 empty-box openings followed by a complete two-actor
timeline, checks exact ordinary no-box pixels and strong geometry at both widths,
and restores the same node. A65th opening preserves complete source at all three
width states and retains following text. Thus the opening count is tested
independently of the canvas-width limit. Production remains the reviewed7860745e;
the77-test source hashes to
`8329ed019740a23fd7f41ae4515aa6ef496509876ec85c52d8ea93d53ae21b7b`.
The Go/first-six reports retain137ce8dd and62ee3654 in both presets. This closes
the limited box checkpoint and its count boundary, not lifecycle/title, all101
sources, all original predicates or complete-suite100%.

### Lifecycle oracle contract: bounded authoring admission

Independent review closes three design clarifications in the native lifecycle
contract at SHA
`d5a3a1b53fd38936de632e99b31e4f3a48000cc67b2ae830310ef3c6a7635203`.
Creation/destruction bind a complete message episode, preserving its implicit
activation/deactivation events. A second pending declaration or EOF cannot
silently overwrite/lose a lifecycle statement, and unknown destruction cannot
implicitly create a participant. Creation gaps use already verified Open/Divide
caption areas and Close footers, not the whole future fragment frame.

The private test checker distinguishes scene-tail, explicit-cap and
destruction-truncated activation ends. The truncated kind requires the exact
independently owned destruction marker and verified absent/crossing cells; it
does not fabricate a closing cap or extend actual ghost heavy strokes. Existing
opaque/text ownership cannot excuse a damaged strip. Required source/handmade
controls include shorthand message episodes, nested/intervening creation,
complete self-return and later routes crossing every formerly active depth
column. This admits only native Model/oracle/codec/test-first work. Production
lifecycle changes still require actual independent executable review and a
genuine source-positive first-red baseline.

### ER reservation experiment rejected

Independent comparison confirms the f05fb reservation experiment has zero new
matches and regresses exactly `merman/5b56a64ee740fc22146d`:239 becomes238 plain
matches, with12 source fallbacks and the same46 pending profiles. The full
43-test log hashes to
`88ed7da8105ec91401dab36c62a72c98975cf0b01af36565d7a61ba9cc4aa791`,
with41 passes and the same complete ER296/new-approach failures. The original
296-ID set is unchanged. The experiment is rejected; restoring73a12c is required
rather than stacking matching/backtracking around a result with no demonstrated
benefit. Its source/tests/logs remain historical evidence, not current success.

A separate header-only per-face capacity proposal is admitted for source/test
authoring only. Because ER cardinalities have horizontal normals, a height h
has h−2 rows on each face; degree2 with height3 cannot select two same-face
ports. A degree-derived height experiment may test that concrete capacity,
without claiming it solves global face isolation or all eleven fallbacks.
Typed-table/Class/State dimensions remain unchanged, the existing43 source
controls are reused, and the previously regressed full5b56 record is mandatory.
The exact dimension candidate still needs independent implementation review.

### Box, horizontal orientation, positive baseline and ER real PTY

Independent review actually reads all eight images in
`/Users/bytedance/.codex/visualizations/2026/10/19/01a15315-ef3b-7953-8e46-ca4aba152240/mermaid-box-orientation-positive-baseline-er-gap/`.
The verified executable copy hashes to
`c492ec662561e64a5f69e2f841abb3fd518615fb776ced9702a6ead35af53b1d`,
with sequence7860745e, charts10c0842f and relations73a12c source freeze. Every
selected source is byte-identical to its frozen vendor input. Actual request,
response Markdown and decoded SSE payloads match all four cases; every recorded
screenshot/file/PTY hash matches its current bytes.

The box images show complete two-group membership, separate exact color/title
compartments, a true cross-group route and independent footers. The configured
horizontal chart shows all twelve months and4000..11000 calibration. Both
160→40→160 pairs restore byte-identical complete PNGs; the40-column states show
ordinary source with following sentinels. The positive10..150 chart retains
category4's zero-length bar slot at actual value10, the line and owner legend.
The ER image preserves the complete long relationship label and both zero-many
endpoint cardinalities.

All four responses use child PID40142 in `/private/tmp`. Recomputing actual model
event timestamps against the four kernel resize records yields response counts
1,1,2,2, proving resizing does not request another model response. The child and
server39716 are independently absent from `ps`. Final raw capture1899212 bytes
matches `dd2d2193443137c43e0e31aca6c5e37bf468d297f5156c5cf32beff69afc4464`;
the1899144-byte capture prefix and68-byte cleanup tail also match their separate
hashes. Browser-tab closure is reported by the executor, not separately recorded
in cleanup JSON. The verification hashes to
`3afba7b5ba24645c06e2886b5f8bbf6330ac4ced9e7d2c0123e7fa51b22421e6`.
This closes only the named frozen-stage real OS PTY evidence with deterministic
local SSE. It is not hosted inference, later lifecycle/ER implementation proof,
or complete-suite100%.

### Complete lifecycle union24 hand-transcribed source facts

The independently authored scratch facts are
`full-obligation-audit/go-lifecycle-union24-source-facts.json`, SHA
`9b29e9bb2c392e3c748cbfcd455425a8bb3cd0f2493580fe576d30bbc5f66b35`.
All14 positive inputs have complete source-transcribed native Models, retaining
45 actor applications,49 messages, three notes, three fragments,87 normalized
ordered events, one participant box,12 creates and11 destroys. Actor roles,
aliases, CJK text, all operator/stroke facts, intervening events and automatic
activation/deactivation episodes are explicit. Six frozen sources bind their
original immutable model hashes and existing role/display source supplements.
The eight additional original positives are not inferred from production.

All ten negatives retain complete input bytes, original error case names and
substrings, complete Go table-row byte witnesses/hashes and the actual original
error condition. These are native whole-source policies, not Go Parse or
error-string equivalence. The full nine functions and21 source applications
remain mapped; the three exact duplicate positive inputs retain both frozen
and function provenance, yielding24 distinct inputs rather than27. The static
maintenance writer only serializes explicit hand tables and checks consistency
against frozen source facts; it invokes no renderer or parser. No repository
fixture, production code or native test is changed by this fact-authoring stage.

### Initial lifecycle checker review remains partial

The first five handmade/typed-model additions are an incomplete test-only
workstream, not lifecycle implementation approval. Static review identifies one
component-domain defect: LifeInterval currently permits a mid-body end without
a destruction marker, and negative death values other than the absent sentinel.
The component must require either an exact tail-reaching interval with no death,
or a legal death row exactly one after its lifetime end. A malformed lifetime
must not make missing termination ink count as intended blank cells. The full
scene currently constructs only bound destruction intervals, but the isolated
component domain still needs its own fail-closed mutation before that helper
gate closes. Complete self-return, note/fragment/box/role composition, codec and
the24-source feature gates remain additional pending work.

### ER per-face capacity exact candidate admission

The mandatory5b56 typed-prefix regression genuinely passes in the complete44-test
baseline. The full log hashes to
`a9e5d951798145e54e7d2230ba6ca43a5f481aaaf24ba701f6dff2f418040b32`:
44 run,42 pass, with only the unchanged complete ER296 and approach controls
failing. Its three actual reports remain byte-identical to the239-match stage
at2fd493f7,37c5cbae andae97e08f. This proves restoration from the rejected mask
and preserves the independent strong prefix/alias/field regression.

The exact scratch candidate26963fdb changes only the header-only ER height from
combined-face capacity to `max(current_height, incident_degree + 2)`. Typed
tables, Class/State, width, gap, edge order, routing and Work limits remain
unchanged. The one-line candidate is admitted for bounded execution after this
baseline; it is not yet a successful implementation. All239 current matches,
the complete5b56 record and57 non-ER records remain mandatory. A zero-benefit or
regressing result must be rejected rather than combined with the old bitmap.

### Lifecycle component-domain correction and ER capacity execution review

The lifecycle component now rejects death values below its absent sentinel,
requires an interval without destruction to reach the final scene row, and
requires an explicit destruction row to equal the interval end plus one. The
new malformed-domain mutations are present in the native test source. This
closes the static finding above; executable evidence for the revised helper
and the still-incomplete lifecycle composition gate remains pending.

The exact ER production candidate26963fdb and test a7a65205 were independently
checked against the full actual debug log
`0aed36815517d9b752ef0fd85e496febb9725a50340ea1e88d712067a45d470b`:
44 run,42 pass, with the complete ER296 and approach tests still genuinely red.
The typed report hashes to
`f97b69544a8bdd3a8c5718c7527bd31c3f7393b37e0298026b246ca5157eecc2`.
All239 prior typed matches remain; exactly five sources become matches, giving
244 matches, six active source fallbacks and46 unresolved profiles. There are
no geometry or oracle errors, and all296 records restore identically. The full
first62 report and complete5b56 record remain byte-identical to the baseline.

The55 changed ER records have92 header-only capacity witnesses. Each witness
was independently recomputed from the locked source facts: no attribute rows,
degree greater than one, and the stated old/new minimum-height algebra. Fifty
previous matches change layout while preserving their strong semantic verdict;
the other five are the genuine gains. The proportional one-line height change
passes limited static/debug implementation review. Release verification, the
remaining source/profile obligations and original function predicates are not
closed by this result.

The exact same ER source/test now also have complete release evidence:
`2f593ed8c34dde2be67d1d5bf24acfefe9a951fe186aebb2b6ac6fb0b4f8d838`.
The full ordered44 run names and42 pass names match debug, with the same two
genuine failures. All three frozen reports are byte-identical across presets.
This closes dual-preset verification for the limited per-face height correction,
not the complete ER cohort or its original function predicates.

### Lifecycle handmade88 independent review

The actual full lifecycle test-only log hashes to
`ce032607a8b108ccc42d8288e1aee39797212f2eb318694bfd64fb88d84abbcc`:
88 run,87 pass, with only the unchanged complete Go101 baseline failing. All77
old run names and76 old passing names remain present. The11 lifecycle tests
pass, including the previously false-accepted unwitnessed-death mutations.
The first-six report remains62ee3654 and Go101 report137ce8dd, retaining the
94-match/seven-source state without production changes.

Static review of the next twelfth handwritten scene covers pending creation
after complete intervening note and nested fragment captions, then both closed
fragment footers. No new material finding is identified. This is native oracle
authoring progress only: that twelfth scene still needs actual execution, and
the complete union24 source-first gate, codec and original conditional ports
remain pending. No lifecycle production admission is granted here.

### Lifecycle union24 actual source-first baseline

The twelfth lifecycle scene subsequently passed in the actual89-test run. The
two complete source-first tests then ran with test source171b1218 and unchanged
production7860745e. The promoted fixture is byte-identical to the independently
authored9b29e9bb source facts. Its native reader retains roles, complete labels,
arrow/stroke/central/number facts, notes, fragments, boxes and all ordered
Create/Destroy/message/activation episodes. The inventory binds all24 distinct
inputs,14 positives, ten negatives, six original frozen models, nine complete
functions and21 function-input applications. Original API/error strings and
ASCII conditions remain explicitly pending.

The full actual91-test log hashes to
`71fbf380c712f582d93b1df04982416ff2a8837280e1218266834173d0acd41f`:
91 run,89 pass, with exactly the complete Go101 and new lifecycle source
geometry gates genuinely red. All12 handmade lifecycle checks pass. The report
`4def0938d2601058a824d03928d45b10b5bae1299562cce9a42767a8263eb0d2`
records all14 positive sources as fallback, zero candidate drawings and zero
geometry matches; all ten native negative-source policies match. Each case is
executed and recorded independently at240,40 and restored240 with sentinels.
All old77 run names and76 passing names remain present, and reports137ce8dd
and62ee3654 remain unchanged. This closes test-first/oracle authoring review,
not lifecycle production implementation or original-suite graduation.

### Independent redundant-preflight experiment admission

The ER-only checkpoint26963 is the baseline for scratch024fa9ff. That candidate
removes the local coarse connected-component calculation and its necessary-only
pair/label filters, retaining actual endpoint A*, both reserved label-side A*
proofs, full label clearance, reservation cleanup, source grammar and existing
Work limits. It introduces no cache, replacement service or combined self-loop
change. Static review admits only a bounded independent experiment. Unreachable
candidates can now consume more actual search work, so every prior244 match,
the complete first62/mandatory5b56 records,57 non-ER records, all296 IDs and
three-width restoration remain mandatory. No gain or any old-match regression
requires restoration to exact26963. This is not implementation graduation.

### Charts73 candidate unresolved single-cell path-arm review

Scratch charts3b9c21be adds an explicit shared axis/data profile with exact
source-derived axis ports and owner masks. Static review identifies a potential
false acceptance in its reused connected-path helper: changing the non-axis
vertical data cell at(8,15) from a straight stroke to a same-family three-arm
junction may leave the route connected and the whole cell claimed. Axis closure
does not check that non-axis cell, and a glyph with an unused arm adds no new
unclaimed cell. The existing two-cell branch mutation cannot prove rejection of
this case. A single-cell negative must run first, followed by actual selected
route/owner arm closure if it reproduces. Final Charts73 oracle admission is
withheld pending that evidence; no production change is authorized by this
review.

### Lifecycle codec92 static checkpoint

The added complete six-model codec test first ran genuinely red, with log SHA
`0421e609f6fbb8ef2f6392cf30e232b7ef4328bfcb75c9811bce78b194285180`:
one run, zero pass. All six source-reviewed lifetime models exposed retained
pending flags and missing normalized events. The current test ca4bc2e9 maps
Create/Destroy identities and checks agreement among source declaration facts,
original created-actor indices/raw labels and normalized create events. Its
new comparison covers every actor/role/alias, message field, note, fragment,
box and ordered event/part against the independent9b29 source models. Mixed
box mapping now requires the actual create/destroy events rather than a pending
flag. Static review passes this local test-only mapping; complete92 execution
and lifecycle production review remain pending. Original API/ASCII conditions
are not certified by the codec.

### Codec92 actual result and limited lifecycle production design admission

Actual codec execution log
`a4ffc41d5d9d52dd433cc7bc215b8011538a194e1c408d93720e0f533d9a4cbb`
contains92 runs,90 passes and the same two true-red source geometry gates. The
complete six-model comparison and all12 lifecycle handmades pass. Go101 report
1486c79e differs from137ce8dd only in the diagnostic errors of the six still
unsupported lifetime inputs: their events are mapped rather than declared
pending. Every old94 successful record, all classifications and all pass flags
remain unchanged; first-six62ee3654 and union24 report4def0938 also remain exact.

The private lifecycle production draft50501796 is admitted for bounded local
implementation after this evidence. It binds pending declarations to complete
message/shorthand episodes, preplans real lifeline intervals and separately
owns death markers, and uses Cap/Tail/Destruction strip ends without column
erasure or shared Canvas changes. Its note restrictions are native admission
policies, not claims that every unborn/dead-target note is invalid Mermaid.
Production acceptance still requires all14 source positives to draw correctly,
all ten source policies, preservation of old94 successes and first-six pixels,
dual builds/tests, source-backed official deltas and the real PTY workflow.

### Redundant-preflight experiment rejected

Actual024fa experiment log
`ed85668f27449788d0fd23c62c98a81aecd2b4d2934b17d7f21093886c5e786c`
contains44 runs and41 passes. ER gains one dense-cycle drawing without losing
its244 matches, but two previously strong State drawings become source fallback:
termaid/d06592ea067167994d6d and3603e1d900fce50e0b85. The latter also loses its
reviewed original-snapshot native geometry obligation. Independent full-record
comparison confirms both regressions. The experiment is rejected; its ER gain
is not accepted progress. Restore exact26963 and prove baseline report recovery
before another independent optimization. No combined self-loop/cache fix is
approved by this rejected checkpoint.

### Shared-axis oracle single-arm defect closed in test-only73

The single-cell non-axis spur genuinely reproduced a false acceptance before
repair. The repaired test1e3db0b4 records only successful predecessor-path edges,
checks their exact non-axis arms/family, and permits visible straight endpoint
caps only for global source endpoints. Exact axis unions remain independently
checked. Work covers all five new arrays and path bookkeeping. Internal sample,
zero-length middle-leg, endpoint tee, turn, horizontal spur and fake crossing
mutations all execute in the new handmade cases.

The complete actual73-test log
`cd025df50e258c8486812ba2f5ddcfa186ad1bc9100fd75e971137b5851a1b78`
contains73 runs and69 passes. All seven new handmades pass; the four feature
gates remain genuinely red. All old64 run/62 pass names retain order, and six
old frozen reports are independently byte-identical. New source-union13 report
acd9ab44 retains two eligible unmet drawing obligations and11 other pending
profiles, with zero oracle/geometry/observation errors. This closes the limited
oracle defect and authoring checkpoint, not production or original predicates.

### Lifecycle first implementation and nested-end oracle correction

First production bc3a18ed draws all14 union24 positives with strong geometry,
preserves all ten source policies, and gains exactly the six frozen lifetime
Go inputs while retaining every previous94 successful complete record. The
first-six report stays62ee3654. The P2 extra non-lifecycle death-marker Work
charge is removed in2dbaf7ed without changing the cap.

The subsequently expanded94 run has92 passes: the title gate remains red, and
a new nested activation episode exposes an oracle false rejection. An explicit
child close may occupy its destroyed parent's last live row: the child owns a
real Cap there, the parent has a Destruction end, and the independent death is
the following row. The old enclosing-depth predicate rejected equal end rows
unless end kinds matched. A separate95th handmade genuinely fails first; its
precise correction admits only the depth-minus-one parent Destruction/child Cap
equal-end combination. Independent death/cap anchors, complete cells, LIFO and
the prohibition on equal parent Cap ends remain mandatory. This is a checker
correction, not a production padding workaround or a completed lifecycle gate.

### Complete official lifecycle source cohort

Independent source-only inspection checks all193 frozen sequence body suffixes
against original bytes. Audit da8713a6 retains eight lifecycle bodies/five exact
body texts and42 authored create/destroy declarations;185 have none. Six other
Create keyword occurrences are ordinary message labels. Seven lifecycle bodies
retain concrete unsupported participant-shape or accessibility/custom-numbering/
wrapping witnesses, with the destroyed-target-note native policy separately
documented rather than called invalid Mermaid.

Exactly one complete source model is independently positive: basic actor
creation/destruction, five identities including distinct Ola aliases/roles,
ten messages,16 ordered events and three creates/destroys each. Facts
`2922a39d7d10cb61735f8584927f709e68dfa89540b91c4e7c36c532cace96a5`
may feed a complete native source-first gate. No original/raw or diagnostic
declaration, source bytes or snapshot is changed by this source audit; production
output did not select the cohort or generate its expected model.

### Official lifecycle source transcription correction

The initial2922a39d fact incorrectly encoded all ten unnumbered messages as
`number=-1`. The native Model contract uses zero for absent autonumber and
explicitly rejects negative numbers. Re-reading the complete immutable body
confirms no autonumber directive; this is a source-model sentinel mistake, not
a renderer defect. The96 first execution retains that exact failure in log
dee84477. No production fix or pixel-derived expectation is justified by it.

Corrected current facts are
`5e5c7dd947af547fcf03ee3878cc9a8f9264ddef40b629fdd220eb60d0fb545f`.
The original2922 fact and da8713 audit are preserved verbatim in dedicated
before-number-sentinel-correction files. Recursive comparison proves the only
changes are ten message number values from minus one to zero; roles, labels,
operators,16 events, source bytes and witnesses are unchanged. Current all193
audit hashes to c7a6a15a; source correction proof6ba04d60 locks raw caa0d994
and body93d419bb. A strong native rerun remains required before promotion.

### ER-only preflight implementation debug checkpoint

Independent e9115210 execution log
`a4b0c89d0ce373a6bbeb8256b605318c47ab6614ab4d0c727a9e360a15e9a855`
has44 runs,42 passes and only the unchanged full296/approach failures. Its typed
report8b712449 contains245 matches, five active source fallbacks and46 pending
profiles, with no geometry/oracle errors and all296 restored. Complete record
comparison proves only dense-cycle33da38 changes; the other295 records are
identical to26963, including mandatory5b56. The entire first62 report remains
37c5cbae byte-for-byte. This ER-specific local optimization passes limited
static/debug implementation review and may proceed to release verification.
It does not revive the rejected whole-unit024fa checkpoint or graduate the
remaining ER/source/original-function obligations.

### Bounded named-band/shared-axis production design admission

The Charts draft758ab19e is admitted for an exact scratch implementation review,
not an immediate production write. Its local source parser must fully validate
raw numeric tails before applying only an authored-band surplus transformation,
and perform that transformation before automatic-domain inference. Understood
ASCII title token concatenation must preserve pinned longest-token/reserved
rules; quoted Unicode and the previous numeric-axis rules remain intact.

Its new source-derived axis mode retains real double-axis ports and single-owner
light/heavy data unions, owner-bound exact values for non-reference quantization,
and honest rejection of unsupported multiowner axis cells. The legacy unkeyed
axis-marker guard can be bypassed only in that replacement mode. No shared
Canvas/API/configuration or budget increase is justified. Every old50 complete
geometry record, the13-source/102-source denominators,11 dependent profiles and
108 original applications remain mandatory. The acknowledged old nonshared
unused-arm oracle debt still prevents complete chart geometry certification.

The additional new-profile near-reference disclosure defect subsequently has
real first-red evidence and a strict local correction. Helper3db2f34f checks a
whole owner/category footer token that round-trips to the source double and
differs from the reference; borrowed caption values cannot qualify. The old
numeric checker and nonshared paths remain untouched. Actual full73 log40901859
has68 passes and five genuine feature failures after same-source policy controls
migrate to their reviewed positive obligations. All seven actual report bytes
remain identical to the preceding first-red baseline. Exact production a246ca87
also reserves the existing footer capacity for this new mode before allocation,
without moving plot coordinates or increasing limits. It is admitted for a
bounded production experiment, not final implementation acceptance.

### Limited dual ER/Charts and lifecycle96 debug checkpoints

ER e911 release log
`fe10f76247fd0d533318682d5bf62a7764b0963f49dfec0052a10f3698143a8a`
actually contains44 runs,42 passes and the same two genuine failures as debug.
Every ordered run/pass name and all three frozen report bytes match debug.
This closes dual verification for the ER-specific optimization only:245 strong
matches, five active source fallbacks and46 pending profiles remain the current
separate obligations, with no complete ER296/original-function graduation.

Charts test-only73 release log
`904a6e447974ae866baa8d46eb0ecc7d38041ce19869a2219afaffcf5c75503a`
has73 runs,69 passes and the same four feature failures as debug. Complete
ordered names and all seven actual report bytes are identical. This closes only
the shared-axis oracle/test-first dual checkpoint; its two eligible positive
sources remain unmet and eleven source-union dependencies stay pending.

Lifecycle production2dbaf7ed with test8f2c1f91 has an independently parsed96/95
debug result, log
`23c1403f2b55272a5e1def3aaaa5ba82cb2d8c18d4888a041fc4a1525bde3dd7`.
The sole failure is Go101's title source. The exact nested-end checker correction
and its mutations pass, as do all three actual shorthand/self/empty-birth source
episodes. Union24 now has14 genuine full-geometry drawings and ten matching
whole-source policies. Every old94 successful record remains unchanged, while
first-six62ee3654 remains exact. No original API/ASCII predicate is certified.

The complete official lifecycle source gate reads exact corrected5e5c7dd9 facts,
checks raw/body bytes and every authored witness, and validates all roles,
operators, labels and16 events at wide/narrow/restored widths with sentinels.
Original raw source still falls back. This admits only a one-diagnostic snapshot
candidate for basic lifecycle body93d419; the other592 bodies, all2145 raw
expectations and all old83 snapshots must remain unchanged. Generated glyph
review and normal-mode dual official tests remain future gates. Release96 also
remains pending at this debug checkpoint.

Lifecycle96 release subsequently closes the limited dual implementation gate:
log `68dfe11029a2270a9f7277fa5e6e61ec266ef464a80b3edebe1725ed99d5bdce`
has96 runs and95 passes. Its complete ordered run/pass names and all three
actual report bytes match debug. The sole title failure and original predicate
obligations remain. Manifest candidatec966a725 is independently checked by full
recursive comparison: only the basic lifecycle diagnostic object changes, with
all other diagnostic/raw objects intact. Its seven Filled arrows, complete
message labels and ordered message adjacency checks are source-derived, and
e7d03e43bb63d064 matches the existing path digest naming contract. Unique snapshot
generation is admitted as a candidate-only next step; preservation of all83 old
snapshot bytes, direct glyph review and normal official dual tests remain.

The generated lifecycle snapshot e7d03e43bb63d064 is directly reviewed against
the complete source model. Its SHA is
`931b62e435508aae3f70e2499c149e8aae20bebcb520518bdb3c03d9b9739c4e`.
Five separate headers preserve the two Ola identities and the single actor
figure. All ten messages, three births and three independent post-message
destruction markers match; future/dead lanes have no ghost vertical junctions,
and Polo/Ola2 remain alive through the final row. All83 previous actual snapshot
hashes match their before ledger; the complete inventory now has84 files.
Normal-mode official dual execution remains required after this glyph review.

### Shared-axis exact-disclosure near-reference review

The exact private Charts candidate32ebd5c6 has no material production finding
in its limited source parsing, band-tail and axis-union diff. Four previous
same-source fallback controls must first migrate to full independent positive
models because the new profile intentionally supports them. A further native
checker challenge is required: a non-reference value10.000000000000002 can lie
within the old numeric tolerance of reference10. The new shared-axis profile
must not accept a reference-only disclosure for that source value. An independent
handmade mutation must prove rejection, with any correction confined to the
new disclosure mode. The production round-trip number formatter is not evidence
that an oracle accepting the wrong value is sound. Final production acceptance
is withheld pending this test-first evidence and complete regression.

The lifecycle official normal-mode dual gate subsequently passes4/4 in each
preset. Complete actual logs hash to
`473b300ece1c34c01d20d2c455b112fd53946e474fcfc52cc6655eab3071d273`
and `f7a618905b7f14290f838b64562bc29588af38a5b4c73f00ef69b11019f00bf3`;
their ordered native run/pass names match. Both actual matrix files are
byte-identical at7f557a6f. All2738 rows have zero errors and integrity errors are
empty. Raw coverage remains one drawing among2145; body diagnostics are83
drawings/510 source, including47 sequence bodies, with35 drawings/558 source at
40 columns and exact recovery. This closes only the reviewed official lifecycle
promotion; the real PTY stage and the complete-suite obligations remain separate.

### Source-complete ER self-face test authoring

Scratch3418a49f passes limited concrete test-authoring review: the new optional
self-face policy defaults off, executes only after complete generic business
edge matching, and checks the selected header-only self paths plus padded label
bounds under the same Work. Old44 native tests retain their default behavior.
Three handwritten scenes retain mirrored faces/ordinary successors, distinct
parallel labels/cards/strokes, and a complete enclosing self loop that must pass
generic semantics before the new layout policy rejects it. The complete source
cohort is independently recomputed as17 inputs/22 self edges from all296, with
15 plain drawing obligations and two authored-profile dependencies. This admits
test installation and genuine first execution, not self-loop production.

The first48 run records an invalid handwritten negative: exchanging cardinality
ends of an unheaded A-to-A self relation is observationally the same relation
read in reverse. It must be a positive, while changing the cardinality multiset
is the real negative. Corrected test5a0d3ea7 preserves the original source facts
and generic matching. Actual48 logfd730215 has45 passes: all three handmades pass,
and only full296, approach and the new17-source feature gates remain red. All
four report bytes match the first48 baseline. The17 partition remains two typed
matches,11 exterior-layout mismatches, two source fallbacks and two profile
dependencies, with no oracle errors. Concrete authoring gate closes; production
still requires its independent local design/patch review.

### Complete Go101 title property and original frontmatter functions

Independent source-only auditde102362 checks all101 sequence inputs: exactly
c3b1faed91d6f5f6 has an authored title, Login flow;100 have none. It preserves the
existing source supplements and original semantic-model hashes. Complete pinned
frontmatter_test.go hashes to98bf6663 and contains two full original functions,
four input applications across sequence/ER/flowchart, six lexical condition
sites and21 runtime condition applications when the final OR is one compound
predicate. Ten contains-value applications remain explicitly mapped as source
facts, not assumed native passes. Three other-family inputs and generic factory/
error conditions remain independent pending obligations.

Minimal whole-source title facts7487baa6 retain two participant identities,
one solid Filled Hi message, its event and the exact Login flow title. The native
projection must require a full independent title row, one blank separating row,
complete diagram geometry, same-node resize recovery and following Markdown.
The original ER themeCSS block and flowchart title sources cannot be silently
stripped or declared supported by this local sequence work. Both source artifacts
are scratch only; no title production or fixture promotion is authorized yet.

The first title design review identifies a routing dependency: the current
Mermaid dispatcher selects the sequence unit only for a first sequenceDiagram
header, while whole title frontmatter begins with a delimiter and otherwise
reaches charts. A bounded private routing peek must select sequence only after
an actual closed block and sequence header, while handing every original byte
to the strict owning parser. It must not admit or discard metadata itself, or
change chart/relations dispatch behavior. This seam needs an explicit local
design and regression record before production.

The title prefix oracle may charge/copy exact owned prefix rows and pass only
remaining Work to the unchanged body checker. Multiline titles preserve their
consecutive decoded rows and one separating blank row. A title-only empty body
needs its own exact blank/sentinel check: the old nonempty body validator rejects
zero actors/fragments, so invoking it unchanged cannot validate that declared
empty state. Neither correction permits unexplained body ink or a budget reset.

Revised title draft19a081c8 explicitly closes both design findings. Its routing
peek is metadata-agnostic and leaves strict admission to the owning sequence
parser, charging the new sequence prefix without changing other raw chart or
no-frontmatter Work paths. Its exact title-only empty branch requires all six
semantic collections empty and scans every remaining cell for only blank rows
and a complete unique sentinel. This admits native authoring and source-first
controls, not title production implementation.

Independent raw-title source audit9a0adc8a inspects every one of193 original
official sequence frontmatter prefixes, locking raw/body bytes, offsets, full
prefixes and metadata witnesses. No input has an authored top-level title:
77 have description only and116 have description plus config. Therefore the
strict one-title-field native profile has zero additional raw admissions in
this frozen cohort. All193 originals must continue preserving complete source;
the existing47 reviewed sequence body diagnostics remain separate. No raw
snapshot or output-derived model is needed to establish this source fact.

### Real lifecycle and ER245 PTY checkpoint

The independent reviewer personally views all seven actual JPEG screenshots in
mermaid-lifecycle-er245. Verification JSON hashes to
`4da346430e4c8d0276674f2d9d0ea8240a86f22b488d8ef6b81c55ab856e9afa`.
Copied releaseae8e10e4 matches the executed binary; its sequence2dbaf7ed and ER
e911 source freeze is distinct from later title/Charts work. Official lifecycle
headers, roles, all messages and independent deaths are visible. Nested source
control preserves real child cap/parent death and subsequent routing through the
dead columns. The ER input preserves four nodes, four source labels and endpoint
cards. All three following sentinels are visible. Both40-column captures show
ordinary source, and the two whole wide/restored JPEG pairs are byte-identical.

Actual image hashes and JPEG frame dimensions1756x1244 are checked directly;
the earlier mistaken PNG draft is not accepted evidence. Exact original suffix,
native source control and Merman input bytes, three request hashes, three SSE
hashes and decoded response Markdown are verified. The same child35001 performs
four actual kernel resizes with recomputed model-response counts1,1,2,2. No new
response accompanies a resize. Final raw1984377 bytes, capture prefix1984309 and
the68-byte cleanup suffix match their separate hashes. Actual ps confirms both
server34605 and child35001 are absent; browser closure is executor/cleanup-record
evidence, not a separate reviewer UI observation. The first duplicate-prompt
attempt remains isolated and excluded. Scratch review1f85fb98 records this
limited deterministic local-SSE result; it is not hosted inference, title
support, later chart implementation or complete-suite100%.

### Shared-axis Charts73 limited implementation dual review

Productiona246ca87 and test3db2f34f execute all73 native tests in each preset:
72 pass and only CompleteMermanXY102Batch remains genuinely red. Independent
review parses the full debugb9f9a874 and release71185d69 logs, verifies identical
ordered RUN/OK names and compares all seven actual report files byte-for-byte.
Against the helper-before-production baseline, only93f0e9dadd2c5c259dd3 and
f8d89363e95de9e9505f change in the complete C1/C2, named13 and positive15 records.
The other100 complete C2 records, including all previous50 drawing pixels, and
the complete old36/23/orientation reports remain identical.

The bounded change now satisfies52 active rectangular geometry obligations,
with zero active source fallback and zero geometry/oracle errors. The other50
profiles and108 original function applications remain pending; complete102
certification remains zero. All15 positive-baseline active inputs and both
eligible named13 inputs satisfy full geometry. The11 other named13 dependencies
remain unmet. This closes only the reviewed private implementation dual gate;
it does not close nonshared unused-arm proof debt, original API conditions,
full Charts102 or the union suite. No shared Canvas or public contract changes.

### Title103 native correction and local producer admission

Independent review finds a physical-cell false acceptance in the new titled
empty-state oracle: concatenated sentinel text could conceal an ASCII character
shifted into its neighboring cell. Actual first-red26b5e469 records1 RUN/0 OK.
The correction checks every ASCII sentinel cell and trailing blank cell, charging
the existing shared Work counter. Repaired debug9726fb1f executes103 tests with
101 passes; only the two genuine title-source feature gates remain red. All six
TitleOracle tests pass. Existing first6 and complete lifecycle24 reports retain
62ee3654 and751cee0f. The initial release103 run also has101 passes, but predates
the physical-cell correction and is not evidence for its release execution.

Concrete title producer review identifies a separate strict-scalar admission gap:
sequence Safe-display decoding intentionally accepts unsafe raw control scalars,
so it cannot alone establish a safe plain YAML title. Revised private candidate
ca20c977 charges and validates the raw value with existing unicode_text, explicitly
rejects BOM/ZWSP, then uses the existing sequence label decoder. Existing Safe
message/alias behavior and shared Unicode code remain unchanged. The paired
factory candidate0a05b170 performs only the reviewed closed-frontmatter routing
peek and passes the original complete source into the strict private parser.
These candidates receive limited implementation admission after the source-first
native failures; final105 controls, full Go101 preservation, normal2738 official
matrix, flow/Markdown, dual presets and actual PTY evidence remain required.
The original other-family/frontmatter API obligations remain separate pending.

### ER self-face bounded production experiment admission

Concrete scratch candidatea5c55302 is independently admitted only as an experiment
against acceptede911. It applies solely to header-only ER self relations: both
ports must have the same horizontal normal, and the existing canvas must have
room for the complete label clearance and markers. A per-pair RAII scalar clip
narrows the existing physical free-cell predicate to that source face's exterior;
it does not exempt actual blocked cells, entity keepout, marker reservations or
scope. Both existing A* routes and complete label-clearance checks use it, and
the clip resets on all exits. Ordinary, typed-table, Class/State and previous
search ordering retain their existing paths. No padding, canvas, bitmap, cache
or budget is added. Actual full48/296 evidence is still required: any old245
loss, nonER/typed-CJK/first62/mandatory5b56 regression or zero gain rejects the
experiment back to e911. Static admission does not certify the17-source cohort.

The self-face trial now has actual limited debug evidence: a5c55302/test5a0d3ea7
executes48 tests,46 pass, and only full296 and fullself17 remain genuinely red
(log7a7ab2b8). Independent comparison with the paired48/e911 baseline finds
exactly12 changed complete records, each with an independently locked header-only
self relation. Eleven previous drawings now satisfy the exterior-face geometry;
091608e5e0f5b544acdb additionally changes from source to strong geometry. All old245
matches survive. The296 partition is246 typed matches, four active source
fallbacks and46 source-profile dependencies; all296 restore and no geometry or
oracle errors occur. The self17 partition is14 full matches, one source fallback
and two profile dependencies. The entire first62 report remains37c5cbae, and all
other284 complete typed records remain identical, including typed-CJK and the
mandatory5b56 source. This closes only the limited debug implementation review;
release and remaining source/original-condition obligations are still separate.

### Title105 complete native cohort, bounded dual producer review

Final private title producerca20c977, local dispatcher0a05b170 and testfc9bcbc8
execute all105 tests in debug and release with105 passes. Independent review
parses complete logs674503d3/91c0b079, verifies identical ordered RUN/OK sets,
and rehashes every frozen source, test, binary, log and report in the ledgers.
The three sequence reports are byte-identical across presets: first6 remains
62ee3654, lifecycle24 remains751cee0f, and Go101 becomesbe21f430. Go101 now has
100 genuine drawings plus one valid-empty state and101 native semantic passes.
Only the complete c3b1faed91d6f5f6 record changes; the other100 complete records
remain identical. Each original_assertion_port remains independently pending.

Both presets also pass complete Flow23, Markdown43 and Official4 native tests.
Actual official report bytes remain7f557a6f across both builds, so all2738 original
and body observations are unchanged. The original105/102 source-first failure,
unsafe metadata controls, physical-sentinel first-red and corrected oracle are
retained as distinct evidence. This closes the local title producer dual gate,
not original frontmatter API/ASCII assertions or complete union coverage. Real
PTY proof for this new binary remains a separate required checkpoint. The extra
CRLF titled-empty source currently has a240-column production capture; expanding
that one control to same-node240/40/240 is a nonblocking coverage follow-up and
must not be attributed to the frozen105 runs. No shared Canvas, Unicode helper,
settings schema or established public interface is changed.

The ER self-face release confirmation independently parses48 RUN/46 OK in log
ebbd607d. Its complete ordered RUN/OK names and all four actual report files are
identical to debug. This closes the bounded a5c55302 implementation dual gate,
while the two full296/self17 native failures and all original-condition debt
remain. No extra self-loop heuristic is included in that checkpoint.

### Real title/shared-axis/ER246 PTY checkpoint

Independent review personally views all seven actual JPEG screenshots in
mermaid-title-shared-axis-er246. Verificationec481035 and copied releaseb753d17f
lock titleca20c977, factory0a05b170, Charts3fe2a587 and ERa5c55302. Charts3fe2
is independently diffed against approveda246: only the Legacy/nonshared comment
changes. All frozen source/test and file/image hashes match actual bytes.

The exact Go c3b title source displays Login flow, both participant headers,
the complete Hi route and following Markdown at200 and40 columns. Both wide
captures are identical entire JPEG bytes. The complete93f0 horizontal XY scene
shows three category slots, four series, three composite keys, non-reference
axis value11 disclosed precisely, and its following sentinel. At40 it preserves
ordinary source, and the restored200 whole JPEG is identical. ER091608 visibly
retains the same-face self relation, ordinary successor, endpoint cardinalities
and following sentinel. PageDown is recorded as a real input key; the accepted
seventh capture includes the page header and complete ER scene.

Actual JPEG SOF dimensions are1756x1244. Three source files match the frozen
vendor inputs exactly; all fenced bytes, including the XY final LF, survive
in response Markdown. Request hashes, SSE hashes and decoded SSE text are
verified. Timestamp recomputation confirms response counts1,1,2,2 during four
kernel resizes of the same child87350. Final raw1159787 bytes, its1159719-byte
prefix and68-byte cleanup suffix match their separate hashes. Actual process
checks confirm server87227 and child87350 are gone. Tab closure remains
executor/cleanup-record evidence. Scratch title-shared-axis-er246-pty-review.json
records the limited approval. This proves the copied binary's real PTY path
with deterministic local SSE; it is not hosted inference, installation, original
API/ASCII assertion graduation or complete frozen-suite coverage.

### Complete sequence683 source-property handoff

Independent source-only inventory584651fc reads all five frozen manifests and
reconciles the terminal-record ledger:683 records,667 distinct input bytes
(Beautiful6,Termaid97,Go101,Merman447,mmdflux32). It preserves all input/case/model
variant/snapshot bindings and full statement UTF-8 witnesses. No native output,
foreign renderer or foreign parser execution is used to construct expectations.

The complete core source-property cohort has402 records and386 distinct inputs:
Beautiful6,Termaid50,Go60,Merman268,mmdflux18. It includes all source variants
without an additional fragment/activation/lifecycle/box event-region property;
metadata, quoted/wrapped/rich labels, appearance roles and extended arrows stay
members when their facts need more work. The first worksheet has249 provisional
complete source Models plus153 explicit missing-fact members. Another281 complete
family records need additional event/region source facts; many are already
expressible by the native oracle, so this pending partition is not a declaration
of native unsupported behavior.

Candidatefactsa03cd482 contain527 actor applications,543 messages,79 notes and622
ordered events, covering all ten ordinary arrow/stroke pairs,29 person figures,
11 source and11 target central decorations, default numbering in11 records and
all three note placements. These249 are247 nonempty source obligations and two
valid-empty cases.239 common actor/message projections agree with upstream;
ten explicit differences include preserved raw break spelling, mmdflux numbered
text, the known Go empty model and a custom-parser test. Common projection
agreement is not role/note/order/original-assertion certification. Every candidate
remains provisional pending independent field/source review by corpus_review;
the source author does not approve their own promotion.

The original-obligation link artifactecc6d614 retains780 imported assertion refs,
181 unique imported units and923 snapshot references. These are not a complete
predicate/branch denominator. It additionally locks the full original Go named-
activate function and exact8cf775e2 input outside the683 corpus, preserving that
separate source/API debt without inflating the frozen family denominator.
Contractf04d7294 and sequence683-core-source-handoff-hashes.json define native
schema, normalization differences, valid-empty/resource separation and the next
complete402 feature gate: pending facts and positive fallback remain genuine
unmet obligations. Zero new native or original-condition passes are claimed by
this maintenance audit. No compiled source, native test or frozen fixture changed.

### Core402 native admission design review

The independent oracle reviewer approves only the root-authored test integration
contract SEQUENCE_CORE402_NATIVE_ADMISSION_DRAFT.md. This is not approval of the
reviewer's own provisional source facts: corpus_review owns that independent
field/source review and promotion remains blocked on it. The design reuses the
existing Model reader and whole-scene checker, preserving all105 old tests and
three report bytes. A local exact-empty helper is proportionate because the
nonempty checker intentionally rejects no-title empty Models.

Before any access/allocation, the empty helper must validate width/height and
storage/product bounds. It accepts only an independently source-declared valid
empty state with a complete available Model whose title and all six collections
are empty. Missing facts must not default to that Model. One shared Work budget
covers every physical cell, one complete column0 ASCII sentinel with exact
one-character cells and trailing spaces, and all cells after it. Shifted/merged/
missing/duplicate sentinel cells, foreign ink, malformed storage, forged nonempty
models and exhausted work need actual native negative controls before source use.

All402 source-property records retain full240/40/240 restoration and sentinels;
153 pending facts remain genuine feature failures. The281 additional-event/region
records remain full-family observations with unmet facts, not semantic passes.
No production expansion, shared interface, budget or original-predicate closure
is approved by this test-only design review.

### Core source facts independent review and exact-empty106 gate

The source author corrects two wording findings without changing data. Previous
artifacts remain byte-for-byte under sequence-core-before-wording-p2-review.
Current inventory26dcb4ab and facts997679a8 differ only in153 pending-member
interpretations, explicitly retaining their within-core obligations. Recursive
proof12959680 establishes all249 Models,402 identities,683/667 counts, witnesses
and source bindings unchanged. Contractdc672be1 identifies corpus_oracle as fact
author and corpus_review as independent field/source reviewer.

Corpus_review independently reconstructs every249 full Model against all source
statements, verifies all683 manifest/source/model/snapshot bindings and the181
original units/102 function slices, and formally approves limited source-fact
promotion and native test authoring. Their durable review artifact hashes to
`a3b00752e74dd57882b2879d41e354f79b7c3d747f8d321d2a62ab338e88fd74`.
This is source approval, not249 observed drawings or original-assertion passes;
153 null models and281 additional-event records remain pending.

Separately, root-authored test255e6985 adds one exact-empty oracle handmade.
Independent review parses complete debuga20b041d and releasea65d3011 logs:
both106 RUN/106 OK, identical ordered names and the old105 ordered prefix
unchanged. All source/binary/log/report hashes match; three old reports remain
byte-identical62ee3654/be21f430/751cee0f. Actual mutations cover malformed physical
sentinel cells, shifted/missing/duplicate rows, trailing ink, all seven forged
nonempty Model fields, storage/domain guards and exact319/320 Work boundary.
The old nonempty validate still rejects an empty Model. This closes only the
new empty helper's test-only executable dual gate.

The first complete-core batch integration requires honest exception labels:
render/capture exceptions cannot all be called oracle exceptions. Its report
must bind the source-facts SHA and explicit per-record model/classification
status. These are local reporting corrections before native first-run evidence;
no producer implementation or expectation changes are requested.

The concrete batch reporting correction is independently reviewed in inc37535186
and actual testc116520f. The complete old106 source is an unchanged byte prefix;
only two TEST definitions are appended, and the actual file contains the exact
reviewed inc. Imported source facts retain approved997679a8 bytes. Exceptions
retain their original error and render/capture/observation/model/validation phase;
the combined counter is honestly named execution_or_native_geometry_errors.
Report source-facts SHA, per-record source classification and model availability
are explicit. Source-valid empty states must remain truly empty at narrow width,
not ordinary-source fallback. All683 records are counted before capture and
retained after exceptions. This closes the local report finding and admits the
first test-only complete-family/core run; actual results are not yet claimed and
no production implementation gate is granted.

### Core first-run P1: source-fact note index convention

The first complete108 run is retained unchanged:107 pass and only the full
core gate fails (log1588bb56). It executes all683 records; the249 models yield
193 drawing matches, two valid-empty matches and54 native geometry errors,
with no source-positive fallback or observation errors. The54 error identities
are exactly the54 models containing notes. The report/source facts remain
997679a8/9ef9c65a in the frozen first-run directory.

Diagnosis is a source-fact schema error, not a drawing fix. The fact author
incorrectly transcribed `notes.after` as the preceding-message count and wrote
that wrong definition in the contract. Existing native Note.after is the
preceding message's zero-based index, with -1 before the first message; the
unchanged validator uses events[after] and label_tops[after+1]. The prior source
review checked chronology against the same wrong declared schema and did not
catch its mismatch with this existing native contract. That original approval
and first failure remain preserved; neither production nor the validator is
changed to accommodate the erroneous facts.

Corrected scratch facts748d2b66 and inventoryc3903e60 change only79 note indices
by minus one plus54 derived Model hashes in each artifact. Contractb29b1872
corrects the definition and Beautiful afterIndex equivalence. Recursive proof
7f3b1f05 verifies all other source, model, actor, message, label, arrow, order,
classification and denominator fields unchanged. Each corrected index is
recomputed from its exact original note source witness and the complete event
stream prefix, independently of renderer output. Original997/26dc artifacts and
original independent-review bytes remain under sequence-core-before-note-index-
correction. Corpus_review must independently verify this correction before
root updates the native fixture; corrected native results are not yet claimed.

Corpus_review independently approves the748d2b66/c3903e60 correction after
rechecking all79 source/order prefixes against the existing native index contract;
their correction review hashes to6c5ffcf6. Root separately adds a source-order/
Note.after invariant to the new integrity test. Independent verification of its
preserved first-red log37812fd6 confirms1 RUN/0 OK and exactly79 equality failures
against original997679a8 facts; every frozen SHA matches. This makes the schema
seam executable before corrected data is used. Only source facts/derived hashes
are corrected; producer and existing geometry validator remain unchanged. The
subsequent corrected full108 result remains pending at this checkpoint.

### Corrected core108 actual dual: source-data seam closed

Independent review of the complete corrected debug/release logs confirms108 RUN,
107 OK in each preset; only the complete core gate remains genuinely red because
153 member Models are still null. All249 modeled members now pass:247 actual
nonempty drawing geometry and two exact valid-empty states, with zero source-
positive fallback, native geometry/execution errors or observation errors. This
is a source-fact index correction; production and the existing validator did not
change. The four actual reports are byte-identical across presets, including
core report7b4cf7b5 and unchanged old62ee3654/be21f430/751cee0f reports.

Compared with the preserved first108 failure, exactly54 note-containing record
Model hashes/outcomes/errors change; the other629 complete record dictionaries
remain identical. All683 wide/narrow/restored frame hashes remain identical.
The old106 ordered RUN/OK subsequences remain intact (GTest groups suite members,
so they are filtered subsequences rather than the first106 execution entries).
This closes the limited source-data/test checkpoint;153 core facts and281
additional-event facts, and original predicates/profiles, remain pending.

### Outside683 original keyword obligation: actual unmet109 checkpoint

The separately retained original Go named-activate source8cf775e2 is now exercised
through the public renderer in all three widths with complete following-content
sentinels. Independent full-log/hash verification confirms109 RUN/107 OK: the
existing core-pending gate and this source-positive geometry requirement are the
two genuine failures. All four old reports remain byte-identical;683/667 corpus
membership is unchanged. Geometry mutation controls honestly remain unexecuted
until the positive draws. This is a native terminal-equivalent obligation, not
completion of the original Go Parse API or its full function predicates. No
sequence producer modification is admitted by this checkpoint.

### Next153 source-facts AUTHOR handoff:20 complete candidates,133 pending

A new additive scratch worksheet bb051376 and source contract ea8db71f declare
20 complete source candidates without editing approved748d2b66 facts, fixtures,
compiled tests or production. Source selection covers six integer autonumber,
five inline titles, existing Go title/Safe facts, ordinary hash comments, plain
extended-br whitespace and all five entity-only inputs without unknown coupled
properties. Counts are46 actors,56 messages,16 notes and72 ordered events.
Every note index is checked against its complete preceding Message count minus
one. The full153 membership remains20 candidate Models plus133 pending;281
additional-event records remain pending. Candidate native execution/pass count
is zero. Corpus_oracle is the AUTHOR; corpus_review independently reviews these
facts before any promotion. The original11 unit/27 application/22 snapshot subset
is retained without claiming a complete original-predicate denominator.

One foreign-profile disagreement is explicit: mmdflux2f85 retains final number14
in its immutable model, while pinned Mermaid12.2's hidden-message counter advance
projects16. The candidate preserves both and leaves the original14 obligation
unresolved; it does not erase the foreign fact or label it passed. Character
reference decoding and escaped-br nonrecursive behavior are explicit source
projection proposals requiring peer review, not observed Loom expectations.

### ER415 limited debug implementation checkpoint

Independent review approves only the local header-only parallel-self source-
measured exterior-margin change415decea. Complete first debug execution is50 RUN,
49 OK; full296 is still genuinely red. The20 complete direction/long-label
variants all draw and execute their geometry mutations, restore and sentinels.
Typed matches rise246 to247; exactlybc08 changes from source to strong geometry,
while the other295 complete typed records remain identical. All old246 matches,
first62 bytes,57 non-ER inputs, typed-CJK cases and mandatory5b56 are preserved;
all296 restore. Three active source fallbacks and46 profile-pending inputs remain.
Proof er415-limited-debug-independent-review.json records the source/test/log
hashes. Release evidence and complete-family/original-predicate graduation are
outside this limited debug verdict. No shared module or established public
contract is changed by this producer patch.

### Remaining133 source-property queue: participant roles36

A separate AUTHOR worksheet120f3c16 and contract4f7200a0 preserve all33 modern
appearance/identifier sources plus three Termaid database-declaration sources.
They contain99 actors,100 messages,eight notes and108 ordered events.31 sources
have role-only properties;five also carry explicit wrapping requirements. All36
retain native_model=null: their typed source worksheet includes effective kind,
metadata/explicit alias precedence and full original statements, while the current
native person boolean cannot represent database/boundary/control/entity/
collections/queue without loss. No shape geometry is invented and no fallback
or native pass is claimed. The other97 core members remain outside this queue.
This is source authoring for independent peer review, not a new executable
Model/policy/production gate; the complete133 pending denominator does not shrink.

### Independent source review: next20 and role-property union correction

Corpus_review independently approves source-only next20 factsbb051376:all20
complete Models and their46/56/16/72 actor/message/note/order applications match
independent source transcription;16 note indices are rechecked;155 full physical
source spans,52 model refs,22 snapshots and11 original unit slices are verified.
This is source/test-authoring qualification, not native geometry execution or
original-predicate completion. The base748d2b66 and all compiled fixtures remain
unchanged at this author checkpoint.

For role36, independent review finds a P2 in the author's complete-property
wording: old first-blocker selection identified36 worksheet records, but the
complete source appearance/declaration-property union contains40. Four coupled
members1900/1ca41/201d/5cf5 were still retained in the other97; no source had been
removed, but the complete-role description was too broad. Original120f3c16 and
4f7200a0 artifacts remain in the before-property-union-p2 archive. Corrected
source worksheet3216848b adds all four complete inputs/spans and explicit
40=36+4/133=36+4+93 accounting. Every one of the36 complete case dictionaries
and99/100/8/108 typed applications is unchanged. Corpus_review independently
retranscribes all36, validates267 spans and independently reconstructs the40
union; they approve only source properties, with all native Models still null.

### Role shape design remains an unapproved author proposal

The sequence-role-shapes design draft1f7f13fe proposes one private Role enum
replacing person, six actual role figures above the existing closed alias frame,
and exact complete figure-slot/blank/center/lifeline/box/global-ink ownership.
It preserves the old Participant/Actor-only geometry and Work path. Source
comparison evidence5a71bfa5 distinguishes Merman/Go/mmdflux box projections,
Termaid's genuine cylinder snapshot/parser-role assertions and Beautiful's
separate SVG person predicate. Termaid cylinder-plus-label-frame equivalence is
an explicit proposed native profile adaptation; foreign exact bytes are retained.
No ASCII role badge, backend, shared Canvas/public contract or configuration
change is proposed. The optional first target is31 role-only complete sources;
five wrap-coupled plus four other appearance-property members stay pending.
The source author cannot approve this design; independent design review and
native handmade/source-first gates must precede any producer implementation.

### Next20 root native integration and actual110 dual checkpoint

Corpus_oracle reviews root's executable join and runtime comparisons only here;
source factsbb051376 retain corpus_review's independent approval. Test4b9b4d36
joins only20 existing base-null members after exact raw source/SHA equality,
forbids replacing the original249 Models, checks the complete20+133 partition,
retains exact-source Model conflict detection and the note/order seam invariant.
Foreign14 versus primary16 remains explicitly an unresolved original-profile
obligation. No producer is changed by this additive facts/test integration.

Independent full debug/release log and artifact verification confirms110 RUN,
108 OK each, with exactly the complete core-pending and outside683 named-activate
source-positive gates genuinely red. All four actual reports are byte-identical
across presets.269 Models yield249 nonempty geometry matches,two valid-empty
matches and18 actual positive source fallbacks;133 core members and281 additional-
event records still lack complete facts. Native geometry/execution and observation
errors are zero. Exactly the20 supplemental record dictionaries change from109;
the other663 complete dictionaries, all original249 Model records, all683 three-
width frame hashes, the old109 ordered RUN/OK subsequences and the three previous
reports remain identical. Proof32fccf0b records the limited dual verdict. This
qualifies the native integration; it is not full402/683 or original-suite success.

### ER415 actual50 dual and cross-family release checkpoint

Limited ER implementation415decea/test6882db74 now has independently verified
complete debug/release50 RUN/49 OK and all five actual report bytes identical.
Only the complete ER296 gate remains red.247 typed geometry matches,three active
source fallbacks and46 profile-pending inputs remain;self17 has all15 plain
matches plus two pending. All20 direction/long-label variants execute complete
geometry mutations, restore and following-content sentinels. The prior debug
comparison's old246 preservation,295 complete unchanged records andfirst62/
mandatory5b56 byte preservation carry through the identical release reports.
This closes the local implementation dual gate, not ER296/original-function
completion. Release cross-family logs independently show flow23/23,Markdown43/43
and official4/4;the complete2738 raw/body official report remains7f557a6f exactly.
All log/binary/source/report hashes were checked; no installation or new PTY
claim follows from these native evidence files.

### Role figure draft findings remain design-only

Independent reviewer corpus_review identifies P1: the original database cap
looked like a rounded separator box rather than a closed ellipse;and P2: the
control head was floating despite being called attached. Both original drafts
remain under sequence-role-shapes-before-cylinder-control-review. Revised
contract3331ecfe/cellsb5419332 use a closed two-row elliptical database cap plus
body/base and an actual ring-junction→elbow→arrow-head attachment. Complete blank,
missing-cap, detached-head/elbow/arm and extra-stroke mutations are required.
Footprint remains at most4×7 and old0/3 bands/caps are unchanged. Independent
re-review is pending;no native authoring or production approval is inferred.

### Role design review closed for enum/native test authoring only

Corpus_review independently closes the revised database-cap and Control-arrow
attachment findings, then identifies a slot-range P2: universal seven-column
checks would inspect outside a minimal far-left old Actor header and add Work.
Revised draftbf609b2f/profile842c3f0b declare actual per-role bounds;old Actor
remains exactly−1..+1,Participant has no figure,and−3..+3 is only the maximum
new-role envelope. Mixed surplus-band blanks stay inside the actor's actual
owned header/lane footprint and are charged;old Actor-only gets no new scan.
A one-character/five-cell far-left old Actor is a mandatory positive regression.
Previous draft/profile bytes are preserved in the before-slot-bound archive.

Their independent review in SEQUENCE_ROLE_NATIVE_DESIGN_INDEPENDENT_REVIEW.md
now grants only limited enum/native test-authoring admission. Single Role truth,
one-time locked-fact-reader projection,old0/3 pixel/Work preservation,the complete
40=31+5+4 property denominator and exact/API/ASCII/browser profile boundaries
remain mandatory. No new role producer, fixture promotion or executed new shape
geometry is approved by this design verdict.

### Real PTY copied-release value labels and ER247 checkpoint

Corpus_oracle personally inspects all five actual1756×1244 JPEGs under
mermaid-value-labels-er247. They show twelve complete month/bar values;the
40-column ordinary-source fallback with following sentinel and genuinely empty
right-side pixels;the restored200-column drawing;two named owners with four
exact category/value readouts and a composite key;and two independent ER self
routes with complete children/optional labels,four endpoint cardinalities and
following sentinel after a real Terminal Next key. Narrow ordinary-code lines
are viewport-clipped,not claimed fully visible on screen. Accessibility old
screen text is not used as pixel evidence.01/03 entire JPEG bytes are identical.

The independently verified copied release isd4bea600,not installed. Three complete
Merman inputs match frozen vendor bytes;all request/SSE hashes and fully decoded
response Markdown preserve those exact inputs and sentinels. Harness inspection
confirms actual pty.fork/execve of the native CLI,raw os.read output and kernel
TIOCSWINSZ/TIOCGWINSZ. Same child777 resizes200→40→200 while response counts stay
1/1;there are exactly three responses total. No inference occurs during resize.
Local deterministic SSE returns source text only and is explicitly not hosted
model inference or an external drawing backend.

Final verification6bca3e8f retains1882998 captured bytes plus a separately recorded
68-byte cleanup tail,exactly1883066 final raw bytes. All42 listed artifact hashes
and nine copied capture-source/test snapshots independently match the pre-capture
source freeze;all five JPEG signatures/dimensions/hashes match. Reviewer probes
os.kill(pid,0) confirm child777/server130 gone. Browser tab closure is supported
by the executor record only;the reviewer does not reopen it. Installed CLI remains
db1f8698. User settings have no before/after hash,so no byte-preservation claim is
made;the harness visibly uses isolated task config/session directories. Independent
review proofe7c24334 grants only this bounded real-PTY checkpoint,not full-suite
completion,new role implementation,installation or real-provider validation.

### Official193 numbering source AUTHOR audit before integer-number expansion

A new read-only complete193 sequence source audit09c3777d preserves every raw/body
SHA,exact suffix offset and full source. Actual numbering-property union44 is
43 command bodies/46 commands plus one original config-only input. Forms are42
bare,two start+step,one off and one start-only.38 command bodies retain in-body
init/config;no directive is stripped to construct a Model. The remaining five
are three complete body candidates,one fractional Model gap and one accessibility/
wrapping/lifecycle/region combination kept pending. No official zero literal,
late first command or keyword-case variant is present;nonofficial handmade
controls must not be counted as extra official coverage.

Primary span evidenceb42d59a3 corrects an important semantic trap:the Jison
start-only production explicitly emits step1,so renderer step||current does not
mean that start-only preserves an old custom step. Bare/off preserve step;
zero start/step operands individually use truthy fallback. Hidden messages still
increment,nonmessage events do not,and config.showSequenceNumbers is ORed with
command visibility. NUM is unsigned with at most two decimal digits and an
explicit space/newline lookahead;this does not justify declaring other vendors'
accepted spellings invalid or silently coercing fractional numbers to integers.
The complex official source's complete numbering trace ends200/201,not200/210.

Four independently transcribed whole-body candidatesc2470d38 retain allten actors,
sixteen messages,two notes,two loop regions and22 ordered events. Three contain
bare commands;the extra config-only suffix correctly has all numbers0 while
its original config numbers1..5 and actorMargin stay separate raw obligations.
No full original Model/raw official pass is claimed. Manifestc966 and all84 old
snapshots are verified unchanged. Source author corpus_oracle submits contract
 a7dfad8e/full source ledger/models to corpus_review for independent source and
schema review;no fact promotion,renderer/backend execution,producer edit or
native pass is authorized by this draft.

### Official193 numbering source review: limited independent approval

Corpus_review independently verifies audit09c3777d and four full body Models
c2470d38,including every193 raw/body identity and suffix offset,2378 physical
spans,the complete44-property/43-command-body/46-command source sets,nine primary
rule spans and390 message-counter applications. All four Models are independently
retranscribed field by field. The start-only step1,truthy-zero,hidden-message
increment and config-OR boundaries are checked against primary source;the
complex sequence ends200/201. Proof is
relations-audit/official-sequence193-numbering-independent-source-review.json.
This closes independent source review only.38 init-coupled bodies,fractional/
marker/metadata obligations and complete raw inputs remain distinct;no native
geometry,producer or manifest promotion is approved by that source verdict.

### Next20 twelve text-syntax sources: AUTHOR contract, awaiting peer review

The complete non-number unmet queue retains all twelve source identities:
five inline titles,five entity-only inputs,one hash-comment input and one extra
BR-whitespace input. Full original source bindings and unchanged bb051376 Models
contain30 actors,28 messages,16 notes and44 ordered events. Physical applications
are five title declarations,56 entity atoms,36 actual BR tags and two comment
lines. Nine distinct original imported functions,17 imported references and12
snapshot applications remain provenance and pending original obligations,not
native passes. No input,Model,vendor asset,manifest,golden or compiled file changes.

Source contract77bb245f and English contract5b742461 lock22 primary UTF-8 spans.
The primary sets frontmatter title before parsing,then each inline title setter
replaces it;valid inline titles may appear among document statements and the
last setter wins. The five source titles are plain and single. Primary title
rendering is one text node,so alias/message BR behavior is not silently inherited
by every title. Protected entity atoms must be recognized before hash comments
or semicolon statement boundaries. Actual BR splitting precedes entity decoding;
escaped literal BR characters cannot recursively become markup. Required atoms
are decimal35/59/9829 and nameslt/gt/infin. Unknown names,HTML numeric replacement,
arbitrary markup and wrapping remain explicit separate source obligations.
Syntax TAB inside an actual BR tag differs from data TAB in an escaped literal;
the latter retains reversible Safe-display. No backend is executed or imported.

Corpus_oracle submits the contract and handoffc3a3de8c to corpus_review for
independent syntax/profile review. This is source-author work,not self-approval
of facts or production. Current family683/667,core402,modeled269 and133+281 fact-
pending denominators remain unchanged. Full100% and complete original predicates
remain unfinished.

### Charts93 unused-arm actual first-red checkpoint

Independent read-only artifact review1d2bdd16 verifies the entire frozen log:
93 RUN,86 OK,seven genuine failures. One is the preexisting full102 pending
feature gate;six new handmade groups expose false acceptance of extra direction
arms/borrowed ports/detours/ownership/atomic marks. The seventh new shared-bound
and Work control passes. No repair or producer approval is implied by this
first-red evidence. Frozen producer remains e28dd6c1,testfbf2bcda,binary55e3f8ea;
log5af82bb5 is preserved in full,including all failures.

All frozen file hashes independently match. All eight actual reports are byte-
identical to the preceding reviewed86 stage,and the old86 ordered RUN/OK sets
are preserved. Therefore this checkpoint adds no drawing/source coverage and
cannot be used as proof that all charts,all original assertions or the complete
suite have passed. The report comparison and complete file checks are in
full-obligation-audit/charts93-unused-arm-first-red-independent-evidence-review.json.

### Numbering116 root native first-red execution review

Independent execution-only review21b8a4f7 checks all twelve frozen file hashes,
including native test1ebddb81,unchanged producerca20c977/factory0a05b170,binary
2602b301,full build log and full test loga22cd8ed. Actual complete116 execution
has111 OK and five genuine failures. The original110 ordered RUN/OK results
remain unchanged. Three new numbering handmade oracle groups pass,covering
owned literal/fractional prefixes,hidden counter state/notes/resets and shortest
significant/scientific spelling. Three public numbering feature tests fail on
the old producer,alongside the existing complete-core pending and outside683
activate-keyword source-positive gates. No118 producer result is implied.

All four actual reports are byte-identical to native110,including all683 full
case dictionaries and every wide/narrow/restored frame hash. Complete records
independently recount249 drawing matches,two valid empties,eighteen positive
source-unmet records,133 core fact gaps and281 additional-event fact gaps,with
zero geometry/execution/observation errors and all683 restored hashes equal to
wide. The private native Message.number becomes a string;both source fact JSONs
remain exactly748d2b66/bb051376. Sourcefacts were separately peer-reviewed;their
author only reviews root-owned execution and frozen comparisons here. Six number
sources remain unmet at this checkpoint,and original API/ASCII/conditional
predicate obligations remain independent. Evidence is
full-obligation-audit/numbering116-independent-execution-review.json. No production,
native test,fixture,golden,shared module or established public contract changes
are made by this review;complete100% remains unfinished.

### Twelve text-syntax sources: independent source/profile authoring approval

Corpus_review independently approves exact contract77bb245f/MD5b742461 after
checking all22 primary file/span hashes,the complete12 vendor sources and all
physical Model bindings,models/snapshots/provenance. All12 full Models equal
unchanged bb051376;30 actors,28 messages,16 notes and44 events remain complete.
The reviewer confirms title initialization/last-wins/context and single-text
presentation,the entity-shield/raw-BR/single-decode/Safe order and the narrow
six-atom/unknown-whole-source profile boundaries. Proof is
relations-audit/sequence-core12-text-syntax-independent-review.json. This is
limited SOURCE/PROFILE plus native test-authoring approval,not producer approval,
new Model content,original-predicate completion or executable-control approval.

### Numbering118 actual first-production debug comparison

Independent execution review682c376f checks every frozen source/test/binary/log/
fact/report SHA in numbering118-first-production-debug-hashes.json. Actual118
has115 OK and three genuine failures. Two are the unchanged complete-core and
outside683 keyword gates. The other is the old numbered-budget test:exact two
formerly unsupported inputs now draw,so their stale whole-source expectations
fail. The complete first-failure log remainsb739a762. Migrating those two source-
backed controls requires separate peer review;this checkpoint is not full-green.

The six independently source-reviewed number Models move from source fallback
to actual full wide/narrow native geometry,with unchanged source Model hashes.
Their vectors are1/5/10/hidden;1/3/10/15/hidden;10/hidden/hidden/25;
1/50/60/hidden;1/2/10/11/hidden;and10/12/hidden/16. Frozen integer0 remains hidden
in the native private string representation. The mmdflux foreign14 versus
primary16 original-predicate disagreement stays explicitly unmet,not overwritten.
Base748d2b66 and supplementbb051376 fact files remain byte-identical.

All683 rows are independently compared. Exactly eight complete dictionaries and
three-width frame sets change:the six number Models plus two unmodeled sources,
merman/0b3d922a18b82ac38939 and merman/a269435c1f0a81939890. These two new drawing
candidates remain additional-event/core facts pending and are never certified or
counted as geometry matches. Other675 full rows are unchanged. Old249 drawing
matches and two valid empties retain their entire dictionaries;old three family
reports remain exact62ee3654/be21f430/751cee0f. All683 restored hashes equal wide.

The core report4ef01103 now records255 drawing matches,two valid empties,twelve
positive source-unmet,133 core fact gaps and281 additional-event fact gaps,with
zero geometry/execution/observation errors. Source/test/binary freeze is
993c9fab/4d48d2df/a60d48a0. This review only certifies the bounded actual debug
comparison,not release,official promotion,realPTY,whole683 completion or original
API/ASCII/conditional predicates. No production,test,fixture or public-contract
edit is made by the reviewer;complete100% remains unfinished.

### Core133 fractional1 and one official raw/body pair: AUTHOR source candidates

A separate explicit number-text source candidate10b49242 preserves the exact
merman/a269435c1f0a81939890 input and complete two-participant,three-message Model.
Its visible tokens are strings10.1/10.11/10.12,with full Filled markers,solid/
dashed/solid shafts,labels,endpoints and Message0/1/2 order. Full source and all
five manifests are checked;only one frozen record has that exact input identity.
All typed-model/reference/options/invocation bindings and the complete original
function0baa7e6f/fullfile9753406d remain linked. Three original contains literals,
one float-artifact prohibition and one render-result expectation are recorded,
not automatically passed. Strict parse/detected-result helper sites and Unicode/
canonical/exact foreign image obligations remain independent.

The source proposal partitions the existing133 pending members into exactly one
candidate and132 untouched pending records,without changing683/667,core402 or
281 additional-event facts. It does not replace any original249 or next20 Model.
Base748 and supplementbb051 keep their old integer facts unchanged. A new explicit
sequence_native_full_model_number_text/v1 decoder must require JSON string tokens,
with empty string meaning hidden;get_int/coercion cannot truncate fractional
expectations. Model2158f35b and contractdfcb4edf are source-only,awaiting independent
review and a later native source gate;observed admission is not certification.

The independent official candidate66e42d2d covers the complete original374-byte
fractional source and its exact200-byte-offset body. RawSHA57af6268,bodyc6e769ce
and Gitblob2a64eca0 are checked. All four source messages are Unheaded,only the
second dashed,with Alice/Bob first-use order and tokens10.1/10.11/10.12/10.13.
No note,title,region,box,activation or lifetime is omitted. The original has only
description:>- frontmatter,not config or title. Primary returns no renderer
metadata for that description;the generic E2E runner records it as a testInfo
annotation. Both complete graph Models are stored as c179c2a5,with full raw
metadata retained separately. Raw native frontmatter admission remains its own
whole-source/unmet profile;body geometry must not be reported as raw compatibility.

Thirteen primary ranges include the full generic official fixture registration,
render/error/conditional screenshot helpers,description metadata handling and
NUM/state/display/rounded-increment rules. The primary counter rounds to two
places after each ordinary message;the expectation does not come from a Loom
capture or arbitrary float printing. Prior193 audit's old integer-representation
gap stays immutable history. The genuine new official diagnostic first failure
is preserved separately;manifestc966 and all84 snapshots remain unchanged.

Corpus_oracle submits both candidates and handofff9b049ef to corpus_review for
independent complete source/schema review. No native or original pass,producer
change,fact promotion,diagnostic declaration or snapshot generation is authorized
by these author artifacts. The reviewer owns no production/tests/fixtures and
performs no build. Complete100% and original API/ASCII/conditional predicates
remain unfinished.

### Two fractional source candidates: independent source/schema approval

Corpus_review independently approves exact Merman10b49242 and official66e42d2d
source/schema candidates. All20 primary full-file/UTF-8 ranges are checked,all
five vendor manifests contain exactly one matching Merman record,and the other
132 core pending members retain their complete dictionaries. Both full Models
are independently reconstructed from complete source:two participants/three
Filled messages and two participants/four Unheaded messages,second message
only dashed,all labels/endpoints/order/central flags and exact fractional tokens.
Official374-byte raw/Gitblob,offset200,174-byte body and description-only metadata
are verified. Raw description-frontmatter admission stays independent/pending.
Proof is relations-audit/sequence-fractional2-independent-source-review.json.
This is limited source/schema approval,not snapshot,overlay-promotion,raw support
or automatic original-predicate completion. The facts' author is not their
independent reviewer.

### Fractional122 root debug: explicit codec and source-bound geometry

Independent runtime/frozen-byte review95fc41aa verifies every listed source/test/
binary/build-log/test-log/fact/report SHA. Actual122 RUN has120 OK and only the
existing full-core pending plus outside683 activate-keyword positive gates red.
All old119 RUN/OK results remain in order. Three new tests pass:strict DisplayText
versus LegacyInteger bidirectional type checks;complete source/fact/schema/core-
partition integrity;and both complete public fractional source geometry gates.
Those gates execute wrong-number,hidden-number,wrong-dashed,wrong-order,missing-
message and extra-message negatives for each source. Raw official description
frontmatter is explicitly checked as complete source fallback,not a raw drawing
pass. Wide/narrow correctness,restoration and complete sentinels remain checked.

The source fact copies are exact10b49242/66e42d2d and old748/bb051;no expected field
is changed to fit the producer. The actual complete683 comparison changes only
merman/a269435c1f0a81939890 from null-fact pending to native geometry match with
Model2158f35b. All683 wide/narrow/restored frame hashes are unchanged,so this is a
newly certified complete source model for an already observed drawing,not a new
rendering change. Other682 complete case dictionaries and all old255 drawing/
two valid-empty matches are unchanged;three old family reports remain exact
62ee3654/be21f430/751cee0f.

The new core report755f5375 records270 Models:256 drawing matches,two valid empty,
twelve positive source-unmet,132 core facts pending and281 additional-event facts
pending,zero native/execution/observation errors. Original mmdflux disagreement
remains unmet. Frozen producer993c9fab/factory0a05,test4f89d323,binarycaff01a5 and
log88a215b7 are preserved. Proof is
full-obligation-audit/fractional122-first-debug-independent-execution-review.json.
Only this debug execution checkpoint is closed;release,official promotion,
realPTY and complete original API/ASCII/conditional assertions are not claimed.
The reviewer edits no actual production/test/fixture/manifest/golden/README/shared
module/public contract and performs no build or commit. Full100% remains unfinished.

### Fractional122 and namedCharts101: bounded actual dual execution

Independent read-only proof12e445b6 verifies all56 frozen files from the four
source/test/binary/full-build-log/full-test-log/report ledgers and all four
comparison-input hashes. Sequence debug and release each execute122 unique tests,
120 OK;Charts each execute101 unique tests,99 OK. Each family's complete ordered
RUN,OK and failure-name lists match across presets. The two sequence failures
are the complete-core pending gate and outside683 activate-keyword positive.
The two chart failures are completeXY102 and the six eligible named-singleton
source obligations. No rerun or truncated tail is used to derive these counts.

All thirteen actual report files are byte-identical across debug/release:four
sequence and nine chart. Source/test identities also match across presets.
Sequence remains production993c9fab/factory0a05b170/test4f89d323,Charts remain
productione28dd6c1/test2e06ede6. This facts/helper stage does not expand source,
canvas,label,event or Work caps;the preexisting resource controls execute with
those same production sources. No subsequent source edit is covered by this
verdict.

The complete683 comparison again changes onlya269's pending Model dictionary to
its full number-text geometry match. Every683 wide/narrow/restored physical
frame hash is unchanged from119,and all other682 full rows remain identical.
270 Models produce256 geometry matches,two valid empty,twelve positive source-
unmet,132 core fact gaps and281 additional-event fact gaps,with zero native or
observation errors. Three old family reports keep their exact hashes. The eight
old chart reports also remain byte-identical to the prior source/producer stage.
Named9 has nine observed source fallbacks,including all six active geometry
obligations;three independent indexed/subcell dependencies and all9/108 original
function applications remain pending. FullXY102 still records70 active oracle
matches and32 profile-pending records,not complete certification.

Independent asset verification walks the exact root inventory7c842c6c:all8370
regular asset files match byte counts/SHA and the exact listed tree. All five
vendor manifests retain2990 records/2916 exact inputs. Every2145 official raw
file matches SHA/length/Gitblob;manifest remainsc966 and all84 old snapshots
match their frozen hashes. The official fractional first-debug full log remains
4 RUN/3 OK with its genuine CompleteCapabilityMatrix failure. Its body diagnostic
still declares source,and raw description-frontmatter still retains its own
whole-source/unmet obligation. Strong native body geometry is not raw admission,
a changed official declaration or an original browser/screenshot pass.

Evidence is full-obligation-audit/fractional122-charts101-dual-independent-execution-review.json. This closes only the named frozen dual execution
checkpoint. The reviewer makes no production,test,fixture,README,manifest,golden,
shared-module or public-contract edit;does not build,install,promote or commit.
Complete100% and original conditional/API/ASCII/browser obligations remain open.


### Text7 / Empty5: independently reviewed final seven-binary checkpoint

Independent reviewer `agent:/root/corpus_review` verified 122 frozen files and
parsed all 14 complete logs from the final debug and release executions. Each
preset executes the same 381 unique test names with 375 passes; ordered RUN,
OK and failure states match exactly. All 22 actual JSON reports are
byte-identical across presets. This is a bounded checkpoint, not completion of
Phase C or the complete upstream assertion port.

| Native binary | Tests | Passed | Remaining failure gates |
|---|---:|---:|---|
| Sequence | 136 | 134 | Complete683 source-fact obligations; outside683 activation-keyword source |
| Charts | 119 | 118 | Complete Merman XY102 source/profile obligations |
| Flow | 23 | 23 | None in this bounded executable |
| Relations | 51 | 49 | Complete ER296; remaining three ordinary-topology sources |
| Official | 6 | 5 | Complete capability matrix |
| Markdown | 43 | 43 | None in this bounded executable |
| Imported terminal observation | 3 | 3 | Observation is not semantic certification |

The current sequence source is `22c55ffa`, chart source `e3ff31a8`, factory
`0a05b170` and restored relation source `415decea`. Final sequence test
`3d8be46c` adds the exact previously unsupported Markdown source as one complete
2-actor/1-message Model; chart test remains `3a21578e` and relation test
`105122dc`. These prefixes identify only the frozen records in the linked
proofs; subsequent edits need their own evidence.

Seven complete reviewed text Models now pass all geometry and three-width
controls. Full683 retains 270 Models: 268 drawing matches, two valid empty
matches and zero modeled source-positive, native or observation errors. All
previous 263 strong complete dictionaries are unchanged. Exactly seven intended
Models gain, while `merman/c1b8a55f506b235bc8a3` changes observation only and
remains unmodeled. The other 132 core and 281 additional-event records still
need source facts, and the original numbering-projection disagreement remains
an unmet foreign predicate.

FullXY102 separately records 76 nonempty geometry matches and five complete
empty states, with 21 pending profiles and zero geometry/oracle/empty-state
errors. Only the five original empty complete records change; the other 97
remain identical. An empty state is not a nonempty drawing. All original 108
function applications and foreign/API/ASCII/SVG obligations retain their own
pending status. The five DenseSource native tests pass for their complete
source Models without automatically promoting any official expectation.

The complete imported terminal observer actually runs all 2,916 distinct
sources and retains all 2,990 vendor identities. The reviewer independently
rehashed all 8,370 immutable asset files and matched every source/hash/ID set
against the actual report; there are zero observation errors. A returned
source or an unverified drawing in this report is still an unmet semantic
obligation. ER remains 247 typed matches, three source-positive unmet and
46 deferred profiles. Its temporary diagnostics are removed: restored415
executes51/49, has no ERDIAG output, and all five reports remain exact.

The first broad Markdown executions really failed43/42 because one original
fallback expectation became obsolete. Its exact source is preserved:
`sequenceDiagram\nA->>B: before #59; A->>B: after`. The local expectation
migration checks the complete decoded label, absence of the raw atom and one
physical head, plus a source-identical full sequence Model with restoration,
sentinel and forged-Model negatives. Both affected binaries subsequently run
in both presets; the first failures remain frozen as history.

The official matrix still contains all 2,738 records. Exactly the unreviewed
`sequence/nightly-check/should-handle-different-line-breaks.mmd` body dictionary
changes; the other 2,737 are exact, including the existing raw CPU title error.
Neither error is silently promoted. The original `c966a725` manifest, 2,145 raw
inputs, 593 body entries and 84 historical snapshots are unchanged. Any new
source/native/snapshot/policy admission remains a separate review gate.

Proofs are
`relations-audit/text7-empty5-independent-limited-dual-review.json` and
`full-obligation-audit/text7-empty5-combined-native-dual-{debug,release}-hashes.json`
with their frozen directories and comparison. These paths are under
`build/mermaid-renderer-research-20261020/`. The first source/native failures,
handwritten alignment correction and complete restored-production evidence
remain separate from the final bounded pass counts.

### Real PTY: decoded entities, complete empty state and literal BR/TAB

Independent reviewer `agent:/root/corpus_review` viewed all seven actual
1756x1244 JPEGs and rehashed all 146 artifact files for copied release CLI
`2faac4c8`. The three exact frozen Merman sources are942c (decoded heart/infinity
messages),bbbb (horizontal no-data state with Lost, alpha/beta and Score0..10),
and6579 (four complete multiline actors/messages/right notes, raw BR syntax,
literal escaped BR and one reversible TAB disclosure). Six production source
snapshots match the frozen sources; original Markdown, SSE text and all three
request byte/hash bindings were independently checked.

One actual OS PTY child88604 receives all three responses. Two verified kernel
ioctls resize200 to40 to200 after response3, with no fourth request. One real
PTY PageDown exposes the complete four-message/four-note scene. At40 columns,
sequence source fallback stays physically at the left while the complete empty
state still fits. Stale accessibility text is not pixel evidence. Primary03b
and restored06 whole JPEGs are byte-identical (`c688741f`). The initial
browser-scrolled03 and first restored05 remain supplemental evidence.

Both child88604 and server88345 are independently absent after cleanup. The
1,899,556-byte raw prefix remains exact with only68 cleanup bytes appended;
the installed CLI retains SHA `db1f8698`. Frozen cleanup records browser tab
153582786 closed; the reviewer did not reopen it. This proves three bounded
local deterministic-SSE terminal paths, not hosted inference, foreign rendering
or full-suite100%.

The immutable artifact directory is
`/Users/bytedance/.codex/visualizations/2026/10/19/01a15315-ef3b-7953-8e46-ca4aba152240/mermaid-entities-empty-xy/`.
Its verification SHA is
`8f3ad4d425ed1475f244537e24a1f09515ee222f0f919f2feeb0b4a6903de67c`.
Independent proof is
`build/mermaid-renderer-research-20261020/relations-audit/entities-empty-breaks-independent-real-pty-review.json`.
No production, test, fixture, shared module, public contract, installed CLI or
user settings change is made by this ledger update. No build, commit or push is
performed by the reviewer. Full Phase C and original assertion coverage remain
unfinished.


## Activation139 / ER97: independently reviewed bounded dual checkpoint

Independent reviewer `agent:/root/corpus_review` rehashed all54 frozen files in
`activation139-er97-{debug,release}-checkpoint-hashes.json` and parsed all six
complete native logs. Each preset executes sequence139/138, relations51/49 and
official6/5:196 unique tests and192 passes. Ordered RUN/OK/failure sets and all10
JSON reports are identical across presets. This is a three-binary checkpoint;
it does not replace the preceding seven-binary execution with inferred totals.

The private sequence parser permits the complete words activate/deactivate only
as message endpoints. Declaration and standalone-control admission remain
unchanged. The original Go activation-keyword function outside683, the eight
endpoint Models, malformed-source controls and the supplemental official
linebreak full4-actor/4-message/4-note/8-event Model all execute successfully.
The full683 feature gate remains genuinely red for unresolved source profiles.
The prior138 first run with135 passes and its original failures is retained.

The private ER change selects between the existing two label directions only
for header-only ordinary ER edges with the same horizontal face. Old247 strong
Models remain strong; Go417261 gains its six complete relationships and all
focused wrong-card/stroke/label/third-endpoint/missing-edge/orphan controls pass.
Current counts are248 strong,2 source-unmet and46 deferred profiles. Forty-four
old strong ASCII header-only records change routing pixels only; their complete
Models, nonframe fields, full semantic geometry, narrow execution and same-Node
restoration remain valid. These pixels are a native semantic projection, not
foreign snapshot equivalence. First62 bytes, mandatory5b56 and the complete20
parallel-self variants/mutations remain unchanged. The only self17 record
change is one validated wide frame. Full296 and the focused remaining-three
feature gates stay red. The rejected routing experiments remain historical.

Before any new official expectation is installed, its complete2738 rows remain
unchanged and retain both the linebreak-body and CPU-raw errors. The reviewer
separately approves the new linebreak native snapshot `acb7e2ef` against the
exact source and full Model: all124x50 drawn cells were independently
reconstructed, including four closed headers, four message shafts, four right
notes in source order, literal BR text, reversible TAB disclosure, lifelines
and the following sentinel. Both presets produce exactly those6861 bytes.
All84 historical snapshot hashes and base manifest `c966a725` are unchanged.

Only the exact body-specific final policy `14cddb28`, sibling snapshot and
private official test `3aa46a75` are approved for a subsequent root-controlled
installation and execution gate. This approval is not an assertion that the
policy has already run. Raw description metadata, original browser/E2E pixels,
CPU title omission and full official compatibility remain unresolved. No
production/shared/public-contract change is made by this review or ledger
update; no build, installation, commit or push is performed by the reviewer.

Independent proofs are under
`build/mermaid-renderer-research-20261020/relations-audit/`:
`activation139-er97-independent-limited-dual-review.json`,
`er97-independent-debug-implementation-review.json` and
`official-linebreak-snapshot-final-tuple-independent-review.json`.


## Role145 / linebreak projection: independently reviewed first dual execution

Independent reviewer `agent:/root/corpus_review` rehashed52 frozen files from
`role145-official2-first-{debug,release}-hashes.json` and parsed the complete logs.
Both presets execute sequence145/144 and official6/5:151 unique tests and149
passes, with identical ordered RUN/OK/failure sets. The original139 outcomes and
all4 sequence JSON reports remain exact. All six new Role enum/component/source-
integrity tests really execute and pass; this is not31 whole-scene source passes.
The new-role whole-scene extension remains separate authored scratch work and
needs independent review and real native execution before any producer work.

The exact reviewed linebreak body projection now executes drawn/source/drawn at
240/40/240 with the full native snapshot and sentinel. Exactly official matrix
index2033 changes; other2737 complete dictionaries remain equal, including the
old fractional projection. Both body-specific bindings are validated and each
is applied once. All5 JSON reports are byte-equal across presets. The original
CPU raw title-omission case remains the sole unresolved official case; its wide
and restored errors are preserved. Original SVG/browser/API/exact assertions,
full683 and complete compatibility remain unfinished.

Independent execution proof:
`build/mermaid-renderer-research-20261020/relations-audit/role145-official-linebreak-independent-limited-dual-review.json`.
This review is independent of my later Role Stage2 AUTHOR work. No production,
native test, fixture, shared module or public contract is changed by this ledger
update, and the reviewer performed no build, install, commit or push.


## CPU native projection / Role153: independently reviewed bounded dual evidence

Independent reviewer `agent:/root/corpus_review` rehashed all60 frozen files in
`CPU/first153-official8-{debug,release}-hashes.json`, rehashed26 prior checkpoint
files and parsed all four complete native logs. Both presets execute sequence
153/150 and official8/8:161 unique tests and158 passes. Ordered outcomes and all
six JSON reports are identical across presets. Every old145 ordered outcome and
all four old sequence reports remain unchanged.

Only official matrix index2705, the exact222-byte CPU raw source, changes to its
reviewed fixed-cell native projection: drawn/source/drawn at240/40/240. Other2737
complete dictionaries, both old sequence binding objects and the old fractional
projection object remain exact. All2738 case error arrays are empty. The new
CPU binding is validated and applies once. The complete source Model, original
metadata and native snapshot are pinned by facts `00dd7b8d`, policy `05ef17a1`
and snapshot `07d0d64f`. The original `c966a725` manifest and all84 historical
snapshot bytes are independently rechecked and unchanged. This is native
projection coverage; the original height60 SVG title omission, font metrics,
browser pixels and other foreign predicates remain non-equivalent pending.

The reviewer independently checks the root-authored one-condition crossing fix:
the previously missing intermediate lifeline arms are now rejected and the
DuplicateAliases group passes. The separately appended heavy-endpoint test
retains its valid complete hand scene but genuinely fails at both heavy-only
endpoint mutations, lines6099/6100, in each preset. These actual first failures
are preserved before the root-authored junction correction is installed. This
entry does not claim that the later correction has executed. The original
Stage2 whole-scene oracle was authored by this reviewer and is not self-approved
here. All31 public role sources honestly remain `complete_source_unmet`, no
source mutation is claimed executed, and the separate5 wrap plus4 coupled
profiles remain pending. Full683 and complete compatibility remain unfinished.

The independent real-PTY checkpoint additionally inspects all six original
2236x1379 screenshots and rehashes39 artifact pins. Three exact local fixture
responses share child89070; actual ioctls resize200 to40 to200 after response3,
with no fourth recorded request. The official598-byte body, Go activation-word
input and six-entity ER input retain source provenance and following sentinels.
The narrow screenshot right region is independently decoded as uniform
RGB(17,18,23), closing the retained-DOM/accessibility false alarm while preserving
the initial note and appended correction. Final raw capture preserves the
1947638-byte prefix plus68 cleanup bytes; recorded process/tab cleanup and the
unchanged installed CLI hash are verified. This is deterministic local SSE and
bounded real terminal evidence, not hosted model inference or full upstream
coverage.

Independent proofs under `build/mermaid-renderer-research-20261020/relations-audit/`:
`cpu-projection-root-role-first153-independent-limited-dual-review.json` and
`activation-linebreak-er97-independent-real-pty-review.json`.
Only this ledger is updated in the tracked tree by the reviewer. No production,
native test, fixture, shared module or established public contract is changed;
no build, install, settings update, commit or push is performed by the reviewer.


## Role153 strict endpoints: independently reviewed root-owned test fix

Independent reviewer `agent:/root/corpus_review` rehashed all22 frozen files in
`role-native-stage2/strict153-{debug,release}-hashes.json` and parsed both complete
logs. Both presets execute153 unique tests with151 passes. Only the full683 and
full31-source feature gates remain red. The heavy-endpoint regression now
passes, including its valid complete scene and both exact bits-preserving
light-to-heavy endpoint mutations. Every other152 ordered outcome remains
identical to the prior first153 run, whose two genuine false accepts at
lines6099/6100 are preserved. All five complete JSON reports remain byte-equal
both across presets and against that prior first-red checkpoint.

This closes only the root-authored one-condition native test-oracle correction
`1571c5ce`, which selects the existing exact bits/style check for new-role
business junctions. It does not change the producer, frozen Models, resource
caps or legacy Participant/Actor-only behavior. All31 complete public role
sources still report `complete_source_unmet`, with zero source mutations
claimed executed; the separate5 wrap and4 coupled profiles remain pending.
This review does not self-approve the reviewer's original Stage2 author work,
permit production implementation, or graduate any source/family denominator.

Independent proof:
`build/mermaid-renderer-research-20261020/relations-audit/role-stage2-root-heavy-endpoint-fix-independent-dual-review.json`.
Only this append-only ledger is changed in the tracked tree by the reviewer;
no shared module or established public contract changed, and no build, install,
settings update, commit or push was performed.


## ER peer-layout label direction: independently reviewed limited dual result

Independent reviewer `agent:/root/corpus_review` rehashed40 new frozen files and
32 baseline files and parsed all four complete native logs. Each preset executes
relations58/56 and official8/8. Ordered outcomes and all seven reports are equal
across presets. The two remaining relation failures are the full296 feature gate
and the focused remaining-three source gate; their unfinished obligations are
retained. The original relation test source is byte-identical to corrected58 and
the preceding4aaf stage, so no expected Model or oracle was changed for this gain.

The owning-only production `f5be6c5f` adds a default-false private fact recording
successful proper peer-rank layout. Only that source-derived plain ER TB layout
permits the already-existing charged actual-port label-direction comparison for
opposite faces. No A* call, label/port candidate, retry, bitmap, padding, shared
module, public contract, resource cap or budget reset is added. A fresh layout
object prevents state leakage; typed tables, other families and all ineligible
layouts retain the preceding rule. The twelve frozen ineligible exact97 pixel
tuples and complete semantic checks actually pass in both presets.

The independent frozen-canvas recomputation confirms the prior1ff failure's
5931-cell external and42-cell enclosed free regions. The completed C2-to-D0
route encloses the future C2 approach; its selected label orientation has actual
port costs25 forward versus15 reverse. Releasing that owned contour, or the
identified physical cut cell, reconnects the regions. This diagnosis is not a
claim that an unexecuted replacement succeeds: the complete accepted native
execution supplies that proof. The immutable249-byte source now draws all12
entities and12 relationships with all focused card/style/label/third-target/
missing-edge/orphan mutations, deterministic placement, three widths and the
following sentinel.

All old248 strong records remain strong;1ff alone gains, yielding249 strong,
1 source-unmet and46 independent pending profiles out of296 records/293 unique
sources. Exactly13 complete dictionaries change:12 old strong records change
only their wide frame, and the new target changes source outcome/frame/errors.
Other283 complete dictionaries are unchanged. All13 changed source Models were
independently checked to be plain TB proper peer DAGs. Every296 source restores
identically, and there are no strong geometry/oracle errors. First62/57nonER,
self17, parallel20 and the full official2738 report remain byte-identical.
Native routed pixel changes are not foreign exact-layout equivalence. Full296,
original functions/assertions and full Phase C remain unfinished.

Independent proof:
`build/mermaid-renderer-research-20261020/relations-audit/er-layered-label-independent-limited-dual-review.json`.
The reviewer changed only this tracked ledger append and scratch proof files;
no actual code, tests, fixtures, build, install, settings, commit or push changed.


## Contextual IDs and current seven native binaries: limited dual checkpoint

Independent execution-artifact reviewer `agent:/root/corpus_review` rehashed all
104 frozen files and parsed all fourteen complete native logs. Each preset runs
423 unique native tests, with418 passes and five genuine failed feature gates:
full683, fullXY102, fullER296, focused remaining-three ER sources, and the full
official capability matrix. Sequence167/166, charts121/120, relations58/56,
official8/7, flow23/23, Markdown43/43 and terminal3/3 have identical ordered
outcomes across presets. All24 complete JSON reports are byte-identical. This
is the actually executed local native test inventory, not the complete frozen
upstream assertion denominator or full Phase C graduation.

The root's initial summary report-hash map accidentally reused historical values
while its `files[]` had correctly pinned all fresh reports. Both original ledgers
are retained. Independent reverse comparison confirms the correction changes
only the report map and explicit correction notice; all104 frozen files, native
outcomes, logs and actual report bytes stay unchanged. All seven changed map
entries are recomputed directly from fresh frozen files in each preset. The
immutable official manifest remains `c966a725` and all84 original snapshots match
the historical generation hash list.

Contextual-ID producer C `896b09e8` was authored by this ledger owner. Its separate
static approval belongs to root and `agent:/root/corpus_oracle`; its runtime
approval belongs to `agent:/root/terminal_union`, proof `824215a9`. This entry
records those independent approvals rather than self-approving the author's
implementation. The peer proof rehashes76 executed/baseline artifacts: the
native test source is unchanged, all31 complete source-bound Role Models now
pass full geometry, meaningful mutations, restore and sentinel, and only daeaa1
changes in that report. Other30 full records/frames remain identical to A/B.
Full683 changes only the corresponding daeaa1 observation dictionary; its270
existing Models still have268 drawing matches and2 valid-empty matches, with
132 core-model and281 other event-region fact obligations pending. Separate5
wrap and4 coupled appearance profiles and original foreign predicates remain
unfinished. The official51 body conflicts over38 distinct sources remain real
errors; no expected source, snapshot or projection policy was promoted here.

The current XY inventory separately retains77 drawing matches,5 valid-empty
matches and20 pending profiles. ER retains249 strong native matches,1 genuine
source-unmet record and46 independent pending profiles. None of these native
projection counts certifies original API/ASCII/browser/SVG/exact-pixel contracts.
The source-only Role38 worksheet review is a later authoring prerequisite and
must not be counted as closing the currently unpromoted official conflicts.

The independent real-terminal checkpoint belongs to `agent:/root/terminal_union`,
proof `c8377392`; its verification and six original screenshot hashes were
rechecked for this ledger append. Three exact local SSE fixture responses share
child61591: eight labeled actors covering seven role types including all six new
contours, the complete contextual-ID source, and the four-layer ER source. The
union of the first two sources covers all eight roles. The corrected caption is
preserved. Recorded real ioctls resize200 to40 to200 after response3 without a
fourth request; narrow source projections and following sentinels are visible,
and restored ER200 is byte-identical to its wide screenshot. The peer review
records raw-prefix preservation, process/port cleanup, historical tab closure and
unchanged installed CLI. These are deterministic local fixture/public terminal
observations, not hosted inference or screenshot-only semantic certification.

Independent current execution proof:
`build/mermaid-renderer-research-20261020/full-obligation-audit/role167C-erf5-seven-independent-dual-review.json`.
Peer C runtime and terminal proofs:
`sequence-audit/role-contextual-id-design/production-C-independent-runtime-review.json`
and `live-pty-independent-review.json` under the same research root.
Only this tracked append-only ledger and scratch audit files changed by this
reviewer. No actual production/test/fixture/shared module or established public
contract changed; no build, install, settings update, commit or push was run.


## Official Role38 body projections: limited first execution checkpoints

This append records three separately frozen root-executed checkpoints. It does
not combine them into a newly executed seven-binary total. The preceding actual
seven-binary423/418 inventory remains a historical checkpoint, not the current
sum of independently rerun binaries. Full upstream compatibility and Phase C
remain open.

The complete first169 sequence run executes169 tests with168 passes in each
preset; only the full683 feature gate fails. All167 prior ordered outcomes and
all six prior sequence/official JSON report bytes remain unchanged. The new
complete38 body report records38 full native geometry matches, zero source-
positive unmet or geometry/oracle errors, and the source-bound alias, Role,
message, note, fragment, box, lifetime and activation adversaries. Six erased-
message and eight changed-receiver candidates are explicitly schema-invalid
and are not counted as geometry rejections. All seven reports are byte-equal
across presets. The original51 official body conflicts remain frozen in that
first169 history before any policy promotion.

The separately executed reciprocal test-first relation binary runs62 tests with
59 passes in both presets on unchanged production `f5be6c5f`. All58 prior ordered
outcomes and the seven prior relation/official report bytes remain exact. The
complete eight-route hand, mirrored valid embedding versus the selected native
plan, and complete296/old-cycle controls pass. The immutable target source and
independent renamed mixed-stroke complete source positives still fail to draw;
full296 and focused remaining-three gates also stay red. This ledger owner is
the reciprocal native test AUTHOR, so this entry records the root's actual
first execution and its preserved failures, not self-approval of that oracle
or permission to implement the reciprocal producer.

The later first11 official harness executes11/11 in both presets with identical
ordered outcomes and both JSON/Markdown reports byte-equal. Independent runtime
review belongs to `agent:/root/corpus_oracle`, proof `532673d0`; the source facts
were authored by that agent and rely on the separate source/schema review
`437391b1` by this ledger owner. All126 frozen execution/asset pins in the peer
proof are rehashed for this append. Exactly51 selected body dictionaries change
to their independently reviewed native projections over38 complete sources.
Other2687 complete dictionaries remain exact: all2145 original raw records and
542 other body records. The fourth binding validates all38 complete source
Models and consumes all51 original body applications. The old three binding
objects and original projection object remain exact; base manifest `c966a725`
and all84 historical snapshot hashes remain unchanged.

This11/11 result is declared native-matrix conformance, not full upstream100.
Original raw drawing coverage remains2/2145, or0.09324009324009325 percent.
Body projection promotion does not certify raw metadata admission, original
browser/font/theme/wrap/SVG/ASCII/API predicates, or the frozen upstream
assertion denominator. Source-only proposals to join31 Role and41 Go Models
into the full683 matrix are still authored scratch; their proposed342 total
must not be recorded as executed here.

Execution evidence under `build/mermaid-renderer-research-20261020/`:
`sequence-audit/official-role38-native-design/first169-{debug,release}-hashes.json`
and `first169-dual-comparison.json`;
`relations-audit/er-reciprocal-native-design/first62-{debug,release}-hashes.json`
and `first62-dual-comparison.json`;
`sequence-audit/official-role38-projection-design/first11-{debug,release}-hashes.json`,
`first11-dual-comparison.json` and
`independent-first11-dual-execution-review.json`
(SHA256 `532673d07373943219057793eb1c24db508072c70ab1ea89838062bf62764592`).
Only this tracked append-only ledger changes in this update. No actual code,
test, fixture, shared module or established public contract changes; the ledger
owner performs no build, install, settings update, commit or push.


## Reviewed342 / reciprocal250 / Termaid45: separate bounded execution facts

These are separately executed, frozen checkpoints, not a freshly composed
seven-binary total. The historical423/418 inventory remains historical. The
previous official11/11 closure remains in force:51 reviewed body applications
consume38 full Models, other2687 complete dictionaries remain exact, raw drawing
coverage stays2/2145 (0.09324009324009325 percent), and original foreign predicates
and full upstream100 remain unfinished. None of the following native totals
changes that raw denominator or asserts a full-suite100 result.

The complete683 join was authored by this ledger owner and independently
reviewed at source/design/code and runtime by root/`agent:/root/corpus_oracle`.
Its first170 run genuinely failed: integrity threw `Expected exact display
sequence number text`; the matrix had30 oracle errors,310 drawing matches and2
valid-empty matches. The author and prior reviews incorrectly selected
DisplayText for ce60, whose explicit schema is legacy-number and whose90 message
number fields are integer0. Original facts, inventory, design/review history and
bad candidate96dda remain preserved. The correction changes only the two Role
reader arguments to LegacyInteger; no Model, source, pixel, validator or cap is
repaired to match output. The first native OS exit is unavailable: a report-
collection wrapper failed on a nonexistent expected filename and recorded its
own exit1. Only already-emitted artifacts were re-frozen under real report
names; no rerun or native OS status was inferred from that wrapper failure.

The independently reviewed repaired170 run executes170/169 in both presets,
with native OS exits1 and only the complete683 unclassified gate red. All342
complete source Models now actually validate:340 drawing plus2 valid-empty;
31 Role and41 Go source-bound Models add72 complete checks. Other611 full row
objects, including all old270, remain exact, and every683 three-state frame and
source-observation field remains unchanged. All five standalone old reports
remain byte-exact; six reports are equal across presets. Core402 has301 Models
and101 explicit-null obligations; outside core has41 Models and240 unclassified
sources. Those341 genuinely unclassified obligations keep full683 red. The
successful integer correction is a repaired result, never the original first170
all-green. Runtime peer proof is `c344fa73`; it does not self-approve this
ledger owner's source/test authoring or certify original foreign predicates.

The reciprocal producer690 independently closes a different bounded execution:
relations65/64 and official11/11 in each preset. Ten reports are byte-equal across
presets. The complete296 matrix has250 strong typed native matches, zero active
source/geometry/oracle errors, all restorations intact and46 independent pending
source profiles, so full296 still fails. Only immutable4a changes in either
full296 report; other295 complete dictionaries and all249 prior strong frames
remain exact. The original remaining-three, renamed mixed-stroke source and
permutation good-scene/mutation gates now pass. First62/57nonER, self17,
parallel20, layered32, odd-cycle controls and official2738 full dictionaries
remain unchanged. Earlier887 failures, diagnostics and the future-anchor
counterexample remain recorded; this is measured bounded success, not a
universal routing claim. The independent runtime reviewer is
`agent:/root/corpus_oracle`, proof `056ac9e9`.

The author-only observation of first172 is separately labeled here: both frozen
full logs show172/170, with only full683 and the complete47 source-profile gate
red. This ledger owner rehashes all25 frozen files per preset, parses both full
logs and confirms all seven report bytes are equal. The new report records45
complete native matches,2 explicit pending full sources, zero source-positive
unmet/native/oracle/observation errors and45 sources with executed adversaries.
Actual counts include111 aliases,144 each message label/arrow/stroke/erasure,
13 note texts,46 fragment labels,21 dividers and45 unrelated-ink controls.
The new resource-versus-physical-activation-cap self-checks and source integrity
TEST pass; resource exhaustion is not negative evidence. Because this ledger
owner authored those TESTs, this is an attributed root-executed first baseline,
not independent approval; the independent runtime review is still pending at
this append. No172 green gate is inferred from45/45. Destroy62b409 and rect3f3d6
remain pending in immutable ffdbc facts. Their separately source-approved two-
Model supplement and173 candidate are scratch only and unexecuted here.

Proofs and first-run freezes under
`build/mermaid-renderer-research-20261020/`:
`sequence-audit/complete683-reviewed-model-join/independent-repaired170-dual-review.json`
(SHA256 `c344fa737756bc29a05eabaaebc2fcea26e3fbb6a5fff4c98cdf54f946904aaf`),
`relations-audit/er-reciprocal-producer-design/independent-first690-dual-review.json`
(SHA256 `056ac9e9d036ffaa207c079ae46ba5c0529d12f030d2b8297904c9bc8860db14`),
and `sequence-audit/remaining-termaid-mmdflux-source-inventory/first172-{debug,release}-hashes.json`.
The earlier official11 peer proof remains `532673d0` in the preceding entry.
Only this tracked append-only ledger is updated here. No actual production,
native test, fixture, validator, shared module or established public contract
changes; no build, native execution, install, settings update, commit or push is
performed by this ledger owner.


### First172 independent runtime review completed (historical append)

The previously recorded first172 author observation now has independent runtime
approval by `agent:/root/corpus_oracle`, proof
`sequence-audit/remaining-termaid-mmdflux-source-inventory/independent-first172-dual-review.json`
under the research root (SHA256
`343b6137ad305f35b4e3deee2f98977cfa990250563061124291731d2935c38a`).
The peer verifies50 frozen files, both complete172/170 logs, old170 filtered
ordered outcomes and six old report bytes, plus seven byte-equal reports across
presets. All45 full source Models and meaningful adversaries actually pass;
62b409 and3f3d6 remain explicit pending profiles in that first baseline. The
earlier pending-review paragraph remains historical, not silently replaced.
This is limited first172 execution approval, not source47 completion,173
execution, original foreign predicate certification, or a new combined total.
The ledger owner authored these tests and records the peer verdict, not self-
approval. No actual code, fixture, shared module/public contract or build changes.


### Class envelope presentation: complete first Release116 trial

The reviewed private optional Class-presentation checker now passes all16
independent complete literal papers and rejects all106 immutable whole-frame
pixel adversaries, with zero skipped baselines, schema errors or resource
errors. A separate TEST also rejects both independently constructed missing
inward-divider-arm cases after each complete good passes. The initial115
original-checker failures remain frozen; no original fixture was rewritten.
The R0 static P1 (interior strokes did not require attachment at both frame
sides) was fixed and reviewed before this R1 trial.

The real Release116 binary ran116 TESTs:113 passed and the same three older
mandatory corpus gates remain red. All original113 ordered statuses and22
previous reports are byte-identical. The two formerly failing Stage2 TESTs
now pass through source-bound optional presentation; this is checker evidence,
not whole22 source admission or new production drawing coverage. Debug parity
is still pending. The wholeClass481192 pending profiles and Endpoint/Angle
fallback/resource debts remain explicit.

Evidence under `build/mermaid-renderer-research-20261020/full-obligation-audit/`:
`envelope116-presentation-r1-first-release-hashes.json` and
`root-envelope116-presentation-r1-first-release-conservation.json`
(SHA256 `125d4b4ff0e0dd8a5ed97a00e1cc144e635f8cc2f3f9f63d5982eb46354cf7bf`).
Native source SHA256 is
`58eff899bde9b1180586a776f6bab8fc73d2204b47a98f2d7b81735da85bc724`.
Only the private native checker/tests changed at this checkpoint; no shared
module, established public contract, cap, producer, install or policy changed.


### Complete official Body22 native first Release264 result

The real first full Release264 sequence-native run passes264/264 TESTs. All22
whole original body applications are captured before semantic decoding, decode
their complete active Models, match full geometry and execute meaningful
mutations. There are zero source-positive, geometry/oracle, observation or
profile-pending errors. Every wide frame draws;12 narrow frames also draw and
receive complete validation, while10 narrow frames retain exact source fallback.
All restored frames/sentinels remain complete. This is native body geometry
only; official matrix admission, Debug parity and original foreign predicates
remain pending.

All old262 ordered statuses and28 old reports are byte-identical. The runner's
literal-name list captured22 old reports plus the new report; six dynamically
named old reports were separately frozen from their actual fresh first-run
outputs, within the recorded execution timestamps, without a rerun. That
supplement must accompany the first-run manifest.

Evidence under `build/mermaid-renderer-research-20261020/full-obligation-audit/`:
`body22-native264-first-release-hashes.json`,
`body22-native264-first-release-dynamic-report-supplement.json` and
`root-body22-native264-first-release-conservation.json` (SHA256
`ebbacf6e3550e886dd42d9a38ba0480420bade937e609643e7e90de81690a3d5`).
Native source SHA256 is
`8fcc2e677b0b8461e7409e1be5ed69d2dc84481340ae637d8091d29bc03fe36f`.
Changes are two appended private TESTs and the reviewed40076-byte reference
fixture; no shared module or established public contract changed.


### Body22 complete first dual-preset native parity

The complete first Debug264 run passes264/264 in399100ms; the complete first
Release264 run passes264/264 in27533ms. All264 ordered statuses and29 complete
reports are byte-identical between presets. All22 new body scenes retain full
geometry,1807 meaningful geometry mutations, zero errors and12 narrow drawings
plus10 exact-source narrow fallbacks. Root independently reviewed the first
Release runtime and reconstructed all66 complete physical frame hashes and
full-row following sentinels. The author runtime observation is explicitly not
independent approval; independent concrete code review belongs to terminal_union
and independent first-runtime review to root.

Evidence under the full-obligation-audit research directory:
`root-body22-native264-first-dual-parity.json` (SHA256
`1e46644ce12e638499c0ca4d01fa51687bd14fda8bc99d97040d1b823bb6b57f`) and
`root-body22-native264-independent-first-release-review.json` (SHA256
`2031af1a8b2c625499183f97666c2649a0b30cbc807923e325661ecd02ab43c6`).
Official Body22 matrix/policy admission and all original foreign predicates
remain pending. No shared module, established public contract or producer changed.

### Relations budget algorithm: genuine first117 result

The reviewed private-native optimization retains caps and all captures/Models:
source-selected conservative first-glyph byte buckets, one shared BFS arena
with paid touched-cell resets, and paid exact-restored Grid equality before
reusing the deterministic hash/sentinel. Actual complete first Release117 is
114/117, not all-green. Whole6eba now passes Endpoint geometry/mutations and
Angle LF/CRLF geometry/mutations, with zero resource errors. Endpoint geometry
is25/29 with4 mandatory source fallbacks; Angle positives are6/6. All35 changed
public capture rows keep identical full physical frames/hashes/fallbacks.
Twenty report files are byte-identical; the4 changed reports contain budget
allowances, the newly successful complete6eba verdicts, or paper work counters.
All16/106 presentation papers and both arm controls stay green.

The unchanged old dense-workspace TEST first fails because the optimized
checker reaches the exact structural error `Unclaimed text/control/marker/route
glyph exposes a lost or extra fact` instead of exhausting Work. Both the genuine
full first failure and a diagnostic-only focused native failure are frozen.
A separately reviewed minimal test-contract correction keeps the whole dense
scene/Model and tests structural rejection, an aggregate work lower bound and
independently declared exact-cost/one-short resource boundaries. It does not
claim to detect every omitted tariff. Its actual first trial is still pending
at this entry; no failed ledger is reset, cap raised or old first failure erased.

Evidence: `native117-budget-algorithm-first-release-hashes.json` and
`root-native117-budget-algorithm-first-release-conservation.json` (SHA256
`1d87ff6d2856f8680f06459d6f826ee1e3d85cbbcb600a9a8b66f8fa6f60a003`).
Only private native implementation/tests changed in this checkpoint; no producer,
shared module or established public contract changed. Full corpus graduation,
Debug relations parity and four ordinary endpoint-source fixes remain open.


### Relations corrected-resource117 first dual parity

Complete first corrected-resource117 runs have115/117 passing in both Release
and Debug. All117 ordered statuses and24 full reports are byte-identical.
Only the pre-existing mandatory Class481192-profile gate and four Endpoint
source fallbacks remain red; unexpected oracle/resource errors are zero.
Whole6eba Endpoint and Angle LF/CRLF full geometry/mutations, all16 good papers,
all106 pixel rejections and both inward-arm controls pass in both presets.
The original117 stale-resource first failure and diagnostic run remain frozen.

Evidence: `root-native117-budget-resource-r1-first-dual-parity.json` under the
full-obligation-audit research directory (SHA256
`560aad4f11784736b7df7c24a69608e65bb4af4e17e241bb4df15fa807f3f481`).
The separately reviewed ordinary-caption capacity producer trial begins only
after that baseline: two local measurement hunks over producer2b5, candidatef15.
Its compile/runtime result is still unknown at this entry. No new source credit,
shared module/public contract change, cap increase, install or commit is inferred.


### Ordinary-caption capacity f15: actual first trial and diagnosis

Complete first Release117 stays115/117, with the same ordered statuses as the
prior2b5 baseline: zero new covered sources and zero losses. Existing289 Class,
25 Endpoint, all6 Angle and16/106 presentation-paper obligations remain green.
Only three already-drawn Class scenes change wide/restored layouts; all22 other
reports are byte-identical. Four mandatory endpoint source fallbacks remain.
The local capacity change is provisional and is not described as a completed fix.

Actual read-only Debug observation of complete c506 reaches the third sorted
edge, Order->Item contains, after the earlier two edge dispatches. All192 third-
edge port pairs fail before clearance acceptance. Five source horizontal ports
remain after the caption-direction guard; three are blocked by actual earlier
persistent caption rectangles and two require additional cell observation.
Earlier route coordinates and Work remaining are not observable in this run;
no ABI offsets or guessed counters fill that gap. LLDB exits0 while the native
mandatory test exits1; these outcomes are distinct.

Complete first Release public regressions pass Markdown44/44, Flow23/23 and
Terminal3/3; Charts stays120/121 with its unchanged mandatory XY102 gate.
All four ordered status lists and12 complete reports are exact against2b5,
including all2916 terminal applications. No new source credit, final Debug
parity, full ctest success, installation, commit or push is inferred.

Evidence under full-obligation-audit: root-class-capacity-f15-first-release-
conservation.json (SHA256821f35b3cb7d74b590780540424cb5a4d45535c7ab9efe6373c4c5cf04013ef8)
and root-class-capacity-f15-public-regression-release-conservation.json
(SHA256896e311d2c2e05bba2d6e0f96d5d2f437529546d8ad5adcc0599e7f31c897439).
Read-only observation is frozen under relations-audit/class481-source-contract/
null192-source-closure/endpoint-production-source-design/first-r1-fallback-
static-audit/readonly-first-failure-observer/root-capacity-f15-c506-debug-run.
The current producer changes only the owning relations layout implementation;
no exported module interface or established public contract changes.


### Complete ClassEnvelope22 and State85 test-first integration

The first complete Release121 run is116/121. All original117 ordered statuses
and24 existing complete reports are byte-identical to the f15 baseline. The
three titled literal papers and their body/presentation mutations pass. All22
original Class envelopes are captured and source/Model/presentation-bound,
including4157 primary fields and64 metadata leaves, but all22 remain genuine
source fallbacks with zero oracle/geometry/observation errors. No new complete
source projection is credited.

Both State85 TESTs first throw the exact private setup diagnostic `State85
exact primary path namespace`, before any per-source loop or State report.
The immutable source inventory has two legitimate primary namespaces:2 files
under merman/oracle-models and38 under merman/runtime-capture/oracle-models.
The loader incorrectly admitted only the former. A separately root-reviewed
single-expression correction admits exactly both slash-terminated prefixes,
retaining traversal rejection and all original size/SHA/authority/Model checks.
The full fixture and all85 source tuples stay unchanged. The genuine first
failure and the prior static-review omission are preserved; corrected execution
is still pending at this checkpoint. This changes only the private test loader,
with no shared module or established public contract change.

Evidence: full-obligation-audit/root-native121-stage3-state85-genuine-first-
release-conservation.json SHA2567decb91ba4ce4c14a7b6b97bc408425ce4c33554ea7fd62d25fb913a50703945;
State85/native-test-first/primary-path-fix-r1/ROOT_INDEPENDENT_REVIEW.json
SHA256d6a99ad2ac24b2584b8dc32deb0f3c6626e6bf2601b5b94f169650f47b5e49b3.

The actual current-f15 pre-admission official15 run is14/15. All2738 complete
rows and all15 ordered statuses are exact against the prior Raw193-r2 baseline;
report SHA256583f0419d7e1adf6d08a7c1f5c36b8c5f54dce3c53dd7a174feb198f76373c91.
The22 body errors remain mandatory. Native264 geometry prerequisites and the
sixth Body22 admission design are independently reviewed, but no new policy or
snapshot has been installed and no official/foreign-assertion credit is inferred.


### Complete121 first dual-preset source gate

After the minimal State loader correction, all85 public source observations,
77 complete Model decodes and48 full geometry/mutation scenes execute. There
are28 mandatory modeled-source fallbacks,7 positive profile gaps,1 hidden-choice
geometry/policy hold and1 original malformed/error API policy hold. Source
integrity, unexpected oracle/resource errors and observation errors are zero;
the hidden-choice outline mismatch is retained as1 geometry error. Actual setup
Work remaining is340812. All old25 reports remain byte-identical.

A separate typed-control correction retains the original seven-byte UNKNOWN
probe with its exact prior maximum-two-byte text rejection, and adds two-byte
XX to reach the direction enum guard. It changes no codec, source Model, capture,
geometry, policy or renderer. The genuine earlier diagnostic mismatch remains
frozen. Complete first corrected Release121 is117/121 in10755ms; Debug121 is
117/121 in153384ms. All121 ordered statuses and27 full reports are byte-identical.
The four remaining mandatory gates are Class481 profiles, four Class endpoint
sources, all22 Class envelopes and complete State85 graduation. No whole100,
full ctest, terminal screenshot, install, commit or push is inferred.

Evidence: full-obligation-audit/root-native121-stage3-state85-first-dual-parity.json
SHA2567866c2e09bd6bf9c046aa5fd9015f2f17060870fb8f66650eee5d80358bc81cc;
root-state85-path-r1-complete-first-release-conservation.json
SHA25667718944347873d3c4cfb566c3b0859fae03b6cbedaf144c35a0b394a378d30d.
Only private native tests/fixture integration changed at this checkpoint; no
shared module or established public contract changed.

### Source-bound c506 path obstruction evidence

The independently reviewed frozen117 diagnostic executes the unchanged original
Endpoint29 TEST with observer-only completec506 selection. Its linked frozen
DWARF UUID and all source/build/binary inputs are checked before the one guarded
launch. LLDB exits0; the native process exits1. Original26 plus5 added events
read5954276 bytes under the unchanged callback/session limits. Four prior
left/right coordinate arrays are complete. The three Canvas snapshots and Work
counter remain explicitly unobservable because complete current DWARF is absent
or ambiguous; no guessed offsets or free-cell assumptions substitute for them.

Actual earlier tracks-right path coordinates cross both previously unknown
source caption clearances: x31..35 at y14 on the left, and x50 at y14..16 plus
x51,y16 on the right. Later edge2 dispatch and the painter's exact successful
source flow establish persistent blocked route witnesses in those strips.
Together with three earlier caption rectangles and seven vertical-port guards,
all192 third-edge source choices have a geometric obstruction. Unread Work does
not exclude simultaneous exhaustion. No target-cell observation, completed fix,
source pass or need for further box-height inflation is claimed.

Evidence: relations-audit/class481-source-contract/null192-source-closure/
endpoint-production-source-design/first-r1-fallback-static-audit/
readonly-first-failure-observer/root-capacity-f15-c506-cell-observation-r1/
ROOT_SOURCE_CLEARANCE_CLOSURE.json
SHA2568376f19849f4044f886eab1954318fe18f9247a9917d1395c09fd8fd2e659aa6.


### Body22 official admission: preserved first failures and complete dual result

The new fixed sixth body descriptor uses the unchanged40,076-byte complete
source refs, the already independently proved native264 body geometry and22
new glyph snapshots serialized only from those approved complete frames. The
old three singleton, RoleBatch and Raw193 contracts remain literal. The first
compile fails because two Google Test EXPECT_THROW macros share one physical
line and generate the same goto label. A separately reviewed mechanical line
split preserves both assertions. The new unused count is removed. A separate
reviewed local selector correction reuses its22 identity set instead of calling
the complete old193 selector22 extra times per matrix request; runtime speedup
is not claimed from the static loop reduction.

The genuine first complete18 run then has14 passes and4 failures. A new raw
reader incorrectly applies the native flat-asset parent guard to nested official
source paths. It fails before matrix observation, so its report has0 rows and
is not conservation evidence. The root/peer-reviewed one-argument correction
supplies the actual immediate source parent to the unchanged regular-file
helper. Safe relative sequence path and immutable manifest membership precede
the bounded physical read; complete SHA/body binding follows that read. No
native helper contract, source fact, Model, snapshot, cap or producer changes.

Complete first corrected runs pass18/18 in both presets: Release48,976ms and
Debug196,303ms. All18 ordered statuses and the full2,738-record report are
byte-identical (SHA25611ac431703e3f12f5316cfc3d77f2575cb344afb9806df92436e3b1dce35ac58).
Exactly22 intended body rows change from unreviewed drawing errors to reviewed
native projections; every other2,716 complete dictionary, all2,145 original
records, all193 prior raw applications and five old binding objects remain
exact. The new sixth binding consumes22/22. Narrow body outcomes are12 drawings
and10 exact-source fallbacks, with wide/restored22 drawings. Original-file
drawing coverage stays195/2145; safe fallback is never counted as drawing.

Evidence in full-obligation-audit: root-official-body22-corrected-first-release-
full-conservation.json SHA256c7df07577d8a763fc294f5c26d85813f266f8a37656ca9d80960d54c5daa7cbb;
root-official-body22-corrected-first-dual-parity.json
SHA25699f8ebfe4a38fa4ad7dbecab41413a202092c9a74912e4e76df4a8b6f7c975c2.
Original browser/SVG/API/ASCII/font/CSS predicates and other family/source
obligations remain open. This closes only Body22 admission, not complete-suite
100%, full ctest or final real-terminal screenshot gates. No shared runtime
module or established public contract changed in this test/asset admission;
all work remains local, uncommitted, unpushed and uninstalled.

### State bare declarations and display aliases: complete first dual trial

The exact native121 byte prefix is retained; two native TESTs add seven entire
source-authored positive scenes and twelve explicit closed terminal profiles.
The unchanged-f15 complete first Release123 run passed118 and failed5; all old
121 ordered outcomes and27 report bytes were exact. The new report observed all
19 sources, decoded all7 positive Models, matched0 scenes and retained12 closed
whole sources, with0 execution/observation errors. This first red remains frozen.

Independent review found a P2 in the production candidate's new entity-vector
payment: full-capacity allocation2*n plus relocationn plus insertion1 requires
3*n+1 object units, while the original candidate claimed2*n+1. The rejected
original remains immutable. Only two new State payments were corrected; the
existing1M/8M caps and source Models were unchanged. The actual entity ceiling
is128 (64 ordinary +64 groups), not the earlier author document's96.

The corrected State-only two-hunk producer a156448d passed119/123 in both first
complete runs: Release10875ms and Debug141261ms. All123 ordered outcomes and all
28 complete report bytes match across presets. The original85 sources/77 Models,
8 nulls, choice and original-error/API gates remain exact. Whole State geometry
plus meaningful mutation groups increase48 to59: new ordinals2,3,6,7,14,17,26,27,
28,37,39. All48 earlier good source report dictionaries are byte-exact, with no
loss; the other26 reports are byte-exact. All7 new positive controls and12 closed
controls pass, with0 errors. This is eleven actual full-source gains, not the
fifteen source-eligibility estimate and not full85 or original-suite completion.

Four eligible whole sources15/21/30/33 remain genuine fallbacks. They combine
horizontal LR/RL layout with physical left/right Notes on an ordinary/composite
state. A separate local boundary-measurement proposal is under independent
review; its internal first failure has not been debugger-observed. Class481,
Endpoint29, ClassEnvelope22 and whole State85 completion remain four mandatory
red native gates. No shared module or established public contract changed in
this State parser/test append. Work remains uncommitted/unpushed/uninstalled.

Evidence: full-obligation-audit/root-state-bare-alias123-test-first-release-
conservation.json, root-state-bare-alias-work-r1-first-release-conservation.json
and root-state-bare-alias-work-r1-first-dual-parity.json. Original and revised
producer locks and independent reviews live under relations-audit/
state85-inventory-source-discovery-root/renderer-first-failure-design/.

### Horizontal State Notes and first Class-envelope production trial

The reviewed State Note correction is one source-measured insertion: horizontal
scope width/advance now reserve the actual outer physical left/right Note
clearance using existing end-deficit fields. It changes no Class/ER path, source
Model, route or cap. First complete Release125 passes120 with the same five
mandatory gates; State85 gains exactly15/21/30/33, reaching63 complete geometry
and useful mutation groups. All59 earlier good State dictionaries and all other
28 reports remain byte-exact. The necessary clipping condition is source-derived;
an actual internal first PC was not debugger-observed. Its Debug proof remains
pending at this checkpoint.

The Class77 native append retains every123 prior byte and adds two TESTs plus
one frozen48807-byte fixture. Genuine unchanged-Class complete Release125 passes
120: all77 sources are captured/bound,2 plain scenes and47 closed boundaries
pass,28 mandatory metadata positives remain unmet,0 protocol/resource errors.
All123 prior ordered outcomes and all28 prior reports are exact.

The independently reviewed Class producer uses26 unique edits in three files:
a private optional Relations owner hint, factory Class source routing and the
owning finite Class YAML/init/title/empty-compartment code. The public renderer
API, ER helpers, State body, routing, caps, frozen source Models and checker are
unchanged. Additional shared-path fixed fees are explicit, so old near-budget
threshold equality is not claimed. Exact hunk composition retains the tested
State Note insertion; no complete preview overwrites advanced source.

The genuine first complete Class production Release125 passes120, with five
failed TESTs. All77 new controls now pass30 complete scenes+47 closed sources
with0 errors. All22 original Class sources draw and pass good geometry, but only
17 complete their required presentation mutations: five same-body/height218
sources exhaust the existing postdecode mutation allowance. Full-grid deep
copy alone needs218*240*sizeof(string)=1674240 object bytes, exceeding their
1350312..1419083 allowances before other work. These are real native resource
failures, not five complete passes or producer fallback. The first artifacts are
retained; a local row-mutation/restoration proposal is under review, with no
cap raise, ledger retry or skipped adversary.

A separate historical ER test still expects the exact typed-logLevel Class
source to fall back. Its complete Class graph is now a supported profile. A
reviewed one-hunk migration retains both original Class/State inputs, requiring
complete Class geometry and a semantic adversary while keeping the State
partner's whole-source fallback. That native-only first trial is pending.
All26 unrelated reports, including complete State85 at63, remain byte-exact
across the Class producer trial. Class481's289 old strong Models stay good;
27 originally unmodeled sources now draw without earning Model/admission credit.
No whole100, fullctest, Debug125, foreign-API/font/CSS/ELK graduation, commit,
push, install or real-terminal completion is claimed here.

Evidence: full-obligation-audit/root-state-horizontal-notes-r1-first-release-
conservation.json, root-class-envelope125-test-first-release-conservation.json,
root-class-envelope-producer-r1-first-release-comparison.json, and the separately
locked native/source review packages. Exact first logs, binaries, inputs and
all29 fresh reports are retained by the frozen execution manifests.


### ABI correction for the first Class Stage3 resource diagnosis

The preceding 32-byte-string arithmetic is superseded, not runtime evidence.
A root compiler diagnostic reused the actual Release native target flags and
module map: LLVM IR returns24 for both sizeof(std::string) and
sizeof(std::vector<std::string>). The implementation always uses sizeof; this
correction changes the diagnostic arithmetic, not the candidate code. The
original first failure logs and conditional32-byte audit remain preserved.

On that actual ABI, the old full-copy-plus-edit fee is1,416,700. The first
copy fee alone does not exceed every allowance. However, mandatory card
selection already prepays at least12,416, so these required operations alone
need at least1,429,116, above the maximum1,419,083 postdecode allowance; further
targeting/content/validation fees only add work. The proposed18-cell row
transaction costs3,348. It still requires actual execution under the unchanged
allowance; this arithmetic does not establish a passing source or a thrown PC.

Evidence: full-obligation-audit/root-native-abi-readonly-diagnostic/abi.ll
(SHA256 be20a3af468a096a53492639f617c6bb132fc65abd7d4a2a7332d50410fe8949)
and ACTUAL_IR_RESULT.json
(SHA256 5cac994c27c895b67891c45cdb19bedc69eb1dd3c490ae8e52b96f04d8648349).
The complete Relations Release125 ER-control migration subsequently passed
121/125; the only changed status is its historical Class-source expectation,
and all29 report dictionaries remain exact. Debug125 native has not run; the
Debug visual production-library build passed.


### Class Stage3 transaction: actual dual-preset checkpoint

The separately reviewed native-only row transaction is now exercised by the
complete Relations target: Release126 passes123 (10,958ms), Debug126 passes123
(149,429ms). The remaining three mandatory Class481/Endpoint29/State85 TESTs
are still red. This is not an all-green or overall coverage claim. All126
ordered statuses and all29 report bytes agree across presets.

All22 Stage3 original sources now complete good geometry plus their mandatory
body/presentation adversaries, with0 resource/protocol/geometry/observation
errors. Their complete wide/narrow/restored/ordinary frames, source Models,
presentation facts and original postdecode allowances equal the first failed
run. Only the Stage3 report changes; all28 other reports remain byte-exact.
The new restoration protocol TEST passes. The old five resource failures and
ABI correction remain separately retained. No source null/debt, foreign
predicate or whole-source admission is promoted by this private test fix.

Evidence: full-obligation-audit/root-stage3-row-transaction-r1-first-release-
conservation.json (04e1a42ddd8017f6185dba463c7baf7cbc2dcb921b0d6ffdc45348baae0e967b)
and root-stage3-row-transaction-r1-first-dual-parity.json
(e826af9abfca6aaec745232a6d33887ea95c080c434c2d9fecea2cd127d1c534).
Independent concrete reviewer: /root/corpus_oracle. Fullctest, real-terminal
final verification and overall100 remain incomplete; no commit/push/install.


### State local direction and anonymous-control identity: dual130 checkpoint

The actual complete Relations130 target passes127 in Release (11,772ms)
and Debug (152,236ms). All130 ordered runtime statuses and all31 complete
report bytes agree. The three mandatory Class481/Endpoint29/State85 aggregate
TESTs remain red; this is not overall suite completion.

Source-backed per-scope directions, paid horizontal facing-Note clearance and
same-owner/same-kind anonymous Start/Final reuse close all13 focused scoped
positive scenes plus all3 new shared-control scenes, with meaningful full
semantic/physical adversaries. The actual original State85 strong scenes grow
from63 to68: ordinals4,19,32,36,56. All old67 complete strong State rows before
the last reuse fix remain exact, as do the old12 focused strong rows and28
unrelated complete reports. Every original85 source,77 Model and8 null, hidden
choice policy, concurrency boundary and original foreign assertion remains
unchanged. Source56 now binds both incoming edges to the same final control;
separate owners remain distinct. Termaid occurrence-count behavior is not
claimed equivalent to the reviewed non-concurrent Mermaid/Merman scope rule.

Frozen current producer is764d16049672b9866542bebdf7665fbebc834b829d975b0953c18e9278720848;
native130 isa5dc704e7a006ebe527c97fc101a9fb03b3d59ef1fc2c961d43a5e10bfd86dd1.
Evidence: full-obligation-audit/root-anonymous-control-producer-r1-first-release-
conservation.json (e4797536983eceb116ad4d894a29a204a19161f5584a81595e8e0de984a22a02)
and root-anonymous-control-producer-r1-dual130-checkpoint.json
(712b97aae77bc67430b7ba9a2964e31fbf1a25b4028730174cd561d01f4f2617).
Independent source/control/concrete reviewer: /root/corpus_oracle. Complete
Debug/Release native evidence supersedes the earlier visual-library-only
checkpoint; fullctest/final real-terminal proof remain pending. No commit,
push, install, original corpus admission or public renderer API change.


### Class generic operands and State description test-first checkpoint

The complete Relations139 target passes133 in Release (12,013ms) and Debug
(162,115ms). All139 ordered statuses and all36 complete report bytes agree.
These are native TEST totals, not whole-corpus drawing coverage. Class generic
operands now preserve first-definition types and base identities:12/13 original
focused scenes and all7 positive partners pass whole geometry and meaningful
mutants. The remaining original scene has a genuine namespace/leaf rank
contradiction. All11 positive branch controls,5 separated-suffix whole-source
boundaries,2 CRLF positives and5 bare-CR boundaries pass their own contracts.
No boundary fallback earns drawing credit. The original481 sources/Models,
including192 null obligations, remain unchanged; all289 old complete strong
Class rows and68 State scenes remain strong. Twenty-three additional original
null sources now draw but remain uncredited pending complete source Models.

State description papers initially exposed test-only defects: two hand-authored
papers had four wrong boundary-arm glyphs, and the private wide-cell matcher
did not consume FTXUI's required empty continuation cell. Versioned corrections
retain all source Models and all30 physical adversaries. All5 good papers and
their semantic/physical adversaries now pass; all10 mandatory public positives
still fall back before the State producer fix. Failed first logs remain intact.

Native140 adds27 source-owned State description controls. The first build
failed because a mutable JSON value was passed to an immutable canonicalizer.
A separately reviewed local test correction serializes and parses the complete
mutated row before hashing its immutable Model; no shared JSON API changed.
The actual corrected Release run passes134/140 (12,503ms):27 observations,
9 complete Models,2 strong scenes and18 whole-source boundaries, with zero
oracle or observation errors. All36 older reports remain byte-exact. The one
older status change closes a stale Class annotation-first fallback expectation:
the exact historical source is retained and now requires its complete Model
plus semantic adversaries, following the pinned grammar/DB addClass ordering.

Current producer is3f10f24a58478e1e6397d8389cbcd2d6adc79ea2157cd29e63e172fa972b72a9;
native140 is7b4e2bae2ae8dbabbfcd0498414ec79fcef23935da052149e3c2c4d43d69f1de.
Evidence: full-obligation-audit/root-class-generic-producer-r2-native139-dual-
conservation.json (a6c1fcca696d9210064232eeb9c9b8745ede4338e80115dbec5ce109edae2900)
and root-description27-mutable-r1-native140-first-release-conservation.json
(ea7c648a346bdc489e00dc9177e1ff0e0e1839ceb13bac8203944de3d326f41e).
Independent concrete reviewers: /root/terminal_union and /root/corpus_oracle.
All six remaining aggregate/focused failures stay mandatory. No complete100,
fullctest, Debug140, final terminal screenshot, foreign assertion admission,
commit, push or installation is claimed.


### State descriptions and compound Class leaf ranks: actual dual141 checkpoint

The complete Relations141 target passes137 in Release (12,462ms) and Debug
(160,142ms). All141 ordered statuses and all38 complete report bytes agree.
The four remaining mandatory TESTs are Class481, Endpoint29, State85 and the
complete State description cohort containing original50's block Note. This
checkpoint is not full-suite or whole-corpus100 completion.

The State producer now owns first-authored titles and ordered literal body
rows in a private StateTextRecord. Body rows measure and draw in the existing
rounded card, with one fully attached divider. Its reviewed compact-alias
quadratic decoder tariff is paid before the new decoder path; original shared
label helpers and caps remain unchanged. Description branch controls pass9/9
complete Models plus meaningful mutants and the observable scoped-direction
contrast; all18 source boundaries retain whole text. The original focused
cohort passes9/10 complete Models, leaving the entire block-Note source50
mandatory. Original State47 becomes strong, raising the old cohort68 to69;
all old68 complete strong rows remain byte-exact. Sources20/49 now draw but
their original null lineage is uncredited. Only three State reports changed;
the other35 complete reports remained exact in that first isolated trial.

The c567 Class source has a globally acyclic leaf graph but a cyclic namespace
quotient. Ordering indivisible whole namespaces horizontally necessarily
reverses a leaf edge. The local private fix measures existing complete cards,
uses global longest-path columns, and gives flat sibling namespaces disjoint
perpendicular bands. It validates the entire candidate before committing,
mirrors RL once, and retains the existing global leaf-rank checker, routing,
markers and limits. Source-ineligible scenes keep the former branch. No
failed-ledger retry or fixture-dependent production path is introduced.

All13 complete original generic scenes and all7 positive partners now pass
whole geometry and semantic adversaries. All6 independent compound controls
pass, and reversing each complete Model's direction reaches the actual rank
diagnostic. Their real first-red run had six whole-geometry failures and zero
oracle/resource errors; the source fix changes both focused TESTs to green.
All289 previously strong Class rows remain byte-exact; only original null
row392 changes its frame. All69 State rows and both description reports remain
exact across the Class trial. The original481 source facts,192 historical
nulls and foreign predicates are not promoted by focused drawing success.

Current private producer is58935b57ff9bedfddd0575ea7e6cfcbf08cb8b5986226d85d08d2e10974f7c15;
native141 is25542f83dfdc0e67740f3bdb9333c8192d32e95439fd23b50ef64cd1ddafe6e5.
Evidence: full-obligation-audit/root-state-description-producer-r1-native141-
first-release-conservation.json
(76fb043863f483009101e785081abb9046dd081535595e8a5a9a8b10a43bc937),
root-class-compound-producer-r1-native141-first-release-conservation.json
(d67e5e8d7a09d650515b0fa33be20e34679d4cd5daa58cfd5b5b4a1262a8a26d),
and root-class-compound-producer-r1-native141-dual-conservation.json
(1b961f7090fde4f30ebc8ec6c0f6611134da85f2e3b7ffa2fb0e207ffaea8a5e).
Independent concrete reviewers: /root/corpus_oracle and /root/terminal_union.
No shared RelationEntity, public renderer API, source fixture history or
resource cap changed. Fullctest, final real-terminal proof and overall100
remain open. No commit, push or installation.


### State block Notes and whole-Class admission: actual dual143 checkpoint

The complete Relations143 target passes140 in Release (12,157ms) and Debug
(164,418ms). All143 ordered statuses and all39 complete report bytes agree.
Three aggregate requirements remain red: Class481, Endpoint29 and State85.
No TEST-total percentage represents complete drawing coverage.

The private State parser now consumes whole multiline Note blocks, preserves
literal body lines and the following edge/scope boundary, and rejects ambiguous
duplicate Notes as explicit whole-source boundaries. Seven complete positive
controls and nine boundary controls pass their contracts. Original State31 and50
become strong, raising State85 from69 to71 strong scenes. The scoped-direction
legacy control for a block Note initially failed because it still required a
fallback. A versioned fixture retains the complete source and now requires the
whole Note Model and observable inverse scoped-rank adversary; all other19
report rows remain exact. The genuine first-red run is preserved.

Thirteen source-owned generic Models are now joined to exact historical Class
null tuples. All481 original sources remain mandatory:302 complete Models pass
whole geometry and meaningful mutants;179 remaining Models stay explicitly
pending. The old289 complete rows remain unchanged. Endpoint29 still has25
strong scenes and four mandatory whole-source fallbacks. Production/native
limits, shared cell types and public renderer contracts are unchanged.

Fresh Release public regression results are Markdown44/44, Flow23/23,
Charts120/121, Terminal3/3 and Official18/18. The known complete Merman XY102
requirement remains red. All2916 terminal sources and2738 official observations
were exercised. Official original whole-source drawing remains195/2145
(9.090909%):1950 retain source fallback and zero are observation errors.
Diagnostic extracted-body results are separate and earn no whole-source credit.
The18 official TESTs passing means matrix integrity, not complete drawing.

Evidence: full-obligation-audit/root-state-block-note-producer-r1-native143-
first-release-conservation.json
(db2e98a8f41759436d3f5830b31cec76a4af5e852e236081e2effb93f96b7b77),
root-state-scoped-note-v2-native143-dual-conservation.json
(79715380bda7f115bf0edf1ad337d6c1d344cddf6c4691bdddecaf19a1c7f6d5),
and root-note711-native143-public-official-conservation.json
(187d94022a104e8648e44cc9c94de711b1abba9f9c7a5b5e30df62c3d4d51340).
Producer7110223798b7aee8a4554727289cadfb2bad974d314f8c31485425642901f927;
nativeb50896e2c66dfedea330c1fb6aa983018ed757b374419be5c449305a7a8484b6.
Independent reviewers: /root/corpus_oracle and /root/terminal_union.
Full-suite100, fullctest and final real-terminal screenshots remain open.
No commit, push or installation.


### Endpoint29 admission and budget closure: actual dual146 checkpoint

Relations146 passes142 in Release (12,692ms) and Debug (165,485ms). All146
ordered statuses and all41 full report bytes agree. The four red aggregate
requirements remain Class481, Endpoint29, State85 and the new Caption source
controls; none is waived. Class481 observes all481 whole sources, decodes331
complete Models and verifies327 complete scenes plus meaningful adversaries.
The remaining150 Model obligations and four positive drawing fallbacks stay
mandatory. All302 formerly strong whole Class dictionaries and all481 full
captured frames remain unchanged across admission and budget fixes. State85
retains71 strong scenes.

The admission first run exposed a real native oracle exhaustion for the large
Angle source: only141,289 Work remained before wide geometry. Existing full-cell
paid equality now transports the wide sentinel/hash to an identical independently
captured restored Grid, avoiding real repeated scans while retaining all five
captures and three retained frames. This enabled actual wide and narrow geometry,
but the first cache run still exhausted at mutant entry. A second local change
reuses the already successfully canonicalized immutable Angle Model's actual
length, guarded by exact JsonVal identity, in the strict codec. Every schema check,
original decoded-copy tariff, mutation and8M cap remains; cached/ordinary complete
Models and wrong-identity/length/resource controls pass. The final real run has
zero oracle/resource/geometry/observation errors and completes all Angle mutants.
New small default lifecycle fees are explicit:14 nonmain Class reports change
only24/40 Work metadata;26 reports remain byte-exact. First errors are preserved.

The test-first Caption gate has9 complete independent good papers,45 real-cell
negative controls and one complete overwritten-caption counterframe. All pass;
6/8 complete public sources pass, with2 mandatory fallbacks. A controlled private
strip producer trial gained one new control but no original four sources and
lost a previously strong whole source. It was rejected and its17 edits exactly
reverted to711022. The complete failed trial is retained. A source-only necessary
counterexample shows a required exterior body anchor outside a29-column Canvas;
this is not an observed first program counter for the other four failures.

A read-only real Debug observation of the original279-byte Chinese b91 State
source confirms initialized parse success and an actual cross-owner comparison
at edge8: source owner2 to root Final owner-1. The first eight edges are accepted.
Work remains explicitly unobservable in that debugger run. No route success or
budget cause is inferred. The unchanged complete85-source report is byte-exact.
A source-local compound-scope proposal and additional test-first whole-source
controls remain under review; no production installation is claimed here.

The official DOM/run-prefix maintainer tool now passes all22 first synthetic
protocol/source controls:11 complete ClassDB captures and11 correctly incomplete
outcomes. It uses real inert DOM/DOMPurify and stock run transformations, not a
rendering backend. Complete285 execution, source Model authoring/admission and
original browser/font/API/SVG assertions remain separate gates. These controls
do not increase official original drawing coverage; its last fresh whole-source
result remains195/2145 (9.090909%).

Evidence: full-obligation-audit/root-endpoint29-caption-native146-first-release-
conservation.json (42b268ec4529ec93c442efb6451634c02e77127df5608dd8329fead89592b5de),
root-caption-strip-producer-r1-native146-failed-trial.json
(045e2a263094c9ab769c5a6a60cc9708d78f6422bc172d8a0d8252195e2afd4b),
root-cjk-b91-current-debug-readonly-first-failure.json
(4ddcb0aecbdfd58a1da64fbe7efce9686e4c65b5771d70ce8e39b38c48fec522),
root-endpoint29-native146-canonical-reuse-first-release-conservation.json
(522858fb8596d44ec3de802107e7a317e70759100a772e7576d21a1aa424f36a),
and root-endpoint29-native146-canonical-reuse-dual-conservation.json
(c4f37706e7f194daa80252cb31f6b9085cff2817a618449ac1ba75096766d412).
Producer7110223798b7aee8a4554727289cadfb2bad974d314f8c31485425642901f927;
native1138aad12c91408f09a049ea19c9363680ac6a446409bba3f706945dae124022.
Independent contributors/reviewers: /root/corpus_oracle, /root/corpus_review and
/root/terminal_union. No shared module/public renderer contract, original fixture
history or cap changed. Overall100, fullctest and final real-terminal screenshots
remain open. No commit, push or installation.


### State cross-scope routing: first complete148 Release trial

The unchanged-producer test-first run passes143/148. Its independent complete
literal crossing and three physical/two schema-valid Model counterfactuals pass;
three of four whole public source controls remain drawing fallbacks. All41 older
reports remain byte-exact. This genuine first-red is retained.

The private producer now uses the existing paid lowest-common-ancestor scope
projection and structural namespace transit rules for State edges with different
owners. Group header protection and physical occupancy remain authoritative.
Class-specific presentation and reciprocal policies, source/Model facts, resource
limits and public contracts are unchanged. The first complete Release trial
passes144/148 (13,129ms): all four full cross-scope controls pass wide/drawn-narrow
geometry and meaningful mutants. All42 other full reports remain byte-exact,
including all327 strong Class and71 strong State scenes. The original Chinese
b91 source still falls back after the former owner barrier; its next failure is
unobserved and remains mandatory. Debug148 verification is pending.

Evidence: full-obligation-audit/root-state-cross-scope-native148-test-first-
conservation.json and root-state-cross-scope-producer-native148-first-release-
conservation.json. Producer4cebb899e7bd4307221014a182bdcd74576fbb9b4dec41d08b87c1adfc8e7a0a;
native5eaa7531e45e06139abfa03da3b907b411cfad5cb1e8a31bea4f7644f2a6e5b7.
Independent producer/native reviewer: /root/corpus_review. No shared module or
public contract changed; no complete-suite100, commit, push or installation.

### Official Class DOM/run observations: first complete285 batch

All285 original official Class applications completed one first ClassDB capture
under the reviewed stock DOM/run prefix. All227 former raw-API captures and all58
former raw-API exceptions are represented; no source or path was dropped.
Root verified all1,140 job/record/log hashes and all single parse/entry/observer,
controlled stop, queue-idle, temporary cleanup and owned-DOM-close conditions.
Full independent graph/source/runtime audit remains pending. These are pre-render
data observations, not native drawing, full viewer/browser/font success, generated
Models or foreign assertion admission. Official whole-source drawing credit
therefore remains at the last fresh195/2145 measurement until a new actual matrix.

Evidence: official-class285-frontmatter-discovery-root/dom-run-render-prefix-
concrete-corpus/control-domain-r1/root-workspace/runs/applications-first/SUMMARY.json,
SHA2565ae501b91ac9feaebba60530545fce225c14476ea8db25502000c95b2893bdbd,
and root-workspace/ROOT_APPLICATIONS_FIRST_AUDIT.json.


The State cross-scope Debug148 run subsequently passes144/148 (170,059ms).
All148 ordered statuses and all43 complete report bytes exactly match Release.
Evidence: full-obligation-audit/root-state-cross-scope-producer-native148-dual-
conservation.json. The independent285 actual-source review also closes with
zero material findings: all855 typed graphs,152,051 nodes and204,997 references
are structurally complete. It confirms all285 original source applications and
all58 earlier raw-API error identities; this authorizes separate Model design,
not native rendering or compatibility credit. Review:
control-domain-r1/independent-applications-first-review-corpus/
independent-applications-first-review.json,
SHA256a2c9e97ffc8d2131d969b1eb61109da69aeba72b2f7e6ca3bef36be262526931.


### Class folded documentation: first complete150 Release trial

The source-authored test-first run passes145/150: the base complete Class scene
passes geometry and meaningful mutants, all10 folded-documentation positives
remain genuine source fallbacks, and all14 complete source boundaries pass.
All43 preexisting full reports remain exact, with zero geometry/oracle/resource/
observation errors in the new source controls. Independent review found a source
P1 in the first scratch candidate: a leading all-space scalar line can establish
an indentation minimum that the first nonblank content may not undercut. R1
remains frozen; R2 adds the paid leading-indent maximum/guard and one whole-source
counterexample while preserving all original24 scratch source dictionaries.

The private Class YAML reader now consumes only complete safe `>`/`>-` folded
documentation, retains actual root indentation/fences/siblings, and keeps all
business body and init text. It adds no visible description, dagre/namespace
policy, shared helper or public API. The first complete150 producer trial
passes146/150 (12,276ms): all11 complete positives pass wide/drawn-narrow geometry
and meaningful mutants; all14 boundaries remain whole source. All44 other full
reports remain byte-exact, including327 Class,71 State and4 cross-scope scenes.
The original four aggregate failures stay mandatory. Debug150, fresh official
whole-source matrix and285 complete source-Model admission are separate open gates.

Evidence: full-obligation-audit/root-class-description-r2-native150-test-first-
conservation.json and root-class-description-r2-producer-native150-first-release-
conservation.json. Producer7085f3d61ccb06335e2880a53df6c36c2f8e12cda60a54d0429de69fab30d15d;
nativef2dcb5019e1dc9fc8d9970f73e8d9791ad742854bdce1ecf12411e1266448165.
Independent reviewer: /root/terminal_union, concrete9a8dd12efd67cbaea62a3344b120983cacafe5d00bd66dd75d11939f42f258bc.
No shared module/public contract, original upstream source/policy, cap, commit,
push or installation changed.

### Chinese b91 next-phase read-only observation

The first fresh Debug148 observation of the exact279-byte original b91 source
records45 bounded events and exits the unchanged complete85 TEST normally with
its genuine mandatory failure. The entire85-source report is byte-exact. Actual
compound scope is true; the Group measures34x60 and the root70x126; all9 entities
and Group header protection complete. Actual connector order2,4,3,0,1,8,5 succeeds,
then edge6 (`重连中` to `连接中`, label `成功`) returns false. This closes the old
prepare owner barrier and locates a later connector failure. It is not a no-route
theorem or a budget diagnosis: actual Work fields remain explicitly unobservable.
All post-run source/binary/object/observer pins match; no drawing credit follows.
Evidence: full-obligation-audit/root-cjk-b91-cross-scope-next-phase-readonly-first-
failure.json and cjk-b91-debug148-next-phase-root-first/{EVENTS,SUMMARY}.json.
That Debug context is historical after the native150 source append and must not
be silently reused for a later build.


### Rejected Caption R2 and unreviewed official Class surfaces

The complete Caption R2 trial passes146/150 while retaining327 strong Class
scenes, and restores the f014 source that rejectedR1 had lost. Its focused controls
improve6/8 to7/8, but none of the original four mandatory positive fallbacks gains
whole geometry. Original pending-Model source `merman/1627e83290ef295ed4e4` loses
its prior drawing and becomes source fallback. All42 unrelated full reports remain
exact. Root rejects the whole R2 strip machinery for that corpus regression and
lack of original-four benefit;19 unique edits are selectively reversed to exact
7085, preserving State/description changes. No more strip design is inferred from
this failed trial. The restored complete Release150 run has all150 statuses and
all45 full reports byte-exact to the accepted7085 run. Debug150 remains running.
Evidence: root-caption-r2-producer-native150-rejected-trial.json and
class-description7085-native150-restored-dual-release-hashes.json under
full-obligation-audit. Failed first artifacts remain frozen.

The first fresh complete official18 run with description7085 passes17/18:
CompleteCapabilityMatrix genuinely fails its existing source-fallback contract
for84 original Class applications at wide/restored width (65 at narrow width).
Every original2145 source and all2738 observations execute. These newly changed
surfaces are unreviewed drawing candidates, not admitted whole diagrams. The
unchanged matrix therefore credits195/2145 drawings, records1866 source fallbacks
and84 errors at width240. Original policy/manifest assertions are not weakened or
updated from observed output. All285 source Models, meaningful geometry and
original predicate mapping remain prerequisites for any eventual admission.
Evidence: root-class-description7085-official-first-unreviewed-surface-failures.json.
No suite100, fullctest, final real-terminal screenshots, commit, push or install.


The restored description7085 native150 Debug run subsequently passes146/150
(167,978ms), matching restored Release146/150 (12,963ms). All150 ordered statuses
and all45 full report bytes are exact across presets, including all four genuine
red aggregates. Evidence: root-class-description7085-native150-restored-dual-
conservation.json, SHA256ea95e6ffb6e5e92c4e647f74b2a58c8294eb65fb33455dfe78533897d84dc7c8.
The official84 changed-surface source-contract failures remain unreviewed and
uncredited. No completion, original-policy weakening, commit, push or install.


### Complete Class285 mapping and current budget-branch diagnostic

The independently reconstructed complete285 official Class mapping has152 whole
terminal Models and133 whole-null obligations. All original285 applications,281
distinct complete sources,1442 classes,1193 members,856 business relations,64 raw
namespaces and38 Notes remain bound. Whole-null rows retain all witnesses and
foreign predicates. This approves native test-first authorship only, not source
admission or drawing coverage. Reviewer: /root/terminal_union; report457b198c64aca2ad8f65c648e726bb9d074427fe12b234cae68686a1639c4793.

A fresh Debug150 diagnostic then executes the unchanged complete State85 TEST
once, with the actual branch address verified from current arm64 instructions.
It observes the exact279-byte original b91 source entry once, no insufficient-
budget true-block event, no observer errors, and all88 postpins unchanged. The
native exits normally with its genuine mandatory failure; its entire85-source
report remains byte-exact (ef83cda321357b1e164aaeaf8d64a062c7172d7f92c1e917e889640e06bfc830).
Zero budget events leaves the cause unproven; it is neither a budget diagnosis
nor a mathematical no-route or budget-sufficiency proof. The static route audit
also corrects the earlier prose ordering: actual edges2,4,3,0,1,8,5 succeed before
edge6 fails. The old frozen evidence is preserved. No cap/order/margin change is
justified by that observation alone. Evidence: full-obligation-audit/root-cjk-
b91-debug150-budget-branch-actual-r2.json, SHA717df3ef2e5c1f0530b01a8c5604c7b5a317c1b1f770fdc24ef0e67752abe7e5.

The source, tests and unrelated PTY shutdown work remain unchanged during this
diagnostic. No shared module or public contract, original policy, commit, push,
installation or Home configuration changed. Full-suite100% remains open.


### Finite Class dagre selector: genuine first-red and controlled producer

The unchanged7085 producer first runs all152 Relations TESTs:147 pass and five
aggregates fail. The new eight-positive source-authored selector control has two
strong known controls and six mandatory dagre fallbacks; all nine closed profiles
retain whole source. All original150 ordered statuses and45 full reports remain
byte-exact. The frozen first-red is retained in root-class-dagre-native152-test-
first-conservation.json (dbbafe4e2c103489ea313ffdd7b9c732437524c32783ced5701464237f19bb49).

The independently reviewed one-expression private Class allowlist trial then
accepts root layout dagre alongside elk using the existing own terminal layout.
Complete Release152 passes148/152 (12,545ms). All eight full positives pass complete
wide/drawn-narrow geometry and meaningful mutants; all nine boundaries remain
whole source, with zero geometry/oracle/resource/observation errors. The original
150 statuses,45 old full reports and the boundary report stay byte-exact; only
the new positive report changes. The original four aggregate failures remain
mandatory. No source/schema/Model/original policy, external renderer, route/cap,
shared helper or exported contract changed. Debug and public regressions remain
open at this point; no official285 admission or full-suite100% is claimed.

Owner: mermaid_relations_impl.cpp Class metadata expression; native source
controls and their9066-byte asset. Independent reviewer /root/terminal_union,
reporteb167c99344528869d1a3c4250ccd744c13a1ea49850de4d2a7168de343adc5d.
Producerc2820804224979458c8163b2bf82001740d459b664f03176131fbb5963ec79be;
nativea3667d78469633793653f32ddb57733fe3713227063799a8b2e6cec62fd4535f.
Evidence: full-obligation-audit/root-class-dagre-producer-native152-first-release-
conservation.json, SHA6af8d00c9a07811a9c2aa6572b10bac68ade32dfed87b18fa29ee48ad704142a.
No commit, push, installation or Home write.


### Class selector dual parity and genuine official285 first native execution

The complete152 Debug/Release selector runs each pass148/152, with all152 ordered
statuses and47 full report bytes equal. The original four completion aggregates
remain red. Proof: root-class-dagre-producer-native152-first-dual-conservation.json,
SHAe48c6666521ebade11499a8e74a44f148afc3517e1dbc0575d51a56cdf6fc5f4.

The separately source-reviewed285 fixture is then installed with two TESTs and
its unchanged1,622,120-byte candidate plus74,234-byte approval sidecar. First
complete Release154 passes149/154 (13,450ms), retaining all152 prior ordered
statuses and47 complete reports exactly. All285 complete original sources have
five public captures before their own bind/decode; all152 complete Models decode.
The new report records61 whole geometry/mutation successes,72 mandatory positive
fallbacks,10 geometry errors,13 harness/resource errors and zero observation
errors. Four historically null sources complete captures but exhaust the
publication fee before binding, so the pending counter129 plus those four errors
is the original133 unresolved mappings; no four-case mapping gain is claimed.
Document/manifest scope remainders are2,844,991 and3,794,319. The complete original
source/Model/debt denominator and pending==0/matched==285 gates remain mandatory.
Proof: root-official-class285-native154-test-first-conservation.json,
SHAb4a0fc896ea72ffbbe1dc6f9b87745bb1089914ef09ae53b5cbbc33f63eae88c.

Source inspection localizes six geometry mismatches to unconverted member
generic tildes. Four further geometry mismatches are Class multiline headers
whose actual authored leading SPACE is lost by the test Content projection.
The13 harness errors include ambiguous first-card selection, repeated namespace
content scans, wrong-scope Grid copy charging and per-cell JSON metadata fees for
packed row strings. These diagnoses require bounded reviewed corrections, not
source/Model normalization or a larger budget. All first-red evidence remains.

The complete public c282 Release replay passes Markdown44/44, Flow23/23,
Charts120/121 with unchanged XY102 failure, Terminal3/3 over2916 original cases,
and Official17/18 over2145 originals plus593 diagnostic bodies. The first four
suites keep their earlier ordered statuses;12 of13 full reports stay byte-exact.
Only the official matrix changes, preserving its genuine source-contract failure
for newly drawn but unadmitted Class surfaces. All original fixture bytes remain.
Proof: root-class-dagre-c282-native154-public-conservation.json,
SHAb01907d1df3250ecfd03a16d8326cf9ce75be1ef081ae3a3af1447ee4a90390c.

No shared production module, exported API, settings, external renderer, cap,
original vendor source/policy, commit, push, installation or Home file changed.
The native Class285 test and source-mapping fixtures are the only new test owner.
Complete upstream100%, fullctest and final real-terminal screenshots remain open.
