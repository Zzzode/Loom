#!/usr/bin/env python3
"""Negative test for the RFC 0002 F3 --store-lint lint (graph_check.py).

No Python test runner is wired in this repo (tests/ is C++ gtest), so this
script is run manually:

    python3 tools/arch/test_store_lint.py

It asserts the F3 gate contract from docs/rfcs/attachments/
0002-implementable-gate.md (store-naming/import lint):

  1. on the LIVE tree the lint passes with the F3 stores landed so far
     (MessagesStore, PromptStore, ... — the list grows as stores land);
  2. a store importing cc.ui.app.* fails the out-of-store rule (a store
     must not import the composition root or any area ranked >= screens);
  3. a features-area module importing a store fails the into-store rule
     (only app-area / screens-area modules may import a store);
  4. a store containing a mutex/jthread/condition_variable token fails the
     threading rule (stores are UI-thread-affined plain data);
  5. a store file whose declared module name is not
     cc.ui.screens.<name>_store fails the naming rule;
  6. a well-formed store (below-screens imports only, imported only by the
     app composition root and a screens-area shim) passes — positive
     control isolating the rules above.

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


def store_lint_on_temp_tree(files: dict[str, str]):
    """Run the store lint against a temp src/ tree (the real baseline is
    unused — the store lint has no baseline)."""
    with tempfile.TemporaryDirectory() as d:
        root = pathlib.Path(d)
        write_tree(root, files)
        old = gc.SRC
        gc.SRC = root / "src"
        try:
            units = gc.load_units()
            deps = gc.module_deps(units)
            return gc.store_lint_check(units, deps)
        finally:
            gc.SRC = old


def main() -> int:
    print("1. live tree: --store-lint passes (F3 stores landed)")
    r = gc.run(False, store_lint=True)
    s = r["store_lint"]
    check(s["passes"], "store lint passes on the live tree")
    # Grows as F3 stores land (MessagesStore, PromptStore, ...). The lint
    # itself has no baseline; this list is the live F3 store set.
    check(s["stores"] == ["cc.ui.screens.messages_store",
                          "cc.ui.screens.permission_store",
                          "cc.ui.screens.prompt_store",
                          "cc.ui.screens.task_view_store"],
          f"F3 stores found (got {s['stores']})")
    proc = subprocess.run(
        [sys.executable, str(HERE / "graph_check.py"), "--store-lint"],
        cwd=ROOT, capture_output=True, text=True)
    check(proc.returncode == 0,
          f"CLI exits 0 (got {proc.returncode})")
    check("RFC 0002 F3 store placement lint: PASS" in proc.stdout,
          "prints the PASS verdict line")

    print("2. temp tree: store importing cc.ui.app.* fails out-of-store")
    s = store_lint_on_temp_tree({
        "src/ui/screens/messages_store.cppm":
            "export module cc.ui.screens.messages_store;\n"
            "import cc.ui.app.app;\n",
        "src/ui/app/app.cppm":
            "export module cc.ui.app.app;\n",
    })
    check(not s["passes"], "gate fails")
    check(s["out_of_store"] == [["cc.ui.screens.messages_store",
                                 "cc.ui.app.app", "cc.ui.app"]],
          f"out-of-store violation (got {s['out_of_store']})")
    check(s["into_store"] == [], f"no into-store violation "
                                 f"(got {s['into_store']})")
    check(s["threading"] == [], f"no threading violation "
                                f"(got {s['threading']})")

    print("3. temp tree: features-area module importing a store fails "
          "into-store")
    s = store_lint_on_temp_tree({
        "src/ui/screens/messages_store.cppm":
            "export module cc.ui.screens.messages_store;\n",
        "src/ui/features/agents/agent_cards.cppm":
            "export module cc.ui.features.agents.agent_cards;\n"
            "import cc.ui.screens.messages_store;\n",
    })
    check(not s["passes"], "gate fails")
    check(s["into_store"] == [["cc.ui.features.agents.agent_cards",
                               "cc.ui.screens.messages_store"]],
          f"into-store violation (got {s['into_store']})")
    check(s["out_of_store"] == [], f"no out-of-store violation "
                                   f"(got {s['out_of_store']})")

    print("4. temp tree: store with a mutex token fails threading")
    s = store_lint_on_temp_tree({
        "src/ui/screens/prompt_store.cppm":
            "export module cc.ui.screens.prompt_store;\n"
            "import std;\n"
            "struct PromptStore {\n"
            "    std::mutex pending_at_mention_mutex;\n"
            "};\n",
    })
    check(not s["passes"], "gate fails")
    check(s["threading"] == [["cc.ui.screens.prompt_store", "mutex"]],
          f"threading violation (got {s['threading']})")
    check(s["naming"] == [], f"no naming violation (got {s['naming']})")

    print("5. temp tree: wrong declared module name fails naming")
    s = store_lint_on_temp_tree({
        "src/ui/screens/dialog_store.cppm":
            "export module cc.ui.screens.dialog;\n",
    })
    check(not s["passes"], "gate fails")
    check(len(s["naming"]) == 1 and "expected cc.ui.screens.dialog_store"
          in s["naming"][0],
          f"naming violation (got {s['naming']})")

    print("6. temp tree: well-formed store passes (positive control)")
    s = store_lint_on_temp_tree({
        "src/ui/screens/messages_store.cppm":
            "export module cc.ui.screens.messages_store;\n"
            "import cc.ui.foundation.ui_types;\n",
        "src/ui/foundation/ui_types.cppm":
            "export module cc.ui.foundation.ui_types;\n",
        # app composition root importing the store — allowed.
        "src/ui/app/app.cppm":
            "export module cc.ui.app.app;\n"
            "import cc.ui.screens.messages_store;\n",
        # screens-area re-export shim importing the store — allowed
        # (same-area; the F3 shim pattern).
        "src/ui/screens/repl_state.cppm":
            "export module cc.ui.screens.repl_state;\n"
            "import cc.ui.screens.messages_store;\n",
    })
    check(s["passes"], f"gate passes (violations: {s['violations']})")
    check(s["stores"] == ["cc.ui.screens.messages_store"],
          f"store discovered (got {s['stores']})")
    check(s["out_of_store"] == [], f"no out-of-store violation "
                                   f"(got {s['out_of_store']})")
    check(s["into_store"] == [], f"no into-store violation "
                                 f"(got {s['into_store']})")

    if FAILURES:
        print(f"\n{len(FAILURES)} check(s) FAILED")
        return 1
    print("\nall checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
