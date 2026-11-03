# Mermaid ER/Block Work-budget overshoot — read-only root-cause audit and design draft

Status: **read-only design input, no production change.** Produced 2026-11-03 on
the Homebrew LLVM 23.1.2 + libc++ Linux port against HEAD `1e438a31` (after the
A1/A2/E/C test fixes). Nothing in this document raises `kWorkLimit`, relaxes a
rejection, or regenerates a paper/roster. Implementation requires Tech Lead
approval and the two independent reviews listed below.

## 1. Symptom (frozen baseline vs this port)

- Frozen contract (2026-10-28, commit `115070b9`): the ER107 same-factory gate
  admits all **107** originals with one `Work` budget of **1,000,000**; measured
  maximum consumed **992,611** (7,389 headroom).
- This port, current HEAD: **95/107**; the same twelve originals spend exactly
  1,000,000 and fail; passing-route maximum **971,611** (967,861 after the
  reverted `capacity()->size()` probe). Official capture layer reports 90 owned /
  17 errors; Block58 reports 47/58 with eleven errors; one Flow Extra7 source and
  the `MermaidCapabilityReport.OfficialMatrix` frozen pin fail with them.

## 2. This is deterministic accounting, not a platform ordering/UB bug

- Project Linux builds use Homebrew **libc++**, not libstdc++. Measured
  `sizeof(std::string)=24`, `std::string_view=16`, `std::vector<int>=24`,
  `std::optional<std::string>=32` — identical to macOS libc++. Work debits are
  therefore byte-identical across the two platforms; a
  "sizeof differs across ABIs" theory is disproven.
- ASan + UBSan (`-fno-sanitize-recover=all`) on ER/Flow/Block failures report
  nothing and reproduce the same counts. No out-of-bounds, UAF, uninitialized
  read or signed overflow.
- Geometry ordering is total/stable: ports are coordinate loops, candidate
  pairs are index-nested + `stable_sort`, A* breaks ties on a serial/cell index,
  ER barycenter ranking uses integer cross-products, there are no pointer-keyed
  or iterated hash containers on the route path.
- The `capacity()`-based styled-payload charge was probed (six
  `capacity()->size()` edits in `detail::styled_payload_bytes`) and **reverted**:
  it only lowered the passing maximum 971,611 → 967,861 and changed none of the
  twelve results. The overshoot is therefore **upstream, in logical size-based
  fees**, not allocator capacity.

## 3. ER107 consumed-work formula (active neo/handDrawn/htmlLabels profile)

For one original, `Work::spend` (deduct, fail at zero) is charged along:

```
W_total =
    P_classify                       // probe table + frontmatter peek
  + P_parse/envelope                 // relation cursor per source/attribute byte
  + P_prepare                        // rank + topological measure, matrices
  + Σ_scope [
        P_scope_fixed
      + Σ_edge (
            P_edge_fixed
          + P_pair_space             // from.size()*to.size() candidate prepay
          + P_pair_sort              // er_paint sort prepay (bit_width-scaled)
          + Σ_candidate (
                P_candidate + P_port_clearance
              + P_astar(pop, neighbour, push)
              + P_label(cursor scan + clearance)
            )
        )
    ]
  + P_ErPaint_finish                 // cells*8, body/notice/run/row emission fees
  + P_valid_packed_rows              // rows*32+128, then 4*row_bytes + 2*styles
  + P_prepay_styled_payload          // 512 + 6*styled_bytes + 4*source_bytes
```

`er_vector_room<T>()` (ports, A* backing/result, reservation vectors) charges on
every call:

```
fee = (size == capacity ? next*sizeof(T) + 2*size*sizeof(T) : 0) + sizeof(T) + 8
```

### Measured decomposition of the twelve failing sources (reverted temporary
counters; end-of-`relations_layout`, first full-1M attempt)

| source (tail) | bytes | room calls / growths / non-growth | room fee | relations spent | remaining |
|---|---:|---:|---:|---:|---:|
| not-so-simple … handdrawn-look | 521 | 1300 / 197 / 1103 | 60,224 | 980,848 | 18,170 |
| not-so-simple … default | 461 | 1300 / 197 / 1103 | 60,224 | 983,728 | 15,350 |
| not-so-simple … elk-layout | 503 | 1300 / 197 / 1103 | 60,224 | 992,031 | 7,005 |
| not-so-simple … handdrawn | 511 | 1300 / 197 / 1103 | 60,224 | 989,380 | 9,648 |
| complex … neo-dark | 566 | 1300 / 197 / 1103 | 60,224 | 874,460 | 124,513 |
| complex … neo-theme | 554 | 1300 / 197 / 1103 | 60,224 | 872,209 | 126,776 |
| complex … redux-color | 572 | 1300 / 197 / 1103 | 60,224 | 982,790 | 16,177 |
| complex … redux-dark-color | 582 | 1300 / 197 / 1103 | 60,224 | 985,055 | 13,902 |
| complex … redux-dark | 570 | 1300 / 197 / 1103 | 60,224 | 874,856 | 124,113 |
| complex … redux-theme | 558 | 1300 / 197 / 1103 | 60,224 | 872,605 | 126,376 |
| all-relationship-types … redux-color | 580 | 1242 / 123 / 1119 | 62,192 | 891,475 | 107,471 |
| all-relationship-types … redux-dark-color | 590 | 1242 / 123 / 1119 | 62,192 | 894,190 | 104,746 |

The first four and the two `redux-color` complex variants exhaust in
`valid_packed_rows`; the other six exhaust in `prepay_styled_payload`. Total
requirement of the worst source is ≈ **1,015,662** (routing finishes ≈ 590k left,
tail needs ≈ 31.3k, only ≈ 15.6k available).

## 4. Root cause: fees added after the green base

`er_vector_room` and the whole base fee schedule are **byte-identical at
`115070b9`**, which admitted these same sources at ≤992,611. So the regression is
the set of Work fees added on the active-ER path by `cf34725a` + `4b58709c`
(seventeen added `spend` sites in `mermaid_relations_core_impl.cpp`; the Block/
styled files did not exist at the base). Density-scaled sites on the `er_paint`
path are the prime suspects and must be measured per source during
implementation:

- pair sort prepay `pairs·((bit_width(pairs)+1)·(2·sizeof(Pair)+4))+128`
  (`mermaid_relations_core_impl.cpp:5337-5341`);
- candidate-space / counting-sort prepay at `:5159-5165` and `:5305-5319`
  (gated on `note_leader || retry_storage` — confirm whether active ER enters
  these);
- added per-byte relation-cursor and ErPaint emission fees in
  `mermaid_er_impl.cpp` / the ErPaint finish block.

The direct-style legend fee (`legend.size()·(4·sizeof(string)+1024)+256`,
`:5975`) is **not** on the ER path (it requires `direct_styles`, i.e.
classDef/style, which these ER sources lack) and is excluded.

## 5. Proposed change (future, not yet approved to implement)

Single owning module: `src/ui/visual/mermaid_relations_core_impl.cpp` (plus, only
if measurement points there, `mermaid_er_impl.cpp`).

1. **Attribute first (temporary, reverted counters).** For each of the 12
   sources, sum every post-base fee site independently; rank by contribution.
2. **Align only redundant post-base fees.** Reduce or remove a new fee only when
   it (a) was added after `115070b9`, and (b) prepays capacity/work already
   covered by a surviving base fee (double prepay) or an allocation that does
   not occur on the active-ER path. Keep every fee that is the sole prepay for a
   real allocation. Do **not** touch `er_vector_room` or any base-proven fee.
3. **No limit change, no new abstraction, no relaxation.** The 1,000,000 limit,
   the ×6 styled multiplier and all rejection predicates stay.

### Aligning to the frozen 992,611 ceiling

The reprice must make every ER107 source satisfy, on the same factory:

- consumed ≤ **992,611** (rebuild margin against the frozen ceiling, not merely
  under 1,000,000);
- exact-budget rerun succeeds with `remaining == 0`;
- `consumed - 1` rerun is rejected;
- worst measured consumed is reported and the margin to 992,611 is positive for
  all 107.

If no reprice of genuinely redundant post-base fees reaches 992,611 while the
independent accounting review still certifies every remaining fee as a
conservative bound, **stop and escalate** — that means a frozen paper is stale
and must go through the ledger/authority process; the limit must not be raised
and a rejection must not be weakened.

## 6. Matrix / roster impact

- Expected only on ER/Block styled originals: the twelve ER107 sources and the
  nine themed Block58 sources can move `error/fallback → drawn`; the two Block
  structural sources and Extra7 are unaffected (section 8).
- Dual-preset original matrices (240/40/restored) must be compared row-by-row:
  only failure→drawn transitions allowed; **any drawn→failure or new error
  aborts and rolls back.** Body-diagnostic track must not change.
- `tools/mermaid/verify_e6c_gap_roster.py` must pass on debug and release with
  byte-identical matrices. When the renderer-driven counts genuinely change, the
  frozen `MermaidCapabilityReport.OfficialMatrix` pins and the reviewed roster
  are updated **in the same change** through the normal reviewed-ledger flow —
  never hand-edited to make a test green.
- No public API, persisted format, CLI or wire change; blast radius is one impl
  unit's fee arithmetic reached through two call sites (`layout_mermaid_content`
  and the layout-cache miss path).

## 7. Rollback and acceptance

Rollback: single `git revert` of the one impl-unit commit; fees revert to HEAD
and the suite returns to the seven-failure baseline.

Acceptance gates (all required):

1. Debug **and** release `-Werror` builds; each runs full
   `ctest -L mermaid -j8` green for ER/Block/Class/Flow/Cache suites.
2. `MermaidER107Runtime`, `MermaidEr107Source107`, `MermaidOfficialEr107`,
   `MermaidBlockSource58.*` pass with the exact/one-unit-short invariants.
3. ASan/UBSan clean on ER/Flow/Block cases.
4. Row-by-row matrix before/after per section 6; roster verifier 0 mismatch;
   matrices byte-identical across presets.
5. Two independent read-only reviews:
   - `agent:/root/b_work_accounting_review` — certify the repriced fees still
     upper-bound every real allocation/work (no under-counting) per family, and
     that no base-proven fee or the limit was changed;
   - `agent:/root/b_matrix_regression_review` — independently verify dual-preset
     row diffs, 240/40/240 lifecycle, cache hit/miss equality, roster and pins.

## 8. Extra7 and the two structural Block cases are independent

- **MermaidFlowGroupEndpointExtra7.NativeSourcesNeedFivePublishedFrames…**:
  temporary production tracing recorded **zero** hits at any production tail
  gate for this source. The failure is inside the test's own 8,000,000-budget
  physical-mutation controls / oracle BFS (the dense `57-dagre-nested-subgraphs`
  paper), not the 1,000,000 ER Work schedule. Different budget, different layer
  → separate item. Any test-oracle-only change must prove the oracle's physical
  control model is faithful to the production five-frame contract (independent
  source/oracle review), not merely make the assertion pass.
- **Block `…block-arrows-and-spece-statements` and
  `…all-arrow-types-in-9-columns-grid`**: with the ample 768M capture budget
  these still throw "rejected by public factory" before any Work tail gate
  (temporary trace showed no `prepay_styled_payload` hit). They are genuine
  parser/geometry feature gaps (space statements; all arrow types in a 9-column
  grid), deterministic and platform-independent. They need a normal
  source-bound Block feature cohort (P3), not a budget change.

## 9. Non-goals

No `kWorkLimit` increase; no rejection/resource-predicate relaxation; no paper,
snapshot or roster regeneration; no change to `er_vector_room` or base-proven
fees; no implementation before approval of this design and the two reviews.
