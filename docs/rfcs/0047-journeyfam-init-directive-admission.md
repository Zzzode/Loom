---
rfc: 47
title: journey-family in-body init directive admission
status: provisional
owners: "@Zzzode"
reviewers: []
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0047 — journey-family in-body `%%{init}%%` inert directive admission

## Summary

D-419 selects the next slice from the remaining capability debt: 9 rows in the
journey family (journey 4, timeline 5) whose only unadmitted input is the leading body
directive `%%{init: {...}}%%` carrying exactly the inert keys `logLevel` and `theme`. The
shared journey-family frontmatter gate (RFC 0040, three byte-identical TU-local copies) today
rejects any body `%%{` line that mentions `theme`; this RFC replaces that substring rule with
a closed structural directive admission (bare `init` is a no-op; a keyed payload must be a
flat object over exactly `logLevel`/`theme` with closed per-family values), discloses the
admitted keys through the existing family footer, and leaves every other directive gated.
The renderers never read either key and already skip `%%` lines.

Target: journey **14/4 → 18/0**, timeline **23/7 → 28/2**; global **1528/617 → 1537/608**;
formal `drawn_at_240` **1342 → 1351**; roster 617 → 608.

## Motivation

### Selection (D-419, measured 2026-11-22)

Every one of the 617 fallback rows was scanned for leading body directives. 9 rows are
journey-family rows whose only directive is a single-line `%%{init…}%%` carrying a subset of
`logLevel`/`theme`; a probe through the public `render_markdown` seam (frontmatter and all
leading directive lines removed) draws **9/9** at 240 columns
(`/tmp/loom-logs/probe-0047-selection.log`; probe source deleted before commit). The other
directive-bearing families are out of scope: state v1/v2 8 rows carry concurrency/composite
bodies that do not draw after the strip (body capability, not config), flowchart 34 rows are
the exact-bit reserved surface, git 9 rows are multi-line `gitGraph`/`themeVariables`
payloads, and quadrant 5 rows change axis geometry.

### Evidence

| family | rows | admitted directive | source |
|---|---|---|---|
| journey | 4 | `{ 'logLevel': 0, 'theme': '<dark|default|forest|neutral>' }` | `journey/pr-check/*-theme.mmd` |
| timeline | 5 | `{ 'logLevel': 'debug', 'theme': '<base|dark|default|forest|neutral>' }` | `timeline/{nightly,pr}-check/…` |

Excluded on purpose, same shape:
`timeline/pr-check/4-…disablemulticolor-true.mmd` (content key `timeline.disableMulticolor`)
and `timeline/pr-check/5-…directive-overridden-colors.mmd` (multi-line `themeVariables`
override) stay whole-source.

The directive is inert for these renderers: `journey_layout`/`timeline_layout` skip every
`%%` line, and neither renderer reads `theme` or `logLevel`. The admitted frame therefore
equals the directive-free twin's frame plus the disclosure footer. The repository already
owns this exact grammar and disclosure vocabulary: `flow_init_directive` in
`mermaid_impl.cpp` admits one-key `{logLevel: <0..5|trace|debug|info|warn|error|fatal>}` and
`{flowchart:{htmlLabels:bool}}`, and `class_init_meta` in `mermaid_class_impl.cpp` parses a
single leading `%%{init:…}%%` (quote-normalized, strict JSON). There is **no clean corpus
twin** body-equivalent to an admitted row, so every row uses the synthetic directive-free
twin.

## Goals

- **G1.** Admit a *leading* directive block (after the frontmatter, before the first
  non-blank, non-comment line): any number of single-line directives, each either the bare
  `%%{init}%%` no-op or `%%{init: {<flat object>}}%%` whose key set is a subset of
  `logLevel`/`theme`, each key at most once, with closed values:
  `logLevel` ∈ {0…5, trace, debug, info, warn, error, fatal} (the repository-wide grammar),
  `theme` ∈ the family's closed directive table (journey dark/default/forest/neutral;
  timeline adds base; gantt's table is empty).
- **G2.** Keep every other directive gated: a non-leading `%%{` line that mentions `theme`
  (unchanged rule), a leading directive with a nested object, an unknown key, an
  out-of-table value, a duplicate key, an unterminated or multi-line directive, or trailing
  text after `}%%`.
- **G3.** Disclose each admitted key with one footer line, after the frontmatter lines, in
  canonical order — `config.logLevel=<v> -> logger only; not applied`, then
  `config.theme=<v> -> colour palette; not applied` — the exact wording
  `flow_init_directive` and the RFC 0040 footer already use.
- **G4.** Keep the three TU-local copies aligned (identical rule text, per-family tables) and
  the Python mirror (`tools/mermaid/journeyfam_tables.py`) faithful, so the four existing
  generators classify these rows truthfully (new kind `init_admitted`).
- **G5.** A frontmatter visual leaf and a keyed body directive together stay gated (mutually
  exclusive channels), so the existing RFC 0040 negative "admitted leaf + in-body init"
  keeps its verdict and no duplicate `config.theme=` footer can be emitted.
- **G6.** No undetected regression: the RFC 0040/0031 ledgers, generators and tests are
  migrated in the same phase; every flipped negative is paired with a new positive on the
  same path and a new negative on the neighbouring shape; no assertion is deleted or relaxed.

## Non-Goals

- The state 8, flowchart 34, git 9 and quadrant 5 directive rows named above.
- Multi-line directives, nested maps (`themeVariables`, `timeline`, `gitGraph`,
  `flowchart`), and content keys of any kind.
- Any change to the frontmatter look/theme tables, to `parse_envelope`, or to the other
  families' directive handling (`flow_init_directive`, `class_init_meta` untouched).

## Proposal

### 1. Mechanism (three TU-local copies, one rule)

`journey_fam_visual_config` (copied byte-identically in `mermaid_journey_impl.cpp`,
`mermaid_timeline_impl.cpp`, `mermaid_gantt_impl.cpp`) gains a directive block scan replacing
the current "any body line starting `%%{` that contains `theme` gates" loop:

1. With the frontmatter split already computed, walk the body's leading lines (blank and
   `%%`-comment lines are skipped, exactly like the renderers).
2. A line whose trimmed text starts with `%%{` must be a complete single-line directive: it
   ends with `}%%`, the inside is exactly `init` (no-op) or `init:` followed by a flat
   `{…}` object with no nested `{`/`[`, no unterminated quote, and nothing after the closer.
3. Split the object on commas; each entry is `key: value`, where both sides go through the
   existing `journey_fam_scalar` (quotes, trailing comments). Unknown keys, duplicate keys,
   values outside the closed tables, and malformed entries gate the whole source.
4. The first line that is neither blank nor a directive is the diagram header; from that
   line onward the pre-existing rule is kept verbatim: a `%%{` line that mentions `theme`
   gates.

New per-family data: `kFamilyInitThemeTable` (journey 4, timeline 5, gantt 0 entries) plus
the shared logLevel grammar; gantt's empty table means every keyed directive gates there
(verdicts unchanged — no gantt original carries a directive). The three copies stay
byte-identical apart from the existing per-family table constants.

### 2. Footer

The family header is emitted when the frontmatter leaves or the directive keys are non-empty.
Order: header, `config.look`, `config.theme` (frontmatter), `config.logLevel`, `config.theme`
(directive). Because G5 gates the composition, at most one `config.theme=` line can appear.
All lines fit the existing 96-byte cap; the emission and height guards are unchanged.

### 3. Reviewed cohort, twin gate and wiring

- Ledger `tools/mermaid/journeyfam-init-0047-cohort.json`: 9 rows with their directive
  keys/values, literal footer lines, measured per-width states, and the synthetic twin rule
  "delete the frontmatter and every leading `%%{init…}%%` line"; independent review record.
- `generate_journeyfam_init_0047_cohort.py`: renderer-free, `--check`-reproducible; drives
  `journeyfam_tables.classify` (extended with the `init_admitted` kind and a directive
  parser mirroring step 2–3) and pins the cohort path+sha digest.
- Gate `tests/ui/test_mermaid_journeyfam_init_0047_cohort.cpp` (5 tests): ledger schema and
  digest pins; per-row twin inertness and literal footer composition; a battery per family
  (bare `init`; theme and logLevel alone and together; single/double quotes; every admitted
  value; unknown key; nested `flowchart`/`timeline`/`themeVariables`; duplicate key;
  out-of-table theme; out-of-grammar logLevel; multi-line; unterminated; trailing text; a
  non-leading directive; the frontmatter+directive composition); the existing family
  batteries still hold; a literal partition.
- Matrix branch `RFC-0047 journey-family init-directive reviewed cohort` placed **before all
  family branches** (rule 38), reading per-row states and requiring `review.verdict ==
  "approved"`.
- **Existing ledgers/generators/tests migrated in the same phase** (flips paired):
  `generate_timeline_cohort.py` + `timeline-cohort.json` (partition 7/16/7 → drawn 7,
  admitted 16, `init_admitted` 5, `themed_excluded` 2), `generate_journey_cohort.py` +
  `journey-cohort.json` (10/4/4 → 10/4/4/0), `generate_journeyfam_0040_cohort.py` +
  `journeyfam-0040-cohort.json` (exclusions sentence + `selection.census_note`; admitted
  sets and digests unchanged), and the three tests that loop the shrunk buckets:
  `test_mermaid_timeline_cohort.cpp` (loop keeps the 2 content-key rows, gains an admitted
  directive positive), `test_mermaid_journey_cohort.cpp` (loop now iterates the empty bucket,
  paired with new directive negatives in the same test), `test_mermaid_journeyfam_0040_cohort.cpp`
  (the 4-path `themed_excluded` loop becomes a 4-path positive over the 0047 ledger; the
  "admitted + body init" negative keeps its verdict under G5).
- Roster `--emit` 617 → 608; `family_residual:journey` 4 → 0 (category removed),
  `family_residual:timeline` 7 → 2; categories 41 → 40.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | journey-family init directive | 9 rows, 3 TUs, 1 family | proposed | red-first (all 9 in the roster at 617); journey 14/4 → 18/0; timeline 23/7 → 28/2; global 1528/617 → 1537/608; formal 1342 → 1351; gaps 48, integrity 0; dual-preset byte-identical sha recorded; full debug ctest with a durable log; independent P1 code review APPROVE |

## Production Readiness Review

To be completed at the `implementable` gate.

## Rollout and rollback

One atomic commit; revert restores 617 and every pin. No persisted data, wire, CLI or
settings change.

## Drawbacks and risks

- A closed directive scanner in three TU-local copies plus its Python mirror; the batteries
  and the twin gate pin closure per family, and the rule text stays aligned (RFC 0040's
  invariant). The mirror is exercised by the four existing generators' `--check` runs.
- A disclosed upstream behaviour the terminal cannot emulate (palette/logger directives not
  applied). The disclosure footer makes the substitution visible, as in every prior family.
- Leading directives that today pass through undisclosed (e.g. a logLevel-only directive) now
  gain a footer line. No corpus row of the three families is affected (measured: the only
  directives are the 9 admitted and the 2 excluded).

## Alternatives considered

1. Do nothing — leaves 9 twin-proven rows on fallback and keeps the journey family's last
   fallbacks.
2. Admit the state 8 init rows — rejected: their bodies do not draw after the strip
   (concurrency/composite capability), so the directive is not their blocker.
3. Reuse `flow_init_directive` by exporting it from `mermaid_impl.cpp` — rejected: it is a
   TU-local helper inside a different partition of the same module and its notice wording
   carries the flowchart's `flowchart.htmlLabels` branch; copying it would widen the blast
   radius into an unrelated renderer. The journey-family copies keep the RFC 0040 three-copy
   invariant.
4. Admit the multi-line `themeVariables` row — rejected: it overrides colours (content) and
   its payload is a nested map.

## Testing and verification plan

- New gate (5 gtests) + one hermetic `--check`: `ctest -L mermaid` delta **+6**; full debug
  ctest 4687 → 4693 executed, complete stdout and exit code captured to
  `/tmp/loom-logs/ctest-full-dbg-0047.log` (TL 2026-11-22 requirement).
- The migrated family gates are the drawn-row regression oracles; the 9 cohort rows assert
  frame = twin + footer line by line.
- Dual-preset matrix and roster `--emit`/`--self-test`/`--matrix` at closure.

## Documentation impact

- [ ] `docs/dev/d419-next-family-body-init.md` — selection (corrected to 9 rows, 3 numbers)
- [ ] `docs/dev/d420-body-init-rfc0047-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — directive admission entry, RFC 0040
      "in-body initializer stays a whole-source gate" sentences amended
- [ ] `docs/README.md` — RFC index row
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — journey/timeline rows
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: frontmatter visual leaf + keyed directive together? | design review | proposed: gated (G5), matching RFC 0046's ishikawa mutual exclusion and keeping the existing negative |
| OQ-2: is the bare `%%{init}%%` a no-op? | design review | proposed: yes — the renderers already skip it and the corpus has no bare body directive |
| OQ-3: logLevel value set | design review | proposed: the repository-wide grammar (0…5 / six names), not the corpus-only value, so the three families cannot drift from `flow_init_directive` |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR.
- Phase: one atomic phase with its review record.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | — | — | draft | Created provisional from D-419; selection corrected to 9 rows after re-measuring all 617 fallbacks |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | (this commit) |
