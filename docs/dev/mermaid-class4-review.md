# Class4 accessibility parser and admission checkpoint

2026-10-30: root Class `accTitle` and `accDescr` were previously parsed as
implicit entities. The production parser now owns their standalone colon
statements before member/entity handling. Each value is retained independently
as 1–256 exact safe UTF-8 bytes after trimming outer ASCII spaces, separately
from the visible title. Duplicate, empty, invalid/control/markup/escape,
multiline, block and nested declarations return the complete source. Exact
reserved identifiers still work as relationship operands, including dotted
open/filled diamonds. No accessibility glyph/title is added. This is private
retention, not a public ARIA/screen-reader API or browser/theme equivalence.
All production budgets remain unchanged; fixed state, validation and allocation
are prepaid. Requirement projection also pays the enlarged private struct.

The independently source-reviewed supplement binds all four complete immutable
source/manifest tuples, both metadata values and all20 entities/19 ordered
relations. Historical152 complete/133 null facts and foreign debts are retained.
Runtime consumers invoke the real production metadata parser, then check all
complete rendered scenes and meaningful semantic/physical negatives. Source,
metadata, model, proof flags and resource/exception forgeries fail closed.

The implementation review initially requested preserving dotted diamond
operands; this was fixed and tested before final APPROVE. Four parser/source
controls and two consumer tests plus the original binding test pass. Source
privacy lint and external positive facade / negative private-type / negative
partition compilation checks pass. RFC lint passes all31 RFCs.

Release passed841/841 Mermaid tests in29.82s. Debug ran841 tests (331.25s),
then retried its seven failures caused solely by stale frozen roster counts;
all seven pass with corrected counts. A final official-matrix regeneration in
both presets includes accurate remaining counts. Full-project CTest and a
commit are not claimed. Earlier interrupted/pre-correction logs are not success
evidence.

Both matrices are byte-identical: `ff3e3a75847f4d2809c6f29ff400aaf64e832fe71fd5b86736a4883104a26f6c`.
Drawn **633/2145 (29.51%)**, fallback1501, errors11. Class is **241/285
(84.56%)**, fallback44, errors0. Fallback/error both count as failure. Exactly
the four original Class observations improve over the already verified ER6
checkpoint; all other observations remain unchanged. See the
[comparison](mermaid-class4-matrix-comparison.json) and
[remaining cases](mermaid-unsuccessful-cases.md). The roster removes exactly
these four rows from each of the official-matrix and Class285 gates.

## Independent review records

### Design/PRR

**APPROVE** the proposed bounded policy as a nonvisual terminal adaptation, subject to the implementation contract and gates below. This approval covers the design, not production admission or a claim of browser, screen-reader, ARIA, theme, or upstream rendering equivalence.

The parser currently reaches ordinary Class operand/member parsing for `accTitle: …` and `accDescr: …`; that path creates an entity before consuming the colon member. The accessibility declarations therefore need an owning branch before that path. The existing ER policy is a useful precedent for a closed plain-text profile, but it consumes metadata without retaining it and also supports a description block, so it should not be treated as identical behavior.

The implementation contract is:

- Recognize the exact, case-sensitive `accTitle` and `accDescr` colon declarations only as standalone, root-scope Class statements. Define the retained value as the text after the colon with outer ASCII spaces removed; preserve its remaining UTF-8 bytes exactly. Require 1–256 bytes after trimming and validate with the existing Unicode safety rules. Reject controls, invalid UTF-8, and ER’s unsupported plain-text characters `<`, `>`, `&`, backslash, and backtick. Do not decode markup or escapes.
- Claim these declarations before implicit entity/member parsing. Reject duplicate setters, empty values, block forms, malformed declaration-shaped input, and declarations in namespaces or class bodies as whole-source fallback. Define the lexical boundary carefully: longer identifiers and legitimate Class operands must not be captured merely because they begin with an accessibility keyword. Test that distinction explicitly. Check physical line boundaries and CRLF handling so preprocessing cannot turn a malformed multiline declaration into an accepted one.
- Store each value as an independent optional owned string in private `RelationDiagram` fields, separate from `diagram_title`. Prepay fixed state, validation, and owned-string allocations against the same production `Work` before allocation; exhaustion must reject the complete source. A minimal reviewed testing facade may return the two optionals and use the caller’s `Work`, but must invoke the production parse path rather than duplicate its logic.
- Do not emit accessibility glyph rows or turn either value into a visible title. Retention is internal to parsing; the public rendered diagram does **not** expose these values through an accessibility API. Original source remains available through the existing message-copy path, and whole-source fallback preserves it when parsing or layout fails. Avoid describing the private fields as public accessibility support.
- Preserve all original null Model facts and foreign assertion debts. A separate source-pinned Class4 supplement must bind each exact source and manifest record, both retained metadata values, and the complete ordered 20-entity/19-edge projection described in the source review. It must distinguish absent metadata from an empty or changed value and retain the original presentation’s absent visible title.

Before admitting the four sources, require positive value-retention tests, including Unicode and exact whitespace behavior; independent removal and mutation of each setter; duplicate, empty, malformed, block, nested-scope, identifier-boundary, invalid UTF-8/control, 256/257-byte, and production-budget exhaustion controls. Public whole-scene tests must establish the complete graph, no phantom accessibility entities, no visible title, and complete-source fallback for rejected inputs. Run the full **833 regression** and the repository-required debug and release builds and tests; report actual results rather than treating this review as execution evidence.

This is a read-only design review. No production changes or tests were run.

### Source projection

**APPROVE** — `class4_source_model_review`

The [source supplement](/Users/zzzode/Develop/Loom/tests/fixtures/mermaid_native/class285_accessibility4_source_supplement.v1.json) matches the four complete candidate sources and their manifest records for ordinals 263, 264, 271, and 284. The paths, global ordinals, byte counts, and source hashes match; each original candidate remains whole-null.

All four rows contain the same complete 20-entity projection, with the expected ordered attribute and method rows, and all 19 ordered relationships with their marker orientation, stroke, labels, and cardinality captions. The stored model hashes also match the canonical models.

Each row separately retains `This is a title` and `This is a description`, while the visible presentation title remains null. The supplement’s qualification retains the original browser, ARIA, theme, and foreign assertion debts and leaves runtime admission separate.

This verdict covers source projection and design only. It does not establish rendered output, runtime admission, or original foreign assertion success. No files were modified.

### Final implementation

**APPROVE — class4_implementation_review.** No remaining material findings in the Class4 changes reviewed.

The dotted diamond boundary now preserves both `accTitle` and `accDescr` as relationship operands for `o..` and `*..`, with controls through the production parser and public rendering. The metadata path retains the two values separately, charges `Work` before owned-string allocation, and rejects unsupported declarations through whole-source fallback.

The official and native consumers bind the four sources to the pinned supplement and production metadata parse. Their admission guards require the complete scene proof and reject resource exhaustion, exceptions, and forged metadata. The original null model facts remain intact; the separate supplement preserves the source projection and foreign assertion debts.

**Runtime evidence:** I did not run tests or modify files. The reported seven initial targeted tests passed; the updated focused tests and final debug/release regressions were still running at the time of this review. Their results are separate from this verdict.
