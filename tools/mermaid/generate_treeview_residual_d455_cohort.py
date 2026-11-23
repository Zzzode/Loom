#!/usr/bin/env python3
"""Generate the D-455 tree-view residual cohort ledger.

Renderer-free. The two rows are the last tree-view fallbacks:

1. the custom-config row whose frontmatter closes with `---      ` (trailing
   spaces) and whose closed `config.treeView` / `config.themeVariables.treeView`
   leaves (rowIndent, lineThickness, labelFontSize, labelColor, lineColor) are
   inert and disclosed; and
2. the icon-suppression row, whose `icon(none)` / empty `icon()` node suffixes
   mean "no icon" upstream and render verbatim here (the projection never draws
   an icon).

D-455 admits both: the shared classify probe now accepts a closing fence with
trailing spaces/tabs (the owning renderer still validates the envelope it
accepts), and the node validator accepts the empty `icon()` call.

Twin rule (pinned): the twin is the same fixture with the frontmatter removed.
The art above the footer must be cell-equal.

Usage: python3 tools/mermaid/generate_treeview_residual_d455_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/treeview-residual-d455-cohort.json")
SCHEMA = "loom_mermaid_treeview_residual_d455_cohort/v1"
DECISION = "D-455"

HEADER = "TreeView native projection v1"
NOTICE_LIMIT = 96

# Canonical disclosure order, independent of source order (same as D-433/434).
LEAF_ORDER = [
    ("config.treeView", "showIcons"),
    ("config.treeView", "defaultIconPack"),
    ("config.treeView", "rowIndent"),
    ("config.treeView", "lineThickness"),
    ("config.themeVariables.treeView", "labelFontSize"),
    ("config.themeVariables.treeView", "labelColor"),
    ("config.themeVariables.treeView", "lineColor"),
]
NOTICE = {
    "showIcons": " -> icons are not drawn; not applied",
    "defaultIconPack": " -> icon pack resolution; not applied",
    "rowIndent": " -> row indentation; not applied",
    "lineThickness": " -> branch line weight; not applied",
    "labelFontSize": " -> label font size; not applied",
    "labelColor": " -> label colour; not applied",
    "lineColor": " -> branch line colour; not applied",
}

COHORT = [
    "tree-view/pr-check/should-render-with-custom-config-and-quoted-labels.mmd",
    "tree-view/nightly-check/should-hide-default-icons-with-icon-none-and-icon.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review (fresh general-purpose "
        "agent, D-455 tree-view residual admission, 2026-11-24; round 1 "
        "REQUEST-CHANGES with a shared-classify fence blocker plus five "
        "findings, round 2 APPROVE after the header-scoped tolerance, the "
        "hardened mirror and the token fix)"),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": (
        "The fence tolerance is scoped to a `treeView-beta` body so packet/"
        "cynefin/radar keep their pre-D-455 verdicts; the empty `icon()` form "
        "is the only node-shape change; the twin and partition arithmetic are "
        "pinned by the per-row gate."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def split_frontmatter(text: str):
    """(frontmatter lines, body) for the corpus envelope that this family owns.

    The closing fence may carry trailing spaces/tabs (one corpus row does).
    """
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        line = lines[close]
        if line.startswith("---") and line[3:].strip(" \t") == "":
            return lines[1:close], "\n".join(lines[close + 1:])
    return None, text


UINT = re.compile(r"^[1-9][0-9]{0,4}$")
WORD = re.compile(r"^[a-z0-9_-]{1,64}$")
PX = re.compile(r"^[1-9][0-9]{0,2}px$")
HEX = re.compile(r"^#[0-9a-fA-F]{6}$")


def leaf_value_ok(parent: str, key: str, value: str) -> bool:
    """The renderer's value shapes (D-433/434 closed tables)."""
    if parent == "config.treeView":
        if key == "showIcons":
            return value in ("true", "false")
        if key == "defaultIconPack":
            return bool(WORD.match(value))
        if key in ("rowIndent", "lineThickness"):
            return bool(UINT.match(value))
        return False
    if key == "labelFontSize":
        return bool(PX.match(value))
    if key in ("labelColor", "lineColor"):
        return bool(HEX.match(value))
    return False


def leaves_of(text: str):
    """The admitted namespace leaves in canonical order, or None when gated.

    Mirrors the renderer's closed `config` walk for the shapes this cohort
    pins: `description`/`config` open blocks, `config.treeView` /
    `config.themeVariables` open namespaces (each at most once), only the leaf
    table below is accepted with its value shape, and a comment line is a
    gate. `description` payload lines (no colon) are pass-through. The live
    C++ draw test remains the authority.
    """
    frontmatter, _ = split_frontmatter(text)
    if frontmatter is None:
        return None
    found = {}
    stack = []
    seen_blocks = set()
    for raw in frontmatter:
        line = raw.rstrip("\r")
        indent = len(line) - len(line.lstrip(" "))
        content = line[indent:]
        if not content:
            continue
        while stack and stack[-1][0] >= indent:
            stack.pop()
        parent = stack[-1][1] if stack else ""
        if parent == "description":
            if content.startswith("#"):
                return None
            continue  # folded payload line
        if content.startswith("#"):
            return None
        if ":" not in content:
            return None
        key, _, value = content.partition(":")
        value = value.strip(" ")
        if not value or (parent == "" and key == "description"):
            if value and parent == "" and key == "description":
                pass  # folded scalar opener
            if parent == "" and key in ("description", "config"):
                path = key
            elif parent == "config" and key in ("treeView", "themeVariables"):
                path = f"config.{key}"
            elif parent == "config.themeVariables" and key == "treeView":
                path = "config.themeVariables.treeView"
            else:
                return None
            if path in seen_blocks:
                return None
            seen_blocks.add(path)
            stack.append((indent, path))
            continue
        if parent not in ("config.treeView", "config.themeVariables.treeView"):
            return None
        unquoted = value
        if len(unquoted) >= 2 and unquoted[0] == unquoted[-1] and unquoted[0] in "'\"":
            unquoted = unquoted[1:-1]
        leaf = (parent, key)
        if leaf not in LEAF_ORDER or leaf in found:
            return None
        if not leaf_value_ok(parent, key, unquoted):
            return None
        found[leaf] = unquoted
    ordered = []
    for parent, key in LEAF_ORDER:
        if (parent, key) in found:
            ordered.append(f"{parent}.{key}={found[(parent, key)]}{NOTICE[key]}")
    if not ordered:
        return None
    if any(len(line) > NOTICE_LIMIT for line in ordered):
        return None
    return ordered


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                      for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    for rel in COHORT:
        source = (repo_root / CASES / rel).read_text()
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        if rel.endswith("quoted-labels.mmd"):
            notices = leaves_of(source)
            if not notices:
                raise ValueError(f"{rel} does not carry the admitted namespace")
        else:
            # The icon-suppression row's only leaf is the showIcons boolean.
            notices = leaves_of(source)
            if notices != ["config.treeView.showIcons=true -> icons are not drawn; not applied"]:
                raise ValueError(f"{rel} does not carry the admitted icon leaves")
        rows.append({
            "path": rel,
            "family": "tree-view",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": "source_fallback",
            "prediction": "predicted-draw",
            "track_scope": "whole-only",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn" if rel in NARROW_DRAW else "source",
                "restored_240": "drawn",
            },
            "expected_footer_lines": [HEADER] + notices,
            "twin": {
                "kind": "frontmatter-free-body",
                "path": rel,
                "sha256": observed_sha,
                "footer_lines": [],
            },
            "ledger_note": (
                "the tree-view projection transcribes node rows verbatim and "
                "never draws icons, colour, line weight or pixel indentation, so "
                "the closed namespace leaves and the icon-suppression suffixes "
                "are inert; the footer discloses each accepted leaf"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": ("a `---` envelope whose closing fence may carry trailing "
                      "spaces/tabs, the closed `config.treeView` / "
                      "`config.themeVariables.treeView` leaves, and node "
                      "`icon(none)` / empty `icon()` suffixes"),
            "footer": {"header": HEADER, "line_limit_bytes": NOTICE_LIMIT},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin is the same fixture with the frontmatter "
                          "removed; the art above the footer must be cell-equal"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("the last tree-view fallbacks: the trailing-space closing "
                     "fence with the closed namespace, and the empty `icon()` "
                     "suppression form"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows), "by_family": {"tree-view": len(rows)}},
        "review": REVIEW,
        "rows": rows,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo-root", default=".")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    repo_root = pathlib.Path(args.repo_root)
    ledger = build(repo_root)
    target = repo_root / LEDGER
    rendered = json.dumps(ledger, indent=2, ensure_ascii=False) + "\n"
    if args.check:
        if target.read_text() != rendered:
            print("ledger is out of date", file=sys.stderr)
            return 1
        print("ledger current:", json.dumps(ledger["counts"]))
        return 0
    target.write_text(rendered)
    print(json.dumps(ledger["counts"], indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
