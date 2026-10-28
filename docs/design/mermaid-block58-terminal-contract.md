# Mermaid Block58 terminal contract

**Status:** B2 design/PRR approved by agent:Harvey on 2026-10-26. The reviewed
contract snapshot is SHA a0f2976fd5408ede363dd2f8307a282559a887d8a9df4734f91a731d532cc54d;
this status/implementation note follows that review. This document
supersedes the earlier unreviewed cell-centre/thin-line/histogram draft. Passing
preflight tests grants no production drawing credit.

## Outcome, ownership and authority

The owner is `loom.ui.visual.mermaid`. Admit all 58 complete frozen Block sources
through the existing cached and uncached factories, with faithful source
placement, contours, captions, edges, categorical roles and visible disclosures.
No shared canvas, global theme provider, released factory/cache signature,
persisted format, dependency or other family's grammar changes.

The source authority is `block58_complete_source_proposal.v1.json`, SHA-256
`1d0006ab73cf61951a15599606cde2e46a59865416ac9f43ad244d45b0e4c27f`.
Source-derived terminal facts are `block58_terminal_projection_proposal.v1.json`,
SHA-256 `b7d0bf628af004718df6ab3550dd09bb0a71481bc2f70b0cf3e47b2584870caf`.
Their independently reviewed source models, witnesses, styles and notices are
immutable. They do not author terminal pixels. The separately authored
`block58_boundary_contract.v1.json` proposes full literal primitive masks,
interiors, glyph rows, caption occupancy and ports for every source type and all
seven observed arrow headsets. Normalized primitive fixtures carry zero original
coverage; their empty/synthetic captions are not changes to frozen originals.

This document makes the family-local amendments to RFC 0030 explicit. Its exact
new geometry takes precedence over the earlier unreviewed draft; the frozen
facts' source semantics and the RFC's immutable resource bounds remain binding.
Full-source source/model binding precedes each independent geometry verification;
captures precede binding in the eventual Native and Official consumers.

## Closed profile and source scheduling

Keep remaining Work at 1,000,000, raw source at 65,536 bytes, canvas at 240 by
1,024 including annotations, placed entities/composites/expanded spacers at 64,
nesting at 16, declarations at 1,024, edges at 64, properties at 256 and classes
at 64. Positive columns/spans are at most 64. Root is separate from the 64 placed
items. Production parses syntax, never source filenames or compiled fixtures.
Unsupported metadata, syntax, styles, dimensions, routing or resources reject the
whole source with no partial art. ASCII Block remains whole-source rejection.

Each scope starts with columns=-1; the default is horizontal auto allocation.
At position p, a positive N places the full retained span at (p%N,p/N), then
advances by min(span,N-p%N). Auto places at (p,0) then advances by span.
There is no pre-wrap, span clamp, skipped row or inherited column setting.
The first ID owns parent, order, span and directions. Later declarations update
only the allowed non-default caption/type fields. Bare `block BL` contains a
child BL inside an anonymous group; `block:2` names group 2 with span one.
Spaces of width N become N independent width-one blank placements.

Resolve all IDs and parents before validating or placing any root child. Preserve
empty group captions and exact one-space leaf captions. Decode only the witnessed
label entities; retain source declarations for visible style/slot legends. Frozen captions happen to be printable ASCII, at most 27 cells; this is a
census, not an exact-caption allowlist or a new production length limit. Keep
the RFC's per-caption 1024-byte raw bound and 256-cell decoded bound. Accept
valid whole FTXUI text glyphs under the existing plain-text validation rules;
reject controls, malformed UTF-8 and unsupported HTML/Markdown syntax atomically.
Decode only &lt; and &gt;, exactly once, into literal caption text. Other named or
numeric entity forms reject; never recursively interpret the decoded result as
HTML or Mermaid syntax. No HTML parser or foreign sanitization equivalence is
claimed. Preserve empty captions and exactly one U+0020. Wrap at at most32 cells
without splitting a wide glyph or combining tail: prefer the last U+0020 word
boundary that fits, otherwise split the long word between whole glyphs. Retain
exact row joiners so rejoining the rows reconstructs every decoded caption byte;
do not trim or collapse authored spaces. Precount and charge the full caption,
glyph rows and joiner storage. Source byte/line witnesses and whole-model hashes,
not diagnostic-body parsing, establish meaning.

## Measurement and allocation

The root is unoutlined and has a **one-cell apron on all four sides**. This
adopts the previously proposed apron and permits the three disjoint top-left
fan-out routes. Every composite has a closed rectangular border and a two-cell
inset on each side. Logical row/column gaps are three cells, spacers included.
For logical extent K=max(column+retained_span), equal unit width is
max(1,max(ceil((natural_width-3*(span-1))/span))). Inner width is K*u+3*(K-1).
Distribute a wider parent grant as q=usable/K and remainder r: the first r
columns get q+1, all others q. Row height is the maximum child's natural height.
Centre each child vertically in its row; centre fixed circle/diamond bodies
horizontally in their grant. Other bodies stretch horizontally. No terminal
reflow changes source allocation. Alignment padding is outside the shape mask.

Let L be maximum caption row width and C its row count. Natural dimensions are:

| Shape | Width and height before parent grants |
|---|---|
| Rectangle, rounded rectangle | max(9,L+4), max(5,C+2) |
| Spacer | 1,5; blank, no mask |
| Circle, diamond | both equal the least even integer >=max(10,L+2*C+6) |
| Hexagon, lean-right, trapezoid, inward notch | max(11,L+8), max(7,C+4) |
| Stadium | H=max(7,C+4), W=max(H+4,L+8) |
| Subroutine | max(9,L+8), max(5,C+2) |
| Block arrow | core max(7,L+4) by max(5,C+2), plus extensions below |

Arrow bits R=1,D=2,L=4,U=8; allowed sets are 1,2,4,8,5,10,15 only. Extension
on each side is max(3 if that side has a head else 0, 1 if there is a head on
the perpendicular axis else 0). Recompute core within the full granted body.
This contains the complete head and shoulders; no clipping or marker substitute.

## Integer masks, captions, ports and complete boundaries

Local cells have integer coordinates 0..W-1,0..H-1; all polygon edges are inclusive.
Mask membership uses signed integer cross products, inclusive edge tests and
even-odd polygon interiors. A union is a set union, never XOR. An outer boundary
cell is included and has at least one excluded cardinal neighbour. Interior is
mask minus boundary and any subroutine rail. Group paint precedes leaves; later
children own their opaque masks. Spaces and exterior alignment padding retain
the inherited parent background. No second area-sized contour buffer is needed.

| Shape | Local mask |
|---|---|
| na, square | same full rectangle; source types remain distinct in binding |
| round | full rectangle, rounded corner glyphs |
| circle | (2*x-(W-1))^2+(2*y-(H-1))^2 <= W^2 |
| diamond | abs(2*x-(W-1))+abs(2*y-(H-1)) <= W |
| stadium | radius r=floor((H-1)/2); middle rectangle x=r..W-1-r plus inclusive disks at (r,r),(W-1-r,r), clipped to body |
| hexagon | [(2,0),(W-3,0),(W-1,H/2),(W-3,H-1),(2,H-1),(0,H/2)] |
| lean-right | [(2,0),(W-1,0),(W-3,H-1),(0,H-1)] |
| trapezoid | [(2,0),(W-3,0),(W-1,H-1),(0,H-1)] |
| inward notch | [(0,0),(2,H/2),(0,H-1),(W-1,H-1),(W-1,0)] |
| subroutine | full rectangle, rails x=2 and W-3 |

For an arrow core's inclusive bounds l,r,t,b and midpoint mx,my, union the core
with selected triangles: R[(r-1,t-1),(r+3,my),(r-1,b+1)],
L[(l+1,t-1),(l-3,my),(l+1,b+1)],
U[(l-1,t+1),(mx,t-3),(r+1,t+1)],
D[(l-1,b-1),(mx,b+3),(r+1,b-1)]. Hidden triangle bases are interior, not extra
strokes. Normalization retains every extension inside its allocated body.

Rectangle/group corners use closed light box glyphs; round uses ╭╮╰╯. Subroutine
rails meet the top with ┬ and bottom with ┴. All nonrectangular outer boundary
cells use **█ (U+2588)**. This deliberate filled-cell terminal adaptation preserves
both incident arms, staircase shoulders and diagonal contacts without guessing
which thin segment owns a cell. It does not claim SVG or font-pixel equivalence.
Route and endpoint glyphs remain separate from these boundary cells. At an
incident route's nonrectangular endpoint, retain the port cell as █; the stem
and its marker start in the adjacent outward cell. The stored route still ends
at that exact port for geometric binding. Rectangle/subroutine/rounded ports
and composite gates use their ordinary reciprocal light-line junctions. This
precedence applies before marker/dotted-line painting; no marker replaces a
port. Thus full primitive masks/boundaries survive when connected as well.

Caption rectangle is centred in core by integer division, with exact measured
L,C; every occupied caption cell must be interior and free of rails. Reject if
containment fails, rather than silently crop or resize an already placed grid.
Preserve blank caption cells, full leading/trailing spacer rows and all row widths.
Ports walk from core.x+floor((core.w-1)/2), core.y+floor((core.h-1)/2) in each
cardinal direction to the last included mask cell. This reaches the inward
notch, not a bounding-box extreme. Groups use the midpoint of each border.

## Finite routes and failure order

Process edges in source order. Sort the 16 source/target port pairs by Manhattan
distance, then source E,S,W,N and target W,N,E,S. Skip an already occupied port
before allocating a candidate. Stems extend normally through their own alignment
padding until just outside the allocated band. Try HV, VH, then HVH lanes at
sorted distinct x coordinates, then VHV at sorted distinct y coordinates. Lanes
come from the root apron, group inner edges, spacer centres and source-derived
ports. No BFS, arbitrary canvas expansion or second area buffer is introduced.

Each polyline must be simple, orthogonal and within the full canvas. Retain no
backtracking, non-adjacent self intersection, prior route overlap, unrelated leaf
allocation traversal, label collision or unrelated group traversal. Ancestor
borders may be crossed once, normally, away from corners: source ancestors
innermost-out followed by target ancestors outermost-in, excluding shared ones.
Store and independently reconstruct every reciprocal gate; never retarget an
edge to a container. Group corners cannot be gates.

An edge label needs a horizontal segment of at least L+4 cells. Try above then
below, centred, with one clear cell on each side and no occupied caption,
contour, route or unrelated leaf band. Point markers are one cell outside their
own ports and face those endpoints; circle/cross markers retain their meanings.
Normal links use light glyphs, thick links the bold attribute, dotted straight
links ┄/┆. Rectangle/rounded/subroutine port and composite gate line bits preserve both
incident border arms. Nonrectangular ports keep their full boundary █ under the
explicit contour precedence above; their adjacent stems remain ordinary routes.
Fail atomically in this order: lexical/model/style validation; measurement and
bounds; mask/caption/port validation; source-order routes/labels; role/row/glyph
validation and serialization; cache retention eligibility. Retention failure
serves the newly computed payload uncached, not rejection or another key's data.

## Block-local amendments to D1, D4 and E2b

| Existing rule | Block-only amendment |
|---|---|
| D1 rectangle/rounded style masks | Above complete masks, including composites, receive roles; other families unchanged |
| D1 styles on groups/nonrectangles reject | Resolve the observed Block node/group targets atomically before paint |
| D1 general proposed color syntax | Block closes fill/stroke/color to witnessed #rgb/#rrggbb; widths 2px normal,4px bold; unsupported values reject |
| D1 eight legend entries/eight lines | Block retains all 122 frozen annotation records across the 58 sources under the total canvas/Work caps |
| D1 source label color | Opaque filled nodes use readable text foreground; disclose source color and RGB loss verbatim |
| D4 theme-dependent display | Store roles only; map one fresh current theme snapshot when building each Element |
| E2b row-only cache storage | One private optional packed plane; row-only public result and sentinel semantics stay exact |

Style order is base, categorical composite preorder index, class assignments in
source order, direct styles; last property wins within a layer. Resolve every
property/class/target before drawing. No general CSS/font/init grammar, unknown
property, default class or partial acceptance. Categorical arrays apply only to
redux-color/redux-dark-color, modulo the source's 12 border slots. The other four
observed themes do not acquire categorical colors. Dark background array is empty.
Keep the reviewed hue arrays, including light slot5's Blue fill/Purple border.

Packed roles: foreground bits0..4, background5..9, bold10; bits11..15 zero.
Codes 7..31 reject. Foreground0 inherits,1=text,2=danger,3=warning,4=success,
5=info,6=accent_purple. Background0 inherits,1=surface_tint_2,2=danger_tint_bg,
3=warning_tint_bg,4=success_tint_bg,5=info_tint_bg,6=surface_tint_4.
Neutral maps1,Red2,Orange/Yellow3,Green4,Cyan/Blue5,Purple6 under the RFC hue
thresholds; source hue/slot legends disclose these lossy joins. No literal RGB
or shared resolve_color() call. Node interiors/caption blanks default bg1;
composite interiors bg6. Exterior/apron inherit bg0. Outlines use source stroke
and parent background; subroutine internal rails use their own fill. Filled
captions use foreground1. Routes use foreground1, underlying composed background
and source bold. Edge labels and annotations use readable text with inherited
background. Blank cells carry the same required background as nonblank cells.

Use the theme provider's existing current palette for Dark, Light,
DarkDaltonized, LightDaltonized, Monochrome, LightAnsi and DarkAnsi. Existing
force_monochrome already selects its palette. high_contrast is presently stored
without changing that palette; Block honors the same snapshot and introduces no
global policy or invented flag-specific color transformation. Verify both flags
and all variants explicitly after implementation, including unchanged geometry
and source bytes. Theme names/look notices preserve authored metadata, not foreign
renderer behavior.

Append notices in this fixed order: metadata, slot legend, source-style declaration
legend, style-loss. Each record starts with one empty separator, followed by its
exact reviewed rows (at most32 cells); no generated duplicate notice and no
trailing separator. Preserve legitimate repeated frozen source records. Across58:
122 records/386 text rows/9179 text cells/508 appended rows. Per-source maximum:
14 records/48 text rows/1176 cells/62 appended rows. Outer width is max(grid,note
width); total height includes separators. Never pad annotation rows to grid width.

## Work, storage and cache dataflow

Follow the RFC's checked prepayment schedule with the same remaining caller Work.
A layout-local fixed scratch supports 1024 spans,256 style pairs,64 entities/edges;
no fixture JSON, regex retry or file lookup exists in production. Model allocation
uses bounded actual capacities; every replacement string pays for both live copies.
The worksheet conservatively includes the prior scan and fixed probe, 12*S scanner,
ID comparison upper D*N*(1+maxID), caption/identity ownership, grid arrays and visits,
all successful/failed candidate points and gates, mask/port/route writes, complete
blank-preserving raster, copied planes and UTF-8 rows, annotation vectors, and
fresh row/run Elements. Charge8 per repeated contour/route/glyph inspection,
16 per property/pair step,32 per child grid step; no retry refunds or Work reset.

The approved frozen58 worksheet additionally charges actual serialized bytes B
by B*(sizeof(string)+2), and16*(A+noteCells) for validation/assembly visits.
This covers FTXUI text-render byte-capacity glyph vectors for the frozen short
single-scalar/ASCII rows; all58 remain feasible, maximum962940. Private payload
validation is allocation-free and occurs once at the layout kernel, before cache
retention. Fresh finish groups base scalars with all FTXUI zero-width tails as
exact original substrings without allocating a glyph vector. General long
combining tails require separate byte-scan and simultaneous string-capacity
prepayment when the producer is connected; 2B and cell-count fees alone are not
a generic bound. Styled public runtime and actual prepayment remain pending.

Measured Debug/Release ABI: string24, Point8, Element16, Cell24, ParserSpan40,
StylePair40, Entity272 bytes. The preflight worksheet is finite-size feasibility,
not proof that production prepays before allocation. Re-run all58 with final glyphs
and report the actual maximum. Production must independently show every allocation
and operation is prepaid, including failures, using the unchanged 1M budget.

One private LayoutPayload owns rows plus optional uint16_t-per-cell plane. Empty
plane is the exact legacy inherited-style path; no new fees or probe for old
families. One dispatch produces this payload for both uncached and cached paths.
The public row getter copies rows from the same private cache lookup; render()
consumes the full payload directly, never the getter plus a second parse. Rejection
normalizes to empty rows AND plane. Source+ASCII exact key checks, FNV collision
handling, LRU order and old row byte-size/capacity meaning remain unchanged.

Before retention verify plane length equals all FTXUI cell widths, including
spaces/continuations, and plane bytes<=2*row bytes per entry and globally (K=2).
Account style backing separately; clear/eviction subtract it; copy/assignment
copy it and rebuild list iterators. Fixed headers are bounded by entry capacity.
Occupied hash collision/oversized/zero-capacity serves its own fresh payload
uncached. It never re-reads an occupied slot for styles. Store no Element, color,
mutable shared plane or view into cache-owned storage in a returned node.

Fresh finish maps one current palette snapshot and emits owned contiguous runs
of whole FTXUI glyphs with equal style. A wide glyph and combining tail cannot
split, disappear or carry inconsistent continuation styles. Concatenated runs
must reproduce complete original row bytes. Runs<=cells; prepay two planes,
R*(512+8*sizeof(string)), H*(256+4*sizeof(Element)), and copied UTF-8 bytes before
admitting a styled layout. Cache hits repeat only bounded fresh-node assembly,
not parser/layout work. Reuse the existing width-aware MermaidNode so narrow
fallback and later width restoration remain per-node and complete-source.

## Alternatives, verification and rollback

Doing nothing leaves all58 originals unadmitted. Bypassing the cache, caching
Elements/active colors or adding an external style side map loses existing key,
copy or theme guarantees. Thin nearest-segment contours discarded incident arms;
full independently authored boundary cells give a simpler precise terminal rule.
Root-without-apron failed the three-edge fan-out; the one-cell apron solves it
without changing source placement or allowing route overlaps.

B2 requires independent review of every full boundary fixture and this contract,
all58 feasible source-bound plans and both preset tests. B3 then requires Native
and Official to own their public five-frame captures before binding, compare full
glyph/foreground/background/bold planes, and reject meaningful good-first mutants.
Required cache controls include uncached/cached equality, blank fills, wide/combined
glyphs, exact source/ASCII keys, rejection, copy/assignment, eviction,clear,width
restore and all palette/flag changes. Collision static control-flow review is
mandatory; without a fixed real collision fixture, report its runtime gap rather
than add a mutable hash hook. B4 requires full Debug/Release builds/Mermaid suite,
all old630 originals/body593/source hashes preserved and independent dual admission.
No preflight/proposal report contributes drawing credit; 90% requires1931 originals.

Rollback removes only Block dispatch/parser/layout/plane changes and Block proofs,
restoring the prior630 behavior and old join sequence. No data migration, public
API change, global palette policy, shared Canvas change, commit or push is needed.
