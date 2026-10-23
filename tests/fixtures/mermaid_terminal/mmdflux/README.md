# Mmdflux terminal test inputs

Pinned upstream: `kevinswiber/mmdflux` tag `mmdflux-v2.6.1`, commit
`c7405b64865938b2df4d33d26389d51691409913` (MIT; full license in `LICENSE`).

`cases.json` contains 292 exact UTF-8 Mermaid source identities across flowchart,
sequence, class, and state diagrams. Every one of the 185 checked-in `.mmd`
fixture paths is preserved. An observer around the public `render_diagram`
facade in an isolated scratch copy captures dynamic loops and formatted strings:
2,549 Text/Ascii invocations, including 2,525 Mermaid invocations; 24 MMDS/custom
registry payload invocations are accounted separately. The complete upstream
`cargo test --locked --lib --tests` run passed 3,241 tests, with 8 ignored.
These figures establish extraction coverage, not Loom rendering correctness.

Original integration assertion sources, fixtures and Unicode text snapshots are
unchanged under `upstream/`. Full upstream production implementation files are
not copied. `assertion_sources/` contains only 126 exact test-function byte slices
for owner-local tests that invoke the public facade. Their original file hash,
Git blob, byte offsets, and line spans are recorded in `provenance.json`.
Other private/non-terminal tests are indexed separately and are not claimed as
ported native predicates. SVG-only snapshots and web UI tests are excluded.

`reference_outputs/` preserves maintenance-time upstream text/ASCII/configuration
outputs, separately from original committed goldens. Output-format or foreign
configuration variants do not inflate the count of source identities. Neither
the Rust crate nor its executable is a Loom runtime/test dependency.

The pinned release CLI's MMDS output supplies typed semantic models for 290
inputs. Semantic contracts identify labels, node shapes, directed endpoints,
line/arrow styles, nested grouping, participant/message order, and activation
facts. Exact upstream layout snapshots remain distinct from these obligations.
Two model gaps are explicit: the malformed `graph TD\n!!!` error case, and the
SVG dynamic-font fixture `multi_font_styles.mmd`, whose model request requires
an upstream dynamic font-metrics provider. The latter has no upstream terminal
snapshot/public rendering observation and must not be silently counted as a
verified terminal drawing. Its raw input remains in the union for audit.

One malformed input is classified as an error; 291 inputs retain drawing intent.
Unsafe control characters, unsupported directives, and differing width policies
require explicit product decisions. Returning source fallback or nonempty text
is not a semantic drawing pass. Native verification is a separate gate.
