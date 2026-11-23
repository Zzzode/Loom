#!/usr/bin/env python3
"""Generate the gitGraph frontmatter-title cohort ledger (D-435).

Selection: a gitGraph original whose frontmatter is a folded description plus a
single `title: <prose>` scalar and no config block. The title is honoured: the
native frame draws `title <text>` as its first row, so the oracle is exact —
the frame minus that row equals the title-free twin's frame.

Usage: python3 tools/mermaid/generate_git_title_d435_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/git-title-d435-cohort.json")
SCHEMA = "loom_mermaid_git_title_d435_cohort/v1"
COHORT_DIGEST = "e420ab586fabe66365e606124409f3551085c3eaa73a6e6c0cfb7a5fb3d76271"
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


def parse(frontmatter: str):
    title = None
    seen = set()
    for line in frontmatter.split("\n"):
        if not line.strip():
            continue
        if not line.startswith(" "):
            key = line.split(":", 1)[0]
            if key in seen:
                return None
            seen.add(key)
            if key == "title":
                title = line.split(":", 1)[1].strip()
                if not title or len(title) > 256:
                    return None
                if title[0] in "\"'" != title[-1] in "\"'":
                    return None
                if title[0] in "\"'" and title[-1] == title[0]:
                    title = title[1:-1]
                    if not title or title[0] in "\"'" or title[-1] in "\"'":
                        return None
                elif any(q in title for q in "\"'"):
                    return None
                if title[0] == " " or title[-1] == " " or "\t" in title:
                    return None
            continue
        if "description" not in seen:
            return None  # a payload line before the description
    if "description" not in seen or title is None:
        return None
    if "config" in seen:
        return None
    return title


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for path in sorted((cases / "git").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        text = path.read_text()
        split = split_frontmatter(text)
        if split is None:
            continue
        frontmatter, body = split
        title = parse(frontmatter)
        if title is None:
            continue
        if not body.lstrip().startswith(("gitGraph", "gitGraph BT:", "gitGraph TB:")):
            continue
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "title": title,
                "expected_title_row": f"title {title}",
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "drawn",
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
        "decision": "D-435",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "gitGraph original whose frontmatter is a folded description plus a "
                    "single title scalar and no config block",
            "cohort_digest": digest,
            "exclusions": "config leaves, init directives, a title after config, a "
                          "duplicate or empty title stay whole-source",
        },
        "review": {
            "reviewer_agent": "TL exemption 2026-11-22: family slice without a design "
                              "gate; the gate is the test document plus the title-row "
                              "oracle",
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
