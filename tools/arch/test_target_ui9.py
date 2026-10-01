#!/usr/bin/env python3
"""Negative test for the RFC 0002 F0 --target-ui9 lint (graph_check.py).

No Python test runner is wired in this repo (tests/ is C++ gtest), so this
script is run manually:

    python3 tools/arch/test_target_ui9.py

It asserts the F0 gate contract from docs/rfcs/attachments/
0002-implementable-gate.md:

  1. on the LIVE tree, `graph_check.py --target-ui9` exits 1 and prints the
     2 SCCs (7-area + chrome<->foundation) — the gate fails today and passes
     after the F1/F2 cuts land (this assertion is the F0-state snapshot;
     update it when F2 dissolves the last SCC);
  2. a synthetic 20th SCC-internal direction in a temp tree fails check (a)
     (the SCC-shape ratchet) — a tools<->visual SCC: equal rank 1, so the
     new directions are NOT back edges and check (b) stays silent, isolating
     check (a);
  3. a synthetic rank-upward edge from a leaf (visual -> foundation) in a
     temp tree fails check (b) (order conformance) — visual stays a
     singleton, so check (a) stays silent, isolating check (b);
  4. the default gate (no flag) stays green on the live tree.

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


def ui9_on_temp_tree(files: dict[str, str]):
    """Run the ui9 check against a temp src/ tree (baseline stays the real
    one — only SRC is redirected)."""
    with tempfile.TemporaryDirectory() as d:
        root = pathlib.Path(d)
        write_tree(root, files)
        old = gc.SRC
        gc.SRC = root / "src"
        try:
            return gc.run(False, target_ui9=True)["ui9"]
        finally:
            gc.SRC = old


def main() -> int:
    print("1. live tree: --target-ui9 exits 1 and prints the 2 SCCs")
    proc = subprocess.run(
        [sys.executable, str(HERE / "graph_check.py"), "--target-ui9"],
        cwd=ROOT, capture_output=True, text=True)
    check(proc.returncode == 1, f"exit code 1 (got {proc.returncode})")
    out = proc.stdout
    check("TARGET UI9 SCC: loom.ui.chrome, loom.ui.foundation" in out,
          "prints the 2-area chrome<->foundation SCC")
    check("TARGET UI9 SCC: loom.ui.dialogs, loom.ui.features, loom.ui.messages, "
          "loom.ui.permissions, loom.ui.prompt, loom.ui.screens, loom.ui.widgets"
          in out, "prints the 7-area SCC")
    check("RFC 0002 F0 target (12 singleton loom.ui areas): FAIL" in out,
          "prints the FAIL verdict line")

    print("2. temp tree: tools<->visual SCC fails check (a) only")
    u = ui9_on_temp_tree({
        "src/ui/tools/t.cppm":
            "export module loom.ui.tools.t;\nimport loom.ui.visual.v;\n",
        "src/ui/visual/v.cppm":
            "export module loom.ui.visual.v;\nimport loom.ui.tools.t;\n",
    })
    check(not u["passes"], "gate fails")
    check(len(u["new_sloom_internal"]) == 2,
          f"2 new SCC-internal directions (got {u['new_sloom_internal']})")
    check(u["new_back"] == [],
          f"no new back direction (got {u['new_back']})")

    print("3. temp tree: visual -> foundation fails check (b) only")
    u = ui9_on_temp_tree({
        "src/ui/visual/v.cppm":
            "export module loom.ui.visual.v;\nimport loom.ui.foundation.f;\n",
        "src/ui/foundation/f.cppm":
            "export module loom.ui.foundation.f;\n",
    })
    check(not u["passes"], "gate fails")
    check(u["new_back"] == [("loom.ui.visual", "loom.ui.foundation")],
          f"new back direction visual -> foundation (got {u['new_back']})")
    check(u["new_sloom_internal"] == [],
          f"no new SCC-internal direction (got {u['new_sloom_internal']})")
    check(u["area_sccs"] == [], "no SCC forms (visual stays a singleton)")

    print("4. live tree: default gate (no flag) stays green")
    r = gc.run(False)
    check(r["passes"], "default gate passes")
    check(not r["module_cycles"], "0 module cycles")
    check(not r["new_upward_edges"], "0 new upward edges")
    check(not r["new_dead_imports"], "0 new dead imports")

    if FAILURES:
        print(f"\n{len(FAILURES)} check(s) FAILED")
        return 1
    print("\nall checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
