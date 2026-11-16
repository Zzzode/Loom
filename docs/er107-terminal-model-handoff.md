# ER107 `terminal_model` — derivability determination and build spec (D278/D279)

Status: **IMPLEMENTED (D279).** The sixth seam `er_terminal_model_json` is
built and verified — debug + release 18/18, all 107 cases byte-exact under
canonical JSON plus `terminal_model_sha256`, two adversarial reviews APPROVE.
This document remains the record of the decisive boundary determination
(section 1) and the self-contained per-field build specification; section 13
keeps the handoff checkpoint for future maintainers. No open design question
remains; there is **no stop-and-ask gate** because no field consumes a
production render/layout/palette return value.

This is the sixth and final test-only ER seam in the D248→D279 chain. It does
not depend on the originating conversation; every law below carries either a
107-case census or a `file:line` to an executable oracle. Companion deep
catalog with per-probe evidence: `/tmp/d278_terminal_model_catalog.md`
(scratch; regenerable from this spec + the fixtures).

## 1. Derivability determination (the boundary question, settled)

**One-sentence conclusion:** all 14 `terminal_model` top-level fields —
including every one of the 24 entity-presentation and 18
relationship-presentation leaf fields (42 presentation leaves total), the
group presentation, 1764 captions and 871 notices — are deterministic
transforms of source facts, closed vocabularies, pure character-count text
metrics, or frozen constants; **none** needs a return value from a production
render/layout/palette function, so a pure parallel test seam regenerates all
107 models byte-for-byte with no shared render/palette contract change.

This determination is the principal output of the D278 read-only investigation.
The four sub-questions:

### 1.1 No pixel geometry anywhere

- Exhaustive numeric-leaf census across all 107 `terminal_model` trees: the
  only numeric leaves are provenance refs (byte/line/witness ordinals), role
  ints (1–6), categorical slots (0–11), insertion indices, 1/2 px stroke widths
  from a config law, **character-count** text metrics, positional ordinals and
  32-cell wrap offsets. A key-name regex sweep for
  `x|y|width|height|coord|pos|frame|rect|geo` returns only
  `minimum_field_content_widths` (character lengths) and
  `source_stroke_width_px` (1|2). No x/y/frame/rect/route field exists.
- Fixture self-declares `producer_or_pixels_read=false`,
  `geometry_goldens=false`, `status=AUTHOR_SOURCE_AND_STRUCTURAL_CHECK_ONLY`,
  and `shared_contract.geometry.coordinates="NOT_GOLDENS; ... no producer
  coordinates."`
- `RelationLayout`/`RelationBox{x,y,w,h}`, `RouteSearch`, `Canvas`, `ErPaint`
  outputs are consumed only by the physical frame tests; they are never
  serialized into the model.
- `minimum_field_content_widths` is the **max UTF-8 byte length of a column's
  display field across the entity's attributes** — the unpadded content input.
  Production frame width is a *different* formula `max(3, label_width+2)`
  (`mermaid_relations_core_impl.cpp:460`); the fixture deliberately omits the
  +2 padding and 3-cell minimum, proving it is not layout output.
- Relationship `start`/`end` are cardinality **marker enums**, not contact
  points; `source_cardA_right`/`source_cardB_left` are source enum names. Real
  bridge/contact geometry is test-private grid work represented in the model
  only by two frozen invariant strings (constants).
- `serialized_body_coordinates` is schema-typed `{"type":"null"}` (a null-only
  type, not a nullable union) and null in 107/107; `decode_semantics`
  hard-requires null (`mermaid_er107_authority.hpp:968`). Emit the constant key
  with null plus a fail-closed tripwire.

### 1.2 Hue / role / token fields need no production palette call

The categorical chain is fully reconstructible from source facts + closed
tables; the ER runtime never parses hex and only token **names** (never RGB)
are serialized.

1. Gate: `categorical = theme in {redux-color, redux-dark-color}`.
2. Slot: `source_getData_colorIndex % 12`; non-categorical ⇒ slot null and
   Neutral/role-1 on both planes.
3. Role/hue are compile-time constexpr tables in production
   (`mermaid_relations_core_impl.cpp:7301-7302` role arrays;
   `mermaid_er_impl.cpp:445-450` hue arrays); probe p26 proved the pinned
   palette's role/hue columns equal those arrays slot-for-slot. The test
   authority's `rgb_hue()` HSL classifier is independent evidence, not a
   production dependency.
4. Token names are a six-entry closed role→name map, structurally identical to
   production `style_role_color()` (`mermaid_impl.cpp:5958-5976`) and the fg/bg
   arrays (`:6605-6609`). Runtime `Palette` RGB in
   `src/ui/foundation/design_tokens.cpp` is never read.
5. redux-dark-color has no fill array ⇒ Neutral fill (36 entities), including
   the slot-5 Blue→Neutral dark rule (`mermaid_er_impl.cpp:460-461`).

**Build route:** embed the closed role/hue/token constexpr tables in the seam
(equal to production's). Do NOT read the 12-slot RGB asset and do NOT call
Palette — that keeps the seam source-only. If independent HSL re-derivation is
ever wanted, the palette asset would have to be passed as an explicit input; it
is not needed for byte-exact parity.

### 1.3 Notices are a closed 10-kind catalog, fully source-triggered

871 notices, uniform 6-key shape
`{kind, ordinal, rows, source_refs, style, text}`, constant style
`{background:"inherited", bold:false, foreground_token:"text"}`. Each kind has
a deterministic source trigger and a fixed emission order (section 9). No
render-time state. Production's pure twin `er_projection_notices`
(`mermaid_er_impl.cpp:386-473`) builds the same strings from the parsed diagram
only.

### 1.4 Caption wrap is pure text, owned by the test authority

1764 captions, uniform 6-key shape `{display, handle, raw, rows, rule,
source_refs}`. Wrap budget is the contract constant 32 cells; the rule is a pure
function of the display string (section 8), implemented in the test authority
`wrap()`/`checked_rows()` (`mermaid_er107_authority.hpp:791-838`) with a
production-identical twin `er_projection_wrap` (`mermaid_er_impl.cpp:358-384`).
No frame width is consulted.

Census basis: 107 cases, 362 entity presentations, 253 relationship
presentations (14 relationship-empty cases — ordinals 41-44, 53-56, 96-101),
1764 captions, 871 notices (1592 wrapped rows), 1 group presentation (case 102
only), 287 typed attributes, 5885 source refs (all resolve into the companion
source fixture's witness set with matching byte spans). Field-count
correction: the brief said 19 relationship fields; the actual union and the
JSON-schema `required` list both contain **18**. All items carry the full
required key set (zero missing across 362/253).

## 2. Asset and target

- Terminal fixture:
  `tests/fixtures/mermaid_native/er107_terminal_projection_proposal.v1.json`,
  `cases[].terminal_model` and `cases[].terminal_model_sha256` (107 cases).
- Source facts fixture:
  `tests/fixtures/mermaid_native/er107_complete_source_proposal.v1.json`
  (source_model, source_witnesses) — the same inputs the source_model/binding
  seams consume.
- Originals: `tests/fixtures/mermaid_official/cases/`.
- Target: native gtest in
  `tests/ui/test_mermaid_er107_emitter_contract.cpp` that canonical-encodes
  emitted vs pinned and requires the canonical strings byte-equal AND
  `sha256(canonical) == terminal_model_sha256` for all 107, with roster
  tripwires (section 12). No source-text/log pattern matching.

## 3. Reuse inventory (do not rebuild)

Committed test-only seams behind the `loom.ui.visual.mermaid.testing` facade
(`src/ui/visual/mermaid_testing_seams.cpp`), all pure source→JSON with `Work`
metering, no fs/env/theme access:

1. `er_src_collect_all(source)` → envelope, physical line count, `SrcModelData`
   (entities/relationships/attributes/metadata/statements/subgraphs +
   `body_witness_count`); `er_src_assemble(...)` builds source_model.
2. `er_src_make_ref(doc, source, start, end, witness)` — the 6-field source ref
   builder (`mermaid_relations_core_impl.cpp:5165-5176`); reuse for every
   terminal `source_refs` array.
3. `er_src_canonical_quote` / `er_src_canonical_append` (bytewise-sorted-key
   canonical writer, compact, full key+value escaping, raw UTF-8, void).
4. `er_whole_scene_json` (`:3085-3204`) — already byte-exact on all 107 by SHA;
   embed its output object as `whole_scene_model`.
5. `er_source_admit_envelope(source)` — single frontmatter/erDiagram admission;
   reuse for fail-closed on malformed/non-ER input.
6. Executable law reference (test support, read-only oracle — mirror the logic,
   do not link): `tests/support/mermaid_er107_authority.hpp` — `wrap`
   (791-811), `checked_rows` (813-838), `generic_display` (840-870),
   `role_display` (872-878), `marker` (889-895), `hue_role` (897-905),
   `rgb_hue` (907-936), and the full `decode_semantics` (955-1248).

## 4. Top-level 14 fields

| field | law |
|---|---|
| `schema` | const `"er107_terminal_model/v1"` |
| `review_status` | const `"UNREVIEWED"` |
| `drawing_credit` | const 0 |
| `serialized_body_coordinates` | const null (schema null-only; add tripwire) |
| `foreign_assertions_pending` | const true |
| `geometry_constraints_ref` | const `"er107_independent_geometry_constraints/v1"` |
| `style_constraints_ref` | const `"er107_complete_style_plane_constraints/v1"` |
| `whole_scene_model` | output of existing whole-scene seam (6 keys) |
| `entity_presentations` | 24-field objects, section 5 (362) |
| `relationship_presentations` | 18-field objects, section 6 (253) |
| `group_presentations` | 12-field object, section 7 (case 102; else []) |
| `captions` | 6-field objects, section 8 (1764) |
| `notices` | 6-field objects, section 9 (871) |
| `metadata` | 7-key object, section 10 |

## 5. `entity_presentations` (24 fields, 362 objects)

Identity / provenance:
- `id` = source entity id (unique per case).
- `owner` = source `source_parent_subgraph` exactly (null or group id); census
  360 null, 2 `"customers"` (case 102).
- `source_color_index` = positional entity index = source
  `source_getData_colorIndex` (0..N-1).
- `source_insert_order` = source `first_insert_order`.
- `alias_attempts` = verbatim copy of source `alias_attempts` (20 non-empty,
  342 empty). Header label = first non-empty alias else id.
- `source_refs` = always exactly **2** six-key refs to the id/label witness
  (entity_id or relationship_entity_reference), via `er_src_make_ref`.

Text / table metrics (pure character math, not frame geometry):
- `header_rows` = `wrap(label)` under 32; all 362 single-row in this cohort.
- `typed_attributes` (287 entries; 117 entities have typed tables), per source
  attribute j: `raw_type` = type literal; generic `type~T~` → `type<T>` via
  `generic_display`; name/comment/key facts verbatim; `ordinal` = local j;
  `source_attribute_ordinal` = source global attribute ordinal; own
  source_refs.
- `table_columns` = ordered non-blank field union in fixed order
  `type,name,keys,comment`. Census: [] 245; (type,name) 63; +keys 26; +comment
  8; +both 20. Mirrors the role-presence test in production `er_table_geometry`
  (`mermaid_relations_core_impl.cpp:444-450`).
- `minimum_field_content_widths` = map column → max character length of that
  column's display field across the entity's attributes (unpadded). 117
  non-empty (type 117, name 117, keys 46, comment 28).
- `attribute_row_heights` = wrapped-row count per attribute; all 287 are 1.
- `bold` = const false.

Hue / role / token (closed tables; categorical only when theme ∈
{redux-color, redux-dark-color}, else slot=null + Neutral/role-1):
- `categorical_slot` = `source_color_index % 12` (218 set, 144 null).
- `border_role`/`fill_role` ∈ 1..6 via `hue_role`; `border_hue`/`fill_hue`
  ∈ {Neutral,Purple,Cyan,Orange,Green,Red,Blue,Yellow}. Hue census — border:
  Neutral 144, Purple 80, Cyan 79, Orange 33, Green 14, Red 8, Blue 2, Yellow 2;
  fill: Neutral 180 (144 non-cat + 36 dark), Cyan 67, Purple 62, Orange 27,
  Green 11, Blue 8, Red 6, Yellow 1. Role census — border 1:144, 2:8, 3:35,
  4:14, 5:81, 6:80; fill 1:180, 2:6, 3:28, 4:11, 5:75, 6:62.
- role→token closed maps — border: 1 text, 2 danger, 3 warning, 4 success,
  5 info, 6 accent_purple; fill: 1 surface_tint_2, 2 danger_tint_bg,
  3 warning_tint_bg, 4 success_tint_bg, 5 info_tint_bg, 6 surface_tint_4.
  Token census — border: text 144, info 81, accent_purple 80, warning 35,
  success 14, danger 8; fill: surface_tint_2 180, info_tint_bg 75,
  surface_tint_4 62, warning_tint_bg 28, success_tint_bg 11, danger_tint_bg 6.
- `text_foreground_token` = const `"text"`; `text_background` and
  `internal_rail_background` = const `"owning_entity_fill"`.
- `border_background_token` = `"surface_tint_4"` when owner present else
  `"surface_tint_2"` (360/2).
- `source_stroke_width_px` = `(look=="handDrawn" || theme=="neo-dark") ? 1 : 2`
  (2 ×238, 1 ×124).

## 6. `relationship_presentations` (18 fields, 253 objects)

- `order` = positional edge index (0..N-1).
- `source` = source `entityA`; `target` = source `entityB`.
- `raw_role` = verbatim `role_literal` (keep all-space).
- `display_role` = `role_display` — strips outer horizontal ASCII space only;
  3 blank (case 13).
- `blank_role` = `display_role.empty()` (true ×3, false ×250).
- `caption_rows` = `wrap(display_role)`; blank ⇒ `[]`. 12 multi-row cases
  (57-62), split at 32 bytes.
- `start` = `marker(cardB=left)`, `end` = `marker(cardA=right)` via closed
  4-map. Marker values: `card:one`, `card:one-many`, `card:zero-many`,
  `card:zero-one`. `start` census: card:one 201, card:one-many 33,
  card:zero-many 18, card:zero-one 1. `end` census: card:one-many 116,
  card:zero-many 108, card:zero-one 14, card:one 15.
- `source_cardB_left`/`source_cardA_right` = raw source enums verbatim:
  ONLY_ONE, ONE_OR_MORE, ZERO_OR_MORE, ZERO_OR_ONE. cardB census: ONLY_ONE 201,
  ONE_OR_MORE 33, ZERO_OR_MORE 18, ZERO_OR_ONE 1; cardA: ONE_OR_MORE 116,
  ZERO_OR_MORE 108, ZERO_OR_ONE 14, ONLY_ONE 15.
- `stroke`: source solid→`solid`, anything else (only `dashed` observed)→
  `dotted` (solid 199, dotted 54). Note the terminal vocabulary rename.
- Constants (all 253 identical): `border_contact` =
  `"PRESERVE_ENTITY_BORDER_ROLE"`; `label_clearance` =
  `"OWNED_U0020_NEUTRAL_PADDING_WITH_MATCHED_SAME_RELATIONSHIP_SHAFT_CONTACTS"`;
  `external_background` = `"containing_group_surface_or_root_surface"`;
  `external_foreground_token` = `"text"`; `label_background_token` =
  `"surface_tint_2"`.
- `source_refs` = union of the edge's source witnesses (entity A/B, card A/B,
  role lexeme/content); ~7/edge in case 0, 1771 ref objects total.

## 7. `group_presentations` (12 fields; only case 102, else [])

`id` = source subgraph id (`"customers"`); `parent` = null (top-level; whole-
scene parent serializes as `""` but terminal group parent is null); `members` =
`["CUSTOMER","ORDER"]` verbatim source order; `header_rows` =
`[source_title_before_sanitize]` (`["customers"]`); `source_refs` = 3 six-key
refs; `source_stroke_width_px` = same look/theme law (1, handDrawn); `bold` =
false; `consumes_categorical_index` = false (groups never increment entity
colorIndex); `border_background` = `"parent_surface"`;
`border_foreground_token` = `"text"`; `caption_background_token` =
`"surface_tint_4"`; `interior_background_token` = `"surface_tint_4"`.

## 8. `captions` (6 fields, 1764) and the wrap law

Shape `{display, handle, raw, rows, rule, source_refs}`. Closed handle scheme:
`entity/<id>/header`; `entity/<id>/attribute/<j>/{type,name,keys,comment}`;
`edge/<i>/role`; `group/<id>/header`.

`rule` closed vocabulary (5 values) with census:
- `FIRST_NONEMPTY_ALIAS_ELSE_ID` (362) — entity headers; first non-empty alias
  else id.
- `ASCII_LITERAL_NO_INVENTED_KEYS` (1138) — attribute fields except converted
  generics; literal spelling.
- `BALANCED_TILDE_GENERIC_THEN_ESCAPED_TEXT` (10) — generic `type~T~` →
  `type<T>` via `generic_display`.
- `OUTER_ASCII_HORIZONTAL_SPACE_REMOVED_BLANK_HAS_NO_WORDS` (253) — edge roles;
  `role_display` outer-trim; all-space ⇒ empty display (3 blanks).
- `SAFE_ASCII_GROUP_TITLE_PROPOSED_IDENTITY_SANITIZATION` (1) — group header;
  source title unchanged in this ASCII cohort.

Wrap law (budget 32 bytes = `shared_contract.bounds.wrap_cells`), implement as
one pure function:
- while more than 32 bytes remain: break at the last U+0020 SPACE at a positive
  offset ≤32, consuming that one space as joiner `" "`; if no such space,
  hard-break at 32 whole bytes with joiner `""`; no trim/collapse.
- preflight requires printable ASCII (32-126).
- each row records `display_byte_start`, `display_byte_end` (text bytes only)
  and `consumed_byte_end` (including joiner); concatenation of text+joiner
  reconstructs `display` exactly.
- empty display ⇒ one empty row (owns no visible words).

Only 12 caption rowsets actually wrap (34-35-byte role strings, cases 57-62,
hard splits); notices show 721 space-joiners vs zero in captions. Because the
corpus mostly fits one row, it cannot exercise wrapping (Rule 30) — add
synthetic unit cases for both the >32-byte hard break and the space break, plus
the empty-display row.

## 9. `notices` (6 fields, 871) — 10 closed kinds, fixed order

Shape `{kind, ordinal, rows, source_refs, style, text}`; `style` const
`{background:"inherited", bold:false, foreground_token:"text"}`; `text` =
concat of `rows[].text + rows[].joiner`; `ordinal` = position in the fixed
emission sequence; rows use the same 32-cell wrap. Emission order is fixed:

1. `profile` (107) — const `"ER native projection v1"`; no refs.
2. `authored_config` (181) — one per ordered config leaf,
   `"path=" + canonical(value)`; refs present (key+value witnesses). Leaves:
   look 86, theme 54, htmlLabels 30, layout 10, er.useMaxWidth 1.
3. `effective_defaults` (107) — `"effective theme=...; look=..."` from the
   fresh-owner policy; no refs.
4. `look_adaptation` (107) — `"look=... -> native orthogonal; ..."`; no refs.
5. `layout_adaptation` (10) — only if `config.layout` present; no refs.
6. `html_labels_adaptation` (30) — only if `config.htmlLabels` present; no refs.
7. `svg_viewport_adaptation` (1) — only if `config.er.useMaxWidth` present; no
   refs.
8. `foreign_theme_debt` (107) — const
   `"theme RGB/font/CSS -> terminal roles; foreign assertions pending"`; no
   refs.
9. `categorical_slot` (218) — one per categorical-theme non-group entity:
   `"entity ID slot=N border=H fill=H"`; refs present (entity id witness pair).
10. `blank_role` (3) — one per all-space edge role, case 13 edges 0-2; refs
    present (role witnesses).

30 distinct ordinal→kind sequences exist cohort-wide, each fully explained by
which leaves/slots/blanks the source contains. Mirror `decode_semantics`
(`:1201-1246`) and production `er_projection_notices` (`mermaid_er_impl.cpp:
386-473`).

## 10. `metadata` (7 keys, identical key set all 107)

- `authored_config` — exact copy of source_model `metadata.authored_config`.
- `ordered_config_leaves` — exact copy of source
  `metadata.ordered_config_leaves` (181 leaves cohort-wide).
- `description` — source 6 fields (key/value witness, path, source_value,
  yaml_style/tag) plus const `visible=false` and a synthesized source_ref from
  the value-witness span.
- `title` — const null (title absent in all 107).
- `source_effective_config_remains_unknown` — const null (source
  `effective_config` null in all 107).
- `proposed_fresh_owner_effective` — theme = authored `config.theme` else pinned
  `redux-color`; look = authored `config.look` else pinned `neo`;
  `categorical = theme in {redux-color, redux-dark-color}`; per-leaf origin
  `"authored"` vs `"pinned_ER_diagram_default"`; const
  `host_initialize_reproduced=false`.
- `config_obligations` — one per source `config_semantic_obligations` (181):
  path/source_value/witness; `foreign_semantics` = source semantic effect text;
  disposition const `PROPOSED_EXPLICIT_LOSSY_TERMINAL_ADAPTATION` (181/181).

Terminal metadata deliberately differs from source metadata: it drops
`effective_config/init_directives/namespaces` and adds the fresh-owner policy
and obligations.

## 11. Construction and guardrails

- New seam `er_terminal_model_json(source, work, provenance)` in
  `mermaid_relations_core_impl.cpp`, plus the same four src touch points as
  prior seams: detail declaration (`mermaid_detail.cppm`), testing export
  (`mermaid.cppm`), thin forwarder (`mermaid_testing_seams.cpp`). No CMake
  change (existing test target).
- Spend-first `Work` metering (e.g. `size*16 + 32768`), return nullopt on
  exhaustion; reuse `er_source_admit_envelope` for fail-closed malformed/non-ER.
- Canonical compare via the test oracle `class481_canonical` + sha256; object
  key order irrelevant, array order load-bearing. Add anti-silence tests for
  the 32-wrap and any naturally-sorted subtree (Rules 31/32).
- `-Werror` clean (`-Wall -Wextra -Wpedantic`); no hardcoded RGB (closed
  tables only); production `ErCursor`/`parse_plain_er` and every production
  call path untouched.

## 12. Acceptance criteria

- Debug + release emitter binary green; zero warnings in own files.
- 107/107 canonical equality AND `terminal_model_sha256` match.
- Roster tripwires: 362 entity / 253 relationship / 1 group presentations;
  1764 captions; 871 notices; 287 typed attributes; 14 relationship-empty
  cases (41-44, 53-56, 96-101); null-only coordinate key.
- Fail-closed battery (malformed/non-ER, zero `Work` budget).
- Synthetic wrap unit cases (hard break, space break, empty display).
- Adjacent cluster 291/291 (relations_semantics 269).
- Two adversarial review rounds; frozen gates unmoved (2145/2738, 48-gap,
  Class285 260/25/0, ER107=107, 0 matrix error).
- Full matrix / ASan / serial gates run once at this family close (deferred per
  directive).
- Local commit only; no push/merge.

Residual out-of-scope (separate physical-frame track): browser/RGB/CSS foreign
debt, the 15-capture Official frame verification, non-ASCII width behavior.

## 13. Checkpoint and first command (handoff contingency)

If the in-session build is interrupted, the chain is at a clean checkpoint:
commits D251 `4f5d43fe`, D271 `01688c3a`, D275 `2f3329d0` (local, not pushed),
plus this spec's own docs commit. No terminal_model code is required to exist
for the tree to build — the seam is additive. A new owner's first command:

```bash
git log --oneline -4 && cmake --workflow --preset dev
```

then read this spec end-to-end and `tests/support/mermaid_er107_authority.hpp`
(`decode_semantics` is the executable law catalog), and start from the existing
`er_source_binding_json` seam as the structural template.

## 14. Prior chain / context

Memory: `d251-er-semantic-lexer.md`, `d271-er-source-model.md`,
`d275-er-source-binding.md`. Efficiency rules in
`docs/dev/engineering-efficiency.md`: Rule 30 (corpus-exact ≠
grammar-faithful), Rule 31 (spec/canonical-harness first; shared helper at
third consumer), Rule 32 (explicit provenance + anti-silence tests; clean-tree
recovery). Two perf docs (`engineering-efficiency.md`,
`mermaid-d117-family-fallback-counts.md`) remain uncommitted by directive and
must not be swept into this spec's commit.
