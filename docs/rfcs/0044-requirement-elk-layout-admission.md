---
rfc: 44
title: requirement elk layout admission
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0044 — requirement `layout: elk` admission

## Summary

D-413 selects the next slice from D-410 bucket 5: the requirement family's
`config.layout: elk` rows. The requirement envelope gate already admits five inert
`config` leaves (`logLevel`, `fontFamily`, `htmlLabels`, `look`, `theme`) and discloses each
through a bounded footer; this RFC adds one more — a closed one-value `layout` table —
promoting **23 rows** whose bodies are byte-identical to already-drawn non-ELK rows. The
ELK layout engine is never applied; the footer says so.

One phase, one TU (`mermaid_requirement_impl.cpp`), no shared module, no BMI change.
Target: requirement **144 / 28 → 167 / 5**; global **1482 / 663 → 1505 / 640**; formal
`drawn_at_240` **1296 → 1319**.

## Motivation

### Selection (D-413, measured 2026-11-22)

Bucket 5's 374 rows were ranked by text-inertness proof, mechanism cost and rows per
mechanism (D-413 §1). The requirement ELK slice wins: 24 rows sit in one roster category
(`elk_layout_engine`, 38 total), 23 of them have a **corpus twin** — a drawn, non-ELK row
whose body is byte-identical after frontmatter removal — and the TU already has both the
admission channel and the disclosure footer.

Rejected alternatives: `treemap.valueFormat` (changes displayed numbers), architecture
`seed`/`randomize` (seed shape), xychart `labelRotation` (rotation debt), tree-view icons
(foreign glyphs), and smaller one-off slices (mindmap `layout` 3, state `useMaxWidth` 4,
c4 `wrap` 1). git `gitGraph.parallelCommits` (12) is the runner-up.

### Evidence

- requirement: 172 originals, **144 drawn / 28 fallback**. `config.layout: elk` accounts for
  24; all 24 classify as `elk_layout_engine`.
- 15 of the 24 carry only `layout`; 8 add `htmlLabels: false` (already admitted by D-066
  F0), so the two leaves compose in one envelope. One row carries the nested
  `config.flowchart.htmlLabels` map and has **no** corpus twin; the envelope rejects
  namespaced entries, so that row stays exclusive — **23 rows promoted**.
- The remaining 5 fallbacks stay: 2 nested `config.flowchart.*` maps (one also `look`),
  1 title-only envelope (the gate requires a description), 1 body-capability row with no
  frontmatter, 1 ELK row without a twin.
- The native requirement projection is a text tree of requirements/elements/connections;
  it draws no geometry, so the layout engine selection cannot change the frame. The 23
  body-identical twins prove it frame by frame.

### Why admission is testable

The renderer never reads `layout`; the proof is the established two-part oracle: per-row
twin inertness (corpus twin frame plus exactly the ledger's footer delta, wide only) and
the existing RFC 0037 gate's 120 drawn-row frames, which stay green only if the new leaf
does not disturb an envelope that carries no `layout`.

## Goals

- **G1.** Admit exactly `config.layout: elk` at envelope indent 2, once, in a closed
  one-value table.
- **G2.** Disclose it as `config.layout=elk -> external layout engine; not applied`,
  appended after every existing notice line so already-drawn frames keep their byte order.
- **G3.** Promote exactly 23 rows; every other verdict is unchanged (the five other leaves
  keep their tables; any other `layout` value rejects; the nested `config.flowchart.*`
  map keeps rejecting).
- **G4.** No regression: the RFC 0037 120-row cohort keeps its frames cell for cell and
  the dual-preset matrix stays byte-identical.

## Non-Goals

- The nested `config.flowchart.htmlLabels` map, the title-only envelope, the body row.
- Other layout engines, `layoutEngine`, ELK geometry.
- flowchart's 222 exact-bit rows and the 67 protected/frozen rows.
- narrow_40 drawing; the ER107 B3 gates.

## Proposal

### 1. Mechanism (one TU, one new leaf)

In `requirement_envelope` (the event-driven gate used by all requirement admission):

- Extend the field bitset with `Layout = 32u`.
- Inside the config block, at indent 2: `layout` is admitted exactly once and only when its
  scalar (the existing `requirement_inert_scalar`: comment strip, trim, matched-pair
  dequote) is `elk`. Any other value rejects the whole source.
- The notice list gains, **last**:
  `config.layout=elk -> external layout engine; not applied`. Appending last is required:
  existing frames (e.g. `htmlLabels` + `look`) must keep their current line order.
- `config_seen && fields == 0` still rejects; a second `layout` leaf rejects; a `layout`
  at any other indent rejects; namespaced entries still reject.

### 2. Reviewed cohort, twin gate and wiring

- Sibling ledger `tools/mermaid/requirement-0044-cohort.json` (schema
  `loom_mermaid_requirement_0044_cohort/v1`) with the 23 rows, each with its corpus twin
  path + sha (body equivalence verified by the generator), literal footer lines, and the
  measured per-width states.
- `generate_requirement_0044_cohort.py`, renderer-free, `--check`-reproducible; the RFC
  0037 generator's `exclusions` sentence is updated (24 → 23 admitted + 1 remaining) and
  regenerated; its 120 rows do not change.
- Partition arithmetic is literal against the filesystem: 172 = 144 drawn + 23 admitted +
  5 remaining, with the drawn 144 split 120 (0037) + 8 (F0) + 4 (base cohort) + 12
  pre-existing.
- Gate `tests/ui/test_mermaid_requirement_0044_cohort.cpp` (5 tests): ledger schema/review/
  pins and sibling↔0037 agreement; per-row twin inertness + exact footer composition; the
  closed table (elk admitted; `dagre`/`elk-layout`/case variants/empty/duplicate/indent 4/
  column-0 `layout` rejected; nested map still rejected; the four other leaves unchanged);
  the composed-order battery (`htmlLabels` + `layout` emits html then layout; the three
  RFC 0037 leaf batteries keep their expected sequences); the literal four-way partition.
- Matrix branch before the requirement family branches, reason
  `RFC-0044 requirement elk layout reviewed cohort`, `raw_active_schema` empty so the
  generic `originals_drawn` counter credits +23.
- Roster: `--emit` 663 → 640, `elk_layout_engine` 38 → 15, category count unchanged (43).

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `layout` admission | 23 rows, 1 family | proposed | requirement 144/28 → **167/5**; global 1482/663 → **1505/640**; formal **1296 → 1319**; gaps 48, integrity 0; narrow measured per row; dual-preset sha recorded |

Closure: full debug ctest once, dual-preset byte gate, RFC `implemented`, closure doc,
design-decisions entry, efficiency retro.

**Closure-report format (TL 2026-11-22):** every closure report states three numbers side
by side — ① the absolute `source_fallback` total (expected **640**), ② the part beyond a
certified profile or frozen/protected authority (expected **289**, unchanged), and ③ this
phase's net reduction (**23**).

## Production Readiness Review

Completed at the `implementable` gate by an independent PRR agent.

## Rollout and rollback

One phase, one atomic commit; rollback is one revert plus a rebuild/re-measure. No
persisted data, wire protocol, CLI, or settings change.

## Drawbacks and risks

- **A disclosed leaf the renderer cannot paint** — stated by the footer.
- **Ordering sensitivity**: appending the new notice last is what keeps the RFC 0037
  frames byte-identical; the composed-order battery pins it.
- **Category causality**: `elk_layout_engine` is labelled `foreign-engine`; this RFC does
  not change the label — the admitted rows are inert *in this family's text projection*
  (proven by twins), while the remaining 15 rows across other families keep the engine
  debt. The roster's `predicate_sufficiency` recomputes on emit.

## Alternatives considered

1. **Do nothing** — leaves 23 twin-proven rows on fallback.
2. **Admit the 24th (nested `config.flowchart.htmlLabels`) too** — needs namespaced-entry
   support and a synthetic twin; rejected as a second mechanism.
3. **git `parallelCommits` (12)** — smaller and second; RFC 0039 called the namespaced
   `config.gitGraph.*` block semantics debt, so a single-key carve-out needs its own
   argument. Deferred.
4. **treemap `valueFormat` / architecture `seed`** — rejected: content-changing keys.

## Testing and verification plan

- New gate (5 gtests) + two hermetic `--check` entries (new generator + the existing RFC
  0037 generator): expected `ctest -L mermaid` delta **+7**.
- The RFC 0037 gate is the drawn-row regression oracle (120 rows) and must stay green.
- Targeted `ctest -L mermaid`; release matrix; dual-preset byte gate at closure; full
  debug ctest once.
- Roster order: review approved → full rebuild → both matrices → cohort gate green →
  `--emit` → `--self-test` → `--matrix`.

## Documentation impact

- [ ] `docs/dev/d413-next-family-requirement-elk.md` — selection (already written)
- [ ] `docs/dev/d414-requirement-elk-rfc0044-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — requirement layout admission entry
- [ ] `docs/README.md` — RFC index
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — requirement row

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: one ledger or extend the 0037 ledger? | design review | proposed: sibling ledger (0037 is a phase record; its exclusions sentence only) |
| OQ-2: narrow_40 disposition | implementation | measured live, pinned |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR.
- Phase: one atomic phase with its review record.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | — | — | draft | Created provisional from D-413 |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | — |
