# P1 Class direct-style batch proposal

Status: executed and independently approved D1a sub-batch; Debug/Release878/878,
serial style/resource/real-PTY14/14 each. Accepted657/2145, Class254/285.
See [complete executed evidence](mermaid-class-style13-review.md). The design
and admission proposal below is retained as the historical implementation
contract; Phase D remains incomplete.

## Source scope

The source-verified remaining Class44 inventory contains13 direct-style files.
They have only three distinct complete bodies, each declaring exactly one
`Class10` and no relationships. Seven use
`style Class10 fill:#f9f,stroke:#333,stroke-width:4px`; six also specify
`color:white`. All source frontmatter remains part of the admission contract.
The other Class failures include undefined CSS classes, callbacks and links;
those constructs must continue returning source until their own contract is
reviewed. In particular, RFC D1 explicitly rejects undefined class references.

## Proposed implementation

Implement Class-only direct `style ID` assignments first. Resolve the exact
entity ID after the whole parse. Retain each exact source declaration; reject
unknown targets, namespaces, default/assigned CSS classes and unsupported
properties. Admit `fill`, `stroke`, `color`, `stroke-width` with existing Phase D
normalization, order and numeric bounds. The initial color subset is hexadecimal
(`#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa`) and the witnessed name `white`; all other
named/function colors keep source. Intermediate alpha keeps source. This is an
explicit subset of D1, never a claim of full CSS support.

Map source color values to the already reviewed semantic hue roles. Paint using
palette tokens only. Allocate the parallel Canvas style channel lazily; retain
plain Cell size and style-free allocation/Work/glyph behavior. Carry owned styled
rows through the native layout cache and final FTXUI element construction;
plain testing/ASCII row APIs remain projections, with applied styles returning
source in ASCII mode. Do not decode or strip styles before caching.

Append the bounded plain legend before fit checks. It contains the exact source
declaration and projected roles, including the unprojected foreground on fill
and width/bold adaptation. Never silently ignore a source property. Enforce the
same static canvas and one-million producer Work limits; charge before every
new allocation/scan/paint, and keep rollback/fallback whole-source.

## Required admission evidence

Author a separate immutable complete-source Model+style asset for all13 cases;
independently review source hashes, frontmatter, complete graph and projected
style tuples. Capture all five public frames before binding/decode. Validate
whole geometry plus exact legend and actual screen attributes in all seven
palettes and both accessibility modes. Verify every entity interior, border and
label owns its expected style, including blank filled cells; reject missing,
wrong or leaked styles, wrong target/property/value, omitted source records,
extra graph/legend facts and no-op adversaries. Unknown/resource-limited checks
never earn success or mutation credit. Check narrow fallback/restoration,
following content, exact-fit/one-short widths and unchanged style-free Work.

Complete independent design/PRR and implementation reviews, Debug and Release
Mermaid regressions, exact matrix and frozen-roster comparisons. Only verified
complete originals may change coverage; body diagnostics remain outside2145.

## Revision1: explicit ownership, grammar and admission gates

The first independent design/PRR review requested changes. The following
contracts close those requests and remain subject to a second review.

### Owned payload and cache

Keep the public row getter as a plain projection. A private owning layout
payload carries glyph rows, optional semantic style runs and measured layout
work; it contains no palette colors, Elements, pointers into source or Canvas,
or references to another cache entry. Cache rendering and uncached rendering
both construct a fresh Element using the active theme/accessibility policy.
The cache render implementation may move into the cache implementation unit;
no new public style or production-detail test API is needed.

Retain exact source bytes plus ASCII as the key, existing configured entry and
byte limits, collision equality check, LRU eviction and empty rejection
sentinels. Styled-entry byte accounting includes source bytes, all row/run text,
vector/run metadata and owned payload metadata. Use checked additions/products;
overflow or an oversized entry serves the correct uncached result without
retention. Evict before insertion. Copies own all payload data and rebuild
indices; tests mutate/destroy a copied cache independently. The style-free
accounting and retained values keep their existing path and tariffs.

### Same producer ledger

Move the fresh Work owner to each styled render call and pass it through private
layout, row extraction, legend, cache payload copy and final Element construction.
No styled helper creates a replacement Work. On a hit, debit the stored measured
layout cost before using copied rows, then pay the same element-construction
tariff as a miss. Before layout returns, reserve a bounded conservative charge
for the two possible payload copies (cache retention and caller); use this
same reservation on uncached calls so cache capacity never changes admission.
Record layout cost only after all extraction/copy reservations are paid. Skip
retention when storage capacity is insufficient. Cache bookkeeping itself must
not convert a paid drawable result into a partial drawing.

Each additional run allocation/string copy, cell visit/write and footer scan
has a documented before-use tariff. Failure returns complete source and never
publishes an admitted partial payload. Add measured exact/one-short Work tests
at extraction, copies and run materialization. Style-free calls retain their
current Work lifetime/path with no new charge, allocation, glyph or Cell growth;
verify measured ledgers. Existing three inactive classDef drawings remain valid.

### Closed D1a grammar and legend

Target IDs are ASCII `[A-Za-z_][A-Za-z0-9_]{0,63}` and resolve after the whole
parse to existing root-level rectangular Class entities. A style statement is
`style ID PROPERTY:VALUE[,|; PROPERTY:VALUE ...]`, with horizontal whitespace
around tokens and an optional trailing separator. Parentheses/functions and
unknown residue reject the complete source. Property names and `white` are
ASCII case-insensitive. Duplicate properties within one declaration reject.
Repeated declarations resolve in source order, with later properties replacing
earlier values. Mixed supported/unsupported declarations reject the whole file.
The hex/white and finite0..64 optional-px width subset above is the only D1a
value grammar. Alpha0 removes fill; a transparent stroke/foreground is rejected
in this sub-batch. Alpha1 is opaque; intermediate alpha rejects.

Retain all ordered exact source declarations in the semantic model. Legend
identity is `(target, normalized properties)`. Equivalent repeated declarations
share one entry showing the first exact declaration and an explicit occurrence
count; no source record is discarded. Distinct declarations remain distinct
even if projection yields the same roles. D1a admits at most seven entries plus
one plain adaptation-key line (eight total), each at most240 cells; exceeding
these deliberately narrower limits returns complete source. Foreground on fill
uses palette.text and explicitly discloses any unprojected source color. Border
width>=4 applies bold to outline/compartment strokes; labels retain normal
weight unless a future reviewed font property is implemented. Fill applies to
strict entity interiors including compartment-cell backgrounds; outer border
background remains default. The screen oracle checks every such ownership rule.

### Concrete source and release prerequisites

Before producer code, independently review
`tests/fixtures/mermaid_native/class_direct_style13_source_models.proposal.v1.json`:
35,243 bytes, SHA-256
`59e4506fe68605d3f91229a28c7dad809147769d7b431de743c6a591953c3fc0`.
All13 full source hashes, exact frontmatter and normalized config, complete
one-node/no-edge Models and style tuples are explicit; every admission flag is
false. Independent design and source reviews must approve before implementation.

A test-private screen oracle must independently own expected full geometry,
legend and per-cell attributes, including blank fill cells/cropping, compartment
strokes, exterior default cells and following content. It must reject missing,
extra, wrong, leaked and no-op styles without treating unknown/resource as
negative credit. Test cache hits/misses, palette switches and accessibility,
ASCII rejection, eviction/copy independence, and inherited inactive definitions.
Retain a per-original disposition ledger, supported syntax/projection guide,
dual-preset Mermaid CTest and serial style suites, real PTY and copy-selection
evidence, plus exact matrix/roster comparisons. These gates do not assert full
browser CSS, arbitrary named colors, full-project CTest or Phase D graduation.

## Independent review records (2026-10-30)

The available Codex CLI ran separate read-only review invocations. These labels
identify the invocation artifact; no Claude-model execution is claimed.

### Source13 authority

**APPROVE proposal only.**

I reviewed all 13 complete pinned sources. Their bytes, hashes, frontmatter, bodies and manifest Git blob identities match the proposal. Each Model captures the complete one-`Class10` graph and its source configuration; I found no omitted source feature, annotation or count. The canonical Model byte counts and hashes also match.

The style tuples agree with RFC 0030: `#f9f` projects to Purple, `#333` to Neutral, and `4px` to a bold border. Where `color:white` appears, its Neutral source role is retained and marked unprojected on the fill. All entries retain `native_admission: false` and zero implemented foreign assertions.

This is source-authority approval only. Independent design and eventual producer/oracle gates remain required; it grants no implementation or coverage credit.

### Revised design/PRR

**APPROVE design/PRR**, subject to the execution gates in the revised plan. I found no concrete contradiction that requires another design revision before implementation.

The proposed private owned payload is feasible alongside the current exact-source-plus-ASCII cache and fresh-element finish path. The measured layout debit and copy reservation address the existing separation between layout’s `Work` lifetime and element construction, while preserving the current style-free path. The grammar, declaration ledger, legend limits, and border/fill ownership rules also define a bounded D1a subset consistent with RFC 0030 Phase D.

Approval depends on completing the plan’s gates: P0 review and dual-preset regression; independent approval of the immutable complete-source asset before producer code; implementation review; the full screen, cache, Work-boundary, and fallback oracle; the per-original ledger and documentation; dual-preset Mermaid regressions, serial style suites, PTY and copy-selection evidence, and matrix/roster comparisons. The implementation review should verify that the public plain-row projection cannot cause cache rendering to lose the styled payload, and that measured hit debits and reserved copies produce the same admission outcome as a miss.

Reviewer: `agent:/root`. This is a design/PRR verdict only. It grants **no implementation, coverage, or Phase D graduation credit**. No files were edited and no tests were run.

## Revision2 clarification proposed during implementation review

The producer review found two issues: named `none`/`transparent` accidentally
escaped the admitted color subset, and logical-line trimming lost declaration
whitespace. Reject those two named values (transparent fill remains available
through alpha-zero hex). Retain the original physical declaration separately
from the trimmed parsing view. Line endings delimit a declaration and are not
part of it; indentation and trailing horizontal whitespace are retained.

For a terminal-safe exact legend, preserve spaces literally and render each
source TAB byte as the visible two-character `\t` spelling. The plain legend key
explicitly discloses `source TAB=\t`. No other control byte is admitted. Retain
unescaped original bytes in the declaration ledger; equivalent repeats show
the first original declaration's escaped display and the exact occurrence
count. The source asset's `verbatim` field denotes the exact logical statement
span, not its indentation; complete immutable source binding plus independently
extracted physical rows supplies the expected raw declaration. No asset changes
or renderer-derived expected text are authorized by this clarification.

The same review accepts a minimal testing-only Work-injection forwarder returning
the existing public Element type through the established facade; no style or
production-detail type is exposed. Production keeps fresh default Work. Measure
actual starting minus remaining work for stored debits, including non-default
test budgets. Exact/one-short tests must reach final Element construction.
