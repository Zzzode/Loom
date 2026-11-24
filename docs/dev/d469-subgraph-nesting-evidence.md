# D-469 — `subgraph_nesting` (30 rows) blocker evidence

TL round 804 directed the next slice at the roster class `subgraph_nesting`
(total 30, roster `causality: debt`). This note records the measured blocker
split and the contract boundary the slice hits. Probe log:
`/tmp/d461/d469-subgraph-probe.log` (temporary target removed afterwards).

## Method

A throwaway render probe attached to the existing test target built the
production `render_markdown` seam and rendered every one of the 30 rows at
240 columns twice: the whole source, and the source with the frontmatter
block stripped. A row whose body draws is blocked only by its frontmatter; a
row whose body stays whole source has a body/projection blocker. Micro
fixtures then isolated the failing grammar.

## Split (30 rows)

**14 rows draw with the frontmatter stripped — config-leaf blockers only.**

| Leaf group | Rows | Source of the refusal |
|---|---|---|
| `config.theme: "redux" / "redux-color" / "redux-dark" / "redux-dark-color"` | 8 (`redux-color-subgraphs/01,02,05,06,09,10,13,14`) | `flow_envelope_body`'s D-440 palette table is `{forest}` only |
| `config.layout: dagre` (+ `flowchart.titleTopMargin: 0`) | 3 (`dagre/6088-2`, `dagre/…7827`, `dagre/sub-graphs`) | dagre profile is admitted, but the untrusted-compound guard rejects these subgraph-bearing graphs |
| `config.flowchart.subGraphTitleMargin: {top, bottom}` | 3 (`self-loops`, `title-margins-set-lr`, `title-margins-set-td`) | the leaf is not in the closed table; non-zero margins are refused like non-zero `titleTopMargin` |

All 14 are `flowchart/` fixtures. Per `docs/dev/d440-flowchart-theme-closure.md`
§2/§4, the 400 fixtures under `flowchart/` are exactly the frozen Flow400
contract (400 sources / 8 complete models / 392 pending-null), and any
flowchart config slice that draws one of them requires a Flow400 re-baseline
(build models, recompute SHAs) — a protected surface requiring a TL decision.
That is why it is not a `debt` slice in the roster sense.

**16 rows keep whole source with the frontmatter stripped — body blockers.**

| Group | Rows |
|---|---|
| user-styled subgraph containers (`style`/`classDef` on subgraphs) | 4 |
| markdown-string subgraph titles / raw-string labels | 5 |
| `graph`/`flowchart` + `%%{init: {"flowchart": {"inheritDir": …}}}%%` | 2 |
| subgraph title margins with cross-boundary edge labels | 1 |
| cluster-edge routing through deep nested subgraphs (v2-18) | 1 |
| `conf-and-directives` theme rows (shape re-declaration conflict) | 2 |
| `style` directives on subgraph ids | 1 |

Total 4 + 5 + 2 + 1 + 1 + 2 + 1 = 16.

The two `conf-and-directives` rows are **not** directive-only documents (the
structural judgement table previously said so): both carry
`graph TD` bodies that fail on a *shape re-declaration conflict*
(`A(Start)` then later `A[/Another/]`), not on subgraph depth. Micro fixtures:

- `graph TD / A --> B` → drawn
- `graph TD / subgraph s / B / end` → drawn
- `graph TD / A(Start) --> B[/Another/] / A[/Another/] --> C[End]` → whole source
- the same two edges with a subgraph → whole source

Bare-node subgraphs already draw, so `subgraph_nesting` names the *detector*
(`\bsubgraph\b`), not the operative blocker for these rows.

## Conclusion

`subgraph_nesting` cannot be closed by an owned config slice:

1. **28 rows** are frozen by the Flow400 flowchart contract; drawing them is a
   Flow400 re-baseline (protected; D-440 already recorded this).
2. **2 rows** need a shape-re-declaration precedence rule in the shared
   flowchart body parser (`A(Start)` then `A[/Another/]`), a behavioural
   contract decision, not a config leaf.

The roster label stays as emitted (detector-driven); this note is the
evidence that the class is contract-gated rather than an implementable debt.
