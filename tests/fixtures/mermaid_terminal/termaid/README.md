# Termaid terminal test inputs

Pinned upstream: `fasouto/termaid` tag `v0.9.1`, commit
`a10dfd506ec67dc11fce3f399c20a353202586ae` (MIT; full license in `LICENSE`).

`cases.json` contains 639 exact UTF-8 source identities: 165 distinct raw
fixtures (166 original fixture paths) and 474 distinct public `render` arguments.
Trailing-newline, indentation, and whitespace variants remain separate; duplicate
ASCII/Unicode/options invocations retain provenance without adding source cases.
The import observes all 1,210 `render` calls in the complete upstream suite:
1,614 tests passed. This is upstream extraction evidence, not Loom test success.

All original test sources, Mermaid fixtures, and 166 checked-in Unicode goldens
are preserved under `upstream/`. The upstream renderer is not linked into Loom.
`reference_outputs/` contains separately identified maintenance-time upstream
outputs for inline/option cases. These are never generated from Loom output.

Every source has an upstream typed semantic model. Dataclass names are recorded
as `__type__`; parent references are IDs, and enum values are explained in
`enum_catalog`. The semantic contracts predeclare labels, identities, shapes,
directed connections, endpoint markers, group membership, sequence order/scopes,
class/ER members, and ordered chart/hierarchy values where applicable. They do not
assert that these facts have already been verified in Loom's terminal pixels.

Original literal output assertions remain in `upstream_output_assertions`,
including exact upstream glyph choices. They require adaptation for Loom's
Unicode UI and retain original conditional/options context. They must not be
blindly treated as unconditional label assertions or rewritten from Loom output.
Foreign private parser/helper/style predicates remain original source evidence;
this data import does not claim those Python predicates run as C++ tests.

Twelve malformed/unknown/empty inputs are classified as robustness cases rather
than successful drawings. Upstream permissive parsing and silent omission are
not a required behavior: unexpected syntax, missing semantics, unsafe control
characters, and terminal width policy differences need explicit adjudication.
`provenance.json` records file hashes, counts, capture evidence, and the case-file
hash. Native verification and semantic snapshot adaptation are separate gates.
