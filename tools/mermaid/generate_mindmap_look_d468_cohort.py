#!/usr/bin/env python3
"""Generate the D-468 mindmap closed look/theme cohort ledger.

Renderer-free. Admits the six `all-shapes-neo-look-*` mindmap originals whose
frontmatter carries the closed `config.look: neo` leaf and one of the six
closed theme names (`neo`, `neo-dark`, `redux`, `redux-dark`, `redux-color`,
`redux-dark-color`). A cell grid has no sketch stroke and no colour, so both
leaves are inert for the terminal projection and are disclosed once each on
the canonical footer; the body is drawn exactly as the config-stripped twin.

Every other look value, theme name, config key, duplicate, init directive,
title or extra field keeps the whole source on fallback. All rows stay whole
source at 40 columns because the footer is wider than the fence.

Usage: python3 tools/mermaid/generate_mindmap_look_d468_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/mindmap-look-d468-cohort.json")
SCHEMA = "loom_mermaid_mindmap_look_d468_cohort/v1"
DECISION = "D-468"

LOOK = "config.look=neo -> source node style; not applied"


def theme_notice(name):
    return f"config.theme={name} -> colour palette; not applied"


ROWS = [
    ("mindmap/nightly-check/all-shapes-neo-look-neo-theme.mmd", "neo"),
    ("mindmap/nightly-check/all-shapes-neo-look-neo-dark-theme.mmd", "neo-dark"),
    ("mindmap/nightly-check/all-shapes-neo-look-redux-theme.mmd", "redux"),
    ("mindmap/nightly-check/all-shapes-neo-look-redux-dark-theme.mmd", "redux-dark"),
    ("mindmap/nightly-check/all-shapes-neo-look-redux-color-theme.mmd", "redux-color"),
    ("mindmap/nightly-check/all-shapes-neo-look-redux-dark-color-theme.mmd",
     "redux-dark-color"),
]

CAPTIONS = ["All Shapes", "Square Branch", "Hexagon Node", "Another Default"]

REVIEW = {
    "reviewer_agent": (
        "task a28cf46e7351de47d round 1 REQUEST-CHANGES (2 MAJOR, 4 NIT) -> "
        "round 2 APPROVE"),
    "date": "2026-11-25",
    "verdict": "approved",
    "note": ("Six mindmap originals carry the closed `look: neo` leaf and one "
             "closed theme name. Both are inert for the terminal projection "
             "(no sketch stroke, no colour); admitted and disclosed, and the "
             "art equals the config-free twin with cells compared verbatim. "
             "Round 1: the review record was a placeholder and the post-fix "
             "release/dual-preset evidence was missing; round 2 APPROVE, "
             "subject to the ledger finalization rebuild-and-rerun."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {fixture["path"]: fixture for fixture in manifest["fixtures"]}
    rows = []
    for path, theme in ROWS:
        observed = sha256_file(repo_root / CASES / path)
        if observed != manifest_rows[path]["sha256"]:
            raise ValueError(f"sha drift for {path}")
        rows.append({
            "path": path,
            "family": "mindmap",
            "sha256": observed,
            "manifest_sha256": manifest_rows[path]["sha256"],
            "prior_status": "source_fallback",
            "oracle": "native-frame",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "source_fallback",
                "restored_240": "drawn",
            },
            "expected_notices": [LOOK, theme_notice(theme)],
            "expected_captions": CAPTIONS,
            "ledger_note": ("closed look/theme frontmatter; the terminal draws "
                            "no sketch stroke and no colour"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "stage": "mindmap closed look/theme frontmatter table",
        "admission": {
            "mechanism": ("the mindmap envelope accepts, alongside the D-439 "
                          "`config.layout: tidy-tree` leaf, the closed "
                          "`config.look: neo` leaf and the closed six-name "
                          "`config.theme` palette (neo, neo-dark, redux, "
                          "redux-dark, redux-color, redux-dark-color); each is "
                          "disclosed once and the graph art is unchanged"),
            "scope": ("the mindmap frontmatter profile only; no renderer "
                      "change and no colour or sketch stroke is applied"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_file(repo_root / MANIFEST),
        },
        "selection": {
            "rule": ("the mindmap originals still in the fallback roster whose "
                     "only non-admitted input is the closed look/theme pair"),
            "cohort_digest": sha256_bytes("".join(
                f"{row['path']}\t{row['sha256']}\n"
                for row in sorted(rows, key=lambda row: row["path"])).encode()),
        },
        "counts": {"total": len(rows), "by_family": {"mindmap": len(rows)}},
        "review": REVIEW,
        "rows": rows,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    repo_root = pathlib.Path(args.repo_root)
    ledger = build(repo_root)
    payload = json.dumps(ledger, indent=2) + "\n"
    target = repo_root / LEDGER
    if args.check:
        if not target.exists() or target.read_text() != payload:
            print(f"ledger is stale: run without --check to rewrite {LEDGER}")
            return 1
        print(f"ledger current: {json.dumps(ledger['counts'])}")
        return 0
    target.write_text(payload)
    print(f"wrote {LEDGER}: {json.dumps(ledger['counts'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
