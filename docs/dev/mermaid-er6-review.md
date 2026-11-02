# ER6 source projection and regression checkpoint

2026-10-30: six original ER matrix errors are admitted as complete drawings.
The immutable whole-source authority retains its 62 complete Models and 144
historical nulls. The independent source supplement supplies exactly ordinals
24, 65, 66, 77, 78 and 199; it preserves all entities, attributes, keys,
comments, cardinalities and ordered relationships. Markdown punctuation,
tildes and Unicode remain literal; only the three reviewed HTML break spellings
map to line breaks. No browser Markdown, ELK, CSS or accessibility equivalence
is claimed.

Source/design reviewer `agent:er6_source_design_review` approved the source
projection. Implementation reviewer `agent:er6_implementation_review` requested
additional BR/order and forged-proof controls, then approved after the resource/
exception mutation-credit loophole was closed. Both failure flags now prevent
mutation credit even when the admission flag is false. The original immutable
asset and production Work limits are unchanged. Larger Models use the explicit
same-ledger Rich1896 domain; the Legacy1849 domain retains its guard and charges.
The supplement is loaded/validated by a separate bounded source-asset ledger;
scene observation, comparison and mutation accounting remain on the scene Work.

Debug and Release each passed **836/836** Mermaid-labeled CTest tests (322.36s
and 37.37s). Their official matrices are byte-identical, SHA-256
`a562212e038a036729c9566feff74d967fa910224f9eec241d7f499edcf34507`.
Exactly six original observations improve and all other observations remain
unchanged: **629/2,145 drawings (29.32%)**, 1,501 fallback, 15 errors.
ER is **68/206 (33.01%)**, 138 fallback, zero errors. All fallback/error outcomes
remain failures. This is rendering success, not LLVM line/branch coverage.
No whole-project CTest or commit is claimed.

The exact [comparison](mermaid-er6-matrix-comparison.json),
[remaining cases](mermaid-unsuccessful-cases.md) and
[machine-readable inventory](mermaid-failure-inventory.json) retain every input.
The roster verifier passed against both build directories; only the six reviewed
ER source-unmet rows were removed from the official-matrix gate.

## Independent review records

### Source/design

**Verdict: APPROVE the bounded source-to-terminal projection; REQUEST_CHANGES for any claim of native admission or completed implementation based on that projection alone.** The six complete fixture files match their source bytes and SHA-256 values in the immutable [ER206 authority](/Users/zzzode/Develop/Loom/tests/fixtures/mermaid_native/er206_native_whole_source_authority.config62.v1.json). All six original `whole_model` values remain null. The [RFC 0030 source contract](/Users/zzzode/Develop/Loom/docs/rfcs/0030-terminal-mermaid-diagrams.md:398) already approves the six source Models and the literal terminal projection; it explicitly leaves concrete consumption, execution, and admission open.

The terminal contract preserves Markdown punctuation, tildes, Unicode and typed field roles literally. It converts only `<br>`, `<br/>`, and `<br />` to LF. This is a terminal text projection, with no claim that literal punctuation verifies browser Markdown, styling, fonts, SVG, or API behavior.

## Six source mappings

All Models have family `er`, direction `TB`, box entities, empty owners, groups and notes. Attribute order and key order are source order. The default and ELK variants in each pair have the same terminal Model; ELK remains an explicitly recorded terminal geometry adaptation, not a claim of browser ELK placement.

| Authority ordinal | Complete terminal Model content |
|---|---|
| **65, 66** | Entities: `person`, header `😀`, rows `(string, *first*Name, [], "")` and `(string, _**last**Name_, [], "")`; `a`, header `*Customer Account*`, row `(**string**, ema*i*l, [], "")`. One ordered relation `person → a`, solid, `card:one` to `card:zero-one`, label `__hẽ😀__`. |
| **77, 78** | One entity whose ID and header are `_**testẽζ➕Ø😀㌕ぼ**_`. Its five rows are listed below. No relations. |
| **24, 199** | Five entities and four ordered relations, listed below. Each relation is solid, from `p`, `card:one` to `card:zero-one`. The only display change in their labels is BR-to-LF. |

For **77/78**, the complete typed rows are, in order, `(type, name, keys, comment)`:

```text
("*__List~List~int~~sdfds__*", "**driversLicense**", ["PK"], "***The l😀icense #***")
("*string(99)~T~~~~~~*", "firstName", [], "Only __99__ \ncharacters are a\nllowed dsfsdfsdfsdfs")
("string", "last*Name*", [], "")
("string", "__phone__", ["UK"], "")
("int", "_age_", [], "")
```

For **24/199**, the complete entities are `p` / `Person` with `string firstName` and `string lastName`, and `a` / `Customer Account`, `b` / `Customer Account Secondary`, `c` / `Customer Account Tertiary`, and `d` / `Customer Account Nth`, each with `string email`. All keys and comments are empty. The ordered targets and projected labels are:

```text
a  "has\none"
b  "has\none\ntwo"
c  "has\none\ntwo\nthree"
d  "has\none\ntwo\nthree\n...\nNth"
```

Ordinal 24 also retains the source-owned `logLevel: 1` diagnostic-only disposition. Ordinals 66, 78 and 199 retain their `layout: elk` metadata and existing terminal adaptation. Descriptions, raw source spellings, configuration, original foreign predicates, and original nulls must remain independently bound. In particular, do not replace raw `<br />` spelling in source evidence with the normalized representation.

## Implementation and admission boundary

The existing ER literal decoder supports the three BR spellings and literal Markdown punctuation; the typed-table checker and multiline relation-label checker also provide relevant semantics. See [ER parsing](/Users/zzzode/Develop/Loom/src/ui/visual/mermaid_relations_core_impl.cpp:70) and the [relations oracle](/Users/zzzode/Develop/Loom/tests/support/mermaid_relations_oracle.hpp:482). That supports the **proposed mappings**, but does not establish that each current producer drawing passes complete geometry or that the active matrix consumes the new Models. A genuine test-first run must determine whether a producer or oracle correction is needed; an error in the inventory is not by itself a root-cause diagnosis.

Use the RFC’s bounded six-row, versioned test-private supplement, source-bound to all six full inputs, with the existing decoder and canonical mutation enumeration. Keep the legacy 1,849-byte guard and defaults unchanged; the two multiline relation Models need the separately bounded **1,896-byte** domain. Preserve the original 62 Models and 144 null obligations in the authority, and account for the six new Models separately until the active join is independently validated. Do not raise production source, canvas, or Work limits to accommodate the test asset.

Required negative and regression evidence includes:

- Mutate each complete header and each attribute type, name, key presence/value, comment and row presence/order; include literal punctuation, Unicode and BR location changes.
- Mutate each relation’s label, cardinalities, stroke, endpoint and presence; specifically reject missing, reordered or merged multiline label rows. Include a physical orphan-cell negative.
- Reject invalid UTF-8, unsupported HTML/entity syntax and malformed or unsupported BR variants through the applicable source/fallback contract; resource or decoder failures must not count as semantic rejection.
- Bind exact source identity and metadata before decoding; capture all five actual views, check full geometry and independent following content, narrow fallback protocol, and exact restoration. Retain genuine first failures.
- Verify actual allocation/clone and ledger fees, the fixed domain bounds, and old-default conservation. Run focused and complete native/public regressions in debug and release, with prior strong scenes and original authority records individually conserved. Active matrix joining and the RFC’s full-suite and real-terminal gates remain separate.

I did not edit files, run a renderer, or execute build and test gates. Thus this review approves the source mapping and bounded design only; it does not certify a drawing, foreign styling, an increased matrix numerator, or full coverage.

### Final implementation

**APPROVE** — no remaining blockers found in the narrow ER rich6 review.

The unconditional guard rejects mutation credit when either resource exhaustion or an exception is reported, including when admission is false. The source snapshot is checked against the captured input, original authority, manifest, and physical fixture; the effective rich pin is tied to the validated supplement and its fixed Model identity. The Legacy mutation domain retains its prior limit and charges, while Rich prepays the additional clone allowance. Supplement validation uses a separate bounded asset ledger; scene decoding, geometry, and mutations continue on the existing scene ledger without a reset.

This is a read-only code review. Final focused and full Debug and Release results remain to be verified; I did not run them.
