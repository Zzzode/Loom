---
rfc: 37
title: requirement family config admission and title projection
status: implementable
owners: "@Zzzode"
reviewers: ["agent:design-review-r1 (a63ae06251ef34362, r1 REQUEST-CHANGES → r2 APPROVE-TO-ACCEPT)", "agent:prr-review-r1 (adaa7ad84dd094605, r1/r2 REQUEST-CHANGES → r3 APPROVE-TO-IMPLEMENT)"]
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
against the same source with every admitted config leaf removed, plus the
bounded disclosure footer; the 6 title rows use the §2 projection twin — title
row + blank row, nothing appended). The 24 `layout: elk` rows, the 2 namespaced `config.flowchart.*` rows
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
| mandatory_gaps | 48 (flowchart body_diagnostic only) | 48 unchanged | `tools/mermaid/e6c-mandatory-gap-roster.json`: all 48 `mandatory_gaps` entries are flowchart; its separate `requirement11_p1p6` / `requirement11_overflow` gate records and the `Requirement11_native_whole_source` aggregate (`mandatory_remaining: 160`) are an already-drawn control, counted only from contract rows, and are unchanged by this RFC |
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
| `config.look` | `neo` 96, `handDrawn` 23, `classic` 1 | 120 |
| `config.theme` | `neo` 16, `neo-dark` 16, `redux` 16, `redux-dark` 16, `redux-color` 16, `redux-dark-color` 16, `default` 1 | 97 |
| top-level `title:` | `Requirement Diagram Title` | 6 |
| `config.htmlLabels: false` (already admitted by D-066 F0) | — | 8 |

The roster's `look_handdrawn` category counts 24 because it also holds
`edge-labels-correctly-when-flowchart-htmllabels-is-false-handdrawn-look.mmd`,
which is one of the 2 excluded namespaced rows; inside the cohort the value is
`handDrawn` 23 (+ `htmlLabels` on 8 of them).

The twin probe confirms **120/120 twin sources draw** once the four keys
(`look`, `theme`, `htmlLabels`, `title`) are removed, so no cohort row hides a
body capability: the body axis is unchanged by this RFC. 30 of the 120 rows
have a **body-byte-equivalent** already-drawn corpus sibling (23 in P1, 7 in
P2) — the source bytes differ by construction (config block, and for 19 rows
the inert `description` text), the bodies and frames do not; the remaining 90
bodies draw natively today (the twin probe) but have no drawn corpus file of
their own — see Drawbacks.

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
  envelope validator (one accept and decode branch), one struct field and one
  field copy; the title row reuses the existing shared projection.
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
- Editing `tests/fixtures/mermaid_official/manifest.json`: its `fixtures[].raw`
  values and `kManifestSha256` are byte-immutable, and the pin is duplicated
  across the protected authority headers. Promotion is expressed as a
  ledger-driven branch in the official-matrix test, never as manifest edits.
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
config.look=neo -> source node style; not applied
config.theme=redux-color -> colour palette; not applied
```

### 2. Title projection (P3)

The envelope validator accepts one top-level `title:` and decodes it with the
**ER family's** existing rule, because that is the rule shared by the two
relationship families and it is the strictest:

- decode with `er_yaml_value(er_strip_comment(value))`
  (`src/ui/visual/mermaid_er_impl.cpp:200-215`), require `Kind::Text`;
- non-empty, at least one non-space "ink" character, every byte printable
  ASCII (`0x20..0x7e`);
- length bounded by `kWidthLimit` (240), which is exactly the paint guard
  (`mermaid_relations_core_impl.cpp:9018-9020` rejects a wider title), so
  **accepted implies width-paintable** — the envelope never accepts a title the
  renderer would discard for width. The same guard also rejects a title whose
  diagram would exceed `kHeightLimit - 2`; that bound depends on the diagram, so
  an accepted title on an over-tall diagram still fails closed with the rest of
  the layout, exactly as an untitled over-tall diagram does. `kTitleLimit`
  (1024) is the generic envelope bound and is deliberately not used here;
- Plain scalars **and** quoted scalars are admitted (ER parity: `er_yaml_value`
  decodes quotes and ER's own title path checks no scalar form, so `title: "X"`
  is a title there too). Folded/block scalars, tabs, control bytes and
  duplicates reject. The corpus titles are plain, so no row depends on this
  branch; it is stated so the boundary is not left ambiguous.

Three title grammars currently exist (ER/Class via `er_yaml_value`; the
flowchart path via `unicode_text`; the generic envelope parse). This RFC does
not unify them; it reuses the ER rule for requirement and states that choice
explicitly so a later unification RFC has the inventory.

`RequirementEnvelope` (non-exported `:detail` partition, so no exported module
interface change) gains one `std::optional<std::string> title` field. The
concrete production edits are: (1) one accept branch in the envelope validator
that decodes `envelope->title` from the parsed `EnvelopeScalar` against the
rule above; (2) one field on the struct; (3) one copy in `project_requirement`
into `RelationDiagram::diagram_title`. The existing shared paint block then
emits it as row 0 plus a blank row — the same path ER and state titles use
today. No new geometry.

The title is **not** disclosed as "not applied" — it is applied, so a footer
entry would be wrong. Its gate is a projection twin: the frame of a titled row
must equal `title row + blank row + frame of the same source with the title
removed`.

### 3. Reviewed cohort, twin gates and wiring

- **Twin definition (one rule, two kinds).** A *config-free twin* is the same
  source with **every admitted config leaf** removed — `look`, `theme` and the
  F0 leaves `htmlLabels`/`logLevel`/`fontFamily` — and with an emptied
  `config:` block dropped too. This matters for multi-key rows: the 8 P1
  `look`+`htmlLabels` rows must not keep an F0 footer in the twin, or the
  expected frame double-counts the footer. A *title-free twin* is
  the same source with the `title:` line removed; it **keeps** `look`+`theme`,
  so it already carries its own footer.
  Expected frames are:
  - config-free / corpus twin rows (114 of 120): `twin frame + the footer
    lines the twin does not already carry`, in the fixed order `logLevel`,
    `fontFamily`, `htmlLabels`, `look`, `theme` (a synthetic twin carries none;
    the 8 F0 corpus twins already carry `v1` + `htmlLabels`, so only the new
    `look` line is appended);
  - the 6 title rows: `title row + blank row + title-free twin frame`, with the
    twin's own footer retained and **nothing appended** (the titled frame is
    the twin's frame with exactly two rows prepended).
- **Corpus twins where they exist.** 30 of the 120 rows have a
  **body-byte-equivalent** drawn sibling (23 P1, 7 P2); the gate compares
  frames, never source bytes (the config blocks differ by construction, and 19
  rows differ in inert `description` text). For those the twin is the corpus
  fixture itself — the `-default` sibling, or the
  `-without-htmllabels-default` F0 sibling (which keeps `htmlLabels` and its F0
  footer) for the 8 `look`+`htmlLabels` rows. **The corpus-twin rule takes
  precedence over the synthetic config-free rule**, including for those 8 rows
  whose corpus twin is not literally config-free; their expected frame is the
  F0 fixture's frame with the source's extra footer lines appended at the end
  of the fixture's footer block (`look`/`theme` follow `htmlLabels` in the
  fixed order). The other 90 rows (84 P2, 6 P3) use the synthesized in-memory
  twin. Both kinds are enumerated per row in the ledger as
  `twin.kind = "corpus" | "synthetic"`.
- Ledger `tools/mermaid/requirement-0037-cohort.json` (schema
  `loom_mermaid_requirement_0037_cohort/v1`): one row per promoted fixture with
  `phase`, the `twin` descriptor (`{"kind": "corpus", "path": …}` or
  `{"kind": "synthetic"}`), the exact expected footer/title lines, and the
  measured `expected_states` (wide/narrow/restored). Per-phase `review`
  records; a phase counts as approved only when every enabled phase has an
  approving record — the RFC 0035 per-phase rule.
- Generator `tools/mermaid/generate_requirement_0037_cohort.py` (renderer-free,
  `--check` reproducible). It carries, not merely mentions, the 0035
  invariants:
  - `ENABLED_PHASES` drives emission: only enabled phases produce rows, so
    landing P1 does not pre-book P2/P3 rows; the counts for all three phases
    stay pinned regardless;
  - a fixed classification **priority** `title→P3`, else `theme→P2`, else
    `look→P1`, so a later phase cannot reclassify an earlier phase's rows (the
    6 title rows also carry `theme`, so a theme-first rule would silently move
    them into P2);
  - a **per-phase path-set digest** (sorted path + source sha256 per row, one
    digest per phase) pinned in the generator and asserted by `--check`, so an
    earlier phase's row set cannot drift while totals stay equal;
  - review records are **authored in the generator** and written to the ledger,
    never read back from it, so a hand-edited ledger fails `--check`;
  - the population is the **whole 172-original requirement family, classified
    by source shape from the filesystem** — not the derived fallback roster,
    which shrinks as phases promote rows — so the pins stay stable across
    phases: 172 = 23 P1 + 91 P2 + 6 P3 + 24 `layout: elk` + 2 namespaced +
    2 malformed + 24 already drawn (no admitted key). Those denominators and
    the per-phase counts 23/91/6 are hard-asserted. The generator does not
    re-derive the frozen D-204/D-395 partitions in `requirement-cohort.json`
    or `requirement-f0-cohort.json`.
- Gate `tests/ui/test_mermaid_requirement_0037_cohort.cpp` (registered in
  `tests/ui/CMakeLists.txt`): per row, the anti-false-green set —
  1. `ASSERT_TRUE(draws(twin, kWide))`: a twin that also falls back must not
     let "both fall back equal" pass (the F0 pattern);
  2. a **native predicate** that also rejects a frame still carrying the raw
     `requirementDiagram`/`config:`/`description:` source signature, not just a
     `mermaid`-vs-`text` surface difference;
  3. `draws(source, width) == expected_states[i]` for each of
     wide/narrow/restored, so a ledger state the render does not produce fails;
  4. determinism (the same source rendered twice is byte-identical) and twin
     equality with the exact footer/title composition;
  5. the fail-closed boundary set (every non-goal key and value, duplicates,
     namespaced subtree, folded title, empty config, malformed envelope);
  6. frozen controls: `mandatory_gaps == 48`, `integrity_errors` empty,
     `drawn_at_240` delta equals the phase row count,
     `original_files_executed == 2145`, and
     `Requirement11_native_whole_source.mandatory_remaining == 160`.
  The gate runs against a full build of both presets; a target-only build's
  matrix verdict is never accepted (`docs/dev/engineering-efficiency.md` rules
  3, 5, 27, 28).
- Matrix branch in `tests/ui/test_mermaid_official.cpp` mapping each ledger row
  to a new reason string. It reads the **per-row** `expected_states` the way the
  Journey cohort loader does (`:5637-5643`), deriving `expected`/`status` from
  the measured wide/narrow/restored states — **not** copying the xychart
  branch's hardcoded `{wide, "source_fallback", wide}` triple (`:10482-10485`),
  which would leave narrow unenforced and bypass OQ-3. The branch reads the
  census **from the ledger**; `manifest.json` and the `kManifestSha256` pin are
  never edited.
- Roster migrated with `tools/mermaid/fallback_attribution.py --emit` in each
  phase commit.
- Existing negative tests that must flip, enumerated exactly (there are two,
  in two different phases — the first draft of this RFC claimed one):
  - **P1:** `F0InertBoundaryKeepsVisualKeysClosed`
    (`tests/ui/test_mermaid_requirement_cohort.cpp:431`) asserts
    `look: handDrawn` stays fallback; it draws after P1. The probe keeps
    `theme: dark`, `layout: elk`, namespaced keys and the invalid forms
    rejected, so no other assertion in it changes.
  - **P3:** `FrontmatterAndThemeGatesStayClosed` (`:330-341`) loops four frozen
    rows asserting both widths fall back. One of them,
    `a-diagram-with-a-title-neo-look-redux-dark-theme.mmd`, is a P3 cohort row.
    P3 **splits it out of the loop** and asserts it drawn-wide /
    source-fallback-narrow (the disclosure footer exceeds the narrow budget,
    the D-066 F0 `narrow_note` contract); the loop keeps the elk row and the
    two malformed rows as full negatives.
  Three comments become false and each phase's commit corrects the one it
  falsifies: the test file header (`:8-9`), the
  `F0InertBoundaryKeepsVisualKeysClosed` preamble (`:429`, "Visual/semantic keys
  and unsupported values stay whole-source" — `look` is admitted at P1), and
  the `FrontmatterAndThemeGatesStayClosed` preamble (`:329-330`).

### 4. Named cross-module couplings

These are string/shape couplings that do not fail to compile if changed on one
side only:

| coupling | files that must change together |
|---|---|
| envelope event model — `parse_envelope` already parses and stores `title` (`mermaid_envelope_impl.cpp:360-368`), so the accept/decode branch is added in the **requirement validator** (`mermaid_requirement_impl.cpp:264-293`), not in the generic parser | `mermaid_requirement_impl.cpp`, `mermaid_detail.cppm` |
| `RequirementEnvelope` / `RequirementDiagram` field layout | `mermaid_detail.cppm`, `mermaid_requirement_impl.cpp` |
| shared footer publication and title paint | `mermaid_relations_core_impl.cpp` (`RelationFamily::Requirement` notice branch, title block) |
| census `expected`/`reason` per fixture | **`tests/fixtures/mermaid_official/manifest.json` holds the source values in `fixtures[].raw`** and the whole file is SHA-pinned by `kManifestSha256`, a pin duplicated across the protected authority headers (`tests/support/mermaid_{block58,class285,class_theme4,er107,er206,er_groupedge,flow400,er198,flow_extra7}_*.hpp`). The promotion therefore must **not** touch the manifest: `tests/ui/test_mermaid_official.cpp` gains a cohort branch (ledger-driven `expected`/`reason` override) following the **Journey / requirement-F0 per-row `expected_states` loader** (`:5637-5643`), **not** the xychart hardcoded triple |
| census reason string | `tests/ui/test_mermaid_official.cpp`, ledger generator, closure doc |
| roster totals | `tools/mermaid/fallback_roster.json`; `tools/mermaid/e6c-mandatory-gap-roster.json` must stay byte-identical (zero requirement rows among the 48 `mandatory_gaps`) |
| frozen D-204 / D-395 partitions | `tools/mermaid/generate_requirement_cohort.py`, `requirement-cohort.json`, `requirement-f0-cohort.json` (read-only for this RFC) |

Dependency direction is unchanged: the requirement renderer keeps depending on
the shared relations core; no new edges are introduced.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `look` admission | 23 rows (`look` only 15, `look`+`htmlLabels` 8) | **done** | **Measured 2026-11-21**: family 24→**47** drawn / 148→**125** fallback; global 1271→**1294** / 874→**851**; formal `drawn_at_240` 1085→**1108**; gaps 48, integrity 0, `Requirement11.mandatory_remaining` 160 unchanged; roster `--emit` 874→851; debug and release matrix byte-identical at sha `36384e6c`; cohort gate 4/4, F0 suite 10/10, official matrix green |
| P2 | `theme` admission | 91 rows (90 `look`+`theme`, 1 `classic`+`default`) | **done** | **Measured 2026-11-21**: family 47→**138** / 125→**34**; global 1294→**1385** / 851→**760**; formal 1108→**1199**; gaps 48, integrity 0, `Requirement11.mandatory_remaining` 160 unchanged; roster `--emit` 851→760; 7 corpus twins + 84 synthetic; debug and release matrix byte-identical at sha `4a5fbe87`; cohort gate 4/4, F0 suite 10/10, official matrix green |
| P3 | `title` projection | 6 rows | proposed | family 138→144 / 34→28; global 1385→1391 / 760→754; formal 1199→1205; title twin gate green; same gate set |

Closure: full debug ctest once, dual-preset byte gate, RFC `implemented`,
closure doc, design-decisions entry, efficiency retro.

## Production Readiness Review

Completed at the `implementable` gate. Current answers:

- **Correctness & tests** — new per-row twin gate plus a boundary suite; the
  gate is the inertness proof, not a golden snapshot. Known flakes
  (`ServerMain.DirectConnect*`, `E2E_Pty_Handoff*`) are pre-existing and unrelated.
- **Build system** — no new module, no new target, no new import, no interface
  change beyond one struct field (`RequirementEnvelope::title`) plus one field
  copy in an already-shipped, non-exported `.cppm` partition. **Fan-out
  measured**: touching `src/ui/visual/mermaid_detail.cppm` in the debug tree
  re-schedules **509** C++ compile jobs (`ninja -C build/debug -n`) — the whole
  `loom.ui.visual.mermaid` import closure, identical for any edit to that
  partition. That one-time closure rebuild is the dominant cost of this RFC and
  recurs once per phase commit (three times total); it is inherent to the
  atomic-phase rule, not introduced by the change. **BMI PSS: N/A with reason**
  — the edit adds one field to a non-exported partition's struct and no new
  BMI-included code: no new inline body, no new template instantiation and no
  new exported declaration, so the only BMI delta is the field itself. No
  concurrency caps; no new textual third-party includes.
- **Rollback** — each phase is one atomic commit (validator table + ledger +
  generator + gate + matrix branch + roster); reverting one phase restores the
  previous census exactly. Rollback is LIFO: every P2 row carries `look` and
  every P3 row carries `look` + `theme`, so P1 is a hard prerequisite of P2 and
  P1+P2 of P3.
- **Observability** — disclosure footer is the user-visible trace; session
  traces and prompt dumps are untouched.
- **Documentation** — design-decisions entry, closure doc, README index,
  efficiency retro.
- **Deletion** — the three stale comments are corrected in place, each in the
  phase that falsifies it: the test file header (`:8-9`), the
  `F0InertBoundaryKeepsVisualKeysClosed` preamble (`:429`, "Visual/semantic
  keys and unsupported values stay whole-source" — `look` is admitted at P1)
  and the `FrontmatterAndThemeGatesStayClosed` preamble (`:329-330`).
- **Deprecation** — none.
- **Wire / persisted shapes** — N/A: rendering-only change. No wire protocol,
  session JSONL, prompt dump, settings or ledger-consumed persisted format is
  touched; the only new persisted file is this RFC's own cohort ledger.
- **Golden suites** — N/A: no `test_ui_*`, `test_dialog_*`,
  `test_prompt_dialog` or truecolor golden changes. The affected surface is the
  mermaid official-matrix census plus the requirement cohort tests; both are
  assertion-based, not snapshot-based.
- **Platform readiness** — N/A-with-reason on every row: rendering-only, so no
  termios/signal/PTY, network, filesystem-permission or offline-cache change;
  Linux debug+release `-Werror` is the only required matrix (macOS is not a
  gate per project policy, and CI is not waited on); the truecolor path is
  untouched apart from a token-only footer line; no new dependency or fetch.
- **CLAUDE.md** — no change: no convention, configuration path or build-layout
  rule changes.
- **Architecture-graph lint** — N/A: no new module, no new import, no new
  target edge (§4 "dependency direction is unchanged").

## Rollout and rollback

Phases land in order P1 → P2 → P3, each leaving the census self-consistent.
Rollback is LIFO, not independent: P1 is a prerequisite of P2 (every P2 row
carries `look`) and P1+P2 of P3 (every P3 row carries `look` + `theme`), so a
P1 revert first requires reverting P3 and P2. No persisted format, wire shape or debug
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
- **Validate-and-ignore `title`.** Rejected: the 6 title rows also carry
  `look: neo` + `theme`, so after P1/P2 they would draw with the authored title
  silently dropped — precisely the unfaithfulness RFC 0036's withdrawal
  punished. This option is not on the table; the real choice is project the
  title or keep rejecting it (OQ-1).
- **Keep `title` rejected** (the do-nothing position for P3). Costs 6 rows but
  is the conservative reading of "never book unpainted content as drawn" — it
  is OQ-1's alternative, not a strawman.
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
- Edited `tests/ui/test_mermaid_requirement_cohort.cpp` (two assertion flips,
  one per phase — P1 `F0InertBoundaryKeepsVisualKeysClosed:431`, P3
  `FrontmatterAndThemeGatesStayClosed:330-341` wide — plus cohort negatives and
  the three stale comments at `:8-9`, `:429` and `:329-330`) and
  `tests/ui/test_mermaid_official.cpp` (matrix branch + reason string).
- Targeted `ctest -L mermaid` per phase; full debug ctest once at closure
  (current baseline 4614 tests; expected closure total 4614 + the new file's
  TEST count, planned 4, with no deletions — the two in-place flips change
  assertions, not test counts). The generator's `--check` is part of every
  phase's verification, not only closure. Per phase the matrix byte-gate runs on the
  **release** preset (debug is ~9x slower; `docs/dev/engineering-efficiency.md`
  rule 2), and the **dual-preset** byte-identical gate runs once at closure;
  record the sha at each phase.
- Known timing flakes are pre-existing (`ServerMain.DirectConnect*` 3-second
  TCP timeouts under load; `E2E_Pty_Handoff` deadline); re-run in isolation.

## Documentation impact

- [ ] `docs/dev/d397-flowchart-family-selection.md` — §7 (added 2026-11-21)
      already holds the corrected 120/23/91/6 measurement, the namespaced-row
      exclusions and the title-projection correction. §6 stays as the historical
      pre-probe estimate and is cross-referenced by §7 rather than rewritten
      (append-only measurement discipline); the closure doc cites §7.
- [ ] `docs/dev/d398-requirement-rfc0037-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — requirement config admission and
      title projection entry
- [ ] `docs/README.md` — RFC index
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: project `title:` (recommended, three small production edits reusing the shared seam) or keep `title:` rejected (6 rows stay fallback; the validate-and-ignore middle option is ruled out by RFC 0036's lesson)? | design review | design review verdict; if rejected, P3 drops to 0 rows |
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
| 2026-11-21 | r1 | `agent:design-review-r1` (independent general-purpose, own render probe + static re-derivation) | REQUEST-CHANGES | MAJOR-1 the F0 boundary flip count was wrong: a **second** negative, `FrontmatterAndThemeGatesStayClosed` (P3 row `a-diagram-with-a-title-neo-look-redux-dark-theme`), flips at wide in P3 → **accepted**: §3 now enumerates both flips, their phases, the narrow-stays-fallback state and the two stale comments. MAJOR-2 the census coupling omitted `manifest.json` / `kManifestSha256` → **accepted**: coupling row added, manifest immutability made a Non-Goal, ledger-driven cohort branch named. MINOR-3 cohort `look: handDrawn` is 23, not 24 (the roster's 24th is the excluded namespaced row) → **accepted**: RFC and D-397 §7 corrected. MINOR-4 the title grammar was described as `parse_envelope` generically and accepted titles wider than the 240-column paint guard → **accepted**: §2 now pins the ER rule (`er_yaml_value`, printable ASCII, ink, ≤ `kWidthLimit`) so accepted implies paintable, and inventories the three existing title grammars. MINOR-5 "zero requirement rows" overclaimed against the `requirement11_*` gate records → **accepted**: evidence row reworded. MINOR-6 OQ-1 conflated ignore with reject → **accepted**: restated, and the Alternatives entry rewritten. NIT-7 "two field copies" understated the change → **accepted**: §2 enumerates the three production edits. OQ-1 → project the title; OQ-2 → corpus-bounded table; OQ-3 stays implementation-time. No owner ruling required. Core measurement (120/24/2/2), per-phase split 23/91/6, inertness of `look`/`theme`, the title-paint mechanics and the phase arithmetic were independently re-verified. |
| 2026-11-21 | r2 | same design-review agent, independent re-read of the revised file | APPROVE-TO-ACCEPT | MAJOR-1 → verified fixed (both flips in §3). MAJOR-2 → verified fixed (manifest Non-Goal + coupling row + ledger branch). MINOR-3/5/6, NIT-7 → verified fixed. Fresh pass: MINOR-A PRR "two struct fields" vs one field → **accepted**: PRR reworded; MINOR-B Testing plan still said one flip → **accepted**: both flips named with line anchors; MINOR-C "Form::Plain only" vs ER parity → **accepted**: Plain and quoted both admitted, folded/block rejected, corpus titles plain so no row impact; MINOR-D rollback overclaimed independence → **accepted**: LIFO, P1 prerequisite of P2, P1+P2 of P3; NIT-E reviewers frontmatter → **accepted**: identity recorded; NIT-F PRR Deletion singular → **accepted**: both stale comments named. Arithmetic, partition, inertness, title-paint mechanics and the ER-only titled-empty guard re-confirmed. No BLOCKER/MAJOR. Recommended promotion to accepted. |
| 2026-11-21 | prr-r1 | `agent:prr-review-r1` (independent general-purpose, production-readiness lens; static re-derivation, no build) | REQUEST-CHANGES | MAJOR-1 the twin transform was under-defined for 99 rows (the 8 `look`+`htmlLabels` and all 91 P2 rows) → **accepted**: §3 now defines the config-free twin as stripping *every* admitted leaf, adds the corpus-twin rule for the 30 byte-equivalent rows, and drops the misleading `config-free`/`title-free` enum. MAJOR-2 the matrix branch cited the xychart cohort pattern, which hardcodes narrow as `source_fallback` → **accepted**: the branch must read per-row `expected_states` like the Journey loader (`:5637-5643`), explicitly not the hardcoded triple. MAJOR-3 the generator lacked an enabled-phase set, classification priority, per-phase digests and review ownership → **accepted**: all four pinned, with the 6 title rows' theme overlap named as the drift case. MAJOR-4 the PRR build-system row asserted fan-out without measurement and the ctest delta was unstated → **accepted**: fan-out measured at 509 compile jobs (`ninja -C build/debug -n`), BMI PSS marked N/A with reason, CMake registration and the expected closure total named. MINOR-1 the P3 loop restructure → **accepted**: split-out assertion specified. MINOR-2 anti-false-green set → **accepted**: six numbered assertions incl. `ASSERT_TRUE(draws(twin))`, a native predicate, per-width ledger equality and five frozen controls. MINOR-3 per-phase formal deltas → **accepted**: 1085→1108→1199→1205. MINOR-4 a third stale comment (`:429`) → **accepted**: all three named. MINOR-5 "accepted implies paintable" overstated → **accepted**: width-paintable, height fails closed. MINOR-6 coupling file list → **accepted**: the accept branch is in the requirement validator. NIT-1 D-397 §6 vs §7 → **accepted**: §6 kept as the historical estimate, §7 cross-referenced. NIT-2 dual-preset per phase → **accepted**: release per phase, dual-preset at closure (efficiency rule 2). Census, phase arithmetic, the 120/24/2/2 partition, D204/F0 disjointness, footer ordering and requirement11 non-interaction verified correct. |
| 2026-11-21 | prr-r2 | same PRR agent, independent re-read of the revised file | REQUEST-CHANGES | All four r1 MAJORs verified fixed. MAJOR-A the coupling table still prescribed the xychart hardcoded branch, contradicting §3 → **accepted**: row reworded to the Journey/F0 per-row `expected_states` loader. MAJOR-B "expected = twin frame + footer" double-counted the 6 title rows → **accepted**: the rule is now split (config-free/corpus rows append the footer; the 6 title rows are `[title, blank] + title-free twin frame`, twin footer retained, nothing appended). MINOR-1 the PRR section omitted the platform/golden/wire rows → **accepted**: three N/A-with-reason bullets added (rendering-only). MINOR-2 the review record's MINOR-4 was marked accepted while the PRR Deletion and Testing-plan bullets still said "two" → **accepted**: both now say three and name `:429`. MINOR-3 "byte-equivalent" overstated → **accepted**: body-byte-equivalent / frame-equivalent stated, and the gate compares frames, never source bytes. NIT-1 "three keys" → four. NIT-2 P1 phase wording now says release run with the sha recorded and the dual-preset gate at closure. NIT-3 the corpus-twin rule's precedence over the synthetic config-free rule is stated for the 8 htmlLabels rows. Arithmetic, 30/90 twin split, frozen controls and D204/F0 disjointness re-verified. |
| 2026-11-21 | prr-r3 | same PRR agent, independent re-read | APPROVE-TO-IMPLEMENT | All r2 findings verified fixed; fresh pass found only three wording NITs, all folded in: NIT-1 the Summary twin-gate parenthetical now covers the 6 title rows; NIT-2 the PRR adds the CLAUDE.md and architecture-graph-lint N/A rows; NIT-3 the general footer rule now says "the footer lines the twin does not already carry", so the 8 F0 corpus twins append only the new `look` line. Arithmetic, the 114/6 and 30/90 splits, the frozen controls and the PRR answer set were re-verified. Gate to `implementable` passed. |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-21 | — | Draft created (provisional), no code | — |
| 2026-11-21 | P2 | `theme` admission: closed 7-palette table with its disclosure line after `look`; corpus-twin resolution by body equivalence over the pinned drawn set with a footer-prefix rule; the synthetic config-free twin added to the gate; 91 rows promoted (7 corpus + 84 synthetic), family 138/34, global 1385/760, formal 1199, dual-preset sha `4a5fbe87`. Reviewed by `agent:aa19624ebdb712b6a` (APPROVE, three NITs applied) | — |
| 2026-11-21 | P1 | `look` admission: closed `classic`/`neo`/`handDrawn` table in the requirement envelope with its disclosure line; ledger + generator (filesystem population, per-phase digests, per-phase review records) + 23-row corpus-twin gate + official-matrix cohort branch; F0 boundary flip; 23 rows promoted, family 47/125, global 1294/851, formal 1108, dual-preset sha `36384e6c`. Reviewed by `agent:ab5e96a11461da018` (r1 REQUEST-CHANGES → final APPROVE) | — |
