#!/usr/bin/env bash
# show_conformance.sh — build, run the markdown conformance suites, and open
# the visual dashboard in your browser.
#
# Usage:
#   scripts/show_conformance.sh              # all three suites
#   scripts/show_conformance.sh GfmExtensions # single suite (gtest filter)
#
# Output: /tmp/conformance_report.html (self-contained, opened in browser).

set -euo pipefail

# Repo root = parent of this script's directory.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build/debug"
JSON="/tmp/conformance.json"
REPORT="/tmp/conformance_report.html"
FILTER="${1:-}"

# 1. Build.
cmake --build --preset debug -j8 >/dev/null

# 2. Run suites (optionally filtered) and dump JSON.
ARGS=("--conformance-json" "$JSON")
if [[ -n "$FILTER" ]]; then
  ARGS+=("--gtest_filter=MarkdownConformance.$FILTER")
fi
"$BUILD/tests/test_markdown_conformance" "${ARGS[@]}"

# 3. Embed JSON into the dashboard.
python3 - "$ROOT/tests/conformance_dashboard.html" "$JSON" "$REPORT" <<'PY'
import json, sys
html = open(sys.argv[1]).read()
data = json.load(open(sys.argv[2]))
embedded = json.dumps(data, ensure_ascii=False)
# Escape sequences that would confuse the HTML parser inside <script>:
#   </script  →  <\/script   (prevents premature script-tag closure)
#   <!--      →  <!--   (prevents script-data-escaped state)
# Both are valid JS string escapes; the browser sees the harmless form,
# the JS engine decodes them back to the original characters.
embedded = embedded.replace('</script', '<\\/script')
embedded = embedded.replace('<!--', '<\\u0021--')
html = html.replace('<script>', f'<script>window.EMBEDDED_DATA = {embedded};</script>\n<script>', 1)
html = html.replace('// ── File loading ──', 'if (window.EMBEDDED_DATA) render(window.EMBEDDED_DATA);\n\n// ── File loading ──')
open(sys.argv[3], 'w').write(html)
PY

# 4. Open in browser.
if [[ "$(uname)" == "Darwin" ]]; then
  open "$REPORT"
else
  xdg-open "$REPORT" 2>/dev/null || echo "Report written to $REPORT"
fi
