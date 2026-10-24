#!/usr/bin/env bash
# RFC 0031 E6d Mermaid libFuzzer runner.
#
# Usage:
#   tests/fuzz/run_mermaid_fuzz.sh           # 10-minute graduation run
#   tests/fuzz/run_mermaid_fuzz.sh smoke     # 500 deterministic seed executions
#
# Seeds are read from the hand-written edge corpus and the pinned
# owned-family originals (read-only; the mutable generated corpus and crash
# artifacts stay under tests/fuzz/{corpus-out,artifacts}).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$REPO/build/fuzz/tests/fuzz/fuzz_mermaid_parse"
OUT="$REPO/tests/fuzz/corpus-out"
ART="$REPO/tests/fuzz/artifacts"
mkdir -p "$OUT" "$ART"

# Darwin has no LeakSanitizer; everything else (Linux) keeps it on.
if [[ "$(uname -s)" == "Darwin" ]]; then
  export ASAN_OPTIONS="halt_on_error=1:detect_leaks=0"
else
  export ASAN_OPTIONS="halt_on_error=1:detect_leaks=1"
fi
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"

SEED_DIRS=(
  "$OUT"
  "$REPO/tests/fuzz/corpus/mermaid"
  "$REPO/tests/fixtures/mermaid_official/cases/flowchart"
  "$REPO/tests/fixtures/mermaid_official/cases/sequence"
  "$REPO/tests/fixtures/mermaid_official/cases/class-diagram"
  "$REPO/tests/fixtures/mermaid_official/cases/er-diagram"
  "$REPO/tests/fixtures/mermaid_official/cases/state-diagram"
  "$REPO/tests/fixtures/mermaid_official/cases/state-diagram-v2"
  "$REPO/tests/fixtures/mermaid_official/cases/requirement"
  "$REPO/tests/fixtures/mermaid_official/cases/pie"
  "$REPO/tests/fixtures/mermaid_official/cases/xychart"
  "$REPO/tests/fixtures/mermaid_official/cases/git"
)

if [[ "${1:-}" == "smoke" ]]; then
  exec "$BIN" -runs=500 -seed=1 -max_len=65536 "${SEED_DIRS[@]}"
fi

exec "$BIN" \
  -max_total_time=600 \
  -max_len=65536 \
  -timeout=20 \
  -rss_limit_mb=1024 \
  -malloc_limit_mb=64 \
  -print_final_stats=1 \
  -artifact_prefix="$ART/" \
  "${SEED_DIRS[@]}"
