#!/usr/bin/env python3
"""Generate the tree-view closed-namespace cohort ledger (D-433).

Selection: a treeView original whose frontmatter is a folded description plus a
closed `config` namespace:

    config.treeView.{showIcons, defaultIconPack, rowIndent, lineThickness}
    config.themeVariables.treeView.{labelFontSize, labelColor, lineColor}

The native projection transcribes node rows verbatim, draws no icons, colour,
line weight or pixel indentation, so every leaf is inert and disclosed under
`TreeView native projection v1`. The oracle is exact: the art above the footer
equals the config-stripped twin's art.

The closing fence is matched strictly (`\\n---\\n`) because that is the shape the
shared classify probe can dispatch; a `---      ` close (the custom-config row)
never reaches any family renderer and stays whole-source.

Usage: python3 tools/mermaid/generate_treeview_ns_d433_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/treeview-ns-d433-cohort.json")
SCHEMA = "loom_mermaid_treeview_ns_d433_cohort/v1"
HEADER = "TreeView native projection v1"
WORD = re.compile(r"^[a-z0-9_-]{1,64}$")
KEY_WORD = re.compile(r"^[A-Za-z0-9_-]{1,64}$")
UINT = re.compile(r"^[1-9][0-9]{0,4}$")
PX = re.compile(r"^[1-9][0-9]{0,2}px$")
HEX = re.compile(r"^#[0-9a-fA-F]{6}$")
COHORT_DIGEST = "91c436b5eddef34232eddf7d5190f139ab5c101cd3d5c77a7e15156e6ead6688"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"

# Canonical disclosure order, independent of source order.
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


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def unquote(value: str):
    if len(value) >= 2 and value[0] in "'\"" and value[-1] == value[0]:
        inner = value[1:-1]
        if value[0] in inner:
            return None
        return inner
    return value


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None
    end = text.find("\n---\n", 4)
    if end == -1:
        return None
    return text[4:end]


def valid_leaf(parent: str, key: str, value: str) -> bool:
    if parent == "config.treeView":
        if key == "showIcons":
            return value in ("true", "false")
        if key == "defaultIconPack":
            return WORD.match(value) is not None
        if key in ("rowIndent", "lineThickness"):
            return UINT.match(value) is not None
    elif parent == "config.themeVariables.treeView":
        if key == "labelFontSize":
            return value is not None and PX.match(value) is not None
        if key in ("labelColor", "lineColor"):
            return value is not None and HEX.match(value) is not None
    return False


def parse(frontmatter: str):
    stack = []
    seen_maps = set()
    leaves = {}
    seen_key = False
    for line in frontmatter.split("\n"):
        if not line.strip():
            continue
        indent = len(line) - len(line.lstrip(" "))
        content = line[indent:]
        if indent == 0:
            if content.startswith("description:"):
                if seen_key or "config" in seen_maps or "description" in seen_maps:
                    return None
                seen_maps.add("description")
            elif content == "config:":
                if "config" in seen_maps:
                    return None
                seen_maps.add("config")
                seen_key = True
                stack = [(0, "config")]
            else:
                return None
            continue
        if not seen_key:
            continue
        while stack and stack[-1][0] >= indent:
            stack.pop()
        if not stack:
            return None
        parent = stack[-1][1]
        key, sep, value = content.partition(":")
        if not sep or not KEY_WORD.match(key):
            return None
        value = value.strip()
        if value:
            if (
                (parent, key) not in LEAF_ORDER
                or (parent, key) in leaves
            ):
                return None
            unquoted = unquote(value)
            if key != "showIcons":
                value = unquoted
            if not valid_leaf(parent, key, value):
                return None
            leaves[(parent, key)] = value
            continue
        if parent == "config" and key in ("treeView", "themeVariables"):
            if key in seen_maps:
                return None
            seen_maps.add(key)
            stack.append((indent, f"config.{key}"))
        elif parent == "config.themeVariables" and key == "treeView":
            if "themeTreeView" in seen_maps:
                return None
            seen_maps.add("themeTreeView")
            stack.append((indent, "config.themeVariables.treeView"))
        else:
            return None
    if not seen_key or not leaves:
        return None
    return leaves


def footer_lines(leaves):
    lines = [HEADER]
    for key in LEAF_ORDER:
        if key in leaves:
            parent, leaf = key
            lines.append(f"{parent}.{leaf}={leaves[key]}{NOTICE[leaf]}")
    return lines


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for path in sorted((cases / "tree-view").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        text = path.read_text()
        frontmatter = split_frontmatter(text)
        if frontmatter is None:
            continue
        # The node grammar rejects an empty `icon()` argument (validate_node), so
        # such a body stays whole-source even with an admitted frontmatter.
        if "icon()" in text:
            continue
        leaves = parse(frontmatter)
        if leaves is None:
            continue
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "leaves": {f"{p}.{k}": v for (p, k), v in
                           ((leaf, leaves[leaf]) for leaf in LEAF_ORDER if leaf in leaves)},
                "expected_footer_lines": footer_lines(leaves),
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
            }
        )
    if len(rows) != 2:
        raise ValueError(f"cohort drifted: {len(rows)} rows")
    rows.sort(key=lambda row: row["path"])
    digest = sha256_bytes("".join(f"{r['path']}\t{r['sha256']}\n" for r in rows).encode())
    if COHORT_DIGEST != "PLACEHOLDER" and digest != COHORT_DIGEST:
        raise ValueError(f"cohort drifted: {digest}")
    manifest_sha = sha256_bytes(manifest_bytes)
    if MANIFEST_SHA256 != "PLACEHOLDER" and manifest_sha != MANIFEST_SHA256:
        raise ValueError(f"manifest changed: {manifest_sha}")
    return {
        "schema": SCHEMA,
        "decision": "D-433",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "treeView original whose frontmatter is a folded description plus "
                    "the closed config.treeView / config.themeVariables.treeView leaves",
            "cohort_digest": digest,
            "exclusions": "look/theme keys, nested config maps (filenameIcons, "
                          "extensionIcons), unknown leaves and style statements stay "
                          "whole-source; a `---      ` closing fence never reaches a "
                          "family renderer through the shared classify probe",
        },
        "review": {
            "reviewer_agent": "TL exemption 2026-11-22: family slice without a design "
                              "gate; the gate is the test document plus the twin oracle",
            "verdict": "approved",
        },
        "counts": {"total": len(rows)},
        "rows": rows,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = pathlib.Path(args.repo_root)
    ledger = build(root)
    rendered = json.dumps(ledger, indent=2, ensure_ascii=False) + "\n"
    target = root / LEDGER
    if args.check:
        if target.read_text() != rendered:
            print("ledger is out of date", file=sys.stderr)
            return 1
        print("ledger current:", json.dumps(ledger["counts"]))
        return 0
    target.write_text(rendered)
    print(json.dumps(ledger["counts"], indent=2))
    print("cohort digest", ledger["selection"]["cohort_digest"])
    print("manifest sha256", ledger["upstream"]["manifest_sha256"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
