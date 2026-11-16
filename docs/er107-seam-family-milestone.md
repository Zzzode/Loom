# ER107 emitter test-seam family — milestone closure (D248→D279, gates D282)

Status: **CLOSED (2026-11-16).** Six test-only seams now regenerate every
component of the immutable ER107 proposal assets from the raw `.mmd`
originals, each guarded by a native 107-case canonical + sha256 differential.
No production render, layout, palette, parser or public-contract behavior
changed; every symbol is reachable only through the
`loom::ui::mermaid::testing` facade.

## 1. The six seams — final form

| # | Seam (testing facade) | Ticket | Output | Decisive property |
|---|---|---|---|---|
| 1 | `er_whole_scene_json_for_testing` | D248 | 6-key `whole_scene_model` from the production parse→IR path | The only seam that calls a production parser; its output is embedded byte-exact into the terminal model |
| 2 | `er_structural_witnesses_json_for_testing` | D248 | 7 span/tiling witness kinds (fixed spans + physical lines + header) | UTF-8 **byte** offsets, not code points |
| 3 | `er_semantic_witnesses_json_for_testing` | D251 | 24-kind parallel semantic lexer, 4222 witnesses across 107 cases | Independent of production `ErCursor`; marker-boundary table mirrored per-entry |
| 4 | `er_source_model_json_for_testing` | D271 | 16-field `source_model` | Parallel record builder sharing the D251 grammar primitives |
| 5 | `er_source_binding_json_for_testing` | D275 | 20-field `source_binding` | 8 derived fields + 12 explicit `ErSourceProvenance` optionals (null-on-unset, no fs/manifest/git/env) |
| 6 | `er_terminal_model_json_for_testing` | D279 | 14-field `terminal_model` | Pure source→JSON; **no production layout/palette return value** — hues/roles/tokens are closed constexpr tables, widths are character counts, the only coordinate key is a constant null |

Per-seam native gtest evidence (all in
`tests/ui/test_mermaid_er107_emitter_contract.cpp`): whole-scene, structural,
semantic, source-model, source-binding and terminal each reproduce all 107
pinned components under `class481_canonical` and match the pinned sha256;
plus UTF-8 offset, fail-closed, zero-budget, null-provenance,
canonical-sort anti-silence and synthetic 32-cell wrap tests. Roster
tripwires: statements 391, entities 362, relationships 253, attributes 287,
subgraphs 1, obligations 181; terminal presentations 362/253/1, captions
1764, notices 871.

## 2. Closure-gate evidence (D282, run once, fresh this round)

- **Full OfficialMatrix (2145 pinned originals × 5 frames).** release
  27.0 s and debug 241.5 s, both PASS; report JSON byte-identical across
  presets, sha256
  `c906c74afddc02efa2bc311399ab6d337fb6dcf2318e6528e78d85067e09dc25`
  (unchanged since D236) — direct proof of zero production drift.
- **ASan+UBSan.** Changed ER surface, 92 tests, zero sanitizer findings:
  emitter 18, er107_source107 7, relations `MermaidEr*` 67. The gate caught
  a real test-side heap-use-after-free (a `JsonVal` view used after its
  local `JsonDoc` was destroyed; non-ASan builds false-green), fixed by
  keeping the doc alive. Giant gates Class285 (723.5 s / 471 MiB) and
  Config110 (990.3 s / 730 MiB) are ASan-infeasible on time, not memory —
  logged as separate test-architecture debt (engineering-efficiency
  Open work #6).
- **Serial `-j1` release gates.** Config110 3 tests 13.40 s, Class285 1
  test 8.85 s, Block58 3 tests 0.48 s, Er107 3 tests 13.30 s; all RC 0.
- **Targeted debug loop (no concurrent load).** terminal 107-case
  differential 1013 ms; full 18-test emitter binary 4957 ms;
  er107_source107 112.9 s; relations_semantics 269 tests 660.5 s.

## 3. Frozen-gate position (all unmoved)

Whole-original @240: **1232 drawn / 913 source_fallback / 0 error**
(2145 denominator). Body geometry: 372 drawn / 173 fallback / **48 frozen
gaps**. Class285: 260/25/0. Flow: 160/240. ER: 175/31/0; ER107 roster
stays **107**. Observation total 2738; `integrity_errors` empty. Full
matrix/ASan/serial deferred gates are now discharged; the broader
107→113 re-baseline is the next, separate milestone.

## 4. Downstream 107→113 atomic re-baseline — blocker, typed

**One-line conclusion: this is a work-sequencing/atomicity constraint, not a
TechLead decision and not a production shared-contract change — and its
original "missing generator tooling" blocker is now resolved.** D242 had
blocked the re-baseline because no checked-in ER107 proposal generator
existed; the six seams above are that generator, each sha-pinned over 107
cases. What remains is the atomic landing itself: the ER-local
`parse_plain_er` admission (group-id edge endpoints + in-group `direction`)
cannot ship alone — it books 6 matrix errors and moves the frozen roster
48→54 under the retained-null ER206/ER107 authority — so it must land in a
single commit together with the six new case artifacts produced by these
seams, the dual `kPins<107>`→`<113>` tables, the `array<bool,107>` capture
set, the hardcoded-107 sites, the global/asset SHAs and the new matrix
byte-gate sha (existing 107 pins stay byte-valid; only the 6 new pins,
aggregates and file-level SHAs move). Proposed segmentation, still one
atomic commit: (1) generate the six SELECTED cases' components with the
seams; (2) re-baseline pins/SHAs/counts; (3) re-apply the parser admission
from `/tmp/d237_probe/d237_backup/`; (4) run the full OfficialMatrix to
confirm the six are admitted under the re-baselined authority with the
deliberately-updated frozen sha. No new decision is required (D242 already
authorized it); no production module beyond the ER-local parser is touched.
