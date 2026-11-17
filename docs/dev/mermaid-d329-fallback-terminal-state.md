# D-329 Terminal Mermaid fallback: why zero-fallback is unreachable and the registry is the end state

Status: **accepted terminal state, 2026-11-17.** This document closes the
long-running "zero whole-source fallback" key result. It is written for a
future engineer who wonders why the OfficialMatrix still reports 907
`source_fallback` originals and whether that number can be driven to zero. The
short answer: it cannot, and the reason is measured rather than assumed. The
fallback count is now a **fully explained, locked, drift-gated** quantity, not
an open defect backlog.

All numbers below come from the pinned Mermaid 12.2.0 corpus via
`MermaidCapabilityReport.OfficialMatrix` at closure commit `ef37b602` (and are
reproducible from the scripts referenced here). Whole-original track, 240
columns.

## 1. What was actually achieved before this decision

Two rounds of owned-proof work proved the methodology works where a single
shared, text-decidable debt exists:

- **RFC 0033 group-edge cohort (6 cases).** Six ER originals carried one exact
  debt (`raw_entity_group_kind_collision_not_representable_without_loss`). Two
  parser admissions plus a sha-bound owned cohort moved them
  drawn 1232→1238, fallback 913→907, errors 6→0, mandatory gaps 54→48, with the
  formal `drawn_at_240` coverage held at 1053 (the six stay formally pending).
  Independently reviewed, dual-preset byte-identical, ten protected authority
  files untouched.
- A **fallback attribution registry** (D-325/D-327) made every remaining
  fallback reproducibly explainable.

The group-edge six were the ideal shape: one shared debt, an already-faithful
renderer, a compact sha-bound cohort. The question this document answers is
whether the remaining 907 contain more cohorts of that shape.

## 2. The 907 are fragmented, not a shared debt

A read-only census (`tools/mermaid/fallback_attribution.py`, whose output is
committed as `tools/mermaid/fallback_roster.json`) classifies every fallback
from the immutable fixture bytes with a deterministic total classifier — there
is no implicit "unclassified" bucket. The 907 split into **44 mutually
exclusive categories**; the largest is 17.6% and the top three together are
only 35.5%. There is no group-edge-style dominant shared debt.

Measured causality rollup:

| Causality | Rows | Share | Meaning |
|---|---:|---:|---|
| foreign-engine | 377 | 41.6% | neo/handDrawn look, ELK/dagre layout, HTML labels, icon fonts — external visual/layout engines the native orthogonal projection cannot byte-faithfully reproduce |
| family-scoped-debt | 266 | 29.3% | unimplemented subset of one family's surface; no cross-family construct isolated, yet the same family renderer draws other rows (so not permanent/foreign) — cannot be closed as one compact cohort |
| permanent | 143 | 15.8% | interactivity (click), dynamic timing (animation), sandbox security — no static terminal equivalent exists |
| debt | 105 | 11.6% | parser/projection gaps for an isolated cross-family construct, in principle buildable |
| debt-candidate | 16 | 1.8% | a narrow suspected debt |

Two structural walls dominate.

### Wall A — foreign visual/layout engines are not closeable without weakening an invariant

377 rows need an external engine's output. The production projection documents
the boundary explicitly, e.g. `mermaid_er_impl.cpp`:
`look=... -> native orthogonal; roughness/relief not reproduced`. Reproducing
neo design tokens, handDrawn roughness, ELK routing geometry, HTML label layout
or font-library glyphs faithfully would require either building replacements
for those engines (open-ended, no terminal truth to validate against) or
accepting an approximate render as if it were faithful — which breaks the core
invariant that Loom never mis-draws foreign semantics. That is precisely the
stop condition set for this work.

### Wall B — most "debt" labels are co-occurrence, not sufficient cause

A construct named in a fallback source is not necessarily what makes it fall
back. The registry's `predicate_sufficiency` table measures, for every
detector, how many *drawn* originals also match it. A construct is a proven
sufficient blocker only when **zero** drawn originals match. Most labelled
debts fail that test:

| Category | Fallback rows | Drawn originals also matching the predicate |
|---|---:|---:|
| classdef_styling | 37 | 20 |
| subgraph_nesting | 31 | 68 |
| git_cherry_pick | 18 | 15 |
| special_link_markers | 16 | 80 (only 16.7% of matches fall back) |
| multiline_label | 7 | 46 |

These are primary suspects that co-occur with the real cause; building them is
not guaranteed to turn any specific row into a draw. The original D-325
"debt" labels were over-claims and the D-327/D-329 sufficiency experiments
corrected them rather than rounding the numbers up.

## 3. The linkStyle probe: even the cleanest-looking candidate is not a real cohort

The only two **proven sufficient blockers** (zero drawn co-occurrence) are
`click_interactive_callback` (18, permanent — hyperlinks/JS callbacks have no
static target) and `linkstyle_directive` (12, labelled debt). linkStyle looked
like a second group-edge: same flowchart/swimlanes surface, one construct
(`linkStyle N stroke/color/width`), sha-bindable, and production has no parser
for it at all (the keyword exists only in a reserved-word list).

A direct render isolation experiment (TechLead D-329) settled it:

- On a **minimal synthetic body**, linkStyle alone is sufficient: with the
  directive the body falls back; with every `linkStyle` line stripped, the same
  body draws.
- On **all 12 frozen-corpus fixtures**, stripping every linkStyle statement left
  the source in fallback — **0 of 12 unblocked**. Each real fixture is dominated
  by a stronger co-blocker (frontmatter `securityLevel`/`htmlLabels`,
  `look=handDrawn`, dagre tuning, subgraph/classDef). A binary search on the
  one fixture without an explicit foreign look showed both the linkStyle and
  the frontmatter config are independently sufficient.

So implementing a linkStyle parser would be a production change with **zero
additional drawn rows in the pinned corpus**. It is recorded in
`corpus_isolation_findings` and deferred, not actioned. This is the concrete
reason no second owned-proof cohort was opened.

## 4. The registry is the terminal state of this KR

The goal was repointed from "zero fallback" to "fallback semantic
explainability and stability", delivered by three mechanisms, all enforced by
tests:

1. **Reproducible attribution.** Every fallback maps to exactly one of 44
   categories with a deterministic predicate over the immutable fixture bytes,
   and every one of the 44 categories carries a causality label (the
   `--self-test` asserts the causality and category key sets are identical, so
   no category can ever ship un-attributed). It re-classifies every path from
   live bytes and proves counts reconcile with no unclassified state.
2. **Explicit, locked declarations.** All six causality values — permanent,
   foreign-engine, family-scoped-debt, debt, debt-candidate — are named with
   reasons instead of being treated as unfinished work; the labels and the
   measured sufficiency table make closeable-vs-structural auditable.
3. **No silent drift.** `--matrix <fresh OfficialMatrix>` fails (non-zero exit)
   on any unregistered fallback path, any reclassified path, or any count
   change. Verified by constructing each violation against a temp matrix:
   unregistered path, stale total (907→908), inflated category count, and
   reclassified row all fail, then restore clean.

A genuinely new closeable cohort can still appear later — but only if a future
renderer change removes a co-blocker and exposes a predicate that is both
sufficient in isolation *and* the sole blocker for a set of real rows. The
isolation experiment is the gate that distinguishes that real opportunity from
a co-occurrence label. Until then, reducing 907 further would require either
foreign-engine emulation or weakening the no-mis-draw invariant, neither of
which is acceptable.

## 5. Reproducing the conclusion

```sh
# hermetic: classification is total and reproducible, counts reconcile
python3 -I tools/mermaid/fallback_attribution.py --self-test
# drift gate against a fresh OfficialMatrix (run only at closure)
python3 -I tools/mermaid/fallback_attribution.py --matrix \
    build/<preset>/tests/ui/mermaid-official-matrix.json
# regenerate after a reviewed renderer change (never hand-edit the roster)
python3 -I tools/mermaid/fallback_attribution.py --emit <matrix> \
    -o tools/mermaid/fallback_roster.json
```

Closure figures: observed wide_240 drawn 1238, source_fallback 907, original
errors 0, mandatory gaps 48, formal `drawn_at_240` 1053. The 907 is understood,
locked, and cannot move without a reviewed registry entry.
