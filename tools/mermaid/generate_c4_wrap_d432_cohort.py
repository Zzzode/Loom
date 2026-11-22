#!/usr/bin/env python3
"""Generate the c4 `config.wrap` cohort ledger (D-432).

Selection: a C4 original whose frontmatter is a folded description plus
`config:` > `wrap: true|false`. SVG-level text wrapping has no terminal
analogue (the projection already wraps), so the leaf is inert and disclosed
under `C4 native projection v1`; the oracle is exact art equality with the
config-stripped twin.

Usage: python3 tools/mermaid/generate_c4_wrap_d432_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/c4-wrap-d432-cohort.json")
SCHEMA = "loom_mermaid_c4_wrap_d432_cohort/v1"
HEADER = "C4 native projection v1"
COHORT_DIGEST = "74d11b9c09b249ad11837142b196873d54991f2a06b93553d58de584efebd453"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None
    end = text.find("\n---\n", 4)
    if end == -1:
        raise ValueError("unterminated frontmatter")
    return text[4:end]


def parse(frontmatter: str):
    section = None
    seen = set()
    wrap = None
    for line in frontmatter.split("\n"):
        if line == "---":
            continue
        if line.startswith("description:"):
            section = "description"
            continue
        if line == "config:":
            if "config" in seen:
                return None
            seen.add("config")
            section = "config"
            continue
        if section == "description":
            if not line.startswith("  ") or line.startswith("   ") or not line.strip():
                return None
            continue
        if section == "config":
            if not line.startswith("  wrap: ") or wrap is not None:
                return None
            value = line[len("  wrap: "):].strip()
            if value not in ("true", "false"):
                return None
            wrap = value
            continue
        return None
    return wrap


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for path in sorted((cases / "c4").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        frontmatter = split_frontmatter(path.read_text())
        if frontmatter is None:
            continue
        wrap = parse(frontmatter)
        if wrap is None:
            continue
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "wrap": wrap,
                "expected_footer_lines": [
                    HEADER, f"config.wrap={wrap} -> SVG text wrapping; not applied"],
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
        "decision": "D-432",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "c4 original whose frontmatter is a folded description plus "
                    "config: > wrap: true|false",
            "cohort_digest": digest,
            "exclusions": "theme/look keys, other config keys, unknown values, init "
                          "directives and Update* statements stay whole-source",
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
