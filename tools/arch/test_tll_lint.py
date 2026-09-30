#!/usr/bin/env python3
"""Negative test for the RFC 0002 F4 --tll-lint lint (graph_check.py).

No Python test runner is wired in this repo (tests/ is C++ gtest), so this
script is run manually:

    python3 tools/arch/test_tll_lint.py

It asserts the F4 gate contract from docs/rfcs/attachments/
0002-implementable-gate.md (TLL acyclicity + file->lib grouping):

  1. on the LIVE tree the lint passes: 0 TLL SCCs, 0 self-loops, and the
     grouping rule holds — all twelve cc_ui_<area> area libraries exist
     (RFC 0002 F4 has landed) and every cc.ui.<area>.* module is homed in
     exactly its own area library, with cc_ui a source-less INTERFACE
     aggregate;
  2. a target_link_libraries cycle (a -> b -> a) in a temp cmake tree
     fails with the SCC reported;
  3. a self-loop (a -> a) fails;
  4. a well-formed acyclic tree with a cc_ui_foundation area library
     homing a cc.ui.foundation.* module passes (positive control; also
     exercises the multi-line PUBLIC form and an inline comment);
  5. a cc_ui_<area> FILE_SET listing another area's module fails the
     grouping rule (target-side);
  6. a split-area module missing from its area library's FILE_SET (left in
     cc_ui) fails the grouping rule (file-side);
  7. the pre-split layout (single cc_ui target, no cc_ui_* libs) passes the
     grouping rule vacuously.

Zero third-party dependencies.
"""

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))

import graph_check as gc  # noqa: E402

FAILURES: list[str] = []


def check(cond: bool, label: str) -> None:
    print(("  ok  " if cond else "  FAIL") + " " + label)
    if not cond:
        FAILURES.append(label)


def write_tree(root: pathlib.Path, files: dict[str, str]) -> None:
    for rel, content in files.items():
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content, encoding="utf-8")


def tll_lint_on_temp_tree(files: dict[str, str]):
    """Run the TLL lint against a temp src/ tree (targets dir + ui modules
    are both read from the redirected SRC)."""
    with tempfile.TemporaryDirectory() as d:
        root = pathlib.Path(d)
        write_tree(root, files)
        old = gc.SRC
        gc.SRC = root / "src"
        try:
            units = gc.load_units()
            return gc.tll_lint_check(units)
        finally:
            gc.SRC = old


def main() -> int:
    print("1. live tree: --tll-lint passes (0 SCCs, 12 area libs, grouping clean)")
    r = gc.run(False, tll_lint=True)
    t = r["tll_lint"]
    check(t["passes"], "TLL lint passes on the live tree")
    check(t["sccs"] == [], f"0 TLL SCCs (got {t['sccs']})")
    check(t["self_loops"] == [], f"0 self-loops (got {t['self_loops']})")
    check(sorted(t["area_targets"]) == [
        "cc_ui_app", "cc_ui_chrome", "cc_ui_dialogs", "cc_ui_features",
        "cc_ui_foundation", "cc_ui_messages", "cc_ui_permissions",
        "cc_ui_prompt", "cc_ui_screens", "cc_ui_tools", "cc_ui_visual",
        "cc_ui_widgets"],
        f"12 cc_ui_<area> targets (got {sorted(t['area_targets'])})")
    check(t["grouping"] == [], f"no grouping violations (got {t['grouping']})")
    # cc_ui is the source-less INTERFACE aggregate: the 12 area libs + the
    # 12 cc_* deps + 3 ftxui components.
    check(set(t["links"]["cc_ui"]) == {
        "cc_ui_foundation", "cc_ui_visual", "cc_ui_tools", "cc_ui_chrome",
        "cc_ui_prompt", "cc_ui_widgets", "cc_ui_permissions",
        "cc_ui_messages", "cc_ui_features", "cc_ui_dialogs", "cc_ui_screens",
        "cc_ui_app",
        "cc_utils", "cc_types", "cc_query", "cc_commands", "cc_orchestration",
        "cc_vim", "cc_hooks", "cc_plugins", "cc_session", "cc_history",
        "cc_skills", "cc_services",
        "ftxui::screen", "ftxui::dom", "ftxui::component"},
          "cc_ui aggregate deps parsed (12 area + 12 cc_* + 3 ftxui)")
    proc = subprocess.run(
        [sys.executable, str(HERE / "graph_check.py"), "--tll-lint"],
        cwd=ROOT, capture_output=True, text=True)
    check(proc.returncode == 0, f"CLI exits 0 (got {proc.returncode})")
    check("RFC 0002 F4 TLL lint: PASS" in proc.stdout,
          "prints the PASS verdict line")

    print("2. temp tree: target_link_libraries cycle fails (a <-> b)")
    t = tll_lint_on_temp_tree({
        "src/cmake/targets/a.cmake":
            "add_library(a)\n"
            "target_link_libraries(a PUBLIC b)\n",
        "src/cmake/targets/b.cmake":
            "add_library(b)\n"
            "target_link_libraries(b PUBLIC a)\n",
    })
    check(not t["passes"], "gate fails")
    check(t["sccs"] == [["a", "b"]], f"SCC reported (got {t['sccs']})")
    check(t["self_loops"] == [], f"no self-loop (got {t['self_loops']})")

    print("3. temp tree: self-loop fails (a -> a)")
    t = tll_lint_on_temp_tree({
        "src/cmake/targets/a.cmake":
            "add_library(a)\n"
            "target_link_libraries(a PUBLIC a)\n",
    })
    check(not t["passes"], "gate fails")
    check(t["self_loops"] == ["a"],
          f"self-loop reported (got {t['self_loops']})")
    check(t["sccs"] == [], f"no non-trivial SCC (got {t['sccs']})")

    print("4. temp tree: well-formed split tree passes (positive control)")
    t = tll_lint_on_temp_tree({
        "src/cmake/targets/cc_utils.cmake":
            "add_library(cc_utils)\n",
        "src/cmake/targets/cc_ui_foundation.cmake":
            "add_library(cc_ui_foundation)\n"
            "target_sources(cc_ui_foundation\n"
            "    PUBLIC FILE_SET CXX_MODULES FILES\n"
            "        ui/foundation/f.cppm\n"
            ")\n"
            "target_link_libraries(cc_ui_foundation\n"
            "    PUBLIC\n"
            "        cc_utils   # leaf util lib\n"
            ")\n",
        "src/ui/foundation/f.cppm":
            "export module cc.ui.foundation.f;\n",
    })
    check(t["passes"], f"gate passes (violations: {t['grouping']})")
    check(t["area_targets"] == ["cc_ui_foundation"],
          f"area target discovered (got {t['area_targets']})")
    check(t["sccs"] == [], f"0 SCCs (got {t['sccs']})")
    check(t["grouping"] == [], f"no grouping violations "
                               f"(got {t['grouping']})")

    print("5. temp tree: wrong-area module in a cc_ui_<area> FILE_SET fails")
    t = tll_lint_on_temp_tree({
        "src/cmake/targets/cc_ui_foundation.cmake":
            "add_library(cc_ui_foundation)\n"
            "target_sources(cc_ui_foundation PUBLIC FILE_SET CXX_MODULES "
            "FILES\n"
            "    ui/chrome/c.cppm)\n",
        "src/ui/chrome/c.cppm":
            "export module cc.ui.chrome.c;\n",
    })
    check(not t["passes"], "gate fails")
    check(any("cc.ui.chrome" in v for v in t["grouping"]),
          f"target-side grouping violation (got {t['grouping']})")

    print("6. temp tree: split-area module left in cc_ui fails (file-side)")
    t = tll_lint_on_temp_tree({
        "src/cmake/targets/cc_ui_foundation.cmake":
            "add_library(cc_ui_foundation)\n"
            "target_sources(cc_ui_foundation PUBLIC FILE_SET CXX_MODULES "
            "FILES\n"
            "    ui/foundation/other.cppm)\n",
        "src/cmake/targets/cc_ui.cmake":
            "add_library(cc_ui)\n"
            "target_sources(cc_ui PUBLIC FILE_SET CXX_MODULES FILES\n"
            "    ui/foundation/f.cppm)\n",
        "src/ui/foundation/f.cppm":
            "export module cc.ui.foundation.f;\n",
        "src/ui/foundation/other.cppm":
            "export module cc.ui.foundation.other;\n",
    })
    check(not t["passes"], "gate fails")
    check(any("must be in cc_ui_foundation" in v for v in t["grouping"]),
          f"file-side grouping violation (got {t['grouping']})")

    print("7. temp tree: pre-split layout passes grouping vacuously")
    t = tll_lint_on_temp_tree({
        "src/cmake/targets/cc_ui.cmake":
            "add_library(cc_ui)\n"
            "target_sources(cc_ui PUBLIC FILE_SET CXX_MODULES FILES\n"
            "    ui/foundation/f.cppm\n"
            "    ui/chrome/c.cppm)\n",
        "src/ui/foundation/f.cppm":
            "export module cc.ui.foundation.f;\n",
        "src/ui/chrome/c.cppm":
            "export module cc.ui.chrome.c;\n",
    })
    check(t["passes"], f"gate passes (violations: {t['grouping']})")
    check(t["area_targets"] == [],
          f"no area targets (got {t['area_targets']})")

    if FAILURES:
        print(f"\n{len(FAILURES)} check(s) FAILED")
        return 1
    print("\nall checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
