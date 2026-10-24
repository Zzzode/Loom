#!/usr/bin/env python3
"""Mermaid module-privacy gate (RFC 0031 E6a).

Run manually alongside the other architecture checks:

    python3 tools/arch/test_mermaid_privacy.py

The module system enforces only half of the E6a privacy contract: an
external translation unit that imports loom.ui.visual.mermaid cannot name
loom::ui::mermaid::detail (it lives in a non-exported implementation
partition), and partitions are not importable outside their module. It does
NOT enforce the other half: a translation unit that itself declares
`module loom.ui.visual.mermaid;` is a real module implementation unit and can
`import :detail;`, which is the one residual hole. This source-scan lint is
therefore the load-bearing gate for the property "no test translation unit is
attached to the production module".

It fails (exits nonzero) when:

  1. any file under tests/ contains a line declaring
        module loom.ui.visual.mermaid;
  2. any file under tests/ imports the production module directly
        import loom.ui.visual.mermaid;
     The test-only facade import
        import loom.ui.visual.mermaid.testing;
     is the allowed form and does not match (the module name is required to
     end at the semicolon).
  3. any file under tests/ spells the private namespace
        mermaid::detail
  4. src/ui/visual/mermaid_detail.cppm does not exist as a NON-exported
     implementation partition (`module loom.ui.visual.mermaid:detail;`,
     never `export module ...`);
  5. src/ui/visual/mermaid_testing.cppm does not re-export the primary
     interface via the wide form
        export import loom.ui.visual.mermaid;
     (clang 23 cannot transitively expose complete types through narrow
     exported using-declarations).

Zero third-party dependencies.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
TESTS_DIR = REPO_ROOT / "tests"
DETAIL_PARTITION = REPO_ROOT / "src/ui/visual/mermaid_detail.cppm"
TESTING_FACADE = REPO_ROOT / "src/ui/visual/mermaid_testing.cppm"

MODULE_DECL_RE = re.compile(r"^\s*module\s+loom\.ui\.visual\.mermaid\s*;")
DIRECT_IMPORT_RE = re.compile(r"^\s*import\s+loom\.ui\.visual\.mermaid\s*;")
DETAIL_REF_RE = re.compile(r"mermaid::detail")
PARTITION_DECL_RE = re.compile(r"^\s*module\s+loom\.ui\.visual\.mermaid:detail\s*;")
EXPORT_PARTITION_RE = re.compile(r"^\s*export\s+module\s+loom\.ui\.visual\.mermaid:detail")
EXPORT_IMPORT_RE = re.compile(r"^\s*export\s+import\s+loom\.ui\.visual\.mermaid\s*;")


def scan_test_tree() -> list[str]:
    failures: list[str] = []
    # tests/fuzz is exempt: a libFuzzer target is a MODULE IMPLEMENTATION UNIT
    # (it declares the production module and imports :detail to drive the
    # non-exported parse/layout entry points), not an isolated assertion TU.
    fuzz_dir = TESTS_DIR / "fuzz"
    for path in sorted(TESTS_DIR.rglob("*")):
        if not path.is_file():
            continue
        try:
            if fuzz_dir in path.parents:
                continue
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        rel = path.relative_to(REPO_ROOT)
        for lineno, line in enumerate(text.splitlines(), start=1):
            if MODULE_DECL_RE.match(line):
                failures.append(
                    f"{rel}:{lineno}: test TU declares the production module: "
                    f"{line.strip()} (import loom.ui.visual.mermaid.testing instead)")
            if DIRECT_IMPORT_RE.match(line):
                failures.append(
                    f"{rel}:{lineno}: test TU imports the production module directly: "
                    f"{line.strip()} (only the .testing facade is allowed)")
            if DETAIL_REF_RE.search(line):
                failures.append(
                    f"{rel}:{lineno}: test TU references the private detail namespace: "
                    f"{line.strip()}")
    return failures


def check_detail_partition() -> list[str]:
    failures: list[str] = []
    if not DETAIL_PARTITION.is_file():
        return [f"missing non-exported partition {DETAIL_PARTITION.relative_to(REPO_ROOT)}"]
    text = DETAIL_PARTITION.read_text(encoding="utf-8")
    if not any(PARTITION_DECL_RE.match(line) for line in text.splitlines()):
        failures.append(
            "mermaid_detail.cppm must declare the non-exported partition "
            "`module loom.ui.visual.mermaid:detail;`")
    for lineno, line in enumerate(text.splitlines(), start=1):
        if EXPORT_PARTITION_RE.match(line):
            failures.append(
                f"mermaid_detail.cppm:{lineno}: partition must not be exported: {line.strip()}")
    return failures


def check_testing_facade() -> list[str]:
    failures: list[str] = []
    if not TESTING_FACADE.is_file():
        return [f"missing test facade module {TESTING_FACADE.relative_to(REPO_ROOT)}"]
    text = TESTING_FACADE.read_text(encoding="utf-8")
    if not any(EXPORT_IMPORT_RE.match(line) for line in text.splitlines()):
        failures.append(
            "mermaid_testing.cppm must wide-re-export the primary via "
            "`export import loom.ui.visual.mermaid;`")
    return failures


def main() -> int:
    failures = scan_test_tree()
    failures += check_detail_partition()
    failures += check_testing_facade()
    if failures:
        print("test_mermaid_privacy: FAIL")
        for failure in failures:
            print(f"  {failure}")
        print(f"\n{len(failures)} privacy violation(s).")
        return 1
    print("test_mermaid_privacy: PASS (no test TU declares/imports the production "
          "module or names mermaid::detail; partition and facade are well-formed)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
