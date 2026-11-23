#!/usr/bin/env python3
"""Generate the flowchart closed theme cohort ledger (D-440).

Selection: the one non-contract flowchart-header original (`conf-and-directives`)
whose frontmatter is a description plus a lone indent-2 `theme: forest` leaf.
Every `flowchart/` fixture belongs to the frozen Flow400 contract (400 sources,
8 complete models), so its pending rows cannot be drawn without a Flow400
re-baseline; `forest` is therefore the only corpus-backed palette value. The terminal draws no palette, so the leaf is inert and
disclosed under the flow notice footer; the body is drawn by the native flow
projection exactly as the config-free twin.

Usage: python3 tools/mermaid/generate_flowchart_theme_d440_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/flowchart-theme-d440-cohort.json")
SCHEMA = "loom_mermaid_flowchart_theme_d440_cohort/v1"
COHORT_DIGEST = "fee3e64bd8ff500fb1b6183e002228b7406c5d328ecb8dbf35d2c7a28181363d"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"
THEMES = ("forest",)
PATHS = ("conf-and-directives/pr-check/settings-from-initialize-nodes-should-be-green.mmd",)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None
    end = text.find("\n---\n", 4)
    if end == -1:
        return None
    return text[4:end], text[end + 5 :]


def theme_of(frontmatter: str):
    """Mirror of the C++ lone-theme profile: description + exactly one leaf."""
    section = None
    leaves = []
    for line in frontmatter.split("\n"):
        if not line.strip():
            continue
        if not line.startswith(" "):
            key = line.split(":", 1)[0]
            if key == "description":
                section = "description"
                continue
            if key == "config":
                section = "config"
                continue
            return None
        if section == "description":
            continue
        if section != "config":
            return None
        indent = len(line) - len(line.lstrip(" "))
        if indent != 2:
            return None
        key, sep, value = line.strip().partition(":")
        if not sep:
            return None
        leaves.append((key, value.strip()))
    if len(leaves) != 1 or leaves[0][0] != "theme":
        return None
    value = leaves[0][1]
    if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
        value = value[1:-1]
    if value not in THEMES:
        return None
    return value


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for rel in PATHS:
        path = cases / rel
        split = split_frontmatter(path.read_text())
        if split is None:
            raise ValueError(f"missing frontmatter: {rel}")
        theme = theme_of(split[0])
        if theme is None:
            raise ValueError(f"shape drifted: {rel}")
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "theme": theme,
                "expected_footer_lines": [
                    f"config.theme={theme} -> colour palette; not applied"
                ],
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
            }
        )
    if len(rows) != 1:
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
        "decision": "D-440",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "flowchart original whose frontmatter is a description plus a lone "
                    "indent-2 theme leaf from the closed corpus-backed palette table",
            "palettes": list(THEMES),
            "cohort_digest": digest,
            "exclusions": "any sibling config leaf, quoted single-quote forms, unknown "
                          "palettes and namespace blocks keep the whole source",
        },
        "review": {
            "reviewer_agent": "TL exemption 2026-11-23: family slice without a design "
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
