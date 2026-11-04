# F0 design — RFC 0031 E2b activation, state families (inert config)

Status: **IMPLEMENTED 2026-11-05, pending the main-agent-committed close
after independent read-only evidence review.** Design produced by the
main agent against `73c8d0a9`; an early review fork over-reached its
read-only brief (it committed docs and wrote the implementation itself),
so its authorship is not taken as review. The main agent audited every
line of the resulting changes, rebuilt and re-ran all evidence under its
own control, and a fresh independent read-only evidence reviewer
adjudicates the batch before commit. Measured result: 40 of 61
whitelist state rows promote (863→903 originals drawn, 1282→1242
fallback), 21 blocked by a closed ten-detector vocabulary.
Investigation was read-only plus temporary, fully-removed render probes
in `test_mermaid_official.cpp` (tree restored clean).

Part of the D-066 zero-fallback program
(`docs/dev/mermaid-d066-zero-fallback-plan.md`), Track A batch F0,
restricted in this first batch to **state-diagram and state-diagram-v2**.

## 1. Why this is E2b, and what E2b mandates

RFC 0031 (`docs/rfcs/0031-…md` §"Phase E2b (deferred)") pre-authorized
config admission only behind four requirements:

1. a per-family, per-key table (projected / declared non-terminal /
   unsupported), the whitelist being the only authority;
2. an RFC 0030 D4 amendment **only if** a theme/themeVariables/init
   theme key is accepted — this batch accepts none, so no D4 amendment;
3. a user-visible note per rendered diagram naming the settings not
   applied in the terminal;
4. a source-registered cohort and ledger for every affected row.

This batch activates E2b for exactly the inert (non-terminal) state keys
below, reusing the existing E2a cohort machinery
(`tools/mermaid/e2a-cohort.json`, `generate_e2a_cohort.py`,
`tests/ui/test_mermaid_e2a_cohort.cpp`, matrix ledger integration at
`test_mermaid_official.cpp:5451-5476,7999-8024`) and the existing
ER projection-notice pattern (`mermaid_er_impl.cpp:386-433`,
`ErPresentation::config_notices`).

## 2. Corpus facts (measured, not assumed)

Whole-fallback census (1278 parsed frontmatters): `logLevel`
"0"=83/"1"=26/"2"=2; `securityLevel` loose=54/sandbox=1; `fontFamily`
courier=136/136 (no other value anywhere in the corpus). Production
removal probe (temporary, removed): state-diagram-v2 has 21 and
state-diagram 12 fallback originals whose only config keys are inert per
section 3 and whose body already produces non-ordinary art (33 total, an
upper bound — body feature debt keeps the rest blocked; the ledger
classifies every row, see section 5).

The full static whitelist-key-only set among the 144 state fallbacks is
**61 files, zero body init**: state-diagram v1 23 (all
`{fontFamily: courier, logLevel: 0}`); state-diagram-v2 38 (30
`{fontFamily: courier, logLevel: 0}`, 6 `{logLevel: 0}`, 1
`{fontFamily: courier, logLevel: 1}` — the lone `info`-body row, and 1
`{securityLevel: loose}` — `clickable-state-with-tooltip.mmd`).
State-only value universe: fontFamily courier 68/68; logLevel "0"=59,
"1"=1 (no "2" in state); securityLevel loose 2 (one pure v2 row, one
alongside `look` and therefore rejected). The other 83 state fallbacks
carry theme/look/layout/`state.useMaxWidth` and stay excluded. **All 61
files already carry folded `description: >-`**, so the existing mandatory
folded-description gate (`mermaid_relations_core_impl.cpp:1850-1852`)
needs no relaxation.

## 3. Per-family per-key table (the whitelist)

E2b-1 requires every reviewed path to be classified. Exhaustive table
for stateDiagram/stateDiagram-v2 frontmatters:

| path | accepted values / form | class | terminal disposition |
|---|---|---|---|
| `description` | folded `>-` only (existing E2a rule; all 61 rows satisfy it) | **projected** | parsed by the envelope, never rendered (existing behavior) |
| `title` | plain scalar ≤256 printable chars (existing E2a rule) | **projected** | prepended as the diagram title (existing behavior) |
| `config.logLevel` | `0`, `1`, `2`, bare or quoted | **non-terminal** | mermaid logger verbosity only (mermaid.core.mjs setLogLevel path); no parse/render branch |
| `config.fontFamily` | `courier` only, bare or quoted | **non-terminal** | terminal uses one fixed monospace cell font; courier is the closest match. Advance-width equivalence is NOT claimed from config — every promoted row proves geometry via its pinned five-frame snapshot |
| `config.securityLevel` | `loose`, `sandbox` | **non-terminal** | terminal never executes HTML/script/click handlers; sandbox only changes SVG iframe attachment upstream (mermaid.core.mjs:1316-1342), identical geometry. NOTE: zero state-corpus row exercises sandbox (the corpus's single sandbox row is another family); the branch is accepted on upstream evidence alone and gets no corpus pixel row this batch |
| every other key/value (`theme`, `look`, `layout`, `themeVariables`, `handDrawnSeed`, `state.*`, unknown keys, nested maps, non-courier fonts, other log levels/security levels, any init directive) | — | **unsupported** | whole source keeps ordinary fallback; fail closed, one bit per key like the flow profile (`mermaid_impl.cpp:1557-1748`) |

**loose-HTML audit of the 61 bodies (basis on which loose is safe
here):** the only angle-bracket constructs are `<br>`/`<br/>`/
`<br />` (the production plain-label decoder already maps these to '\n',
`mermaid_relations_core_impl.cpp:37-45`), state `<<fork>>`/`<<join>>`
syntax (not HTML), and one `click A "url" "tooltip"` statement in the
tooltip row (non-geometry; sandbox would disable it, loose retains it —
same static diagram). No other tags exist. The tab-form `<br\t/>`
(multiline different-line-breaks files) is NOT decoded and the label
parse rejects it; those rows fail closed and are predicted-blocked
(section 5).

## 4. User-visible disclosure footer (E2b requirement 3)

Bare `config.k=v` lines do not satisfy E2b-3/RFC 0030 D6, which require
stating WHICH settings were not applied. The state path appends a fixed
bounded disclosure block beneath the art (precedent: ER projection's
"ER native projection v1" header plus `-> …` explanation rows and blank
separator, `mermaid_er_impl.cpp:427-443`,
`mermaid_relations_core_impl.cpp:5097`):

    State native projection v1
    config.fontFamily="courier" -> terminal font; not applied
    config.logLevel=0 -> logger only; not applied
    config.securityLevel="loose" -> no interaction layer; not applied

(Leaf lines are alphabetical by key; an actual diagram emits only the
leaves its config carries.)

Conventions (all snapshot-pinned, therefore review-visible):

- Header `State native projection v1` is emitted once whenever at least
  one non-terminal leaf is accepted; every accepted leaf gets one line in
  the fixed form `config.<key>=<value> -> <phrase>`. The closed
  key→phrase table (the only three phrases that exist):

  | key | phrase (exact) | longest composed line |
  |---|---|---|
  | `logLevel` | `logger only; not applied` | 45 bytes (`=0/1/2`, always a bare integer) |
  | `fontFamily` | `terminal font; not applied` | 57 bytes |
  | `securityLevel` | `no interaction layer; not applied` (value-neutral: loose enables clicks/HTML upstream and sandbox disables them, but the terminal has no interaction/HTML layer under either) | 67 bytes (`="sandbox"`) |

- Values: integers bare (`0`), strings quoted (`"courier"`, `"loose"`,
  `"sandbox"`), booleans would render `true`/`false` (none occur in this
  batch's whitelist). A state-specific small encoder emits these; the ER
  notice encoder (`mermaid_er_impl.cpp:334-351`, which rejects integers)
  is NOT widened or touched.
- Leaf line order is alphabetical by key (fontFamily, logLevel,
  securityLevel) — a new state-specific convention, deliberately fixed
  and pinned, not ER's source order.
- **State-specific line cap is 96 bytes per row** (a distinct
  `kStateNoticeLineLimit` constant; the composed rows above are ≤67),
  deliberately separate from ER's 32-byte notice cap
  (`mermaid_relations_core_impl.cpp:5072`), which this path must NOT
  reuse. The whole block is bounded by kHeightLimit and Work-prepaid
  (section 5): the spend covers each appended row plus
  `sum(line bytes)` style-run cells, on the same formula basis as
  5077-5081 but with the state constants.
- **Zero-notice byte-identity guarantee**: an envelope with no accepted
  non-terminal leaf (all E2a rows, and any config-less state source)
  appends neither header nor separator; its art stays byte-identical to
  today. This is asserted in the cohort test.

## 5. Ledger, gate, evidence, and the real render append point

Production changes (state path only):

- Extend `state_source_envelope`
  (`mermaid_relations_core_impl.cpp:1817-1872`) from description/title
  acceptance into the section-3 whitelist profile: one bit per key, exact
  value checks, quoted/bare scalars, Work fail-closed, init still
  rejected. A present but empty `config:` map stays rejected (at least one
  accepted leaf is required), preserving the pre-E2b outcome. Accepted
  leaves are returned on `StateEnvelope` as a bounded vector of fully
  composed footer lines (≤3 leaves, fixed storage).
- **The footer is appended on the STATE canvas path, not via ER.** State
  renders through `canvas.lines()` (`mermaid_relations_core_impl.cpp:
  5953-5985`, where title and the direct-style legend are already
  appended); `er_paint` is null for state and ER's
  `er_projection_notices`/`ErPaint::finish` (5037-5102) are never reached.
  The new code appends the section-4 block right after `canvas.lines()`,
  modeled on the title append at 5957-5972: kHeightLimit guard
  (`result.size() + 1 + notice rows`), per-line
  `kStateNoticeLineLimit = 96` check (state constant, NOT ER's 32),
  explicit Work spend covering rows + `sum(line bytes)` + the style-run
  vector (plain style, same formula basis as the ER notice accounting at
  5077-5081 with the state constants), and a leading blank separator row
  only when the block is present.
- No other family gate, the ER encoder, theme/layout/look handling, or
  any contract changes in this batch.

Ledger/evidence (E2b requirement 4), mirroring E2a:

- NEW `tools/mermaid/e2b-cohort.json` + `tools/mermaid/generate_e2b_cohort.py`
  (mechanical byte-level selection, never renderer-derived; `--check`;
  approved-verdict pin): every unchanged state-diagram(-v2) original
  with column-0 `---`, exactly section-3 whitelisted leaves at the stated
  values, folded description, and no body init. Every selected row is
  pre-registered predicted-draw or predicted-blocked with a
  CLOSED blocker vocabulary and a sha-bound reviewed execution set;
  zero rows unclassified (E2a rule).
- Closed blocker coverage for the 61 rows (delivered vocabulary, ten
  detectors; every row is classified, zero residual): five
  `state-style-statement` (classDef/class/style), one
  `state-click-statement` (the tooltip row), one
  `state-choice-double-bracket` (`state X [[choice]]`), two
  `state-fork-join-html-entity` (`&lt;&lt;fork/join&gt;&gt;` entities),
  two `state-label-br-tab-rejected` (`<br\t/>`), two
  `state-edge-literal-newline` (literal `\n` in an edge label), one
  `state-edge-html-entity` (`#colon;`/`&…;` in a label), two
  `state-composite-divider` (standalone `--`), one
  `state-non-state-body` (the lone `info`-body file), and a sha-bound
  reviewed set `state-native-routing-debt` of four rows (labeled
  multi-edge fan-out and labeled composite entry/exit plans the bounded
  router cannot close without overlap). The compact self-loop riser
  turned out acceptable under the five-frame gate and is among the 40
  draw rows. The generator may PROPOSE the routing set from its
  verification run, but the membership is FROZEN only at ledger approval
  — the approved-verdict review pins the exact sha set, and `--check`
  never grows it automatically (same rule as E2a's reviewed sets). A row
  enters predicted-draw only after its production art (footer included)
  is verified by the dedicated cohort test; classification is purely
  source-content plus the frozen reviewed set, never ad-hoc renderer
  output, and any row outside the vocabulary aborts generation.
- NEW `tests/ui/test_mermaid_e2b_cohort.cpp` registered in
  `tests/ui/CMakeLists.txt` (copy the add_executable pattern at
  ~298-302), mirroring `test_mermaid_e2a_cohort.cpp`: ledger
  schema/approval pin, fixture byte pins, every predicted-draw row
  actually renders WITH the disclosure footer and every predicted-blocked
  row keeps whole source at 240; zero-notice byte-identity for a
  config-free control; narrow_40 uniformly source and restored_240 ==
  wide_240 (E2a conservative width stance).
- Matrix: add an E2b ledger map after the E2a one
  (`test_mermaid_official.cpp:5451-5476`) and a disposition branch after
  the E2a block (~7999-8024; E2a/E2b are disjoint by construction — E2a
  selects config-free rows), reason
  `RFC-0031 E2b reviewed cohort`. The matrix records disposition only;
  pixels are gated exclusively by the dedicated cohort test, so no row
  can be admitted without its pixel gate.

Roster pins that can move and must be re-pinned together through the
reviewed `verify_e6c_gap_roster.py --write` path (never ad hoc): the
e6c roster json gates and the matching C++ pins in
`tests/support/mermaid_gap_roster.hpp` (state85=14 and
state_reverse_escape=1 lines are checked for overlap; any gate whose
report count drops is re-pinned in the same commit with the before/after
enumerated). official_matrix=48 is a body-error gate and is not expected
to move; the run confirms.

## 6. Acceptance and rollback

- Full serial OfficialMatrix once: row diff confined to state-diagram /
  state-diagram-v2 originals selected by the new ledger; no
  drawn→failure anywhere; every other family byte-identical.
- Debug/release matrix and all evidence reports raw-byte identical;
  ASan/UBSan clean on the new cohort test and OfficialMatrix.
- One independent read-only review of design (this doc), one of the
  evidence/ledger; reviewers never build/write/commit.
- Roster: the frozen state counts move down only through the roster's
  reviewed re-pin after the matrix run; never `--write` to mask.
- One local commit, not pushed. Rollback: any non-state row change, any
  drawn→failure, missing/incorrect footer note on an admitted row, or
  dual-preset divergence reverts the batch.

## 7. Out of scope (later F batches)

Other families' E2b activation (xychart/requirement/er/class/git/mindmap/
pie) is a separate batch with its own per-key table even where the values
overlap. theme/look/layout (F1/F2/F4), htmlLabels outside state, semantic
gitGraph keys, and every Track R renderer are untouched here.
