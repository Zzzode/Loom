# Beautiful Mermaid terminal test assets

Upstream: https://github.com/lukilabs/beautiful-mermaid

Release: `v1.1.3`; commit `65f4e0ab8c289e9691a9e5bc4f9eac7108cf449b`.
The npm 1.1.3 `gitHead` matches that tag. All assets are MIT licensed; the full
upstream license is in `LICENSE`.

## What is preserved

- All 22 original `src/__tests__/*.test.ts` files, including their assertion text.
- All 100 original terminal golden files: 63 ASCII and 37 Unicode.
- `provenance.json`: source paths, exact SHA-256 hashes, byte lengths and pin.
- `test_inventory.json`: all 597 static test declarations, with helper/parser,
  layout, SVG and public-terminal categories distinguished.
- `cases.json`: 128 unique public-terminal drawing inputs, deduplicated by exact
  input bytes; all original golden and inline-render provenance remains attached.

The 128 inputs comprise 66 flowcharts, 29 class diagrams, 22 XY charts, six
sequence diagrams and five ER diagrams. Inputs preserve their meaningful source
whitespace. The 86 statically resolved inline render calls were extracted with a
scratch TypeScript AST tool, including the finite interpolated long-label case.
Three unresolved call expressions are the golden fixture runner and its two
aggregate diagonal checks; all their input files are already included.

## Independent expectations

Every positive case requires a genuine drawing. Empty output or a Mermaid source
fallback is a failure. `expectation.snapshots` preserves the upstream expected
rendering and foreign layout/ASCII options, while `expectation.assertions` retains
151 direct contains/not-contains facts. Relational and row/order assertions remain
in `expectation.obligations` and the original source. Porting a semantic predicate
is distinct from claiming exact foreign glyph, spacing, theme or private API
parity. Production Unicode rendering does not require adding an ASCII API.

`expectation.semantic_model` was serialized using only the release-pinned upstream
parser as an offline maintenance oracle. It preserves node IDs/labels/shapes,
relationships and endpoint markers, styles, groups, actor/message order, notes,
fragments and chart data. No parser, layout or rendering implementation is
vendored here; native tests read data only and require no Bun, JavaScript or Go.
The models are supporting parser facts. Original source and assertion/golden
semantics take precedence over permissive or lossy upstream behavior.

## Upstream execution evidence

The exact release suite ran with scratch Bun 1.3.9: **711 tests passed, zero failed,
1739 expect calls across 22 files**. The five dedicated terminal suites account
for 187 tests: 186 public drawing tests and one validator-helper test. The other
524 tests cover nonterminal/parser/layout/style behavior and are accounted for,
without being counted as terminal rendering coverage.

Log from the development run:
`build/mermaid-renderer-research-20261019/beautiful-1.1.3-upstream-tests.log`.
These are upstream results, not a claim that Loom's native port passes them.
