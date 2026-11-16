# ER107 Stage A — nested-group generator extension design (D287)

Status: **Stage A implemented and verified (test-only).** Extends the
parallel ER107 record generator (the D248→D279 seams) so the six SELECTED
group-edge originals — nested subgraphs, quoted multi-word group ids and
group-id relationship endpoints — can be generated. The production
`parse_plain_er` admission is deliberately NOT part of Stage A; it ships in
the later atomic 107→113 re-baseline (Stage B–D). This document records the
two design decisions D287 required to be justified, not just implemented.

## 1. Why a parent-linked group tree, not a flat model

The D271 record model represented subgraphs as a flat list with a single
`current_scope` and one `source_parent_subgraph` per entity, and explicitly
rejected nesting (`subgraph_depth >= 1 -> false`). That is sufficient for
the original 107 cohort (only one root-level subgraph ever appears) but
cannot represent the six new cases, two of which nest a `details` group
inside a `domain` group.

Two target shapes were considered:

- **Flat list + depth tag.** Keep `SrcSubgraph` flat and add a `depth`
  integer, reconstructing hierarchy implicitly from declaration order.
  Rejected: depth alone does not identify *which* group is the parent when
  siblings exist at the same depth, and entity ownership would have to be
  re-derived on every read by re-walking a stack that was thrown away. It
  also invites order-special-casing for the six samples rather than a
  general rule.
- **Parent-linked tree (chosen).** Add `SrcSubgraph::parent` (immediate
  enclosing group id, empty at root) and replace the single scope with an
  explicit `scope_stack` of open-group indices while parsing. This is the
  same shape the production renderer already uses — `ErGroupParse.frames[]`
  is a depth stack and the whole-scene model serializes `groups[].parent` —
  so the parallel record converges on the production terminal state instead
  of a parallel ad-hoc encoding.

The tree gives three properties the flat model cannot:

1. **Innermost ownership falls out naturally.** An entity declared inside
   `domain > details` is owned by `details`, not `domain`; the back of the
   scope stack is always the innermost open group. Verified against
   production: `nested-...-default` yields `ORDER.owner = details` while
   `domain.members` contains only `CUSTOMER` (a group lists its direct
   non-group members, not the nested subgroup).
2. **`end` restores the correct parent.** Popping the stack returns to the
   enclosing group, which the single-scope variable could not do.
3. **No per-sample specialization.** The same code handles root groups,
   siblings and arbitrary nesting up to the existing `kGroupDepthLimit`
   (16), matching the production frame cap; the 16-deep reject and the
   nested accept are both generic.

The result subgraphs serialize with `source_parent_subgraph` (null at root)
and the terminal group presentations serialize `parent` (null at root),
mirroring the whole-scene `groups[].parent` / entity `owner` contract.

## 2. Quoted group-id lexer — reuse, not a new grammar

A group id may be a quoted multi-word string (`subgraph "Customer Domain"`)
and may later appear quoted as a relationship endpoint. Rather than invent a
new quoted-token rule, Stage A reuses the existing `er_sem_read_name`
helper (the same primitive entity names and edge roles already use): it
accepts either a bare name-byte run or a quote-inclusive span and returns
the witness boundaries.

Two layers reuse it consistently:

- The D251 witness lexer `er_sem_body` reads the subgraph id through
  `er_sem_read_name`, so `subgraph_id` keeps the **quote-inclusive** byte
  span (witnesses must point at the actual source bytes).
- The D271 record collector reads the same span but stores the **unquoted
  content** as the logical id (`Customer Domain`), exactly as production
  `ErCursor::quoted` strips the quotes and the whole-scene id/label do.

Relationship endpoints apply the same unquoting to their logical ids
(`left_id`/`right_id`) while their witness ordinals still reference the
quote-inclusive spans. A quoted endpoint therefore resolves to the stored
subgraph id, and an endpoint naming a declared group is routed to the group
rather than inserted as an entity (with a post-pass to remove any group row
inserted by a forward reference before the group was declared). This keeps
the entity table equal to the production entity set.

## 3. Bidirectional-invariance evidence (the D287 hard gate)

Stage A must both extend the six cases and leave every existing byte
untouched. Verified on the debug preset with the production parser at its
unchanged HEAD behavior (zero parser-admission markers in the working
tree):

- **Regression half — the 107 stay byte-identical.** All 107-case
  canonical + sha256 differentials pass: whole-scene, structural,
  semantic, source_model, source_binding and terminal_model. The emitter
  suite is 21/21 (the original 19 plus two Stage A tests). The
  nested-group refactor changed internal record structures but no existing
  case's serialized output.
- **Forward half — the six are generated correctly.**
  `GroupEdgeSourceModelBuildsExpectedNestedTrees` runs the source_model
  seam (the layer independent of the production parser) over the six
  SELECTED originals and pins the production-semantics-traced group tree
  (id/parent/direct-members, captured from production whole-scene output)
  and entity ownership, including the quoted-id and nested cases.
- **Adversarial-review boundary coverage.** Four additional pins lock the
  edge rules rather than the happy path: `M1` a forward-referenced group
  endpoint (relationship before the `subgraph` declaration) leaves no
  group-named entity row, keeps `first_insert_order` dense 0..N-1 and still
  resolves through the db-id map; `M3` the D251 lexer keeps a
  quote-inclusive `subgraph_id` byte span over all six fixtures (the
  collector is the layer that strips quotes); `M2`/N1 an empty quoted id
  (`subgraph ""`) fails closed while exactly 16 nested groups (the
  production frame cap) are accepted and the 17th is the rejected boundary.
- **No production/ER-semantic regression.** `relations_semantics` 269/269
  and `er107_source107` 7/7 pass. An earlier single failure in
  `MermaidErSubgraphSourceProfile` was proven by isolation to be caused
  solely by the *parser admission* (reverting the two parser hunks while
  keeping all of Stage A restored it to green); it is therefore deferred to
  the atomic re-baseline, where its frozen counts move with the admission.
- **Scope.** Two files, all changes inside anonymous-namespace test-only
  lexer/collector/assembler/terminal code; no hunk enters the production
  `parse_plain_er`. The two parser hunks are held out of Stage A.

## 4. Why the forward test targets source_model, not terminal_model

The terminal seam embeds the whole-scene model, which is produced by the
production parser. Until the parser admission lands, the production
whole-scene seam still rejects the six sources, so the terminal seam
returns nullopt for them even though the parallel generator can parse
them. Stage A consequently pins the generator at the source_model layer
(which has no whole-scene dependency). The terminal-level, seam-vs-
production differential for the six cases is restored in Stage B–D once the
parser admission and the new pins land atomically.
