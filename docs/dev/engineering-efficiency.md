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

26. **A frozen closed-profile that flips from fallback to drawn can have
    several independent root causes; and a throwaway census's absolute count
    is not comparable to a doc baseline, only its same-probe transition set
    is.** In D228 the StateScoped closed-profile failure survived the first
    MAJOR fix (the inline `Node:::class` edge guard): a second, unrelated
    mechanism — a state-local named `red` admitted in the direct color table
    though no reachable fixture spells a direct `fill:red` — was what drew
    the SHA-pinned `unknown_style` source. When a frozen gate stays red after
    the obvious fix, extract the exact failing source's tokens and diff them
    against the reachable fixtures' actual spellings before assuming one
    patch covered it; each distinct admission path is a separate root cause.
    Separately, a render seam probed at one capture height produced an
    absolute drawn count offset by a constant from the living doc's
    different-geometry census, which first read as a phantom extra draw. The
    trustworthy signal was a same-probe HEAD-vs-working-tree run over the
    whole family (stash, rebuild, re-probe, restore): the TRANSITION set was
    exactly the four authorized fixtures with an empty regression set, while
    either absolute number alone was misleading. Update the doc by applying
    the measured delta to its own baseline, never by copying the probe's
    absolute total.

27. **A gtest discovery failure does not just false-red a build — it can
    silently shrink the registered test denominator, producing a misleading
    "100% passed" that skipped whole binaries.** When the gtest_discover_tests
    step cannot load `libssl.so.3` (the rule-5 trap), ninja reports a failure
    but the binary is still built; ctest then simply has fewer tests
    registered. In D235 the same tree reported 1007, then 1075, then the true
    1357 mermaid tests depending solely on how many discovery steps had failed
    — every run printed "100% passed", so the under-count was invisible
    without comparing against `ctest -N`. Before quoting any full-suite
    number, force complete discovery by building WITH the
    `LD_LIBRARY_PATH` prefix (zero `error while loading` lines) and confirm
    the denominator with `ctest -L mermaid -N | grep -c 'Test #'`; never
    compare efficiency across two runs whose denominators differ. A partial
    green is not a family-closure gate.

28. **A census probe must use the EXACT predicate of the gate it feeds, and a
    dual-preset byte-identity mismatch after a source change is usually a
    stale link, not a real divergence.** In D236 two throwaway probes used a
    looser "rendered cells differ from the text control" test than the cohort
    gtest's `drawn_native`, which also requires the frame to contain no raw
    diagram keyword; the loose probe over-claimed rows as drawn that the gate
    classifies as source, producing two wrong ledger partitions (9/7 then
    13/3) before the gate-identical predicate gave the honest 11/5. When a
    ledger's draw/block split is renderer-decided, derive it with a probe that
    mirrors the production gate condition line for line, and treat any
    disagreement as a probe defect, never as a reason to weaken the gate.
    Separately, the first D236 debug/release OfficialMatrix byte comparison
    failed because the release binary had not been relinked against the changed
    `test_mermaid_official.cpp` (its matrix kept the prior sha); a forced
    rebuild + re-run made the sha match with no code change. After editing a
    test that writes a byte-pinned report, rebuild that exact target in BOTH
    presets and re-run before comparing shas; a one-preset-old report sha is a
    build-currency problem, not a renderer non-determinism.

29. **A cohort pixel gate, an independent review and a graduated family
    semantics suite can ALL be green while the OfficialMatrix capability
    report still rejects the change — check the family's frozen whole-null
    authority BEFORE building, not after the cohort/review investment.** D237
    admitted ER subgraph group-endpoint edges behind an ER-local parser change
    and got a renderer-free cohort ledger (5/5), an APPROVED independent
    adversarial review and a fully green `relations_semantics` (269/269 after
    graduating two frozen obligations with framework-derived Models) — then
    `MermaidCapabilityReport.OfficialMatrix` failed because every ER original
    is enumerated through the frozen ER206 whole-null authority, whose
    retained-null expectation the six new draws violate
    (`...never admission`, errors 0->6, gap roster 48->54). ER has no cohort
    dispatch escape (unlike E2a/E2b/E2c); the only legal draw path is the
    ER107 supplemental whole-Model re-baseline (facts/contract/source-helper
    triple SHA, `array<bool,107>`, dual `kPins`), a separately-authorized
    milestone of the same class as the D-120 class-link block. Rule 12 says
    check the frozen contract before building; rule 29 sharpens it: before
    scoping ANY renderer-admission cohort, grep the matrix dispatch for a
    family-specific retained-null authority (ER206/ER107, Class285, the E6c
    roster) and classify the work up front as either an in-authority cohort
    admission or a re-baseline milestone. A cohort gtest and an agent review
    prove rendering fidelity, never authority compliance; the full
    `MermaidCapabilityReport.OfficialMatrix` is the only gate that proves the
    latter, so run it before treating a green cohort as shippable.
30. **A byte-exact corpus differential proves equality to the frozen fixture,
    not faithfulness to the production grammar — a reference model and its port
    share the same blind spot, so cross-check every charset and boundary flag
    against the production parser's own tables, and prove the diverging input is
    corpus-absent before accepting a bounded-envelope reject.** D251's parallel
    ER lexer matched all 107/107 originals (4222 witnesses, byte + sha256) yet
    the independent review found a systematic marker-boundary divergence: the
    throwaway Python reference inferred a word boundary from the token's first
    byte, and the C++ port copied it, so symbolic markers that start with a word
    byte (`o|`, `o{`) and the `boundary=false` textual forms (`1+`, `0+`,
    `many(0/1)`) were over-constrained versus production's per-entry flag table.
    The corpus cannot expose it because every marker is followed by a space;
    the same class of corpus-invisible gap recurred for digit-leading
    attributes, lone tildes, reserved names, subgraph frame depth and quoted-
    content validation. The green 107-case gate was necessary but never
    sufficient. Rules: (a) when porting a parser as a parallel witness, diff
    your marker/keyword tables row-by-row against the production entry structs
    (here `er_cardinality`'s `{text, marker, word}`), never re-derive a
    boundary rule from token shape; (b) for every grammar surface you choose not
    to model, first run a byte census proving it occurs zero times in the
    corpus, then fail the whole input closed at an envelope guard rather than
    emit a divergent span, and record it as a documented limitation; (c) keep a
    throwaway cross-check reference in /tmp for fast rule iteration, but the
    durable gate is the native gtest over the real compiled seam with sha256 —
    and an independent adversarial review that reads production source, not the
    reference, is what actually catches the shared blind spot.
31. **For a large deterministic generator, write the leaf-level derivation
    spec and the canonical-equality harness BEFORE production C++, and reuse an
    admission helper at the third consumer instead of copying it.** D271's
    16-field source_model producer (~1100 new lines) reached 107/107 canonical
    byte-equality plus pinned sha256 on its FIRST full test run, with only two
    trivial `-Werror` build iterations (an unused variable, a mis-scoped
    anonymous namespace). The enabling work was upstream, not in the compiler
    loop: a read-only subagent produced an 829-line /tmp catalog cross-checked
    by independent `python3 -I` probes (marker/ordinal/YAML/ordering rules),
    and the differential compared whole canonical strings + the content-
    addressed digest rather than hand-asserting each leaf. Rules: (a) when a
    third parallel seam needs the same input admission, extract one helper
    immediately — the D271 envelope refactor was behavior-preserving and
    covered by the two existing 107-case differentials, so de-duplication was
    near-free; (b) make the fixture's own canonical hash the test oracle so a
    single comparison covers every field and array order at once; (c) keep
    roster counts (391/362/253/287/181) as explicit tripwires. Rule 30 still
    applied at review: the nested-subgraph frame stack and literal-block `|`
    style were corpus-absent divergences no differential could see, caught only
    by reading production — closed with a fail-closed reject and an
    indicator-preserving fix rather than golden data.
32. **When a "producer" needs data the source cannot provide, make it an explicit
    input rather than an impure read, and treat an apparently-sorted fixture as
    a trap that needs an anti-silence test.** D275's source_binding has 12 of
    20 fields that exist nowhere in the raw .mmd (manifest/prior cross-refs,
    component digests). The correct seam is still pure:
    `er_source_binding_json(source, work, const ErSourceProvenance&)` with every
    provenance field std::optional, emitting explicit JSON null when unset and
    never touching fs/manifest/git/env; content-hash fields are independently
    recomputed by the gtest as tripwires. Two traps bit during the work:
    (a) the build/ tree was wiped mid-session, so a ninja failure surfaced as
    "build/debug is not a directory" rather than a compile error — distinguish a
    missing build tree from a code break and reconfigure before debugging code;
    (b) gtest discovery still failed on libssl after a clean tree, and a normal
    build stopped mid-way leaving some targets unlinked — `cmake --build ... --
    -k 0` keeps linking past the discovery failure so every binary exists for
    direct LD_LIBRARY_PATH runs. The canonical writer's object-key sort was
    invisible to the 107 differential because every frozen object is already in
    alphabetical order; the durable guard is a synthetic out-of-order-key
    source whose hash only matches when sorting actually happens (a no-sort
    writer would pass all 107 yet fail this one test). Rule 30 again: the
    unescaped-key and dead-bool-return defects were corpus-invisible, found by
    reading the writer, not the differential.

33. **When an RFC pins an explicit per-family / "at minimum" probe list, treat
    that list as the authoring checklist for the test, not as background.** RFC
    0040 §3 enumerated the probes the cross-family battery must cover "per
    family". The first gate draft implemented the full set only for timeline;
    gantt and journey carried table membership plus two negatives, so a later
    edit to a single TU's comment-stripping, dequoting, unmatched-quote or
    indent-4 logic would have passed every test — the exact drift the battery
    exists to prevent. The adversarial review caught it (MAJOR), costing a test
    rewrite plus a full rebuild/rerun cycle. A shared battery helper is only a
    drift guard when it is *executed per TU*: one `FamilyBattery` table +
    `run_battery()` run once per family, with literal verdicts and literal
    footer blocks, so no family can be silently weaker. The same review round
    also showed the value of pinning the footer as a block (blank separator +
    header + leaf order) rather than as substrings.

34. **A body-equivalent twin does not predict the admitted row's narrow state.** A
    disclosure footer is part of the whole-surface 40-column predicate, so inheriting
    the corpus twin's `narrow_40` is invalid: in RFC 0041 the footer's longest line
    exceeded a 40-column viewport and flipped the five container-kind rows from draw
    to fallback, falsifying the design-time prediction (5 draw) — the measured result
    is 0 of 9. Rule: after adding a footer (or any trailing rows), re-measure every
    admitted row at 40 columns live, record the measured state, and never derive
    `NARROW_DRAW` from a twin. The PRR's "measure, never transcribe" requirement is
    what caught it before the closure.

35. **A hand-copied fixture path is a silent trap; existence-check every pinned list and
    share one list between the generator and the gate.** In RFC 0042 the Pie7 set typed into
    the new gate test had `should-render-a-pie-diagram-with-capital-letters...` where the
    corpus says `should-render-a-simple-pie-diagram-with-capital-letters...`. The gate then
    reported "drawn row must draw" for a row it had never loaded (empty read → fallback),
    and a throwaway diagnostic probe repeated the same typo, so the first diagnosis was
    "the renderer does not draw this row" instead of "the path is wrong". Rule: every
    hardcoded path list is asserted against the corpus (the generator already does
    `PIE7 <= originals`), the gate recomputes the same check, and a diagnostic that reports
    an empty source should print the byte size before any conclusion.

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
6. **Giant frozen gates cannot run under ASan inside a practical closure
   window — a test-architecture defect, not an acceptable steady state
   (logged 2026-11-16, D282/D284).** Two whole-source capability gates are
   correct but pathological under AddressSanitizer+UBSan. Measured this round
   on the 109 GiB host (so memory is ruled out — peak RSS stayed flat and
   low): `MermaidCapabilityReport.Class285` takes **723.5 s** at **471 MiB**
   peak (vs 8.85 s in release, ~82x) and the three
   `MermaidOfficialConfig110Native.*` tests take **990.3 s** at **730 MiB**
   peak (vs 13.4 s, ~74x); both PASS with zero sanitizer findings. A full
   `ctest -L mermaid` ASan run is killed by the outer task-lifetime harness
   when it reaches Class285 — not by an OOM or a sanitizer abort. A test that
   cannot enter the sanitizer lane is structurally under-verified for memory
   safety. Suggested disposition (separate milestone, do NOT do during a
   feature closure): (a) split each monolithic gate into per-fixture/per-track
   gtest cases so they parallelize and bound per-case time; (b) shrink or
   shard the giant in-memory capture/observation structures; or (c) if a gate
   is genuinely irreducible, mark it explicitly `ASAN_EXEMPT` in code with the
   measured slowdown + the compensating release-serial/byte-matrix coverage
   written next to the marker, rather than silently relying on "too slow to
   run". Until then the D282 closure covers these gates via release `-j1`
   serial runs plus the dual-preset byte-identical OfficialMatrix, both
   executed fresh this round, and covers the changed ER surface (92 tests)
   under ASan.

## Retro log

- **2026-11-22, RFC 0039 gitGraph family (`66d0c942`).** +12 whole draws in one
  atomic phase; census 1419/726 → 1431/714, formal 1233 → 1245, gaps 48,
  dual-preset byte-identical at `9d925815`; the full debug ctest was green on
  one complete run (4631/4631, CTEST_EXIT=0, 268 s). No rework loop after the
  design gate. What to keep:
  1. *Specify the regression oracle before the code, not after.* The gate's
     fifth test began as an unverifiable "E2a byte-identity"; the PRR showed a
     cell-surface comparison cannot see a lone trailing blank row and required
     a full untrimmed surface equality between each drawn row and the same
     source with its frontmatter stripped. Cost of catching it in review: one
     round. Cost of shipping the original: the exact regression the RFC exists
     to prevent would have passed green.
  2. *A "native-drawn" mnemonic is a cross-module contract.* Three gates use
     `find("gitGraph") == npos`; a lowercase footer header would have made 12
     promoted rows read as fallbacks in any gate reusing it. Rule: before
     emitting a new user-visible footer string, grep the tree for existing
     substring predicates over the same text.
  3. *Automate the reviewed pin.* The cohort digest lives in the generator as a
     reviewed constant; until this phase it was enforced only by a manual
     `--check`. A hermetic `add_test` running `--check` closes that gap for
     free (`MermaidGit0039Cohort.GeneratorIsReproducible`). Follow-up debt: the
     0037 and 0038 generators still rely on the manual step.
  4. *Probe predicates must match the census predicate.* The D-402 probe's
     first version compared `Screen::ToString()` (foreground colours included)
     and reported 318 fallback rows as "drawn"; the matrix compares character +
     background. Rule: reuse the matrix predicate (cell surface) for any
     "does it draw" measurement.
  5. *Release-primary again:* release matrix 27 s vs debug 239 s for a
     byte-identical artifact; the mermaid label ran once in debug (1444 tests,
     3530 s) and the full suite once at closure.
- **2026-11-22, RFC 0038 state look/theme family (`648d7035`).** +28 whole
  draws in one atomic phase; census 1391/754 → 1419/726, formal 1205 → 1233,
  gaps 48, dual-preset byte-identical at `59cf7de8`. The phase ran front-to-back
  without a rework loop after review (approve → regenerate → rebuild → both-preset
  matrix → roster `--emit`, i.e. RFC 0037 rule 5 applied in order). Loops and
  costs worth keeping:
  1. *A render-defined split must be pinned, never classified.* The counterpart
     of RFC 0037 rule 1: here a source-shape classifier over-selects by 16 on the
     63 look/theme rows, so the 28-row cohort is a reviewed literal path set with
     one whole-cohort digest, and the generator asserts only source-derivable
     counts (145/28/4/4/1). Rule: when the split depends on rendering, pin the
     set and pin the digest — do not derive it.
  2. *Every existing whole-source scan is a coupling.* The design review found
     the E2b negative; the PRR found the E2a negative, whose floor silently broke
     at 71−28=43 against `EXPECT_GT(50)`. Rule: grep the tests for the family's
     fixture roots, not for the feature's name.
  3. *A twin-precedence rule needs a measured "none" branch.* Corpus twins were
     impossible on this family (every body-equivalent drawn state sibling carries
     `logLevel`/`fontFamily` lines that cannot prefix the source footer). Rule:
     record the measured reason in the ledger and the generator doc instead of
     forcing a weaker comparison.
  4. *Load flakes need the isolation re-run before they count.* The `-j$(nproc)`
     full run produced two timeouts — `ServerMain.DirectConnect*` (documented)
     and `Tools.RuntimeTeamCreate...` (mock-server request-count wait) — both
     green alone (0.47 s / 0.17 s). Rule: re-run failing timing tests serially
     and record both numbers; never widen a timeout to make the run look green.
  5. *Release-primary paid off again:* the matrix cost 27 s in release versus
     240 s in debug (8.9×) for a byte-identical artifact; iteration used release,
     and the full debug ctest ran at closure.
  6. *A red full gate is not laundered by a partial rerun.* The first complete
     run ended `CTEST_EXIT=8` (two 3 s-TCP/mock-server timing failures); the
     second complete run ended `CTEST_EXIT=8` again with two *different* timing
     failures. The initial write-up called both "reruns passed", which the Tech
     Lead rejected. Rule: the full gate's verdict is one complete run; a failing
     item is either fixed or backed by **>= 3 consecutive clean reruns plus a
     surface-unrelatedness argument** (file diff, symbol references, failure
     mechanism), and the closure record states the red run's exit code verbatim.
- **2026-11-21, RFC 0037 requirement family (P1 `e645212f`, P2 `c42647ad`, P3
  `bd542e71`).** +120 whole draws in three phases; census 1271/874 → 1391/754,
  formal 1085 → 1205, gaps 48, dual-preset byte-identical at each phase. Rework
  loops, all caught before shipping:
  1. *A derived artifact must not be the generator's population.* The first
     generator enumerated `fallback_roster.json`, so the phase's own `--emit`
     migration removed the promoted rows and made the pins self-destruct. Rule:
     classify the family by source shape over the fixture tree (172 originals),
     never over a roster the phase itself shrinks.
  2. *Define the twin over all admitted keys.* "Same source with the added key
     removed" leaves an F0 footer for the 8 `look`+`htmlLabels` rows and
     double-counts it; the design review caught this before code. Rule: the
     twin is the config-free source, with the footer delta recorded per row.
  3. *Reuse the stricter sibling decoder.* P3's first draft decoded the title
     with the config-leaf helper (trim + dequote), so `true`, `123`, `a: b`,
     `[x]` and unbalanced quotes were admitted — wider than the pinned ER rule.
     Rule: call the sibling family's decoder and add one negative per rejection
     class it owns.
  4. *Negatives must be well-formed up to the single violated rule, again.*
     P1's first title negatives omitted `description:`, so they rejected at the
     description gate, not the title branch. Rule: add the positive control
     that removes only the violated rule and still draws.
  5. *`require()` in the matrix collection loop truncates the artifact.* A
     pending-ledger run throws, then reports `observations.size() != 2738` and
     overwrites the matrix JSON with a truncated report. This is the deliberate
     fail-closed norm for cohort loaders. Rule: after a phase is approved,
     rerun the matrix in both presets before emitting the roster.
  6. *Bookkeeping dominated the findings again.* 13 of 19 review findings
     across the design, PRR and three phase reviews were record drift
     (disclosure string, comments, test counts, flip counts, coupling rows).
     Rule: when a phase changes a user-visible string, a test count or an
     assertion's phase, update the RFC and the generator note in the same edit,
     then rerun `--check` and `rfc_lint.py`.
  7. *Never pipe the full suite through a formatter.* Running
     `ctest --preset debug -j$(nproc) | tail -12` returned `tail`'s exit code
     and dropped the summary line, so a green-looking tail could not prove the
     run's verdict and the suite had to be repeated. Rule: redirect the full
     suite to a log file, append `CTEST_EXIT=$?`, and read the verdict from the
     file.
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
- **2026-11-14, sankey family renderer (1 whole draw, D-211,
  predating its commit).** Twentieth greenfield family and the smallest
  draw of the program: an 18-fixture pool where only one original is
  reachable and the other 17 are a config-contract hard wall. (1) Rule 18
  (measure, don't guess) produced a second recorded defeat for text
  censuses: an earlier keyword census had claimed swimlanes split 41/5
  across two header dialects; the production-seam re-probe showed 46/46
  single-dialect `swimlane-beta`, every one carrying the same
  htmlLabels + useMaxWidth double contract. The trustworthy 1-native/
  17-gated sankey partition likewise came only from probing the public
  render_markdown seam, and the renderer-free generator's hard drift guard
  (18/1/17) makes any future corpus drift fail the ledger check instead of
  silently re-bucketing. (2) The independent review again earned its cost
  on a happy-path-invisible defect: an engine `%%{init ...}%%` directive
  was being swallowed as a plain comment, which no fixture in the pinned
  corpus contains. The one-line reject fix is text-level isomorphic with
  four neighboring renderers, and the two residual C++/Python parser
  divergences were accepted as fail-safe NITs with the discipline recorded
  in code comments rather than patched to parity — reject-only differences
  that are unreachable on the corpus do not justify grammar churn. (3) No
  false green/red or flake occurred: debug, release -O2 and ASan+UBSan
  agreed on the 1/17/0 partition, the dual-preset OfficialMatrix sha
  matched on the first comparison
  (5167f087f20c892429b89b0a7d7e4bc59a4eea96087edac8a824353976bb1d7f), and
  the full 1339-test mermaid suite plus the `-j1` four-gate serial run
  (47.29s wall) were green first pass. (4) Interface fan-out was the
  unavoidable kind: one declaration added to the `mermaid_detail.cppm`
  interface unit forces BMI recompiles of its importers exactly as every
  prior family addition did; the renderer itself is a body-only
  translation unit so later edits stay inside the visual archive. (5) No
  new numbered rule; rules 18, 20, 22 and 25 again cover the work, and the
  three build trees kept one writer each with absolute-cd/RC-marker logs
  under `/tmp/g4probe/sankey/`. Throughput: +1 draw and a fully documented
  46-fixture hard wall for one census/ledger/review/gate on the shared
  scaffolding; no frozen contract moved (flow 160/240, body 372/173/48,
  Class285, 48-gap roster, observation total 2738).

- **2026-11-14, D228 state style group (uncommitted at retro; local commits
  follow).** Outcome +4 whole draws (1214 -> 1218 / 931 -> 927), one italic
  hard gate retained. (1) Gate wall-clock was dominated by the two full
  OfficialMatrix runs (debug ~729s, release ~92s) and the 1007-test mermaid
  label suite (374.49s); per rule 2 the release matrix carried the
  byte-identity gate at ~1/8 the debug cost, and iteration used only targeted
  StateStyles/frozen-gate filters. (2) One misleading signal, now rule 26: a
  throwaway census reported an absolute drawn count one above the expected
  delta because its fixed capture height differs from the doc's census
  geometry. The durable fix was a same-probe HEAD-vs-working A/B over all 145
  state originals (stash/rebuild/re-probe/restore), which isolated the
  transition set to exactly the four authorized fixtures and an empty
  regression set; the doc applies the measured +4 to its own baseline rather
  than copying the probe total. (3) The independent re-review earned its cost
  twice: the prior round's single "MAJOR" masked two independent root causes
  — the inline `Node:::class` edge mis-label AND an over-broad state named-
  color table that drew a SHA-pinned closed profile (`style A fill:red`).
  Fixing only the first left the frozen gate red; the second needed a
  corpus-attested context split (red only inside the border shorthand). An
  upstream check — diffing the failing source's tokens against every
  reachable fixture spelling before coding — would have caught the named-red
  overreach in the original implementation instead of at review. (4) No
  false green/red or flake: debug, release -O2 and ASan+UBSan agreed, the
  dual-preset OfficialMatrix sha matched first comparison
  (48ffcbc5fbbe23415e1ab9bef43c003d8f4e61456222fca261c7f99e3a0dfc6c), and the
  only OfficialMatrix redness while verdict=pending was the designed review
  gate (proven: 62/63 with the exact "must be independently approved"
  message), which cleared on verdict flip with no code change. (5) Interface
  fan-out was the known append-only kind in `mermaid_detail.cppm` (three
  structs + four declarations, zero existing types changed); the style logic
  is a body-only state TU so later edits stay inside the visual archive.
  (6) New numbered rule 26 added (multiple independent frozen-gate root
  causes; same-probe transition set over absolute census). The three build
  trees kept one writer each; no frozen contract moved (flow 160/240, body
  372/173/48, Class285 260/25/0, 48-gap roster zero state rows, observation
  2738).

- **2026-11-14, D235 gitGraph empty quoted-tag group (uncommitted at retro;
  local commits follow).** Outcome +3 whole draws (1218 -> 1221 / 927 ->
  924), a complete group smaller than the nominal four (the mandate accepts a
  complete sub-group). (1) Gate wall-clock was again dominated by the two
  OfficialMatrix runs (debug ~732s, release ~90s) and the full mermaid label
  suite (507.50s); release carried the byte-identity gate at ~1/8 the debug
  cost per rule 2, and iteration used only targeted gitgraph/e2a filters.
  (2) A self-inflicted rework: the first take_tag wiring inverted the success
  predicate (`else if (!cursor.take_tag(...))`), which rejected every valid
  tag; caught immediately by the native RED->GREEN run and corrected in all
  three call sites — an upstream check of reading the full if/else chain
  (does a successful helper advance the loop?) before building would have
  caught it in one pass. (3) The independent review earned its cost on
  completeness, not a hidden defect: it returned APPROVED with no MAJOR but
  flagged two stale-doc findings (a companion cohort doc and the generator's
  module docstring still describing the retired detector), both fixed in the
  same pass; it also independently reproduced the whole-corpus A/B and
  proved the lossless framing (empty string = zero glyphs = no dropped
  content, frame-identical to an omitted tag). (4) New rule 27 from a real
  denominator trap: gtest discovery failures silently un-register tests, so
  the "full" suite read 1007 then 1075 then the true 1357 depending on
  libssl discovery health, all reporting 100%; only `ctest -N` against a
  fully-discovered build exposes the real count. (5) No false green/red or
  flake once discovery was forced complete; debug, release -O2 and
  ASan+UBSan agreed, the dual-preset OfficialMatrix sha matched first
  comparison
  (5bcc065116a3e69a64325ca786aa0eaaccd2ba04d7290b743371779f1ca7f5f7), and the
  only red tests while the ledger verdict was pending were the two designed
  review gates, which cleared on approval with no code change. (6) The change
  is a body-only file-local Cursor helper (no interface unit, so no BMI
  fan-out); no frozen contract moved (2145/2738, 48-gap roster, Class285
  260/25/0, flow 160/240, body 372/173/48, E2b state 51/10).

- **2026-11-15, D236 xychart folded-description envelope (uncommitted at
  retro; local commits follow).** Outcome +11 whole draws (1221 -> 1232 /
  924 -> 913), extending an existing envelope rather than writing a new
  renderer. (1) Gate wall-clock again dominated by the two OfficialMatrix
  runs (debug ~739s, release ~92s) and the full 1367-test label suite
  (491.66s); release carried the byte gate at ~1/8 debug cost per rule 2,
  iteration used only targeted chart/e2c filters. (2) Two self-inflicted
  rework loops, both now rule 28: a census probe with a looser predicate than
  the gtest gate (cell difference only, missing the "no raw diagram keyword
  in frame" condition) over-claimed draws and produced two wrong ledger
  partitions (9/7, then 13/3) before a gate-identical probe yielded the
  honest 11/5 — the fix was to mirror the gate predicate line for line, not
  to argue with it. (3) A first dual-preset byte-identity failure was a
  stale release link (the changed test_mermaid_official.cpp had not been
  relinked, so release kept the prior matrix sha); forced rebuild + re-run
  matched immediately, no code change — recorded so a future one-preset-old
  sha is read as build currency, not non-determinism. (4) The independent
  review earned its cost by re-deriving the 16-row selection and 11/5
  partition without reading the ledger row list and by a stash A/B proving
  the only new admissions are description-only (two config-only drawers
  pre-date D236); it returned APPROVED with one doc NIT (a stale "seven"/
  "calibrated axis" narrative), fixed before the verdict flip. (5) No false
  green/red or flake once discovery was complete: debug, release -O2 and
  ASan+UBSan agreed, the dual-preset sha matched
  (c906c74afddc02efa2bc311399ab6d337fb6dcf2318e6528e78d85067e09dc25), and
  the only red tests while the ledger verdict was pending were the designed
  review gates. (6) New numbered rule 28 added (gate-predicate-identical
  census; stale-link byte mismatch). The production change is a body-only
  file-local helper with no Work spend and no interface/BMI fan-out; no
  frozen contract moved (2145/2738, 48-gap roster, Class285 260/25/0, flow
  160/240, body 372/173/48, E2a 59/10, E2b 51/10 byte-identical).
- **2026-11-15, D237 ER subgraph group-edge investigation (all production/test
  code reverted; docs-only outcome).** Outcome: zero shipped draws; a clean
  lossless 7-row ER group was found, fully implemented and proven at the
  renderer/cohort/semantics layers, then correctly rejected by the frozen
  OfficialMatrix ER206 whole-null authority and reverted rather than move a
  frozen gate without authorization. (1) The selection work was sound and
  cheap: cross-family CellCapture scans proved the frontmatter envelope seam is
  exhausted (every remaining envelope-only row carries a non-droppable
  config/color/layout contract; the three config-free exceptions are roster-
  pinned or E2a-adjudicated), and the ER group-edge selector reproduced the
  exact 7 paths over all 206 ER originals with zero missing/extra. (2) The
  expensive rework was building the cohort ledger, the independent review and
  the ER107-style mutation graduation (framework-derived complete Models,
  model SHAs, exact exec/skip spans; 269/269 relations green) BEFORE learning
  the family's matrix path enforces a retained-null authority that has no
  cohort escape. Rule 29 (new) codifies the earlier, cheaper check. (3) A
  second instance of the rule-27 pipe-mask trap: an early full
  relations_semantics run was read as green through `| tail`, hiding the one
  failing ER subgraph gate; the independent reviewer caught it by checking the
  raw exit code. Always capture the binary's exit status directly. (4) The
  blocking matrix failure is a correct fail-closed, not a bug: the error string
  is the architectural invariant `OfficialER206 whole-null ... never admission`,
  and the only legal route (ER107 supplemental whole-Model re-baseline) is a
  separately-authorized milestone identical in shape to the D-120 class-link
  block. The complete patch is preserved under /tmp/d237_probe/d237_backup/ for
  that future milestone; all frozen gates and the 1232/913 counts are
  byte-identical to HEAD (no production, test-target or contract change
  remains in the tree).
- **2026-11-15, D242 ER107 group-edge re-baseline authorization -> deep scope
  (still docs-only; tree green at 9e4bb79b/1232-913).** TechLead D242 explicitly
  authorized the local ER107 whole-Model re-baseline for the six group-edge
  originals, so the work moved from "blocked pending authorization" to a
  generator-feasibility question. Investigation proved the production
  representation is sufficient (the ER-local parser already renders all six
  losslessly; two canonical whole-scene models pass `validate_impl`); the real
  wall is that the ER107 facts/proposal assets have NO in-repo producer and no
  Model->canonical-JSON serializer — `bind_case` only re-derives facts to
  validate, never emits, and git history has no generator. Each new case is a
  byte-exact ~20KB terminal_model + ~1.9KB 20-field binding + 5 SHAs, validated
  by opaque `require()`s (exact display reconstruction/joiner/byte offsets), so
  a proposal EMITTER over the oracle's already-derived `Projection`/capture
  structs is the missing, dedicated piece of tooling. Blast radius was measured
  and is contained: per-case SHAs are content-local (existing 107 pins stay
  valid); `shared_contract` (20KB) and `palette_source_and_projection` (7KB)
  carry zero case references and stay byte-stable, as do presentation (fixed
  17 upstream files), the contract doc and the source helper — so only 2 of the
  9 pinned SHA constants move (kFactsSha, kProjectionSha), plus the dual
  kPins 107->113, five hardcoded-107 sites, the 7 component_hashes/counts/
  selection aggregates and verify_er107_admission.py's SELECTED/admission math/
  BASELINE_MATRIX_SHA. No partial lands (parser alone = 6 matrix errors, roster
  48->54), so the emitter + parser must be one atomic green change with an
  independent B3 review. Re-confirmed no lighter in-contract cohort remains:
  info(2) is a version-banner meta-directive with no projectable diagram,
  zenuml(1) a single-file new grammar, katex(4) needs a math engine, all other
  Part-B groups are visual/config contracts or the same frozen authorities.
  Decision recorded for the next milestone rather than forcing a non-green or
  expectation-only change (D242 forbids both); seed artifacts in
  /tmp/d237_probe/d237_backup/.
- **2026-11-16, D248/D251 stage-3 structural-witness seam (local, green; 69/69
  subset incl OfficialMatrix).** Two self-inflicted rework loops, both recorded
  so they do not recur. (1) **Byte-vs-code-point false defect.** A case was
  briefly logged as a "stale offset" (`byte_end` 202 vs a reported 196-byte
  source) and nearly written into the contract as a 106/107 golden exception.
  The 196 was Python `len(str)` counting Unicode code points; the file is 202
  UTF-8 **bytes** (three U+2192 arrows, `E2 86 92`, +6 bytes). Rule: when
  validating byte offsets/lexemes, measure with byte-aware tools (`wc -c`,
  `len(bytes)`, `.size()` on the byte string), never a decoded code-point
  length; a pinned `byte_end == byte file size` is consistent by construction.
  The production seam uses `string_view`/`size()` and matched 107/107 (2394
  witnesses, zero tiling gaps) with no golden-fitting once the measurement
  error was removed. (2) **Dangling by-value JSON handle in the test, not a
  production bug.** Storing `&c` where `c` is the by-value `JsonVal` argument
  of an `iter` lambda dangles after the lambda returns; the reused address
  resolved a different (265-byte ASCII) case, which surfaced as an impossible
  "no multibyte content" failure. Rule: copy the needed scalar (`std::string`
  path) out while the handle is live; never hold a non-owning view/pointer to a
  serdes handle across an iteration boundary. A standalone throwaway Python
  re-derivation of the seven span rules in /tmp cross-checked all 2394
  witnesses in seconds and was the fastest way to separate harness defects
  from production correctness; the durable regression stays in the native
  gtest (production seam + loom.crypto slice SHAs), per the no-text-matching
  policy.
- **2026-11-16, D251 parallel 24-kind ER semantic witness lexer (test-only
  additive seam; no production change).** Outcome: 107/107 ordered byte-exact
  differential (4222 semantic witnesses across 24 closed kinds, all nine fields
  incl. recomputed sha256), debug+release 9/9, adjacent cluster 291/291, two
  independent adversarial review rounds ending APPROVE; no frozen gate moved
  (2145/2738, 48-gap roster, Class285 260/25/0, E2a 59/10, E2b 51/10, body
  372/48, flow 160/240, ER107 stays 107, 0 matrix error). (1) Gate wall-clock:
  the native contract binary is ~2.2 s and the throwaway Python reference
  re-derives all 4222 spans in 0.09 s, so rule iteration stayed in the /tmp
  reference and the C++ was ported once the grammar was pinned; the dominant
  cost was the two review rounds and the first interface-edit BMI fan-out, not
  test execution. (2) False green recurred (Rule 27): piping the build through
  `tail` masked ninja's non-zero exit and a gtest-discovery libssl failure
  looked like a compile break; durable fix was `set -o pipefail` + reading
  `${pipestatus[1]}` and running the binary directly with the glibc/openssl
  loader path rather than trusting the discovery step. A transient 0-byte
  binary was a link/discovery race, confirmed on rebuild (1.3 MB ELF). (3)
  Re-done work: an unconditional `entity_id` emit after `read_name` polluted
  all 253 relationship left endpoints (93-case regression) and had to be made
  entity-path-only; and the first-review marker-boundary table was re-derived
  against production's per-entry flags. The upstream check that would have
  caught both before review is a row-by-row diff against the production
  `er_cardinality` entry struct and construct dispatch, not the corpus — now
  Rule 30. (4) Fan-out behaved exactly as the build-layout note predicts:
  touching the two `.cppm` interface files (mermaid.cppm, mermaid_detail.cppm)
  forced a broad UI BMI recompile (~115 ninja steps on the first build); later
  fixes were `.cpp` body edits and recompiled only the relations TU and its
  dependents. (5) The Python-first derivation was measurably cheaper than
  editing C++ blind: each grammar correction cycled in sub-second Python over
  all 107 cases before any compile, and the C++ port reached byte-exact on the
  first full differential once the reference was green; the durable regression
  remains native gtest (real compiled seam + sha256), per the no-text-matching
  policy. (6) New Rule 30 added (corpus-exact != grammar-faithful; shared
  reference/port blind spot; bounded-envelope census + fail-closed). All
  hostile-input divergences the reviews found were empirically zero-occurrence
  in the 107 bodies, so every fix is corpus-invisible and the 4222 roster is
  unchanged; remaining unmodeled surfaces (in-quote content validation,
  terminal-`1` lookahead, 64-entity cap) are documented in the lexer header.
- **2026-11-16, D271 ER source_model generator test seam (test-only; no
  production change; commit 01688c3a local, not pushed).** Outcome: a parallel
  record-building seam regenerates the 16-field source_model for all 107
  originals; native differential passes canonical byte-equality AND the pinned
  source_model_sha256 for every case, with roster tripwires 391 statements /
  362 entities / 253 relationships / 287 attributes / 181 obligations /
  1 subgraph / 4 directions. debug+release emitter binary 11/11, adjacent
  cluster 291/291, two independent adversarial review rounds ending APPROVE;
  frozen gates unmoved (2145/2738, 48-gap, Class285 260/25/0, ER107=107,
  0 matrix error). (1) Gate wall-clock stayed small: the emitter binary is
  ~2.6 s; the dominant cost was the read-only spec/catalog subagent and the two
  reviews, not compile or test. (2) No false green/red: pipefail + direct
  binary runs with the glibc/openssl loader path continued to side-step the
  known gtest-discovery libssl failure; every ninja RC=1 was confirmed as the
  discovery step with zero compile warnings. (3) Re-done work was minimal —
  exactly two build iterations for trivial `-Werror` issues (unused
  block_start; the public seam accidentally left inside an anonymous
  namespace) and one logic correction caught before any green run (folded
  description chomp dropped the trailing empty segment). The upstream check
  that made this cheap was the /tmp derivation catalog + independent python
  probes, now Rule 31. (4) Fan-out: the two `.cppm` edits again forced the
  broad UI BMI recompile on first build; subsequent body-only edits recompiled
  just the relations TU. (5) The shared er_source_admit_envelope extraction
  de-duplicated the third copy of input admission with behavior proven
  unchanged by the existing D248/D251 differentials (their tests stayed green).
  (6) Rule 30 recurred at review: nested subgraph frame ownership and the
  literal-block `|` YAML style are corpus-absent (0 occurrences), invisible to
  the 107-case differential; closed with a depth>=2 fail-closed reject and an
  indicator-preserving yaml_style fix, both corpus-invisible. New Rule 31
  added (spec/canonical-harness first; shared admission at third consumer).
  Remaining unmodeled corpus-absent surfaces documented in code.
- **2026-11-16, D275 ER source_binding test seam (test-only; no production
  change; commit 2f3329d0 local, not pushed).** Outcome: a pure seam
  regenerates the 20-field source_binding for all 107 originals; 8 fields
  (three byte refs with lines/raw sha, two component canonical digests, body
  digest, witness count, dense ordinal list) derive from the source and 12
  manifest/prior fields arrive on an explicit optional ErSourceProvenance with
  null-on-unset. Native differential passes canonical byte-equality AND the
  pinned source_binding_sha256 for every case, with a 12-null/8-populated
  contract, an out-of-order-key canonical-sort anti-silence test, a
  zero-budget fail-closed case and a malformed-input battery. debug+release
  emitter binary 15/15, adjacent cluster 291/291, two adversarial review rounds
  ending APPROVE with no residual findings; frozen gates unmoved (2145/2738,
  48-gap, Class285 260/25/0, ER107=107, 0 matrix error). (1) Dominant cost was
  the read-only spec/catalog subagent and the two reviews plus a full rebuild,
  not test execution (emitter ~4 s). (2) False-red infrastructure, not code:
  the build/ tree was deleted externally mid-session (a "not a directory"
  error, not a compile failure) and after a clean reconfigure the gtest
  discovery/libssl step stopped ninja before all targets linked; durable moves
  are reconfigure-on-missing-tree and `-- -k 0` to finish linking, then run
  binaries directly with the glibc/openssl loader path. (3) Re-done work was
  small: three trivial API-mismatch build iterations (is_int/root()/const get
  vs the actual JsonMutVal surface) and one test path missing the `cases/`
  segment; the upstream check is to read the serdes JsonMutVal API before
  writing the writer. (4) Fan-out: the exported ErSourceProvenance struct in
  mermaid.cppm triggered the broad UI BMI recompile on the clean tree; later
  body-only edits were cheap. (5) The D271 collection was de-duplicated into
  er_src_collect_all shared by both seams with the D271 budget/behavior
  unchanged and its 107/107 test guarding the move. (6) Rule 30/31 both
  recurred: corpus-invisible canonical-writer defects (dead bool return,
  unescaped object keys) and the no-sort-would-still-pass-107 trap were caught
  by review and a synthetic anti-silence source, not the differential. New
  Rule 32 added (explicit provenance inputs; anti-silence test for naturally
  sorted fixtures; clean-tree/discovery recovery).

- **2026-11-16, D279 ER terminal_model test seam (test-only; no production
  change; local, not pushed).** Outcome: the sixth and largest seam
  (`er_terminal_model_json`, ~700 additive lines) regenerates the 14-field
  terminal_model for all 107 originals — 362 entity / 253 relationship / 1
  group presentations, 1764 captions, 871 notices — byte-exact under canonical
  JSON plus the pinned terminal_model_sha256. debug+release emitter 18/18,
  adjacent relations_semantics 269/269 + er107_source107 7/7, production
  test_mermaid_official 63/63 (run by the architecture reviewer), two
  adversarial reviews APPROVE (one MINOR defensive-optional propagation fixed;
  no BLOCKER/MAJOR). Frozen gates unmoved; pure additive diff, zero
  deletions, no shared/contract change. (1) The decisive efficiency move was
  building read-only `python3 -I` cross-validators for every law (ref witness
  sequences, hue/role/token tables, wrap offsets, notice order/templates,
  caption handle domain, scalar presentation fields) BEFORE the C++ rebuild
  loop: the native differential then converged 214 failures -> 14 -> 0 in two
  iterations, with only three real defects (a missing metadata key, an
  alias-array shape, an aliased-entity label witness). Validate the spec in a
  scratch interpreter; spend compiles only on confirmed code. (2) New trap — a
  synthetic unit source must satisfy the FULL composed grammar, not just the
  new seam's parallel lexer. The wrap test first used an empty-frontmatter
  source that the seam's own collector accepted but the embedded
  whole-scene seam's production `parse_relations` rejects (all real ER107
  carry a description), surfacing as a confusing nullopt. Bisection across
  whole_scene/source_model seams localized it; the fix was a realistic
  `description:` frontmatter, not a code change. (3) Reuse held: whole_scene
  embedded zero-touch via `JsonMutDoc::raw_json` deep-copy; collection,
  ref-builder and canonical writer shared; closed hue/role/token tables
  embedded equal to the production constexpr arrays rather than reading
  runtime Palette RGB. (4) Rule 30 recurred twice: only 12 captions actually
  wrap and none use a space joiner in the corpus, so the space-break branch
  needed a synthetic >32-byte quoted role; and the alias label-witness
  distinction (20/362 entities) is invisible unless aliased cases are probed
  directly. (5) Post-auto-compaction the build stayed in-session rather than
  handing off: the committed handoff spec doubled as the design record and
  safety net, so no context was lost and no field was downgraded. (6) The
  recurring gtest-discovery/libssl false-red was handled with the established
  `-- -k 0` + direct-binary loader-path move; emitter test execution stays
  ~1-4 s, so differential runs were never the bottleneck.

- **2026-11-16, D282 ER107 family closure gates (test-only seam family; no
  production change; local, not pushed).** The D248->D279 six-seam chain
  passed its deferred full ceremony — full matrix, ASan+UBSan and the serial
  three-family gate, each run once this round rather than inferred from the
  layered evidence. (1) Full OfficialMatrix (2145 pinned originals x 5
  frames): release 27.0 s and debug 241.5 s, both PASS, and the two report
  JSONs are byte-identical, sha256
  c906c74afddc02efa2bc311399ab6d337fb6dcf2318e6528e78d85067e09dc25 — the
  same digest recorded at D236. Frozen ledger unmoved: whole-original @240
  1232 drawn / 913 source_fallback / 0 error; body_diagnostic 372/173/48;
  Class285 260/25/0; ER 175/31/0; integrity_errors empty; ER107 stays 107.
  A test-only seam cannot move a renderer count, and the unchanged digest
  proves it. (2) ASan+UBSan on the changed ER surface, 92 tests, zero
  sanitizer findings: emitter_contract 18/18, er107_source107 7/7 and the
  relations_semantics `MermaidEr*` filter 67/67. The two giant frozen gates
  are excluded from the ASan lane with measured, not asserted, reasons (see
  Open work #6): Class285 isolated ASan 723.5 s / 471 MiB peak and
  Config110 990.3 s / 730 MiB peak, both PASS cleanly — duration blowup
  (~74-82x), not OOM on a 109 GiB box and not a failure. They are instead
  covered this round by fresh release `-j1` serial runs (Config110 3 tests
  13.40 s, Class285 1 test 8.85 s, Block58 3 tests 0.48 s, Er107 3 tests
  13.30 s; 10 tests, RC 0) plus the byte-identical dual-preset matrix. (3)
  Targeted-loop timing, clean debug with no concurrent load: the new
  TerminalModel 107-case canonical+sha differential 1013 ms; sibling
  differentials source_binding 1081 ms, source_model 390 ms, whole_scene
  252 ms, structural 363 ms, semantic 430 ms; the full 18-test emitter
  binary 4957 ms; er107_source107 7 tests 112.9 s; relations_semantics 269
  tests 660.5 s. (4) The ASan gate earned its keep and must not be replaced
  by layered evidence: it caught a real heap-use-after-free that every
  non-ASan build masked. The D279 wrap unit test's lambda returned a
  non-owning `JsonVal caption` view into a local `JsonDoc parsed`; when the
  lambda returned, the doc destructor freed the yyjson heap and the view
  dangled. Under debug/release the freed cell happened not to be reused, so
  the assertions read stale-but-intact bytes and the suite stayed green;
  ASan reported the free/use pair immediately (yyjson default_free ->
  unsafe_yyjson_is_obj in the test body). The fix keeps the JsonDoc alive
  inside the result struct (move the doc; the view is valid for the doc's
  lifetime), after which emitter_contract is 18/18 clean under ASan and
  re-verified 18/18 in both presets. Lesson: a view/handle returned past its
  owning document's scope is a false-green class only a sanitizer reliably
  exposes — do not treat "passes in debug and release" as memory-safe. (5)
  Two harness traps this round, neither a code defect: `export
  LD_LIBRARY_PATH` with the Homebrew glibc prefix poisoned sibling system
  utilities (`date`/`basename`/`tail` failed with a GLIBC_PRIVATE lookup), so
  the prefix must stay per-command rather than exported; and an early full
  ASan suite "disappeared" mid-Class285 because the outer background-task
  harness ended, which was correctly treated as inconclusive and re-run
  isolated instead of being counted as a pass.
- **2026-11-17, ER group-edge parser admission (D293→D298) — NO-GO, no
  commit of production code.** A real OfficialMatrix run disproved the D237
  "admit the grammar and six rows draw" hypothesis. Measured delta from
  applying the two `parse_plain_er` hunks: drawn 1232→**1232 (zero gain)**,
  source_fallback 913→907, **error 0→6**, mandatory_gaps 48→54, coverage
  unchanged. Whole-null ER originals can only be `drawn` via the ER107
  owned-proof branch (`test_mermaid_official.cpp:19134`); the six cannot join
  kPins because their ER206 debt is `raw_entity_group_kind_collision`, not
  `active_config`. Once they parse, they render differently from the text
  control so the silent `source_fallback` downgrade no longer applies and the
  default `error` status sticks (`er206_whole_null_unmet`, "never admission").
  The independent cohort's char-surface gate (native grid != text grid) went
  green on all 21 per-width states — a strictly weaker predicate than the
  official whole-null proof+geometry policy, i.e. the precise false-green the
  authority path exists to catch. **Rule (rework-avoidance): a renderer-free
  or char-difference cohort gate never establishes OfficialMatrix admission
  for a whole-null original; only an owned, geometry-ready proof does. Run the
  real matrix once before building ledger/admission machinery on the
  assumption that "parses ⇒ drawn".** Shelved via `git stash -u` plus a
  GC-proof `/tmp/d295_shelved/` backup (patch, generator, ledger, pre/post
  matrices); full negative result and exit criteria in
  `docs/er107-group-edge-admission-nogo.md`. Stage A `916b9dd9` retained
  (test-only group-tree/lexer capability, 107 byte-identical) as necessary but
  not sufficient. Targeted gates stayed green throughout (emitter 21/21,
  relations 269/269, source107 7/7); the only red was the matrix's own 48→54
  count, which correctly refused the net-negative change.
- **2026-11-17, D301 geometry-gap investigation — hypothesis falsified, no
  production change.** A second milestone hypothesized a renderer geometry
  defect behind the same six rows. Production-faithful measurement (the same
  `er206_observe`/`note13_capture` normalized Grids and `following_row` as
  `run_official_er206_case`) showed all six satisfy
  `!wide && restored && a && b && c` once the grammar parses — no layout work
  is warranted; the only real blocker remains the authority owned-proof path.
  **Rule (measurement fidelity): a raw `ftxui::Screen` pixel capture is NOT a
  drop-in for the official geometry predicate — raw pixels can hold multi-char
  cells while `following_row` requires single-char cells, which produced a
  false `a/b/c=0` negative until the normalized `note13_capture` Grid was used.
  Reuse the exact production capture+predicate when probing an official gate;
  a hand-rolled capture proves only itself.** Targeted-loop cost stayed low:
  each SourceProfile filter run ~0.28–0.36 s; the six-fixture faithful probe
  ~0.01 s (all under the 820-byte bound; `Work{kEr206SceneLimit}` is required
  — a default `Work` throws "Relation oracle work exhausted"). Baseline-restore
  rebuild of the three touched targets took 35 s, zero warnings. **Harness
  trap: an incremental multi-target build reported success while only
  re-LINKING and silently skipping a stale object (the binary still held a
  deleted TEMP test despite a newer source mtime); a forced single-target
  rebuild corrected it. After reverting a test TU, verify via
  `--gtest_list_tests`, not just a green run — a stale extra test that calls
  SUCCEED() still passes.** No OfficialMatrix full run was needed (per the
  milestone's targeted-only boundary); the D298 full-matrix evidence already
  covers the end-to-end disposition.
- **2026-11-18, RFC 0034 Phase 1 bounded layout-Work allowance (D-363, parked
  interim-red, uncommitted).** Layered gates held cost down: meter/sweep/N-1/
  N-4 iteration used single-target debug builds and the ~0.7 s
  `MermaidEnvelopeWork` suite; the dual-preset byte gate needed only four
  OfficialMatrix runs total (one clean + one Phase-1 per preset), captured via
  a stash cycle rather than repeated full runs, and the release binary
  (~27 s) carried the census work. Dominant wall-clock was the one debug
  OfficialMatrix (~240 s) and the 661 s relations_semantics serial binary.
  **Rework loop worth recording: the §3.2 sweep was first written as a
  single-pool binary-search of the minimum budget, but `relations_layout`
  auto-enables the supplemental pool, so every measured `Work` became two-pool
  and the unique-1M-crosser signal read 0 — a measurement that could not
  observe its own target. The fix was to stop suppressing the pool and instead
  observe pool accounting directly (a non-full `supplemental_remaining` after a
  successful draw is an exact primary-exhaustion witness), which needed zero
  test-only production switches. Rule: when the code under test owns the meter
  mode, observe the resulting ledger rather than trying to force the legacy
  mode. Second finding: the design's interim-red list named only the matrix
  gap, but the full-suite run surfaced two more frozen single-pool
  one-short-layout tests (er107_runtime, er107_source107) across all 107
  pins; running the whole 93-binary mermaid suite once (parallel `-P4`,
  ~10 min) to enumerate the complete red set was cheaper and safer than
  assuming the design's list was exhaustive. No false green/flake; the only
  reds are deterministic meter-state/ledger gates traced to one mechanism.
  `-Werror` clean both presets; no BMI/module-edge change (graph_check PASS).
- **2026-11-19, RFC 0034 Phase 4 closing full-suite gate — two environmental
  flakes, neither a Loom regression, both proven by isolation.**
  (1) **Host port-scanner contaminates exact-count localhost tests (new
  root cause; not CPU load).** `BridgeDaemon.ConsecutivePollFailuresUse…`
  failed deterministically 4/4 in debug and 3/3 in release with 33 recorded
  requests vs the expected 32, always at ~12 s. The DaemonServer constructor
  starts no thread, so there is no in-process background poller; the client
  issues exactly 8 polls × 4 HTTP retries = 32 connections. `strace -f
  trace=connect,accept4,recvfrom` showed 32 client `connect()` calls but 33
  server-side requests: the extra one is a foreign `GET / HTTP/1.1` with
  `User-Agent: persona-ports` / `Host: localhost:<ephemeral>`. Source: the
  `persona-link`/`persona-node` devbox tunnel agent under
  `~/.persona-devbox-cn/`, which sweeps every localhost listener and probes
  it; the single-threaded bridge mock (`tests/support/bridge_test_helpers.hpp`)
  pushes every inbound request into `requests_` before routing, so a probe
  landing in the test's ~12 s window inflates the exact-size assertion. Rule:
  when an exact-request-count test over an ephemeral port is off by exactly
  one with no code path that could emit it, suspect a host scanner before the
  product — confirm with `strace recvfrom` and read the foreign User-Agent.
  Do not "fix" the assertion; it is an environment collision, and the other
  12/13 bridge tests pass.
  (2) **E2E_Pty question/selection cluster under sustained load.** At load
  average 16–20 on the 56-core shared box (3 logged-in users), 7 of 95 PTY
  tests timed out — the `E2E_Pty_Question_*` and one Selection/ProgramStatus
  case, all gated by the harness's fixed 10 s wall-clock `terminal.wait()` for
  a full question→tool-result→second-model-turn sequence; 88 sibling PTY tests
  in the same binary passed, and `question-default` flipped pass/fail across
  reruns (non-deterministic). `McpStdio.WatchdogKillsSilentChild` failed once
  under parallel load and passed on every isolated rerun. This is the same
  class as the documented ServerMain/Config110 flakes (rule 8); record the load
  average and re-run the binary directly with `LOOM_BINARY` set (bare
  invocation returns the ctest `SKIP_RETURN_CODE 125`, not a failure).
  Practical note: do not run other test binaries concurrently with a full
  ctest on this box — parallel release runs during the matrix window added to
  the scheduling pressure that starves the PTY child's 10 s deadline.
- **2026-11-21, D-395 F0 — three rework-loop rules from one milestone.**
  (1) **`ctest` from the repository root is a silent false green.** Running
  `ctest -R '<test>'` from the repo root prints "No tests were found!!!" and
  exits 0, so a census command that never executed reads as a pass and the
  unchanged artifact looks like "no delta". Rule: always run `ctest` from the
  preset build directory (or pass `--test-dir build/<preset>`), and treat
  "No tests were found" as a failure signal, not a no-op. Confirm a census
  run by the artifact mtime/sha changing, not by the exit code alone.
  (2) **An admission widening without its matching promoted-row ledger is a
  renderer-only mandatory-gap drift.** The first F0 gate draft also accepted
  a nested `config.flowchart.htmlLabels`; the one corpus row carrying it
  then drew while its manifest expectation stayed `source`, producing one
  `error` row and gaps 48→49. Rule: every gate widening lands in the same
  change as the exact promoted-row ledger (cohort entry or manifest), and
  the matrix is re-run before attributing any census delta.
  (3) **Committed data ledgers must be reproducible by their own tool.**
  The independent review's r2 caught that adding `selection.superseded_note`
  by hand made `generate_requirement_cohort.py --check` fail; adding the
  same key to the generator restored reproducibility. Rule: when annotating
  a tool-generated ledger, update the generator in the same commit and run
  its `--check`.

### 2026-11-21 — RFC 0035 (xychart) implementation retro

- **A render-diff predicate is not a fallback predicate.** Comparing a
  mermaid-fenced render against a plain `text`-fenced render can differ for
  reasons unrelated to native drawing (fence language handling), so it
  reports "drawn" for a source that is actually on fallback. Symptom: the
  per-row gate passed while the twin-equality assertion proved the frame was
  the untouched source. Rule: a fallback probe must use the source's own
  hallmark (the diagram keyword still visible in the frame, as the E2c gate
  does) and must be combined with a twin assertion; never infer "drawn" from
  "differs from the text fence".
- **Locate body directives structurally, not by first substring.** The P4
  fixture's folded description mentions ``%%{init: ...}%%`` as literal text;
  a naive `find("%%{")` erased the description tail and left the real
  directive in the "twin", producing identical frames and a misleading
  orientation failure. Rule: search for body constructs only after the
  closing frontmatter fence.
- **Per-phase review records, not one ledger verdict.** A single
  `review.verdict` combined with a generator that iterates the records it
  finds lets a newly enabled phase be admitted before it is reviewed
  (silent false green). Rule: the generator and the gate both iterate the
  enabled phase list and require an approved record per phase.
- **A negative test must prove the intended rejection branch.** The P4
  empty-object evidence was a directive missing one closing brace, so it
  passed on JSON malformation instead of the walker's empty-object check.
  Rule: negative sources must be well-formed up to the single violated
  rule; reviewers should re-derive the parse result for each negative.
- **Bookkeeping is part of the gate.** Five of the nine post-review findings
  across P1–P4 were metadata drift (ledger notes, selection.rule text,
  counts, a stale doc index), each requiring a regenerate-and-check cycle.
  Rule: when a phase changes a row's classification or mechanism, update the
  generator's per-phase note and rule text in the same edit, then run
  `--check`.

### 2026-11-22 — RFC 0040 (journey-family) implementation retro

1. **Gate wall-clock.** The new cohort gate is 18 s for 11 tests (7 gtest +
   4 hermetic `--check`); the three existing family cohorts add ~112 s. The
   dominated step is the release OfficialMatrix at 27.8 s normally, but the
   one full debug `ctest -L mermaid` run cost 483 s real (3855.9 s*proc,
   1457 tests) — 17x the release matrix — so iteration used the focused gates
   plus the release matrix, and the debug label run happened once.
2. **False green/red.** None in a green run. Two near-misses were caught before
   any red gate: the roster `--self-test` key-set break if `--emit` omits an
   emptied detector category (design review BLOCKER; `look_classic_other`
   empties here), and the matrix `observed_gap_rows` 48 -> 79 break if the new
   branches sit after the generic fallback (PRR). Both are durable rules now
   (key-set emission in `--emit`; branch order).
3. **Rework.** One real loop: the first gate draft's gantt/journey batteries did
   not implement the RFC §3 per-family probe set, found at code-review r1
   (MAJOR) and fixed by one `FamilyBattery` executed per family (rule 33). The
   upstream check that would have caught it earlier: read §3's "per family, at
   minimum" list as a test-authoring checklist at draft time.
4. **Build fan-out.** No interfaces changed: three implementation units
   recompile + archive relink + executable relinks; no BMI change; one new test
   executable. The dual-preset byte gate needed no recompile after the ledger
   flip (data-only), only the matrix rerun.
5. **Cheaper approach evidence.** Targeted gate 18 s vs 483 s label run vs
   27.8 s release matrix; the release matrix is byte-identical to debug
   (`fd852b74…`), so the family closure used release for the census and debug
   once for the full suite.
6. **New entry.** Rule 33 (per-family probe lists are an authoring checklist;
   run the shared battery per TU).

### 2026-11-22 — RFC 0041 (agentflow) implementation retro

1. **Gate wall-clock.** The new cohort gate is ~36 s for 13 tests (6 gtest + 5 family
   + 2 hermetic); the debug `ctest -L mermaid` label run cost 1465/1465 at the phase
   gate; the release OfficialMatrix is 27.2 s. Iteration used the focused gates plus
   release.
2. **False green/red.** One real falsification caught by design, not by a red gate: the
   pre-measurement narrow prediction (5 draw) was wrong; the live per-row assertion
   (a PRR MAJOR) forced the measured 0/9 before the ledger was pinned. No flake.
3. **Rework.** The design r1 review found the "strict widening" claim false (indented
   lines outside a config block) and the fail-open `theme` + other leaf; both were fixed
   in the RFC before any code. One implementation-loop cost: the first ledger run pinned
   `narrow_40 = drawn` for five rows and the gate went red on the live assertion — the
   correct behavior, and the reason the assertion exists.
4. **Build fan-out.** One `.cpp` body recompiled, then archive + ~114 executable
   relinks; no BMI change; one new test executable target.
5. **Cheaper approach evidence.** Focused gate 36 s vs the 1465-test label run; release
   matrix byte-identical to debug (`326fb96f…`), so the census used release.
6. **New entry.** Rule 34 (a footer changes the narrow predicate; re-measure, never
   inherit from the twin).

### 2026-11-22 — RFC 0042 (pie) implementation retro

1. **Gate wall-clock.** The new pie gate is ~15 s for 7 tests; the adjacent chart/family
   guard sweep is 204 tests in 239 s; `ctest -L mermaid` is 1472/1472 at the phase gate;
   the release OfficialMatrix is ~27 s.
2. **False green/red.** One real self-inflicted red: the hand-copied Pie7 path typo made a
   real drawn row look undrawn, and a diagnostic probe reused the typo (rule 35). No
   product defect and no flake.
3. **Rework.** The r1 design review's scalar-rule MAJOR (misattributed
   `chart_yaml_scalar`) would have produced an untestable "trailing comment positive"; it
   was fixed in the RFC before code. The PRR's emit-order MAJOR would have turned the
   drift gate red by construction, also fixed before code.
4. **Build fan-out.** One charts-TU body recompiled, archive + dependent relinks; no BMI
   change; one new test executable and one hermetic ctest.
5. **Cheaper approach evidence.** Focused pie gate 15 s vs 1472-test label; release matrix
   byte-identical to debug (`b00ab2c3…`), so the census used release.
6. **New entry.** Rule 35 (existence-check hand-copied fixture paths; share one list).

### 2026-11-22 — RFC 0043 (small-family theme) implementation retro

1. **Gate wall-clock.** The new gate is ~17 s for 6 tests; the three family cohort tests
   are 10/13/13 s; the four hermetic `--check` entries total 0.25 s; the debug
   OfficialMatrix is 239 s, release 27 s. Full debug ctest 4667/4667 in 545 s.
2. **False green/red.** Two self-inflicted reds, both caught by the gate rather than
   shipped: the expected footer composition did not account for the source's own trailing
   blank row (fixed by making the composition tail-blank-count-insensitive), and the
   scalar battery initially reused `families()[0]` only. A third red was a stale-binary
   artifact: the gate printed 6/6 while the build had actually failed on that file — the
   `-Werror` string concat (`const char* + const char[8]`) only surfaced on the next
   build. Lesson: after editing a test file, treat the build exit as part of the gate.
3. **Rework.** The bill itself was wrong once: D-410's 5d was 5 rows because the D-402
   probe's `keys=` field folds description payload continuation lines into the key set.
   Recomputing structurally found the sixth row (venn night9) and re-bucketed 5a/5b/5c.
   The design gate then caught two more arithmetic/semantic defects before code (venn
   11+3+3+4 and the description-after-config verdict), which would each have produced a
   red or a wrongly-labeled ledger row had they shipped.
4. **Build fan-out.** Three `.cpp` bodies recompiled, archive + dependent relinks; no BMI
   change; one new test executable and four hermetic ctests.
5. **Cheaper approach evidence.** Focused gate 17 s vs 4667-test full suite; release
   matrix byte-identical to debug (`8de4c546…`), so the census used release.
6. **New entry.** Rule 36: a probe's key-set field that concatenates payload continuation
   lines is not a classifier — parse the frontmatter structurally, and re-derive a bill
   subgroup before pinning scope on it.

### 2026-11-22 — RFC 0044 (requirement elk) implementation retro

1. **Gate wall-clock.** The new gate is ~19 s for 5 tests; the RFC 0037 120-row oracle is
   ~135 s; the three hermetic `--check` entries total 0.2 s; the debug OfficialMatrix is
   240 s, release 27 s. Full debug ctest 4675/4675 in 555 s.
2. **False green/red.** Three self-inflicted reds, all caught before shipping: the twin
   composition treated `footer_lines` (which includes the header) as leaf-only and
   double-counted the header; `layout : elk` (spaces around the key) was expected to reject
   but the envelope parser trims keys and admits it, so the battery was wrong and was
   corrected to pin the real behavior; a ctest invocation without `LD_LIBRARY_PATH` failed
   all five gtests and looked like a product red — the environment, not the code.
3. **Rework.** The code review's MAJOR was a provenance defect, not a runtime one: the
   generator's `admits()` was a naive split that disagreed with the live gate on indent-4,
   quoted and commented values. It would not have changed the shipped 23 rows, but it is
   the ledger's provenance authority, so the mirror was rewritten and probe-matched.
   Design r1's BLOCKER (three existing assertions pinning elk as fallback) would have
   reded the suite; design r2 caught a self-contradiction introduced by the r1 rewrite.
4. **Build fan-out.** One `.cpp` body recompiled, archive + dependent relinks; no BMI
   change; one new test executable and three hermetic ctests.
5. **Cheaper approach evidence.** Focused gate 19 s vs 4675-test full suite; release matrix
   byte-identical to debug (`0732abe2…`), so the census used release.
6. **New entry.** Rule 37: when a ledger generator claims to mirror a live gate, drive the
   same shapes through both (indent, quoting, comments, duplicates) as a probe table; a
   `--check` that reproduces proves stability, not fidelity.

### 2026-11-22 — RFC 0046 (layout/display tuning) implementation retro

1. **Gate wall-clock.** The new gate is ~12.5 s for 5 tests; the architecture cohort gate
   grew from ~9 s to ~20 s (the 4 new rows plus the new whole-vs-body frame-identity
   assertion); ishikawa 2 s, smallfam 6 s, state-0038 65 s, E2a 6.5 s, E2b 4 s. Debug
   OfficialMatrix 273 s, release 27 s. Full debug ctest 4687 executed (the `-N`
   denominator is 4689 because of two `DISABLED_` layout tests).
2. **False green/red.** Two: the architecture cohort gate reded because two tuning rows
   draw on the *body* track at 40 columns while their whole sources (footer included)
   stay source — the ledger needed the body-track narrow constants, not a whole-track
   guess. And the 0046 matrix branch was initially placed after the per-family branches,
   so the architecture cohort branch silently swallowed 4/11 rows; the matrix stayed
   green because the two ledgers agreed, which is exactly the "shadowed branch" false
   green the code review caught.
3. **Rework.** The first mirror pass in this phase repeated the RFC 0045 class of defect
   in a new place: the architecture mirror skipped folded-description payload lines while
   the production scan still gated payload lines that *begin* with a visual key, and the
   ishikawa mirror and gate disagreed on theme+namespace co-occurrence. Both were fixed
   by making the two sides literally the same rule and adding the paired positives and
   negatives to the battery.
4. **Build fan-out.** Three `.cpp` body recompiles (`ishikawa`, `architecture`,
   `relations_core`), no BMI change, one new test executable and one hermetic ctest.
5. **Cheaper approach evidence.** Focused gates ~110 s versus the 4687-test full suite;
   release matrix byte-identical to debug (`10e819f8…`), so the census used release.
6. **New entry.** Rule 38: place a new cohort's matrix branch before every per-family
   branch and prove it is reached (a per-row ledger read), because a shadowing branch is
   invisible whenever the old and new ledgers happen to agree; and when a mirror and its
   gate can disagree about a *combination* of admitted leaves, pin the combination with
   one positive and two order-swapped negatives in the same battery.

### 2026-11-22 — RFC 0047 (journey-family init directive) implementation retro

1. **Gate wall-clock.** The new gate is ~7 s for 5 tests (58-60 probe entries driven
   through the public renderer twice); timeline 1.4 s, journey 0.5 s, 0040 2 s;
   `ctest -L mermaid` 1508/1508 in 535 s. Debug OfficialMatrix 259 s, release 34 s. Full
   debug ctest in `/tmp/loom-logs/ctest-full-dbg-0047.log`.
2. **False green/red.** Three self-inflicted, all caught before shipping: (a) the
   selection scan used `%%\{[^}]*\}%%`, which cannot match a nested `init: { ... }%%`
   payload, so it reported 17 directive rows and a 10-row slice — a structured line scan
   and a render probe corrected it to 9; (b) a `nohup ... &` release build reported
   "completed" when the wrapper shell exited while the real builder ran on, and the stale
   release binary produced the *previous* RFC's matrix (1342/`10e819f8…`) — the release
   matrix is now always confirmed by `pgrep -f '[c]make --build'` before it is trusted;
   (c) the twin+footer composition initially double-counted the fenced block's trailing
   blank row (false red), fixed by dropping it before splicing, the pie_0042 pattern.
3. **Rework.** Two review round-trips were avoidable: PRR r1/r2 bounced the mirror-fidelity
   probe because it was specified as "a table both sides assert" without a single-source
   artifact or a review-bound digest — the third round needed a committed JSON file read
   by both sides plus `selection.probes_sha256` hardcoded. Specify an oracle artifact with
   its binding mechanism up front. The code review then caught that the digest was still
   `PENDING` (inert binding) and that the per-row oracle only asserted draw+footer rather
   than frame = twin + footer; both were test-only fixes.
4. **Build fan-out.** Three `.cpp` body recompiles, archive + dependent relinks, no BMI
   change; one new test executable and one hermetic ctest.
5. **Cheaper approach evidence.** Focused gates ~11 s versus the 1508-test mermaid label
   (535 s); release matrix byte-identical to debug (`86e49423…`), so the census used release.
6. **Near-budget tests (measured, follow-up).** Three consecutive full runs each
   timed out one of two mermaid tests that sit at 80-100% of their budgets:
   `MermaidOfficialConfig110Native.EveryWholeSourceIsObservedBeforeDecodeAndAllProfilesRemainMandatory`
   (300 s budget; 240.09 s in the RFC 0046 run; isolated 213/250/260 s) failed all
   three runs, and `MermaidBlock58AdmissionJoin.RejectsUnreviewedIncompleteOrBorrowedVerdicts`
   (60 s budget; 51.84 s in run1; isolated 25 s) failed once. Both are pre-existing
   budget-margin marathons, not regressions, but they make a green full-suite run
   load-dependent; a future slice should either cut their scope or calibrate the
   budgets against measured isolated times.
7. **New entry.** Rule 39: a selection scan over fixtures must strip the frontmatter first
   and classify body lines structurally — a folded `description:` payload can contain
   directive-looking prose (the journey theme fixtures do), and a regex that stops at the
   first `}` undercounts nested `%%{init: {...}}%%` payloads. Rule 40: never trust a
   background builder's wrapper exit; only the builder process's own exit (or its absence
   via `pgrep -f '[c]make --build'`) proves a fresh binary, and a stale binary can silently
   reproduce the previous milestone's matrix.

## Retro: RFC 0048 State concurrent regions + in-body init/accessibility (2026-11-22)

1. **Selection rerun cost, not waste.** The D-421 pool (19 rows) was a roster-category
   reading; the first implementation run of the matrix exposed **17** P1 rows instead of 11
   through the E6c gap diff (14 "unexpected (renderer-only)" rows), which is the fastest
   possible measurement: the frozen gap ledger is itself the completeness check. Treat a
   gap-diff surprise as a **scope measurement**, not a test failure to silence — the honest
   responses were admit (scope amendment) or book into a protected roster, and only the
   former preserves the invariants.
2. **Dead probes.** Two of my synthetic negatives were over-strict rather than real rules
   (a `direction` inside a *nested* child, and an empty-frontmatter footer expectation); both
   were caught by running the gate against the contract before asking for review. Write the
   negative from the RFC sentence, then re-read the sentence before pinning it.
3. **Measured vs projected pins (rework).** Three RFC-projections did not survive contact:
   the E2a `checked` re-pin was **80**, not the reviewed **86** (the two E2b rows are
   themselves config-bearing cohort members, counted once, not twice); the E2b scan lost
   **16** rows, not 8; and the 0046 constants needed **no** migration at all because that
   suite's counts derive from its own tuning ledger. Lesson: a PRR §"footprint" that lists
   "file:line → new constant" is a hypothesis; only the suite's own output is evidence.
   Record the measured value and the derivation delta in the RFC amendment, never silently.
4. **Build fan-out.** One BMI change (`mermaid_detail.cppm`: two side tables, a region-aware
   `entity`/`control`, the `measure` split) recompiled the `:detail` importers; body-only
   churn after that was archive + relink. One new test executable plus one hermetic
   generator check.
5. **Cheaper approach evidence.** The 25-row cohort gate (frame oracle + 12 grammar
   negatives + 25 probe cases) runs in ~19 s; the full mermaid label is 274 s wall
   (1515 tests). Region probes were validated with a temporary single-TU render test that
   was deleted before commit — cheaper than adding a harness binary.
6. **Discovery shortcut worth keeping.** `test_mermaid_probe.cpp` (a tiny TU that imports the
   public render entry point) is the cheapest way to inspect a frame during development:
   build one target (~2 s) instead of the 240 s matrix, and the matrix is then used only to
   confirm the census.
7. **New entry, rule 41:** a *source-shape* blocker detector in a renderer-free ledger must
   be retired in the same commit that admits the shape, and its vocabulary entry may stay
   (a closed set is a compatibility surface) while its branch goes. Otherwise the ledger
   predicts a fallback the renderer no longer produces, and the failure surfaces as a
   mysterious per-row "registered blocked row rendered" much later.

## D-424 gitGraph closed-boolean slice retro (2026-11-22)

1. **A calm gate surface still needs one measurement run.** The slice looked
   trivial (one envelope branch plus two `if`s), yet the first gate run found
   three distinct defects: a duplicated footer header in the test's own
   composition helper, an older gate whose *negative* case used the exact shape
   being admitted (RFC 0045's `showBranches` unknown-leaf probe), and a
   5-row/1-row body grouping the 18 fixtures actually form. The corpus is the
   only source for the third; write the group shape from the ledger, never from
   the fixture names.
2. **Off-by-one in footer composition is invisible in the art.** Assert the
   frame tail with the ledger block as a unit (blank separator + ledger lines)
   rather than rebuilding it by hand; the duplicated header produced a
   ten-minute detour because the assertion diff printed the *art* line as the
   first mismatch. Read the whole tail once when the arithmetic disagrees.
3. **Renaming a slice id is cheap; inventing a phantom RFC number is not.**
   This slice was opened under a TL review exemption ("no design doc, put the
   rationale in the commit/test doc"). Using `RFC 0049` in file names and
   reason strings would have pointed readers at a nonexistent RFC, so the slice
   is named by its decision id (`D-424`) everywhere — ledger schema, generator,
   test suite, matrix reason string.
4. **The semantic oracle costs less than it looks.** The caption-adjacency
   predicate is ~25 lines and distinguishes `●─┘1` from `*@main → 5`; a
   substring scan would have produced a false green for every
   `showCommitLabel: false` row that still prints a branch-head id.

## D-425 pie namespace slice retro (2026-11-22)

1. **The body-only census is the cheapest selection instrument found so far.**
   Stripping the frontmatter and rendering all 565 fallbacks through
   `render_mermaid` (one ~2 s probe build, ~1 s of rendering) classified every
   row into "config-only blocker" (133) versus "body blocker" (432) and showed
   that the roster's category label is often *not* the blocker: the seven
   mindmap `look: neo` rows and all 17 sankey `family_residual` rows fall back
   even with the frontmatter removed. Use it before writing any family slice;
   it costs minutes and prevents a slice that cannot land.
2. **A twin that has no footer changes the canvas helper.** `canvas_lines`
   anchored on the footer header; a config-free twin has no footer, so the
   comparison swallowed the sentinel row and every row "failed" identically.
   Trim the sentinel as well as trailing blanks, and read one failure in full
   before assuming the renderer is wrong.
3. **Quoted scalars are values, not shapes.** The pie gate first listed
   `textPosition: "0.5"` as a negative; `chart_yaml_scalar` unquotes it (as
   git's `git_inert_scalar` does), so the negative was a false expectation —
   flip it into a positive that pins the *normalized* footer value instead of
   weakening the gate.

## D-426 pie accessibility slice retro (2026-11-23)

1. **A "known blocker" written a day earlier can be half-wrong.** The D-425
   closure said the last four pie rows needed only the acc no-op; probing one
   row showed `neutral` still falls back because three of the four themes were
   never in the RFC 0042 table. Probe the *whole* candidate set before sizing a
   slice — the first row that works does not prove the others.
2. **Closed-table extensions collide with older "out-of-table" negatives.** The
   RFC 0042 gate listed `default`/`forest`/`neutral` as must-reject values; the
   right migration is to move those three into a positive (covered by the new
   gate) and keep an equally sized negative list of values that stay outside,
   not to delete the negative loop.
3. **Twin equality is the cheapest no-op proof.** Deleting the acc lines from
   the source and requiring the entire frame — art *and* footer — to be
   identical needs no new oracle machinery and catches any leak of the metadata
   into layout, labels, or the footer block.

## D-427 treemap valueFormat slice retro (2026-11-23)

1. **A wait loop that greps for a build command matches its own wrapper.** The
   D-423 reachability self-proof said "condition reachable", but
   `while pgrep -f 'cmake --build'; do sleep; done` never exits: the background
   shell's *command line* contains the literal text, so the waiter sees itself.
   Match on a unique sentinel (a log marker) or on the pid of the process you
   started, and re-check that the condition can become false before waiting.
   Cost: ~40 minutes of idle wall clock.
2. **Production-file edits invalidate the probe shortcut.** `test_mermaid_probe`
   is a two-second build only while its own TU is the change; touching
   `mermaid_treemap_impl.cpp` rebuilds every importer. Group probe-driven
   measurement before the production edit, or expect a full closure rebuild per
   iteration.
3. **A closed grammar must accept the corpus spelling, not the textbook one.**
   `$0,0` carries d3 zero-pad flags on *both* sides of the group separator; the
   first grammar accepted only the leading one and every fixture fell back.
   Write the grammar from the corpus strings, then let negatives prove
   closedness (`$00,0`, `.7f`, `2d`).
4. **Quoted YAML scalars appear in config values too.** `valueFormat: '$0,0'`
   is quoted; both the C++ parser and the Python mirror must strip a matching
   quote pair, or the mirror and the renderer disagree silently.

## D-429 packet namespace retro (2026-11-23)

1. **A sibling gate's helper can carry a family-specific guard.** The packet
   gate reused `native_frame` from the treemap gate, including its
   `packet`/`title:` line rejections — but the packet projection legitimately
   transcribes its `packet` header, so every live frame read as "source echo".
   Re-derive each helper's guards from *this* family's frame shape instead of
   copying them; a one-line guard difference cost a full debug cycle.
2. **"Gate green" evidence must name the test.** The TL rule is right: the
   targeted log now shows `MermaidPacketCohort.GatedPacketsStayWholeSource`
   (the red one) passing by name, plus all four D-429 names, not just a count.

## D-430 radar namespace retro (2026-11-23)

1. **A multi-section Python dispatcher must keep its sections separate.** The
   ledger mirror merged `config` and `themeVariables` into one branch; the first
   leaf line then fell through to the "unknown key" reject and the cohort
   silently shrank to one row. Write one branch per section, then assert the
   expected row count loudly (it did).
2. **A failed matrix report is not a roster input.** Emitting the roster from a
   matrix run that failed its own expectations produced a sufficiency table
   that cannot reconcile (`dagre_layout_tuning`). Always gate the emit on a
   green matrix (`MTX=0`) and re-run `--self-test` immediately.
3. **Emit ordering matters when a slice adds a matrix branch.** The branch and
   the reach proof live in different hunks; patch them together and verify the
   loader symbol exists before building, or the report is produced with three
   unexpected draws and a red matrix.

## D-431 cynefin / D-432 c4 frontmatter retro (2026-11-23)

1. **"Accepted" and "has a leaf" are two different facts.** D-432's first
   parser returned `optional<string>`: absent meant both "plain source" and
   "rejected frontmatter", so every source without a `wrap` leaf fell back and
   the twin oracle failed on the first row. Return an explicit tri-state
   `{accepted, leaf?}` whenever a gate both rejects shapes and admits shapes
   with no payload.
2. **Derive the twin before deriving the footer.** The gate's `config_free_twin`
   path is what proves inertness; running it first would have exposed the
   tri-state defect before any matrix time was spent. Order the gate tests
   ledger → twin → negatives, and run the new target before the matrix.
3. **A family close needs the roster to shrink by the exact number.** After
   D-431, `--emit` moved total 542 → 537 and dropped `family_residual:cynefin`;
   after D-432, c4 goes 9 → 8. Diff the category map, not just the total.

## D-433 tree-view namespace retro (2026-11-23)

1. **A renderer-free generator must mirror the reachable set, not only the
   grammar.** The tree-view parser accepts a `---      ` closing fence, but the
   shared classify probe closes frontmatter only on a line exactly `---`, so
   that source never reaches any family renderer. The generator therefore
   splits fronts strictly and the gate pins the unreachable shape as a
   negative. Check *which* component owns reachability before admitting a row.
2. **Never rewrite `return result;` by regex across a whole function.** A
   debug pass that replaced every return inside the parser also rewrote the
   success return, so the bisect showed "rejections" on successful parses and
   sent the diagnosis down two wrong paths. Scope debug edits to explicitly
   tagged regions, and delete them before the gate run.
3. **Write body negatives at the shape the gate actually recognizes.** A
   `classDef` appended to an existing node line is not a statement; the gate is
   key-based on a node line's first token. The negative must start its own
   line, otherwise it tests something the production rule never claimed.
4. **Indentation stack beats fixed columns when one corpus uses several.**
   `treeView:` children sit at indent 4 and 6, and the theme block nests one
   level deeper; a fixed-column state machine would have to special-case each
   file. Pop to the nearest lower indent and compare canonical paths.

## D-434 tree-view icon-map retro (2026-11-23)

1. **A guard that stops rejecting is a silent admission.** While adding the map
   branch, the leaf guard became `if (slot != nullptr) {...}` so an unknown
   leaf under a known parent was accepted as a leaf and counted. The gate's
   `filenameIcons + colour: red` negative caught it. When a new branch shares a
   guard, re-read the guard as a closed question: which inputs must still be
   rejected?
2. **Validate after normalizing, not before.** The map value `'fa:bell'` failed
   the bare-word check because the quote pair was stripped later. Any
   value grammar with optional quotes must unquote first, then validate.
3. **A "config must carry a leaf" counter must count every leaf kind.** Map
   entries did not increment the scalar `leaves` counter, so a map-only config
   was rejected as empty; the probe showed `showIcons + map` drawing while
   `map` alone fell back, which isolated it in one run.
4. **A family register row can silently miss its history entry.** The D-433
   d117 history row was never written even though the same script updated the
   family row; the D-434 commit carries the missing row. Verify the grep, not
   just the script's exit status.

## D-435 gitGraph title retro (2026-11-23)

1. **A "blocked" contract can be recorded in three places.** Changing the
   frontmatter-title contract broke a dedicated semantics test, an RFC 0039
   cohort negative and an RFC 0045 namespace negative. Grep for the *behaviour*
   (the word the frame would print, the key name) across `tests/ui` before
   running the matrix, not only the family's own gate.
2. **Quoted scalars need an explicit balance rule.** An unbalanced `"` survived
   as plain text and drew. The value grammar now decides lead/trail quote
   symmetry first, then strips the pair, then rejects inner quotes — mirrored
   in the Python generator, which otherwise drifts on the next corpus refresh.
3. **Deriving a gate from another family's file leaves dead helpers.** The
   copied c4 gate carried `canvas_lines`/`config_free_twin`/`expect_footer`
   that the title oracle never uses; `-Werror` only reports some of them and
   the rest are noise. Build the helper set from the oracle you actually wrote.

## D-436 gitGraph main-branch retro (2026-11-23)

1. **A config value that changes parsing must be a parser input, not a
   post-pass.** Renaming the trunk after `parse_gitgraph` would resolve
   `checkout trunk` as an unknown branch (whole-source fallback) or create a
   duplicate branch. The two defaulted parameters keep the public two-argument
   callers working and make the rename take effect before the body is read.
2. **A hand-written Python mirror must be traced, not assumed.** Two state
   machine slips (payload lines rejected because the description section was
   never entered; `gitGraph:` children rejected because the `section !=
   "config"` guard ran first) each produced "0 rows" with no clue. Print the
   per-line trace once when a mirror returns nothing.
3. **`is_null()` is not "key absent" on this JSON type.** A missing key made
   both value branches fire for both rows; `has(key)` is the predicate the
   ledger readers elsewhere already use. Check the API before writing a
   presence test.
4. **Five negative lists carried the same now-admitted leaf.** Grep for the
   config *key name* across `tests/ui` before coding the gate; migrating them
   after the fact cost a build+run cycle each.

## D-436 follow-up: an admission surface can exceed its first ledger

The first D-436 ledger claimed 2 rows, but the same renderer change also released
12 `git/visual-check/*-neo-look-*` rows that carried `look: neo` + `theme` with
a trunk leaf. The matrix caught them as "unexpected renderer-only" rows in one
run. Lesson: after widening a closed config namespace, re-run the matrix **before**
writing the closure doc, and size the ledger from the matrix's unexpected list,
not from the hand-picked fixtures. The twelve rows were added to the ledger
rather than blocked in the renderer — the combination is legitimate and the
renderer already draws it.

## D-437 wardley theme retro (2026-11-23)

1. **A ledger's "frontmatter_gated" label is a diagnosis, not a proof.** The
   four wardley rows were listed as frontmatter-gated because the blunt key scan
   fired first; their bodies (column-0 statements) failed a second, unlabelled
   check. Always probe the *body alone* after implementing the config part — the
   first probe here showed "config admitted, still fallback" and saved a wrong
   closure.
2. **Twin-frame trimming must remove the sentinel, not only blanks.** The
   config-free twin's capture ends with a blank row plus the sentinel, so the
   art comparison failed on two extra rows while the art itself matched. Trim
   both the blank and the sentinel when comparing against a footer-free twin.
3. **A gate copied to a new family drags the old family's fixture helpers.**
   The cloned git gate carried `kBody`/`probe` for gitGraph, which collided with
   the wardley ones; build the helper set from the new oracle before the first
   compile.

## D-438 sankey config NO-GO retro (2026-11-23)

1. **A family-residual label hides where the blocker actually sits.** The 17
   `family_residual:sankey` rows were labelled by a config detector, so the
   slice was designed against that label. All 17 use a bare `sankey` header,
   which the shared `classify_mermaid` predicate never dispatches — the config
   work was implemented, tested and then proved to move zero corpus rows. Probe
   the smallest surviving input (a bare body, no frontmatter) **before**
   designing the config admission.
2. **Renderers are only reachable through the dispatch predicate.** Before
   widening a family's frontmatter grammar, grep the classifier for the family
   keyword and check every spelling the corpus uses; a header alias is a shared
   change and belongs in a TL decision, not in the slice.
3. **Zero-effect slices are reverted, not committed.** The finished patch is
   kept at `/tmp/d438-sankey-config.patch` (applies cleanly to `04a98f1c`) so
   authorization does not pay for the reimplementation.

## D-439 mindmap layout retro (2026-11-23)

1. **The probe-first rule from D-438 paid off immediately.** A one-file temporary probe
   built once (47 objects) told us in 0.5 s that only 3 of the 5 `tidy-tree` fixtures are
   body-admitted: the icon row and the anonymous `((...))` root row fail the body grammar
   regardless of the config leaf. The ledger was sized from measured rows, not from the
   config detector's bucket — no matrix surprise, no ledger widening.
2. **A strict envelope scanner needs a cross-check, not a rewrite.** `mindmap_body` keeps
   its line-by-line closed scan and adds one `parse_envelope` cross-check for the config
   event; the Original10/Shapes4 frames stay byte-identical because the footer is emitted
   only for the newly admitted leaf.
3. **Generator mirrors must encode every rejection, not just the acceptance.** The first
   generator accepted 19 rows because a description-only envelope also ends with
   `config_seen == false`; the cohort pin (`total == 3`) caught it on the first run.

## D-440 flowchart theme retro (2026-11-23)

1. **Probe the whole corpus, not the directory you plan to touch.** The
   twin-probe showed 11 drawable flowchart/ candidates; the decisive fact was
   one directory up: `flowchart/` has exactly 400 fixtures, the same 400 as the
   frozen Flow400 contract. One 8-row edit attempt produced
   `flows.errors = 8` and five extra mandatory gaps in a single matrix run —
   that run is the cheapest possible proof of the freeze.
2. **Size a cohort by the contract it would violate, not by the renderer it
   would satisfy.** After narrowing to `{forest}` the slice is 1 row; the
   remaining 10 candidates are contract-pending, not "hard".
3. **A closed table must be corpus-backed per value.** Listing five palettes
   because the probe saw five values was the mistake; the correct rule is one
   value per non-frozen corpus row.

## D-441 state layout retro (2026-11-23)

1. **Enumerate the corpus value table before writing the closed set.** The state
   envelope's `look`/`theme` tables are corpus-backed per value; `layout` has
   exactly four state fixtures and one admitted value (`elk`), so the table is
   one entry and the fourth fixture is excluded by its body (notes), not by a
   guessed config rule.
2. **Check how the shared inert-scalar reader treats quotes and comments
   before writing negatives.** The first gate draft asserted
   `layout: 'elk'` and `layout: elk # note` were rejected; both are accepted by
   design (same YAML string, comment stripped). Two assertion migrations per
   slice from re-deriving leaf semantics instead of reading the shared reader.

## D-442 agentflow family-close retro (2026-11-23)

1. **A family can close on two rows when the blocker is one inert pair.** The
   agentflow residual was 2 rows behind `look: handDrawn` + `handDrawnSeed`;
   the whole slice is the rigid envelope gaining two closed leaves with a
   canonical footer order. Prefer this over leaving a family "almost closed".
2. **Closing a family always costs negative migrations.** The RFC 0041 gates
   had pinned the pair as rejected in three places. Re-point every negative at
   a shape that is still rejected (`fontFamily`, `look: neo`, zero seed) and
   move the positive assertions to the new gate in the same commit.

## D-443 inert-leaf batch retro (2026-11-23)

1. **A whole-roster twin probe is the right scoping instrument; the twin must
   strip the *whole config block*, not one key.** The D-441 measurement had
   already produced the "66 twin-drawable rows" figure. Re-running it as a
   temporary C++ probe with two extra variants (config-stripped, and fully
   frontmatter-free) split the 505 rows into 61 config-blocked, 27
   frontmatter-blocked (25 flowchart) and the rest body-blocked. That split is
   what made it obvious that only 6 rows are locally closable this batch.
2. **The probe belongs in `tests/ui` only transiently.** It is a C++ gtest
   binary (so it exercises the real renderer) but it must be deleted before
   the commit; never let a probe target survive into a slice commit.
3. **Four-family-scale admission can share one ledger.** One generator, one
   ledger, one gate file and one exact-path matrix branch covered three
   families with six rows. Splitting per family would have tripled the
   artifact count for the same evidence.

## D-445 c4 layout-config retro (2026-11-23)

1. **An outline projection makes upstream layout knobs genuinely inert.**
   `$c4ShapeInRow` changes SVG row packing, but the terminal c4 projection
   emits statement rows only, so the honest oracle is "delete the call, the
   outline must not change" rather than a config-strip twin (which would be
   byte-identical here).
2. **Keep the disclosure block ordered, not appended ad hoc.** D-432's wrap
   line had its own inline footer; generalizing to a disclosure list with wrap
   first preserved every drawn frame and made the new line placement a
   one-line change instead of a second footer path.

## D-446 state comments retro (2026-11-23)

1. **When the twin keeps the same config, compare whole frames, not art slots.**
   The first oracle sliced the footer using the D-443 convention (config-free
   twin ⇒ no footer). Here the twin keeps its admitted config and therefore
   emits the same footer, so the honest and simpler oracle is full-surface
   equality between the source and the comment-free twin.

## D-447 usecase accessibility retro (2026-11-23)

1. **A missing ledger row turns "drawn" into a matrix error, not a fallback.**
   The renderer change landed before the D-447 branch, so the matrix booked one
   `Expected exact ordinary-code source fallback` error and gaps 48→49. That is
   the expected transient state: land ledger + branch before judging the run.
2. **Accessibility declarations have a family precedent; reuse it.** State's
   RFC 0048 scalar accTitle/accDescr no-op (validated, silent, duplicate-gated)
   transferred directly to usecase, including the block form.

## D-448 xychart labels retro (2026-11-23)

1. **Bisect by fixture variants, not by re-reading a 2,000-line renderer.** A
   six-case probe matrix (positive/negative values × domains × decimals ×
   line/bar) located the failure class ("any bar ≤ 0 with labels") in one
   build-and-run cycle; reading the geometry code alone had suggested three
   different false leads.
2. **Keep the probe out of the commit.** The probe target and file were
   removed before the slice commit; only the gate and ledger remain.

## D-449 state descriptions retro (2026-11-23)

1. **Probe a body family by minimal variants before assuming capacity.** The
   complete-graph probe showed an 8-transition layout wall (shared geometry,
   out of scope), while a two-line probe isolated the `state X: Description`
   grammar gap as the actual blocker for four rows.

## D-443…D-449 batch boundary retro (2026-11-23)

1. **Re-configure before building after a probe target is deleted.** The first
   boundary run already had the probe file removed but the release build files
   were generated before the removal, so ninja failed on the missing
   `test_mermaid_d450_probe.cpp` (`build=1`) and the following ctest booked six
   `NOT_BUILT` entries — six "failures" that were a stale target graph, not six
   failing gates. Always re-run `cmake --preset release` after removing a probe
   and confirm `ctest -N` no longer lists it before reading a boundary run as
   evidence.
2. **Check a reviewer-flagged placement change against the pre-diff path.** The
   D-448 `tiny_span` redirect fixed the vertical one-cell label but also moved
   the already-drawn horizontal one-cell label from past the plot edge to the
   bar tip. No fixture covers that shape, so the matrix stayed byte-identical
   while an arbitrary drawn input changed. Restricting the redirect to
   `!chart.horizontal` restored the old profile; a horizontal one-cell probe now
   pins the shared outside column.
3. **A quote-blanked shape view cannot supply a payload.** Usecase validated
   `accTitle:`/`accDescr:` values from the blanked view, so the canonical
   `accTitle: "..."` spelling fell back. Keys are matched on the blanked view;
   the value must be read from the raw statement.
4. **A slice's gate set must include every family census gate, not just the
   new cohort gate.** The D-445…D-449 targeted runs passed while five older
   gates kept pinning the admitted rows as whole-source; only the release
   boundary surfaced them, costing an extra full build+ctest cycle. Run the
   family's existing census gates (`MermaidE2aCohort`, `MermaidE2bCohort`,
   `MermaidCapabilityReport.MermanXY102`, the per-family `*Cohort` gates) in
   the same targeted invocation as the new gate.
5. **Check protected observation rosters before admitting a corpus row.** The
   horizontal D-448 twin breaks the protected E6c `merman_xy102` observation
   (`source_fallback`), which is a reconciliation only the TL can authorize.
   Grep the protected rosters for the fixture path/sha in the same step that
   the ledger is written, so the slice lands with its final scope instead of
   being narrowed at the boundary.

## D-450/D-451 quadrant-init and c4-styling retro (2026-11-23)

1. **A disclosure footer can collide with the source-echo guard.** Both new
   gates rejected any frame line starting with the admitted marker
   (`%%{init`, `UpdateElementStyle(`), which also matched the disclosure lines
   themselves and failed the oracle for every admitted row. Guard on
   "marker and not a disclosure" (`not applied`) instead of the bare prefix.
2. **When a partition empties, pin the empty set instead of deleting it.** The
   quadrant `init_gated` 5 and c4 `styled_excluded` 9 populations both moved
   entirely into their own cohort ledgers. Keep the partition check in the
   family gate with the new zero count (plus the owning-ledger skip list in the
   generator); deleting it would let a future non-admitted row slip through
   unasserted.
3. **A view helper over a temporary is a release-only bug.** In the D-451
   attribute loop, `trim_view(args[i].substr(0u, eq))` bound a `string_view`
   into the temporary produced by `std::string::substr`; the debug binary kept
   the memory alive by luck, while the release binary emitted corrupted
   attribute names (`"red"olor`). Bind a named `std::string_view arg = args[i]`
   and slice the view instead. This is exactly the class of defect the
   release boundary exists to catch.

## D-452/D-453 treemap-classdef and venn-style retro (2026-11-24)

1. **A dedupe key derived from the rendered line rejects distinct inputs.** The
   venn style statement's disclosure line omits values, so keying duplicate
   detection on that line rejected the legitimate pair `style A fill:#fff` +
   `style A fill:#000`. Key duplicates on the normalized parsed statement
   (targets plus ordered key=value pairs) in the renderer *and* in the
   generator, so the two mirrors agree on what "duplicate" means.
2. **A forwarded `string_view` parameter is a shared mutable contract.** The
   venn duplicate set had to be threaded from the body loop into
   `venn_style_statement`; adding the parameter was mechanical, but the
   release-only dangling-view lesson from D-451 says to confirm every new
   `string_view` is either bound to a named local or lives for the whole call.
   `props_text` slices `content`, which the caller owns — no copy needed.
3. **Cross-check an emptied partition against its owning ledger.** When D-453
   moved the five venn body-style rows into their own ledger, the family gate's
   `body_style_gated` partition became a vacuous `EXPECT_EQ(0, 0)`. Add a
   membership assertion against the owning ledger's row set, so a row that
   disappears from both ledgers fails the gate instead of passing twice.
4. **Generator/C++ identifier charset drift is invisible until a fixture hits
   it.** The venn generator rejected `-` in style targets while the renderer
   accepted it; no cohort row exercised the difference. Compare the two
   charsets whenever a new identifier validator is added, and pin one negative
   per charset difference in the gate.

## D-454 timeline nested-directive retro (2026-11-24)

1. **A family-local parser fix is not enough when a shared peek misidentifies
   the header.** The renderer accepted the joined directive, but the source
   never reached it: `classify_mermaid`'s header peek stopped on the
   directive's continuation line (`'cScale0': '#ff0000'`) and classified the
   source as Charts. Diagnostic order that works here: (a) scratch probe that
   prints the rendered frame, (b) stderr markers at each renderer reject
   point — their absence localizes the reject to the dispatch layer, before
   any family code runs.
2. **A comment-scanning body loop is not a directive-aware loop.** The
   timeline body parse skipped `%%`-prefixed lines and therefore parsed a
   spanning directive's continuation lines as diagram statements. Return the
   body tail offset from the visual-config pass instead of re-deriving it.
3. **Twin builders must delete the whole directive, not its opener.** The
   RFC 0047 gate's twin removed `%%{`-prefixed lines; a spanning directive
   left its payload behind and the "twin must itself draw" assertion caught
   it. Any twin/oracle that strips a construct must strip the construct's
   extent.
4. **Empty partitions need ledger cross-checks (again).** The timeline
   cohort's `themed_excluded` went 2→0; the 0047 cohort pinned 5→7 and the
   family gate pins were migrated in the same slice so no row can vanish
   between the two ledgers.
5. **A duplicate rule must name its key, and the mirror must share the key.**
   The renderer keyed nested-leaf duplicates on identity (`outer.key`) while
   the Python mirror keyed them on the disclosure line (which includes the
   value), so two directives setting the same leaf to different values were
   predicted-draw by the generator and gated live. When a dedupe rule exists
   in two languages, pin the one shape where the two candidate keys disagree
   (same identity, different value) in the shared probe artifact.
6. **A prospective bound must be checked against the prospective size.** The
   mirror tested `len(buffer) > 2048` before appending while the renderer
   tested `size + piece + 1 > 2048`; a single long continuation line split the
   two. Bound checks on both sides must be written as the same expression, and
   the over-bound case belongs in the probe artifact.
7. **Extending an aged ledger needs its own review record.** The RFC 0047
   cohort ledger bound the new probe artifact to the 2026-11-22 RFC review.
   Add a `review.decisions.<id>` record per later extension (reviewer identity,
   date, scope, verdict) and make the top-level verdict the conjunction, so a
   green ledger never claims an older sign-off for newer rows.

## D-455 tree-view residual retro (2026-11-24)

1. **A raw echo of a valid body can be indistinguishable from the native
   frame.** The tree-view projection transcribes node rows verbatim, so
   `native_frame`-style signature checks cannot tell a fallback echo from a
   native frame for well-formed bodies; `draws()` (mermaid-language view vs
   text-language view) is the discriminator. Keep the native-frame assertion
   for positive cases only where the projection adds or strips content.
2. **Older gates pin the old answer, and that is the point.** Two family gates
   encoded "empty `icon()` gates" and "a trailing-space fence never reaches a
   renderer". Both were flipped with the decision recorded in place; the
   trailing-text fence (`---x`) was added as the replacement negative so the
   gate still proves the boundary rather than shrinking.
3. **A generator's exclusion prose is a durable claim.** D-434's ledger said
   the trailing-space row "never reaches a family renderer"; D-455 makes that
   false. The exclusion text is authored in the generator, so it was updated
   and the ledger regenerated in the same slice — otherwise the register would
   carry a silently stale claim.
