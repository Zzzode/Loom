#!/usr/bin/env python3
"""Generate the D-448 xychart negative-bar data-label cohort ledger.

Renderer-free. The two rows are the negative/decimal bar fixtures whose only
blocker was the compact data-label profile refusing bars on the negative side
of the zero reference. The profile now places the label at the authored tip on
either side and treats a one-cell bar as an external label; each value must
appear exactly once in the frame.

Usage: python3 tools/mermaid/generate_xychart_negative_labels_d448_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/xychart-negative-labels-d448-cohort.json")
SCHEMA = "loom_mermaid_xychart_negative_labels_d448_cohort/v1"
DECISION = "D-448"

COHORT = [
    ("xychart/nightly-check/"
     "should-render-negative-and-decimal-values-with-correct-labels-for-vertical-xy-chart.mmd",
     ["-2.5", "0.75", "5.1"]),
    ("xychart/pr-check/"
     "should-render-negative-and-decimal-values-with-correct-labels-for-horizontal-xy-chart.mmd",
     ["-2.5", "0.75", "5.1"]),
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = {
    "xychart/nightly-check/"
    "should-render-negative-and-decimal-values-with-correct-labels-for-vertical-xy-chart.mmd",
}

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review (fresh general-purpose "
        "agent, D-448 xychart negative-bar labels, 2026-11-23)"),
    "date": "2026-11-23",
    "verdict": "approved",
    "note": (
        "Reviewer re-ran the generator --check entry and the live gate and "
        "verified: the sample window includes the tip on both sides of the "
        "reference; one-cell bars label externally and never erase the mark; "
        "each value token appears exactly once; zero-valued bar series stay "
        "whole-source."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                      for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    for rel, tokens in COHORT:
        source = (repo_root / CASES / rel).read_text()
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        if "showDataLabel: true" not in source or "bar [ -2.5, 0.75, 5.1 ]" not in source:
            raise ValueError(f"{rel} does not carry the negative-label profile")
        rows.append({
            "path": rel,
            "family": "xychart",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": "source_fallback",
            "prediction": "predicted-draw",
            "track_scope": "whole-only",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn" if rel in NARROW_DRAW else "source_fallback",
                "restored_240": "drawn",
            },
            "value_tokens": tokens,
            "ledger_note": (
                "compact bar data labels at the authored tip on both sides of "
                "the zero reference; one-cell bars label externally"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "profile": "compact single-bar showDataLabel with one-cell/negative tips",
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "oracle": "each authored value token appears exactly once in the frame",
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("the two negative/decimal showDataLabel bar fixtures"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows), "by_family": {"xychart": len(rows)}},
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
    sys.exit(main())
