---
rfc: 31
title: Mermaid Metadata Envelope and Renderer Structural Cleanup
status: implementable
owners: "@Zzzode"
reviewers: ["agent:/root/envelope_design_review (RFC 0031 design, three review rounds, approved)", "agent:/root/envelope_feasibility_review (RFC 0031 implementation feasibility, three review rounds, approved)", "agent:/root/envelope_prr_review (RFC 0031 production readiness, approved)"]
created: 2026-10-23
last-reviewed: 2026-10-23
tracking: local RFC-0031 implementation history; user requested architecture cleanup with the shared envelope layer first
---

# RFC 0031 — Mermaid Metadata Envelope and Renderer Structural Cleanup

## Summary

Replace the Mermaid renderer's divergent metadata parsing with one bounded
envelope reader that extracts YAML frontmatter structure and
`%%{init: JSON}%%` directives while leaving per-family value validation in
place. Then admit the config-free gitGraph and stateDiagram originals, split
the 5,077-line relations implementation by family, replace the string-prefix
dispatcher with a family probe table, coarsen the hand-written Work charges,
and close the module-privacy and test-gate gaps. The envelope phase comes
first because it is the missing layer, not because it changes a diagram
grammar.

This RFC does not admit browser theme or arbitrary config in its first
rendering phase. That is a separate product decision with its own amendment to
RFC 0030; E2a admits only `title`/`description`, so the existing
whole-source rule for `theme`, `themeVariables` and init theme keys remains
intact. Every rendering change is admitted through a per-row source ledger;
the renderer's output is never the evidence for an expectation.

## Motivation

The earlier renderer review found a sound core (pure parse/layout, whole-source
fallback, width-aware FTXUI node) with structural debts. The envelope debt is
the most consequential: every official original except six begins with YAML
frontmatter, and each family re-implements a different subset of it.

### Evidence

Measured on 2026-10-23 against the pinned `mermaid@12.2.0` corpus and the
current working tree:

| Metric | Current | Target | How measured |
|---|---|---|---|
| Official originals with frontmatter | 2139 / 2145 | one structural reader | RFC 0030 Phase B inventory; per-file first-line scan |
| Owned-family originals with `description` | 1599 / 1608 | same admission, one reader | body-stripped statement scan |
| Owned-family originals with `config` | 1258 / 1608 | deferred to E2b | body-stripped statement scan |
| Owned-family originals with init directives | 118 | one JSON extraction | directive-token scan, excluding prose mentions |
| gitGraph originals drawn | 0 / 135 | ledger-derived | official matrix `wide_240` |
| stateDiagram originals drawn | 0 / 145 | ledger-derived | official matrix `wide_240` |
| gitGraph/state originals with no `config` and no init | 64 / 5 | E2a cohort | body-stripped scan (state v1 has 0) |
| Envelope reader functions | 6 + chart-local parser | 1 reader | see the migration table |
| `mermaid_relations_impl.cpp` | 5,077 lines / 140 top-level declarations | split by family | `wc -l`, declaration scan |
| Hand-written `work.spend` sites | 709 | coarse boundary charges | `grep -ro work.spend` |
| Dispatch flag references | 31 in a 95-line slice | family probe table | `mermaid_impl.cpp:2265-2360`, references to `configured_*`/`titled_*`/`multiline_directive`/`class_envelope_seen` |
| Mermaid ctest registrations | 746 | reconciled after each phase | `ctest -N` |

Per-family envelope evidence:

| Family | Originals | `description` | `config` | init | Drawn |
|---|---:|---:|---:|---:|---:|
| flowchart | 400 | 395 | 317 | 34 | 54 |
| sequence | 193 | 193 | 116 | 56 | 193 |
| class | 285 | 285 | 238 | 10 | 211 |
| ER | 206 | 206 | 192 | 0 | 62 |
| state v1 | 42 | 42 | 38 | 4 | 0 |
| state v2 | 103 | 103 | 94 | 4 | 0 |
| requirement | 172 | 170 | 154 | 0 | 11 |
| pie | 24 | 24 | 17 | 0 | 7 |
| xychart | 48 | 46 | 30 | 1 | 2 |
| gitGraph | 135 | 135 | 62 | 9 | 0 |
| **Total** | **1608** | **1599** | **1258** | **118** | **540** |

The two zero families are not zero because their body grammars are absent:
gitGraph has a 48-test semantic suite and state has dedicated state tests.
They are zero because frontmatter sources never reach those parsers. The
dispatcher keeps `header = "---"` for a frontmatter source, sets configured
flags only for the families it already knows, and falls through to
`charts_layout`, which accepts only pie/xychart. gitGraph's parser would also
reject a leading `---`; state never reaches the ER envelope branch. Correcting
this routing is part of E2a, and the general dispatcher replacement is E4.

The other debts are measured, not stylistic: 709 hand-written Work charges
with repeated prepay corrections in the Phase C record; 31 dispatch flag
references in a 95-line slice; 5,077 lines in one relations unit; and a
`detail` namespace that tests can reach despite the interface comment calling
it private.

## Goals

- G1. Provide one bounded structural envelope reader for YAML frontmatter and
  init directives, with one failure policy and one set of limits. Migrate the
  six existing envelope readers and the chart-local config parser to it; each
  family keeps its value validation and precedence. Delete the old readers
  after differential tests pass.
- G2. Admit `title`/`description` for config-free gitGraph and stateDiagram
  originals, with a pre-registered source cohort and a per-row ledger. E2a
  claims only this cohort; `config` admission is E2b and requires an explicit
  RFC 0030 amendment.
- G3. Preserve every existing drawing and every fallback row that is not
  re-derived from source. No grammar widening beyond E2a's metadata.
- G4. Split the relations implementation by family without changing the
  public factory or any rendered output; state the private declaration
  surface explicitly.
- G5. Replace the string-prefix dispatcher with an ordered family probe table
  and remove the accumulated flag cluster.
- G6. Replace the 709 hand-written Work charges with coarse boundary charges
  under the existing single cumulative one-million budget, preserving
  outcome-level behavior.
- G7. Close the module-privacy gap, split the failing capability gate from the
  regression gate, and add fuzzing for the parse entry points.

## Non-Goals

- No config, theme, `themeVariables` or init theme admission in E2a. E2b is a
  separate decision and requires a documented RFC 0030 amendment plus a
  user-visible note; it is not authorized by this draft.
- No new diagram family and no style/HTML projection; those belong to RFC 0030
  Phase D and its successors.
- No external YAML, CSS or HTML library; no new third-party dependency.
- No full YAML parser: the reader parses the structure the existing readers
  already accept, and family validators decide the values.
- No body-diagnostic result counted as an original-file drawing.
- No matrix reclassification because the renderer now draws; only source
  re-derivation through the ledger.
- No wire, session, configuration, persisted-format or public factory change.
- No speculative `bool use_ascii` API change; that is removed from scope.
- No performance claim without a measured before/after.

## Proposal

### Ownership and boundaries

The owner remains `loom.ui.visual.mermaid` in `loom_ui_visual`. The envelope
reader is a new private implementation unit, `mermaid_envelope_impl.cpp`.
It is structural: it receives source and Work, returns a typed view or
`nullopt`, and does not decide which diagram family a config value is valid
for. Per-family whitelists, value validation, precedence and body parsing stay
in their current owners.

The module already imports `loom.serdes.json` in the charts, gitGraph,
relations and sequence units, so init-directive JSON reuses that parser
without a new dependency. No shared color, theme, terminal-protocol or
configuration module is introduced. The public factory is unchanged.

### Phase E1: one bounded structural envelope reader

#### Admitted structure

YAML frontmatter is admitted only in this structural form:

- An opening `---` line, a closing `---` line, and a body after it.
- Top-level keys: `title`, `description`, `config`. Any other top-level key is
  returned as `Unsupported`; the family returns source unless it has an
  explicit reviewed use.
- `title`: one plain or quoted scalar.
- `description`: a plain scalar, a folded `>`/`>-` block, or a literal
  `|`/`|-` block. Line folding follows the reviewed rule already implemented
  by the class reader; the reader returns raw text and line structure, not a
  family-specific decoding.
- `config`: a mapping of string keys to scalars, quoted scalars or block
  scalars, at most four levels below the top level, at most 128 entries, two
  spaces per level (a family may use a different indent width only if its
  existing reader already accepts it). No lists, flow style, anchors, aliases,
  tags, multi-document markers or duplicate keys.
- Any malformed indentation, unterminated fence, unsupported YAML construct or
  duplicate key returns `nullopt`.

`%%{init: JSON}%%` directives are admitted only as strict JSON objects parsed
with `loom.serdes.json`, at most 4 KiB and 64 top-level keys each. The reader
returns every directive in source order with its byte span; the family applies
its existing precedence. Duplicate JSON keys are detected by a bounded
pre-scan if the JSON parser does not expose them; a duplicate returns source.
An init directive is admitted only before the family header for E1; inline
directives keep their family-specific handling.

The structural reader is a superset of what the six existing readers accept:
config depth four covers `config.xyChart.xAxis.showLabel` and
`config.flowchart.subGraphTitleMargin.top`; block scalars cover ER
`themeCSS: |`; quoted scalars cover gitGraph `mainBranchName: 'trunk'`.
Each family then applies its own stricter value rules unchanged. This is what
lets E1 be behavior-preserving without pretending that the families share one
value grammar.

Limits: frontmatter at most 16 KiB, title at most 1 KiB, description at most
8 KiB, init at most 4 KiB per directive and 8 KiB total, total source
unchanged at 64 KiB. Every byte read, key parsed and value copied has a
before-use Work charge.

#### Typed view

```text
Envelope {
  raw_frontmatter: string_view; // exact frontmatter span, ordered text
  frontmatter_events: vector<FrontmatterEvent>; // key, indent, line, value span
  raw_body: string_view;        // exact post-frontmatter bytes, for hashes
  effective_body: string_view;  // after leading init directives, family parser
  title: optional<RawScalar>;
  description: optional<RawScalar>;
  config: ConfigView;           // path lookup, bounded
  init: vector<InitDirective>;  // spans and raw JSON
}
ConfigStatus { Absent, Present, Unsupported }
```

`frontmatter_events` preserves key order, indentation, line numbers and value
spans. It is required: the Flow reader's acceptance is order-sensitive
(`description` before/after `config`, config-only `layout` handling), and
class/ER/sequence have related ordering constraints. Families that do not
need order ignore it; families that do validate the event stream instead of a
flattened view. The differential suite includes synthetic order mutations
(description/config swapped, duplicate keys, parent without children) in
addition to every pinned fixture and negative.

`raw_body` preserves the exact bytes used by the body-diagnostic track;
diagnostics remain separate and never count as original drawings.
`effective_body` starts at the family header after the reviewed leading init
span. Family-specific body trimming remains in the family. The reader does not
decode title/description values or decide precedence; each family keeps its
existing decoder and precedence.

#### Migration

E1 migrates only the existing readers and is behavior-preserving for every
family:

| Family | Reader replaced | Retained family logic |
|---|---|---|
| flowchart | `flow_envelope_body` | flow config whitelist; `flow_envelope_parse_prepay` stays until E5 |
| sequence | `sequence_frontmatter` | title precedence and scalar decoding |
| class | `class_source_envelope`, `class_yaml_meta` | class whitelist, folded-description rules, init consumption |
| ER | `er_source_envelope` | ER whitelist, `themeCSS` block handling |
| requirement | `requirement_envelope` | requirement strict folded-description and CR rules |
| charts | chart-local config parser | pie/XY whitelist and three-level paths |

The reader is wired family by family. Each migration keeps the old reader as a
test-only reference until the differential suite passes; the old reader is
deleted in the same commit that proves equivalence. The differential suite
runs every pinned fixture and every existing negative through both readers and
compares body offsets, accepted keys, rejection reasons and Work outcomes.
Behavior-preserving is claimed for those pinned inputs plus old-limit ±1
boundary probes. Inputs between the old and new limits are outside the
equivalence claim; any newly accepted or rejected input is recorded, and no
pinned input changes.

`flow_envelope_parse_prepay` is not an envelope reader; it is a Flow-specific
body prepay. It stays in `mermaid_impl.cpp` during E1 and is folded into the
coarse charges in E5.

### Phase E2a: config-free gitGraph/state admission

E2a admits `description` for the 69 originals that carry neither `config` nor
an init directive: 64 gitGraph (all config-free) and 5 state v2. `description`
is documentation-only and is never rendered. `title` is admitted only where
the family already projects a title (state uses the relations diagram title);
the one titled gitGraph source in the cohort is pre-registered as
predicted-blocked with a "title not projected" reason rather than silently
dropped. State v1 has no original that is both config-free and init-free (its
four config-free files are all init-bearing) and is unchanged by E2a.

E2a also includes the minimal routing correction: after envelope extraction,
the dispatcher must classify a gitGraph or stateDiagram effective body before
the charts fallback. This is a small, revertible change; E4 replaces the whole
dispatcher later. Without it, no amount of metadata admission reaches the
gitGraph or state parser.

Before implementation, E2a writes a durable pre-registration:

- `docs/dev/mermaid-e2a-cohort.md` — the human-readable table.
- `tools/mermaid/e2a-cohort.json` — path, SHA-256, prior status, prediction,
  concrete blocker and ledger row for every file.

Every one of the 69 files is classified as predicted-draw or
predicted-blocked with a concrete body/grammar/layout reason. An "unknown"
prediction is forbidden. A predicted-blocked file must cite a non-envelope
blocker; a file whose only blocker is the envelope must be predicted-draw.
The ledger is reviewed by an independent agent, not by the implementer alone.
The phase passes only when both gitGraph and state v2 have at least one
predicted-draw file, every predicted-draw file draws, zero files remain
unclassified, and no expectation changes because the renderer draws. Any
family with zero predicted-draw files fails the phase; there is no
overall-only exception. The ledger records the prior status, the metadata keys
present, the remaining non-envelope blockers and the disposition.

### Phase E2b (deferred): config admission

E2b is not authorized by this draft. Admitting `config` requires:

1. A per-family, per-key table classifying every reviewed path as projected,
   declared non-terminal or unsupported. The per-family whitelist is the only
   authority; there is no global config tier.
2. An explicit amendment to RFC 0030 D4 if any `theme`, `themeVariables` or
   init theme key is accepted. Until that amendment, those keys retain source.
3. A user-visible note on diagrams that carry accepted non-terminal config,
   not only a ledger entry. The note states which settings were not applied in
   the terminal.
4. A source-registered cohort and ledger for every affected row.

Semantic gitGraph keys (`showBranches`, `showCommitLabel`, `mainBranchName`,
`parallelCommits`, `mainBranchOrder`, `rotateCommitLabel`) are not
non-terminal: they change content, ordering, visibility or orientation. They
must be implemented as projected profiles or remain unsupported. In
particular, `showCommitLabel: false` and `showBranches: false` need their own
hidden-label/hidden-lane profiles rather than being silently ignored.

### Phase E3: split the relations implementation

The current implementation is one anonymous namespace. Splitting it by family
requires moving the shared declarations out of that namespace; the public
factory is unchanged, but the private declaration surface is not. The split
uses the existing `detail` declarations and measures the resulting interface
fan-out. E3 does not depend on the E6a pilot; if E6a later moves those
declarations into a private partition, that migration is E6a's own change and
does not alter E3's behavior gate.

New implementation units (body-only; the interface declaration surface
changes and the fan-out is measured):

- `mermaid_relations_core_impl.cpp` — existing entity storage, typed columns,
  group ownership, shared routing/box primitives (`ErCursor`, `trim_relation`,
  `er_yaml_value`, `RelationDiagram`, `parse_plain_er`).
- `mermaid_class_impl.cpp`
- `mermaid_er_impl.cpp`
- `mermaid_state_impl.cpp`
- `mermaid_requirement_impl.cpp`

No new abstraction is invented: the core unit contains only primitives already
shared by the current callers. Each unit targets at most 1,600 lines and the
graduation records actual line and declaration counts.

### Phase E4: family probe table

Replace the prefix `if`/`else` chain and its flag cluster with an ordered
probe table. The probe order must preserve the current outcomes:

1. Parse the envelope; compute `raw_body` and `effective_body`.
2. A configured or bare flow header.
3. A titled/configured sequence, including the multiline-directive prefix
   route.
4. A gitGraph effective body.
5. Class, ER, state or requirement relations family, including the
   init-before-header class case.
6. Charts as the explicit final fallback for pie/xychart.

Each probe returns `Matched(family)` or `NotThisFamily`; the charts entry is
the fallback, not a probe. The table owns the order and the ambiguity rules;
probes never parse metadata. The 31 flag references in
`mermaid_impl.cpp:2265-2360` disappear. E2a's minimal routing correction is
subsumed by this table.

### Phase E5: coarse Work charges

Replace the 709 hand-written `work.spend` calls with coarse boundary charges
under the existing single cumulative one-million budget: envelope, parse,
layout and render each charge at their boundaries (input bytes, declarations,
cells, runs) and draw from the same remaining count. Do not introduce fixed
per-phase quotas; a heavy layout phase may use budget a cheap envelope left
unused.

The acceptance criterion is outcome-level: every existing fixture and every
official original keeps its drawn/source/error classification and glyph grid.
A Work-unit difference is acceptable only if it is recorded and does not change
an outcome. The Work declaration is exported and used by tests; any structural
change updates those static assertions in the same commit.

### Phase E6: privacy, cache, gate and fuzz

E6 is split into independently revertible workstreams:

- **E6a — privacy.** The current tests are themselves module implementation
  units (`module loom.ui.visual.mermaid;`) and can reach `detail` directly; a
  partition alone does not stop them. Move those test translation units out of
  the module and make `detail` a private partition or an internal module.
  Because a non-exported partition cannot serve an independent testing module,
  declare a narrow `loom::ui::mermaid::testing` namespace in the main interface
  (or an exported testing partition) containing only the reviewed seams; the
  `loom.ui.visual.mermaid.testing` module forwards those symbols, and tests
  import the testing module. The pilot must build with `clang-scan-deps` on
  both presets. Graduation: no test TU declares the production module;
  importers of `loom.ui.visual.mermaid` cannot name `detail`; tests obtain
  seams only from `loom.ui.visual.mermaid.testing`; that namespace or partition
  is the only exported seam.
- **E6b — cache (conditional).** Measure Markdown re-render cost first. If a
  cache is justified, attach it to the message/element or a per-source LRU
  keyed by source hash and theme revision; never a process-global graph cache.
  State the relationship to the existing 256-entry token LRU. If the
  measurement shows no benefit, drop the item with a recorded rationale.
- **E6c — test gate.** Split `MermaidOfficial.CompleteCapabilityMatrix` into a
  permanently non-failing capability report (JSON/Markdown) and a green
  regression subset. The report is evidence; the regression subset is the
  gate. An expected-red gate is rejected because it keeps a known-red test in
  the gate and dilutes regressions. The ledger remains the only path for
  expectation changes.
- **E6d — fuzzing.** Add a libFuzzer harness under `tests/fuzz/` reusing the
  existing JSON/SSE harness conventions and the `ENABLE_FUZZING` gate. Seed it
  from the pinned fixtures; a 10-minute run must produce no crash, timeout or
  unbounded allocation.

### Contracts and documents affected

| Contract | Impact | Update path |
|---|---|---|
| RFC 0030 Phase D measured scope and D4 | E2a does not touch `theme`/config, so D4 is unchanged; E2b would require an amendment | E2b design, if activated |
| `docs/user/mermaid-diagrams.md` | metadata grammar, E2a admission, E2b deferred; existing sequence title, git init, XY theme and class layout statements | Per-phase documentation update |
| Official matrix JSON (`expected_per_state`, `decision_reasons`, coverage note) | envelope admission can change rows | Per-row ledger only |
| `tests/support/*authority.hpp` oracles | may encode the current frontmatter rejection | Ledger-driven review |
| `docs/decisions/design-decisions.md` | envelope ownership and the non-terminal decision (E2b) | Per-phase update |

The public factory, wire backends, session traces, `messages.jsonl`,
`dump-prompts/`, selection and copy behavior are unchanged.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| E1 | Structural envelope reader | One structural YAML/init reader; migrate six readers and the chart parser; behavior-preserving over every pinned input and old-limit ±1 probes | proposed | One reader; old readers deleted after old-vs-new differential passes on all pinned fixtures, negatives and old-limit ±1 boundary probes; every existing fixture keeps its drawn/source/error status and glyph grid; limits/negatives pass; PSS before/after and Linux dual-preset recorded; graph lint passes; 3935/746 registrations reconciled; no new dependency |
| E2a | Config-free gitGraph/state | `description` for 69 originals; `title` only where the family already projects it; minimal routing fix; pre-registered cohort and ledger | done | Durable pre-registration at `docs/dev/mermaid-e2a-cohort.md` and `tools/mermaid/e2a-cohort.json` (independent agent review approved after two adversarial rounds); both gitGraph and state v2 have predicted-draw rows; every predicted-blocked row cites a non-envelope blocker (grammar, 240-column capacity, or execution-attested native parent-route debt); 42 predicted-draw / 27 predicted-blocked, every draw empirically renders and every blocked row keeps source; zero unclassified; config/init rows keep whole source; descriptions never rendered; dual debug/release builds, family suites, full official matrix (git/state rows zero-error) and real-PTY `E2E_Pty_Mermaid` pass; ctest 3977 total / 786 Mermaid |
| E2b | Config admission (deferred) | Reviewed config whitelist, 0030 amendment, visible config note | deferred | Separate design/PRR; no obligation created by this draft |
| E3 | Relations split | Body-only core + class/ER/state/requirement units | proposed | All relation fixtures, snapshots and mutations unchanged; public factory unchanged; private declaration surface recorded; actual unit sizes and fan-out measured |
| E4 | Probe table | Ordered family probes replace the flag cluster | proposed | Relative to the post-E2a baseline, all 2145 originals and 593 diagnostics keep their classification; the 31 flag references removed; no behavior change |
| E5 | Coarse Work charges | Boundary charges under the single 1M budget | done (macOS) | Outcome-level matrix and glyph conservation unchanged; budget exhaustion still returns source; 1M total and caps unchanged; Work shape/kWorkLimit/static asserts unchanged (no "updated" needed); Work deltas recorded below |
| E6a | Privacy | Private partition/internal module + testing seam; tests moved out | done (macOS) | No test TU declares the production module; public importers cannot name `detail`; only reviewed seam exported; both presets and `clang-scan-deps` pass |
| E6b | Cache (conditional) | Measure first; per-message cache only if justified | proposed | Measured benefit recorded; theme/source invalidation; no process-global cache; otherwise dropped with rationale |
| E6c | Test gate | Capability report vs green regression subset | done (macOS) | Gating set named (`ctest -L mermaid`, 810 tests; strict subset `-E '^MermaidCapabilityReport\.'`, 798); 12 capability reports permanently non-failing with frozen `EXPECT_EQ` pins and a reviewed SHA/size-pinned `mandatory_gaps` roster diffed both directions; green regression subset passes both presets; ledger remains the only expectation path |
| E6d | Fuzz | libFuzzer harness and seed corpus | done | New `fuzz` preset (global fuzzer-no-link+asan+ubsan; per-target compile option removed because it broke clang-23 synthetic BMI builds), `fuzz_mermaid_parse` reaches the structural envelope plus every family parser/layout and full dispatch, seeded from 14 edge files + 1607 owned-family originals. Smoke (500/1621 seeds) and a 600 s run (217,451 executions, 60,771 features, peak RSS 637 MiB) exit clean with no crash/timeout/artifact; harness is a module implementation unit using `import :detail` + the testing facade |

## Production Readiness Review

PRR for the `implementable` gate. Every row is answered. Rows that require the
Linux dev box or the first implementation are explicit phase gates; nothing is
recorded as measured before it is.

### 1. Correctness and tests

- [x] Every new behaviour has a named test. E1 adds a structural envelope
  suite (plain/folded/literal/quoted scalars, three- and four-level config,
  duplicate and unknown keys, init JSON, bounds, raw/effective bodies) plus an
  old-vs-new differential over every pinned fixture and negative. E2a adds the
  pre-registered 69-file cohort, predicted-draw cases and the ledger. E3-E5 add
  fixture, snapshot and mutation conservation.
- [x] Failure paths are covered: empty source, malformed frontmatter,
  unsupported YAML, oversized input, duplicate JSON keys, exhausted budget,
  unresolved family and the existing whole-source fallbacks.
- [x] Golden suites assessed. Mermaid matrix, terminal snapshots and Markdown
  goldens are the affected suites; `test_ui_*`, `test_dialog_*`,
  `test_prompt_dialog` and `test_cost_threshold_dialog` are unaffected because
  no dialog, prompt or palette contract changes. Any changed Mermaid snapshot
  is re-derived through the ledger, never blindly refreshed.
- [x] The known timing flake list is respected; no new wall-clock assertion or
  tight timeout is added. Matrix checks remain deterministic.
- [x] Expected serial ctest total is stated: 3935 registered tests in the
  debug build on 2026-10-23, of which 746 are Mermaid. New tests are added per
  phase; no baseline test is deleted. The phase record reconciles additions and
  any deletion exactly.
- [x] String/shape-based couplings are listed in the contracts table:
  frontmatter acceptance, matrix `expected_per_state`/`decision_reasons`, the
  authority oracles under `tests/support/`, and the user guide. The ledger is
  the only expectation-change path; emitter and consumer sides change together.

**Notes:** the old-vs-new differential is the correctness gate for E1. It
compares body offsets, accepted keys, rejection reasons and Work outcomes
before the old readers are deleted.

### 2. Build system and module discipline

- [ ] Affected producer-TU BMI PSS measured before and after. This cannot be
  measured on the macOS host; `tools/arch/measure_bmi.py` requires Linux. E1
  records the before/after PSS on the Linux dev box before the phase
  graduates. This is the only open build row.
- [x] No Ninja concurrency reduction; memory is handled by TU splitting, not
  by throttling.
- [x] Interface files gain declarations, not definitions; the new envelope and
  testing-seam declarations are out-of-line in implementation units. The
  god-interface inline-body count does not increase.
- [x] No new textual standard-library or third-party includes. The new unit
  uses `import std;` and the already-imported `loom.serdes.json`.
- [x] Named partitions are used only for module-internal privacy (E6a), not
  for cross-area layering. The testing namespace/partition is a reviewed test
  seam, not an area boundary.
- [x] No new upward dependency edges. E1 reuses the existing
  `visual → foundation` and `visual → utils` edges; E6a's testing module is in
  the same target. `tools/arch/graph_check.py` and the upward-edge baseline are
  rerun at E1 and E6a.

### 3. Rollback and compatibility

- [x] Commits are atomic per phase relative to the frozen
  `tools/mermaid/rfc0031-baseline.json`; E1, E2a, E2b, E3, E4, E5 and
  E6a-E6d are independently revertible.
- [x] No flag-day interface change. The public factory is unchanged; the
  private `detail` migration is test-only and keeps a reviewed seam.
- [x] Persisted data is untouched: sessions, settings cascade, config schema
  and debug traces keep their shapes.
- [x] Wire-protocol compatibility is untouched: no request/response field is
  added or removed.

### 4. Observability

- [x] Session traces remain valid: `messages.jsonl` block coverage and
  `dump-prompts/<id>.jsonl` are not touched.
- [x] No background worker, thread or constant-rate render ticker is added;
  the renderer remains event-driven.
- [x] No new ad-hoc print path. The ledger and capability report are
  build-directory artifacts; any diagnostic uses the existing channels.
- [ ] Build/performance metrics from this PRR are recorded after the phases.
  E1 records PSS/fan-out; E5 records Work deltas; E6b records cache
  measurements; E6d records fuzz results. This is a phase evidence gate, not a
  design gap.

### 5. Documentation and deletion

- [x] `CLAUDE.md` is updated only if E6a changes the build/module conventions;
  otherwise it is untouched.
- [x] Non-obvious decisions go to `docs/decisions/design-decisions.md`:
  envelope ownership, the deferred config tier and the privacy seam.
- [x] Dead code is deleted in the same phase: six old envelope readers and the
  chart-local parser in E1, the dispatch flag cluster in E4. Prefer deletion
  over keeping both paths.
- [x] No deprecated module or persisted shape is introduced; there is no
  silent shape drift.
- [x] All code, comments and documentation remain in English.

### 6. Platform readiness (macOS / Linux)

- [ ] Debug and release `-Werror` builds on the Linux dev box. macOS debug and
  release presets built clean on 2026-10-23, and `graph_check.py --target-ui9`
  passed (12 singleton areas, 0 SCC-internal/back directions). The Linux run is
  an E1 gate before code lands.
- [x] macOS CI is N/A. Project directive 2026-09-29 makes GitHub CI a
  non-gate; local macOS dual-preset verification is the signal. CI is not
  waited on or monitored.
- [x] Termios, signals and Apple-only guards are N/A: no platform, terminal
  driver or signal code is touched.
- [x] Existing truecolor goldens are unchanged; RFC 0031 changes no color,
  style, theme or ANSI rendering for existing drawings. E2a adds new
  gitGraph/state ANSI outputs that are additive and reviewed through the
  ledger.
- [x] The offline dependency cache is unaffected; no new dependency or network
  fetch is introduced.

**Notes:** the two open platform/build rows (PSS and the Linux dual-preset run)
are execution gates for E1, not unresolved design questions. The macOS host
cannot provide PSS; this limitation is recorded rather than bypassed.

### Review sign-off

| Role | Reviewer | Date | Verdict |
|---|---|---|---|
| Design | agent:/root/envelope_design_review | 2026-10-23 | approve |
| Production readiness | agent:/root/envelope_prr_review | 2026-10-23 | approve |
| Code (per phase) | — | — | — |

## Rollout and rollback

Order: E1 → E2a → E4 → E5 → E6a/E6c/E6d; E3 may run after E1 in parallel with
E2a; E2b is deferred. E4 depends on E2a's minimal routing fix; the earlier
draft's claim that E2 and E4 were independent was wrong.

### Baseline freeze (E1 prerequisite)

Before any E1 code, freeze the RFC 0030 working-tree baseline: record
`git rev-parse HEAD` (`b8286503`), a `git stash create` tree hash or a complete
`git status --porcelain` plus per-file SHA-256 manifest, and the current
3935-total / 746-Mermaid ctest registration. Store the record at
`tools/mermaid/rfc0031-baseline.json`. If the owner authorizes a commit, the
baseline commit is preferred. E1 must not mix the uncommitted RFC 0030 work
with E1 changes; no E1 commit lands before the baseline is recorded.

The record was produced on 2026-10-23: HEAD `b8286503`, 11,368 manifest
entries, 3935/746 ctest. Verify it with
`tools/mermaid/verify_rfc0031_baseline.py`. The Linux PSS procedure is
`tools/mermaid/rfc0031-pss-runbook.md`; the current macOS host cannot measure
PSS and has no saved SSH target, so the before/after files remain an E1 gate.

Rollback boundaries:

- E1: revert `mermaid_envelope_impl.cpp`, its declarations and the migrated
  call sites. The old readers are deleted only after the differential gate;
  the commit that deletes them is separately revertible.
- E2a: revert the metadata admission, the minimal routing fix and the cohort
  ledger rows. Other families are unaffected; E4 is not yet in place.
- E3: restore the single relations unit; the public factory never changed.
- E4: restore the prefix dispatcher; probes are additive otherwise.
- E5: restore the per-site Work charges; outcomes are the gate.
- E6a-E6d: revert independently; no user settings are involved.

## Drawbacks

- The structural reader is a new single point of failure for every family.
  Differential tests and per-family migration commits bound the risk.
- E2a intentionally leaves 71 gitGraph and 140 state originals with config or
  init outside the cohort. The coverage gain is real but bounded; E2b needs
  its own product decision and 0030 amendment.
- Splitting relations increases translation units and the chance of duplicated
  helpers; the core unit and a no-new-abstraction rule bound that risk.
- Coarse Work charges may change the exact budget at which a pathological
  source falls back. Outcome conservation is measurable; per-source Work
  equality is not promised.
- The privacy change is a test refactor with a new testing module; it adds a
  module and a migration cost but makes the boundary real.

## Alternatives considered

- **Keep the six readers.** Smallest immediate diff, but the divergence is
  the root cause of gitGraph/state zero coverage and repeated prepay
  corrections. Rejected as the status quo.
- **Extend each reader in place, no shared reader.** Avoids a new unit but
  preserves six divergence points and the metadata bugs that follow. Rejected
  for the same reason as the status quo.
- **Use a full YAML library.** Correct for arbitrary YAML, but adds a
  dependency, accepts anchors/aliases/lists the renderer must not interpret,
  and expands the attack surface. Rejected.
- **Strip metadata and count body diagnostics as originals.** Would inflate
  coverage while hiding unsupported semantics; RFC 0030 forbids it. Rejected.
- **Accept all config as a no-op.** Maximizes admission but silently drops
  layout, visibility and orientation semantics. Rejected; E2b requires a
  per-key projected/unsupported decision and a user-visible note.
- **Accept theme with a visible marker now.** Would unlock more state files,
  but it changes RFC 0030 D4 without an amendment and adds a user-visible
  caveat to every themed diagram. Rejected for E2a and left as the E2b
  decision.
- **Big-bang rewrite.** The existing core is sound; a rewrite would discard
  the fixture/oracle investment. Rejected in favor of independently revertible
  phases.
- **Fixed per-phase Work quotas.** Simpler to describe but can make a heavy
  layout phase fall back earlier. Rejected in favor of coarse cumulative
  charges.

## Testing and verification plan

- Structural envelope tests: plain/folded/literal/quoted scalars, three- and
  four-level config, duplicate keys, unknown keys, anchors/lists, malformed
  indentation, unterminated fence, oversized input, malformed init JSON,
  duplicate JSON keys, raw/effective body offsets, and synthetic order
  mutations (description/config swapped, duplicate keys, parent without
  children).
- Old-vs-new differential: every pinned fixture and every existing negative
  through both readers; compare accepted keys, rejection reasons, body offsets
  and Work outcomes.
- Per-family migration tests: every existing fixture keeps its status, glyphs
  and semantic assertions.
- E2a cohort: the 69-file pre-registration, predicted-draw evidence, ledger
  and zero-unclassified check.
- Structural tests: reader count, old functions absent, probe order, relation
  unit ownership, Work charge boundaries.
- Dual preset `-Werror` builds and serial ctest; 746 Mermaid registrations
  reconciled after each phase.
- Real PTY: config-free gitGraph/state admission, width fallback/restoration,
  copy and selection, no control-sequence injection.
- Fuzz: 10-minute libFuzzer run per harness, no crash/timeout/unbounded
  allocation.

## Documentation impact

- [ ] `docs/user/mermaid-diagrams.md`: metadata grammar, E2a config-free
  admission, E2b deferred, existing title/git/XY/class statements reconciled.
- [ ] `docs/dev/mermaid-conformance.md`: ledger and capability-report split.
- [ ] `docs/dev/mermaid-e2a-cohort.md` and `tools/mermaid/e2a-cohort.json`:
  the E2a pre-registration.
- [ ] `tools/mermaid/rfc0031-baseline.json`: the frozen E1 baseline record.
- [ ] `tools/mermaid/verify_rfc0031_baseline.py` and
  `tools/mermaid/rfc0031-pss-runbook.md`: baseline verification and the Linux
  PSS procedure.
- [ ] `docs/decisions/design-decisions.md`: envelope ownership and the
  non-terminal config decision (E2b).
- [ ] `CLAUDE.md`: only if the build or module conventions change (E6a).
- [ ] `docs/README.md`: RFC index.

## Open questions

| Question | Owner | Resolved by |
|---|---|---|
| What is the exact per-family config whitelist (projected / non-terminal / unsupported), and does any theme key require a 0030 amendment? | agent design review | E2b design, if activated; E2a does not depend on it |
| Which seam mechanism should E6a use: an exported `testing` namespace in the main interface or an exported testing partition, with `detail` in a private partition or internal module? | agent build review | E6a two-preset pilot |
| Is a Markdown layout cache justified by measurement, or should it be dropped? | agent performance review | E6b measurements |
| Which gitGraph/state bodies in the 69-file cohort have non-envelope blockers? | agent source review | E2a pre-registration before implementation |

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-23 | — | Provisional draft created | Local, uncommitted | Measured 2139/2145 frontmatter originals, 1599/1608 owned descriptions, 1258/1608 config, 118 init, git 0/135 and state 0/145; six readers plus the chart parser, 5,077-line relations unit, 709 Work sites, 31 dispatch flag references, 746 Mermaid tests. No code, expectation or configuration change. |
| 2026-10-23 | — | Two independent reviews requested changes; draft revised | Local, uncommitted | Corrected the root cause (frontmatter sources fall through to charts, not the ER envelope), narrowed E2a to the 69 config-free originals, deferred config/theme to E2b, expanded the structural grammar and raw/effective body contract, retained the Flow prepay, split E6, changed E5 to coarse cumulative charges, and added the contract/ledger and pre-registration requirements. Re-review pending. |
| 2026-10-23 | — | Second re-review requested two majors; draft corrected | Local, uncommitted | E2a now admits `description` and only family-projected titles; the one titled gitGraph cohort source is pre-registered blocked rather than silently dropped. `Envelope` carries ordered `frontmatter_events` and synthetic order negatives. E3 uses the existing `detail` declarations and no longer depends on the E6a pilot. E6a defines an exported testing namespace/partition because a non-exported partition cannot serve a separate testing module. Re-review pending. |
| 2026-10-23 | — | Both independent reviews approved the revised draft | Local, uncommitted | Design and implementation-feasibility verdicts APPROVED after three rounds; no blocking or major findings remain. The RFC stayed provisional pending the gate update. No code, expectation or configuration change. |
| 2026-10-23 | — | Accepted after independent design review | Local, uncommitted | Design and feasibility verdicts APPROVED; summary, motivation, evidence, goals, non-goals, proposal, alternatives and open questions with owners/resolutions are complete. Status moved provisional → accepted. PRR is next; no implementation, drawing or expectation credit. |
| 2026-10-23 | — | PRR approved; promoted to implementable | Local, uncommitted | Independent PRR review approved after four majors were closed: E6c resolved to a permanently non-failing capability report with a green regression gate, E2a made mechanically pass/fail with a durable pre-registration, the E1 baseline freeze and rollback boundary defined, and macOS debug/release plus graph-check results recorded. PSS and Linux dual-preset remain E1 execution gates. No code or expectation change. |
| 2026-10-23 | E1 | Pre-implementation baseline frozen; Linux PSS blocked | Local, uncommitted | `tools/mermaid/rfc0031-baseline.json` records HEAD b8286503, 11,368 modified/untracked file hashes, the stash tree and 3935/746 ctest. Verifier `tools/mermaid/verify_rfc0031_baseline.py` reports 0 mismatches. The macOS host has no `/proc` PSS and no saved SSH target; the before/after PSS commands are in `tools/mermaid/rfc0031-pss-runbook.md`. E1 production code was deliberately not started so the before-PSS tree remains measurable. No code, expectation or configuration change. |
| 2026-10-24 | E1 | Shared envelope implemented; all six readers migrated (macOS) | Local, uncommitted | New `mermaid_envelope_impl.cpp` structural reader (bounded lines, ordered events, raw/effective body, depth-4/128-entry config, block scalars, leading-init directives with strict end-of-line tails). Flow, sequence, requirement, class, ER and charts migrated; family value whitelists retained verbatim (notably ER `themeCSS: |` Text decoding and the charts sibling-indentation/empty-map rules). Debug and release `-Werror` clean. Tests: new envelope unit 22/22; terminal corpus 4/4; charts 127/128 (only pre-existing `CompleteMermanXY102Batch` pending-profiles gate); relations, flow, sequence, gitgraph and classic-shapes suites show exactly the pre-existing baseline failures (relations 9, flow `Official400` gate); `MermaidOfficial.CompleteCapabilityMatrix` actuals unchanged at flow 54, class 211, ER 62, requirement 11, pie 7. Full debug ctest adds only the 8 pre-existing `StreamingMigration` macOS `<tmpdir>` golden mismatches (zero Mermaid references). Equivalence was proven through the pinned semantic suites and their negative matrices; the old per-family readers were removed in the same change. Linux PSS remains outstanding and is the E1 graduation blocker; no commit yet. |
| 2026-10-24 | E2a | Config-free gitGraph/state admission implemented (macOS) | Local, uncommitted | Minimal dispatcher routing correction; gitGraph admits a description-only frontmatter (`git_frontmatter_body`) and stateDiagram-v2 admits description plus a projected title (`state_source_envelope`, routed before the ER envelope branch). Durable ledger `tools/mermaid/e2a-cohort.json` + generator + `docs/dev/mermaid-e2a-cohort.md`; two independent adversarial ledger reviews (selection re-derivation, draw-row grammar audit, detector mechanics) produced the final 42 predicted-draw / 27 predicted-blocked partition — notably identifying 3 empty-quoted-tag, 4 nine-branch and 16 parse-accepts-but-unroutable (`plan_parent_routes`) git rows. New durable `test_mermaid_e2a_cohort` gate (5 tests, public API), 7 MermaidGitGraphEnvelope and 6 MermaidStateEnvelope module tests; PTY E2E extended with gitGraph+state turns. Debug and release -Werror clean; cohort 5/5, gitgraph 55/55, state additions green, all other family suites at the pre-existing baseline; CompleteCapabilityMatrix git/state rows zero-error (its remaining failures are the standing family mandatory gates); real-PTY E2E_Pty_Mermaid passes. ctest 3977 total / 786 Mermaid. No public interface or wire/session contract changed. |
| 2026-10-24 | E4 | Dispatch flag cluster replaced by ordered probe table (macOS) | Local, uncommitted | Added detail::MermaidProbeFamily/Decision/classify_mermaid (declarations out-of-line in mermaid.cppm, bodies in mermaid_impl.cpp): one textual scan reproduces the legacy header/peek/multiline-route logic byte-for-byte (including route-overwrite-not-OR semantics and all fixed charges), then ordered probes Flow>Sequence>GitGraph>Relations (Class/Er/State/Requirement) with Charts terminal; the execution switch spends identical prepay amounts. Legacy configured_*/titled_sequence/multiline_directive flags removed from the execution path (only scan-local names remain). New test_mermaid_probe TU with 11 invariant tests covering bare/configured headers, multiline directive class-vs-other, leading-init class envelope, exact/starts_with relations prefixes, E2a git/state routes, malformed-envelope textual degradation and route budget exhaustion. Debug+release -Werror; Mermaid ctest 782 shows only the 12 standing mandatory gates (PTY passes isolated); no classification change on the full official/terminal corpora. |
| 2026-10-24 | E3 | Relations unit split into core + 4 family implementation units; private declaration surface added to mermaid.cppm (macOS) | Local, uncommitted | Pure code motion, zero behavior change. `mermaid_relations_impl.cpp` (5,131 lines) split into `mermaid_relations_core_impl.cpp` 3,209 / `mermaid_class_impl.cpp` 1,091 / `mermaid_requirement_impl.cpp` 492 / `mermaid_er_impl.cpp` 210 / `mermaid_state_impl.cpp` 74. Core exceeds the RFC 1,600-line target (~3.2k): the genuinely shared code alone is that large (RelationLayout ~860, parse_relations ~530, type cluster ~190, ErCursor+plain-ER parser cluster ~710); the RFC-literal five units were landed and no sixth unit invented (spec3 risk decision). mermaid.cppm 174 → 387 lines: one non-exported `detail` relations block with 9 enums, 31 structs + 1 alias, 42 free-function prototypes and 28 member-function prototypes, zero brace bodies (inline-def ratchet unchanged at OK). RelationDiagram/RelationLayout/CompoundScopeRoute/RouteSearch/RelationBox inline bodies converted to out-of-line text-identical definitions; six default arguments live in the cppm only (identifier, class_identifier, class_display_label, paint_path, paint_port, label_place); file-local symbols (ErCursor, parse_plain_er cluster, PendingClassNote, parse_relations, connect_relation, route_scope, ClassMeta, requirement model tail) stay in per-TU anonymous namespaces. One roster refinement vs spec3 line ranges: `er_description_text` is shared (the E2a core `state_source_envelope` calls it as well as the ER unit), so it is defined in core at detail scope and declared in the cppm; body byte-identical. GMF/imports per spec: core+class GMF `<ftxui/screen/string.hpp>`; class+er `import loom.serdes.json;`; state+requirement `import std;` only. Motion fidelity machine-verified: 524 `spend(` / 418 `sizeof(` occurrences conserved across the five files; every non-scaffold line matches the original in source order (35 enumerated header/default-arg transforms, 123 type/structural lines relocated to the cppm). mermaid.pcm consumers 9 → 13 (debug and release CXX.dd). Debug and release `-Werror` clean; test_mermaid_relations_semantics 209/218 with exactly the same 9 pre-existing baseline failures before and after at every intra-phase step (0,1,2,3,4); e2a cohort 5/5, envelope 22/22, gitgraph 55/55, official only CompleteCapabilityMatrix, charts only CompleteMermanXY102Batch, flow only Official400 gate, sequence 264/264, classic shapes 5/5 on both presets (release relations + e2a explicitly re-run). `inline_def_check.py` OK; `graph_check.py --target-ui9` 12 singleton areas, 0 back edges. Dispatcher `mermaid_impl.cpp`, tests/fixtures and tests/support untouched. Linux dual-preset/PSS remain the outstanding execution gate per the E1 runbook; no commit yet. |
| 2026-10-24 | E5 | Coarse Work boundary charges under the unchanged 1M budget (macOS) | Local, uncommitted | Fine-grained Work.spend scanner/allocation-prepay bookkeeping consolidated into named per-unit boundary fees; the single cumulative 1,000,000 budget, `Work` struct, `Work::spend`, kWorkLimit and all caps are byte-for-byte unchanged (gitgraph_semantics static asserts still pass unedited). Envelope: two mandated sites (entry 3072+3/S before the line vector grows; frontmatter 1024+8/F charged only on the has_frontmatter path, with the 16384 init cap paid only when a `%%{` directive is present), replacing 8 sites — `copy_string`/`collect_block` became plain copies, every grammar-rejection boolean preserved. Gitgraph: parse fee 12/S+65536 before commit/branch reserve, table reserve kept, layout Router per-pop/per-neighbor/per-path-reconstruction, canvas cell fees and area fees all kept (17→10 sites). Charts: entry 32768+2/S at chart_source_lines, pie rows boundary extended with slices²·8 for the duplicate scan, existing count/area boundaries kept (51→31). Sequence: measured parse entry fee (one allocation-free pre-pass; comment/frontmatter/`@{}`-role lines excluded since those keep their own retained profile fees; 2/content-byte + 128/statement + 176/colon-tail-byte capped 512/line) and a frontmatter 4096+12/F helper fee; layout declaration fee 3·Σlabel_bytes+200·counts; canvas/title fees kept (118→71). Flow: flow_envelope_parse_prepay generalized into two-profile flow_parse_prepay — full formula verbatim for configured/envelope/reopen, bare profile (60/S + identifier unit + 64·128·34 + 4096) charged on first valid header; first bare group reopen pays only the full-minus-bare delta (never both); node_size classic/decision fees folded into the collector literal_bytes term; all Router per-pop/neighbor and Canvas per-cell sites and flow_layout 4·area, route, rank and edge boundary fees kept (57→32). Dispatch (E4 table): seven probe spends/prefix_work folded into one classify fee (192 fixed; +600+1/peek-byte only when a frontmatter block or multiline directive is actually walked) — bare-header families pay no per-byte term, preserving their exact pre-family budget; MermaidProbeDecision.prepay is retained (and always zero) so the cppm seam and tests are untouched. Every multiplication keeps an explicit pre-spend overflow guard. ErCursor (63 sites in the split core) and all five relations units were audited and deliberately left untouched (documented exception: dense grammar, per-operand prepays :745/:798/:885/:886/:905/:909/:917/:1175 and the 1408/S class activation, 64/S state, 64/S+16384 requirement entry fees are exactly the kind of sub-construct boundaries the safety rules say to keep; per-site removal there was not provably outcome-preserving in this slot). New tests in the existing test_mermaid_envelope module TU (no new binary): MermaidEnvelope.WorkBudgetSplitPhase (entry-vs-frontmatter two-site split) and MermaidEnvelope.WorkBudgetSharedAcrossPhases (cumulative parse→layout threshold via flow_ascii_rows); the two E4 test_mermaid_probe assertions that pinned the deleted per-family prepay accumulator (`EXPECT_GT(prepay,0)`) were moved to the new E5 contract (`EXPECT_EQ(prepay,0)`) — production family classification unchanged, all 11 probe tests green. Debug+release -Werror clean; the pinned failure set is byte-identical to the pre-E5 baseline on both presets (MermaidOfficial.CompleteCapabilityMatrix 1, Official400CompleteSourceNativeMandatoryGate 1, CompleteMermanXY102Batch 1, nine relations gates); matrix actuals unchanged (flow 54 / sequence 193 / class 211 / ER 62 / requirement 11 / pie 7 / xychart 2); MarkdownMermaid.CumulativeCanvasWorkBudgetFallsBackForOtherwiseAdmittedChain (8,20 draws; 64,100 source) and ResourceLimitsPreserveSourceWithoutPartialGraph stay source; WorkBudgetExhaustionRejected still rejects; envelope suite 24/24. Site counts (grep -c '.spend(' per file): envelope 8→2, gitgraph 17→10, charts 51→31, sequence 118→71, flow mermaid_impl 57→32, relations five units unchanged (core 351, class 105, requirement 34, er 8, state 9), cells 5; total 763→658 textual occurrences. Linux dual-preset/PSS remain the outstanding execution gate from E1. |
| 2026-10-24 | E6a | Detail hidden in a non-exported implementation partition; one reviewed testing seam behind a facade; tests detached from the production module (macOS) | Local, uncommitted | New layout per the verified clang-23 mechanics: `src/ui/visual/mermaid_detail.cppm` declares the non-exported partition `module loom.ui.visual.mermaid:detail;` (explicit `import loom.ui.visual.mermaid;` plus non-exported using-aliases of every seam entity) and now owns kSourceLimit/kWidthLimit/kHeightLimit/kLabelLimit/kGroupLimit/kGroupDepthLimit, unicode_text/validated_plain_label, the four *_layout decls, Point/glyph primitives, Cell/Canvas and the full E3 relations private declaration block; `mermaid.cppm` keeps the GMF + the two exported render_mermaid overloads and adds ONLY the reviewed `export namespace loom::ui::mermaid::testing` seam (kWorkLimit=1'000'000, Work, Envelope cluster + parse_envelope, GitGraph cluster + parse_gitgraph, FlowAsciiRows + flow_ascii_rows; declarations/data initializers, no brace bodies). New facade `mermaid_testing.cppm` = `export module loom.ui.visual.mermaid.testing;` + solely the wide `export import loom.ui.visual.mermaid;` (narrow export-using cannot transitively expose complete types on clang 23). New PRIVATE plain impl unit `mermaid_testing_seams.cpp` defines thin forwarders testing::{parse_envelope,parse_gitgraph,flow_ascii_rows} -> detail counterparts. All 11 production impl units got the short `import :detail;` (dotted form parse-fails); Work::spend moved to namespace testing in mermaid_cells_impl.cpp while Canvas/Cell stay detail and bind through the alias; internal call sites of the four seam entry points were qualified `detail::` because moving Work into testing made unqualified lookup ADL-ambiguous between detail:: and testing:: (the detail partition declares the distinct detail parser functions the wrappers forward to). Five test TUs (envelope, flow_ascii, gitgraph_semantics, gitgraph_upstream, and the E4 probe TU) were converted to ordinary importers of `loom.ui.visual.mermaid.testing` (textual gtest/ftxui includes retained like test_mermaid_official.cpp); using/namespace aliases (private_flow/original_git/native_git) and the two static_asserts (now the seam-shape pin testing::kWorkLimit / testing::Work::remaining) rewritten; every test kept its own local Work replica; zero TEST macros added/removed (24/2/11/44/11 unchanged). The E4 probe cluster (MermaidProbeFamily/MermaidProbeDecision/classify_mermaid) was added to the reviewed seam so test_mermaid_probe could detach — the one deviation from the E6a spec's enumerated seam list, which predates that TU; it is type-cluster-wide like the others and exposes no Canvas/layout surface. New lint tools/arch/test_mermaid_privacy.py (not wired into ctest; run with graph_check/inline_def_check): fails on any tests/ line `module loom.ui.visual.mermaid;`, direct `import loom.ui.visual.mermaid;`, or `mermaid::detail` reference, and pins the non-exported partition decl + facade export-import; it exits 0. Evidence: debug and release configure+build clean under -Wall -Wextra -Wpedantic -Werror with clang-scan-deps ordering primary -> :detail -> impls -> facade; grep proves zero production-module declarations, zero direct primary imports, zero mermaid::detail refs under tests/; inline_def_check.py OK (mermaid absent from the shrink list), graph_check.py --target-ui9 PASS (12 singleton areas). Negative probes against the real debug BMI with CMake-exact flags: external `import loom.ui.visual.mermaid;` naming `detail::Canvas` fails "no member named 'detail' in namespace 'loom::ui::mermaid'"; `import loom.ui.visual.mermaid:detail;` fails "import directive must end with a ';'"; positive controls (facade importer instantiating Work and calling parse_envelope; direct primary importer reading testing::kWorkLimit) compile — wide-export-visibility of testing to direct importers is the documented convention/lint half. Test totals: ctest -R Mermaid = 795 registrations on both presets; debug 12 failed / release 12 failed within the Mermaid regex suites, byte-identical to the pre-E6a standing gates (MermaidOfficial.CompleteCapabilityMatrix; MermaidFlowSemantics.Official400CompleteSourceNativeMandatoryGate; MermaidChartsSemantics.CompleteMermanXY102Batch; nine relations gates), zero new failures; detached binaries green: test_mermaid_envelope 24/24, test_mermaid_probe 11/11, test_mermaid_gitgraph_semantics 55/55 (incl. TermaidGitGraph*), test_mermaid_flow_semantics 60 pass + only Official400 gate, relations 209 pass + 9 gates, charts 127 + 1 gate, sequence 264, e2a_cohort 5 on both presets. The release E2E_Pty_Mermaid PTY test (matched by the -R Mermaid regex but not a renderer suite) fails on `/clear` frame timing; verified pre-existing by stashing E6a and rebuilding release at HEAD (fails 3/3 there; passes on debug) — environment/timing, unrelated to module structure. Linux dual-preset clang-scan-deps remains the outstanding RFC graduation gate from E1. |
| 2026-10-24 | E6c | 12 mandatory gates converted to permanently non-failing capability reports; strict green subset named via ctest LABELS (macOS) | Local, uncommitted | Test/tool/docs only; no production renderer code under src/ui/ changed (plus LABELS in tests/ui/CMakeLists.txt and one LABELS line in tests/e2e/CMakeLists.txt). The 12 red gates were renamed to MermaidCapabilityReport.{OfficialMatrix,Flow400CompleteSources,MermanXY102,Class481,Class285,ClassEndpoint29,State85,ClassCaptionStarvation8,StateReverseEscape,ClassNotes18,Requirement11P1P6,Requirement11Overflow} (12 renames, zero add/delete of those gates); enumeration, provenance, resource/integrity checks and JSON/MD report emission are unchanged. Aspiration assertions removed; frozen observed counts became explicit EXPECT_EQ pins (ER errors 6/pending 144/admitted 62; Flow errors 9/decoded 54/pending 346; Class errors 30/decoded 237/pending 48/matched 211; Class481 matched 341/pending 137; Endpoint29 matched 26; State85 matched 71/geometry_errors 1/positive_pending 7/original_policy 1/choice_policy 1; Caption 6/2; Reverse 8/1; Notes18 strong 16; XY102 102/85/17, geometry 77, empty 5, profile pending 20, partition 3,17,82,0, zero error counts; P1 complete 8/geometry 1; Overflow complete 1/fallbacks 1/errors 1); all other zero-error/resource pins kept hard. Each report serializes a canonical mandatory_gaps array (matrix family summaries gained target_whole_sources_admitted/mandatory_remaining; Flow report gained complete_source_target/mandatory_pending_remains and is counts-only with no roster because pending membership is Model-authority determined). New textual support header tests/support/mermaid_gap_roster.hpp (import std/loom.serdes.json/loom.crypto.crypto; included only by the three ordinary-importer TUs, never the flow attached module TU) loads tools/mermaid/e6c-mandatory-gap-roster.json (schema e6c_mandatory_gap_roster/v1, 61620 bytes, sha256 29611b1dae04a3c5b30d14fa904ece3be6ab37d924ae6621079e7633afcd6f1d), verifies size/SHA/schema/gate keys, canonicalizes rows and diffs both directions; the six-message matrix error mapper returns 'unknown' on anything outside the closed set and unknown stays ADD_FAILURE. Roster bootstrapped once from debug report mandatory_gaps arrays (356 rows across 11 keys: official_matrix 94, merman_xy102 20, class481 140, class285 74, class_endpoint29 3, state85 14, class_caption_starvation8 2, state_reverse_escape 1, class_notes18 6, requirement11_p1p6 1, requirement11_overflow 1), then independently audited row by row by re-deriving every row from report ground truth (raw errors/outcomes) and checking the spec-named ids (class481/endpoint 5974d9ab/a254b69/c50650c; state 2419c8d2/17f03f94; caption, reverse b91, notes ordinals 272-277, P1, overflow case 0) and distribution (94 = 47 flowchart body_diagnostic + 30 class original + 9 flowchart original + 6 ER + 1 conf + 1 requirement); marked reviewed in-file. class35_errors gained bool terminal_failures=true; only the six reporter call sites pass false; every green caller keeps default behavior. Requirement catch blocks roster only the geometry/verification outcomes named in the spec and keep ADD_FAILURE for resource/protocol/semantic/mutation/lifecycle and unexpected throws. New MermaidCapabilityRoster.{Integrity,UnknownMatrixErrorCategoryIsRejected} guard the ledger. tools/mermaid/verify_e6c_gap_roster.py (python3 -I) diffs serialized gaps both ways and pins size/SHA, with maintenance-only --write; it reports OK for debug and release and fails on tampering in both directions. All 11 tests/ui Mermaid gtest blocks and E2E_Pty_Mermaid carry LABELS mermaid (the E1 spec named nine blocks; test_mermaid_probe from E4 and test_mermaid_e2a_cohort from E2a were added later and included so -L mermaid is the complete gate): 810 registered on both presets (807 post-E2a plus the two new roster tests and the renamed gate), 798-test strict subset. Verification: debug ctest -L mermaid -j8 = 809/810 and strict subset 798/798; release 809/810 with the four full reporter binaries green (34/61/128/218); the single failure both presets is the pre-existing real-PTY timing flake E2E_Pty_Mermaid ('state fixture clear did not complete' under load avg 6-10; passes alone on an unloaded debug machine; the E6c e2e diff is one LABELS line, so it is not causal). Reports are byte-stable across presets (verifier OK both). Matrix drawn_at_240 is 582, not the E1-era 540: the +42 is exactly the E2a reviewed gitGraph/state cohort; no other frozen count moved. Docs: mermaid-conformance.md, mermaid-obligation-ledger.md, mermaid-terminal-tests.md. Linux dual-preset remains the outstanding RFC graduation gate from E1. |
| 2026-10-24 | E6d | libFuzzer Mermaid harness and repaired fuzz gate (macOS) | Local, uncommitted | Replaced the two broken per-target `-fsanitize=fuzzer` COMPILE options with link-only options plus a global `fuzz` preset (`-fsanitize=fuzzer-no-link,address,undefined`); this fixed the pre-existing clang-23 `-Wreduced-bmi-output-overrided` ENABLE_FUZZING failure. Added fuzz_mermaid_parse (module impl unit via `import :detail` + testing seam; envelope, sequence/charts/gitgraph/relations layouts, and both render_mermaid entry points rendered into fixed 240/40 headless screens; fresh Work per call; 65536-byte gate), tests/fuzz/corpus/mermaid (14 edge seeds), tests/fuzz/run_mermaid_fuzz.sh (smoke + 600 s, Darwin-safe detect_leaks=0). Smoke 500 runs/1621 seeds clean; 600 s: 217451 executions, 60771 features, peak RSS 637 MiB, zero crashes/timeouts/artifacts. Normal debug/release presets unaffected. Linux fuzz run remains the execution gate. |
