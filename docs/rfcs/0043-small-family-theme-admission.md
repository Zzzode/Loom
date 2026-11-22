---
rfc: 43
title: ishikawa, venn and railroad theme admission
status: implemented
owners: "@Zzzode"
reviewers: ["agent:design-review", "agent:prr-review", "agent:code-review"]
created: 2026-11-22
last-reviewed: 2026-11-22
supersedes: ""
tracking: local
---

# RFC 0043 — ishikawa, venn and railroad theme admission

## Summary

D-410's root-cause bill isolates **6 remaining rows** whose only blocker is an inert
`config.theme` leaf: ishikawa 2 (forest, dark), venn 3 (dark, redux-color,
redux-dark-color) and railroad 1 (dark). Each family's TU-local key-based frontmatter gate
rejects any `theme`/`look`/`config` key outright; this RFC widens each gate for exactly one
shape — a column-0 `config:` block whose sole leaf is an indent-2 `theme` in a closed
corpus table — and discloses the accepted value through a bounded footer appended below the
art.

One phase, three TU-local implementations (no shared module, no BMI change). Target:
ishikawa **7 → 9** drawn / **6 → 4** fallback; venn **11 → 14** / **10 → 7**;
railroad **8 → 9** / **1 → 0**; global **1476 / 669 → 1482 / 663**; formal
`drawn_at_240` **1290 → 1296**.

## Motivation

### Selection (D-411, measured 2026-11-22)

D-410 buckets the 669 remaining wide fallbacks (recomputed structurally on 2026-11-22):
222 exact-bit (flowchart, non-roster), 67 protected/frozen (43 in the protected gap roster
+ 24 ER107/ER206), 380 capability debt. Within the 380 the sub-bucket split is 5a 106
(frontmatter semantic keys), 5b 15 (non-inert `look`/seed/`fontFamily` shapes), 5c 253
(body/mechanism syntax) and **5d 6 inert theme-only rows** — the last currently closeable
sub-bucket, and the subject of this RFC.

The classification is reproducible from the immutable fixture bytes: structural frontmatter
parsing of the D-402 probe rows (726) minus the 57 rows promoted by RFC 0039–0042 = 669;
the verdict for 5d is "semantic key set ⊆ `{config, description, theme}`, not in the
protected roster, not a flowchart-family row".

### Evidence

- All 6 rows strip-draw at 240 columns; the rest of each family's fallbacks are
  `useMaxWidth`/`diagramPadding`/`look: handDrawn`/seed rows or body capability.
- Exactly one other row in the corpus has the same frontmatter shape,
  `conf-and-directives/pr-check/settings-from-initialize-nodes-should-be-green.mmd`
  (theme `forest`). It is **not** admitted here: its renderer mechanism is the flowchart
  chain, not a small-family TU, so it stays in D-410 bucket 5c. The "entire inert vein"
  claim is therefore scoped: this RFC closes every theme-only row reachable through these
  three family gates, not every theme-only row in the corpus.
- Corpus twins (body byte-equality measured against the current 1476 drawn rows):
  ishikawa's two theme rows share the body of
  `ishikawa/pr-check/1-should-render-a-simple-ishikawa-diagram.mmd`; venn's two redux rows
  share the body of
  `venn/pr-check/16-should-render-a-venn-diagram-with-a-3-set-union-without-explicit-pairwise-subsets.mmd`.
  `venn/nightly-check/9-should-render-with-dark-theme.mmd` and
  `railroad/pr-check/ir-dark-theme.mmd` have **no** drawn body-equivalent and use the
  synthetic config-free twin. So four corpus twins and two synthetic twins.
- Row attribution measured with `fallback_attribution.py`'s classifier: the 6 rows sit in
  `family_residual:ishikawa` (4 → 2), `family_residual:venn` (7 → 4) and
  `family_residual:railroad` (1 → 0). The railroad category therefore disappears from the
  roster (44 → 43 categories); the RFC 0040 seeding semantics only force-emit `DETECTORS`
  categories, and the `--self-test` causality/key-set equality shrinks with the counts, so
  the gate stays green. The commentary line that says "44 categories" in
  `tools/mermaid/fallback_attribution.py` is made count-free in the same phase.
- None of the 6 is in the protected `e6c-mandatory-gap-roster.json` (whose fallback rows
  are class-diagram 25 + flowchart 18), so `mandatory_gaps` stays 48 and no protected file
  changes.

### Why admission is testable

None of the three renderers reads `theme`; the text surface has no colour contract. The
proof is the established two-part oracle: per-row twin inertness (corpus twin frame, or the
config-free synthetic twin's frame, plus exactly the ledger's literal footer delta, wide
only) and a drawn-row frame-identity oracle per family over its already-drawn rows.

## Goals

- **G1.** Admit exactly the closed per-family tables: ishikawa {dark, forest};
  venn {dark, redux-color, redux-dark-color}; railroad {dark}.
- **G2.** Disclose each accepted leaf through a bounded footer — header
  `Ishikawa native projection v1` / `Venn native projection v1` /
  `Railroad native projection v1`, then exactly one line
  `config.theme=<value> -> colour palette; not applied`.
- **G3.** Promote exactly 6 rows; every other accept/reject verdict is unchanged.
- **G4.** No regression: each family's already-drawn rows keep their frames cell-for-cell
  and the dual-preset matrix stays byte-identical.

## Non-Goals

- `ishikawa.useMaxWidth`/`diagramPadding`, venn's `look`+`handDrawnSeed` rows, and any
  body-capability row.
- The flowchart-mechanism `conf-and-directives` theme row (bucket 5c).
- flowchart's 222 exact-bit rows and the 67 protected/frozen rows (the D-410 policy fork).
- narrow_40 drawing; painting the palettes; the ER107 B3 gates.

## Proposal

### 1. Closed tables (per family, corpus-bounded)

| family | TU constant | admitted `theme` |
|---|---|---|
| ishikawa | `kIshThemeTable` | `dark`, `forest` |
| venn | `kVennThemeTable` | `dark`, `redux-color`, `redux-dark-color` |
| railroad | `kRailThemeTable` | `dark` |

No `look` value is admitted by any family.

### 2. Grammar (config-block-scoped, TU-local, three copies)

The three gates today are flat key scans over each TU's existing frontmatter bounds: the
leading spaces are stripped, the text before the first colon is compared, and **any**
`theme`, `look` or `config` key at **any** indent makes the whole source fall back. Every
other key is ignored; body gates are unchanged (`%%{init` + `classDef`/`style `/`:::` in
ishikawa and railroad, `classDef`/`style `/`:::` only in venn).

The revision keeps that scan's envelope (opening `---\n`, the TU's fence rule, the
column-0 metadata passthrough) and replaces only the `theme`/`config` verdicts:

- A column-0 non-blank, non-comment line carrying a colon is a block header. `config:`
  opens the config block — exactly one per frontmatter; a second rejects. `theme` or `look`
  at column 0 rejects (today's verdict). Any other column-0 key keeps today's passthrough
  and **closes** an open config block. A column-0 line without a colon keeps today's
  passthrough.
- Inside the config block: blank lines and YAML comments (leading `#`) are skipped; any
  line with a non-zero indent is interpreted. The admitted leaf is exactly an indent-2
  `theme` read through the TU-local scalar helper below, whose dequoted/commented value
  must be in that family's table. Everything else — another leaf name, a colon-less line,
  a non-2 indent, a second `theme` — rejects the whole source. The leaf key is compared
  **raw** (text before the first colon, agentflow's `leaf.substr(0, colon) != "theme"`), so
  `  theme : dark` rejects in all three families; railroad's column-0 key trim never
  applies to the nested leaf.
- A config block that ends without an admitted `theme` rejects.
- Indented `config`/`look` keys outside an admitted config position still reject exactly as
  today (the old flat scan did not ignore them); indented non-visual keys outside a config
  block are still ignored.
- The in-body `%%{init}%%`/style/classDef gates stay unchanged.

**Scalar helper (TU-local copy of agentflow's `af_scalar`, three copies —
`ishikawa_scalar`, `venn_scalar`, `railroad_scalar`):** trim leading/trailing spaces and
tabs; strip a trailing `#` comment that starts at index 0 or follows a space/tab, outside
quotes; charge `value.size() + 1` to `Work` on the stripped-but-not-dequoted value; then
dequote a matched leading/trailing pair. Pinned verdicts (identical in all three families;
the table applied is the family's):

| shape | verdict |
|---|---|
| `  theme: dark` | admitted, footer `config.theme=dark -> colour palette; not applied` |
| `  theme: dark # note` | admitted (comment stripped) |
| `  theme: "dark"` / `  theme: 'dark'` | admitted (matched pair dequoted) |
| `  theme: "dark` | rejected (unmatched quote never matches the table) |
| `  theme: dark extra` | rejected (not a table member) |
| `  theme:` (empty) | rejected |
| `   theme: dark` / `    theme: dark` | rejected (indent ≠ 2) |
| `theme: dark` at column 0 | rejected (today's gate) |
| `  theme: forest` in venn, `  theme: redux` in ishikawa | rejected (other family's table) |
| `theme: DARK` | rejected (case-sensitive) |
| `  look: handDrawn` / `  htmlLabels: false` in config | rejected (non-theme leaf) |
| `  theme dark` (colon-less) | rejected |
| two `theme` leaves, or two `config:` blocks | rejected |
| `config:` with no theme | rejected |
| `  # comment` or a blank line inside config | skipped (theme elsewhere still decides) |
| `\ttheme: dark` | rejected by the existing tab pre-check |
| `config: {theme: dark}` (inline flow mapping) | rejected (no block leaf) |
| `config:` with theme **first**, then column-0 `description:` + payload | admitted (description closes the block; its payload is outside and ignored) |
| `description:` nested **inside** the config block (indent > 0) | rejected (non-theme leaf) |
| unrelated column-0 metadata key (`title: x`) before/after | passthrough; an admitted theme elsewhere still decides |

### 3. Per-family preserved behaviour (must not drift)

| behaviour | ishikawa | venn | railroad |
|---|---|---|---|
| column-0 key comparison | raw text before `:` | raw text before `:` | **spaces trimmed around the key** |
| frontmatter fence | `---` + optional trailing spaces/tabs | exact `\n---\n` | `---` + optional trailing spaces/tabs |
| body `%%{init` gate | yes | **no** | yes |
| row cap used by the footer height check | `kIshikawaMaxRows` 1024 | `kVennMaxRows` **512** | `kRailroadMaxRows` 1024 |

### 4. Disclosure footer

Appended inside the family's own `*_layout` (no parser seam, no new entry point): blank
separator row, header, theme line. Line cap 96 bytes; per-line charge
`size * 4 + sizeof(std::string) + 32`; emission charge
`(1 + n) * (4 * sizeof(std::string) + 256) + bytes * 2 + 256`; the append is refused when
`art_rows + 1 + n >` the family's row cap. The footer is appended **after** venn's title
combine (`mermaid_venn_impl.cpp:363-369`), so `art_rows` is simply `out.size()`, which
already includes the title rows — the same quantity the existing cap counts. Refusal keeps
the whole source on fallback.

### 5. Reviewed cohort, twin gate and wiring

- One sibling ledger `tools/mermaid/smallfam-0043-cohort.json` (schema
  `loom_mermaid_smallfam_0043_cohort/v1`) with the 6 rows (a `family` field), literal
  `expected_states` (wide measured), `expected_footer_lines`, and `twin` (corpus with
  path+sha for the four body-identical rows, synthetic for venn `dark` and railroad).
- The three family ledgers gain `admitted` / `admitted_count` / `admitted_digest` in
  `selection` (agentflow's `admitted_count`/`admitted_digest` naming, plus the explicit path
  list this RFC needs) and a four-way partition
  (`rows + admitted + frontmatter_gated(remaining) + body_style_gated` = originals):
  ishikawa 7 + 2 + 4 + 0 = 13; venn **11 + 3 + 3 + 4** = 21; railroad 8 + 1 + 0 + 0 = 9.
  Generators must **reclassify, not subtract**: a row whose only frontmatter gate was an
  admitted theme and whose body carries `style `/`classDef`/`:::` moves to
  `body_style_gated` (venn `explicit-set-style-over-redux-palette.mmd` is exactly this case;
  its `config.theme: redux-color` is admitted, then its body style fires). Pins therefore
  change: ishikawa frontmatter 6 → 4; venn frontmatter 7 → **3** and body 3 → **4**;
  railroad frontmatter 1 → 0.
- The sibling ledger and the three family `admitted` lists carry the same six paths; the
  digest is agentflow's definition — sha256 of the concatenated `path\tsha256\n` lines
  (`generate_agentflow_0041_cohort.py:107-109`) — and the gate asserts sibling ↔ family
  agreement path by path and per-family digest, so the duplicate cannot drift silently.
- The partition check is **literal arithmetic against the enumerated fixture total**, not a
  recomputation from the ledger's own lists: ishikawa 7 + 2 + 4 + 0 = 13, venn
  11 + 3 + 3 + 4 = 21, railroad 8 + 1 + 0 + 0 = 9, with the filesystem enumeration of each
  family's fixtures as the total, so a row dropped from every list is caught.
- Generator `tools/mermaid/generate_smallfam_0043_cohort.py`, renderer-free,
  `--check`-reproducible, pinned path sets/digests, body-equivalence verification. **The
  Python path/digest set and the C++ closed tables are a string-shape coupling**: they must
  name the same six paths and the same admitted values, and the gate asserts ledger ↔ C++
  agreement row by row so the two copies cannot drift silently (design-decisions hazard).
- **Twin rule**: wide-only frame equality = twin frame + the literal footer delta through
  the same sentinel wrapper; narrow is a separate live measurement (rule 34) — the footer
  line is 48–60 characters, wider than a 40-column viewport, so the expected measured state is
  `source_fallback` on narrow for all 6: expected 0/6 until measured, from the RFC 0041
  precedent (the 9 agentflow footer rows, whose theme lines vary 48–60 characters, all
  measure narrow `source`), and recorded from the live gate run — never asserted from the
  prediction.
- Gate `tests/ui/test_mermaid_smallfam_0043_cohort.cpp`: ledger pins (schema, review
  pins, the 6 literal paths with sha256); per-row twin inertness; one closed-table battery
  per family (every admitted value with the exact two-line footer; the rejects in §2);
  per-family four-way partition checks against their ledgers; a live narrow measurement;
  and a drawn-row frame-identity oracle over each family's already-drawn rows (7 + 11 + 8 =
  26).
- Matrix branch: one block per family, before the family cohort branch and the generic
  fallback, reason `RFC-0043 small-family theme reviewed cohort`, states read per row,
  `raw_active_schema` empty so the generic `originals_drawn` counter credits +6. The
  existing family-branch commentary is corrected to the post-admission counts and points at
  the sibling branch: venn "ten gated" → 7, ishikawa "six gated" → 4, railroad "one gated" → 0.
- Roster migrated with `fallback_attribution.py --emit` after the cohort gate is green:
  669 → 663; `family_residual:railroad` empties (44 → 43 categories).

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| P1 | `theme` admission | 6 rows, 3 families | **done** | measured: ishikawa **9/4**; venn **14/7**; railroad **9/0**; global **1482/663**; formal **1290 → 1296**; gaps 48, integrity 0; narrow **0/6** live; dual-preset sha `8de4c546…`; full debug ctest 4667/4667 |

Closure: full debug ctest once (D-401 §7 discipline), dual-preset byte gate, RFC
`implemented`, closure doc, design-decisions entry, efficiency retro.

**Closure-report format (TL 2026-11-22):** every closure report states three numbers side
by side and none may replace another — ① the absolute `source_fallback` total (expected
**663**), ② the part of it beyond a certified profile or frozen/protected authority
(expected **289 = 222 exact-bit + 67 protected/frozen**, unchanged by this phase), and
③ this phase's net reduction (**6**).

## Production Readiness Review

Completed at the `implementable` gate by an independent PRR agent (different lens from the
design reviewer). The questionnaire rows:

### 1. Correctness and tests

- Every new behaviour has a new test: the six-test cohort gate, the three family-test pin
  changes, and four hermetic generator `--check` entries.
- Failure paths covered: every §2 verdict row (table miss, quoted/unmatched, indent, leaf
  name, duplicate, no theme, fence/body gates).
- Golden suites: N/A — no rendered-cell golden, no truecolor path, no dialog/session shape
  is touched; the text surface is asserted by the cohort gate itself.
- Known flakes: none added; no wall-clock assertion.
- Serial ctest total: 4657 → **4667** (+10), the pie-closure baseline.
- Cross-module couplings named and both-sided: sibling/family ledger fields, matrix reason
  strings, roster category names, C++ closed tables ↔ Python path/digest set, footer
  literals. `docs/decisions/design-decisions.md` consulted.

### 2. Build system and module discipline

- BMI PSS: N/A — the three files are `.cpp` body units; no `.cppm`, import, or module
  change, so no BMI producer is affected.
- No Ninja concurrency reduction; no interface declaration added; no new textual includes;
  no new module (the graph stays a DAG, unchanged).

### 3. Rollback and compatibility

- One atomic commit including the roster, so a single `git revert` restores 669 and every
  pin together; no separate `--emit` step.
- No persisted user data, wire protocol, CLI, or settings change.

### 4. Observability

- No new diagnostics, threads, or render ticker; the layout stays event-driven; session
  traces unaffected.

### 5. Documentation and deletion

- The three old flat gates are replaced, not wrapped. RFC, closure doc, design-decisions
  entry, docs index, d117 rows, d394 historical note, retro, audit-fixture note.

### 6. Platform readiness

- Debug + release build `-Werror` clean on the Linux dev box (the closure gate); output is
  ASCII cell text with no platform-specific code, so termios/signals guards are untouched.
- macos-14 CI is not a verification gate in this project (directive 2026-09-29); it must
  not be waited on.
- truecolor golden path untouched (no colour is painted). Offline dependency cache
  unaffected; no new fetch.

### Review sign-off

| Role | Reviewer | Date | Verdict |
|---|---|---|---|
| Design | agent:design-review | 2026-11-22 | approve (r3) |
| Production readiness | agent:prr-review | 2026-11-22 | approve (6 MINOR + 2 NIT folded) |
| Code (per phase) | agent:code-review | 2026-11-22 | approve (3 MINOR + 1 NIT folded) |

## Rollout and rollback

One phase lands P1. Rollback is one revert plus a rebuild/re-measure. No persisted user
data, no wire protocol, no CLI contract changes.

## Drawbacks and risks

- **Three TU-local copies** of the gate shape and the scalar helper. A shared helper was
  rejected for the same BMI-fan-out reason as RFC 0040/0041; the per-family batteries and
  the shared §2 verdict table keep them from drifting.
- **A disclosed leaf the renderer cannot paint** — stated by the footer.
- **A vacuous negative loop**: once railroad's gated list is empty, a
  "gated rows must not draw" loop would pass on nothing. The existing family tests
  therefore assert the partition sizes explicitly and add admitted draw+footer positives.

## Alternatives considered

1. **Do nothing.** Leaves the last 6 family-reachable inert-key rows behind while six
   families already disclose colours.
2. **Three separate RFCs.** Rejected: near-identical documents and review chains for 1–3
   rows each.
3. **Flowchart's 222 rows instead.** Rejected: exact-bit machine and the protected roster.
4. **Folding in the `useMaxWidth`/`diagramPadding`/seed rows.** Rejected: non-inert
   (layout/geometry) by the D-410 classification.
5. **A shared frontmatter helper.** Rejected: cross-TU BMI fan-out for three call sites.

## Testing and verification plan

- **Red first.** Before the change, record that the 6 rows are `source_fallback` on the
  committed matrix (roster `total_fallback` 669) — promotion is measured, not assumed.
- New gate executable `test_mermaid_smallfam_0043_cohort`, registered with
  `LABELS "mermaid"`, six `TEST`s: `LedgerSchemaReviewsAndPins`,
  `RowsAreInertAgainstTwins`, `ClosedTablesAdmitWithExactFooter`,
  `OutOfTableAndShapeRejects`, `FamilyLedgersPartitionFourWays`,
  `DrawnRowsKeepFrameIdentity`.
- Four hermetic generator checks (+1 for the new generator, +3 for the existing
  `generate_{ishikawa,venn,railroad}_cohort.py`, which already support `--check` but are
  not registered): expected `ctest -L mermaid` delta **+10** (6 gtests + 4 hermetic).
- Existing family tests change in the same commit (they pin the old partitions):
  `tests/ui/test_mermaid_ishikawa_cohort.cpp` (`frontmatter_gated_count` 6 → 4, admitted
  draw+footer assertions for the 2 promoted rows),
  `tests/ui/test_mermaid_venn_cohort.cpp` (`frontmatter_gated_count` 7 → 3,
  `body_style_gated_count` 3 → 4, 3 promoted positives),
  `tests/ui/test_mermaid_railroad_cohort.cpp` (1 → 0; the gated loop would be vacuous, so
  it is replaced by an explicit asserted-empty partition plus the admitted positive).
  `test_mermaid_official.cpp` gains the sibling-ledger matrix branches.
- Targeted `ctest -L mermaid` in the phase; release matrix in the phase; dual-preset byte
  gate at closure; full debug ctest once at closure.
- Roster in the only green order: review approved → full rebuild → both matrices → cohort
  gate green → `--emit` → `--self-test` → `--matrix`.
- **False-green guardrails.** The matrix branch is a ledger echo; the render proof is the
  cohort gate; full build before any matrix verdict; both presets rebuilt before the sha
  compare; a red full gate handled per D-401 §7.

## Documentation impact

- [x] `docs/dev/d410-fallback-root-cause-bill.md` — measured 5a/5b/5c/5d split
- [x] `docs/dev/d411-next-family-ishikawa-venn-railroad.md` — selection
- [x] `docs/dev/d412-small-family-rfc0043-closure.md` — closure record
- [ ] `docs/decisions/design-decisions.md` — small-family admission entry
- [ ] `docs/README.md` — RFC index
- [ ] `docs/dev/mermaid-d117-family-fallback-counts.md` — ishikawa/venn/railroad rows
- [ ] `docs/dev/engineering-efficiency.md` — per-task retro
- [ ] `tests/fixtures/mermaid_native/direct_theme_source_audit.v1.json` — add a
      `superseded_by` note; the file is unconsumed (no code reads it), so the edit is
      documentation only
- [ ] `docs/dev/mermaid-next-family-spike-d394.md` — mark the ishikawa-6/railroad-1
      inventory historical

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| OQ-1: one ledger for three families or three sibling ledgers? | design review | resolved: one ledger with a `family` field (6 rows, one phase record), sibling naming as in RFC 0041 |
| OQ-2: does venn `dark` (no corpus twin) use a synthetic twin? | design review | resolved: yes, together with railroad (2 synthetic of 6) |
| OQ-3: narrow_40 disposition | implementation | ledger `expected_states`, measured live |

## Gates and who confirms

- `provisional` → `accepted`: independent agent-run design review.
- `accepted` → `implementable`: independent PRR with a different lens.
- Phases: one atomic phase, with its review record.

## Review History

| Date | Round | Reviewer | Verdict | Findings and disposition |
|---|---|---|---|---|
| 2026-11-22 | r1 | agent:design-review | REQUEST-CHANGES | 2 BLOCKER + 5 MAJOR + 8 MINOR + 3 NIT. B1 venn `dark` row missing from the scope → added (6 rows, endpoints corrected); B2 the three existing family tests were unmentioned → named with pin deltas; MAJOR-1 "indented lines ignored" was wrong (the flat scan rejects indented visual keys) → grammar rewritten with the exact admitted position; MAJOR-2 venn partition arithmetic → 11+3+4+3; MAJOR-3 scalar semantics unpinned → `af_scalar` copy and the §2 verdict table; MAJOR-4 per-family behaviours → §3 table; MAJOR-5 "entire vein" overclaim → scoped with the conf-and-directives exclusion; MINORs: D-411 roster numbers (measured 4/7/1 → 2/4/0, 44→43), D-410 arithmetic recomputed structurally, live narrow assertion named, gate+footer in the layout body, exact footer literals, +10 test delta, graph unchanged, C++/Python table coupling, agentflow ledger field names; NITs: `config.ishikawa.*` naming, 44→43 comments, arithmetic note. All folded |
| 2026-11-22 | r2 | agent:design-review | REQUEST-CHANGES | Scope (exactly 6), endpoints, roster, twins, scalar, +10 delta, protected files all confirmed. 2 MAJOR + 4 MINOR + 1 NIT: (1) venn partition is 11+3+3+4, not 11+3+4+3 — `explicit-set-style-over-redux-palette` reclassifies to body_style once its theme is admitted (this also refutes r1's folded "7→4" pin; correct pin 7→3, body 3→4); (2) the §2 row "config first, description after → rejected" contradicted the grammar (column-0 description closes the block) → relabeled and the positive added; MINORs: matrix commentary values are 4/7/0, agentflow has no `admitted` path list (kept the list and added a sibling↔family cross-check), venn footer cap must count title rows, missing unrelated-column-0 passthrough positive; NIT: narrow 0/6 is expectation-until-measured with the RFC 0041 precedent. All folded |
| 2026-11-22 | r3 | agent:design-review | **APPROVE-TO-ACCEPT** | All r2 folds re-derived and confirmed (venn 11+3+3+4, §2 config/description semantics against `af_frontmatter_gated`, pin numbers 6→4/7→3/3→4/1→0, matrix commentary 4/7/0). One NIT: the RFC 0041 narrow precedent said "identical 48-character line"; the 9 rows' lines vary 48–60 and are all narrow `source` → reworded. Gate: provisional → accepted |
| 2026-11-22 | PRR | agent:prr-review | **APPROVE-TO-IMPLEMENT** | Independently confirmed: corpus twins byte-identical, endpoints/roster/partition arithmetic, footer lengths, inline-gate reachability (no interface change), matrix branch placement cannot shadow, generators green pre-registration, roster category removal clean, one-revert rollback, protected ten untouched. 6 MINOR + 2 NIT folded: venn footer append point + `art_rows = out.size()` after the title combine; nested leaf key compared raw (`  theme : dark` rejects everywhere); digest defined (`path\tsha256\n`); partition check is literal arithmetic against the filesystem total; red-first baseline step; PRR questionnaire filled; six TEST names + `LABELS "mermaid"`; audit/d394 doc edits named. Gate: accepted → implementable |
| 2026-11-22 | P1 | agent:code-review | **APPROVE** | Independent read-only review ran the live binaries: gate, three family cohort tests and four hermetic checks. Verified gate fidelity against `git diff` and `af_frontmatter_gated`, af_scalar/Work parity, footer budgets with venn's 512-row cap including title rows, generator/C++ verdict parity, digest order, non-vacuous tests, matrix branch placement. 3 MINOR + 1 NIT folded: scalar normalizations now run for every family, dead census/`case_base` code removed, venn signature line restored, tab-blank parity documented |
| 2026-11-22 | — | — | draft | Created provisional from D-410's 5d bucket |

## Implementation History

| Date | Phase | Change | Commit |
|---|---|---|---|
| 2026-11-22 | — | Draft created (provisional), no code | — |
| 2026-11-22 | — | Revised after design review r1; no code | 6c6d0993 |
| 2026-11-22 | — | Folded design review r2; accepted; no code | 2f07e491 |
| 2026-11-22 | — | Accepted after design r3; PRR folded; implementable; no code | a7953e42 |
| 2026-11-22 | P1 | theme admission implemented: three TU gates + footers, six rows promoted, sibling ledger, gate + hermetic checks, matrix branch, roster 669→663 | 7aa711a3 |
