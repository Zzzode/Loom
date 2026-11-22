---
rfc: 47
title: journey-family in-body init directive admission
status: provisional
owners: "@Zzzode"
reviewers:
  - agent:design-review-evidence
  - agent:design-review-grammar
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0047 — journey-family in-body `%%{init}%%` inert directive admission

## Summary

D-419 selects the next slice from the remaining capability debt: 9 journey-family rows
(journey 4, timeline 5) whose only unadmitted input is a leading body directive
`%%{init: {...}}%%` carrying exactly the inert keys `logLevel` and `theme`. The shared
journey-family gate (RFC 0040, three byte-identical TU-local copies) today rejects any body
`%%{` line that mentions `theme`; this RFC replaces that substring rule — for the two enabled
families only — with a closed structural admission of the *leading* directive block, discloses
the admitted keys through the existing family footer, and leaves every other directive gated.
The renderers skip `%%` lines and never read either key.

(4 further flowchart rows carry the same inert logLevel-only directive, but they belong to the
exact-bit reserved surface and are excluded as reserved, not because their directive is
non-inert.)

Target: journey **14/4 → 18/0**, timeline **23/7 → 28/2**; global **1528/617 → 1537/608**;
formal `drawn_at_240` **1342 → 1351**; roster 617 → 608.

## Motivation

### Selection (D-419, measured 2026-11-22)

All 617 fallback rows were scanned for body directives: 67 rows carry a `%%{` body line
(flowchart 34, git 9, timeline 7, journey 4, quadrant 5, state v1 4, v2 4). Exactly 9 are
journey-family rows whose only directive is a single-line `%%{init…}%%` over
`logLevel`/`theme`; a selection probe through the public `render_markdown` seam (frontmatter
and all leading directive lines removed) drew them at 240 columns
(`/tmp/loom-logs/probe-0047-selection.log`, 1 test PASSED; the probe source was deleted before
commit, and the P1 cohort gate re-proves the stronger property frame = twin + footer per row).
The other directive-bearing families are out of scope for their own reasons: state v1/v2 8
rows do not draw after the strip (concurrency/composite body capability), git 9 rows are
multi-line `gitGraph`/`themeVariables` payloads, quadrant 5 rows change axis geometry, and the
flowchart 34 are the exact-bit reserved surface.

### Evidence

| family | rows | admitted directive | sources |
|---|---|---|---|
| journey | 4 | `{ 'logLevel': 0, 'theme': '<dark\|default\|forest\|neutral>' }` | `journey/pr-check/*-theme.mmd` |
| timeline | 5 | `{ 'logLevel': 'debug', 'theme': '<base\|dark\|default\|forest\|neutral>' }` | `timeline/nightly-check/{9,10}-…`, `timeline/pr-check/{6,7,8}-…` |

Excluded on purpose, same family: `timeline/pr-check/4-…disablemulticolor-true.mmd` (content
key `timeline.disableMulticolor`) and `timeline/pr-check/5-…directive-overridden-colors.mmd`
(multi-line `themeVariables` override) stay whole-source.

The directive is inert for these renderers: `journey_layout`/`timeline_layout` skip every
`%%` line and neither reads `theme` or `logLevel`. The admitted frame therefore equals the
directive-free twin's frame plus the disclosure footer. **No clean corpus twin is body-equal to
any of the 9 rows** (verified against every drawn original of both families), so every row uses
the synthetic directive-free twin.

The repository already owns this grammar and disclosure vocabulary, but not a shared
implementation to import: `flow_init_directive` (`mermaid_impl.cpp`) admits one-key
`{logLevel: <0..5|trace|debug|info|warn|error|fatal>}` and `{flowchart:{htmlLabels:bool}}` by
quote-normalizing into **strict JSON**; `class_init_meta` (`mermaid_class_impl.cpp`) parses a
single leading `%%{init:…}%%` with a quote state machine. This RFC shares their **value set and
footer wording**, not their parsers: the journey-family copies use the YAML-scalar grammar they
already own (`journey_fam_scalar`: quotes and trailing `#` comments).

## Goals

- **G1.** Admit the *leading* directive block of the two enabled families. Normative grammar
  (each clause is a gate when violated unless stated otherwise):
  1. The scan starts after the frontmatter split, walks body lines, and skips blank lines and
     `%%`-comment lines; the leading block ends at the first line that is neither.
  2. A leading line whose trimmed text starts with `%%{` is a directive candidate. A trailing
     `\r` is removed first (the renderers tolerate CRLF). Leading spaces/tabs are allowed
     (the renderers trim).
  3. The candidate must end with `}%%`; text after that closer may contain only spaces/tabs.
     The directive body is the text between the first `%%{` and that closer.
  4. Trimmed body `init` is a no-op (bare directive, no footer). Otherwise the trimmed body must
     begin with the literal `init:` (no space before the colon) followed by a payload.
  5. The payload, trimmed, must be a single `{…}` object. Its interior is validated by a quote
     state machine mirroring `normalize_directive_quotes`: an unterminated quote, a raw control
     byte, or a backslash inside a single-quoted string gates. The interior must contain no
     `{`, `}`, `[`, or `]` (a nested object gates).
  6. The interior is split on commas; each entry is split on its first colon; key and value are
     read with `journey_fam_scalar`. Keys must be `logLevel` or `theme`, at most once each.
  7. Closed values. `logLevel` ∈ {`0`,`1`,`2`,`3`,`4`,`5`,`trace`,`debug`,`info`,`warn`,
     `error`,`fatal`} (the `flow_init_directive` value set; integer spelling or quoted name).
     `theme` ∈ the family's closed directive table: journey `{dark, default, forest, neutral}`,
     timeline `{base, dark, default, forest, neutral}` (gantt's table is empty; see G4).
  8. `{}` (empty payload, no entries) is admitted as a no-op equal to bare `init`, with no
     footer. Multiple leading directives are admitted independently; a key repeated across two
     directives is a duplicate and gates.
- **G2.** Keep every other directive gated or pass-through, exactly as today: a **non-leading**
  `%%{` line that mentions `theme` gates; every other non-leading line (including directives
  without `theme`, non-`init` directive names, and directives not in the leading block) keeps
  the long-standing pass-through. A leading directive with an unknown key, an out-of-table
  value, a duplicate key, a nested object, a malformed/partial payload, trailing text, or a
  non-`init` directive name gates the whole source.
- **G3.** Disclose each admitted directive key with one footer line, after the frontmatter
  lines, in canonical order — `config.logLevel=<v> -> logger only; not applied`, then
  `config.theme=<v> -> colour palette; not applied` — the exact wording `flow_init_directive`
  and the RFC 0040 footer already use. The family header is emitted when the frontmatter leaves
  or the directive keys are non-empty.
- **G4.** Keep the three TU-local copies byte-identical apart from their table/switch constants,
  and keep the Python mirror faithful. Because the leading-directive contract is only defined
  for these two families, gantt sets an explicit `kFamilyInitDirectivesEnabled = false` and
  keeps the legacy substring rule verbatim: no gantt verdict or frame changes (measured: 0 of
  52 gantt originals carry any `%%{` line). The Python mirror carries the same switch, so the
  gantt generator can never see the new kind.
- **G5.** A frontmatter visual leaf and any keyed leading directive together gate (mutually
  exclusive channels), so the existing RFC 0040 negative "admitted leaf + in-body init" keeps
  its verdict and no duplicate `config.theme=` footer can be emitted. A frontmatter leaf plus a
  *bare* `init` stays admitted (today's verdict). Consequence, documented and tested: a
  frontmatter leaf plus a logLevel-only directive changes from today's undisclosed draw to a
  gate; no corpus row has this shape.
- **G6.** No undetected regression: the RFC 0040/0031 ledgers, generators and tests are migrated
  in the same phase; every flipped negative is paired with a positive on the same path and a new
  negative on the neighbouring shape; no assertion is deleted or relaxed.

### Deliberate cross-channel asymmetry

The directive `theme` table is **not** the frontmatter table, and the two channels can judge the
same value differently: journey's frontmatter table is `redux*` only, so `config: theme: dark`
gates while `%%{init:{theme:'dark'}}%%` is admitted; timeline's frontmatter table has
`neo`/`neo-dark`/`redux*` but not `base`, while its directive table has `base` but not those.
This is intentional: each table is a closed, corpus-bounded surface (the same policy as
`docs/decisions/design-decisions.md`, "closed tables are corpus-bounded"), the frontmatter
tables are an existing pinned contract this RFC must not change, and the directive corpus
exercises a different value set. Both directions are pinned by literal assertions in the new
gate.

## Non-Goals

- The state 8, git 9, quadrant 5 and flowchart 34 directive rows named above.
- Multi-line directives, nested maps (`themeVariables`, `timeline`, `gitGraph`,
  `flowchart`), and content keys of any kind.
- Any change to the frontmatter look/theme tables, to `parse_envelope`, to the gantt family's
  directive handling, or to `flow_init_directive`/`class_init_meta`.

## Proposal

### 1. Mechanism (three TU-local copies, one rule)

`journey_fam_visual_config` (copied byte-identically in `mermaid_journey_impl.cpp`,
`mermaid_timeline_impl.cpp`, `mermaid_gantt_impl.cpp`) replaces its current "any body line
starting `%%{` that contains `theme` gates" loop with, in order:

1. **Directive block scan** (G1.1–G1.8) over the leading lines, producing `log_level_value`,
   `directive_theme_value`, and their presence bits; any violation returns `std::nullopt`.
2. **G5 composition check**: if the frontmatter produced any visual leaf and any keyed leading
   directive was admitted, return `std::nullopt`.
3. **Legacy tail scan**: from the first non-leading line onward, keep the existing rule verbatim
   (a `%%{` line containing `theme` returns `std::nullopt`).
4. **Footer**: header, then `config.look`, `config.theme` (frontmatter), `config.logLevel`,
   `config.theme` (directive); at most one `config.theme=` can appear because of step 2.

New per-family constants: `kFamilyInitDirectivesEnabled` (journey/timeline true, gantt false;
false keeps step 1 and 2 out and runs the legacy loop over the whole body),
`kFamilyInitThemeTable` (journey 4, timeline 5, gantt 0 entries), and the shared logLevel set.
The code of the three copies remains byte-identical; only these constants differ.

### 2. Reviewed cohort, twin gate and wiring

- Ledger `tools/mermaid/journeyfam-init-0047-cohort.json`: 9 rows, each with `path`, `sha256`,
  `manifest_sha256`, `prior_status` (+ its manifest `raw` expectation, which is `source` for
  the red-first pin), directive keys/values, the literal `expected_footer_lines`, measured
  per-width states, and the synthetic twin rule "delete the frontmatter and every leading
  `%%{init…}%%` line"; plus `selection` (rule, roots, cohort digest, `census_note`) and an
  independent `review` block.
- `generate_journeyfam_init_0047_cohort.py`: renderer-free, `--check`-reproducible; drives
  `journeyfam_tables.classify` (extended with the `init_admitted` kind and a directive parser
  mirroring G1 steps 2–4, i.e. the same switch, tables, grammar and the legacy tail rule) and
  pins the cohort path+sha digest and per-family counts.
- Gate `tests/ui/test_mermaid_journeyfam_init_0047_cohort.cpp` (5 tests): ledger schema,
  `prior_status`, digest and count pins; per-row twin inertness and literal footer composition;
  a battery per family (bare `init`; `{}`; theme and logLevel alone and together; single and
  double quotes; every admitted value; indent 2; CRLF; unknown key; nested
  `flowchart`/`timeline`/`themeVariables`; duplicate key within and across directives;
  out-of-table theme; out-of-grammar logLevel; multi-line; unterminated quote; trailing text
  after `}%%`; `init :` with a space; a non-`init` leading directive name; a non-leading
  no-theme directive that stays pass-through; a non-leading theme directive that gates;
  frontmatter-less source; frontmatter leaf + keyed directive = gate; frontmatter leaf + bare
  `init` = admit; and the cross-channel asymmetry literals from G1); the existing family
  batteries still hold; a literal partition.
- Matrix branch `RFC-0047 journey-family init-directive reviewed cohort` placed **before all
  family branches** (rule 38), reading per-row states and requiring `review.verdict ==
  "approved"`. It is provably reached: the matrix test itself asserts, for each of the 9 ledger
  paths, an original-track observation whose `reason` equals the branch reason.
- `tests/ui/CMakeLists.txt`: add the `add_executable`/`target_link_libraries`/
  `gtest_discover_tests` block for the gate and the hermetic `add_test` running
  `generate_journeyfam_init_0047_cohort.py --check` (the +6 ctest delta).
- **Existing ledgers/generators/tests migrated in the same phase** (flips paired):
  `generate_timeline_cohort.py` + `timeline-cohort.json` (partition 7/16/7 →
  drawn 7, admitted 16, `init_admitted` 5, `themed_excluded` 2),
  `generate_journey_cohort.py` + `journey-cohort.json` (10/4/4 → 10/4/4/0),
  `generate_journeyfam_0040_cohort.py` + `journeyfam-0040-cohort.json` (exclusions sentence +
  `selection.census_note`; admitted sets and digests unchanged), and the three tests that
  consume the shrunk buckets: `test_mermaid_timeline_cohort.cpp` (the loop keeps the 2
  content-key rows and gains an admitted-directive positive), `test_mermaid_journey_cohort.cpp`
  (the now-empty loop is paired with new directive negatives in the same test, and the existing
  logLevel-only `with_log_comment` source gains a footer assertion),
  `test_mermaid_journeyfam_0040_cohort.cpp` (the 4-path `themed_excluded` loop becomes a 4-path
  positive over the 0047 ledger; the "admitted + body init" negative keeps its verdict under
  G5). `generate_gantt_cohort.py` and `gantt-cohort.json` are **unchanged**: the mirror's gantt
  switch returns the legacy kinds, so `init_admitted` is unreachable there; the RFC records this
  measured rationale so the absent migration is intentional.
- Roster `--emit` 617 → 608; `family_residual:journey` 4 → 0 (category removed),
  `family_residual:timeline` 7 → 2; categories 41 → 40.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | journey-family init directive | 9 rows, 3 TUs, 2 enabled families + gantt disabled | proposed | red-first (all 9 in the roster at 617); journey 14/4 → 18/0; timeline 23/7 → 28/2; global 1528/617 → 1537/608; formal 1342 → 1351; gaps 48, integrity 0; dual-preset byte-identical sha recorded; full debug ctest with a durable log and exit code; branch-reach assertion green; independent P1 code review APPROVE |

## Production Readiness Review

To be completed at the `implementable` gate.

## Rollout and rollback

One atomic commit; revert restores 617 and every pin. No persisted data, wire, CLI or
settings change.

## Drawbacks and risks

- A closed directive scanner in three TU-local copies plus its Python mirror. The batteries and
  the per-row twin gate pin closure; the mirror is exercised by the four generators' `--check`
  runs and by the new hermetic check.
- A disclosed upstream behaviour the terminal cannot emulate (palette/logger directives not
  applied). The footer makes the substitution visible.
- Leading directives that today pass through undisclosed now gain a footer or a verdict: a
  logLevel-only leading directive gains a footer (the only affected existing test source is
  `test_mermaid_journey_cohort.cpp`'s `with_log_comment`, which is updated to assert it), and a
  non-`init` leading directive name / a malformed leading directive now gates. No corpus row of
  either family is affected (measured: the only directives are the 9 admitted and the 2
  excluded).
- Cross-channel theme-table asymmetry is user-visible (`theme: dark` in frontmatter gates while
  the directive admits it). It is corpus-bounded by policy and pinned both ways, but a reader
  may find it surprising.

## Alternatives considered

1. Do nothing — leaves 9 twin-proven rows on fallback and keeps timeline/journey's last
   config-only fallbacks.
2. Admit the state 8 init rows — rejected: their bodies do not draw after the strip, so the
   directive is not their blocker.
3. Unify the directive theme table with the frontmatter table — rejected: it either changes the
   pinned frontmatter contract (journey/timeline verdicts asserted literally today) or drops 5
   of the 9 rows (`base` for timeline; `dark/default/forest/neutral` for journey), so it buys
   consistency at the cost of the slice while leaving the asymmetry for the next value.
4. Reuse `flow_init_directive` by exporting it — rejected: TU-local helper in another partition,
   strict-JSON semantics that reject the corpus's single-quoted/trailing-comment spellings, and
   its notice carries the flowchart branch; exporting it would widen the blast radius into an
   unrelated renderer.
5. Enable directive admission for gantt with an empty theme table — rejected: it would tighten a
   currently-drawn pass-through for logLevel-only sources with no corpus benefit; G4's explicit
   switch keeps gantt bit-identical.

## Testing and verification plan

- New gate (5 gtests) + one hermetic `--check` registered in `tests/ui/CMakeLists.txt`:
  `ctest -L mermaid` delta **+6**; full debug ctest 4687 → 4693 executed, complete stdout and
  exit code captured to `/tmp/loom-logs/ctest-full-dbg-0047.log` (TL 2026-11-22 requirement).
- The migrated family gates are the drawn-row regression oracles; the 9 cohort rows assert
  frame = twin + footer line by line.
- Dual-preset matrix and roster `--emit`/`--self-test`/`--matrix` at closure.

## Documentation impact

- [ ] `docs/dev/d419-next-family-body-init.md` — selection
- [ ] `docs/dev/d420-body-init-rfc0047-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — directive admission entry (including the
      cross-channel asymmetry policy and the gantt switch); RFC 0040 "in-body initializer stays
      a whole-source gate" sentences amended
- [ ] `docs/README.md` — RFC index row (already staged as Provisional; updated at closure)
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — journey/timeline rows, whose current
      cells are also stale/mis-ordered (table header is "Remaining fallback | Drawn"; journey
      reads `10 | 18` from before RFC 0040 and must be corrected to 4/14 → 0/18, timeline to
      7/23 → 2/28)
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: frontmatter visual leaf + keyed directive together? | design review | resolved: gated (G5) |
| OQ-2: bare `%%{init}%%`? | design review | resolved: no-op, no footer (G1.4); no corpus body uses it |
| OQ-3: logLevel value set | design review | resolved: share the `flow_init_directive` value set (0…5 / six names) with a hardcoded literal assertion in the new gate; the parsers differ deliberately (YAML scalar vs strict JSON) and this RFC does not claim they cannot drift |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review (two lenses: selection
  evidence/footprint and grammar/architecture).
- `accepted` → `implementable`: independent PRR.
- Phase: one atomic phase with its review record.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | r1 | agent:design-review-evidence | REQUEST-CHANGES | M1 CMake registration missing from the footprint → added; m1 gantt generator absence measured and stated; m2 probe log does not enumerate paths → claim rescoped, P1 gate is the authoritative proof; m3 summary scope over-wide (4 flowchart legacy rows) → requalified; n1 `prior_status` → added; n2 G5 definition → fixed to keyed directives; m3 d117 stale cells → documented |
| 2026-11-22 | r1 | agent:design-review-grammar | REQUEST-CHANGES | B1 gantt empty-table contradiction → explicit `kFamilyInitDirectivesEnabled` switch; M2 cross-channel table asymmetry → policy paragraph + both-direction pins; M3 CRLF → strip `\r` (G1.2); M4 unterminated quote → quote state machine (G1.5); M5 edge cases → normative clauses and battery list; M6 G5 mechanism + logLevel-only tightening → Proposal step 2 and Drawbacks; m7 logLevel-only test source → footer assertion; m8 rule-38 reach proof → reason assertion in the matrix test; m9 drift wording → softened; n10 tables enumerated; n11 indentation → G1.2; n12 mirror steps 2–4 → stated |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | e47bbaa1 |
| 2026-11-22 | — | r1 revisions folded in (still provisional) | (this commit) |
