#!/usr/bin/env python3
"""Generate the wardley closed-theme cohort ledger (D-437).

Selection: a wardley original whose frontmatter is a folded description plus a
closed `config` block with exactly one indent-2 `theme` leaf among
base/dark/forest/neutral. The palettes are never drawn by the terminal
projection, so the leaf is inert and disclosed under
`Wardley native projection v1`; the body is transcribed verbatim (column-0 and
two-space spellings both accepted).

Usage: python3 tools/mermaid/generate_wardley_theme_d437_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/wardley-theme-d437-cohort.json")
SCHEMA = "loom_mermaid_wardley_theme_d437_cohort/v1"
THEMES = ("base", "dark", "forest", "neutral")
COHORT_DIGEST = "4e208f1bdb4913f39ae55b89aaedf89250eeaec444b70e2bc7c7ecdcf0bd7f0b"
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
    section = None
    seen = set()
    theme = None
    for line in frontmatter.split("\n"):
        if not line.strip():
            continue
        if not line.startswith(" "):
            key = line.split(":", 1)[0]
            if key in seen:
                return None
            seen.add(key)
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
        stripped = line.strip()
        key, sep, value = stripped.partition(":")
        if key != "theme" or not sep or theme is not None:
            return None
        value = value.strip()
        if value not in THEMES:
            return None
        theme = value
    if "description" not in seen or theme is None:
        return None
    return theme


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for path in sorted((cases / "wardley").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        split = split_frontmatter(path.read_text())
        if split is None:
            continue
        theme = parse(split[0])
        if theme is None:
            continue
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "theme": theme,
                "expected_footer_lines": [
                    "Wardley native projection v1",
                    f"config.theme={theme} -> colour palette; not applied",
                ],
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
            }
        )
    if len(rows) != 4:
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
        "decision": "D-437",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "wardley original whose frontmatter is a folded description plus "
                    "config.theme in the closed base/dark/forest/neutral table",
            "cohort_digest": digest,
            "body_spellings": "column-0 and two-space statement indentation are both "
                              "transcribed verbatim",
            "exclusions": "top-level theme keys, other theme names, look keys, unknown "
                          "leaves and style statements stay whole-source",
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
