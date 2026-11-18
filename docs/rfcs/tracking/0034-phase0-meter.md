# RFC 0034 — Phase 0 evidence: naming the ordinal-198 render-side refusal

Status: **Phase 0 complete (measured).** This is the hard prerequisite for any
Phase 1 `src/` change under `docs/rfcs/0034-er-handdrawn-nested-whole-source-admission.md`.
It names the exact meter and gate function that refuse the whole source of
ER206 ordinal 198, reproduces the result on both build presets, and states the
implication for Route A vs Route B. It gives evidence and a lean only; the
route decision is deferred to a Phase-1 design review and is not made here.

All measurements used existing test-only seams. **No production file under
`src/` was modified**, no protected asset was touched, and the temporary probe
was removed before the evidence commit.

## 1. Probe placement and method

The probe was a temporary gtest in
`tests/ui/test_mermaid_relations_semantics.cpp`
(`MermaidErSubgraphSourceProfile.TemporaryRfc0034Phase0RefusalProbe`), deleted
after capture. It drove only reviewed, already-exported seams:

- `loom::ui::mermaid::testing::render_mermaid_with_work(source, fallback,
  false, Work&, FinalEmissionTrace*)` — the exact public render pipeline with
  caller-controlled budget and passive metering (forwarded to
  `detail::render_mermaid_with_budget`, `mermaid_impl.cpp:6858`).
- `testing::class_relations_rows(source, work, false)` — forwards directly to
  `detail::relations_layout` (`mermaid_relations_core_impl.cpp:8908`).
- `testing::layout_succeeds(source, false, work)` — forwards to
  `detail::layout_mermaid_content` (`mermaid_impl.cpp`), i.e. the layout stage
  including packed validation and prepay but excluding final emission.
- `testing::er_whole_scene_json_for_testing` — the parser-only seam, used to
  isolate parse cost.
- `testing::FinalEmissionTrace` — passive finish/ElementWork meter
  (`mermaid.cppm:160-183`). An all-default trace after a failed render means
  `finish_native_layout` returned at its `if (!layout) return {}` guard
  (`mermaid_impl.cpp:5992`), i.e. the failure is upstream in layout, never in
  final Element emission.

The fixture under test is the immutable pinned original
`tests/fixtures/mermaid_official/cases/er-diagram/visual-check/nested-er-subgraphs-with-direction-override-handdrawn-look.mmd`
(532 B whole source; the body after the closing `---` fence is 222 B). Two
budgets were injected per source: the production `testing::kWorkLimit =
1'000'000` and an enlarged 128,000,000 (equal to the test scene limit), plus a
binary search for the minimum succeeding budget.

## 2. Reproduction commands

```bash
# debug
LD_LIBRARY_PATH=/home/linuxbrew/.linuxbrew/opt/openssl@3/lib:/home/linuxbrew/.linuxbrew/opt/curl/lib \
  cmake --build --preset debug -j8 --target test_mermaid_relations_semantics
./build/debug/tests/ui/test_mermaid_relations_semantics \
  --gtest_filter='*TemporaryRfc0034Phase0RefusalProbe*'

# release
LD_LIBRARY_PATH=/home/linuxbrew/.linuxbrew/opt/openssl@3/lib:/home/linuxbrew/.linuxbrew/opt/curl/lib \
  cmake --build --preset release -j8 --target test_mermaid_relations_semantics
./build/release/tests/ui/test_mermaid_relations_semantics \
  --gtest_filter='*TemporaryRfc0034Phase0RefusalProbe*'
```

## 3. Raw readings (identical on debug and release)

Work accounting is deterministic integer debit/credit, so both presets produce
byte-for-byte identical numbers; this was confirmed by running the same probe
on each.

### 3.1 Render outcome and layout-Work spend

| Run | Injected budget | native element | work remaining | work spent |
|---|---:|:---:|---:|---:|
| FULL whole source | 1,000,000 | **no (plain-text fallback)** | 0 | 1,000,000 |
| BODY (frontmatter removed) | 1,000,000 | yes | 314,581 | 685,419 |
| FULL whole source | 128,000,000 | yes | 126,709,306 | 1,290,694 |
| BODY (frontmatter removed) | 128,000,000 | yes | 127,314,581 | 685,419 |

### 3.2 FinalEmissionTrace fingerprint

| Run | styled_entered / preflight_complete / allocation_started / returned_native | layout finish `failed_debit` | ElementWork `active / cap / debit / failed` |
|---|---|---:|---|
| FULL @1M | all false (all-default trace) | 0 | inactive (0/0/0/0) |
| BODY @1M | all false (legacy non-packed rows path) | 0 | inactive |
| FULL @128M | all true | 0 | active / 102,564,352 / **71,840** / **0** |
| BODY @128M | all false (legacy path) | 0 | inactive |

The all-default trace on FULL @1M is the decisive fingerprint: the render
never entered `finish_native_layout`'s metered body, so neither the legacy
layout-Work finish fees nor the ER ElementWork fees were the refusal point.

### 3.3 Minimum succeeding budget (binary search, whole source)

```
highest failing budget  = 1,290,693
lowest succeeding budget = 1,290,694
production cap (kWorkLimit) = 1,000,000
shortfall = 290,694
```

### 3.4 Three-stage localization (whole source)

| Stage (seam) | @1,000,000 | @128,000,000 | cost at 128M |
|---|:---:|:---:|---:|
| A. `relations_layout` | fails, remaining 0 | ok | 1,289,582 |
| B. `layout_mermaid_content` (adds packed validation + prepay) | fails, remaining 0 | ok | 1,290,694 |
| C. final `finish_native_layout` emission | not reached at 1M | ok | ElementWork only 71,840 of 102,564,352 |

Parser-only cost (`er_whole_scene_json_for_testing`, whole source):
**224,715**, and it succeeds at the production 1M budget.

## 4. Named refusal point

- **Meter:** the shared production layout Work, `testing::Work` with
  `remaining = kWorkLimit = 1'000'000` (`mermaid.cppm:94`); the only debit
  primitive is `Work::spend` (`mermaid_cells_impl.cpp:71-78`), which returns
  false and clamps to zero when a fee exceeds the balance.
- **Gate function:** `detail::relations_layout` on the ER path
  (`mermaid_relations_core_impl.cpp:8908`; the ER-specific grouped/active
  section begins at `:8957`). A `work.spend(...)` returns false there, so the
  function returns `std::nullopt`. Stage A already fails, which places the
  refusal before the packed-row validation and `prepay_styled_payload` gates
  in `layout_mermaid_content` (`mermaid_impl.cpp:6835-6842`) and before every
  final-emission fee.
- **Threshold:** the whole source needs exactly 1,290,694 layout Work to
  return a native layout; the 1,000,000 cap is short by 290,694.
- **Propagation to the user-visible fallback:** `layout_mermaid_content`
  propagates the nullopt -> `finish_native_layout` returns at its
  `if (!layout) return {}` guard (`mermaid_impl.cpp:5992`) ->
  `render_mermaid_with_budget` returns nullopt (`:6869`) ->
  `render_mermaid` yields no element -> the markdown code renderer returns
  the raw source as plain text (`markdown_render_code_impl.cpp:106`). This is
  exactly the `wide == ordinary` grid observed in the RFC 0034 feasibility
  probe.

## 5. Confirmation and refutation of the RFC's working hypothesis

The RFC framed the blocker as an "envelope double-parse Work ceiling". The
evidence confirms the **Work-exhaustion** part but refines the mechanism:

- **Confirmed:** it is a hard budget refusal (`remaining` reaches exactly 0),
  not a semantic presentation gate; raising only the layout-Work budget makes
  the same source draw natively. The failure is in the single Work pool shared
  by the envelope parse and the body layout.
- **Refuted / corrected:**
  1. Parsing does not exhaust the budget. The parser alone costs 224,715 and
     fits comfortably. The dominant charge is post-parse ER layout/routing for
     the deep-nested (domain > details) grouped source under an active
     projection envelope (roughly 1.06M of the 1.29M total), not a parser
     blow-up. The historical FS-1b "double parse" label over-attributes the
     cost to parsing.
  2. The ER ElementWork meter (`kErElementWorkLimit = 102'564'352`) is **not**
     binding: on the successful run it consumes only 71,840 with zero failed
     debits. The binding constraint is the legacy 1M layout Work.
  3. It is not specific to the `handDrawn` value (the `neo` look fails
     identically, recorded in the RFC 0034 feasibility probe); the active
     projection envelope plus deepest nesting is the discriminator.
- **Negative result stated plainly:** there is no semantic reject branch for
     this source; every observed failure is the integer layout-Work balance
     reaching zero.

## 6. Implication for Route A vs Route B (evidence and lean only)

Recall the two candidate capabilities in RFC 0034: Route A removes the
envelope/body double charge by reusing the envelope parse; Route B grants a
bounded, separately-metered allowance to the active-projection envelope path.

- The largest saving Route A could possibly return is bounded by the
  parser-only cost, 224,715 Work (and realistically less, because the body is
  still parsed once). Even crediting the full 224,715, the residual whole
  -source need is `1,290,694 - 224,715 = 1,065,979`, which still exceeds the
  1,000,000 cap by 65,979.
- **Lean: Route A alone cannot fit ordinal 198 under the production 1M
  layout-Work cap.** A bounded allowance (Route B) on the layout Work for this
  active-ER envelope path is therefore the stronger candidate; the allowance
  must target the layout Work pool, not the ElementWork meter, which already
  has ample headroom.
- This is not a final decision. Phase 1 must first itemize where within
  `relations_layout` the ~1.06M non-parse cost is incurred for this source,
  size a fail-closed bounded cap, and prove dual-preset byte-identity for all
  2,737 other rows before choosing. A Route-A parse reuse may still be worth
  doing on its own merits (it reduces cost), even if a bounded allowance is
  also required.

## 7. Constraints honored

- No `src/` change on master; the temporary probe was test-only and removed.
- The ten RFC 0033 protected files and all ER107/ER206 frozen assets are
  byte-unchanged (the fixture and the ER206 prior were read only).
- No full OfficialMatrix run; only a targeted gtest on both presets.
- Nothing in Phases 1-4 was started; this document is evidence only and
  production-readiness review remains outstanding.
