# Merman promotion candidate (review only)

This candidate merges 1,532 additional exact UTF-8 inputs into the frozen
100-case source corpus. All 100 original case IDs, source identities, expectation
facts, primary oracle references, and provenance prefixes remain unchanged.
Repeated source variants/options are merged by exact SHA256; no whitespace
normalization or typed-model-to-source synthesis occurs.

## Independently declared native requirements

The candidate has 1,632 source inputs: 1,605 diagrammatic-family drawing
obligations, 18 structured-family semantic-text obligations, 4 explicit fallback
obligations for upstream-unsupported terminal families, and 5 negative probes.
These requirements come from successful strict public source admission and the
complete public `merman-cli capabilities --json` document for 0.9.1. They are
requirements to implement/check in Loom, **not claims that these drawings pass**.
The complete capability document includes dynamically admitted Swimlane, which a
static source-table extractor omitted in the initial runtime handoff.

## Preserve observation separately from obligations

The actual runtime observations are 11 primary diagrammatic reports, 1 primary
structured-text report, 8 String results in declared structured-only families,
7 fallback-only results, 1,366 String results whose projection is unresolved,
213 source inputs without an unambiguous ASCII binding, 21 foreign policy/render
errors, and 5 parse errors. These categories total 1,632.

The immutable runtime handoff previously called 16 sources `structured_text`:
that combined the 1 primary report and 8 family-declared String results with
**7 explicit StructuredText_Fallback-only sources**. This candidate corrects the
interpretation in `upstream_observation.category`, preserving the original kind
counts and original classification for audit. Neither a fallback nor a String
result becomes observed diagrammatic evidence. Original runtime README/counts
remain historical extraction artifacts; this promotion note is the corrected
classification and must accompany them.

`expectation.native_requirement` states the desired Loom behavior.
`expectation.upstream_observation` retains measured projection/outcome and raw
classification. `declared_primary_family_projection` is a third, family-level
axis. These three concepts must not be substituted for one another.

## Public-source denominator and excluded private data

All 1,632 inputs were actual complete arguments at the public typed-parse API,
with `suppress_errors=false`. Of them 1,627 were strictly accepted; official
Mermaid.js admission was not independently rerun. Five are explicit public-text
negative probes: four internal layout shape names that must be rejected and one
documented upstream-invalid local sequence stress fixture. They remain a
separate negative denominator, rather than positive syntax coverage.

The 519 source-free/modified typed-model rendering **invocations** are not source
cases and receive no fabricated Mermaid text. Four expected-output literals and
two assertion-failure messages are likewise outside the source denominator.
The 170 ambiguous rendering invocations remain recorded without attributing
success to an arbitrary equivalent source. Two combining-mark source cases and
one bidi-control source case are explicit native width/disclosure policy cases;
they must not silently pass as ordinary label output or source fallback.

## Proposed promotion sequence

1. Independently review this candidate and the immutable runtime handoff.
2. Copy its existing runtime-capture assets unchanged into the vendor directory.
3. Apply only this candidate's cases.json, promotion-obligations.json,
   full-public-capabilities.json, PROMOTION.md, and maintenance merger.
4. Keep historical source manifests/locks as provenance; create a reviewed
   promotion inventory and only then regenerate the parent-owned root lock.
5. Run native JSON/text consumers. Normal tests need no foreign renderer,
   Python, Rust, or Node runtime.

This script writes scratch output only. It never modifies the real vendor tree,
root lock, production renderer, build files, or Git state. Re-running with the
same three input hashes yields identical output. Existing fixture assets and
runtime-capture models/assertions/references are shared rather than duplicated.

## Complete original source availability

`upstream/COMPLETE_ORIGINAL_SOURCE_IMPORT.v1.json` records the closed set of
128 original Merman source files referenced by the 1,376 pinned test-function
records. This import adds the 83 previously absent `upstream/src/**` files
verbatim from the same v0.9.1 commit; the other 45 were already present. Each
file has byte, SHA-256, and Git-blob pins. The historical `handoff-lock.json`
remains a 204-file handoff record and is not reinterpreted as this import lock.

These source bytes are review provenance only. They are not compiled, executed,
rendered, or parsed by Loom, and they grant no parser, geometry, semantic,
runtime, or conformance credit. After an independent review, regenerate the
parent inventory with the command in the source-import record and update only
the test-private inventory digest that pins that reviewed root lock.
