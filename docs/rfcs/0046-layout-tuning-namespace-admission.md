---
rfc: 46
title: Layout and display tuning namespace admission (ishikawa, architecture, state)
status: provisional
owners: "@Zzzode"
reviewers: []
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
**52/51 → 54/49**.

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
- No drawn fixture in any of the three families is body-equivalent (0/11 measured), so all
  rows use the synthetic config-free twin.
- Each key is a layout/display knob: `useMaxWidth` is upstream SVG responsive sizing,
  `diagramPadding` the drawn padding, and the architecture knobs select the fcose engine's
  determinism and edge length. The terminal renderers are deterministic text projections
  that never read them; the disclosure says so.

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
  `state` namespace at indent 2 in the existing `parse_envelope` gate.
- **G4.** Disclose every admitted leaf with one footer line appended after the existing
  notices; the families that had no footer (architecture) gain the bounded channel.
- **G5.** No regression: existing look/theme/ELK tables keep their verdicts; each family's
  drawn rows keep their frames; dual-preset matrix byte-identical.

## Non-Goals

- State's 8 in-body `%%{init}%%` theme rows (needs a cross-family body-init design).
- mindmap look/theme (body-blocked), sankey (body dialect), pie/treemap/cynefin (content).
- The protected 43 and flowchart exact-bit 222; the three standing debts.

## Proposal

### 1. Mechanism (three TUs, one namespace shape)

All three gates admit the same shape: a column-0 `config:` block, exactly one indent-2
namespace of a fixed name, and indent-4 scalar leaves from a closed table, each at most
once, everything else fails closed. Values are read through each TU's existing scalar
helper (comment strip, dequote) so quoted and commented spellings normalize identically.

- **ishikawa** (`ishikawa_frontmatter_gated`, line scan): a state machine gains
  `namespace_seen` and a leaf set; `config:` without a leaf still falls back.
- **architecture**: the frontmatter scan becomes a structural block scan; the body-side
  gate (init/classDef/style/`:::`) is unchanged.
- **state** (`state_source_envelope`, `parse_envelope`): a `StateNs` bit plus the
  `state.useMaxWidth` leaf, mirroring RFC 0045's explicit
  `(StateNs && !UseMaxWidth)` rejection.

### 2. Footer

Per family, appended last: `<Family> native projection v1` then one line per leaf, e.g.
`config.ishikawa.useMaxWidth=false -> display sizing; not applied`,
`config.architecture.idealEdgeLengthMultiplier=3 -> layout tuning; not applied`,
`config.state.useMaxWidth=false -> display sizing; not applied`. All lines are under the
96-byte cap; the emission and height guards mirror the existing family paths.

### 3. Reviewed cohort, twin gate and wiring

- Sibling ledger `tools/mermaid/layouttuning-0046-cohort.json` (schema
  `loom_mermaid_layouttuning_0046_cohort/v1`) with the 11 rows, each with its synthetic
  config-free twin (delete `config:` and every following indent ≥ 2 line to the next
  column-0 key), literal footer lines and measured per-width states.
- `generate_layouttuning_0046_cohort.py`, renderer-free, `--check`-reproducible; its
  structure mirror asserts the namespace/leaf/indent/closed-value rules and the tab/init
  rejections the gates apply.
- Gate `tests/ui/test_mermaid_layouttuning_0046_cohort.cpp` (5 tests): ledger pins and
  sibling-ledger agreement; per-row twin inertness and exact footer composition; a battery
  per family (both booleans admitted, quotation/comment normalization, unknown namespace,
  unknown leaf, out-of-table value, deeper indent, duplicate leaf, duplicate namespace,
  empty namespace, empty config); existing-family batteries unchanged; literal partition.
- Matrix branch reading per-row states from the ledger (`raw_active_schema` empty).
- Roster and ledgers: `--emit` 628 → 617, categories remeasured.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | layout/display tuning namespaces | 11 rows, 3 families | proposed | red-first (11 rows in the current roster as `family_residual`/`dagre_layout_tuning`); then global 1517/628 → 1528/617; formal 1331 → 1342; gaps 48, integrity 0; narrow measured per row; dual-preset sha recorded; independent P1 code review APPROVE |

## Production Readiness Review

To be completed at the `implementable` gate by an independent PRR agent.

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

- New gate (5 gtests) + one hermetic `--check`; `ctest -L mermaid` delta **+6**; full debug
  ctest 4681 → 4687 executed, with the complete stdout and exit code captured to a fixed
  log path per the TL 2026-11-22 process requirement.
- Each family's existing gates stay the drawn-row regression oracle.
- Dual-preset matrix byte gate and roster `--emit`/`--self-test`/`--matrix` at closure.

## Documentation impact

- [ ] `docs/dev/d417-next-family-layout-tuning.md` — selection
- [ ] `docs/dev/d418-layout-tuning-rfc0046-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — namespace admission entry
- [ ] `docs/README.md` — RFC index row
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — ishikawa/architecture/state rows

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: one combined ledger vs three family ledgers? | design review | proposed: one sibling ledger with a `family` field (RFC 0040 precedent) |
| OQ-2: are closed value tables (not "any boolean") right for `seed`/`diagramPadding`? | design review | proposed: closed literals only, as every prior cohort |
| OQ-3: architecture gate rewrite scope | implementation | the frontmatter scan only; body gating unchanged |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR.
- Phase: one atomic phase with its review record.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | — | — | draft | Created provisional from D-417 |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | (this commit) |
