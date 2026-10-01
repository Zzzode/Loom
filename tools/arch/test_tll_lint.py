#!/usr/bin/env python3
"""Negative test for the RFC 0002 F4 --tll-lint lint (graph_check.py).

No Python test runner is wired in this repo (tests/ is C++ gtest), so this
script is run manually:

    python3 tools/arch/test_tll_lint.py

It asserts the F4 gate contract from docs/rfcs/attachments/
0002-implementable-gate.md (TLL acyclicity + file->lib grouping):

  1. on the LIVE tree the lint passes: 0 TLL SCCs, 0 self-loops, and the
     grouping rule holds — all twelve loom_ui_<area> area libraries exist
     (RFC 0002 F4 has landed) and every loom.ui.<area>.* module is homed in
     exactly its own area library, with loom_ui a source-less INTERFACE
     aggregate;
  2. a target_link_libraries cycle (a -> b -> a) in a temp cmake tree
     fails with the SCC reported;
  3. a self-loop (a -> a) fails;
  4. a well-formed acyclic tree with a loom_ui_foundation area library
     homing a loom.ui.foundation.* module passes (positive control; also
     exercises the multi-line PUBLIC form and an inline comment);
  5. a loom_ui_<area> FILE_SET listing another area's module fails the
     grouping rule (target-side);
  6. a split-area module missing from its area library's FILE_SET (left in
     loom_ui) fails the grouping rule (file-side);
  7. the pre-split layout (single loom_ui target, no loom_ui_* libs) passes the
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
        "loom_ui_app", "loom_ui_chrome", "loom_ui_dialogs", "loom_ui_features",
        "loom_ui_foundation", "loom_ui_messages", "loom_ui_permissions",
        "loom_ui_prompt", "loom_ui_screens", "loom_ui_tools", "loom_ui_visual",
        "loom_ui_widgets"],
        f"12 loom_ui_<area> targets (got {sorted(t['area_targets'])})")
    check(t["grouping"] == [], f"no grouping violations (got {t['grouping']})")
    # loom_ui is the source-less INTERFACE aggregate: the 12 area libs + the
    # 12 loom_* deps + 3 ftxui components.
    check(set(t["links"]["loom_ui"]) == {
        "loom_ui_foundation", "loom_ui_visual", "loom_ui_tools", "loom_ui_chrome",
        "loom_ui_prompt", "loom_ui_widgets", "loom_ui_permissions",
        "loom_ui_messages", "loom_ui_features", "loom_ui_dialogs", "loom_ui_screens",
        "loom_ui_app",
        "loom_utils", "loom_types", "loom_query", "loom_commands", "loom_orchestration",
        "loom_vim", "loom_hooks", "loom_plugins", "loom_session", "loom_history",
        "loom_skills", "loom_services",
        "ftxui::screen", "ftxui::dom", "ftxui::component"},
          "loom_ui aggregate deps parsed (12 area + 12 loom_* + 3 ftxui)")
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
        "src/cmake/targets/loom_utils.cmake":
            "add_library(loom_utils)\n",
        "src/cmake/targets/loom_ui_foundation.cmake":
            "add_library(loom_ui_foundation)\n"
            "target_sources(loom_ui_foundation\n"
            "    PUBLIC FILE_SET CXX_MODULES FILES\n"
            "        ui/foundation/f.cppm\n"
            ")\n"
            "target_link_libraries(loom_ui_foundation\n"
            "    PUBLIC\n"
            "        loom_utils   # leaf util lib\n"
            ")\n",
        "src/ui/foundation/f.cppm":
            "export module loom.ui.foundation.f;\n",
    })
    check(t["passes"], f"gate passes (violations: {t['grouping']})")
    check(t["area_targets"] == ["loom_ui_foundation"],
          f"area target discovered (got {t['area_targets']})")
    check(t["sccs"] == [], f"0 SCCs (got {t['sccs']})")
    check(t["grouping"] == [], f"no grouping violations "
                               f"(got {t['grouping']})")

    print("5. temp tree: wrong-area module in a loom_ui_<area> FILE_SET fails")
    t = tll_lint_on_temp_tree({
        "src/cmake/targets/loom_ui_foundation.cmake":
            "add_library(loom_ui_foundation)\n"
            "target_sources(loom_ui_foundation PUBLIC FILE_SET CXX_MODULES "
            "FILES\n"
            "    ui/chrome/c.cppm)\n",
        "src/ui/chrome/c.cppm":
            "export module loom.ui.chrome.c;\n",
    })
    check(not t["passes"], "gate fails")
    check(any("loom.ui.chrome" in v for v in t["grouping"]),
          f"target-side grouping violation (got {t['grouping']})")

    print("6. temp tree: split-area module left in loom_ui fails (file-side)")
    t = tll_lint_on_temp_tree({
        "src/cmake/targets/loom_ui_foundation.cmake":
            "add_library(loom_ui_foundation)\n"
            "target_sources(loom_ui_foundation PUBLIC FILE_SET CXX_MODULES "
            "FILES\n"
            "    ui/foundation/other.cppm)\n",
        "src/cmake/targets/loom_ui.cmake":
            "add_library(loom_ui)\n"
            "target_sources(loom_ui PUBLIC FILE_SET CXX_MODULES FILES\n"
            "    ui/foundation/f.cppm)\n",
        "src/ui/foundation/f.cppm":
            "export module loom.ui.foundation.f;\n",
        "src/ui/foundation/other.cppm":
            "export module loom.ui.foundation.other;\n",
    })
    check(not t["passes"], "gate fails")
    check(any("must be in loom_ui_foundation" in v for v in t["grouping"]),
          f"file-side grouping violation (got {t['grouping']})")

    print("7. temp tree: pre-split layout passes grouping vacuously")
    t = tll_lint_on_temp_tree({
        "src/cmake/targets/loom_ui.cmake":
            "add_library(loom_ui)\n"
            "target_sources(loom_ui PUBLIC FILE_SET CXX_MODULES FILES\n"
            "    ui/foundation/f.cppm\n"
            "    ui/chrome/c.cppm)\n",
        "src/ui/foundation/f.cppm":
            "export module loom.ui.foundation.f;\n",
        "src/ui/chrome/c.cppm":
            "export module loom.ui.chrome.c;\n",
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
