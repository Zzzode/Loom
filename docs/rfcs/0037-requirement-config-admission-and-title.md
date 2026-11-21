---
rfc: 37
title: requirement family config admission and title projection
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-11-21
last-reviewed: 2026-11-21
supersedes: ""
tracking: local
---

# RFC 0037 — requirement family config admission and title projection

## Summary

requirement is the largest remaining fallback family with a clean slice: **148
of 874** fallbacks. This RFC admits the three frontmatter input classes those
rows actually carry — a closed `config.look` table, a closed `config.theme`
palette table (both validated and disclosed, never painted), and the top-level
`title:` key, which is **projected** through the shared relation-title seam
rather than ignored. Target: exactly the **120 measured rows** whose only
non-admitted input is those keys, proven per row by a twin gate (frame equality
against the same source with the key removed, plus the bounded disclosure
footer). The 24 `layout: elk` rows, the 2 namespaced `config.flowchart.*` rows
and the 2 malformed envelopes stay whole-source fallback. Family target:
**144 drawn / 28 fallback**; global **1391 drawn / 754 fallback**.

## Motivation

### Evidence

Measured 2026-11-21 on the certified matrix
`build/debug/tests/ui/mermaid-official-matrix.json` (generation `ee75f003…`,
byte-identical across presets; RFC 0035 closure) and the 874-row
`tools/mermaid/fallback_roster.json`.

| Metric | Current | Target | How measured |
|---|---|---|---|
| requirement originals `wide_240` | 24 drawn / **148 fallback** / 0 error | 144 drawn / 28 fallback | `MermaidCapabilityReport.OfficialMatrix` census |
| global `wide_240` | 1271 drawn / 874 fallback | 1391 / 754 | same |
| formal `drawn_at_240` | 1085 | 1205 | certification census |
| mandatory_gaps | 48 (flowchart body_diagnostic only) | 48 unchanged | `tools/mermaid/e6c-mandatory-gap-roster.json` (contains **zero** requirement rows) |
| integrity_errors | 0 | 0 | matrix header |

### Why requirement is the selected family

RFC 0036 (flowchart) was withdrawn after its design review falsified the
blocker composition: flowchart's 240 fallbacks are body capability, geometry
input or frozen gap fixtures. `docs/dev/d397-flowchart-family-selection.md` §6
then measured the next families by stripping frontmatter from each fallback
row and re-rendering:

| family | fallbacks | draws after stripping the frontmatter |
|---|---|---|
| requirement | 148 | 146 |
| git | 77 | 50 |
| state-diagram-v2 | 70 | 25 |
| swimlanes | 46 | 46 (lane topology flattened — wrong geometry) |

requirement is selected: its blockers are frontmatter-only, it needs no new
body capability, and its renderer already projects the whole diagram grammar
through the shared relations core.

### Exact composition of the 148 fallbacks

Measured by a throwaway render probe (never committed; evidence log
`/tmp/req_probe_d397_corrected.log`; method: render every requirement fixture
through the public `render_markdown` path, then re-render a twin with
`look`/`theme`/`htmlLabels`/`title` removed from the frontmatter and compare
the native frame with the same source fenced as `text`):

| class | rows | disposition |
|---|---|---|
| admitted-key-only (the cohort) | **120** | this RFC |
| `config.layout: elk` | 24 | non-goal (layout engine) |
| `config.flowchart.htmlLabels` (namespaced subtree) | 2 | non-goal (F0 keeps every namespaced key closed) |
| malformed envelope (leading newline; indented closing fence) | 2 | stays fallback, fail-closed |
| **total** | **148** | |

Corpus key census of the 120 cohort rows:

| key | values | rows |
|---|---|---|
| `config.look` | `neo` 96, `handDrawn` 24, `classic` 1 | 120 |
| `config.theme` | `neo` 16, `neo-dark` 16, `redux` 16, `redux-dark` 16, `redux-color` 16, `redux-dark-color` 16, `default` 1 | 97 |
| top-level `title:` | `Requirement Diagram Title` | 6 |
| `config.htmlLabels: false` (already admitted by D-066 F0) | — | 8 |

The twin probe confirms **120/120 twin sources draw** once the three keys are
removed, so no cohort row hides a body capability: the body axis is unchanged
by this RFC. 30 of the 120 bodies are byte-equivalent to an already-drawn
corpus body (23 in P1, 7 in P2); the remaining 90 bodies draw natively today
(the twin probe) but have no drawn corpus file of their own — see Drawbacks.

### Why admission is testable

`look` and `theme` select upstream stroke style and colour palette. The
terminal never replicates either for any drawn row, so they can only be
admitted under the RFC 0035 OQ-1 precedent as **validated-and-ignored input
with disclosure**, and the inertness claim is a per-row frame-identity twin:
the frame with the key must equal the frame without it plus the bounded
footer. This is verifiable, unlike "book the unpainted geometry as drawn",
which OQ-1 rejected.

`title` is the opposite case: upstream *paints* it, and ignoring it would
silently drop authored content. The shared relations core already paints
`RelationDiagram::diagram_title` (first row + one blank row, `leading_rows =
2`) for the ER and state families (`mermaid_relations_core_impl.cpp:9138`), so
requirement **projects** the title through the same seam.

## Goals

- **G1.** Promote exactly the measured 120-row cohort (phase split 23 / 91 /
  6), pinned per row in a reviewed ledger.
- **G2.** Fail closed: every unknown or out-of-table value, duplicate key,
  namespaced subtree, `layout`, malformed or non-description envelope, folded
  title and every body-level rejection keeps the whole source on fallback.
- **G3.** No layout or glyph change: production edits are the requirement
  envelope validator plus the title field copy; the title row reuses the
  existing shared projection.
- **G4.** No regression: fallback never grows, errors stay 0, mandatory gaps
  stay 48, dual-preset matrix byte-identical, existing F0 footers byte-identical.

## Non-Goals

- `config.layout: elk` (24 rows) — a different layout engine.
- `config.flowchart.htmlLabels` and any other namespaced subtree (2 rows) —
  D-066 F0 explicitly keeps namespaced keys closed; extending that boundary
  needs its own evidence.
- Theme values with no requirement-corpus row: `base`, `dark`, `forest`,
  `neutral` (they occur 8/17/13/8 times in other families' fixtures only),
  plus `themeVariables.*`, `handDrawnSeed`, `securityLevel`, `curve`, `fontSize`,
  `fontFamily` beyond the F0 `courier` value.
- Painting upstream's stroke style or palette for `look`/`theme` — they stay
  disclosed-only. A future F1 policy RFC owns whether the terminal should.
- The 2 malformed-envelope rows and any body-grammar widening.
- narrow_40 drawing: the disclosure footer already makes admitted narrow rows
  fall back (D-066 F0 `narrow_note`), measured per row in the ledger.
- The three ER107 fail-closed gates (`MATRIX_API_CONFIRMED`,
  `REVIEWED_HELPER_SHA`/`kB3ReviewStatus`, `WRITE_REVIEW_APPROVED`).
- flowchart, gitGraph, swimlane and state families.

## Proposal

### 1. Closed value tables (P1 `look`, P2 `theme`)

The requirement envelope (`requirement_envelope` in
`src/ui/visual/mermaid_requirement_impl.cpp`) gains two leaves beside the
existing `htmlLabels`/`logLevel`/`fontFamily` table. Both are at indent 2 under
a single `config:` block, at most once, exactly like the F0 leaves:

| key | admitted values | rejects |
|---|---|---|
| `look` | `classic`, `neo`, `handDrawn` | any other string, quoting mismatch, duplicate |
| `theme` | `default`, `neo`, `neo-dark`, `redux`, `redux-dark`, `redux-color`, `redux-dark-color` | any other string, duplicate |

The tables are **corpus-bounded**, matching this renderer's documented
convention (`kRequirementNamedColors`: "extend only against new corpus
evidence"). The four upstream palettes without a requirement row are listed
explicitly in Non-Goals rather than silently dropped.

Disclosure footer (ordering fixed so the existing F0 footers stay byte
identical): `logLevel`, `fontFamily`, `htmlLabels`, then `look`, then `theme`:

```
Requirement native projection v1
config.look=neo -> terminal glyph style is Loom-owned; not applied
config.theme=redux-color -> colour palette; not applied
```

### 2. Title projection (P3)

The envelope validator accepts one top-level `title:` and decodes it with the
grammar `parse_envelope` already applies: plain or quoted scalar, non-empty
after trim, printable ASCII (`0x20..0x7e`), length ≤ `kTitleLimit`. Folded or
block forms, tabs, control bytes, blank text and duplicates reject.

`RequirementEnvelope` gains an `std::optional<std::string> title`;
`project_requirement` copies it into `RelationDiagram::diagram_title`, and the
existing shared paint block emits it as row 0 plus a blank row. No new
geometry: the same code path paints ER and state titles today.

The title is **not** disclosed as "not applied" — it is applied, so a footer
entry would be wrong. Its gate is a projection twin: the frame of a titled row
must equal `title row + blank row + frame of the same source with the title
removed`.

### 3. Reviewed cohort, twin gates and wiring

- Ledger `tools/mermaid/requirement-0037-cohort.json` (schema
  `loom_mermaid_requirement_0037_cohort/v1`): one row per promoted fixture with
  `phase`, `twin` kind (`config-free` or `title-free`), the exact expected
  footer lines, and the measured `expected_states` (wide/narrow/restored).
  Per-phase `review` records; a phase counts as approved only when every
  enabled phase has an approving record — the RFC 0035 per-phase rule.
- Generator `tools/mermaid/generate_requirement_0037_cohort.py` (renderer-free,
  `--check` reproducible): partitions the 148 fallbacks into the four classes
  above, pins the denominators 148/120/24/2/2 and the per-phase counts 23/91/6,
  and preserves review blocks across regeneration. It does not re-derive the
  frozen D-204/D-395 partitions in `requirement-cohort.json` or
  `requirement-f0-cohort.json`.
- Gate `tests/ui/test_mermaid_requirement_0037_cohort.cpp`: per row, native
  frame, determinism, `text`-fence non-equality, twin equality with the exact
  footer/title composition, and the fail-closed boundary set (every non-goal
  key and value, duplicates, namespaced subtree, folded title, empty config).
- Matrix branch in `tests/ui/test_mermaid_official.cpp` mapping each ledger row
  to a new reason string; roster migrated with
  `tools/mermaid/fallback_attribution.py --emit` in each phase commit.
- The existing F0 boundary probe in `tests/ui/test_mermaid_requirement_cohort.cpp`
  flips exactly one assertion (`look: handDrawn` now draws) and gains the
  cohort-side negatives; `theme: dark` stays rejected, so the rest of that
  probe is unchanged.

### 4. Named cross-module couplings

These are string/shape couplings that do not fail to compile if changed on one
side only:

| coupling | files that must change together |
|---|---|
| envelope event model (`title` is already parsed and stored by `parse_envelope`) | `mermaid_envelope_impl.cpp`, `mermaid_detail.cppm` |
| `RequirementEnvelope` / `RequirementDiagram` field layout | `mermaid_detail.cppm`, `mermaid_requirement_impl.cpp` |
| shared footer publication and title paint | `mermaid_relations_core_impl.cpp` (`RelationFamily::Requirement` notice branch, title block) |
| census reason string | `tests/ui/test_mermaid_official.cpp`, ledger generator, closure doc |
| roster totals | `tools/mermaid/fallback_roster.json`, `tools/mermaid/e6c-mandatory-gap-roster.json` (must stay unchanged: zero requirement rows) |
| frozen D-204 / D-395 partitions | `tools/mermaid/generate_requirement_cohort.py`, `requirement-cohort.json`, `requirement-f0-cohort.json` (read-only for this RFC) |

Dependency direction is unchanged: the requirement renderer keeps depending on
the shared relations core; no new edges are introduced.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `look` admission | 23 rows (`look` only 15, `look`+`htmlLabels` 8) | proposed | family 24→47 drawn / 148→125 fallback; global 1271→1294 / 874→851; roster regenerated; targeted cohort + F0 tests green; dual-preset matrix byte-identical with new sha recorded |
| P2 | `theme` admission | 91 rows (90 `look`+`theme`, 1 `classic`+`default`) | proposed | family 47→138 / 125→34; global 1294→1385 / 851→760; same gate set |
| P3 | `title` projection | 6 rows | proposed | family 138→144 / 34→28; global 1385→1391 / 760→754; formal 1085→1205; title twin gate green; same gate set |

Closure: full debug ctest once, dual-preset byte gate, RFC `implemented`,
closure doc, design-decisions entry, efficiency retro.

## Production Readiness Review

Completed at the `implementable` gate. Current answers:

- **Correctness & tests** — new per-row twin gate plus a boundary suite; the
  gate is the inertness proof, not a golden snapshot. Known flakes
  (`ServerMain.DirectConnect*`, `E2E_Pty_Handoff*`) are pre-existing and unrelated.
- **Build system** — no new module, no new target, no interface change beyond
  two struct fields in an already-shipped `.cppm` producer; fan-out is the
  existing `mermaid_detail` BMI closure.
- **Rollback** — each phase is one atomic commit (validator table + ledger +
  generator + gate + matrix branch + roster); reverting one phase restores the
  previous census exactly. P3 depends on P1/P2 only through its twins.
- **Observability** — disclosure footer is the user-visible trace; session
  traces and prompt dumps are untouched.
- **Documentation** — design-decisions entry, closure doc, README index,
  efficiency retro.
- **Deletion** — the F0 boundary comment that says "every theme/look row keeps
  whole-source fallback" is corrected in place (it becomes false).
- **Deprecation** — none.

## Rollout and rollback

Phases land in order P1 → P2 → P3, each independently revertible and each
leaving the census self-consistent. No persisted format, wire shape or debug
trace changes. The ledger is additive; the roster is regenerated, never hand
edited.

## Drawbacks and risks

- **Unverified upstream bodies (largest risk).** 90 of the 120 rows have no
  drawn corpus sibling: their bodies draw under our renderer (twin probe) but
  were never compared against upstream output, because upstream's visual-check
  corpus only carries `-default` files for 23 families of bodies. Promoting
  them books *our* projection as drawn on the strength of the D-204 reviewed
  renderer contract, not on an external comparison. Mitigation: the ledger
  records which rows have a drawn body sibling, and the gate proves the
  frontmatter axis only.
- **Colour-only theme admission can be misread as a colour promise.** The
  footer states "not applied" per row and no frame may change; twin gates
  enforce it.
- **Closed palette table excludes four upstream palettes.** A future corpus row
  with `theme: dark` still falls back; the RFC lists that explicitly instead of
  silently accepting it.
- **Title row width.** The shared paint block rejects a title wider than
  `kWidthLimit`; a corpus title is 25 characters, so no row trips it, and the
  gate pins the measured state.

## Alternatives considered

- **Do nothing.** 148 fallbacks remain; rejected because 120 of them are
  frontmatter-only and the pattern (validate, disclose, twin-gate) is
  established by D-066 F0 and RFC 0035.
- **Validate-and-ignore `title`** (the RFC 0036 lesson applied to titles).
  Rejected: ignoring a painted title silently drops authored content, while the
  projection seam already exists and is two field copies. This is OQ-1.
- **Admit `layout: elk` too** (24 further rows). Rejected: it is a different
  layout engine, not an inert key; it would book unimplemented geometry.
- **Admit the namespaced `config.flowchart.htmlLabels`** (2 rows). Rejected:
  D-066 F0 deliberately keeps every namespaced subtree closed; opening it is a
  boundary decision of its own, for 2 rows.
- **Admit the full upstream theme enum** (11 palettes). Rejected for now: this
  renderer's convention is corpus-bounded tables; the four extra palettes have
  no requirement-row evidence. Revisit with a corpus row. This is OQ-2.
- **Body-capability work first (icons, `<br>`, nesting).** Not applicable: the
  requirement family has no such slice; its 148 fallbacks are these four
  classes exactly.

## Testing and verification plan

- New `tests/ui/test_mermaid_requirement_0037_cohort.cpp` (per-row twins,
  boundary suite, ledger schema and per-phase reviews).
- Edited `tests/ui/test_mermaid_requirement_cohort.cpp` (one F0 assertion flip
  plus cohort negatives) and `tests/ui/test_mermaid_official.cpp` (matrix
  branch + reason string).
- Targeted `ctest -L mermaid` per phase; full debug ctest once at closure
  (current baseline 4614 tests); dual-preset byte-identical matrix gate at
  closure and after each phase (record the sha).
- Known timing flakes are pre-existing (`ServerMain.DirectConnect*` 3-second
  TCP timeouts under load; `E2E_Pty_Handoff` deadline); re-run in isolation.

## Documentation impact

- [ ] `docs/dev/d397-flowchart-family-selection.md` §6 — corrected cohort
      measurement (120 admitted; the 2 namespaced rows excluded; title
      projected, not ignored)
- [ ] `docs/dev/d398-requirement-rfc0037-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — requirement config admission and
      title projection entry
- [ ] `docs/README.md` — RFC index
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: project `title:` (recommended) or validate-and-ignore it? | design review | design review verdict; if ignored, P3 drops to 0 rows and the 6 rows stay fallback |
| OQ-2: corpus-bounded theme table (recommended) or the full 11-palette upstream enum? | design review | design review verdict; either way the cohort stays 120 rows |
| OQ-3: per-row narrow_40 disposition under the footer budget | implementation | ledger `expected_states`, measured per row in P1 |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR with a different lens.
- Phases: P1, P2, P3, each atomic with its own review record.
- Graduation: per-phase targeted census; family closure runs full ctest once
  and the dual-preset byte gate.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-21 | — | — | draft | Created provisional from the D-397 §6 measurement and the corrected twin probe |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-21 | — | Draft created (provisional), no code | — |
