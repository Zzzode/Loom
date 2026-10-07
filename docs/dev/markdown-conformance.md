# Markdown Conformance Suite

Architecture and operations guide for the CommonMark 0.31.2 + GFM conformance
suite that validates Loom's markdown parser against the official spec examples.

## Overview

The suite renders official spec markdown through Loom's **production parser**
(`lex_blocks` + `tokenize_inline` — the same AST the terminal renderer
consumes), serializes the result to HTML, and compares against the spec's
expected HTML. It exists to catch parser regressions and to measure conformance
against the reference implementation (cmark-gfm).

**Current baseline (2026-10-06):**

| Suite | Pass | Total | % |
|---|---|---|---|
| CommonMark 0.31.2 | 652 | 652 | 100.0% |
| GFM core | 668 | 672 | 99.4% |
| GFM extensions | 30 | 30 | 100.0% |
| **Total** | **1350** | **1354** | **99.7%** |

The 4 failures are all spec-fixture issues that cmark-gfm also cannot pass —
see [Failure categorization](#failure-categorization).

## Architecture

```
tests/fixtures/*.json          ← official spec examples (JSON arrays)
        │
        ▼
test_markdown_conformance.cpp  ← gtest runner
        │
        ├─ parse JSON → {markdown, html, example, section} per example
        │
        ├─ loom::ui::render_markdown_to_html(markdown, gfm_extensions)
        │       │
        │       ├─ detail::lex_blocks(source, gfm_extensions)
        │       │       └─ scan_link_ref_defs / scan_footnote_defs (pre-scan)
        │       │       └─ main block loop (paragraphs, lists, blockquotes, …)
        │       │       └─ tokenize_inline per paragraph (links, emphasis, …)
        │       │
        │       └─ HTML serializer (test-only, see below)
        │               └─ render_block_to_html / render_inlines_to_html
        │               └─ render_footnotes_section (GFM footnotes)
        │
        ├─ normalize_html(actual) vs normalize_html(expected)
        │
        └─ floor check: passed >= kSuiteFloor ? PASS : FAIL
```

### Key design decisions

1. **Production parser, test-only serializer.** The suite calls
   `lex_blocks` + `tokenize_inline` — the same code path the terminal renderer
   uses. If the parser produces a wrong AST, the suite catches it. The HTML
   serializer (`markdown_html_impl.cpp`) exists only because the spec examples
   express expected output as HTML; the production FTXUI renderer outputs
   Elements, not HTML.

2. **Whitespace-insensitive comparison.** HTML is whitespace-insensitive
   outside `<pre>` blocks. The normalizer collapses text-node whitespace,
   lowercases tag names, and preserves `<pre>` content verbatim. This mirrors
   the CommonMark `normalize.py` approach.

3. **Floors as regression gates.** Each suite has a pass-count floor
   (`kCommonMarkFloor`, `kGfmFloor`, `kGfmExtensionsFloor`). The suite FAILS
   if the parser drops below the floor. Floors are raised as parser fixes
   land; they are never lowered.

4. **GFM extensions toggle.** CommonMark runs with `gfm_extensions=false`;
   GFM core and extensions run with `gfm_extensions=true`. This mirrors
   cmark-gfm's `--extensions` flag.

## Fixtures

Three JSON files under `tests/fixtures/`, each an array of
`{markdown, html, example, section}` objects:

| File | Examples | Source |
|---|---|---|
| `commonmark_spec.json` | 652 | CommonMark 0.31.2 spec (CC BY-SA 4.0) |
| `gfm_spec.json` | 672 | GFM core spec (GitHub Flavored Markdown) |
| `gfm_extensions.json` | 30 | GFM extensions spec (tables, strikethrough, autolinks, task lists, footnotes) |

The GFM fixtures were converted from the spec's `spec.txt` format. The `→`
(U+2192) character in HTML fields represents a tab and is already converted
in the JSON. The original `.txt` files are kept alongside for reference.

Fixture paths are resolved relative to the test file (`__FILE__`), so the
suite works regardless of ctest's working directory.

## HTML serializer

`src/ui/visual/markdown_html_impl.cpp` — impl unit of
`loom.ui.visual.markdown`. File-local render helpers walk the BlockToken /
InlineToken AST and emit standard HTML.

### Why a separate serializer?

The production renderer (`markdown_render_impl.cpp`) outputs FTXUI Elements
for terminal display. The spec examples express expected output as HTML. A
separate serializer lets the suite compare against the spec without
duplicating parser logic.

### What it handles

- All CommonMark block types: paragraphs, headings, code blocks, lists
  (tight/loose, ordered/unordered, task items), blockquotes, tables,
  horizontal rules, HTML blocks, math blocks
- All CommonMark inline types: text, bold, italic, code, links, images,
  escapes, raw HTML
- GFM extensions: tables (with alignment), strikethrough (re-tokenized for
  nested constructs), autolinks (mailto:/xmpp:, non-ASCII domains, short
  domains), task list items, footnotes (refs + definitions + backrefs)
- GFM same-type emphasis flattening (`****foo****` → `<strong>foo</strong>`,
  not `<strong><strong>foo</strong></strong>`)

### Footnote rendering

Footnote numbering is deferred to the serializer (not the lexer) because
`tokenize_inline` is called eagerly during block parsing and the full
document's reference order isn't known until all blocks are parsed. This
mirrors cmark-gfm's `process_footnotes` post-parse phase:

1. `collect_footnote_defs` recursively walks the token tree and collects
   `FootnoteDef` blocks (first-def-wins by normalized label).
2. As `FootnoteRef` tokens are encountered during rendering, display numbers
   are assigned in first-reference order and ref counts are tracked.
3. After all body blocks are rendered, `render_footnotes_section` emits
   `<section class="footnotes" data-footnotes>` with `<li>` entries in
   reference order. The backref is placed inside the last paragraph (before
   `</p>`) or directly in `<li>` if the last block is not a paragraph.
4. The section loop is index-based (not a snapshot) so that footnote refs
   inside footnote def content are picked up correctly.

## Normalization

`normalize_html()` in the test file implements whitespace-insensitive
comparison:

- **Global CRLF → LF** first, so per-state loops don't need lookahead.
- **Tag parsing**: split into tag name + remainder; lowercase the name,
  collapse whitespace in the remainder.
- **Text nodes**: collapse runs of spaces/tabs/newlines/form-feeds to a
  single space; trim leading/trailing.
- **`<pre>` blocks**: whitespace is significant — preserved verbatim (only
  CRLF → LF).
- **`<IGNORE>`**: the GFM spec uses `<IGNORE>` for crash-test examples; any
  output is accepted.

## Floor system

```cpp
constexpr int kCommonMarkFloor = 652;       // 100.0% of 652
constexpr int kGfmFloor = 667;              // 99.3% of 672
constexpr int kGfmExtensionsFloor = 29;     // 96.7% of 30
```

- The suite **FAILS** if `report.passed < floor` (regression).
- Passing at exactly the floor is the status quo (not a failure).
- When a parser fix lands and the pass count increases, raise the floor to
  the new count. This prevents future regressions from silently undoing the
  fix.
- Floors are pass-counts, not percentages, so adding new examples to the
  fixtures doesn't change the floor (but the total does).

## Failure categorization

The 6 remaining failures fall into 3 categories, all of which cmark-gfm also
cannot pass:

### 1. Spec fixture format stale (GFM ex 279, 280)

The GFM core spec uses an older checkbox HTML format
(`<input disabled="" type="checkbox">`), while the GFM extensions spec (ex
28–30) and cmark-gfm use the newer format
(`<input type="checkbox" disabled="" />`). Loom matches the new format.
Changing to the old format would break extensions ex 28–30 (net −1).

### 2. Spec fixture self-contradiction (GFM ex 619, 620)

These are CommonMark examples included in the GFM spec. They expect bare
URLs/emails to NOT be linked, but the suite runs with `gfm_extensions=true`
(which enables autolinking). Running the GFM suite without extensions would
break all GFM-specific tests (tables, strikethrough, task lists, etc.).

### 3. Tagfilter on HTML blocks (GFM ex 652, extensions ex 22)

The GFM tagfilter extension escapes "dangerous" HTML tags (`<script>`,
`<style>`, `<xmp>`, `<textarea>`, etc.) by replacing the leading `<` with
`&lt;`. Loom applies tagfilter to both inline HTML (during tokenization)
and HTML block content (during HTML serialization), with one exemption:
**Type 1 HTML blocks** (`<script>`, `<pre>`, `<style>`, `<textarea>`) are
raw-text elements — their content is not parsed as HTML, so the tagfilter
does not apply to them. This resolves the tension between GFM example 652
(which expects `<xmp>` escaped inside a `<blockquote>` Type 6 block) and
CommonMark examples 140–145 (which expect `<script>`/`<style>` unescaped
in Type 1 blocks).

## How to run

```bash
# Build
cmake --build --preset debug -j8

# Run all three suites
./build/debug/tests/test_markdown_conformance

# Run a single suite
./build/debug/tests/test_markdown_conformance \
  --gtest_filter='MarkdownConformance.GfmExtensions'

# Run via ctest
ctest --preset debug -R MarkdownConformance
```

## Visualization (JSON output + HTML dashboard)

The test runner can dump results as structured JSON, which
`tests/conformance_dashboard.html` renders as an interactive dashboard.

### Quick start

```bash
scripts/show_conformance.sh
```

This builds the test runner, runs all three suites, embeds the results into
the dashboard, and opens it in your browser. The report is a self-contained
HTML file at `/tmp/conformance_report.html` — no file picker, no
drag-and-drop, no server.

To view a single suite:

```bash
scripts/show_conformance.sh GfmExtensions
```

### Alternative: drag-and-drop (no python3 needed)

If you don't have python3, you can load the JSON manually:

```bash
cmake --build --preset debug -j8
./build/debug/tests/test_markdown_conformance --conformance-json conformance.json
open tests/conformance_dashboard.html    # macOS; xdg-open on Linux
```

Then drag `conformance.json` onto the drop zone in the browser (or click to
browse). The dashboard is a single self-contained HTML file with **zero
external dependencies** — no CDN, no server, no build step. It works offline.

### Generating JSON

```bash
./build/debug/tests/test_markdown_conformance \
  --conformance-json conformance.json
```

The flag is consumed before gtest sees it, so it composes with
`--gtest_filter`:

```bash
# Only the extensions suite, JSON output
./build/debug/tests/test_markdown_conformance \
  --gtest_filter='MarkdownConformance.GfmExtensions' \
  --conformance-json ext.json
```

### JSON schema

```json
{
  "timestamp": "2026-10-05T09:18:42Z",
  "suites": [
    {
      "name": "CommonMark 0.31.2",
      "total": 652,
      "passed": 652,
      "failed": 0,
      "pass_rate": 100.0,
      "floor": 652,
      "by_section": {
        "Tabs": {"passed": 3, "total": 3},
        "Emphasis and strong emphasis": {"passed": 132, "total": 132}
      },
      "failures": [
        {
          "example": 279,
          "section": "Task list items (extension)",
          "markdown": "- [ ] foo\n- [x] bar\n",
          "expected": "<ul>\n<li><input disabled=\"\" ...",
          "actual": "<ul>\n<li><input type=\"checkbox\" ..."
        }
      ]
    }
  ]
}
```

| Field | Type | Description |
|---|---|---|
| `timestamp` | string | ISO 8601 UTC generation time |
| `suites[].name` | string | Suite display name |
| `suites[].total` | int | Total examples in the fixture |
| `suites[].passed` | int | Examples that matched spec HTML |
| `suites[].failed` | int | `total - passed` |
| `suites[].pass_rate` | float | `100 * passed / total`, 1 decimal |
| `suites[].floor` | int | Regression gate (pass-count floor) |
| `suites[].by_section` | map | Section name → `{passed, total}` |
| `suites[].failures[]` | array | Per-failure detail (empty if all pass) |
| `failures[].example` | int | Spec example number |
| `failures[].section` | string | Spec section name |
| `failures[].markdown` | string | Raw markdown input |
| `failures[].expected` | string | Spec expected HTML |
| `failures[].actual` | string | Loom's actual HTML output |

### What the dashboard shows

- **Summary cards** — one per suite, with pass rate (color-coded),
  passed/total, and floor status (✓ or ✗).
- **Section bar chart** — horizontal bars for sections with failures,
  sorted worst-first. Green = 100%, yellow = 90–99%, red = <90%.
- **Failure cards** — expandable cards per failure, showing the markdown
  input, expected HTML, and actual HTML in side-by-side `<pre>` blocks.
  Known fixture issues are auto-categorized with a badge (e.g. "Spec
  fixture format stale", "Tagfilter trade-off").
- **Dark mode** — follows the system `prefers-color-scheme`.

### CI integration

The `--conformance-json` flag is safe to use in CI. The JSON is written
after all tests complete (even if some fail), so the dashboard always
reflects the full run. A typical CI step:

```yaml
- name: Run conformance suite
  run: |
    cmake --build --preset release -j8
    ./build/release/tests/test_markdown_conformance \
      --conformance-json conformance.json
- name: Upload conformance report
  uses: actions/upload-artifact@v4
  with:
    name: conformance-report
    path: conformance.json
```

## How to extend

### Adding a new fixture

1. Add the JSON file to `tests/fixtures/`.
2. Add a `TEST(MarkdownConformance, ...)` in `test_markdown_conformance.cpp`.
3. Set the floor to the initial pass count (even if low).
4. Raise the floor as parser fixes land.

### Raising a floor

1. Run the suite and note the new pass count.
2. Update the `constexpr int k*Floor` in `test_markdown_conformance.cpp`.
3. Update the comment above the floors.
4. Update the baseline table in this document.

### Adding a new GFM extension

1. Add the `InlineTokenKind` / `BlockTokenKind` enum values to
   `markdown.cppm`.
2. Implement lexing in `markdown_lexer_impl.cpp`.
3. Add rendering cases to `markdown_html_impl.cpp` (HTML serializer).
4. Add no-op cases to `markdown_render_impl.cpp` and
   `markdown_api_impl.cpp` (production renderer — `-Wswitch` under
   `-Werror` requires all cases covered).
5. Add conformance examples to the fixtures and raise the floor.

## Known limitations

- **HTML serializer is test-only.** It is not wired into production paths.
  The production FTXUI renderer handles all block/inline types but does not
  render footnotes as superscript links (they appear as literal `[^label]`
  text).
- **Entity decoding in labels.** Loom decodes HTML entities after the `[`
  handler, so `[^a&amp;b]` stores the raw label (pre-existing architectural
  difference, affects links too).
- **Footnote refs inside footnote defs.** The index-based section loop
  handles this correctly, but mutually-recursive footnotes (`a` refs `b`,
  `b` refs `a`) may render in an unexpected order. Rare edge case.
- **No visual diff in terminal output.** The test output shows expected vs
  actual HTML as text. For a visual dashboard, see
  [Visualization](#visualization-json-output--html-dashboard) above.

## Files

| Path | What |
|---|---|
| `tests/test_markdown_conformance.cpp` | gtest runner: fixtures, normalization, floors, `--conformance-json` output |
| `tests/fixtures/commonmark_spec.json` | 652 CommonMark 0.31.2 examples |
| `tests/fixtures/gfm_spec.json` | 672 GFM core examples |
| `tests/fixtures/gfm_extensions.json` | 30 GFM extensions examples |
| `src/ui/visual/markdown.cppm` | Module interface: AST types, `normalize_label` |
| `src/ui/visual/markdown_lexer_impl.cpp` | Block + inline lexer (production parser) |
| `src/ui/visual/markdown_html_impl.cpp` | HTML serializer (test-only) |
| `src/ui/visual/markdown_render_impl.cpp` | FTXUI terminal renderer (production) |
| `src/ui/visual/markdown_api_impl.cpp` | Public API + caching (production) |
| `tests/conformance_dashboard.html` | Visual dashboard (loads JSON, zero dependencies) |
