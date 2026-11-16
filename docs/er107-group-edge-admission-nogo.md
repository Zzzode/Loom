# ER group-edge whole-source admission — NO-GO closure (D293→D298)

Status: **NO-GO for the parser-only / independent-cohort route (2026-11-17).**
This is a recorded negative result, not an abandoned task: it closes one
specific route with measured evidence and states the exact conditions under
which the goal can be revisited. Stage A (commit `916b9dd9`) is retained and
is independently valuable; see §5.

## 1. Goal that was attempted

Make six ER originals that address a subgraph as a relationship endpoint (or
carry an in-group `direction` override) render as **drawn** whole originals in
the OfficialMatrix, by admitting two grammar forms in the production
`parse_plain_er`:

1. a relationship endpoint that resolves to a declared subgraph id;
2. a `direction` override nested inside a subgraph (consumed as a
   non-terminal directive; only the top-level direction sets
   `diagram.direction`).

The seven-row population (six draw candidates + one handDrawn whole-source
Work-ceiling row) is selected mechanically, renderer-free, by
`tools/mermaid/generate_er_group_edge_cohort.py`, which reproduces the exact
set over all ER originals.

## 2. Symptom — why these rows cannot be "drawn" the obvious way

All six candidates are **whole-null ER206 originals**
(`whole_model is null`). In the OfficialMatrix, a whole-null ER original is
classified `drawn` through exactly one path: the ER107 **owned-proof**
branch in `run_official_er206_case` (`tests/ui/test_mermaid_official.cpp`,
the `er107_local_proof` gate). Non-kPins ER paths invoke the same function
with no proof object.

`Observation::status` defaults to `{"error","error","error"}`:

- **Before admission** the parser rejected the grammar, so the mermaid render
  was identical to the plain-text control fence; the `wide && a` condition
  then marked each width `source_fallback` — a silent, error-free fallback.
- **After admission** the parser accepts and produces a real scene that
  differs from the text control (`wide == false`). With no owned proof, the
  row can no longer be downgraded to `source_fallback` and is left at its
  default, booking an explicit error
  `er206_whole_null_unmet` ("OfficialER206 whole-null art/ordinary/restore
  unmet; never admission").

The six cannot join the ER107 kPins cohort that would grant them a proof:
their ER206 whole-null debt is
`raw_entity_group_kind_collision_not_representable_without_loss`, whereas the
kPins cohort is derived from the sha-frozen ER206 prior by the exact
predicate `whole_model is None AND debts == {active_config}`
(`tools/mermaid/verify_er107_admission.py`), which selects precisely the
existing 107 and excludes every group-edge row.

## 3. Why the independent-cohort route is objectively infeasible here

A separate live-renderer cohort gate (the D237 approach, rebuilt this round)
passes: its predicate is a weak char-surface comparison — the native grid
differs from the text-control grid — and all 21 per-width states matched.
But that predicate is strictly weaker than the OfficialMatrix whole-null
policy, which additionally demands an authority-owned proof and a strict
ER206 geometry check. The matrix deliberately treats "renders, but with no
reviewed proof" as **error, never admission** (the literal error string ends
in "never admission"). Wiring the independent ledger into the matrix would
not satisfy `official_er107::owned(...)`, so it cannot produce a `drawn`
status. The weak gate's green result is the exact false-green class the
whole-null authority path exists to prevent.

Corroborating evidence: `MermaidErSubgraphSourceProfile` shows the two
admitted grammar forms themselves fail the strict ER206 geometry gate
(unsupported 7→5, geometry 0→2). Even with a proof slot, a proof is only
"ready" when `!wide && restored && a && b && c` holds, which the current
renderer does not achieve for these scenes.

## 4. Measured OfficialMatrix delta (the decisive evidence)

Real debug OfficialMatrix run (63 tests, ~727 s), parser admission applied
to an otherwise unchanged Stage A tree:

| metric                         | before | after |
|--------------------------------|--------|-------|
| observed wide_240 **drawn**    | 1232   | **1232 — zero gain** |
| source_fallback                | 913    | 907   |
| **error**                      | 0      | **6 (new)** |
| mandatory_gaps                 | 48     | 54    |
| coverage.drawn_at_240          | 1053   | 1053  |
| total observations             | 2738   | 2738  |

All six new errors use the already-known, roster-vocabulary category
`er206_whole_null_unmet`; no unknown error category is introduced. The
seventh (handDrawn nested) row correctly stays `source_fallback` on the
pre-existing envelope double-parse Work ceiling, with zero errors. Net
effect of the parser-only change: **six new explicit errors in exchange for
zero new drawn originals**, and the project's headline zero-error ledger is
broken. That is a net-negative trade, so the change is shelved.

## 5. Stage A is retained — do not conflate with "tried and useless"

Commit `916b9dd9` (D287 Stage A, *feat(mermaid): extend ER source-model
seam with nested group trees*) stays. It is a test-only generator capability
extension with independent value:

- a parent-linked nested group tree (`SrcSubgraph.parent` + `scope_stack`)
  replacing the flat single-scope model;
- reuse of the existing `er_sem_read_name` lexer so quoted multi-word group
  ids are represented (quote-inclusive witness spans, unquoted logical ids);
- a forward-reference post-pass that keeps the entity table and
  `first_insert_order` dense.

It keeps all 107 existing cases byte-identical and is fully covered
(emitter contract 21/21 under debug/release/ASan). It is a **necessary but
not sufficient** step: it lets the parallel generator represent these
sources, but representing them does not grant the authority-owned proof or
the strict geometry that an OfficialMatrix `drawn` status requires.

## 6. Exit criteria — what would unblock real admission

Revisit only via an authority-path change, which is RFC-level, multi-commit,
and touches protected frozen assets (not a single atomic change):

1. Give whole-null group-edge rows a route to an **owned proof** — either
   resolve the ER206 `raw_entity_group_kind_collision` debt / extend the
   active_config selection predicate so the rows can enter a proof cohort,
   or add a reviewed second supplemental proof slot wired into
   `run_official_er206_case`.
2. Close the **strict ER206 geometry gap** for these scenes so a captured
   proof reaches `ready` (`!wide && restored && a && b && c`).
3. Re-baseline the affected protected authorities in the same reviewed
   change: the ER206 prior, the selection predicate and its cohort lock,
   and the e6c mandatory-gap roster (size + sha256 in
   `mermaid_gap_roster.hpp`).
4. Re-run the full OfficialMatrix and require a real drawn increase with the
   zero-error ledger preserved.

## 7. Where the shelved work and evidence live

- In-repo stash: `stash@{0}` — "D295/D298 shelved ER group-edge parser
  admission (NO-GO ...)". Restore with `git stash apply stash@{0}` (keeps
  the stash) or `git stash pop`. It contains the two production parser
  hunks, the SourceProfile 7/0→5/2 update, CMake registration, the cohort
  gate test, the generator and the 7-row ledger.
- Out-of-tree backup (GC-proof, with matrix evidence): `/tmp/d295_shelved/`
  — the same patch and new files plus `d295_pre_admission_matrix.json`,
  `d295_post_admission_matrix.json` and a `README.md` with resume steps.
- Throughout this work the ER107 active_config kPins, the three
  `mermaid_native` proposal JSONs and the ER206 prior were **never modified**
  (verified zero-diff); no production or protected file is changed in the
  retained tree.

## 8. D301 follow-up — the geometry-gap hypothesis is falsified

Milestone M-Next hypothesized that the deeper blocker was renderer geometry:
that the two admitted grammar forms fail the strict ER206 whole-null geometry
gate, so the six scenes could not earn an owned proof even if one were
offered. Direct, production-faithful measurement **disproves** that
hypothesis. There is no renderer geometry capability gap for these scenes.

### Symptom investigated

`MermaidErSubgraphSourceProfile` books the two admitted-form literals
(`Group_business_endpoint`, `local_direction`) as geometry=2 after admission.
The question was whether that reflects a real layout defect (broken group
boxes, dropped edges, non-determinism, disturbed sentinel) or merely an
obligation-classification mismatch.

### Measurement (production-faithful path, not a char-difference probe)

Using the same `er206_observe` normalized `note13_capture` grids and the same
`following_row` predicate that `run_official_er206_case` uses (frame mapping
verified equivalent to `official_er_capture`: mer240, mer40, mer240-restored
vs the text controls), each of the six real fixtures satisfies every
geometric component of the whole-null drawn condition
`!wide && restored && a && b && c`:

| fixture (short) | !wide | restored | a | b | c | drawn predicate |
|---|---|---|---|---|---|---|
| relationships-between-subgraphs | 1 | 1 | 1 | 1 | 1 | **1** |
| subgraphs-direction-override | 1 | 1 | 1 | 1 | 1 | **1** |
| subgraphs-quoted-ids | 1 | 1 | 1 | 1 | 1 | **1** |
| subgraph-and-entity (pr-check) | 1 | 1 | 1 | 1 | 1 | **1** |
| nested-direction-default | 1 | 1 | 1 | 1 | 1 | **1** |
| nested-direction-elk-layout | 1 | 1 | 1 | 1 | 1 | **1** |

(`narrow_source` is 1 for three rows and 0 for three; both are acceptable —
the supplemental proof records `narrow_exact_ordinary` per row.) Work budget
is not implicated: ~119–122M Work units remain at the gate, and rendered
dimensions are within the 240×1040 bounds. Visual dumps confirm structurally
complete scenes — group box containing the inner entity, a separate entity,
an edge attached to the group box with both cardinality markers and the role
label, and a clean independent sentinel row. Re-render determinism
(`restored`) holds for every fixture.

A first probe built on the independent cohort's raw ftxui `Screen` capture
reported `a/b/c = 0`; that was a **false negative** caused by capture
granularity (raw pixels can hold multi-char cells, while `following_row`
requires single-char cells). The normalized `note13_capture` Grid used by the
official path splits cells correctly and the predicate passes. This is a
concrete instance of the rule already logged in §3: a renderer-free /
char-difference gate does not establish official admission — and neither does
it faithfully reproduce the official geometry predicate.

### Why the SourceProfile "geometry=2" is not a geometry defect

Those two literals carry the profile `valid_unimplemented_source_obligation`
with an empty Model. The closed-source branch of the test *expects*
whole-null unimplemented obligations to render as plain text (mermaid grid ==
text grid). After admission they correctly produce art, so the equality no
longer holds and the branch counts geometry — but the art itself is correct
and passes the official drawn predicate. Driving that counter to zero would
require **reclassifying** the literals as `mandatory_complete_positive` with
complete Models (an authority/test-contract change), not changing the
renderer. The other five `unsupported` literals exercise unrelated
unimplemented surfaces (classDef, styles, accessibility, entity/group kind
collision) and are outside rendering-geometry scope, so the literal
"unsupported 7→0" target is not reachable by geometry work alone.

### Conclusion and minimal viable change surface

No production rendering or layout change is warranted: the geometry already
reaches the target once the grammar parses. The sole remaining blocker is the
authority-owned proof path described in §6 (resolve the ER206
group-collision debt / selection predicate, or add a reviewed supplemental
proof slot, plus the same-reviewed re-baseline of the ER206 prior and e6c gap
roster). That is the RFC-level, protected-asset work this milestone was
explicitly scoped to avoid. No layout architecture change is needed; the
"minimal viable change" lives entirely in the authority layer, not in
`RelationLayout`/`ErPaint`.
