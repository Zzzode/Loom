#!/usr/bin/env python3
"""Generate the D-463 git Work-envelope admission cohort ledger.

Renderer-free. Admits the eight gitGraph originals whose whole-render Work
exceeds the fixed 1M primary but which are otherwise complete: the two
9-branch graphs and the six cherry-pick graphs (all six exercises of the
`cherry-pick id: … parent: …` form). A bounded canvas-derived supplemental
pool (`git_envelope_layout_allowance(w, h) = w*h*768 + 65536`, enabled only by
the git layout from the planned canvas) funds them; graphs that fit the
primary never reach the clamp, so their charge sequence and bytes are
unchanged.

The art contract is unchanged: the gate pins wide/narrow/restored states,
restored cell equality and the drawn markers. narrow_40 is source for every
row (the git projection's captions exceed the 40-column fence).

Usage: python3 tools/mermaid/generate_git_work_d463_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/git-envelope-d463-cohort.json")
SCHEMA = "loom_mermaid_git_envelope_d463_cohort/v1"
DECISION = "D-463"

ROWS = [
    "git/nightly-check/12-should-render-commits-for-more-than-8-branches.mmd",
    "git/nightly-check/74-should-render-commits-for-more-than-8-branches-using-switch-instead-of-checkout.mmd",
    "git/visual-check/multiple-branches-cycling-theme-color-limit-and-cherry-pick-merge-commit-with-parent-neo-look-neo-dark-theme.mmd",
    "git/visual-check/multiple-branches-cycling-theme-color-limit-and-cherry-pick-merge-commit-with-parent-neo-look-neo-theme.mmd",
    "git/visual-check/multiple-branches-cycling-theme-color-limit-and-cherry-pick-merge-commit-with-parent-neo-look-redux-color-theme.mmd",
    "git/visual-check/multiple-branches-cycling-theme-color-limit-and-cherry-pick-merge-commit-with-parent-neo-look-redux-dark-color-theme.mmd",
    "git/visual-check/multiple-branches-cycling-theme-color-limit-and-cherry-pick-merge-commit-with-parent-neo-look-redux-dark-theme.mmd",
    "git/visual-check/multiple-branches-cycling-theme-color-limit-and-cherry-pick-merge-commit-with-parent-neo-look-redux-theme.mmd",
]

REVIEW = {
    "reviewer_agent": (
        "one independent read-only adversarial review (task a7fe534c4a6a6808e); "
        "round 1 REQUEST-CHANGES on the stale E2a count pins, a gates log "
        "without the E2a target, unreproducible judgement-table totals and "
        "composition counts, an overclaimed byte-identity argument and an "
        "overstated envelope bound; all fixed, round 2 APPROVE"),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": ("Eight complete gitGraph originals were blocked only by the fixed "
             "1M Work primary. A bounded canvas-derived pool admits them; no "
             "grammar or geometry changes."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def body_of(text: str) -> str:
    lines = text.split("\n")
    if lines and lines[0] == "---":
        for index in range(1, len(lines)):
            if lines[index] == "---":
                return "\n".join(lines[index + 1:])
    return text


def branch_labels(text: str):
    """Branch names the projection must draw as branch refs (body only)."""
    names = []
    for raw in body_of(text).split("\n"):
        line = raw.strip()
        for prefix in ("branch ", "checkout ", "switch "):
            if line.startswith(prefix):
                name = line[len(prefix):].strip().strip('"')
                if name and name not in names:
                    names.append(name)
    return names[:5]


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {fixture["path"]: fixture for fixture in manifest["fixtures"]}
    rows = []
    for path in ROWS:
        text = (repo_root / CASES / path).read_text()
        observed = sha256_file(repo_root / CASES / path)
        if observed != manifest_rows[path]["sha256"]:
            raise ValueError(f"sha drift for {path}")
        if "gitGraph" not in text:
            raise ValueError(f"not a gitGraph source: {path}")
        rows.append({
            "path": path,
            "family": "git",
            "sha256": observed,
            "manifest_sha256": manifest_rows[path]["sha256"],
            "prior_status": "source_fallback",
            "oracle": "native-frame",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "source_fallback",
                "restored_240": "drawn",
            },
            "expected_branch_refs": branch_labels(text),
            "ledger_note": ("the fixed 1M Work primary was the only blocker; the "
                            "art contract is unchanged"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "stage": "git Work envelope: bounded canvas-derived supplemental pool",
        "admission": {
            "mechanism": ("gitgraph_layout enables the two-pool meter from the "
                          "planned canvas before the grid/route/emission fees "
                          "(git_envelope_layout_allowance(w,h) = w*h*768 + 65536)"),
            "bound": ("measured whole-render totals 3,575,502 (216x29, 571/cell) "
                      "and 1,788,063 (148x30, 403/cell); the bound is 768/cell "
                      "plus 65,536, i.e. >=34% headroom at the binding row. The "
                      "rate is empirical, not a worst-case proof: a crafted "
                      "denser graph can exceed it and then fails closed to whole "
                      "source"),
            "scope": ("enabled only by the git layout; every other family and "
                      "every default Work keeps the single-balance clamp"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_file(repo_root / MANIFEST),
        },
        "selection": {
            "rule": ("the eight gitGraph originals whose whole render exceeds the "
                     "1M primary but completes under a larger budget (probe "
                     "/tmp/d461/bigwork-all.log)"),
            "cohort_digest": sha256_bytes("".join(
                f"{row['path']}\t{row['sha256']}\n"
                for row in sorted(rows, key=lambda row: row["path"])).encode()),
        },
        "counts": {
            "total": len(rows),
            "by_family": {"git": len(rows)},
        },
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
