#!/usr/bin/env bash
# format-cpp.sh — run the pinned clang-format across the source tree
#
# Reads the exact version from .clang-format-version for macOS / Linux parity.
# Resolves the binary in this order:
#   1. CLANG_FORMAT env override
#   2. clang-format-21 on PATH (Linux)
#   3. Homebrew llvm@21 (Apple Silicon or Intel macOS)
#   4. clang-format on PATH
#
# Usage:
#   scripts/format-cpp.sh           # rewrite files in place
#   scripts/format-cpp.sh --check   # dry-run, exit non-zero on diff (for CI)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

REQUIRED_VERSION="$(< "${ROOT_DIR}/.clang-format-version")"
REQUIRED_MAJOR="${REQUIRED_VERSION%%.*}"

mode="write"
for arg in "$@"; do
    case "${arg}" in
        --check|-n) mode="check" ;;
        -h|--help)
            sed -n '2,15p' "$0"
            exit 0
            ;;
        *) echo "unknown argument: ${arg}" >&2; exit 2 ;;
    esac
done

if [[ ! "${REQUIRED_VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "error: .clang-format-version must contain one exact major.minor.patch version." >&2
    exit 1
fi

resolve_clang_format() {
    if [[ -n "${CLANG_FORMAT:-}" ]]; then
        echo "${CLANG_FORMAT}"
        return
    fi
    if command -v "clang-format-${REQUIRED_MAJOR}" >/dev/null 2>&1; then
        command -v "clang-format-${REQUIRED_MAJOR}"
        return
    fi
    for prefix in /opt/homebrew /usr/local; do
        if [[ -x "${prefix}/opt/llvm@${REQUIRED_MAJOR}/bin/clang-format" ]]; then
            echo "${prefix}/opt/llvm@${REQUIRED_MAJOR}/bin/clang-format"
            return
        fi
    done
    if command -v clang-format >/dev/null 2>&1; then
        command -v clang-format
        return
    fi
    echo ""
}

CLANG_FMT="$(resolve_clang_format)"
if [[ -z "${CLANG_FMT}" ]]; then
    echo "error: clang-format not found. Install clang-format ${REQUIRED_VERSION}." >&2
    exit 127
fi

VERSION_LINE="$("${CLANG_FMT}" --version 2>&1 | head -1)"
ACTUAL_VERSION=""
if [[ "${VERSION_LINE}" =~ version[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+) ]]; then
    ACTUAL_VERSION="${BASH_REMATCH[1]}"
fi
if [[ "${ACTUAL_VERSION}" != "${REQUIRED_VERSION}" ]]; then
    echo "error: unsupported clang-format version." >&2
    echo "       found: ${VERSION_LINE}" >&2
    echo "       expected: clang-format ${REQUIRED_VERSION}" >&2
    echo "       override with CLANG_FORMAT=/path/to/clang-format-${REQUIRED_MAJOR}" >&2
    exit 1
fi

files=()
while IFS= read -r -d '' file; do
    files+=("${ROOT_DIR}/${file}")
done < <(cd "${ROOT_DIR}" && find src tests \
    \( -name '*.cppm' -o -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) \
    -not -path '*/build/*' -not -path '*/install/*' -print0 2>/dev/null)

if [[ ${#files[@]} -eq 0 ]]; then
    echo "no C++ sources found under ${ROOT_DIR}/{src,tests}" >&2
    exit 0
fi

echo "[format-cpp] using ${CLANG_FMT}"
echo "[format-cpp] ${VERSION_LINE}"
echo "[format-cpp] mode=${mode}, files=${#files[@]}"

if [[ "${mode}" == "check" ]]; then
    "${CLANG_FMT}" --dry-run --Werror "${files[@]}"
else
    "${CLANG_FMT}" -i "${files[@]}"
fi
