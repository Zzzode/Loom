#!/usr/bin/env python3
"""Generate the gitGraph main-branch cohort ledger (D-436).

Selection: a gitGraph original whose frontmatter config carries
`config.gitGraph.mainBranchName` and/or `mainBranchOrder` (alongside only the
four closed booleans). Both leaves are honoured: the trunk is renamed and its
lane order applied, and the drawn frame shows the effect.

Usage: python3 tools/mermaid/generate_git_mainbranch_d436_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/git-mainbranch-d436-cohort.json")
SCHEMA = "loom_mermaid_git_mainbranch_d436_cohort/v1"
COHORT_DIGEST = "115731960e15c33ab6e779b2f2874ceaa838861a6f29115640d303c2d2c0ecb1"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None
    end = text.find("\n---\n", 4)
    if end == -1:
        return None
    return text[4:end], text[end + 5 :]


BOOLEANS = ["showBranches", "showCommitLabel", "rotateCommitLabel", "parallelCommits"]
THEMES = ["neo", "neo-dark", "redux", "redux-dark", "redux-color", "redux-dark-color"]
NOTICE = {
    "showBranches": " -> branch reference labels",
    "showCommitLabel": " -> commit id labels",
    "rotateCommitLabel": " -> label rotation; not applied",
    "parallelCommits": " -> parallel lane compaction; not applied",
}


def parse(text: str):
    split = split_frontmatter(text)
    if split is None:
        return None
    frontmatter, body = split
    values = {}
    section = None
    seen = set()
    for line in frontmatter.split("\n"):
        if not line.strip():
            continue
        if not line.startswith(" "):
            key = line.split(":", 1)[0]
            if key in seen:
                return None
            seen.add(key)
            if key == "config":
                section = "config"
                continue
            if key == "description" and section != "config":
                section = "description"
                continue
            return None
        if section == "description":
            continue  # a description payload line
        stripped = line.strip()
        if section == "config":
            key, sep, value = stripped.partition(":")
            if key == "look" and sep:
                if value.strip() != "neo" or "look" in seen:
                    return None
                seen.add("look")
                values["look"] = value.strip()
                continue
            if key == "theme" and sep:
                theme = value.strip()
                if theme not in THEMES or "theme" in seen:
                    return None
                seen.add("theme")
                values["theme"] = theme
                continue
            if stripped != "gitGraph:" or "gitGraph" in seen:
                return None
            seen.add("gitGraph")
            section = "gitGraph"
            continue
        const_indent = len(line) - len(line.lstrip(" "))
        if section == "gitGraph" and const_indent == 2:
            # Back at config level: the closed flat look/theme tables.
            key, sep, value = stripped.partition(":")
            if key == "look" and sep:
                if value.strip() != "neo" or "look" in seen:
                    return None
                seen.add("look")
                values["look"] = value.strip()
                continue
            if key == "theme" and sep:
                theme = value.strip()
                if theme not in THEMES or "theme" in seen:
                    return None
                seen.add("theme")
                values["theme"] = theme
                continue
            return None
        if section != "gitGraph":
            return None
        key, sep, value = stripped.partition(":")
        if not sep or key in seen:
            return None
        seen.add(key)
        value = value.strip()
        if key in BOOLEANS:
            if value not in ("true", "false"):
                return None
            values[key] = value
        elif key == "mainBranchName":
            if len(value) >= 2 and value[0] in "'\"" and value[-1] == value[0]:
                value = value[1:-1]
            if not value or len(value) > 64 or " " in value or "\u2205" in value:
                return None
            values[key] = value
        elif key == "mainBranchOrder":
            if not value.isdigit() or len(value) > 3:
                return None
            values[key] = value
        else:
            return None
    if "mainBranchName" not in values and "mainBranchOrder" not in values:
        return None
    if not body.lstrip().startswith("gitGraph"):
        return None
    footer = ["GitGraph native projection v1"]
    if "look" in values:
        footer.append(f"config.look={values['look']} -> source node style; not applied")
    if "theme" in values:
        footer.append(f"config.theme={values['theme']} -> colour palette; not applied")
    for key in BOOLEANS:
        if key in values:
            footer.append(f"config.gitGraph.{key}={values[key]}{NOTICE[key]}")
    if "mainBranchName" in values:
        footer.append(
            f"config.gitGraph.mainBranchName={values['mainBranchName']}"
            " -> trunk branch label; honoured")
    if "mainBranchOrder" in values:
        footer.append(
            f"config.gitGraph.mainBranchOrder={values['mainBranchOrder']}"
            " -> trunk lane order; honoured")
    return values, footer


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for path in sorted((cases / "git").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        parsed = parse(path.read_text())
        if parsed is None:
            continue
        values, footer = parsed
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "config": values,
                "expected_footer_lines": footer,
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
            }
        )
    if len(rows) != 14:
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
        "decision": "D-436",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "gitGraph original whose frontmatter config carries "
                    "config.gitGraph.mainBranchName and/or mainBranchOrder, with only "
                    "the four closed booleans plus the closed look/theme tables",
            "cohort_digest": digest,
            "exclusions": "unknown leaves, init directives, a non-numeric order, a name "
                          "with a space or the ∅ placeholder, other look/theme values "
                          "stay whole-source",
        },
        "review": {
            "reviewer_agent": "TL exemption 2026-11-22: family slice without a design "
                              "gate; the gate is the test document plus the trunk "
                              "identity/order oracles",
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
