#!/usr/bin/env python3
"""Generate the RFC 0031 FS-aws icon-shape cohort ledger.

The AWS icon pack is expressed through shape data, e.g.
`Cloudwatch@{ icon: "aws:arch-amazon-cloudwatch", label: "CloudWatch" }`.
flow_shape_data previously rejected every key other than `label:` and
`shape:`, so these nodes stayed on ordinary fallback. This cohort admits
a closed vocabulary of three pinned AWS architecture icon names: the
terminal has no icon font, so each admitted icon projects to one fixed
marker cell (U+25A0) prepended to the human label (or, absent a label
key, the node id) inside an ordinary rectangle. Any non-aws namespace,
unknown icon, or extra key fails closed.

All three fixtures carry description-only frontmatter, so once the body
parses the whole original is admitted through the existing envelope
profile; every frame (240/40/restored) draws.

Usage: python3 tools/mermaid/generate_aws_icon_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/aws-icon-cohort.json")

SCHEMA = "loom_mermaid_aws_icon_cohort/v1"

ROWS = [
    "flowchart/visual-check/should-render-aws-icons-with-labels-and-rect-elements.mmd",
    "flowchart/visual-check/should-render-multiple-aws-icons-with-rect-elements-in-a-flowchart.mmd",
    "flowchart/visual-check/should-render-single-aws-icon-with-rect-elements-without-unintended-opacity.mmd",
]


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    for rel in ROWS:
        if rel not in manifest_rows:
            raise ValueError(f"ledger row missing from manifest: {rel}")
        observed = sha256_file(repo_root / CASES / rel)
        if observed != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": observed,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn",
                "restored_240": "drawn",
            },
            "expected_body_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn",
                "restored_240": "drawn",
            },
            "ledger_note": (
                "FS-aws closed-vocabulary icon shape data admitted; icon "
                "projects to a fixed marker cell, human text retained"
            ),
        })
    review = {
        "reviewer_agent": (
            "independent read-only adversarial review (fresh general-purpose "
            "agent, FS-aws icon shape data, 2026-11-08)"),
        "date": "2026-11-08",
        "verdict": "REVIEW_PENDING",
        "note": "Filled by the independent review before family closure.",
    }
    existing_path = repo_root / LEDGER
    if existing_path.exists():
        try:
            prior = json.loads(existing_path.read_text())
            if isinstance(prior.get("review"), dict):
                review = prior["review"]
        except json.JSONDecodeError:
            pass
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-aws-icon-shape",
        "icon_projection": {
            "namespace": "aws",
            "closed_vocabulary": [
                "arch-amazon-cloudwatch",
                "arch-amazon-eks-cloud",
                "arch-amazon-route-53",
            ],
            "rendering": (
                "one fixed U+25A0 marker cell prepended to the human label "
                "or node id inside a rectangle; unknown icons and non-aws "
                "namespaces fail closed"
            ),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": (
                "every flowchart fixture binding a double-quoted "
                "@{ icon: \"aws:...\" } node; FontAwesome `fa:` markers and "
                "usecase diagrams use separate parsers and are excluded"
            ),
            "roots": ["flowchart"],
        },
        "review": review,
        "counts": {
            "total": len(ROWS),
            "predicted_draw": len(ROWS),
            "predicted_blocked": 0,
            "predicted_original_draw": len(ROWS),
        },
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


if __name__ == "__main__":
    raise SystemExit(main())
