---
rfc: 46
title: Layout and display tuning namespace admission (ishikawa, architecture, state)
status: implementable
owners: "@Zzzode"
reviewers: ["agent:design-review", "agent:prr-review"]
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0046 — Layout/display tuning namespace admission

## Summary

D-417 selects the next slice from the remaining capability debt: 11 rows across three
families whose only unadmitted frontmatter is a closed-value layout/display tuning knob —
`config.ishikawa.useMaxWidth` (`true`/`false`) and `config.ishikawa.diagramPadding` (`50`),
`config.architecture.randomize` (`true`/`false`), `.seed` (`42`) and
`.idealEdgeLengthMultiplier` (`3`), and `config.state.useMaxWidth` (`true`/`false`). None of
the three renderers reads these keys; each family's existing frontmatter gate is extended to
admit one named namespace with a closed value table, and the footer discloses that the tuning
was not applied. One phase, three TUs, no shared module.

Target: global **1517 / 628 → 1528 / 617**; formal `drawn_at_240` **1331 → 1342**;
ishikawa **9/4 → 12/1**, architecture **16/4 → 20/0**, state v1 **28/14 → 30/12**, state v2
**52/51 → 54/49**; roster 628 → 617, categories **43 → 41**.

## Motivation

### Selection (D-417, measured 2026-11-22)

After RFC 0045 the roster is 628 fallback / 43 categories. Grouping every row by its
structural config-leaf set separates render-well-from-config rows from body capability:
the remaining `strip`-drawable config rows are dominated by content-changing keys (pie
`donutHole`/`highlightSlice` 8, treemap `valueFormat` 6, cynefin geometry 5, gitGraph
content keys 32, flowchart exact-bit surface 180). The three smallest, cleanest inert
blocks are the ones this RFC takes; they are layout/display knobs, not content.

State theme rows (8, in-body `%%{init}%%`) are deferred to a body-init mechanism, mindmap
look/theme (6) and sankey (17) are body-blocked per D-410 bucket 5c, and the protected /
exact-bit surfaces stay untouched.

### Evidence

- Every one of the 11 rows is `description` + a single `config:` block carrying only the
  namespace with its closed leaves; bodies draw natively at 240 columns once the config
  block is removed (D-410 bucket 5a: ishikawa 3, architecture 4, state 2 + 2).
- **Twin selection (measured, corrected after design review r1):** two rows have a clean
  corpus twin — `ishikawa/pr-check/9-should-render-with-custom-diagrampadding.mmd` is
  body-identical to the drawn `ishikawa/pr-check/1-should-render-a-simple-ishikawa-diagram.mmd`
  (the RFC 0043 ledger already uses fixture 1 as the corpus twin for rows 7/8), and
  `architecture/visual-check/should-render-a-linear-chain-with-an-increased-idealedgelengthmultiplier.mmd`
  is body-identical to the drawn
  `architecture/visual-check/should-render-a-linear-chain-with-default-fcose-knobs.mmd`.
  The other nine use the synthetic config-free twin: the four state rows do have
  body-equal fixtures (`should-render-a-simple-state-diagrams.mmd` /
  `v2-should-render-a-simple-state-diagrams.mmd`), but those twins already carry their own
  E2b `logLevel`/`fontFamily` footer, so no clean corpus twin exists for them; the two
  ishikawa `useMaxWidth` rows and the three architecture boolean/integer rows have no
  body-equal drawn fixture. Corpus twins must be used where they exist.
- Each key is a layout/display knob: `useMaxWidth` is upstream SVG responsive sizing,
  `diagramPadding` the drawn padding, and the architecture knobs select the fcose engine's
  determinism and edge length. The terminal renderers are deterministic text projections
  that never read them; the disclosure says so.
- **Footprint is wider than three TUs.** The four families' existing admission ledgers and
  cohort tests pin the current fallback verdicts and must be updated in the same atomic
  phase (enumeration in Proposal §3): the architecture cohort ledger and its test
  (`frontmatter_gated_count=4` and `GatedArchitecturesStayWholeSource` /
  `VisualGateIsKeyBased`), the ishikawa cohort ledger and test (`frontmatter_gated_count=4`,
  `admitted_count=2`), the RFC 0043 small-family ledger/test (ishikawa four-way partition
  `13/7/2/4/0` and the `ishikawa.useMaxWidth` negative), the RFC 0038 state test
  (`state.useMaxWidth` negative), and the E2a/E2b whole-source scans. The production
  `.cppm` shared parser is untouched.

### Why admission is testable

The established two-part oracle: per-row twin inertness (config-free twin frame plus exactly
the ledger's footer delta) and each family's existing drawn-row gates (RFC 0043 small-family
gate for ishikawa; the architecture family tests; the RFC 0038 state gates), which stay green
only if the new namespace leaves do not disturb envelopes that carry none.

## Goals

- **G1.** ishikawa: admit exactly one `ishikawa` namespace (indent 2) with `useMaxWidth`
  (`true`/`false`) and `diagramPadding` (`50`) leaves at indent 4, each at most once.
- **G2.** architecture: replace the flat "any config key is a fallback" scan with a
  structured config-block admission for exactly one `architecture` namespace with
  `randomize` (`true`/`false`), `seed` (`42`) and `idealEdgeLengthMultiplier` (`3`).
- **G3.** state (v1 + v2): admit `state.useMaxWidth` (`true`/`false`) at indent 4 under a
  `state` namespace at indent 2 **inside `state_source_envelope` only**; the shared
  `detail::parse_envelope` already emits the namespace event, dotted path and indent and is
  not modified.
- **G4.** Disclose every admitted leaf with one footer line appended after the existing
  notices; the families that had no footer (architecture) gain the bounded channel.
- **G5.** No undetected regression: the existing family ledgers, cohort tests and E2a/E2b
  scans are updated in the same atomic phase, every flipped verdict is paired with a new
  negative, and no assertion is deleted or relaxed.

## Non-Goals

- State's 8 in-body `%%{init}%%` theme rows (needs a cross-family body-init design).
- mindmap look/theme (body-blocked), sankey (body dialect), pie/treemap/cynefin (content).
- The protected 43 and flowchart exact-bit 222; the three standing debts.

## Proposal

### 1. Mechanism (three TUs, one namespace shape)

All three gates admit the same shape: a column-0 `config:` block, exactly one indent-2
namespace of a fixed name, and indent-4 scalar leaves from a closed table, each at most
once, everything else fails closed. Values are read through each TU's own scalar helper
(comment strip, dequote) so quoted and commented spellings normalize identically; the
architecture TU has no such helper today (only `trim_copy`/`trim_view`) and gains a
TU-local copy mirroring `ishikawa_scalar`, not a shared one.

- **ishikawa** (`ishikawa_frontmatter_gated`, line scan): the scan gains a
  `namespace_seen` flag and a closed leaf/value table for the `ishikawa` namespace; a
  `config:` block without any admitted leaf still falls back, and the existing indent-2
  `theme` leaf keeps its table.
- **architecture**: the flat "any config key is a fallback" frontmatter scan becomes a
  structural block scan in the same TU. It is deliberately hand-rolled rather than
  switched to `detail::parse_envelope`: the 16 clean rows must keep their current verdict,
  and `parse_envelope` adds description form/limit and duplicate-path rules that could
  change them. The body-side gate (`%%{init`, `classDef`, `style `, `:::`) is untouched,
  and the battery pins the hand-rolled scan's closure.
- **state** (`state_source_envelope` only): a `StateNs` bit and the `state.useMaxWidth`
  leaf, with the same namespace-without-leaf rejection pattern RFC 0045 applies
  (`(GitGraphNs && !Parallel)` there; `(StateNs && !UseMaxWidth)` here).
  `detail::parse_envelope` is unchanged.
- **Composition inside one namespace** is admitted only in the corpus order and disclosed
  in a fixed canonical order independent of source order: ishikawa `useMaxWidth` then
  `diagramPadding`; architecture `randomize` then `seed` then `idealEdgeLengthMultiplier`;
  state `useMaxWidth` alone. A namespace without any admitted leaf falls back. The battery
  pins admit-or-reject and the canonical footer order for every co-occurrence.

### 2. Footer

Per family, appended last: `<Family> native projection v1` then one line per leaf, e.g.
`config.ishikawa.useMaxWidth=false -> display sizing; not applied`,
`config.ishikawa.diagramPadding=50 -> diagram padding; not applied` (65 bytes),
`config.architecture.idealEdgeLengthMultiplier=3 -> layout tuning; not applied` (77 bytes),
`config.state.useMaxWidth=true -> display sizing; not applied`. All lines are under the
96-byte cap; the emission and height guards mirror the existing family paths. Architecture
has no footer channel today and gains one by appending the bounded rows to its output
before the return; the 16 clean rows carry no `config:`, so no footer is appended and their
frames stay byte-identical.

### 3. Reviewed cohort, twin gate and wiring

- Sibling ledger `tools/mermaid/layouttuning-0046-cohort.json` (schema
  `loom_mermaid_layouttuning_0046_cohort/v1`) with the 11 rows, a `family` field, literal
  footer lines, measured per-width states, and a twin per row: `kind: "corpus"` for the two
  rows above (pinned path + sha) and `kind: "synthetic"` (delete `config:` and every
  following indent ≥ 2 line to the next column-0 key) for the other nine.
- `generate_layouttuning_0046_cohort.py`, renderer-free, `--check`-reproducible; its
  structure mirror asserts the namespace/leaf/indent/closed-value rules, the tab and body
  init rejections, and the corpus-twin body equivalence it claims. The synthetic twin's
  "config-free" check is structural (no `config:` line, no namespace/leaf line at its
  indent) — never a whole-source substring search: three architecture descriptions
  legitimately contain the key names in prose, the same trap RFC 0045's code review found.
- Gate `tests/ui/test_mermaid_layouttuning_0046_cohort.cpp` (5 tests): ledger pins and the
  corpus-twin pins; per-row inertness (corpus twin frame + footer, or synthetic twin frame
  + footer); a battery per family (both booleans admitted, quotation/comment
  normalization, unknown namespace, unknown leaf, out-of-table value, deeper indent,
  duplicate leaf, duplicate namespace, empty namespace, empty config, and the
  composition orders above); the existing-family batteries in their updated form; literal
  partition.
- Matrix branch reading per-row states from the ledger (`raw_active_schema` empty).
- **Existing ledgers, generators and tests updated in the same atomic phase** (each flip
  paired with a new negative; nothing deleted or relaxed):
  - `tools/mermaid/architecture-cohort.json` + `generate_architecture_cohort.py`:
    the generator's `gate_kind` mirror becomes the same structural scan as the production
    gate (the flat `theme|look|config` regex cannot yield the new counts);
    `frontmatter_gated_count` 4 → 0, `selection.draw_count` 16 → 20,
    `counts.total` / `counts.predicted_draw` / `counts.predicted_original_draw` 16 → 20,
    the generator's partition pin `(16, 4, 0)` → `(20, 0, 0)` and its measured narrow
    constants extended to the 4 new rows; the ledger's narrative fields
    (`renderer.theme_gate`, the review note's "16 draw / 4 frontmatter-gated") are rewritten
    so they do not become false statements.
  - `tests/ui/test_mermaid_architecture_cohort.cpp`: counts; **a new frame-identity
    assertion** in `CleanArchitecturesDrawOnWholeAndBodyTracks` — for every drawn row,
    `same_surface(whole_wide, body_wide)` plus the absence of the `Architecture native
    projection v1` header and any `config.architecture.` line (the 16 clean rows carry no
    `config:` and must stay footer-free); `GatedArchitecturesStayWholeSource` asserts the
    gated lists are *empty* (non-vacuously, by `EXPECT_EQ(count, 0)` for each list) and the
    4 new rows' inertness is covered by the 0046 gate; `VisualGateIsKeyBased` flips its
    `config: architecture: randomize: false` sample to a positive and keeps a paired
    negative (an out-of-table `idealEdgeLengthMultiplier: 4`); the stale header comment
    ("sixteen clean … four frontmatter-gated") is corrected.
  - `tools/mermaid/ishikawa-cohort.json` + `generate_ishikawa_cohort.py`:
    `frontmatter_gated_count` 4 → 1 (`look: handDrawn`); the RFC 0043 `admitted` list and
    its `admitted_digest` stay byte-identical (2 theme rows) and a new
    `layout_admitted` bucket carries the 3 namespace rows with its own digest, so the
    partition becomes five-way: 13 = 7 drawn + 2 admitted + 3 layout-admitted + 1
    frontmatter + 0 body. The ledger's `theme_gate` narrative and review note are rewritten
    to match.
  - `tests/ui/test_mermaid_ishikawa_cohort.cpp`: counts 4/2 → 1/2 with the new bucket
    asserted; the admitted loop's `config.theme=` assertion stays, the layout bucket gets
    its own `config.ishikawa.` assertion; the stale header comment ("six frontmatter-gated")
    is corrected.
  - `tests/ui/test_mermaid_smallfam_0043_cohort.cpp` (the generator
    `generate_smallfam_0043_cohort.py` is unchanged: `admitted_count` stays 2 and its
    `EXPECTED` stays 43/26/6/11):
    the FamilySpec gains an `expected_layout_admitted` field (3 for ishikawa, 0 for venn
    and railroad), the partition sum becomes five-way
    (`draw + admitted + layout_admitted + frontmatter + body == family_total`) and the
    disjoint-list coverage loop additionally walks `layout_admitted` with its own count
    assertion (otherwise `covered.size()` falls short and the coverage silently shrinks);
    the `ishikawa.useMaxWidth` negative flips for the ishikawa spec only (venn/railroad
    keep it) with a paired out-of-table negative; the test named
    `FamilyLedgersPartitionFourWays` is renamed to five-way. **The RFC 0043 theme-only
    contract stays**: the smallfam ledger keeps its 2 ishikawa rows, `counts.admitted=6` /
    `counts.other=11`, `per_family["ishikawa"]==2`, `twin_kinds` (corpus 4 / synthetic 2),
    the `:238`/`:245` theme requirements, the `:278-286` family-vs-smallfam admitted
    equality and the `:561` row-count pin are all unchanged; only the ishikawa family
    ledger's own five-way partition and the new FamilySpec field move.
  - `tests/ui/test_mermaid_state_0038_cohort.cpp` + `generate_state_0038_cohort.py`: the
    `state.useMaxWidth` negative flips, paired with a negative (e.g. `state.width`), and
    the flip also asserts the footer line
    `config.state.useMaxWidth=<v> -> display sizing; not applied` (not merely `draws()`);
    the generator's "namespaced config.state.* stays closed" exclusion note is corrected
    **without changing its `EXCLUDED_EXPECTED["namespaced config.state.*"]=4` count** — the
    4 rows still do not belong to the 0038 cohort.
  - `tests/ui/test_mermaid_e2a_cohort.cpp`: the whole-source scan gains the RFC 0046 ledger
    in its exclusion set, and the state-side set is pinned the way the git side is (a
    `state_skipped_by_rfc0046` set equality, or an exact `checked == 104`) so over-listing
    cannot pass silently; 108 → 104 rows scanned.
  - `tests/ui/test_mermaid_e2b_cohort.cpp`: the state scanned set drops the 4 rows
    (`checked` 43 → 39) and the `skipped_by_rfc0038` set equality extends to the new
    skipped-by-0046 set.
- Roster and ledgers: `--emit` 628 → 617; roster categories
  `family_residual:ishikawa` 2 → 0, `family_residual:architecture` 4 → 0,
  `family_residual:state-diagram` 9 → 7, `family_residual:state-diagram-v2` 22 → 20,
  `dagre_layout_tuning` 18 → 17; categories **43 → 41** (two emptied categories). The
  matrix family fallbacks move by the same 11: ishikawa 9/4 → 12/1, architecture 16/4 →
  20/0, state v1 28/14 → 30/12 and v2 52/51 → 54/49.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | layout/display tuning namespaces | 11 rows, 3 families | proposed | red-first (11 rows in the current roster as `family_residual`/`dagre_layout_tuning`); then global 1517/628 → 1528/617; formal 1331 → 1342; gaps 48, integrity 0; narrow measured per row; dual-preset sha recorded; independent P1 code review APPROVE |

## Production Readiness Review

Completed at the `implementable` gate by an independent PRR agent.

### 1. Correctness and tests

- Every new behaviour has a new test: the five-test cohort gate (ledger pins incl. the two
  corpus-twin pins, per-row inertness, per-family batteries, literal partition) and one
  hermetic `--check`; no assertion is deleted or relaxed — every flipped verdict is paired
  with a new negative, and the architecture family gains a **new** frame-identity assertion
  for its 16 clean rows (`same_surface(whole_wide, body_wide)` plus footer absence) because
  the existing architecture tests did not protect the no-footer invariant.
- Flakes: none added. Serial ctest total: **4681 → 4687**.
- The synthetic-twin config-free check is structural, not a whole-source substring search
  (three architecture descriptions mention the key names in prose).

### 2. Build system and module discipline

- N/A for BMI: three `.cpp` body units (ishikawa, architecture, `relations_core` state
  gate); no `.cppm`, import or declaration change; `detail::parse_envelope` is untouched.
  Body edits recompile only those objects.

### 3. Rollback and compatibility

- One atomic commit including the three gates, the sibling ledger and generator, the
  updated family ledgers/generators/tests, the matrix branch and the roster; a revert
  restores 628 and every pin. No persisted data, wire, CLI or settings change.

### 4. Observability / 5. Documentation / 6. Platform

- The footer line per key is honest ("display sizing" / "diagram padding" /
  "layout tuning; not applied"); the renderers provably never read the keys. Architecture
  gains a bounded footer channel without touching the 16 clean rows' frames (asserted).
  Documentation impact is enumerated (d417/d418/decisions/README/d117 from the current
  census/efficiency retro). Debug + release `-Werror` on the Linux box; ASCII output;
  macos-14 CI is not a gate; no new fetch.

### Review sign-off

| Role | Reviewer | Date | Verdict |
|---|---|---|---|
| Design | agent:design-review | 2026-11-22 | approve (r3) |
| Production readiness | agent:prr-review | 2026-11-22 | approve (1 MAJOR + 9 MINOR + 7 NIT folded) |
| Code (per phase) | pending | — | — |

## Rollout and rollback

One atomic commit including the ledger, generator, gate, matrix branch and roster; a revert
restores 628 and every pin. No persisted data, wire, CLI or settings change.

## Drawbacks and risks

- Three mechanism surfaces in one phase; the family batteries and the twin gate pin each.
- A disclosed upstream behaviour the terminal cannot emulate (layout tuning not applied).

## Alternatives considered

1. Do nothing — leaves 11 twin-proven rows on fallback.
2. Pie `donutHole`/`highlightSlice` (5) — rejected: changes what is drawn.
3. State in-body init themes (8) — deferred: a cross-family body-init mechanism, not a
   config admission.

## Testing and verification plan

- New gate (5 gtests) + one hermetic `--check`: registered like its siblings —
  `add_executable(test_mermaid_layouttuning_0046_cohort …)` / `target_link_libraries` /
  `gtest_discover_tests(… PROPERTIES TIMEOUT 600 LABELS "mermaid")`, plus
  `add_test(NAME MermaidLayouttuning0046Cohort.GeneratorIsReproducible COMMAND python3 -I
  …/generate_layouttuning_0046_cohort.py --check …)` with `LABELS "mermaid" TIMEOUT 120`.
  `ctest -L mermaid` delta **+6**; full debug ctest 4681 → 4687 executed, with the complete
  stdout and exit code captured to `/tmp/loom-logs/ctest-full-dbg-0046.log` and cited with
  its sha256 in the closure doc.
- The updated family gates (architecture incl. the new frame-identity assertion, ishikawa,
  smallfam 0043, state 0038, E2a, E2b) are the drawn-row regression oracles and must stay
  green.
- Dual-preset matrix byte gate and roster `--emit`/`--self-test`/`--matrix` at closure.

## Documentation impact

- [ ] `docs/dev/d417-next-family-layout-tuning.md` — selection (twin evidence corrected)
- [ ] `docs/dev/d418-layout-tuning-rfc0046-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — namespace admission entry, and the existing
      "`state.useMaxWidth` stays closed" sentence is amended
- [ ] `docs/README.md` — RFC index row status
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — ishikawa/architecture/state rows
      corrected **from the current census** (the state rows are stale at 70/33 and 23/19,
      not derived by subtracting from them)
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro (capture cost of the new gate
      and any rework loop)

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: one combined ledger vs three family ledgers? | design review | resolved: one sibling ledger with a `family` field (RFC 0040 precedent) |
| OQ-2: are closed value tables (not "any boolean") right for `seed`/`diagramPadding`? | design review | resolved: closed literals only, as every prior cohort |
| OQ-3: architecture mechanism — `parse_envelope` or hand-rolled? | design review r1 | resolved: hand-rolled structural scan in the TU (M3); `parse_envelope`'s description/limit rules could change the 16 clean rows |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR.
- Phase: one atomic phase with its review record.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | — | — | draft | Created provisional from D-417 |
| 2026-11-22 | r1 | agent:design-review | REQUEST-CHANGES | Census, row selection, closed value sets, arithmetic and the inertness argument all verified independently. 2 BLOCKER: the "no body-equivalent drawn twin (0/11)" evidence was false — ishikawa `diagramPadding` and architecture `idealEdgeLengthMultiplier` have clean corpus twins already used by sibling ledgers → corpus twins adopted for those two, synthetic for nine (the state four have body-equal fixtures but with their own E2b footers); and the "existing batteries unchanged" claim was false — the architecture/ishikawa/smallfam/state ledgers and tests, plus the E2a/E2b scans, pin the current fallbacks and are now enumerated as same-phase updates with paired negatives. 3 MAJOR: the state change is confined to `state_source_envelope` (`parse_envelope` untouched); architecture needs a new TU-local scalar helper; the architecture mechanism is resolved to a hand-rolled scan. 4 MINOR + 2 NIT folded (composition order, exact footer literals, 43→41 categories, reviewer record, log path). All folded |
| 2026-11-22 | r2 | agent:design-review | REQUEST-CHANGES | Re-verified B1/M1-M3. One load-bearing item still open (B3): the ishikawa admitted bucket could not move from 2 to 5 without breaking the RFC 0043 theme-only contract (smallfam pins at `:238/:245/:263/:278-286/:561`); the folded text already used a separate `layout_admitted` bucket, and it is now explicit about every unchanged smallfam pin and the five-way partition. Two MINORs (roster category numbers 9→7 / 22→20 vs the matrix family fallbacks; architecture `draw_count`/`predicted_draw` enumeration) and three NITs folded |
| 2026-11-22 | r3 | agent:design-review | **APPROVE-TO-ACCEPT** | Every consumer of the ishikawa/smallfam pins re-checked and consistent (partition 13 = 7 + 2 + 3 + 1 + 0; smallfam generator unchanged); roster/architecture numbers and all NITs verified; E2a 108→104 and E2b 43→39 reproduced exactly. Gate: provisional → accepted |
| 2026-11-22 | PRR | agent:prr-review | REQUEST-CHANGES | 1 MAJOR: the 16 clean architecture rows' byte-identical frames were not guarded by any test (the existing gate computed both surfaces without comparing them; the matrix only counts verdicts) → a new frame-identity assertion is specified. 9 MINOR: the smallfam disjoint-list coverage loop; the architecture generator's flat `gate_kind` regex; stale narrative fields in both family ledgers; the empty-gated-list disposition; the E2a state-side over-listing pin; README/d117 staleness; the CMake registration and timeouts; the state 0038 `EXCLUDED_EXPECTED` count must stay 4. 7 NIT: venn/railroad naming, canonical footer order, structural twin check, footer assertion on the state flip, stale test names/comments, efficiency retro, complete family-fallback list. All folded |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | (this commit) |
