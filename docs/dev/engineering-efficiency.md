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
