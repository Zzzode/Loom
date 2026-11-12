# Gantt family renderer design (RFC 0031, D-066 zero-fallback)

Status: design, 2026-11-12. Precedes implementation. Patterned on the
shipped journey (`744c3a24`) and timeline (`fe3eb520`) greenfield
renderers; same RFC 0031 cohort/ledger/review ceremony, no new RFC.

## 1. Scope and corpus facts (measured, do not re-census)

52 originals under `cases/gantt/{nightly,pr,visual}-check/`. Every file
has YAML frontmatter. Theme/look gate partitions them into **41
theme-clean / 11 themed** (themed stay source: the terminal has no color
contract). Two of the 41 theme-clean fixtures carry deliberately
malformed dates and must fail closed (stay source, never error):

- `...issue-1060...invalid-date.mmd` — task date `2019-09-1` (day not
  zero-padded).
- `...issue-5496...year-typo...mmd` — task date `202-12-01` (3-digit
  year).

Expected native-draw cohort is therefore **39 originals**, subject to
live measurement; the generator drift-guards all three sets
(draw / themed / fail-closed) separately.

Directive census (line-leading keywords across 52 files): `section` 116,
`dateFormat` 51, `title` 47, `excludes` 31, `axisFormat` 27,
`tickInterval` 15, `todayMarker` 4; `includes`, `accTitle`/`accDescr`
rare. Task-tail tokens: `after` 97, `crit` 44, `done` 38, `active` 34,
`until` 3, `milestone` 2. Duration units in corpus: d 201, h 48, m 36,
s 9, ms 7, w 2 (decimals occur, e.g. `0.024s`). `dateFormat` values in
corpus: `YYYY-MM-DD` (optionally followed by `HH:mm:ss`), `x`, `X`,
`SSS`, `ss`, `D`, `HH:mm:ss`, `HH:mm`.

Task field-shape census (label `:` comma-tail; frontmatter prose rows
excluded). Each signature is an accepted parser shape:

| Count | Field signature (first → last) |
|---:|---|
| 51 | `duration` (implicit chained task) |
| 41 | `id, absdate, duration` |
| 37 | `after, duration` |
| 31 | `id, after, duration` |
| 21 | `absdate, duration` |
| 18 | `id, id, time, duration` (vertical-marker form, see §5) |
| 17 | `time, duration` |
| 16 | `tag, id, after, duration` |
| 15 | `id` (frontmatter `key: value` prose, not a task — parser never sees it) |
| 14 | `tag, id, absdate, duration` |
| 14 | `empty` (frontmatter `key:` prose) |
| 11 | `tag, tag, absdate, duration` |
| 9 | `tag, id, absdate, absdate` (start + explicit end) |
| 9 | `tag, tag, after, duration` |
| 9 | `tag, tag, duration` |
| 8 | `tag, duration` |
| 7 | `id, epoch, duration` |
| 4 | `tag, :idref, absdate, duration` (leading-colon id) |
| 4 | `id, absdate, absdate` |
| 4 | `tag, absdate, duration` |
| 3 | `id, after, until` |
| 2 | `milestone, crit, id, absdate, 0` |
| 2 | `epoch, epoch` |
| 1 | `id, <bad absdate>, duration` (fail-closed fixture) |
| 1 | `<bad absdate>, duration` (fail-closed fixture) |
| 1 | `id, epoch, epoch`; 1 `id, epoch, empty` (milestone) |

## 2. Projection decision: structural table, no schedule arithmetic

The terminal cannot draw the SVG time axis. Following the journey/timeline
doctrine ("faithful linear structural listing rather than an SVG geometry
claim"), the renderer parses and **validates** the gantt structure and
projects it to a bounded native text table. It does **not** compute
calendars: no date math, no `excludes`/weekend resolution, no `after`/
`until` endpoint resolution, no tick placement. Schedule tokens are
strictly shape-validated and transcribed. This is the simplest correct
projection, keeps every risk inside one new module unit
(`mermaid_gantt_impl.cpp`), and preserves all semantic content as text.

Non-terminal directives (`dateFormat`, `axisFormat`, `tickInterval`,
`excludes`, `includes`, `todayMarker`, frontmatter `topAxis`/`compact`/
`displayMode`) describe SVG axis/geometry behavior the terminal does not
have; they are accepted as bounded validated text, not interpreted — the
same non-terminal logic already accepted for dagre/useMaxWidth.

## 3. Output shape

```
<title>

Section: <name>
- <label> [crit][done][active][milestone] (id <id>) | <start spec> | <span spec>
```

- One line per task (wrapped at 64 cells via the shared wrap helper), so
  row count stays linear in task count.
- Start spec renders the transcribed token: an absolute date/time/epoch
  token, `after <ids>`, `until <ids>`, or `chained` when only a duration
  is given (implicit predecessor chain).
- Span spec is the duration token, or `end <date>` for the two-date form;
  milestones render `milestone` instead of a span.
- Tags are closed-vocabulary text markers only — no color (no color
  contract), consistent with the other greenfield families.
- Section titles sanitize `<br>` variants to a space before emission.

## 4. Parser and fail-closed rules

- Reuse the frontmatter strip + key-based theme/look visual gate verbatim
  from the timeline renderer (the three greenfield gates must stay
  identical; an independent generator re-scan cross-checks it).
- First non-comment body line must be exactly `gantt`; else `{}`.
- Directives consumed as no-ops after bounded-text validation
  (`unicode_text`, length caps). `dateFormat`/`axisFormat` values are
  opaque identifier-like tokens, not interpreted.
- Task line = label `:` tail; split tail on commas. A small bounded state
  machine accepts exactly the §1 signatures:
  1. zero or more tags from the closed set `{done, active, crit,
     milestone}` — any other tag word is a hard reject.
  2. zero or more id tokens (bare identifier, or `:idref`); corpus max
     two leading ids.
  3. scheduling tail, one of: `duration`; `absdate|time|epoch ,
     (duration|absdate|time|epoch)`; `after <ids> [, duration]`;
     `[after <ids>,] until <ids>`; the milestone zero-span form.
- **Strict date shape validation** is what makes the two malformed-date
  fixtures fail closed without interpreting the calendar:
  - calendar date: `YYYY-MM-DD` with exactly 4-digit year and zero-padded
    2-digit month/day, optional ` HH:mm:ss` (rejects `202-12-01` and
    `2019-09-1`);
  - clock time: `HH:mm` or `HH:mm:ss`; epoch: digits only; SSS: 3 digits;
  - duration: `\d+(\.\d+)?(ms|[smhdw])`.
  Any token outside these shapes → whole source `{}`.
- Bounds (mirror timeline/journey): source ≤ 65536 B; caps on sections,
  tasks/section, fields/task, output rows (512), wrap 64, width 240;
  overflow-checked Work fees; every `work.spend` checked; no throwing
  parse (no `stoi`); `unicode_text` on every emitted string; tab and bare
  CR rejected except the one corpus-required exception below; empty output
  rejected.

## 5. Special forms

- **Vertical markers** (`should-render-vertical-markers...`): task lines
  shaped `label : vert, v1, 17:30, 2m` carry two leading bare identifiers
  before the scheduling fields. Accepted by step 2 of the state machine
  (multi-id transcription); no marker geometry is drawn, the ids are
  preserved in text. If live measurement shows the shape cannot be
  admitted cleanly, the fixture stays source and the ledger records it
  rather than weakening the state machine.
- **`after a b` / `until x y`**: the remainder of the field is a
  space-separated dependency id list; transcribed verbatim, not resolved.
- **Tab inside a break marker**: the multiline-section-title fixture
  contains a literal tab in `<br\t/>`. Unlike timeline (which rejects tabs
  globally), gantt must sanitize break markers with an internal-whitespace
  rule (`<br` + horizontal whitespace + optional `/` + `>`), case
  insensitive, before the global tab rejection runs on the *sanitized*
  text. This is the single deliberate tab carve-out; any other tab still
  fails closed.
- Semicolon in title and `#` in section/task text are ordinary text.

## 6. Non-goals

Computed schedules/dates, weekend/exclude arithmetic, dependency
resolution or cycle detection, time-axis/tick geometry, color for
crit/active/milestone, and compact/topAxis visual behavior. None are
reachable from a structural text projection; revisiting any of them is a
separate reviewed milestone.

## 7. Build and verification checklist

1. New `src/ui/visual/mermaid_gantt_impl.cpp`
   (`gantt_layout(string_view, Work&)`); declaration in
   `mermaid_detail.cppm`; register in `loom_ui_visual.cmake`.
2. `MermaidProbeFamily::Gantt` + classifier header bool (`gantt`,
   including after a frontmatter fence) + dispatch case; `-Wswitch`
   exhaustiveness.
3. Renderer-free `tools/mermaid/generate_gantt_cohort.py` +
   `gantt-cohort.json`: drift guard over the draw(39)/themed(11)/
   fail-closed(2) partition, manifest sha pins, live-measured
   NARROW_DRAW, real review block (no placeholder).
4. `tests/ui/test_mermaid_gantt_cohort.cpp` CellCapture char-vector
   gate (never `Screen::ToString`): whole+body per-width states, themed
   stay source, both malformed-date fixtures fail closed, tag/id/dep/
   milestone/vertical-marker shapes, cap exhaustion, break-marker
   sanitizing including the tab carve-out, theme-word non-gating.
5. 1:1 OfficialMatrix hook (loader + earliest-match branch) so the
   observation total stays 2738 and frozen flow/Class285/48-gap
   dispositions are unreachable.
6. Ceremony per engineering-efficiency.md: release-primary iteration;
   closing full debug+release OfficialMatrix byte-identical; full
   `ctest -L mermaid`; three-family serial gate to an independent log
   with SERIAL_RC and per-case times; ASan contact; independent
   fresh-agent zero-build review before the ledger flips to approved;
   local commits, push only on explicit user request.

## 8. Risk notes

- A strict date regex that over-rejects would only keep a fixture on
  source (never an error), so the failure direction is safe; the full
  matrix confirms no unexpected reject beyond the two documented fixtures.
- Accepting opaque `dateFormat` text while strictly validating task date
  *shapes* is intentional: format interpretation is upstream's job, and
  the two malformed fixtures are still caught by shape alone.
- If the 39-draw estimate is wrong at live measurement, adjust the ledger
  partition from evidence and record why; do not bend a validator to hit
  a number.
