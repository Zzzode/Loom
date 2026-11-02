# Mermaid rendering coverage improvement plan

**Status:** In progress. The P0 Class routing checkpoint is complete; broader
coverage work remains open. This plan does not advance an RFC phase gate and
coordinates the existing
[RFC 0030](../rfcs/0030-terminal-mermaid-diagrams.md),
[RFC 0031](../rfcs/0031-mermaid-envelope-and-renderer-cleanup.md), and the
[obligation ledger](mermaid-obligation-ledger.md). A proposed cohort must use
an already approved applicable RFC design or complete the required RFC design
and production-readiness gates before production feature implementation.
Bug fixes and small refactors follow the repository's normal rules. RFC and
code reviews are agent-run.

## Current complete backlog

The [prioritized implementation backlog](mermaid-implementation-backlog.md)
is the execution plan for all **1,482 unsuccessful originals**, not just the
Flow57 pair. Its [complete case appendix](mermaid-unsuccessful-cases.md) lists
every failed file once; [the JSON inventory](mermaid-failure-inventory.json)
retains source hashes, diagnostics and assignments. The disjoint assignments
are P0 0 errors, P1 499 existing Flow/Class/ER fallbacks, P2 447 other admitted
family fallbacks, and P3 536 additional-family/category fallbacks. Cross-cutting
configuration and label work overlaps these assignments and adds no denominator.

Current Class defined-style6 checkpoint (2026-10-30): coverage
**663/2,145 (30.91%)**,1482 fallbacks, zero errors; Class260/285 (91.23%).
Debug/Release889/889 and serial style/resource/real-PTY25/25 each pass.
[Executed evidence](mermaid-class-defined-style6-review.md). P0 remains complete.

Class4 additionally fixes phantom entities from accessibility declarations; see
[Class4 evidence](mermaid-class4-review.md). RequirementMarkdown1 and Flow grouped7 are also complete; P0 is complete; next is the Flow style4 batch.

## Historical plan and execution records

The sections below preserve earlier plans and dated execution evidence. Their
"current", "next", "immediate" and "remaining" counts refer to those historical
checkpoints. The active assignment and latest accepted coverage are in
Current complete backlog above and the linked implementation backlog.

## Objective and baseline

Increase the number of complete, pinned official Mermaid originals that render
faithfully in Loom's terminal. Preserve unsupported source rather than silently
losing semantics. The primary metric is **originals drawn / all 2,145 pinned
originals at 240 columns**: a fallback or matrix error earns zero. Per-family
rates and error and fallback counts are reported alongside the overall rate.
The denominator is fixed to the current Mermaid 12.2.0 corpus; any upstream
repin gets a separate baseline and is not presented as a gain against it.

The historical 2026-10-29 checkpoint draws **622/2,145 (29.00%)** originals;
Class draws **237/285 (83.16%)**. Both source fallbacks and errors count as
failures. Debug and Release each passed 830 Mermaid tests; the full project
build and CTest were not run as part of this checkpoint.

The local instrumented Debug baseline on 2026-10-27 found **596/2,145
(27.79%)** drawn, 1,501 fallbacks and 48 matrix errors. Sequence drew 193/193,
Class 211/285, GitGraph 55/135, ER 62/206, Flow 54/400, Requirement 11/172,
Pie 7/24, XY 2/48 and stateDiagram-v2 1/103. Other corpus categories had no
credited drawings. Full per-family counts and the 48 error paths are in the
[gap analysis](mermaid-gap-analysis.md). These figures are a single local
checkpoint, not a Debug/Release graduation result. The ledger contains older
checkpoints and estimates; measure a fresh baseline for each work package.

The 821 Mermaid-labeled CTest tests produced 820 passes and one timeout at
180 seconds in that run; the timed-out test passed directly in 218 seconds.
A capability-report test can pass while reporting known gaps. Line/branch
coverage, input availability, fallback checks, body-only diagnostics and
passing CTest counts are separate measures and never drawing credit. The
matrix's `frontmatter` label and whole-scene labels are not per-file root-cause
proof. The independently audited terminal-suite obligations are a separate
workstream; do not equate official-file drawing with complete upstream test or
browser/SVG conformance.

## Outcome rules

- Count a source as drawn only after its **complete original**, including
  meaningful metadata, passes its independently source-bound terminal
  semantics and geometry checks. A visually plausible but unreviewed picture
  remains unadmitted. Empty diagrams require their own reviewed semantics and
  must not be credited merely for producing no ink.
- Each selected cohort is an immutable list of paths, source hashes, expected
  semantics and explicit baseline outcomes, with independent negative and
  mutation controls. Declare the selection and oracle before implementation;
  do not derive expected results from the renderer. A case with multiple
  blockers stays pending until all relevant semantics are met.
- Preserve complete-source fallback for unsupported syntax, metadata,
  ambiguous semantics, resource exhaustion and insufficient width. Existing
  work, canvas and dimension limits are not relaxed to manufacture passes.
  Keep 240 → 40 → 240 lifecycle, following text, streaming and source-copy
  behavior verified where applicable.
- A reviewed change to observed capability must update the applicable source
  oracle, frozen counts, gap roster and ledger together. Never delete or
  weaken an assertion merely to make the report green. Keep historical
  outcomes and record every admission, changed fallback and new error.
- For every cohort report before/after drawn, fallback and error counts for
  the cohort, family and full corpus, plus regressions and other outcome
  changes. A zero-error target applies to the relevant report classification;
  a known error is not silently reclassified as a success.

## Ordered work packages

Each package is proposed. Counts below are the snapshot baseline or a
measurement target, not a claim that all rows have the same cause or can be
fixed by one change. Work can proceed in independently reviewed cohorts after
the baseline and source-oracle prerequisites are met.

### P0 — Make the baseline repeatable and classify the gaps

1. Rebuild and run the Mermaid label in Debug and Release, producing fresh
   240/40/240 matrix and capability reports. Investigate the slow official
   mutation test: capture its runtime without instrumentation and with it,
   then make the normal test signal repeatable without weakening assertions.
   Record whether any platform-specific flake is reproduced; do not count a
   timeout as a pass.
2. Bind all 48 baseline error paths to the exact reported diagnostic and
   distinguish confirmed source fallback, unreviewed art, restore/protocol
   mismatch, and any actual runtime failure. The existing messages include
   alternatives, so do not assign a unique cause without evidence.
3. Inventory all 1,501 fallbacks with source hashes and evidence for the first
   confirmed blocker: metadata/configuration, body grammar/family, geometry,
   fixed resource/width limit, or missing oracle. Record overlapping blockers
   separately and mark unknown when unproven. In particular, do not infer that
   the 970 `frontmatter` labels have one shared root cause.

**Completion evidence:** both presets have archived reports and test logs;
all 48 errors have a source-linked disposition; every fallback has a recorded
first confirmed blocker or explicit unknown, with a reproducible witness and
owner for each selected follow-up cohort. The original 2,145 paths and hashes
remain intact. Baseline measurement alone earns no new drawing credit.

#### P0 work in progress (2026-10-28)

At HEAD `7d69a1178b7d36ff2332470fec97d340863db504` on macOS with LLVM
23.1.3, separate, non-instrumented Debug and Release builds ran the 821
`mermaid`-labeled CTest tests. The working tree also contained unrelated local
changes; this is not a clean-checkout or full-repository test claim. Release
passed 821/821. Debug passed 820/821, with
`MermaidOfficial.RoleBodyBatchParsedMutationsReachTheirSpecificTupleAndSchemaGuards`
timing out at 180 seconds. Its direct, untimed rerun is recorded separately;
it passed in 208.60 seconds, compared with 41.86 seconds in the Release CTest
run. Do not treat that retry as a green Debug CTest run. Both official matrix JSON files are
byte-identical (SHA-256
`07b08446ff905bb6907353cd65d31533dbf1ad25f3b20f7db521cd1517ac0cb4`) and
retain 596 drawn, 1,501 fallbacks and 48 errors. The local build logs and
reports are under `build/mermaid-baseline-debug/` and
`build/mermaid-baseline-release/`; those ignored artifacts may be regenerated.

`tools/mermaid/inventory_matrix_gaps.py` verifies the matrix's pinned corpus,
checks each source hash against both the manifest and matrix, and emits an
inventory containing each gap's three-width observations, source identity,
report messages and available family observations. Example:

```bash
python3 tools/mermaid/inventory_matrix_gaps.py \
  build/mermaid-baseline-release/tests/ui/mermaid-official-matrix.json \
  build/mermaid-baseline-release/gap-inventory.json
```

The 48 errors comprise 26 mapped Class positives reporting ordinary source,
14 Class/Flow rows with an unreviewed-art-or-protocol diagnostic, six ER rows
with a whole-null art/ordinary/restore diagnostic, and two expected-source
protocol mismatches. For all 26 mapped Class positives, the report records
wide output equal to ordinary source, matching restored cells, and no verifier
resource exhaustion or exception. These are observations, not the underlying
parser/layout root cause. The new inventory explicitly initializes every
blocker as unknown; it has not diagnosed all 1,501 fallbacks or adjudicated
the 48 errors. P0 remains in progress and these counts do not advance P1.

#### P0 mutation-test timeout repair (2026-10-28)

The former single RoleBodyBatch mutation test took 208.60 seconds in an
isolated non-instrumented Debug run, above its 180-second CTest limit. A
buffered-read and manifest-index trial still took 175.25 seconds and left too
little margin; it was discarded. The existing mutation checks now run in seven
independent parameterized cohorts: one general, one half-arrow and five
configuration batches. The 49 configuration sources remain exhaustive, guarded
by a compile-time inventory assertion; no mutation or diagnostic assertion was
removed. Each cohort retains the `mermaid` label and original 180-second
timeout. Independent agent `agent:/root/p0_review` checked the partition and
approved it, including follow-up evidence review.

On the same local host, Debug and Release each passed **827/827** Mermaid
CTest entries (`-j8`); the former single entry became seven, accounting for the
six-entry increase. The slowest Debug cohort took 41.93 seconds within the full
run. The two resulting official matrices were byte-identical to the baseline
matrix, and `verify_e6c_gap_roster.py` passed for both build directories. This
repairs the observed test timeout without changing renderer behavior, the
timeout limit, or the 596/2,145 drawing result. These local full-label runs are
not a full repository CTest or Linux graduation claim.

#### P0 Class26 runtime diagnosis (2026-10-28)

With temporary diagnostic logging in the existing production functions, each
of the 26 complete pinned sources was passed directly to `render_mermaid` on
the non-coverage-instrumented Debug build. The production sources and temporary
test were restored after the observation. The source hashes, per-source
observations and normalized-trace hash are recorded in
[`mermaid-class26-diagnostics.json`](mermaid-class26-diagnostics.json); the
[normalized trace](mermaid-class26-diagnostic-trace.txt) is retained alongside it.

- All 26 sources passed the Class envelope and body parser, completed initial
  layout and subsequently fell back. This was not a metadata parsing failure.
- The 20 cardinality variants reached the `Class08` → `C2` relation with 312
  candidate endpoint pairs; none completed the caption-aware route. For each
  variant, 270 pairs were rejected because at least one caption-bearing port
  faces vertically, while the current caption placement requires horizontal
  normals. The other 42 were rejected when a caption clearance cell was not
  free in the active route area. All had roughly 770,000 work units remaining
  when the connector returned false and no insufficient `Work::spend` was
  observed. The trace does not identify the owner of every blocked cell or
  establish that allowing vertical captions alone would complete the diagram.
- The six `full-with-notes` variants reached the note leader to `Class10`.
  The 88 × 40 port combinations yielded 3,520 pairs. The sorting-buffer
  prepayment requested 297,120 work units with 251,639–252,133 remaining,
  so `Work::spend` rejected it and set the remainder to zero. This identifies
  the immediate budget boundary, not proof that a larger budget alone would
  complete the remaining routes.

These observations identify the first failure encountered for this cohort;
they do not validate unattempted later edges or grant drawing credit. The
immediate follow-up is a source-bound geometry and resource-accounting design,
with unchanged safety limits and independent review before any feature change.
Per-attempt caption checks are retained in compressed form as
[`mermaid-class26-caption-trace.txt.gz`](mermaid-class26-caption-trace.txt.gz);
its hash and normalization are recorded in the diagnostic JSON.

#### P0 Class26 implementation and validation (2026-10-29)

The implementation uses two changes with the existing work limit: routing competing Class endpoint-caption relations in stable descending
central-label display width, and using a compact, stable index order for note
leader candidates. The second change uses prepaid fixed-size arrays for its
compact case and retains the existing wider path for larger candidate sets.
The fixed implementation was independently reviewed before integration.

In the final Debug and Release runs, the complete source-bound `Class285` geometry and mutation
report observed 237/237 admitted complete Models, including all 26 previously
failing originals. `ClassNotes18` observed 22/22 complete cases, including all
six target note variants, without geometry or mutation errors. After a test-only update to the frozen counts and independently reviewed gap
roster in the spike, the Debug `Class285`, `ClassNotes18`,
`ClassCaptionStarvation8`, and `OfficialMatrix` report tests passed. Release
and Debug each passed all 827 Mermaid-labeled tests on the prior implementation. With the two reviewed testing seams and three
new focused tests, Debug and Release each passed **830/830** Mermaid-labeled
CTest entries, including the terminal test. The 32
`MermaidRelations*` Debug tests also passed. Broader Debug reports found three
additional Merman Class endpoint originals with complete geometry and mutation
checks; an independent reviewer verified their source bindings. One still falls
back at 40 columns, and upstream browser/API assertions remain unverified.
Those three are outside the 2,145-file official denominator.

The final official matrix capture compared all 2,738 rows and all three
capture states to the Debug baseline; the hashes and changed rows are retained in
[`mermaid-class26-spike-matrix-comparison.json`](mermaid-class26-spike-matrix-comparison.json).
The only changes were the 26 original
wide and restored observations from error to drawn, and 20 corresponding narrow
observations from fallback to drawn; no earlier drawn observation regressed.
Its wide count is 622/2,145 (29.00%) and Class is 237/285
(83.16%); fallbacks and errors count as failures. The matrix roster contains 70 rows after the independently reviewed
removal of the 26 resolved original entries.

The new focused tests verify budget exhaustion and shared Work, stable compact
ordering against an independent reference, and public-rendered caption
competition including CJK, equal-width ties and unrelated labels. The 65,535
and 65,536 candidate products cannot reach sorting under the unchanged
1,000,000-unit Work limit: the candidate prepayment rejects them before
allocation. Both preset runs passed the mandatory-gap verifier and produced
the same official matrix SHA-256
`a9ff29453aeeddf44bda8c8fb9d521ec43ba8a6bcd73522704b4087f9ac8c0d9`.
The testing-facade additions passed the privacy lint and positive/negative BMI
probes on both presets. Independent reviewers `agent:/root/class_geometry_review`
and `agent:/root/note_budget_review` approved the implementation and resource
accounting; `agent:/root/class_oracle_review` verified the resolved roster rows.
The work does not close RFC 0030 Phase C or the remaining Class Model gaps.

### Historical error work assignment (2026-10-29)

This earlier assignment is superseded by the executed checkpoints below.
Class4, ER6, DefaultConfig, RequirementMarkdown1 and eight Flow originals are
resolved; active P0 is the two grouped headless/mixed Flow sources. The original
assignment was to resolve four Class, ten Flow and six ER whole-null diagnostics: an
unreviewed picture needs an independently bound model and geometry review,
while a source/restore mismatch needs its own correction. Investigate the two
ordinary-source protocol errors in `conf-and-directives` and Requirement.
Treat each as a distinct outcome, not automatically as a missing renderer.

#### Flow10 capture diagnosis (2026-10-30)

The final Class-checkpoint Release matrix and its five-frame Flow400 capture
were joined by exact fixture identity and source SHA-256; the ten rows and
input artifact hashes are retained in [mermaid-flow10-diagnostics.json](mermaid-flow10-diagnostics.json).
All ten wide frames differ from ordinary source, and all ten restored wide
frames equal their initial wide frames. Six narrow frames equal ordinary source;
four do not. Each of the ten remains whole-null with no decoded complete Model,
so none earns drawing success from these observations. All ten completed the
five-frame capture without verifier resource exhaustion. The shared error text
combines unreviewed wide art with ordinary/restore protocol differences; these
comparisons isolate the observed trigger without proving the art correct or
identifying its underlying layout cause.

The next test-first cohort should source-bind a complete independent Model and
mutation checks for the smallest defensible subset, beginning with the two
`57` nested-subgraph variants (both also produce non-source narrow frames).
Their source bodies are byte-identical (SHA-256
`fb40841b605787b3c31ab9c34840d405d05bfd250613b7b32258b2cbabb1324a`):
three sibling subgraphs contain six nodes, three internal node edges, one
cross-subgraph node edge, and three edges involving subgraph containers. The
Dagre variant adds `layout: dagre` in its frontmatter. Both complete envelopes
must be independently bound even though the bodies match. The existing
`GroupEndpoint15` source authority accepts at most two tagged group-endpoint
edges and requires distinct targets, whereas this body has three tagged edges
and repeats targets `two` and `c2`. Its validator must not be widened without
an independently reviewed model, assignment algorithm and negative witnesses.
`MermaidFlowSourceFacts.SiblingSubgraph57KeepsCompleteSourcesAndSharedTargets`
pins both complete source hashes, their distinct Dagre configuration and the
exact common body; it is source-provenance coverage only, not geometry credit.
The new test and four related Flow resource tests passed in both Debug and
Release (5/5 each). No complete-source geometry oracle has yet been added for
this pair, and the official rendering counts remain unchanged.
The 240/40/240 public captures and source identity must remain intact; an
unverified wide or narrow image remains an error, not a successful rendering.
Any unsupported or semantically incorrect result remains a documented failure.

**Completion evidence:** all 48 original baseline error paths, including the 26 now resolved, have been
individually adjudicated; the original-source 240-column matrix has zero
unexplained errors and no newly introduced errors. Count as drawn only the
paths that actually meet the full-source oracle; other paths remain explicit
fallback or pending. Per-cohort semantic mutations and lifecycle tests pass.

### P2 — Shared metadata, configuration and label projection

Use the P0 inventory to select high-impact, source-backed cohorts spanning
Flow, GitGraph and other already-supported families. Work through RFC 0030
Phase D and RFC 0031 E2b gates as applicable for bounded frontmatter/init
values, theme/style semantics, selected shape properties and rich/HTML/icon
labels. Admit a value only when its terminal meaning is preserved; rejecting
unsupported style or unsafe input remains valid fallback. Do not discard
metadata or count a metadata-stripped body as an original.

**Completion evidence:** each chosen configuration or label cohort has its
exact denominator, before/after original-file outcomes, negative controls and
cross-family regression results recorded. The selected complete originals
move to drawn only where all other blockers are resolved; all remaining cases
retain documented fallback. No unsupported configuration silently changes a
diagram's meaning.

### P3 — Deepen existing diagram families

After root-cause and oracle selection, work in small independently shippable
cohorts, prioritizing demonstrated impact and semantic dependencies:

1. **Flow:** admitted shape/edge/label and subgraph grammar, then source-proven
   geometry and routing failures. Separate parser/envelope blockers from fixed
   capacity limits; do not assume every fallback is router debt.
2. **Class and ER:** member and attribute compartments, relationship
   cardinality and labels, note connectors, grouping and complete geometry.
3. **Requirement and State:** requirement/element relationship geometry;
   state next-line braces and region dividers, then composite and pseudostate
   geometry where a reviewed profile exists.
4. **GitGraph, Pie and XY:** configuration-dependent semantics and bounded
   parent routing; pie labels/slices; chart axes, ticks and multiple series.

**Completion evidence:** each cohort publishes its fixed source list and
semantic oracle, actual completed-drawing numerator/denominator, unresolved
cases by confirmed blocker, useful mutation controls, and full-corpus
before/after matrix. The Sequence 193/193 baseline and all previously admitted
cases remain protected; an observed regression blocks graduation rather than
being offset by gains elsewhere.

### P4 — Expand to additional diagram families

Use the refreshed inventory to rank currently uncredited families by expected
user value, number of distinct source-bound cases and dependency readiness.
Start with one explicitly selected family and a bounded grammar/terminal
semantic contract; design and review it through the applicable RFC process
before implementation. Do not infer that a zero official rate means the body
alone is unsupported, or promise a family-wide rate from a small cohort.

**Completion evidence:** each new family has a reviewed grammar, complete
original-source positive and negative cases, defined semantics for unsupported
features, safe width/resource behavior, per-family outcomes and documentation.
Expand to the next family only with its own measured cohort and gate.

### P5 — Close terminal-suite and platform verification gaps

Reconcile the five complementary terminal suites at the original-function and
assertion level, including helpers, macros, options and runtime expansion.
Port source-bound assertions with their provenance and report both the
measured denominator and what is still unclassified. Separately complete the
existing Linux execution and BMI PSS gates and exercise real-terminal behavior;
these are not inferred from the macOS Debug checkpoint.

**Completion evidence:** each claimed upstream assertion or function has a
traceable source and passing native predicate; unresolved denominator or
foreign browser/SVG behavior remains explicitly unclaimed. Record Debug and
Release results, platform/build metrics, real-terminal evidence and residual
limitations in the owning RFC and ledger.

## Reporting and acceptance for every work package

1. Freeze baseline revision, fixture and oracle identities, host, compiler,
   preset, test selection and per-type outcomes before modifying production.
2. Run independently reviewed source/semantic and implementation checks; run
   the owning focused tests and the Mermaid label in Debug and Release, plus
   the repository's required checks for changes being landed. Record test
   counts, failures, timeouts and any isolated flake rerun. Use the project's
   standard build parallelism and do not weaken timeout or resource constraints
   without an evidence-backed, separately reviewed change.
3. Verify corpus integrity and applicable roster pins; regenerate and review
   capability reports on both presets. Compare every original outcome against
   baseline and reconcile all changed rows, including new or disappeared
   matrix errors. Update the user syntax guide, gap analysis and owning RFC
   history with evidence.
4. Publish a compact table: total and per-family drawn / total, fallback,
   error, admitted cohort / selected cohort, unresolved blockers and test
   status. Keep an append-only dated record of the baseline and each result.
   No target percentage, ETA, full conformance or phase completion is claimed
   until its evidence is measured.

The immediate next task is P0: refresh both preset baselines and classify the
48 error rows before choosing an implementation cohort. P1 and later do not
start production feature work until their source-bound oracle and applicable
RFC gates are satisfied.

## ER6 completed batch (2026-10-30)

Six source-pinned ER originals now pass complete geometry and mutations.
Debug/Release each pass 836/836 Mermaid tests, with identical matrices.
Official coverage is 629/2,145 (29.32%), fallback 1,501, error 15.
Remaining disjoint priorities: P0 15, P1 518, P2 447, P3 536.
See [source and implementation reviews](mermaid-er6-review.md) and
[exact matrix comparison](mermaid-er6-matrix-comparison.json).
Next P0: Class4 accessibility declarations, then Flow10 and Requirement1.

## Class4 completed batch (2026-10-30)

Accessibility declarations no longer create phantom Class entities. All four
complete source graphs and their metadata bindings pass. Release841/841;
Debug841 complete outcomes after seven stale-roster retries. Coverage633/2145
(29.51%), fallback1501, errors11. Remaining P0 11, P1 518, P2 447, P3 536.
[Source/design/code review and verification](mermaid-class4-review.md) and
[exact chained observation comparison](mermaid-class4-matrix-comparison.json).
Next P0: Requirement1 and Flow10; continue P1/P2/P3 after the remaining errors.

## RequirementMarkdown1 completed batch (2026-10-30)

The complete source, every literal field,14 meaningful adversaries and strict
resource/proof controls pass. Debug/Release842/842. Official coverage634/2145
(29.56%),fallback1501,errors10. Requirement12/172 (6.98%),fallback160,error0.
Remaining P0 10 (all Flow),P1 518,P2 447,P3 536. Continue through the remaining
priorities. [Executed reviews](mermaid-requirement-markdown-review.md) and
[one-case observation delta](mermaid-requirement-markdown-matrix-comparison.json).

### Flow P0 grouped7 checkpoint (2026-10-30)

The separate repeated-target domain admits six complete originals; source5064
also fixes group-port re-entry, temporary occupancy restoration and permanent
port sealing in both router modes. Seven source-owned Models pass complete
geometry, restoration and six mutations each. Debug/Release852/852. Coverage
641/2145 (29.88%),fallback1501,errors3; Flow61/400 (15.25%),fallback336,errors3.
Strict source/proof/ownership and actual resource controls pass, and both
matrices are identical with exactly seven improved original observations.
See [review and execution](mermaid-flow-p0-grouped7-review.md) and
[comparison](mermaid-flow-p0-grouped7-matrix-comparison.json).
Remaining: P0 3, P1 518, P2 447, P3 536. Continue through P0 and the remaining
priorities without requiring per-case user confirmation.

### Flow P0 length1 checkpoint (2026-10-30)

The nighttime edge-length original binds its independent complete source and
P0 Model to the unchanged reviewed visual294 LinkProfile3 geometry/mutations.
Debug/Release854/854. Coverage642/2145 (29.93%),fallback1501,errors2;
Flow62/400 (15.50%),fallback336,errors2. Both final matrices are identical;
exactly one original improves, with no changed statuses elsewhere. See
[executed evidence](mermaid-flow-p0-length-review.md) and
[comparison](mermaid-flow-p0-length-matrix-comparison.json).
Remaining P0 2,P1 518,P2 447,P3 536. Continue with grouped headless/mixed
physical strands and exact duplicate-edge multiplicity, then the lower priorities.

## Flow P0 strands2 completed batch (2026-10-30)

Debug867/867 (287.50s), Release867/867 (37.32s), plus
final exact-branch control1/1 in both. Coverage644/2145 (30.02%),fallback1501,
errors0; Flow64/400 (16.00%),fallback336,errors0. Exactly two original
observations improve;2736 other observation statuses remain unchanged.
[Executed evidence](mermaid-flow-p0-strands-review.md) and
[comparison](mermaid-flow-p0-strands-matrix-comparison.json).
P0 is complete; remaining P1 518,P2 447,P3 536. Continue with the independently
reviewed Class13 direct-style source/design proposal, then the lower priorities.

## Class direct-style13 completed batch (2026-10-30)

Coverage657/2145 (30.63%), fallback1488, errors0; Class254/285 (89.12%),
pending31. Debug878/878 (287.71s), Release878/878
(37.79s), serial style/resource/real-PTY14/14 each.
Exactly13 original statuses improve;2725 other observations remain unchanged.
[Executed evidence](mermaid-class-style13-review.md) and
[exact comparison](mermaid-class-style13-matrix-comparison.json).
P0 is complete; remaining P1 505, P2 447, P3 536. Continue
[defined-Class-style6](mermaid-class-defined-style6-plan.md), then remaining
existing-family work before P2/P3. No phase graduation is claimed.

## Class defined-style6 completed batch (2026-10-30)

Coverage663/2145 (30.91%), fallback1482, errors0; Class260/285 (91.23%),
pending25. Debug889/889 (315.76s), Release889/889
(38.94s), serial style/resource/real-PTY25/25 each.
Exactly six originals improve;2732 other observed statuses stay unchanged.
[Executed evidence](mermaid-class-defined-style6-review.md) and
[exact comparison](mermaid-class-defined-style6-matrix-comparison.json).
P0 complete; remaining P1 499, P2 447, P3 536. Continue
[Flow style4](mermaid-flow-style4-plan.md), then remaining P1 before P2/P3.
Proposals/design alone earn no credit; no phase graduation.
