# ER107 complete-original terminal projection proposal

Status: **DRAFT — B1 source approved; B2 design pending; no production admission**.
Owner: `loom.ui.visual.mermaid`, specifically ER metadata and ER layout output.
Parent: RFC 0030, provisional above-90-percent complete-original milestone.

## Intended result and fixed boundaries

The current admitted baseline is 688/2145. The user milestone remains at least
1931/2145, not completion of this candidate. ER107 is the 107-source candidate
selected from the immutable ER206 authority where `whole_model` is null and the
set of `whole_null_debts.kind` is exactly `{active_config}`. This historical debt
classification is a selection input, not proof that every body or configuration
can be rendered. Independently owned full-source facts must establish that.

Frozen ER206 authority SHA-256:
`76d54ecbc725e59645501f7079f9387b272172d584703f0ea72f143c83c469db`.
The candidate path/source-digest lock is
`edbf30ae159e9e9b2bb3ff6a16f74ff0a75df18fb45520627425810c99aa71f1`
using sorted relative cases paths plus NUL, ASCII SHA-256 and LF. The candidate
contains 43,704 raw bytes. The 107 count earns zero new drawing credit.

Do not edit the frozen ER206 authority, its 62 admitted profiles, upstream raw
sources, manifest expectations or prior consumer evidence. New facts belong in
an independent sidecar. The fixed denominator remains 2145; body 593 is separate.
Unknown, partially projected or over-budget sources stay whole source and earn
zero credit. No commit or push is authorized.

## Source-only candidate submitted for B1 review

The new `tests/fixtures/mermaid_native/er107_complete_source_proposal.v1.json`
contains 3,743,357 bytes, raw SHA-256
`5a1c642cc7a3e2cf08e00355be845711520f762fc999dc13fe7dbb4a417297f3`.
Ramanujan authored it independently without producer or frame expectations.
Harvey independently approved B1 at the exact SHA above after raw re-parsing,
all witness/config/hash checks and fixed-commit upstream excerpt verification.
The immutable proposal retains its original UNREVIEWED literal; this external
receipt approves source facts only. Its terminal Models remain null. An authoring self-check is not a native runtime
or admission test.

The candidate contains 6,616 byte/line witnesses, 362 entity identities,
287 attributes, 253 relationships, one subgraph and four authored directions.
There are 31 distinct exact bodies. Config has 181 leaves: 86 look leaves
(32 handDrawn, 54 neo), 54 theme leaves (nine each of neo, neo-dark, redux,
redux-dark, redux-color and redux-dark-color), 30 htmlLabels=false leaves,
ten layout=elk leaves and one er.useMaxWidth=false leaf.

Source metadata includes 103 folded `>-` descriptions and four plain descriptions;
all 107 titles are absent. YAML AST style `>` names the folded category, not the
complete raw chomping indicator. Direct raw-byte inspection confirms `>-`, which
existing ER code already supports. Preserve and test that metadata ownership;
no new description syntax is justified by this cohort. Entity and
relationship strings are ASCII here, but empty, one-space and two-space roles
remain distinct complete-source facts. Eight authored attribute names begin
with `*`; that lexeme does not invent a PK key.

## Current implementation evidence

`er_source_envelope` in `src/ui/visual/mermaid_er_impl.cpp` owns ER configuration
validation. Its current closed top-level profile includes `layout: elk` and
`theme: forest`; it has no general `look`, ER `htmlLabels` or `useMaxWidth`
profile. Nested accepted namespaces are limited to `flowchart` and
`themeVariables`. A whitelist extension alone cannot establish faithful
presentation semantics.

`parse_relations` in `mermaid_relations_core_impl.cpp` invokes that owner for
frontmatter/init input and passes the unchanged complete body to `parse_plain_er`.
The existing private `relations_layout` builds source-owned entity/relationship
geometry, paints it and returns text rows. Class, State and Requirement share
that row-returning entry point. The public dispatch currently wraps those rows
in the private `LayoutPayload` with an empty role plane. Block already uses a
nonempty private role plane; global Canvas and theme policy need no change.

An ER presentation profile must retain entity, attribute, relationship, group,
label and metadata ownership through layout. Reconstructing ownership by parsing
finished glyph rows, reading producer pixels into expectations, stripping config
and admitting body-only output, or relying on non-failing capability reports is
not an acceptable integration path.

## Design decisions that must close before production work

1. Bind the independently authored candidate sidecar and review every raw config
   leaf, value, precedence rule and byte witness. Preserve original metadata and
   foreign assertion debt. Existing whole-null debt alone is insufficient.
2. Define the terminal projection of every selected theme/look/layout/htmlLabels/
   useMaxWidth value using pinned upstream source. Distinguish actual terminal
   colors/geometry, explicit bounded projection notices and foreign SVG/CSS/font/
   browser obligations. No value may become accepted solely by being ignored.
3. Specify complete ER entity/table/attribute/key/relationship/cardinality/direction/
   group semantics and independent geometry constraints. Any missing body form
   remains an explicit candidate failure, not a silent reduction of the cohort.
4. Compare an ER-local private entry/output with the smallest extension of the
   existing private relation layout. Preserve all legacy callers and their
   source behavior. A proposed shared internal change must explain why local
   composition cannot preserve source-owned geometry cleanly. Avoid a global
   Canvas change, pixel-parsing overlay, duplicate parser/layout or generic new
   renderer abstraction.
5. Prepay new parsing, metadata, semantic planes, serialization and fresh FTXUI
   nodes with the same live Work. Keep the 1,000,000 cap and 240x1024 canvas limits.
   Specify atomic whole-source rejection and exact narrow fallback/cache behavior.

These are open design gates, not an implemented or approved configuration profile.
The source author's work is independent of production code and render output.

## Required stage evidence

B1: independent 107-source raw/manifest/model/config ownership, complete byte
witnesses, immutable source pins and independent review. No terminal art claim.

B2: a complete source-derived terminal contract with exact theme/role/notice
mapping, geometry and route constraints, primitive/scene boundaries, Work and
cache costs, legacy retention and alternatives. Independent design/production-
readiness review must approve the selected integration before production edits.

B3: Native and Official each own uncached, miss and hit captures for wide, narrow,
restored and ordinary-source controls before model binding; validate complete
planes, exact source fallback or complete geometry, source locks and useful
semantic/physical good-first mutants. Neither consumer borrows the other's
frames or uses producer art to derive expected geometry. The current source-only
preflight `verify_roots` performs whole-asset component validation and is not a
capture-order protocol. A runtime consumer must defer semantic/component binding
until its own captures; only raw asset identity checks may precede them. Adapt
the staging API or perform final global checks after all owned captures rather
than reusing the preflight constructor as an admission shortcut. Foreign debt stays
explicit. Debug and Release proofs must agree.

B4: both full builds, complete Mermaid-labelled regressions including PTY,
independent implementation/admission review, strict fresh dual-consumer join,
and exact preservation of all prior 688 admitted originals and 593 body rows.
A local 460-file preservation snapshot records prior hashes; it is
only change-accounting evidence, not a source authority or test substitute.
Formal credit requires the actual admitted outcomes, never a proposed 107 gain.

## Proposed private integration for B2 review

Harvey's independent architecture lens recommends moving the existing private
payload boundary into `relations_layout` rather than creating another ER entry
or adding an optional side output. This recommendation does not approve B2.

Proposed private signature:

```cpp
std::optional<LayoutPayload> relations_layout(
    std::string_view source, Work& work,
    bool class_envelope = false, bool requirement_input = false);
```

Production currently has one dispatch consumer. The fuzz consumer discards the
result. Class, State, Requirement and legacy ER would return the identical rows
with an empty role plane. Dispatch would return that payload directly, moving
its current wrapper without adding a new public API. The cache and fresh finish
already accept this private payload and need no change.

ER envelope validation must run once, retain optional ER-only presentation facts
and move them through the same parsed scene. No attempt-old-then-reparse-new
fallback. New dynamic metadata/planes must allocate only when the new profile
is active. Entity/group/table ownership and source insertion order still exist
before serialization; attach roles there. Committed paths, ports and label
rectangles are available while painting a successful route; failed route
candidates must leave no roles behind. Preserve raw whitespace-only relationship
facts during parsing when the projection needs a disclosure.

The ER-only serializer must use the agreed full layout extent for rows and
roles. `Canvas::lines()` trims based on nonblank glyphs; leave its legacy path
unchanged rather than trimming new rows and guessing a plane origin. Do not
change global Canvas, the palette provider, cache storage policy or public API.
This is a justified change inside the owning Mermaid module: an envelope-only
change cannot preserve geometry ownership that exists only in relation core.

Before implementation, measure all affected constructor/move/return costs and
prove the existing admission paths retain their behavior and bounded Work.
The new profile prepays metadata, semantic planes, serialization and fresh
nodes with the original live counter; no reset or cap increase. An unchanged
legacy glyph result is necessary but not sufficient if its Work path changes.

## Confirmed upstream presentation inputs; projection still open

All following source observations are pinned to commit
`1fb7869866dd619168d840291c9c54b4f129758b`; no JavaScript renderer was executed.
They complement B1. The new source-only
`er107_presentation_source_audit.v1.json` binds17 fixed-commit files and49 exact
excerpts,72,136 bytes, raw SHA-256
`8f35938eb74591a428823aae3c1279c4a333fa8240b7d92181e77e7f22ed18ba`.
Each cached file Git blob and size matches the fixed-commit recursive listing;
Harvey independently approved source-evidence provenance after fetching all17
raw files and verifying49 excerpts and canonical hashes. Cached recursive-tree
reconstruction yields actual root171013601d2b707e35af79142d4c605a21fcbe82;
the API response records requested commit1fb7869 rather than that root. Online
tree recheck was rate-limited403; cached subtree reconstruction and independent
raw fetches provide the stated bounded evidence. This is not a terminal Model
or B2 approval. The B1 source proposal remains unchanged.

- `config.ts` resolves frontmatter/directive, initialize, diagram default and
  global default in that order; same-layer diagram-scoped appearance takes
  precedence. Theme-variable regeneration follows the resolved name.
- ER schema defaults are `theme: redux-color` and `look: neo`. The53 sources
  without authored theme cannot automatically be treated as noncategorical.
  Actual upstream host initialize values remain separate. The proposed Loom
  fresh-owner default must be explicit; it cannot claim to reproduce an unknown
  browser harness configuration.
- ER assigns categorical index to non-group entities in source insertion order.
  `erBox` stamps slots for redux-color/redux-dark-color. Styles additionally
  require a nonempty border array; background palette length is independent.
  The four-redux style branch is not this two-theme categorical gate. The
  two pinned border arrays have12 entries; redux-dark-color background is empty.
- `look: handDrawn` changes marker/background rendering. Fixed-cell terminal
  disclosure and any contour/stroke adaptation must be defined before acceptance;
  merely discarding the value is not a profile.
- `htmlLabels: false` does not mean plain text. `createText` defaults to Markdown;
  SVG labels still use Markdown tokenization. Whitespace-only roles produce no
  visible words, while their source bytes stay distinct.
- ER attribute labels first pass `parseGenericTypes`; for `type~T~` this creates
  `type<T>`, then angle brackets are entity-escaped before text rendering. The
  terminal display needs that conversion, not a raw tilde-containing type.
- `er.useMaxWidth` controls SVG width/max-width attributes. It is not a terminal
  layout width or permission to resize geometry to fit.

Remaining B2 decisions include effective fresh-owner defaults, all per-theme
border/fill/row/label roles, look and SVG-viewport disclosures, precise label
normalization/generic limits, layout=elk adaptation, complete independent
geometry and blank-fill bounds, raw blank-role disclosure, fixed cost formulas
and non-vacuous mutations. No production edit is authorized by this draft.

## Proposed presentation contract — not yet approved

The new profile is activated only by source configuration that the legacy ER
profile cannot represent. Its closed leaves are global `look` (neo/handDrawn),
`theme` (the six recorded themes), `htmlLabels=false`, `layout=elk`, and
`er.useMaxWidth=false`. Unknown keys/values, duplicates, empty namespaces and
mixed legacy-only presentation options reject atomically. Parse once and decide
one profile; rejection never retries a body suffix. Legacy accepted inputs keep
their existing terminal contract pending a separately reviewed migration.

For this proposed fresh-owner profile, absent theme/look resolve to the pinned
ER diagram defaults redux-color/neo. Loom has no Mermaid `initialize()` host
layer; external browser initialization remains an explicit foreign obligation.
These defaults are a terminal profile choice, not a claim about an unknown
upstream test harness. Under that choice,71 of107 sources are categorical
(53 implicit plus18 explicitly color-themed);36 are noncategorical. Source
facts and original host predicates remain unchanged.

For categorical inputs, non-group entity insertion order selects12 border slots.
Use the same reviewed RGB-to-palette hue thresholds as RFC0030's private role
encoding, independently bound to ER's arrays and index ownership. Proposed
foreground roles are `[6,5,3,5,4,6,2,3,5,4,5,2]`; light fill roles are
`[6,5,3,5,4,5,2,3,5,4,5,2]`. This retains light slot5's different fill/border
hues. Dark-color has no categorical background array: retain neutral surface
fill, never modulo an empty array. Entity borders and table rails use the entity
border role; readable text uses text role. Blank interior cells carry their fill.
Groups use neutral border and group surface, do not consume categorical indices,
and retain complete source membership. Connected border ports retain the entity
border role; external stems, crow's-foot markers and labels use neutral text.

Noncategorical theme/row-fill mapping still needs its exact source-bound table;
this paragraph does not approve one. Rounded/rough geometry, source font weights
and exact SVG RGB values are not inferred from role equivalence.

Proposed label rules are limited to the recorded ASCII subset: plain entity
aliases, types, names, keys and comments; a complete raw type matching ASCII
`Identifier~Identifier~` converts to `Identifier<Identifier>`; unsupported Markdown/HTML/sanitizer cases reject
whole source. Preserve leading asterisks as names, not invented PK keys. Blank
relationship roles display no words, consistent with Markdown tokenization;
the distinct raw empty/one-space/two-space strings must remain bound and receive
an exact visible disclosure. Later aliases cannot replace a first nonempty
alias, attribute blocks append, cardinalities retain left/right orientation and
all source relationships remain required.

Proposed notices follow the drawing and one blank separator, in source-defined
metadata order: authored config leaves, effective-default summary, lossy
theme-slot legend and blank-role disclosures. Description remains validated,
source-bound documentation metadata and is never rendered, preserving the
existing decision-register contract. The exact text
catalog and wrapping/joiner law remain to be frozen; use bounded32-cell wrapping
rather than forcing every simple scene wider than a40-column viewport. Notices
must explicitly identify native orthogonal adaptation of look/layout and SVG
viewport options. They cannot certify the unavailable browser rendering engine,
roughness, font metrics, CSS or exact RGB. Removing/changing each required notice
must fail a physical test, not merely a source-hash check.

Complete entity/table/attribute geometry, group ownership, all endpoint marker
kinds, stroke identity, direction, route exclusion, label association and
non-overwrite constraints must use source-derived whole-scene predicates. Reuse
the established ER oracle mechanically with a new typed source projection;
never derive a terminal Model from production output. Scene extent/blank apron,
notice placement and row/plane serialization must be specified before B2.

New profile storage/loops are paid before allocation/iteration. Serialization
must pre-count actual UTF-8 bytes, full style cells, runs and rows, including blank
fill and notices. Reuse the private payload encoding and existing fresh-finish
node cost accounting, without double charging legacy row wrapping. Fixed local
state, failed route candidates and generic/description scratch are included.
A live same-counter regression must prove exact-budget success and short-by-one
rejection for generic non-scoring examples, plus actual107-source costs; an
over-budget source remains a recorded unmet candidate. No estimated cost is a
runtime success claim.

## B2 review candidate: remaining terminal laws

The following closes the proposed choices above for review; none is approved
until the complete independent source-derived terminal proposal and PRR pass.

All four noncategorical themes map node borders/rails/text to text role and
node interiors to surface_tint_2. No categorical index changes these36 sources.
Groups map to surface_tint_4 with text border/label. Background/row RGB shades
are deliberately coalesced into these declared terminal surfaces; no zebra-row
or exact RGB equivalence is claimed. The source theme name and this adaptation
remain visible. Every source/theme font and SVG/CSS predicate stays in foreign
debt. This rule is a fixed-cell projection, not a conclusion about unknown host
rowEven/rowOdd overrides. The new profile accepts no themeVariables overrides.

All new-profile glyphs have bold=false. For neo, resolve strokeWidth from the
selected source theme (including neo-dark=1px); the other recorded themes use
2px. The handDrawn ER style selects1px, and group borders select1px. Both1px
and2px quantize to the normal single-cell stroke; exact width stays foreign
debt. This is consistent with the reviewed private role encoding;
source font/roughness is disclosed rather than inventing bold. At inherited
Markdown dim=2, preserve that bit exactly. Hyperlinks and other attributes are
absent. All current application palettes/flags are selected by one fresh theme
snapshot; source theme never mutates the global provider.

The body canvas has neutral foreground/background (text/surface_tint_2), with
owned group/entity/table overrides. Source-owned rectangle/table geometry
selects roles before glyph serialization. Committed labels use the same neutral
text/background, including their blank clearance. Successful route/port style
updates cannot overwrite an entity's categorical border. Failed candidates
never leave style writes. A family-local optional paint context can supply
these hooks without changing global Canvas; its new storage is prepaid only
on activation.

For the proposed topology branch, step 8 below is the sole definition of label
crop bounds, output dimensions and apron. It must match `ErPaint::finish`; this
older summary does not redefine the per-label extent. Retain complete entity
interiors. Legacy serializers remain unchanged.

Append one empty separator row and the following ordered notice records. Every
record uses text role, inherited background and bold=false; wrap to32 cells by
last fitting U+0020, otherwise by whole glyph. Preserve every consumed row joiner
in the expected typed projection. No trim/collapse of a record's source-derived
payload is allowed. Body width and longest notice row jointly determine the
intrinsic width; each serialized row's role count equals its actual cell width.
No canvas-wide padding is silently added to shorter notices.

Description is bound as decoded documentation plus raw chomping/byte witnesses,
with visible=false. Do not emit a description notice or infer browser aria proof.
Malformed or unsupported description still rejects the complete input.

1. `ER native projection v1`
2. One `PATH=JSON_VALUE` for every authored config leaf in source order. PATH is
   dotted and starts with `config`; JSON strings are quoted, booleans lowercase.
3. `effective theme=THEME; look=LOOK` using the declared fresh-owner resolution.
4. `look=LOOK -> native orthogonal; roughness/relief not reproduced`
5. `layout=elk -> native directed routing`, only if that leaf is authored.
6. `htmlLabels=false -> native Markdown subset`, only if authored.
7. `er.useMaxWidth=false -> SVG viewport only; native width fallback unchanged`,
   only if authored.
8. `theme RGB/font/CSS -> terminal roles; foreign assertions pending`
9. For categorical entities in source non-group insertion order:
    `entity ID slot=N border=HUE fill=HUE`. N is zero-based modulo12. Source hue
    names use the reviewed hue classes; dark-color's absent background says
    `fill=Neutral`. Preserve authored identity rather than generated browser IDs.
10. For each whitespace-only role in source relationship order:
    `edge N raw-role=JSON_STRING -> no visible words`. N is zero-based; exact
    JSON strings are `""`, `" "`, or `"  "` for the recorded cases.

This catalog makes each lossy adaptation and authored option observable.
Positive unchanged records must pass before missing/changed/extra notice, joiner,
slot, raw blank-role and role-plane controls are rejected. Notices alone do not
prove entity/attribute/relationship geometry; the complete ER oracle still must
consume every source object and reject extra or misassociated ink.

The existing parser retains its raw source64KiB, 64 entity, 128 relationship,
group-depth16 and canvas240x1024 safety bounds. The proposed route branch has the
stricter eligibility limits in B2: source at most1024 bytes, at most10 entities,
at most8 relationships, and ASCII geometry labels with at most256 displayed
cells per label. Existing parsing/layout charges and the same caller-owned Work
remain additive. Presentation, paint state, planes, notices and any growable
container are charged before initialization/allocation/growth. The only output
crop and fee definitions are the exact B2 rules in steps 8 and the prepayment
table below; no other multiplier applies. Legacy fee preservation and107
same-factory feasibility must be demonstrated, not inferred from this proposal.


## B2 candidate: bounded source-topology ER route plan (2026-10-27)

Status: **design proposal only; independent B2 design/production-readiness review
is required before any production edit.** It does not change the fixed Work or
canvas limits and grants no drawing credit.

### Observed regression boundary

The complete Mermaid-labelled regression runs both execute 914 tests: Debug and
Release each pass 910 and fail the same four ER107 gates. The owner-private
same-factory layout test seam succeeds for 92/107 sources under its default
1,000,000 Work; only Native and Official captures prove physical drawing. The
15 source fallbacks group into four `a-not-so-simple` variants, six variants of
the same multiple-relationship graph, two categorical-theme variants of the
all-relationship-types graph, and three centered-edge-label variants. The
largest successful source consumes 957,407 Work. Thirteen failures exhaust the
default budget during routing; two finish routing but cannot pay the exact
remaining output fee. All15 return source fallback and receive zero drawing
credit. This is runtime evidence, not proof that all failures share one route
cause.

A size-neutral temporary diagnostic around the same factory seam localized all
15 default failures. Thirteen fail in A* `connect_relation` while processing
zero-based relationship index3: ten sources have7 entities/8 relationships
and three centered-label sources have5/5. Two all-relationship-types sources
have10/5 and finish routing, then fail the final output prepayment. For this
current A* diagnostic, `C` is endpoint-port pairs visited before failure, `D`
is successful route-segment cell visits, and `L` is committed label-clearance
cells. In the proposed router, `C` will instead count tested rail candidates;
those future costs are not inferred from the A* counts. `B` is UTF-8 output
bytes, `A` style cells, `R` style runs and `H` rows in the output fee:

| Pinned source path | Original manifest SHA-256 | Source config | Failure stage | N/E | A* C/D/L | Work remaining | B/A/R/H | Fee | Shortfall |
|---|---|---|---|---:|---:|---:|---|---:|---:|
| `er-diagram/visual-check/a-not-so-simple-er-diagram-handdrawn-look.mmd` | `b94dd9461cb48f79d7587949b2436a714d2eccf2b721b51fc653929dd29fee3a` | look=handDrawn | A* edge 3; exhausted | 7/8 | 96/328/171 | 0 | — | — | — |
| `er-diagram/visual-check/a-not-so-simple-er-diagram-without-htmllabels-default.mmd` | `d83bf9578ac1dc44f9a195fc3270a0b15f043c78e32a98aa3c9f7ecbb9233942` | htmlLabels=false | A* edge 3; exhausted | 7/8 | 96/328/171 | 0 | — | — | — |
| `er-diagram/visual-check/a-not-so-simple-er-diagram-without-htmllabels-elk-layout.mmd` | `9e67f0f8a3b1a8e8b140afb28533a87fca19de04e5ae8afbd321964fcb694e56` | layout=elk, htmlLabels=false | A* edge 3; exhausted | 7/8 | 96/328/171 | 0 | — | — | — |
| `er-diagram/visual-check/a-not-so-simple-er-diagram-without-htmllabels-handdrawn-look.mmd` | `8a28863a13d83071147ce74d0762549059c23f4bae7d84b076b2c7e3429d1216` | look=handDrawn, htmlLabels=false | A* edge 3; exhausted | 7/8 | 96/328/171 | 0 | — | — | — |
| `er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-neo-dark-theme.mmd` | `8eebc6f5211dd38fe9e07afa02b2ce13e57830763b122eab13c09dda235407ef` | look=neo, theme=neo-dark | A* edge 3; exhausted | 7/8 | 96/328/171 | 0 | — | — | — |
| `er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-neo-theme.mmd` | `1b3ddd1737548decf15c4377d9bbcd85f2adbe67dc5372bc74428c210789e779` | look=neo, theme=neo | A* edge 3; exhausted | 7/8 | 96/328/171 | 0 | — | — | — |
| `er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-redux-color-theme.mmd` | `0e85ccc2facc24e969b185e0fa03f5ac9ed0230c8c3303db19146b64ce8321f0` | look=neo, theme=redux-color | A* edge 3; exhausted | 7/8 | 96/328/171 | 0 | — | — | — |
| `er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-redux-dark-color-theme.mmd` | `e384f1691e146f9b8cc930571f5bffc1e23c855d374394f01136ba7a10af63ae` | look=neo, theme=redux-dark-color | A* edge 3; exhausted | 7/8 | 96/328/171 | 0 | — | — | — |
| `er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-redux-dark-theme.mmd` | `3208055893e92dfa6070688167e42c2897cae08e35735e2b47b1d36366a2e564` | look=neo, theme=redux-dark | A* edge 3; exhausted | 7/8 | 96/328/171 | 0 | — | — | — |
| `er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-redux-theme.mmd` | `e3c8b65b542f43477793534db3ee3ba3787cf3624a630e99837954e5d751fb52` | look=neo, theme=redux | A* edge 3; exhausted | 7/8 | 96/328/171 | 0 | — | — | — |
| `er-diagram/visual-check/er-diagram-with-all-relationship-types-neo-look-redux-color-theme.mmd` | `d8bcf1c47bd62c2e19dfbd2265c4785d2dfc010bf541a26e8bcdaf0df97041b3` | look=neo, theme=redux-color | output prepayment | 10/5 | 20/266/531 | 524735 | 6163/5317/232/153 | 599186 | 74451 |
| `er-diagram/visual-check/er-diagram-with-all-relationship-types-neo-look-redux-dark-color-theme.mmd` | `331fdbf10f0caab501c7bce3e25505fce25cf0785d758bc589846f701f3a826f` | look=neo, theme=redux-dark-color | output prepayment | 10/5 | 20/266/531 | 524056 | 6196/5350/232/153 | 601232 | 77176 |
| `er-diagram/visual-check/er-diagram-with-edge-labels-centered-when-htmllabels-is-false-default.mmd` | `d8d64660f3b9650744000e4c48bf2a60e62ca3e931e7552e0d95f18a70c9922f` | htmlLabels=false | A* edge 3; exhausted | 5/5 | 59/382/153 | 0 | — | — | — |
| `er-diagram/visual-check/er-diagram-with-edge-labels-centered-when-htmllabels-is-false-elk-layout.mmd` | `c1f5f2812d4bd69c5e0b2ffc5b83091ef780d8e5d6f41f5d4e2076c6804a4f22` | layout=elk, htmlLabels=false | A* edge 3; exhausted | 5/5 | 59/382/153 | 0 | — | — | — |
| `er-diagram/visual-check/er-diagram-with-edge-labels-centered-when-htmllabels-is-false-handdrawn-look.mmd` | `9d69b6885d9b2d5fd122f380c2b61861204ddf861d6fb5dbf0835c16268d821b` | look=handDrawn, htmlLabels=false | A* edge 3; exhausted | 5/5 | 59/382/153 | 0 | — | — | — |

These A* counters came from size-neutral temporary instrumentation and the
production source was restored and rebuilt afterward. C is endpoint-port pairs
visited, D is successful route-segment cell visits and L is committed label
clearance cells before failure. Work remaining is the counter immediately
before the unchanged output fee; shortfall is fee minus remaining. The two
categorical cases need a compact layout that reduces actual B/A/R/H under the
same fee; no fee may be discounted.

The failing sources reduce to three distinct topologies: a seven-entity DAG
with long-span edges, a ten-entity set of independent relationship pairs, and a
five-entity DAG with parallel `CUSTOMER` to `ADDRESS` edges. Theme and metadata
variants remain separate complete sources. No source path, theme name or
fixture ordinal may select a route algorithm.

### Proposed ER-local branch for bounded source-topology layouts

This branch is intended to address all15 measured ER107 failures: 13 exhaust
the current A* connector route and two reach the final output prepayment but
cannot afford it. The same source-neutral compact layout must reduce the latter
two's actual output dimensions enough to pay the unchanged fee. Its eligibility
predicate also selects all six 10/5 theme variants, including four that drew
before this change. Those six are one source-derived topology cohort: the
bounded layout deliberately changes their cell coordinates, so acceptance
preserves complete graph semantics, theme roles and valid geometry rather than
requiring byte-identical old frames. Enumerate every already-successful source
selected by the predicate before implementation; re-capture and validate each
one against its complete source-bound role/geometry contract. All non-selected
successes must retain their existing whole-scene results. Full ER107 admission
still requires all107.

The source-neutral eligibility predicate is: active ER presentation, raw source
at most1024 bytes, root-scoped TB acyclic graph, at most10 unaliased box entities
and8 relationships, no attributes, annotations, class members, notes, groups or
self-edges, ASCII geometry labels no wider than40 cells and no taller than8 rows,
and at least one of (a) a rank-skipping edge,
(b) a repeated ordered endpoint pair, or (c) three or more labeled edges
sharing an adjacent-rank gap. It selects the observed 7/8 long-span graph, the
5/5 repeated-pair graph, and the 10/5 independent-pair graph without reading a
path, theme or fixture ordinal. The four already-successful theme variants of
the 10/5 graph also satisfy this predicate and are intentionally included in
the re-layout cohort. Before implementation, enumerate the full eligible
intersection with all92 prior factory successes and record which ones receive
new coordinates. For those selected sources, Native/Official validators must
prove the full source geometry, relationship semantics and theme roles after
the re-layout; other prior successes retain their complete existing results.
Eligible sources use only this route plan; if it fails, the whole source
rejects atomically. If the planner's statically bounded Work ceiling exceeds
the live `Work::remaining()` before route-plan writes, the branch is not selected
and the existing A* path runs on the untouched scene. Ineligible sources retain
A*. The computed layout must fit
240x1024. Class, State, Requirement, legacy ER, global Canvas, cache and public
APIs are out of scope.

The route and placement algorithm is specified as follows:

1. Build a fixed64x64 directed adjacency table from all relationships and
   retain each original edge separately. Stable Kahn repeatedly removes the
   lowest ready original entity ordinal. Longest-path ranks update as each
   vertex is removed; a cycle rejects the source. Run one downward and one
   upward stable barycentric sweep. Compare exact rational neighbor means by
   checked cross multiplication; equal means keep the current order. Crossing
   scores compare only edges between the same adjacent rank pair and count
   parallel relationships separately. Keep the best of initial/downward/upward;
   score ties keep the earlier order.
2. Require geometry-bearing labels to contain only printable ASCII bytes
   (`0x20..0x7e`) and LF separators; reject tabs, controls and non-ASCII bytes
   for this branch. Also require each label's widest row to be at most40 cells
   and its row count at most8. All107 ER107 candidate originals satisfy these
   bounds; labels beyond them remain on the existing path. Scan each string_view
   once: width is the byte count of its widest LF-delimited row, row count is
   `1 + LF_count`, and visibility means at least one byte other than U+0020.
   These rules exactly match `label_rows` and
   `ftxui::string_width(std::string(row))` on this closed ASCII set, without a
   vector or temporary string. Pay each byte visit before reading it and store
   width, rows and visibility in fixed arrays. Invisible labels have zero
   placement width/height and no bridge; their source notices remain.
3. Sort relationships by descending rank span, descending visible label row
   count, descending label cell width, then ascending original relationship
   ordinal. Stable insertion sort implements the total order. Within each rank,
   retain barycentric order. Let `W=max(entity.width)` and set each entity's
   local x to `floor((W-entity.width)/2)`, so boxes share one centered column.
   For this no-attribute/annotation/member ER box profile, define
   `existing_min_height` exactly as
   `max(3, label_rows(entity.label)+2, incident_degree+2)`. The current layout
   computes the first two terms at `RelationLayout::measure`'s plain-box sizing
   and then applies the ER-specific `degrees+2` reserve; `degrees` increments
   once at each relationship endpoint. Set
   each height to `max(existing_min_height,
   2+sum(label_rows(edge)+2))` over every incident relationship, counting
   parallel relationships separately. Each `label_rows+2` pitch is at least
   two cells, including for an invisible whitespace-only label. Start `y=0`;
   visit entities by rank then retained rank order, place each at current y,
   then advance by its height plus one blank row. Add one more blank row when
   the next entity begins a different rank. This is a deterministic vertical
   stack, and no horizontal route leg can cross another box.
4. ER cardinality markers require horizontal endpoint normals: the existing
   `ports_for` rejects vertical normals for these markers. Every route candidate
   therefore chooses one exterior side, right before left, and uses that same
   horizontal normal at both endpoints. For each candidate, peek at the next
   unused port row for both endpoints on that side; advance those per-entity
   cursors only when the candidate is selected. Failed right-side candidates
   therefore do not consume slots before a left-side candidate is tried. For
   edge `e`, its pitch is `label_rows(e)+2` and its port row is
   `box.y+1+cursor+floor(pitch/2)`. The box-height reserve from step2 makes all
   endpoint slots fit. Source uses `edge.start`, target uses `edge.end`; each
   route point is `border+normal*(marker_size(marker)+1)`. `paint_port` remains
   the only marker painter.
5. For each edge, try at most `E` rail columns on the right, then `E` on the
   left. Let `marker_width=max(marker_size(edge.start),marker_size(edge.end))`
   across all edges and `max_label_width=max(visible_label_width)`, or zero
   when none is visible. The
   shared column has inclusive x extent `[0,W-1]`. For track `t`, the right
   rail is `W-1+M+L+6+2*t`; the left rail is `-M-L-6-2*t`. Adjacent tracks
   leave exactly one blank column. Build a route from the source route point
   horizontally to `(rail_x,source.route.y)`, vertically to
   `(rail_x,target.route.y)`, then horizontally to the target route point.
   Check candidate ports, routes and labels against the complete accepted
   plan. A rail is reusable only when all new segments and contacts remain
   disjoint. The first valid candidate in this fixed order wins. If none fits
   the final `240x1024` canvas or whole-plan constraints, reject the source.
6. Put each visible label on that edge's source horizontal leg. With
   `lo=min(source.route.x,rail_x)`, `hi=max(...)` and `w=label_width`, set
   `halo_left=lo+floor(((hi-lo+1)-(w+4))/2)` and `label.x=halo_left+2`;
   reject unless the full halo fits the bounded allocation. Let `h` be visible
   label rows and set `label.y=source.route.y-floor(h/2)`. The owned label
   rectangle is `[x,x+w-1] x [y,y+h-1]`; contacts are
   `(x-1,source.route.y)` and `(x+w,source.route.y)`. The expanded clearance
   halo is `[x-2,x+w+1] x [y-1,y+h]`. The four approach/contact cells on that
   shaft row are the only cells of the halo this relation may use. No other
   route, label, marker or entity may occupy any cell in the halo. Empty and
   whitespace-only labels create no visible gap and retain their notices.
7. A route without a visible label stores four live vertices
   `[source.route,rail_source,rail_target,target.route]` and paints three
   segments. A labeled route stores six live vertices. With a right rail,
   route order is `[source.route,left_contact,right_contact,rail_source,
   rail_target,target.route]`; with a left rail it is
   `[source.route,right_contact,left_contact,rail_source,rail_target,
   target.route]`. `label_gap_after=1` omits the segment between vertices 1
   and 2, leaving exactly four painted segments; both contact vertices remain
   painted by their adjacent prefix/suffix segments. Prefix and suffix
   `paint_path` calls supply same-relation virtual neighbors across that gap.
   For a right rail, the
   prefix `after` is `left_contact+(1,0)` and the suffix `before` is
   `right_contact-(1,0)`. For a left rail, prefix `after` is
   `right_contact-(1,0)` and suffix `before` is `left_contact+(1,0)`. The
   prefix `before` is `source.route-source.normal`; the suffix `after` is
   `target.route-target.normal`. These virtual neighbors give both contacts
   reciprocal horizontal bits. Dotted strokes use the existing `paint_path`
   mapping, including the dotted horizontal contact glyph. No other edge may
   enter a relationship's label rectangle, halo or approach cells. Unused
   vertices are zeroed and charged before writes.
8. Build and validate every plan before Canvas construction, ER style-plane
   preparation or entity drawing. Use separate `allocation_bounds` and
   `output_bounds`. Allocation bounds union entity boxes, marker/stem cells,
   selected route cells, owned label rectangles and full label-clearance halos.
   For each label, the existing `ErPaint::label` crop extent is exactly
   `[x-1,y-1]..[x+w,y+h]`; it includes the side-padding columns and one blank
   row above and below. Output bounds union entity boxes, marker/stem cells,
   painted route cells and these per-label crop extents. They exclude only the
   additional reservation halo `[x-2,y-1]..[x+w+1,y+h]` outside that extent.
   This must match `ErPaint::finish`'s `right-left+3`/`bottom-top+3` crop and
   its one-cell apron. Translate every planned coordinate by
   `dx=1-allocation_bounds.left` and `dy=1-allocation_bounds.top`. Canvas width
   is `allocation_bounds.right-allocation_bounds.left+3`; height is
   `allocation_bounds.bottom-allocation_bounds.top+3`. Index zero and the final
   row/column provide the one-cell apron. After painting, assert the existing
   crop equals translated output bounds, then let its one-cell apron define
   output rows and count actual `B/A/R/H`. Validate endpoint ownership and
   orientation, marker stems, route/entity and route/route intersections,
   label ownership/clearance, dotted identity, multiplicity, both bounds and
   canvas limits against the whole plan. Only a fully valid plan is committed
   and painted. Failure leaves Canvas and the role plane untouched and rejects
   the source.

### Exact storage and Work accounting target

Use fixed records only; no heap-backed plan containers, path strings or dynamic
inner fields. The following records contain the rank order, entity boxes,
horizontal port cursors, rail occupancy, labels and complete selected routes.
Their final byte size and every later field write must be reviewed before
implementation. Obtain aligned raw storage only after paying sizeof(ERRouteScratch),
then pay a separate sizeof(ERRouteScratch) initialization-write charge before
value-initializing it. Do not let default member initialization touch scratch
before both charges; charge subsequent field updates before each write.

~~~cpp
struct ERBox {
    int x;
    int y;
    int width;
    int height;
};

struct ERBounds {
    int left;
    int top;
    int right;
    int bottom;
};

struct ERRoutePlan {
    std::uint16_t edge_ordinal;
    std::uint8_t segment_count;       // 3 without a label gap; 4 with one
    std::uint8_t label_gap_after;     // 0xff=no visible gap; 1=gap after vertex 1
    std::uint8_t rail_side;           // 0=right; 1=left
    std::uint8_t rail_track;
    std::array<Point, 6> vertices;    // 4 live without a gap; 6 with a gap
    EndpointPort source;
    EndpointPort target;
    LabelPlacement label;
    bool has_label;
};

struct ERRouteScratch {
    std::array<std::array<std::uint8_t, 64>, 64> adjacent;
    std::array<std::uint8_t, 64> rank;
    std::array<std::uint8_t, 64> indegree;
    std::array<std::uint8_t, 64> outdegree;
    std::array<bool, 64> removed;
    std::array<std::uint16_t, 64> current_order;
    std::array<std::uint16_t, 64> best_order;
    std::array<std::uint16_t, 64> position;
    std::array<std::size_t, 64> neighbor_sum;
    std::array<std::uint16_t, 64> neighbor_count;
    std::array<std::size_t, 65> rank_starts;
    std::array<ERBox, 64> entity_boxes;
    ERBounds allocation_bounds;
    ERBounds output_bounds;
    std::array<std::array<std::uint16_t, 2>, 64> next_port_row;
    std::array<std::uint16_t, 128> edge_order;
    std::array<std::uint16_t, 128> label_width;
    std::array<std::uint16_t, 128> label_rows;
    ERRoutePlan candidate;
    std::array<ERRoutePlan, 128> plans;
};
static_assert(sizeof(ERRouteScratch) <= 32768);
~~~

Let N be entities (N<=64), E relationships (E<=128), C tested rail
candidates (C<=2E^2), S selected painted segments (S<=4E), D route-cell
validation visits (sum of ManhattanLength+1 per selected painted segment), Q
visible labels (Q<=E), M marker/stem cells (M<=4E), V live route vertices
(V<=6E), U bounds contributions (U=N+M+S+2Q), and L label-clearance cells
(sum((label.width+4)*(label.height+2))). Existing source parse, ER parse,
Canvas/role-plane writes and final payload costs remain unchanged and additive.
The same-factory test seam must include initial Flow classification and the
complete public layout/payload validation path; a stripped ER-only body is not
budget evidence.

Before inspecting label bytes, pay 4*sum(label_bytes). Charge each actual
primitive scan, comparison, move, candidate check, record-field write and
validation visit immediately before it. Use checked arithmetic for every
product and sum. The following are worst-case operation bounds for the
specified algorithm; the implementation must still charge each actual visit,
not guess an aggregate charge:

~~~text
scratch storage                          = sizeof(ERRouteScratch)
scratch value-initialization writes       = sizeof(ERRouteScratch)
label width/row scan                     = 4*sum(label_bytes) plus actual byte visits
adjacency and degree writes               = E adjacency + 2*E degree updates
stable Kahn scans                        <= 2*N*N vertex/adjacency visits
Kahn rank/removed writes                  <= E rank relaxations + 2*N fields
barycentric neighbor scans                <= 2*N*N adjacency visits
barycentric rational-key writes           = 4*N fields
crossing-score comparisons                <= 3*E*(E-1)/2 edge pairs
barycentric position writes               = P; P<=2*N*(N-1)+3*N
barycentric insertion-order writes        <= N + N*(N-1)
barycentric best-order copies              <= 3*N
relationship-order comparisons             <= 4*E*(E-1)/2 field comparisons
relationship-order writes                  <= E initial + E*(E-1)/2 moves
incident port-pitch scans                  = 2*E endpoints
label metric writes                        = 2*E fields
entity box layout writes                   = 4*N fields
candidate side-cursor reads                 = 2*C fields
candidate endpoint/port formation           = 2*C endpoint visits
candidate route-plan writes                 = 40*C scalar fields
candidate conflict visits                  <= C*(6*N + 80*E + 10*Q)
accepted route-plan copies                 = 40*E scalar fields
accepted port-cursor and rail writes         = 3*E fields
bounds contributions                       = 4*U coordinate comparisons
marker orientation/ownership checks          = 2*E endpoint checks
marker/stem validation                      = M cell visits
segment/entity box intersections             = S*N
segment/segment intersections                = S*(S-1)/2
segment/label intersections                  = S*Q
label/label intersections                    = Q*(Q-1)/2
route-cell validation visits                 = D
label-clearance validation visits             = L
final coordinate translation writes           = 2*N+8*E+6*Q+2*V+8 fields
route and label scene writes                  = existing Canvas/role-plane charges
~~~

Each ERRoutePlan contains exactly 40 scalar fields: one edge ordinal, four
route metadata bytes, twelve vertex coordinates, fourteen endpoint-port
coordinates/markers, eight label-placement fields and one label flag. Candidate
construction writes all40 fields before each collision attempt; accepting it
copies those40 fields into its edge slot. Each candidate has at most four route
segments. Its conflict bound covers those segments and up to four marker/stem
cells against entity boxes, prior routes, markers and label halos, plus the
candidate label rectangle against those occupied regions. Candidate search
tries at most E right rails then E left rails per relationship; read both
endpoint-side cursors and charge endpoint formation before each attempt.
P counts actual position writes made by rank-position recomputation. V counts
only live route vertices shifted to final canvas coordinates. U is the number
of entity, marker, segment, owned-label and clearance rectangles/points folded
into allocation/output bounds. Final whole-plan validation pays its own
segment/entity, segment/segment, segment/label and label/label visits. D and L
are charged while validating the selected route cells and label halos. These
counts include the previously unitemized entity placement, marker checks,
candidate peeks, bounds construction and coordinate translation; any additional
implementation visit must be charged immediately and added to the reviewed
bound before production use.

Validate the complete selected plan before committing any entity box, route,
label or role-plane cell. On a Work or geometry failure, discard the plan
atomically. After the plane is committed, count actual serialized B UTF-8
bytes, A style cells, R style runs and H rows with checked arithmetic before
output allocation. Preserve the existing production prepayment exactly:

~~~text
initial plane scan       = 8*canvas_cells + 128
body/notices scan        = 8*body_cells + 32*notice_count
output prepayment        = 48*B + 12*A
                           + R*(512 + 8*sizeof(string))
                           + H*(256 + 4*sizeof(Element))
                           + 4*H*sizeof(string) + 2*B + 256
~~~

For the two output-prepayment failures, the measured shortfalls are exactly
599186-524735=74451 and 601232-524056=77176. The style-run and output fees
cannot be lowered. Existing Canvas/role-plane fees pay scene writes once; new
validation fees pay only their additional reads. Any scratch-field change alters
sizeof(ERRouteScratch) and requires a renewed budget review and boundary tests.
The ER Work limit remains 1,000,000 and the canvas remains 240x1024.

### Required review and acceptance evidence

Independent B2 review must approve the eligibility predicate, Kahn and
barycentric tie rules, relationship order, horizontal port allocation,
rail-side ordering, label/contact ownership, fixed records, exact Work formula
and atomic failure boundary before production edits. Close the measured default
factory stage table for all 15 failures and enumerate every already-successful
source selected by the same predicate. After approval, run all 107 complete
originals through the same public-path factory with the exact 1,000,000 cap.
For every successful source, prove its actual consumed budget succeeds exactly
and fails one unit short. Re-capture and validate every selected prior success,
while proving all non-selected prior successes retain their whole-scene results.
Native and Official consumers must independently capture five frames before
binding, assert complete source semantics and theme/style roles, and reject
meaningful geometry mutations. Run Debug and Release, the complete Mermaid CTest
label and strict reviewed joins. Only 107/107 complete drawings and passing
Native/Official joins close this ER gap. Every source fallback remains zero
drawing credit.

### Same-factory test seam

The production 1,000,000 feasibility regression must include the same initial
Flow parse attempt, classification, ER parsing/layout and payload validation as
the public factory. The existing owner-private boolean seam forwards the real
source, ASCII option and caller-owned Work to the shared layout path; it exposes
no payload or style plane and accepts no fabricated layout. Preserve public
render signatures, dispatch order, cache ownership, Block test probe, default
Work limit and rejection behavior. Five-frame public captures remain drawing
proof; the boolean factory result is only budget evidence.

## B2 P2 clarifications: complete planes and finite generic grammar

The background at an outer border is its parent surface, not its entity fill.
For this table, parent means root surface_tint_2 or the containing group's
surface_tint_4. All cells have bold=false and no extra attributes/hyperlinks;
Official preserves only its inherited dim bit. Apply this full-style table:

| Cell category | Foreground | Background |
|---|---|---|
| Root body/apron, external route/marker | text | root surface_tint_2 |
| Group interior blanks/caption | text | group surface_tint_4 |
| Group boundary, corner or committed gate | text | group's parent surface |
| Entity outer boundary/corner/connected port | entity border role | entity parent surface |
| Entity interior caption and blank fill | text | entity fill role |
| Internal table rails/intersections | entity border role | entity fill role |
| Rail intersecting outer boundary | entity border role | entity parent surface |
| Route inside group, outside entities | text | containing group surface_tint_4 |
| Committed label and its owned U+0020 side padding | text | root surface_tint_2 |
| Matched same-relationship inline shaft contacts | route text | containing host surface |
| Notice glyphs and their own spaces | text | inherited background |

Priority is root initialization; groups outer-to-inner; entity interiors and
internal rails; outer boundaries/corners; committed route/gate/label effects.
Source ports and group gates retain their complete FG/BG/bold style despite a
route commit. Committed labels cannot overwrite an entity or group boundary or
port; existing exclusion geometry remains mandatory. Failed candidates do not
change the plane. Child rectangles override group interior fill only within
their owned masks, with their boundaries still on the group parent surface.
This fixes the earlier ambiguity without changing Canvas or global palette.

The finite new-profile generic grammar is exactly:
`Identifier = [A-Za-z_][A-Za-z0-9_]*`,
`GenericType = Identifier '~' Identifier '~'` covering the entire raw type token.
Plain types without tilde retain the existing supported literal type grammar,
subject to the closed Markdown/HTML-free display subset. For GenericType,
validate the raw token first, then replace the two delimiters with literal angle
brackets. The generated display is never interpreted as HTML/Markdown and is
not passed back through a raw-angle rejection path. Nested, multiple-pair,
unmatched or comma-containing tilde forms reject the whole new-profile source.
Tildes in names/keys/comments/aliases are outside this finite generic profile.
The cohort's `type~T~` becomes literal `type<T>`; source lexemes/witnesses stay
unchanged. This is a deliberate closed subset of the fuller upstream
comma/outermost-delimiter algorithm, not an alternating-replace approximation.

Keep M working canvas cells distinct from A serialized role cells. Prepay the
new working plane at sizeof(uint16_t)*M before allocation, plus its actual
initialization/ownership/route writes. Prepay at least8*M for the new crop scan
and8*A for output cell bookkeeping before those scans; each additional pass or
write pays its own bounded visits. Serialization's4*A covers only two output/
cache role planes; it does not also pay the working plane. B/R/H formulas above
remain additive. Any new ownership map, retained label context or generic
scratch has its own typed-size prepayment before creation/growth. Legacy glyphs,
empty-plane finish and the production1M cap remain fixed; actual same-factory
cost/retention tests are required before admission.


## ER inline-caption contact clarification

The owned blank side-padding rule does not prohibit the established ER inline
label bridge. A complete scene match may certify a pair of reciprocal horizontal
shaft contacts for exactly one source relationship and its matched label gap.
Only the actual nonadjacent jump in that selected trace is eligible: the two
points must match that label's gap boundaries, share a row inside its height,
and carry the required reciprocal arms with no extra branch. They cannot be a
marker, entity/group boundary, another relationship's contact or foreign ink.

Matched contacts retain their complete route FG/BG/bold style, including the
containing group surface. Caption cells and all other owned side-padding cells
use the neutral label style; those other side-padding cells must be U+0020.
This reuses the existing ER route-assignment semantics instead of transplanting
Block's detached-label clearance rule or constructing another layout solver.

A test-private ER successful-label witness may expose only relation ordinal,
matched label gap and the two actual contact points, bound to the same grid and
source Model. It is issued only after the full existing scene checks, including
all ink, ports, direction and multiplicity, pass. Failure/exception leaves an
empty witness; mutants require fresh matching and cannot reuse an old receipt.
The optional output must leave legacy null-output checks/fees unchanged. Inline
positive, broken-arm, extra-branch, wrong-owner, foreign-marker and boundary
negatives close this interpretation before B2. The terminal proposal, pure
helper and contract bindings must be synchronized to this clarified candidate.
