#!/usr/bin/env python3
"""Generate the pie accessibility-no-op cohort ledger (D-426).

Renderer-free. Selection (semantic, source-state markers only): a pie original
with a reviewed folded description, a `config:` block carrying exactly one
`theme:` leaf from the closed table, and body-level scalar `accTitle:` /
`accDescr:` lines.

`accTitle`/`accDescr` are accessibility metadata: Mermaid renders them only into
the accessibility DOM, and a text terminal has no separate accessibility tree,
so they are validated no-ops. The criterion is a semantic source marker (the
declared accessibility text), and the oracle is exact: the drawn frame must
equal the same source with the two acc lines deleted, byte for byte.

Usage: python3 tools/mermaid/generate_pie_acc_d426_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/pie-acc-d426-cohort.json")
SCHEMA = "loom_mermaid_pie_acc_d426_cohort/v1"
EXPECTED_TOTAL = 4
HEADER = "Pie native projection v1"
THEME_TABLE = ("dark", "redux", "redux-dark", "redux-color", "redux-dark-color", "default",
               "forest", "neutral")

COHORT_DIGEST = "d0618087dabc24d2d7086b925076cb0cc732580ade49f5276fa25e591926b9c3"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def cohort_digest(rows) -> str:
    payload = "".join(f"{row['path']}\t{row['sha256']}\n" for row in rows)
    return sha256_bytes(payload.encode())


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None, text
    end = text.find("\n---\n", 4)
    if end == -1:
        raise ValueError("unterminated frontmatter")
    return text[4:end], text[end + 5 :]


def theme_leaf(frontmatter: str):
    lines = frontmatter.split("\n")
    if not lines or lines[0] != "description: >-":
        return None
    themes = []
    in_config = False
    for line in lines[1:]:
        if line == "config:":
            in_config = True
            continue
        if not in_config:
            if not line.startswith("  ") or line.startswith("   ") or not line.strip():
                return None
            continue
        if not line.startswith("  theme: ") or line.startswith("   "):
            return None
        themes.append(line[len("  theme: ") :])
    if len(themes) != 1 or themes[0] not in THEME_TABLE:
        return None
    return themes[0]


def body_accessibility(body: str):
    titles, descrs, kept = 0, 0, []
    for line in body.split("\n"):
        stripped = line.strip()
        if stripped.startswith("accTitle:"):
            titles += 1
        elif stripped.startswith("accDescr:"):
            descrs += 1
        else:
            kept.append(line)
    return titles, descrs, "\n".join(kept)


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    rows = []
    for path in sorted((cases / "pie").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        frontmatter, body = split_frontmatter(path.read_text())
        if frontmatter is None:
            continue
        theme = theme_leaf(frontmatter)
        if theme is None:
            continue
        titles, descrs, without = body_accessibility(body)
        if titles != 1 or descrs != 1:
            continue
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "theme": theme,
                "expected_footer_lines": [
                    HEADER,
                    f"config.theme={theme} -> colour palette; not applied",
                ],
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
                "twin_sha256": sha256_bytes(
                    ("---\n" + frontmatter + "\n---\n" + without).encode()),
                "ledger_note": (
                    "D-426 pie accessibility no-op: scalar accTitle/accDescr are "
                    "validated and consumed; the drawn frame must equal the "
                    "acc-line-deleted twin byte for byte."),
            }
        )
    if len(rows) != EXPECTED_TOTAL:
        raise ValueError(f"cohort drifted: expected {EXPECTED_TOTAL}, got {len(rows)}")
    rows.sort(key=lambda row: row["path"])
    digest = cohort_digest(rows)
    if COHORT_DIGEST != "PLACEHOLDER" and digest != COHORT_DIGEST:
        raise ValueError(f"cohort row set drifted: digest {digest} != {COHORT_DIGEST}")
    manifest_sha = sha256_bytes(manifest_bytes)
    if MANIFEST_SHA256 != "PLACEHOLDER" and manifest_sha != MANIFEST_SHA256:
        raise ValueError(f"manifest changed without review: {manifest_sha}")
    themes = {}
    for row in rows:
        themes[row["theme"]] = themes.get(row["theme"], 0) + 1
    return {
        "schema": SCHEMA,
        "decision": "D-426",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "pie original with a reviewed folded description, exactly one "
                    "closed-table theme leaf and one scalar accTitle plus one "
                    "scalar accDescr in the body",
            "cohort_digest": digest,
            "theme_table": list(THEME_TABLE),
            "exclusions": "block-form accDescr, duplicates, empty or oversized "
                          "payloads and control bytes stay whole-source; the D-425 "
                          "config namespace rows keep their own ledger",
        },
        "review": {
            "reviewer_agent": "TL exemption 2026-11-22: family slice opened without a "
                              "design gate; the gate is this test document plus the "
                              "twin-equality oracle it pins",
            "verdict": "approved",
        },
        "counts": {"total": len(rows), "themes": themes},
        "rows": rows,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
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
    print("cohort digest", ledger["selection"]["cohort_digest"])
    print("manifest sha256", ledger["upstream"]["manifest_sha256"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
