# FS-7b design — collapsed subgraphs (`@{ view: collapsed }`)

Status: **DESIGN ONLY. No production change yet.** Authored 2026-11-09
against `64a5b3be`; revised after an independent read-only design review
(task a0c3099202250e9d4, verdict PROCEED-WITH-CHANGES) that disproved the
first draft's "Region shape alone suppresses descendants" assumption.
All integration points below are verified read-only against the source,
with file:line evidence. The four target fixtures currently fall back on
the body track (confirmed with a CellCapture char-vector probe, not
`Screen::ToString`).

## 1. Goal and corpus

Admit the collapsed-subgraph directive so a subgraph annotated
`<groupid>@{ view: collapsed }` renders as a single titled rectangle: its
descendant vertices, nested frames and internal edges are hidden, and any
edge that targeted a hidden descendant is redirected to the collapsed
ancestor node. All four fixtures are description-only frontmatter, so the
whole gate is the body grammar, not the envelope.

Fixtures are referenced by name to avoid numbering ambiguity:

| Fixture (short name) | Tier | External edge shape |
|---|---|---|
| `...collapsed-subgraph-as-a-single-node-with-edges-redirected-to-it` (**single-node**) | 1 | authored edges already name the group id (`Start --> one`, `one --> End`); only internal `A->B->C` disappear |
| `...collapsed-subgraph-alongside-an-expanded-one` (**alongside-expanded**) | 1 | `g1 --> g2` is group-to-group; internal `A1->A2` hidden; sibling `g2` stays expanded |
| `...redirect-an-edge-targeting-a-deeply-nested-node-to-the-outermost-collapsed-subgraph` (**deeply-nested**) | 2 | `Start --> A` (A nested two levels) must remap to collapsed `outer` |
| `...collapse-nested-subgraphs-to-the-outermost-collapsed-ancestor` (**nested-ancestor**) | 2 | `Start --> inner` (a hidden nested group) remaps to collapsed `outer`; the inner group collapses into the outermost ancestor |

None of the four appears in `e6c-mandatory-gap-roster.json` or any
`*cohort*.json` (verified grep, zero hits; only the
`rfc0031-baseline.json` file inventory lists them), so admission does
not move the 48-gap roster or Class285.

Syntax facts: the operative directive is the trailing
`id@{ view: collapsed }` line after the group's `end`. The
`subgraph g1 [Collapsed]` bracket text is the ordinary group *title*
(`flow_group_header`, mermaid_impl.cpp:1123), not the collapse marker.

## 2. Current rejection point

A standalone `outer@{ ... }` statement routes through `flow_statement`
(:1661) to `flow_node_from_id` (:1539) → `Diagram::vertex` (:865), which
returns `nullopt` for any id equal to a `Group.id`. `flow_shape_data`
(:1278) has no `view` key. The parse fails and the body keeps ordinary
source fallback. The pre-scan (:1175-1177) only recognizes `subgraph`
lines, so the directive cannot create a spurious deferred shell.

## 3. Corrected architecture (five coordinated sites)

The first draft assumed a `place_region` leaf short-circuit would make
descendants disappear everywhere. It does not: `flow_layout` consumes
`graph.vertices`/`graph.groups`/`graph.edges` directly in four places
that hard-fail on a hidden node's zero box or a missing group box
(:5199-5200 group-box completeness, :5204-5215 zero-box endpoint,
:5224-5225 placed-slot validation, :5301-5307 unconditional paint, and
both router branches :5212 and :5447). The design therefore adds an
explicit visibility/projection layer rather than relying on Region shape.

### 3.1 Parser — group collapse statement and `Group.collapsed`

Add `bool collapsed = false;` to `Group` (:817) as the **last data
member**, following the FS-7a in-struct precedent (`deferred`,
`forward_referenced`). It must be appended after `forward_referenced`,
never inserted earlier: the positional aggregate at :2245-2246 ends in a
trailing `true` that binds `deferred`, so an earlier bool would silently
swap the two same-typed flags with no compiler warning and corrupt
pre-scan shell semantics. Guard the layout with
`static_assert(sizeof(Group) == <current value>);` next to the struct so
a size change cannot move the exact-budget tests (prepay charges
`8 * sizeof(Group)` at :2355/:2461).

In `flow_statement`, before node resolution, detect the exact statement
`<id>@{ view: collapsed }` with these fail-closed rules:

- the directive is accepted only while `graph.group_stack` is empty (all
  four fixtures author it after `end`); a directive targeting a group
  still open on the stack rejects;
- the id must match an already-declared, **non-deferred** group; an
  unknown id or a forward-reference shell rejects (a collapse directive
  cannot declare a group);
- parse `@{`, the sole key `view`, `:`, then the value token. Accept
  only the bare word `collapsed` or the double-quoted `"collapsed"`
  spelling — body statements do not pass through
  `normalize_directive_quotes` (that helper serves init directives only,
  :1960), so single quotes are NOT accepted here;
- require optional whitespace then `}`; reject a second key, any other
  view value, or any trailing token;
- zero new unconditional `work.spend()` (mirror `flow_shape_data`'s
  `(void)work`, :1281) — the directive is a closed fixed-token
  production covered by the existing whole-body prepay;
- a repeat directive on the same group with the same value is
  idempotent; a conflicting value rejects.

### 3.2 Region layout — leaf-sized collapsed group, kept in `groups`

In `place_region` (:4144), when `owner >= 0` and
`graph.groups[owner].collapsed`, short-circuit **after** the item sort
(:4185) but **before** the ordinary owner-frame finishing code
(:4293-4302): do not enumerate the owner's direct vertices and do not
recurse into child groups. That finishing code applies a title band
(`shift_y`, title-width widening) intended for `group_frame`; reusing it
would make the Region box larger than the rectangle `node_box` actually
paints, so router ports (taken from `group_boxes`) would land on blank
cells outside the visible border. The short-circuit instead returns a
Region with empty `nodes` and exactly one `groups` entry
`{owner, {0,0,w,h}}` where `w/h` are strictly the ordinary
`node_size` rectangle result (:2896-2898), with no shift or title-band
expansion.

**Never** place a group index into `Region.nodes`: that vector is
indexed as a vertex subscript at :5162-5165, and a group index would
alias and overwrite a real vertex slot. Because placement is bottom-up
and a collapsed owner never recurses, all descendants are excluded from
every ancestor Region's `nodes`/`groups`. Nested collapse (the
nested-ancestor fixture) is automatic — the inner group is never visited
once the outer owner short-circuits.

### 3.3 NEW: flow_layout visibility + endpoint projection/edge-filter layer

Insert this before the Router (before :5182). It is the layer the first
draft omitted.

0. **Whole-path bypass when nothing is collapsed.** Immediately after
   parse, compute `any_group_collapsed` (one scan of
   `graph.groups`). When it is false, `flow_layout` takes the exact
   existing code path and never enters this layer, so the
   byte-frozen FS-7a/GroupEndpoint15/E6c gates see literally no branch
   taken. The "non-collapsed graphs are byte-identical" guarantee (4.1)
   is structural, not dependent on an identity transform happening to
   match.
1. **Visibility map.** For each vertex, walk its `group` parent chain
   (depth bounded by `kGroupDepthLimit` = 16); the vertex is hidden iff
   the chain reaches a group with `collapsed`. A group is hidden iff it
   is, or is nested inside, a collapsed group.
2. **Endpoint resolution.** For every `graph.edges` endpoint, resolve to
   a visible target: a visible vertex keeps its `EndpointKind::Vertex`
   and its own placed box; a hidden vertex or hidden group resolves to
   the nearest collapsed ancestor group, presented as
   `EndpointKind::Group` with that group's `group_boxes` entry (the
   existing group-endpoint representation; ordinary Rectangle ports at
   :4364 and the existing port sealing at :5320-5357 are reused
   unchanged).
3. **Edge filter.** Drop an edge whose two endpoints resolve to the same
   collapsed leaf (an internal edge). This same-leaf drop must be done
   explicitly here; it cannot rely on the `place_region` quotient guard
   (:4245), which is local to ranking and never rewrites `graph.edges`.
4. **Synthetic projected Edge records (hard constraint).** The layer
   emits elements of the form `{ original edge index, synthetic Edge }`.
   The synthetic Edge copies the original's `text`, `stroke`,
   `arrow_start`/`arrow_end` and `minimum_length`, but its `from`/`to`
   are the projected endpoints (with `EndpointKind::Group` where
   remapped). Swapping only the EndpointBox while keeping the original
   Vertex/Vertex Edge is insufficient: `route_one` reads the group kind
   from the *Edge reference* (:5321 crossed-port erasure, :5342
   group→descendant check, :5398-5401 `seal_group_port`, :5422-5423
   `connect()` protection), and both router loops (:5440, :5465) must be
   handed the synthetic Edge. The rich-label lookup still uses the
   **original** index for `rich->edge[index]` (:5417).
5. **Router path selection.** Replace the single `has_group_endpoint`
   input with a layer-computed flag: "the projected edge set contains a
   Group endpoint" (`has_group_endpoint` has only three consumers,
   :5169/:5183/:5433, so this fully substitutes). When true — notably the
   deeply-nested fixture, whose authored graph has
   `has_group_endpoint == false` but whose remap yields a Group
   endpoint — route through the tagged/group branch (:5183+) using the
   synthetic edges and `group_boxes`; otherwise the ordinary branch, also
   over the projected set.

### 3.4 Fix the direct consumers to respect visibility

- :5224-5225 placed-slot validation: skip hidden vertices (their boxes
  stay zero) instead of hard-failing.
- :5301-5307 paint loop: skip hidden vertices.
- :5191-5200 group-box completeness: only require boxes for visible
  groups; a group hidden inside a collapsed ancestor has no Region box
  and must not trip the invariant.
- Both router branches consume the projected synthetic edge list from
  3.3 rather than the raw `graph.edges`.
- :5233-5235 hard width gate: the `longest` label scan currently runs
  over the raw `graph.edges`, so a dropped internal edge whose label
  exceeds the width limit would fail the whole collapsed source even
  though the edge is never drawn. Move the scan onto the projected
  (post-filter) edge list so hidden edges cannot trigger the width
  rejection. The canvas-area accounting at :5237-5239 may continue to
  over-count hidden vertices/edges — it only over-spends Work, never
  paints a cell, so it is left unchanged.

### 3.5 Paint the collapsed group as a node

At the group paint loop (:5292-5300), branch on `Group.collapsed`: a
collapsed group is painted with `node_box` using a synthetic
`Vertex{group.id, group.text, Shape::Rectangle, ...}` (titled
rectangle), not with `group_frame`. Pass the group's rich runs
(`group_runs`, already computed at :5295-5297) as `node_box`'s fifth
argument (:3843-3845) so a bold/underline group title does not silently
degrade to plain text. Expanded groups keep `group_frame` unchanged.

### 3.6 Deliberately unchanged / conservative semantics

- **classDef/style:** a group id is never a vertex, so
  `resolve_flow_styles` (:1029-1031) already fails on it and :5010
  rejects grouped-vertex styling. A collapsed leaf is therefore always
  an unstyled plain rectangle; combining `class g1 x` with collapse keeps
  the existing fail-closed behavior with no new interaction.
- **dagre B6 gate:** `flow_untrusted_dagre_compound` (:5050) runs on the
  unrewritten model and is not relaxed. A collapsed source carrying
  `layout: dagre` may still be conservatively rejected; the four
  fixtures are description-only and unaffected. Intentionally deferred.
- **Fan-in convergence:** several edges remapping onto one collapsed
  leaf can produce parallel edges that exhaust `used_ports` (:5412-5415).
  That fails closed (whole source), which is safe; documented as a known
  limit, not a defect.

## 4. Frozen-contract hazards and guardrails

1. **48-gap roster and Class285** stay byte-identical: the change is
   flowchart-body-only and gated on the new default-false `collapsed`
   flag, so a non-collapsed graph follows the existing path with no
   branch taken. Enforcement is the full OfficialMatrix plus the E6c
   roster diff gate and the Class285 gate.
2. **Work/gas budget.** No budget constant changes in this cohort. Two
   guardrails make that safe: (a) the parser branch adds zero new
   unconditional `work.spend()` (prepay at :2369-2469 is a parse-bound
   function of bytes/identifiers, independent of which branch runs);
   (b) `static_assert(sizeof(Group) == ...)`. On the layout side the
   collapsed path places strictly fewer items and routes strictly fewer
   edges. The 7213 dense-dagre and FlowStyleBaseline exact-budget tests
   use sources without a collapse directive, so `collapsed == false`
   guarantees a byte-identical path. A meter recalibration remains the
   separate deferred FS-1b milestone.
3. **Authored geometry, not upstream-derived.** There is no Chromium
   model for these four in the repo. Each fixture gets a reviewed cohort
   ledger with the standard five-frame proof (wide native, narrow,
   restored identity, ordinary-source difference, following), same shape
   as the FS-7a ledger, with an independent review of the authored
   collapsed-node geometry.
4. **narrow_40 is measured, not assumed.** Unlike FS-7a (every row
   narrow-source), a collapsed graph is much narrower than the expanded
   form and may draw at 40 columns. The ledger must pin the narrow frame
   from a real CellCapture rather than presupposing `source`.
5. **No lossy render.** If tier-2 remap cannot be proven pixel-clean
   behind the existing router, tier 1 ships alone and tier 2 stays on
   source with a documented blocker.

## 5. Incremental slices and ledgers

No standalone parser-only commit (it would be dead code and, per
MINOR-1, its intermediate output is an *expanded* render, not unchanged
fallback). Parser accept/reject unit tests land inside Slice B.

- **Slice B — tier 1 (single-node, alongside-expanded).** Parser +
  `Group.collapsed` + region leaf (:3.2) + visibility/projection layer
  (:3.3) including same-leaf edge drop and the :3.4 consumer fixes +
  node paint (:3.5). No endpoint remap is needed because the external
  edges already target group ids; the layer still has to drop internal
  vertex-vertex edges whose boxes are now hidden. Own cohort ledger and
  durable pixel gate (schema modeled on
  `loom_mermaid_fs7a_subgraph_cohort/v1`).
- **Slice C — tier 2 (deeply-nested, nested-ancestor).** Adds the
  hidden vertex/group → collapsed-ancestor remap and the tagged-path
  selection of :3.3(5). Separate ledger so a tier-2 slip cannot block the
  tier-1 gate; ships only if the router proves pixel-clean, otherwise the
  two fixtures stay source with a written blocker (same status class as
  the FS-1b Work-ceiling row).

## 6. Fail-closed test matrix (parser level, inside Slice B)

Accept: bare `collapsed`, double-quoted `"collapsed"`, repeat
(idempotent). Reject: unknown view value; single quotes; a second key;
trailing tokens; unknown group id; directive while a group is still open
(non-empty group stack); directive naming a deferred shell; conflicting
repeat value. Plus a `static_assert` on `sizeof(Group)`.

## 7. Reviewer open questions — resolved

1. *Is the leaf short-circuit sufficient to suppress the subtree?* No —
   the four direct consumers listed in 3.4 must be fixed explicitly; the
   Region alone is insufficient.
2. *Does a collapsed leaf need distinct ports?* No — present it as
   `EndpointKind::Group` + a `group_boxes` Rectangle entry and reuse the
   existing Rectangle ports and group-port sealing; force the tagged
   router path when a remap yields a Group endpoint.
3. *Can Slice B avoid changing Work constants?* Yes, given zero new
   unconditional spend and the `sizeof(Group)` static_assert; the
   collapsed layout does strictly less work and non-collapsed sources
   are byte-identical.
4. *One ledger or two?* Two — one per slice, matching the one-cohort-
   one-JSON convention and isolating a tier-2 slip.

## 8. Review history

- **Round 1** (independent read-only general-purpose agent, task
  a0c3099202250e9d4, zero builds): PROCEED-WITH-CHANGES, four MAJORs.
  The draft's premise that a Region leaf shape alone suppresses
  descendants was disproven — `flow_layout` has four direct
  vertex/group/edge consumers that hard-fail on hidden nodes, plus a
  group-index/vertex-slot aliasing hazard and a mis-located redirect
  layer. All four were resolved by introducing the explicit
  visibility/projection layer (3.3), keeping collapsed groups in
  `Region.groups` (3.2), and re-slicing B/C (section 5).
- **Round 2** (same agent): PROCEED-WITH-CHANGES, no MAJOR; four MINORs
  and two NITs, all incorporated above. The three hard correctness
  constraints are the synthetic Edge carrying the projected Group kind
  (3.3.4), the no-collapse whole-path bypass (3.3.0), and bypassing the
  owner-frame finishing code in the region short-circuit (3.2). The
  remaining items (longest-label scan moved onto projected edges 3.4,
  `collapsed` appended as the last Group member 3.1, group rich runs
  passed into `node_box` 3.5) are recorded as explicit design rules.
  After these edits the tier-1 Slice B is judged independently
  shippable; tier-2 Slice C is a small remap step on top.
