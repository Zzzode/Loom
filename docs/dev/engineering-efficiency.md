# Engineering efficiency

Living record of how work actually spends time in this tree, the concrete
cases that wasted it, and the rules that prevent recurrence. R&D efficiency
is a first-class, inspected metric here: scale must not make each unit of
progress exponentially slower. Every entry below traces to a real observed
case (a commit, a log, or a measured number), not a generic best practice.

Read this together with the build/test section and the verification rules
in the top-level `CLAUDE.md`. Add a new entry whenever a task reveals a
repeated slowdown, a false green/red, a reverted implementation, or a
rework loop — and do the per-task retro at the end of every unit of work.

## Efficiency metrics to watch

- **Gate wall-clock.** The actual elapsed time of the verification ceremony
  for a change (targeted tests, full label suite, OfficialMatrix, serial
  three-family gate, ASan, byte-compare), not just pass/fail.
- **Draws (or other outcome) per cohort.** Marginal product per bounded unit
  of work. A sustained drop is a signal that the easy vein is exhausted and
  the strategy should change, not a reason to run the same ceremony harder.
- **Rework rate.** Work implemented and then reverted, or probes that gave a
  false answer. Each revert/false-green is an efficiency defect.
- **Build fan-out.** How many translation/module units a one-file edit
  recompiles, driven by the BMI graph rather than the archive split.
- **Flake tax.** Time lost to timing-sensitive tests and environment load.

## Measured bottlenecks (Mermaid program, this host)

The OfficialMatrix renders **2145 pinned fixtures × 5 frames** (widths
240/40/240 plus two ordinary-control frames; `test_mermaid_official.cpp`
`constexpr std::array<int,5> widths`) and captures each into a fixed
**240 × 800 = 192,000-cell** char vector even for three-node graphs. That
is roughly 10.7k full-width renders per run, plus 400 body-diagnostic rows.

- Same matrix, measured 2026-11-09: **debug 242.6s vs release 27.6s (~8.8×)**.
  The debug preset is `-O0` plus assertions; ASan matrix runs are ~900s.
- Full `ctest -L mermaid`: **1191 tests, ~507s parallel**; the serial
  three-family gate alone is ~398s because its members are single giant
  tests (Config110 ~206s, Class285 ~167s, Block58 ~23s) that ctest cannot
  parallelize internally.
- Host is 56 cores but shared; under load average 53–92 the ~287–299s
  Config110 test crosses the 300s ctest cap and flakes (it always passes
  run directly without the cap). Load tax is real and recurring.
- The matrix main loop renders **every fixture cold**. The E6b
  `LayoutRowsCache` exists and measured a ~640× single-call speedup, but the
  only cache instance in the test TU is a 4-entry cache used to test Block58
  cache semantics; the 2145-case harness never attaches one.
- Module BMI fan-out: touching an interface `.cppm` (e.g. `design_tokens`)
  forces ~62 module recompiles regardless of the library split; body edits
  to `mermaid_impl.cpp` still trigger the module closure. Full builds are
  multi-minute.

## Test and infrastructure rules

1. **Layer the gates; do not run the full ceremony per small cohort.** During
   iteration run only the affected targeted tests. Run the full serial
   OfficialMatrix, the three-family serial gate, ASan and the debug/release
   byte-compare once per family closure / final milestone (the D-117
   milestone policy). Group small related cohorts into one ledger/review/
   gate/commit so the fixed ceremony cost is amortized (the FS-5 G1–G7
   grouped batches proved this: +3 shapes for roughly one ceremony).
2. **Prefer the release preset for full-matrix runs; keep debug for the
   contact surface and the closing byte-compare.** Release is ~8.8× faster
   and the dual-preset byte-identical gate already proves equivalence. Use
   debug for the changed-surface tests and one closing matrix on each
   preset; do not pay the debug matrix cost on every iteration.
3. **A target-only build can leave `test_mermaid_official` stale-linked and
   produce a false green.** After a production change, do a full build before
   trusting an OfficialMatrix verdict (FS-sgtm lesson: the first "green"
   matrix was a stale binary; the full build exposed the real 48→51 roster
   move). Verify the binary timestamp, not just the exit code.
4. **One writer per build tree (D-100).** Never run two configure/build
   invocations against the same `build/<preset>` concurrently — including
   builds a "read-only" review fork launches. Concurrent writers cause random
   clang BMI serialization ICEs / bus errors in unrelated files and poisoned
   outputs that look like compiler defects. `pgrep -af 'cmake --build|ninja'`
   before building; kill stray writers and, on corruption, wipe and
   reconfigure the preset.
5. **Direct binary runs need the brew OpenSSL/curl library path or they
   false-red at load time.** Prefix both gtest discovery builds and direct
   test-binary runs with
   `LD_LIBRARY_PATH=/home/linuxbrew/.linuxbrew/opt/openssl@3/lib:/home/linuxbrew/.linuxbrew/opt/curl/lib`.
   Without it the binary fails with `libssl.so.3: cannot open shared object`
   (result 127), which looks like a test failure.
6. **Never decide "drawn" from `Screen::ToString`.** Color escapes make a
   verbatim text fallback differ from the mermaid frame as a string even when
   the cells are identical. Use the `CellCapture` char-vector comparison
   (`screen.PixelAt(x,y).character`) that the cohort gtests standardize on.
7. **Do not run combinatorial render-probe grids.** A 7-level × 315-fixture ×
   240×80 ablation run once took 37 minutes and ~10 GB. Probe one variable
   at a time against a handful of representative fixtures; run the full
   matrix only to confirm a scoped hypothesis.
8. **Timing flakes are environmental, not regressions — prove isolation.**
   `ServerMain.DirectConnect*` and the Config110 cap flake under high load.
   Re-run the single test directly (no ctest cap) before treating a timeout
   as a product failure, and record the load average alongside the result.
9. **Use the matrix JSON override for analysis instead of re-running under
   ctest.** `LOOM_MERMAID_MATRIX_PATH=<path> test_mermaid_official
   --gtest_filter=MermaidCapabilityReport.OfficialMatrix` writes the full
   report (~28s release) for offline Python census work, and can be a
   background task while non-build work continues.

## Implementation and rework-avoidance rules

10. **Verify flip counts with a real full-matrix run before scoping a cohort;
    static analysis over-predicts.** FS-sq statically guessed 21 flips; the
    matrix showed 4. A targeted render probe is not evidence of whole-corpus
    safety.
11. **An envelope/parser change that looks non-terminal can still create a
    matrix error.** The envelope parser sees only frontmatter, so it cannot
    tell a clean-bodied fixture from one carrying a body shape debt. A broad
    `layout: elk*` whitelist flipped `3-layout-elk-subgraphs` from
    source_fallback to **error** once the envelope opened and its unsupported
    cylinder body surfaced as unreviewed art (FS-elk). Admit the single
    verified value and keep siblings explicit fail-closed controls; confirm
    with the full matrix, not a targeted render.
12. **Check the family's frozen admission contract before building, not
    after.** class-link/callback and subGraphTitleMargin were fully
    implemented and green, then reverted because admitting them trips the
    frozen Class285 gate (errors 0→10) / the E6c 48-gap roster (48→51/58).
    A non-terminal key is not sufficient when a sibling with a byte-identical
    envelope is roster-pinned. If a product/contract change is required to
    pass, stop and report the root cause; do not self-relax a frozen contract.
13. **Grep every earlier cohort before admitting a new token/shape.** Newly
    admitted shapes repeatedly served as the "unknown token" negative in
    earlier cohort tests and broke them in bulk (FS-5e/FS-5 G3/G7: up to 11
    sites). Negative examples must use a token from the far end of the
    roadmap, never the next queue item; after an admission,
    `grep -rn "shape: <newtoken>" tests/ui/test_mermaid_fs*.cpp`.
14. **The matrix hook is an earliest-match else-if chain — migrate the
    existing ledger, do not add a shadowed branch.** A shadowed hook builds
    observations but never calls `run_case`, becoming dead code that can also
    threaten the gap roster (FS-init MAJOR). Overlapping cohorts move the
    earlier ledger in place with fixture shas untouched.
15. **Narrow predicates to exactly the new case and lock the preserved
    behavior with a regression test.** An over-broad self-connection drop in
    FS-7c silently deleted authored self-loops on visible vertices; the fix
    gated the drop on "an endpoint was actually remapped" and added a
    visible-self-loop survival test.
16. **Boolean "it draws" tests cannot catch geometry defects.** Contour
    dead-writes and even-width edge-port misplacement (G3 brace, G5 doc) were
    invisible until exact glyph/pixel-column assertions were added. For any
    new shape, add TB+LR edge attachment tests and, where a visual centre can
    diverge from width/2, an even-width pixel-column assertion.
17. **Reviews are read-only and agent-run.** A fork inheriting context has
    repeatedly violated read-only by launching its own builds (single-writer
    hazard) and suffered identity confusion. Spawn a fresh general-purpose
    agent with an explicit zero-build brief; verify after the review that it
    wrote nothing and built nothing.
18. **A token-splitting/segmentation fix can hold wide states while flipping
    narrow states — re-measure every per-width geometry pin, not just the
    240-column partition.** The timeline M1 colon-split fix left the 7-row
    wide draw partition byte-identical but shortened one fixture's rows enough
    to flip its 40-column state source->drawn. After any change to how a line
    is split into tokens/events, regenerate the cohort's NARROW_DRAW pins from
    live renders before trusting the ledger.
19. **Enumerate the corpus's actual marker spellings before writing
    case-folding/normalization code or its tests.** A first `<br>` sanitizer
    handled upper-case variants but missed mixed case, and its test invented a
    `<br >` spelling that exists nowhere in the corpus or the accepted
    vocabulary. Run `grep -rhoiE '<pattern>' cases/<family>/ | sort | uniq -c`
    first; code the real spellings, test the real spellings, and keep
    invented forms as explicit fail-closed controls, not success cases.
20. **In a comma-field state-machine parser, every accept branch must advance
    the field index; a missing increment fails whole files silently.** The
    gantt lone-duration branch set the span but omitted `++index`, so the
    trailing `index == field_count` guard rejected the whole source — and
    because nearly every gantt ends with a chained `: 24d` task, one missing
    increment suppressed ~24 fixtures at once. Synthetic happy-path tests
    passed (they used other shapes); only the full-corpus partition test
    exposed it. When adding a parser branch, explicitly trace index
    advancement on every path, and keep the full-corpus draw-partition test as
    the oracle rather than trusting hand-written single-shape cases.
21. **A structural parser must enforce node roles (leaf vs container), not
    just token shapes — faithfully transcribing an impossible tree is still
    over-acceptance.** The first treemap parser validated indentation and
    values but allowed a value-bearing leaf to have nested children, a shape
    upstream mermaid almost certainly rejects and that no corpus fixture uses.
    The independent reviewer caught it; the fix tracks open leaf depths on a
    stack and fails closed when a node would nest inside a leaf, with
    leaf-child and valid-sibling regression tests. When modeling an
    indentation/tree grammar, write down the role invariants (which node kinds
    may have children) and test the impossible combination, even if the corpus
    never contains it.
22. **Every output path must go through the same bounded emission guard; a
    special-cased line that appends directly bypasses the row cap, width limit
    and Work fee.** The c4 renderer pushed the title straight to the output
    vector while statements used a guarded helper, so a long or duplicated
    title could exceed the 240-cell/1024-row contract. The fix routes the title
    through the identical guarded helper, rejects duplicates like the sibling
    renderers, and counts the row cap over the combined output so two vectors
    cannot each fill the cap. When adding a renderer, enumerate every place a
    row reaches the output vector and confirm each one passes the single
    cap/width/Work check — special input lines (title, header, footer) are the
    usual offenders.
23. **Never re-emit a source keyword through a hand-counted fixed-length
    substring; transcribe the token itself or derive the length from it.** The
    quadrant renderer rebuilt the six-character `x-axis`/`y-axis` keyword with
    `substr(0, 5)`, silently emitting `x-axi`. It compiled cleanly and the
    draw-vs-source surface partition still passed — any native frame differs
    from the text fallback — so the only thing that caught it was a verbatim
    keyword assertion against the CellCapture text. When the output must
    preserve a source token, copy the token (or use its measured length)
    instead of retyping its width, and give every emitted keyword a literal
    substring assertion in the cohort test, not just a draw/source boolean.
24. **In a strip-then-whitelist validator, the final allow-set must still
    contain the token class you validated in an earlier scan, and a
    sub-validator must not assert on what follows its own construct.** The
    usecase edge validator removed connector runs and validated every
    identifier run in a first pass, then a final punctuation whitelist loop
    rejected the same identifier letters it had just accepted — so every real
    edge failed while the unit-level happy paths were never exercised. A
    second defect had the `@{...}` sub-validator also constraining the
    characters after its closing brace, rejecting a valid
    `@{...} <<stereotype>>` adjacency. When validation is split into passes
    or helpers, each stage owns only its own span; either re-derive the
    allowed character set from one place or keep the previously validated
    class in the final whitelist. The full-corpus CellCapture partition test
    (not single-shape synthetic cases) exposed the first defect — keep it as
    the oracle per rule 20.

25. **A backgrounded `cmake --preset`/`ctest` inherits the shell's current
    working directory, and a direct ctest run leaves it drifted inside
    `build/<preset>/tests/ui`; a preset then fails reading
    CMakePresets.json while the task wrapper still reports exit 0.** This
    cost a wasted build round-trip in two consecutive families (cynefin and
    wardley) because the wrapper's success was trusted instead of the
    in-log RC marker. Prefix every build/configure command with an absolute
    `cd /home/zhangdi.zode/Develop/Loom`, append an explicit in-log
    `*_RC=$?` marker, and verify that marker (never the background-task exit
    code) before treating the artifact as current. After any direct binary
    or ctest run, reset the foreground cwd back to the repo root. The same
    trap applies to gtest filter names: a misremembered filter runs zero
    tests and still exits 0, so confirm the log says one test actually ran
    before copying or trusting any output artifact.

## Per-task efficiency retro (mandatory at every closure)

Attach a short efficiency section to each closure report, answering:

1. What was the gate wall-clock, and which step dominated it?
2. Did any test give a false green/red or flake? What is the durable
   prevention (rule above or a new one)?
3. Was any work reverted or re-done? What upstream check would have caught it
   earlier?
4. Did a one-file edit cause an unexpectedly large build fan-out?
5. Is there a measurement showing the chosen approach was cheaper than the
   alternative (grouped vs single cohort, release vs debug matrix)?
6. Any new entry this document should capture? Add it before closing.

## Open efficiency work (prioritized, each needs its own validation)

1. **Gate layering + release-primary matrix** — process-only, no product
   code, no frozen-contract risk; highest immediate return. Adopt rules 1–2
   as the default ceremony.
2. **Persistent sha-keyed matrix cache** — key on `(source sha, width,
   renderer content hash)`; fixtures are sha-pinned so most re-renders are
   unchanged across cohorts. Reuse the E6b cache mechanism; the frozen
   byte-identical matrix gate validates invalidation safety.
3. **Fixture-level parallelism inside the heavy matrix tests** — renders are
   deterministic per call with per-call Work and thread_local preview flags;
   shard fixtures across threads on the 56-core host. Needs a determinism and
   shared-cache design pass.
4. **Adaptive capture height** instead of the fixed 800 rows, to cut the
   192k-cell work for small graphs. Golden byte risk — gate strictly.
5. **Split `mermaid_impl.cpp` / reduce BMI fan-out** — structural, schedule
   separately; reduces compile time, not run time.

## Retro log

- **2026-11-09, FS-elk cohort (`eb3118c8`).** Outcome +1 whole draw. A broad
  whitelist was implemented first and rejected by the full matrix (one new
  error, two no-op rows held by the FS-1b Work ceiling), then narrowed to one
  verified value — a rework loop that rules 10–11 now front-load. The full
  debug matrix (243s) was run multiple times during diagnosis; per rule 2 the
  census iterations should have used the ~28s release binary (the final
  debug/release byte-compare still ran once). Gate wall-clock dominated by
  OfficialMatrix and the 398s serial gate; no flake at load ~23. Frozen
  contracts unmoved; full-matrix sha `2f41e510`.
- **2026-11-10, efficiency inspection that created this document.** Root
  causes of perceived slowdown separated into (a) declining marginal product
  as cheap non-terminal veins exhaust and remaining rows carry compound/frozen
  debts, and (b) high fixed ceremony wall-clock (10.7k cold renders/run, 8.8×
  debug penalty, no matrix cache, monolithic serial tests, BMI fan-out). (a)
  is inherent to the remaining problem; (b) is addressable and tracked above.
- **2026-11-10, FS-maxwidth cohort (2 whole draws, predating its commit).**
  First cohort executed under the new layered/release-primary rules. (1)
  Reused the pre-existing FS-6 ledger and its generic Flow400 whole branch
  instead of creating a ledger/test-binary/authority string: the two rows
  migrated body-only→whole-and-body in place, so the full suite count stayed
  1191 (no new gtest target, no CMake reconfigure, no new authority whitelist
  entry) — the fixed ceremony was amortized onto an existing gate. (2)
  Iteration used release-targeted builds plus a temporary probe attached to an
  already-built test binary (then removed), not full debug matrices; the full
  release matrix ran once at ~28s and debug once only for the closing
  byte-compare (identical, sha `adefc5c4`). (3) Scope discipline: a census
  found six accepted-leaf/no-profile rows across five distinct missing
  predicates; only the single cohesive `Layout|Flowchart|MaxWidth` pair (2
  rows) was taken, deferring four other predicate shapes rather than bundling
  unrelated changes. Gate wall-clock was still dominated by the 524s full
  suite and 420s serial gate at family closure; no flake, no false green, no
  reverted work. Lesson applied: prefer in-place ledger migration through a
  generic branch over a parallel cohort whenever the rows already belong to an
  admitted ledger.
- **2026-11-12, timeline family renderer (7 whole draws, predating its commit).**
  Second greenfield family executed under release-primary/layered gates.
  (1) Gate wall-clock: release full matrix ~92s vs debug ~761s (8.3x); all
  iteration (M1 split fix, N1 case-folding, M3 fail-closed tests) ran against
  release cohort binaries, debug matrix ran twice only because an M2 comment
  landed between the two closing runs — the comment-only edit was separately
  proven byte-identical (same release matrix sha before/after), so the second
  debug run was ceremony, not diagnosis. (2) Two self-inflicted reds, both
  caught by the new durable tests before any matrix gate: an uppercase-only
  `<BR>` transform missed mixed-case markers (fixed by one longest-first
  case-insensitive scan), and a test invented a non-canonical `<br >` spelling
  absent from both the renderer vocabulary and the corpus (corrected to
  canonical `<Br>`). Upstream check that would have prevented both: enumerate
  corpus marker spellings with `grep -oiE` BEFORE writing folding code and
  tests. (3) The M1 colon-split fix kept the 7-row wide-240 draw partition
  exactly as the review predicted but flipped one fixture's narrow-40 state
  source->drawn (shorter event rows fit 40 columns); the stale NARROW_DRAW pin
  was caught by the live pixel gate and re-measured. Lesson: a segmentation
  fix can hold wide states while moving narrow states — re-measure every
  per-width pin after any token-splitting change, never assume only the wide
  partition matters. (4) No shipped rework, no flake, no false green; frozen
  contracts (flow 160/240, body 372/173/48, Class285, 48-gap roster) unmoved;
  observation total held at 2738. (5) Greenfield economics confirmed again:
  +7 draws for one ledger/one review/one gate, reusing the journey
  text-table/visual-gate scaffolding — the same fixed ceremony that bought +1
  on an envelope cohort now buys a whole family.
- **2026-11-12, gantt family renderer (39 whole draws, predating its commit).**
  Third and largest greenfield family; executed under release-primary/layered
  gates with a pre-implementation design doc that froze the corpus census so
  no re-census was needed during coding. (1) Gate wall-clock again dominated by
  the ~95s release / ~13min debug matrix; all parser iteration ran on the
  release cohort binary (~5s per full 39-fixture render pass). (2) Two real
  parser defects were caught by the durable corpus gate before any matrix run:
  a lone-duration branch set the span but forgot to advance the field index,
  which the trailing `index == fields.size()` guard turned into a whole-source
  reject — because nearly every gantt ends with a chained `: 24d` task, this
  one missing `++index` suppressed ~24 fixtures at once; and the non-terminal
  `weekend`/`weekday` directives plus the zero-span `vert, 01,` marker form
  were absent from the first grammar pass. Both were shape gaps the synthetic
  tests alone did not cover, which is why the live-fixture partition test is
  the real oracle. (3) The independent reviewer reimplemented the parser in
  Python and reproduced the exact 39/11/2 partition — the strongest
  accept/reject verification this program has used; it also caught one MINOR
  (milestone zero-span mislabeled "end 0") no test asserted. (4) NARROW_DRAW
  again had to be measured, not assumed: 35 of 39 draw at 40 columns even
  though the rows are long, because the structural table wraps rather than
  overflowing — reinforcing rule 18 (re-measure every width pin). No flake, no
  false green; frozen flow 160/240, body 372/173/48, Class285 and the 48-gap
  roster unmoved; observation total held at 2738. (5) Throughput economics
  improved again: +39 draws — the biggest single batch in the program — for
  one design/ledger/review/gate, reusing the journey/timeline text-table and
  visual-gate scaffolding. Rule 20 above captures the field-consumption
  bookkeeping lesson.
- **2026-11-12, treemap family renderer (10 whole draws, D-137, predating its
  commit).** First family self-selected under the D-137 mandate rather than
  pre-scoped: a quick grammar/header census ranked the candidates and picked
  treemap over agentflow (a flowchart-syntax variant that inherits frozen
  shape/subgraph debt), c4 and usecase, because it is a self-contained
  indentation tree with no edges or shapes and all 26 originals are real
  source-backed fixtures. (1) The upfront census paid off again: the 26 files
  split cleanly into 10 plain / 10 classDef / 6 frontmatter-config purely by
  grep-able contracts, so the visual gate was written once and the live
  partition test passed on the first full-corpus run — no parser rework loop
  like gantt's index bug. (2) NARROW_DRAW still had to be measured, not
  assumed: 9 of 10 draw at 40 columns; the single long-text fixture stays
  source because its 64-cell wrap exceeds the 40-column terminal — another
  confirmation of rule 18. (3) The independent reviewer (Python reference
  model over all 26 files plus 67 attack cases) found zero MAJOR but caught a
  real semantic gap the corpus never exercises: a value node was allowed to
  have nested children, which upstream likely rejects. Fixed fail-closed with
  a leaf-depth stack before the ledger flipped, plus direct cap tests. The
  lesson: a structural parser must enforce node *roles* (leaf vs container),
  not just token shapes — a faithful transcription of an impossible tree is
  still over-acceptance. (4) No flake, no false green; frozen flow 160/240,
  body 372/173/48, Class285, the 48-gap roster and observation total 2738 all
  unmoved. (5) Throughput: +10 draws for one design-free census/ledger/review/
  gate, reusing the journey/timeline/gantt scaffolding nearly verbatim. Rule
  21 above captures the leaf-role enforcement lesson.
- **2026-11-12, c4 family renderer (23 whole draws, D-137, predating its
  commit).** Second D-137 self-selected family; a closed-vocabulary
  function-call grammar rather than an indentation tree. (1) The renderer-free
  generator's hard drift guard again paid for itself: the manual census
  predicted 24 draw / 8 gated, but the generator's frontmatter scan found a
  ninth gate (`config: wrap: true` in description-wrapping.mmd) and refused to
  emit the ledger until corrected to 23/9. Re-confirms rule 19 — let the
  drift guard, not a hand count, own the partition. (2) Two real grammar gaps
  were caught by the live-fixture partition test, not the synthetic cases: a
  tight brace `){` (the parser only accepted `) {`) and a bare-identifier
  element name (`Person(customer, Customer, ...)`). The corpus is the oracle;
  hand-written happy paths miss spacing/quoting variants. (3) The independent
  reviewer found a genuine invariant hole the corpus never triggers: the
  title line bypassed the shared row-cap/width/Work emission guard and allowed
  duplicates, unlike all three sibling renderers. Fixed by routing the title
  through one guarded push helper and rejecting duplicates, then unifying the
  row cap across the title+statement vectors so the cap cannot be filled twice.
  Lesson captured as rule 22. (4) A REQUEST-CHANGES verdict was closed by
  sending the exact diff back to the same reviewer for re-verification rather
  than self-approving — the adversarial loop is only trustworthy if the same
  reviewer confirms the fix. No flake, no false green; frozen flow 160/240,
  body 372/173/48, Class285, the 48-gap roster and observation total 2738 all
  unmoved. Throughput: +23 draws for one census/ledger/review/gate, reusing the
  greenfield scaffolding.
- **2026-11-12, venn family renderer (11 whole draws, D-148, predating its
  commit).** Third D-137/D-148 self-selected family; a small declarative
  grammar (set/union/text) close in shape to treemap. (1) The source-driven
  live census again overrode a hand estimate: the initial grep suggested 14
  draw, but separating frontmatter gates from in-body `style`/classDef/`:::`
  color statements produced the true 11/7/3 partition. The generator's hard
  drift guard and the live-fixture partition test are the only trustworthy
  partition source — re-confirms rule 19. (2) The independent reviewer's M2
  was the same class of defect as the earlier c4 finding (rule 22): the title
  was inserted into the output vector directly, bypassing the row/width/Work
  guard. Fixed by giving the title its own vector filled through the identical
  guarded push and counting the row cap over the combined output. The fact
  that the same anti-pattern recurred across two renderers is exactly why
  rule 22 says to enumerate every path that reaches the output vector. (3) The
  reviewer also surfaced a nesting over-acceptance (a set could nest under a
  set); the corpus only ever uses two levels, so the role invariant was made
  explicit (set/union depth 0, text depth 1) and tested both ways — another
  application of rule 21. Two MINORs (quote-aware suffix scan, gate parity on
  non-corpus inputs) were deliberately deferred: both fail closed, have zero
  corpus impact, and "fixing" them without a corpus driver would be speculative
  widening. No flake, no false green; frozen flow 160/240, body 372/173/48,
  Class285, the 48-gap roster and observation total 2738 all unmoved.
  Throughput: +11 draws for one census/ledger/review/gate on the shared
  greenfield scaffolding.
- **2026-11-13, quadrant family renderer (7 whole draws, D-157, predating its
  commit).** Seventh greenfield family; a flat declarative grammar
  (title/axes/quadrant labels/[x, y] points), structurally closest to venn.
  (1) The live census again overrode the mandate's hand estimate: the brief
  said 13 clean / 1 gated, but separating %%{init}%% visual directives
  (axis position, chart sizing, themeVariables colors) and inline point
  styles gave the true 7 draw / 5 init-gated / 2 body-style-gated partition.
  Re-confirms rule 19; the generator drift guard is the acceptance oracle,
  not the estimate. (2) New defect class, captured as rule 23: the axis
  keyword was re-emitted via a hand-counted `substr(0, 5)` for the
  6-character `x-axis`/`y-axis`, silently producing `x-axi`. It compiled and
  the draw-vs-source surface partition still passed (any native frame
  differs from text), so only a verbatim keyword assertion in the
  CellCapture test caught it. (3) The corpus contains a valid header-only
  "empty frame" fixture; the sibling-wide `out.empty() => reject` convention
  would have dropped a clean original, so the empty case emits the one
  verbatim keyword row through the same guarded push (rule 22 respected).
  (4) Per-track surface equivalence: the header-only fixture draws on the
  whole track (frontmatter makes the surfaces differ) but its one-line body
  is surface-identical to plain text at every width, so the ledger carries
  distinct whole/body state pins, not one shared narrow flag — another
  rule-18 style reminder that geometry pins are per track. (5) Machine
  environment drift, not a code defect: gtest discovery and test binaries
  failed with `libssl.so.3: cannot open shared object file` (the brew-glibc
  loader does not trust the system lib path; LD_LIBRARY_PATH was empty).
  Fix for every test invocation on this box:
  `LD_LIBRARY_PATH=/home/linuxbrew/.linuxbrew/opt/openssl@3/lib` — openssl
  only, brew glibc/llvm stay on RUNPATH, system tools remain unaffected.
  No flake, no false green; frozen flow 160/240, body 372/173/48, Class285,
  the 48-gap roster and observation total 2738 all unmoved. Throughput: +7
  draws for one census/ledger/review/gate on the shared greenfield
  scaffolding.
- **2026-11-13, architecture family renderer (16 whole draws, D-161,
  predating its commit).** Eighth greenfield family and the largest
  single greenfield batch since gantt; a declarative node/edge DSL
  (group/service/junction + compass-port edges), structurally a stricter
  validator than the outline renderers because it transcribes accepted
  DSL lines verbatim rather than rebuilding them. (1) Two parser defects
  in the first iteration, both classes already on the register. The
  declaration id was split on a space, but the corpus writes
  `service vm1(server)[VM]` with no space between the id and the
  brackets — every icon/label declaration was rejected until the id was
  read as a leading identifier run up to the first `(`/`[`/space.
  Re-confirms rule 19 (enumerate the real marker spellings before
  writing the tokenizer) and rule 23 (never assume a hand-counted token
  boundary; derive it from the actual character class). (2) The two edge
  endpoints have OPPOSITE field order — source is `name:Port`, target is
  `Port:name` — and one shared validator silently rejected every edge.
  Splitting into source/target validators is an application of rule 21
  (encode the role/order invariant explicitly and test both directions).
  (3) The live narrow census again overrode the empty initial pin: ten of
  the sixteen clean diagrams also draw at 40 columns on both tracks; the
  six larger/deeply-nested ones stay source there, re-confirming rule 18
  (measure every per-width pin from live renders, don't assume source).
  (4) No new rule was needed; the verbatim-transcription design means
  rule 23's keyword-substring hazard cannot recur because no DSL token is
  reconstructed. No flake, no false green; frozen flow 160/240, body
  372/173/48, Class285, the 48-gap roster and observation total 2738 all
  unmoved. Throughput: +16 draws for one census/ledger/review/gate on the
  shared greenfield scaffolding.
- **2026-11-13, kanban family renderer (11 whole draws, D-161 continuous
  family after architecture).** Ninth greenfield family and the first
  with a 100% clean corpus (11/11, zero gated fixtures); the simplest
  grammar so far — a flat list of `id[Label]` cards with optional flat
  `@{...}` annotations, no edges or nesting. (1) The wide-track renderer
  passed on the first iteration; the only live correction was the
  per-width geometry pin (three compact boards draw at 40 columns, eight
  long-label/many-card boards stay source there), again measured rather
  than assumed — rule 18. (2) A self-caught Occam violation: the first
  draft copied the sibling wrap helper even though verbatim card
  transcription never wraps, then added a dummy call to keep it
  referenced. Dead code dressed up as used code; deleted the helper and
  its constant and let the single guarded push plus the label byte cap
  bound the width. Prefer deleting dead code to referencing it. (3) The
  `@{...}` block required an explicit "flat, no nested brace" shape check
  so a structural annotation is transcribed but a nested/object payload
  fails closed; the final `tail.empty()` is the load-bearing guard that
  rejects any trailing token after the card. No new rule needed; rules 18
  and 22 cover the measurement and the single emission guard. No flake,
  no false green; frozen flow 160/240, body 372/173/48, Class285, the
  48-gap roster and observation total 2738 all unmoved. Throughput: +11
  draws for one census/ledger/review/gate on the shared greenfield
  scaffolding.
- **2026-11-13, usecase family renderer (14 whole draws, D-176,
  predating its commit).** Tenth greenfield family and by far the
  grammar-richest (UML edges, boundaries, notes, quoted multiline
  labels, named edges), so it needed a quote-aware logical-statement
  joiner rather than the line-at-a-time sibling parser: physical line
  breaks inside a double-quoted label are merged while the quote count
  is odd, and an unclosed quote fails closed. Two real validator defects
  surfaced only through the live CellCapture suite. (1) The edge
  validator removed connector runs and validated every identifier in a
  first scan, then a final whitelist loop rejected the same identifier
  letters it had just accepted — a self-contradictory two-stage
  validator that rejected every real edge. Captured as rule 24: a
  strip-then-whitelist validator must keep the stripped token class in
  its final allow-set. (2) The @{...} sub-validator also asserted what
  could follow the closing brace, so a `@{...} &lt;&lt;Stereotype&gt;&gt;`
  adjacency (fixture 09) was rejected; fixed by making the sub-validator
  validate only the block and return the position past its brace,
  leaving the suffix to the declaration loop (local closure). (3) The
  partition again came from the source census, not the brief's count:
  eight files contain ::: but two are also frontmatter-gated, giving the
  true 14/12/6. Re-confirms rule 19. The live narrow census produced nine
  40-column draws including the header-only frame — rule 18. No frozen
  contract moved; flow 160/240, body 372/173/48, Class285, the 48-gap
  roster and observation total 2738 all unmoved. Throughput: +14 draws
  for one census/ledger/review/gate on the shared greenfield
  scaffolding.
- **2026-11-13, tree-view family renderer (8 whole draws, D-180,
  predating its commit).** Eleventh greenfield family, an indentation
  tree rather than a line/edge DSL: every node sits on a 2+4k space grid
  and carries a quoted or bare label with optional icon(...) and ##
  description suffixes, transcribed verbatim with source indentation
  preserved. (1) The live source overrode the brief's census. One fixture
  closes its frontmatter with a trailing-whitespace fence ("---      ");
  an exact-"---" splitter treated it as clean, but it contains
  config:/themeVariables and is a visual gate. The correct, source-driven
  partition is 8 clean / 5 frontmatter / 2 body, not the brief's 9/4/2 —
  recorded as a census difference per the milestone rule. Both the C++
  fence scan and the Python generator now tolerate trailing spaces/tabs
  on a closing fence (rule 19: enumerate the real marker spelling). (2)
  A self-caught validator defect: the icon(...) branch consumed the whole
  balanced parenthesized pair atomically but left a parens counter at 1,
  so every suffixed node was rejected at the end-of-line check; fixed by
  keeping the pair local to the branch and not touching the stray-paren
  counter — another instance of rule 24 (a sub-validator owns only its
  own span). (3) All eight clean trees are compact enough to also draw at
  40 columns on both tracks (rule 18 live measurement). No new rule was
  needed; rules 18, 19, 22 and 24 cover the work. No frozen contract
  moved; flow 160/240, body 372/173/48, Class285, the 48-gap roster and
  observation total 2738 all unmoved. Throughput: +8 draws for one
  census/ledger/review/gate on the shared greenfield scaffolding.
- **2026-11-13, ishikawa family renderer (7 whole draws, D-187,
  predating its commit).** Twelfth greenfield family and the simplest
  grammar yet: ishikawa-beta is a relative-indentation tree whose nodes
  are bare labels (the effect root at 2 spaces, then cause categories and
  nested sub-causes), transcribed verbatim with source indentation
  preserved. (1) The candidate count of 13 was never a clean-count
  promise; the live, fence-tolerant census gives 7 clean / 6
  frontmatter-gated / 0 body — the six themed/padded/usemaxwidth
  fixtures carry theme/look/config keys the terminal cannot express.
  Recorded per the milestone rule that census numbers are candidates and
  the source decides (rule 19). (2) Unlike tree-view's strict 2+4k grid,
  ishikawa indentation is relative — fixture 13 deliberately mixes
  2/3/4/5/6/8/10/12/14-space indents — so the renderer only requires a
  >=2-space indent and a bare-label character set rather than
  reconstructing parent/child geometry it cannot draw anyway. The
  distinction is asserted in the cohort test. (3) All seven clean trees
  also draw at 40 columns on both tracks (rule 18 live measurement). A
  self-caught test bug treated a skipped blank line as a rejected empty
  node; the renderer skips whitespace-only lines by design, so the test
  now asserts that behavior. No new rule; rules 18, 19 and 22 cover the
  work. No frozen contract moved; flow 160/240, body 372/173/48,
  Class285, the 48-gap roster and observation total 2738 all unmoved.
  Throughput: +7 draws for one census/ledger/review/gate on the shared
  greenfield scaffolding.
- **2026-11-13, cynefin family renderer (7 whole draws, D-187,
  predating its commit).** Thirteenth greenfield family: cynefin-beta is a
  strict 0/2 two-space indented frame — five domain keywords, quoted items,
  title/accTitle/accDescr directives and labeled domain transitions —
  transcribed verbatim with indentation preserved. The live fence-tolerant
  census gives 7 clean / 5 frontmatter-gated / 0 body from a candidate pool
  of 12 (the five carry config/theme/look keys: seed, boundary amplitude,
  width/height, showDomainDescriptions, per-domain themeVariables).
  (1) Rule 18 bit again and the gate caught it before any commit: the
  first ledger draft assumed all seven frames draw at 40 columns, but the
  live CellCapture measurement showed only four do — the transition,
  accessibility and self-loop fixtures have longest lines 50/74/45
  columns, so the wrapped native frame coincides with the text surface and
  stays source on the narrow track (the four narrow-draw frames top out at
  23-28 columns). Width pins are measurements, never guesses. (2) The
  independent review opened REQUEST CHANGES on one real MAJOR: YAML allows
  whitespace before a mapping colon (`config : 42`), and the frontmatter
  key scanner cut at the colon without trimming the key tail, so an
  unrenderable visual contract could draw (fail-open). The one-line fix
  trims the key after the cut; three RED-then-GREEN pins lock it. The
  same untrimmed `substr(0, colon)` key pattern is inherited by twelve
  earlier greenfield renderers with no live trigger — recorded as a
  separate cross-family hardening follow-up, not widened into this
  family's blast radius. (3) Two self-inflicted process slips, both
  cheaply caught: a backgrounded `cmake --preset` inherited a cwd drifted
  into build/debug/tests/ui and failed reading CMakePresets.json while the
  task wrapper still reported exit 0 — always prefix an absolute `cd` and
  verify the in-log RC, not the task exit code; and a misremembered gtest
  filter (`MermaidOfficial.OfficialMatrix` instead of the actual
  `MermaidCapabilityReport.OfficialMatrix`) ran zero tests and nearly led
  to copying a stale matrix — confirm "1 test" actually ran before
  trusting an output artifact. No new numbered rule; rules 18, 20 and 22
  cover the renderer work. No frozen contract moved; flow 160/240, body
  372/173/48, Class285, the 48-gap roster and observation total 2738 all
  unmoved. Throughput: +7 draws for one census/ledger/review/gate on the
  shared greenfield scaffolding.
- **2026-11-13, wardley family renderer (6 whole draws, D-187,
  predating its commit).** Fourteenth greenfield family and the richest
  grammar so far: wardley-beta is a strict 0/2 two-space map frame with
  thirteen validated statement shapes (title/size, anchor/component with
  float-pair coordinates and optional integer label offsets, single-float
  components inside pipeline blocks, four link operators plus a
  custom-labeled form with one-per-line semicolon continuations,
  evolve/note/annotation/evolution/deaccelerator directives and balanced
  non-nesting pipeline braces), transcribed verbatim with indentation
  preserved. The live fence-tolerant census gives 6 clean / 4
  frontmatter-gated (the theme-test maps) / 0 body from a candidate pool
  of 10; fixture 6's only non-ASCII byte (an em-dash) lives in its
  frontmatter description, so the body stays pure ASCII.
  (1) Rule 18 held again: the narrow-40 ledger was left empty and
  populated only from a live CellCapture probe, which showed just the two
  compact frames (longest line 33 columns) draw narrow while the four
  coordinate/note-heavy frames (lines up to 104 columns) stay source —
  width pins remain measurements, never guesses. (2) The renderer was
  written after the cynefin `config : 42` MAJOR, so its frontmatter key
  scanner trims before comparing from the first draft; the independent
  review was explicitly asked to re-attack that seam. (3) The background
  preset cwd-drift failure from cynefin RECURRED here — a background
  `cmake --preset` again inherited a cwd left inside build/debug/tests/ui
  and failed reading CMakePresets.json while the task wrapper reported
  exit 0. Because this is now a twice-repeated rework loop it is promoted
  to numbered rule 25: prefix every build with an absolute `cd` to the
  repo root, trust an explicit in-log RC marker rather than the task exit
  code, reset the foreground cwd after any ctest run, and confirm a gtest
  filter actually ran one test before trusting its output. Rules 18, 20
  and 22 cover the renderer work; rule 25 covers the build hygiene. No
  frozen contract moved; flow 160/240, body 372/173/48, Class285, the
  48-gap roster and observation total 2738 all unmoved. Throughput: +6
  draws for one census/ledger/review/gate on the shared greenfield
  scaffolding.
- **2026-11-13, railroad family renderer (8 whole draws, D-193,
  predating its commit).** Fifteenth greenfield family and the first
  multi-dialect grammar: four headers (IR nested calls plus ABNF/EBNF/PEG
  textual notations) behind one enum, each validated by a dialect-specific
  recursive-descent parser and transcribed verbatim. The live
  fence-tolerant census gives 8 clean / 1 frontmatter theme-gated / 0 body
  from a candidate pool of 9; the family's only non-ASCII byte is an
  em-dash in one frontmatter description so every body is pure ASCII.
  (1) A real fixture caught a real state-machine bug on the first run: the
  multi-line IR rule's closing `) ;` line sits on column 0, and the
  initial column-based rule boundary treated it as an illegal new rule and
  rejected the whole fixture. The reliable boundary is the ';' terminator,
  not the column — a column-0 line may either open a rule or close a
  multi-line one, while an indented line is a continuation only inside an
  open IR rule. Only the real-fixture CellCapture run exposed it; the
  synthetic happy paths did not. (2) Rule 18 held again: the narrow-40
  ledger was filled only from a live probe, which showed just the two
  frames whose every line is <=40 columns draw narrow (2/8). (3) A C++
  test-literal trap: `"\x7fb"` is one out-of-range hex escape because `\x`
  consumes every following hex digit, not one byte — split adjacent
  literals (`"\x7f" "b"`) to bound the escape; the compiler caught it, but
  it cost a build round-trip. (4) Rule 25's cwd-drift failure recurred a
  THIRD time even though the rule was codified during wardley: a
  backgrounded matrix build again omitted the absolute `cd` and inherited
  the ctest cwd, failing on CMakePresets.json. Codification alone did not
  prevent it; the mechanical rule is now applied literally — every
  backgrounded configure/build command starts with an absolute
  `cd /home/zhangdi.zode/Develop/Loom &&`, and the in-log RC marker is the
  only trusted status. No new numbered rule beyond 18/20/22/25; the parser
  work is covered by 20 (every branch advances) and 22 (single guarded
  push). No frozen contract moved; flow 160/240, body 372/173/48,
  Class285, the 48-gap roster and observation total 2738 all unmoved.
  Throughput: +8 draws for one census/ledger/review/gate on the shared
  greenfield scaffolding.
- **2026-11-14, radar family renderer (3 whole draws, D-196,
  predating its commit).** Sixteenth greenfield family, single header
  radar-beta with a one-level curve block: axes (identifiers with
  optional Id["label"] aliases), curves as inline numeric arrays or
  braced named `ident uint`/uint lists, and a closed scalar whitelist,
  all transcribed verbatim. The live fence-tolerant census gives 3 clean
  / 3 frontmatter config/theme-gated / 0 body from a candidate pool of
  6; the only non-ASCII bytes are in a gated frontmatter so every clean
  body is pure ASCII.
  (1) The independent review earned its keep again: it opened REQUEST
  CHANGES on a real MAJOR the happy-path tests missed. The value-list
  parser treated every empty comma slot as skippable, so `{1,,2}`,
  `{,1}` and `{1,,}` all drew even though only one optional TRAILING
  comma is grammar-legal — 4,980 distinct fail-open shapes across a
  26,334-case differential. The fix makes an empty slot legal only as
  the final piece after a real element; one coherent rule now covers
  inline and block forms. The companion MINOR (an empty multi-line block
  drew while inline `{}` failed) is closed by requiring at least one
  value line before a block can close. Lesson: a "skip empty token"
  branch must distinguish a single terminal trailing delimiter from an
  empty slot anywhere in the sequence — position-aware, not blanket.
  (2) Rule 18 held: the narrow-40 ledger was filled only from a live
  probe, showing the two <=23-column smoke frames draw while the complex
  frame with a 46-column line stays source (2/3). (3) Rule 25 hygiene
  was applied mechanically this time — every backgrounded build started
  with an absolute `cd` and was trusted only via its in-log RC marker,
  so no cwd-drift rework recurred. No new numbered rule; rules 18, 20,
  22 and 25 cover the work. No frozen contract moved; flow 160/240, body
  372/173/48, Class285, the 48-gap roster and observation total 2738 all
  unmoved. Throughput: +3 draws for one census/ledger/review/gate on the
  shared greenfield scaffolding.
- **2026-11-14, packet family renderer (4 whole draws, D-196,
  predating its commit).** Seventeenth greenfield family and one of the
  simplest grammars: two interchangeable headers (`packet` and
  `packet-beta`), a strict two-space grid, and only two statement shapes —
  a title and a field whose position is one uint or a uint-uint range,
  followed by a colon and one quoted printable-ASCII label. No blocks, no
  recursion. The live fence-tolerant census gives 4 clean / 2 frontmatter
  config-gated / 0 body from a candidate pool of 6; every body is pure
  ASCII (the family's em-dashes live in two frontmatter descriptions).
  (1) The independent review approved with zero MAJOR on the first pass —
  58 attack vectors, 103 boundary cases and 900,000 fuzzed/mutation inputs
  against a separate spec oracle produced zero divergence. It also caught
  an imprecision in my own recon note: I had said the non-ASCII byte was in
  one clean fixture, but there are em-dashes in two frontmatters (one
  clean, one gated); the substance (all bodies ASCII) was right and no
  code changed, but the ledger/commit wording was corrected. (2) Rule 18
  again refused to be guessed: the recon speculated narrow-40 might not be
  all-four, but the live probe showed every clean frame draws narrow
  because the longest line is 34 columns — width pins stay measured.
  (3) Rule 25 hygiene held for the whole family; every backgrounded build
  used an absolute `cd` and was trusted only via its in-log RC, with no
  cwd-drift rework. No new numbered rule; rules 18, 20, 22 and 25 cover
  the work. No frozen contract moved; flow 160/240, body 372/173/48,
  Class285, the 48-gap roster and observation total 2738 all unmoved.
  Throughput: +4 draws for one census/ledger/review/gate on the shared
  greenfield scaffolding.
- **2026-11-14, eventmodeling family renderer (6 whole draws, D-196,
  predating its commit).** Eighteenth greenfield family and the first whose
  entire candidate pool is clean: 6 clean / 0 gated / 0 body, so the whole
  family draws. The grammar uses a distinct column layout — top-level `tf`,
  `data`, `{`, `}` at column 0 and data-block entries at indent 2 — rather
  than the two-space statement grid of the recent families, which is why
  the state machine tracks a separate block-open/expect-brace pair.
  (1) The independent review again earned its keep: it opened REQUEST
  CHANGES on two real MAJORs the happy paths missed. The dotted-name
  validator checked each character but not the start of each segment, so
  `1A`, `A.1B` and a digit-led data key drew even though the grammar only
  allows identifier-led segments; and an empty data block drew because the
  close brace was unconditional. Both are now closed with regression pins,
  matching the radar family's per-segment and block-has-entry discipline.
  The same pass tightened three MINORs (one inline block / one reference
  per frame, single trailing comma, no orphan data header). A 680,000-case
  differential confirmed every changed decision was tightening-only. (2)
  Rule 18 held: all six frames draw at 40 columns only after a live probe
  (longest line 37), not by assumption. (3) Rule 25 hygiene continued to
  hold for builds, though a direct ctest still drifts the foreground cwd,
  which is reset immediately afterward. No new numbered rule; rules 18, 20,
  22 and 25 cover the work. No frozen contract moved; flow 160/240, body
  372/173/48, Class285, the 48-gap roster and observation total 2738 all
  unmoved. Throughput: +6 draws for one census/ledger/review/gate on the
  shared greenfield scaffolding.
- **2026-11-14, requirement style renderer (4 whole draws, D-204,
  predating its commit).** First extension of an existing greenfield renderer
  instead of a new family TU, and the first task whose need crossed into a
  shared module (the Class/ER/State style seam). (1) A Python text census
  produced 84 false "no-blocker" buckets because it cannot see parser
  reachability; the trustworthy partition only came from a native probe
  driven through the public testing seam (classify_mermaid + layout_succeeds)
  with cumulative stage attribution. Rule 18 again: never census a parser's
  accept set by grepping its input. The probe also corrected a recon guess —
  lowercase `docref` was never a blocker because the field key match folds
  case — and proved all 156 residuals carry a real config/theme gate, giving
  source evidence for the hard wall instead of a guess. (2) The independent
  review earned its keep with a MAJOR the char-vector cohort could not see:
  font-weight reached the disclosure legend but never the pixels (the
  effective-property merge dropped the new field and the paint guard skipped
  bold-only nodes). CellCapture compares glyphs, not cell attributes, so a
  style-semantic renderer needs explicit pixel-attribute assertions
  (PixelAt().bold counts), not just char equality. The new
  FontWeightReachesBoldPixels gate is the durable pin. (3) A stale-link near
  miss: a full-matrix run used a binary linked before the MAJOR fix. The
  disposition counts stayed valid because bold is pixel-only, but the
  authoritative debug/release byte gate was re-run only after rebuilding the
  matrix target and confirming its mtime post-dated the static library —
  the existing "never trust a target-only matrix verdict" rule, applied
  deliberately. (4) Shared-seam containment: the change was an append-only
  ClassStyleProperties field the Class grammar never sets plus a single-
  sourced extraction of the color helpers; the 256-test relations-semantics
  binary (debug and ASan) and a byte-identical dual-preset matrix prove the
  Class/ER/State callers are unchanged. No new numbered rule; rules 18, 20,
  22 and 25 cover the work. No frozen contract moved; flow 160/240, body
  372/173/48, Class285, the 48-gap roster and observation total 2738 all
  unmoved. Throughput: +4 draws for one census/ledger/review/gate on the
  shared scaffolding, with one shared-module change paid for by a full
  caller regression rather than a new isolated TU.
- **2026-11-14, agentflow family renderer (27 whole draws, D-211,
  predating its commit).** Nineteenth greenfield family and the largest
  single-family draw since gantt, from a flowchart-syntax-variant pool that
  could only be recovered by modeling its real grammar, not by routing it to
  the flowchart renderer. (1) Rule 18 again defeated a text-only census:
  grepping the 38 fixtures for feature keywords could not tell whether the
  parser would accept each construct; the trustworthy 27-native/11-gated
  partition only came from a per-fixture probe through the public
  classify_mermaid + layout_succeeds seam, and the same probe drove every
  parser iteration (frontmatter fence, dedent-vs-end, bracket endpoints,
  arrow offset arithmetic) instead of guessing why a source rejected. (2) The
  independent review caught a real MAJOR the happy-path fixtures never
  trigger: metadata duplicate-key detection used a block-local bitmask, so
  the same key across two `@{ }` blocks silently overwrote a shape and
  emitted duplicate string rows. The durable fix is an element-persistent
  key mask, which also made the quote-aware component splitter and the
  node-only shape owner rule fall out of the same closed-vocabulary
  discipline. (3) Two classes of edge-tokenizer defect recurred while
  hand-rolling offsets (a substr length counted a bare id's whole line, and
  a pipe-label offset double-counted the arrow prefix); each threw only on
  synthetic inputs, which is why the per-feature mini-probe, not just the
  corpus run, is the cheapest way to catch offset arithmetic. (4) No new
  numbered rule; rules 18, 20, 22 and 25 cover the work, and rule 25's
  absolute-cd/RC-marker discipline held across debug, release and ASan
  trees with one writer per tree. No frozen contract moved; flow 160/240,
  body 372/173/48, Class285, the 48-gap roster and observation total 2738
  all unmoved. Throughput: +27 draws for one census/ledger/review/gate on
  the shared scaffolding.
