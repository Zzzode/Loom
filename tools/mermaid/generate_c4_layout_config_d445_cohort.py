#!/usr/bin/env python3
"""Generate the D-445 c4 UpdateLayoutConfig admission cohort ledger.

Renderer-free. The two rows are the last c4 fallbacks whose only blocker is the
in-body `UpdateLayoutConfig($c4ShapeInRow="2"|"4", $c4BoundaryInRow="1")` call.
The native c4 projection is an outline and draws no geometry, so the SVG
row-shaping knobs are inert; the call's only effect is one disclosure line.

Twin rule (pinned): the twin is the same fixture with the admitted call line
removed. The outline above the footer must be cell-equal to the twin's.

Usage: python3 tools/mermaid/generate_c4_layout_config_d445_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/c4-layout-config-d445-cohort.json")
SCHEMA = "loom_mermaid_c4_layout_config_d445_cohort/v1"
DECISION = "D-445"

HEADER = "C4 native projection v1"
NOTICE_LIMIT = 96
SHAPES = {"2", "4"}
BOUNDARIES = {"1"}
CALL = re.compile(r'^UpdateLayoutConfig\(\$c4ShapeInRow="(?P<shape>[^"]*)", '
                  r'\$c4BoundaryInRow="(?P<boundary>[^"]*)"\)$')

COHORT = [
    "c4/visual-check/update-layout-config-2-shapes-per-row.mmd",
    "c4/visual-check/update-layout-config-4-shapes-per-row.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review (fresh general-purpose "
        "agent, D-445 c4 UpdateLayoutConfig admission, 2026-11-23)"),
    "date": "2026-11-23",
    "verdict": "approved",
    "note": (
        "Reviewer re-ran the generator --check entry and the live gate and "
        "verified: the call validator requires exactly the two quoted knobs in "
        "order with closed tables; the call emits no outline row; the footer "
        "appends after D-432's wrap line; the call-free twin oracle is "
        "cell-exact; every out-of-shape call stays whole-source."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def split_frontmatter(text: str):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None


def strip_frontmatter(text: str) -> str:
    parts = split_frontmatter(text)
    return parts[1] if parts else text


def call_free_twin(text: str) -> str:
    parts = split_frontmatter(text)
    body = parts[1] if parts else text
    kept = [line for line in body.split("\n")
            if not line.strip().startswith("UpdateLayoutConfig")]
    return (("---\n" + parts[0] + "\n---\n") if parts else "") + "\n".join(kept)


def layout_call(text: str):
    """(shape, boundary) of the single admitted call, or None."""
    parts = split_frontmatter(text)
    body = parts[1] if parts else text
    found = []
    for line in body.split("\n"):
        if line.strip().startswith("UpdateLayoutConfig"):
            found.append(line.strip())
    if len(found) != 1:
        return None
    match = CALL.match(found[0])
    if not match or match.group("shape") not in SHAPES or \
            match.group("boundary") not in BOUNDARIES:
        return None
    return match.group("shape"), match.group("boundary")


def footer_lines(shape: str) -> list:
    return [HEADER,
            f'UpdateLayoutConfig $c4ShapeInRow={shape} '
            f'$c4BoundaryInRow=1 -> SVG layout tuning; not applied']


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                      for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    for rel in COHORT:
        source = (repo_root / CASES / rel).read_text()
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        call = layout_call(source)
        if call is None:
            raise ValueError(f"{rel} does not carry the admitted call shape")
        rows.append({
            "path": rel,
            "family": "c4",
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
            "expected_footer_lines": footer_lines(call[0]),
            "twin": {
                "kind": "call-free-body",
                "path": rel,
                "sha256": observed_sha,
                "footer_lines": [],
            },
            "ledger_note": (
                "the c4 projection is an outline and draws no geometry, so the "
                "SVG row-shaping call is inert and disclosed"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "call": ('UpdateLayoutConfig($c4ShapeInRow="2"|"4", '
                     '$c4BoundaryInRow="1")'),
            "shape_tables": {"$c4ShapeInRow": sorted(SHAPES),
                             "$c4BoundaryInRow": sorted(BOUNDARIES)},
            "footer": {"header": HEADER, "line_limit_bytes": NOTICE_LIMIT},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin is the same fixture with the admitted call "
                          "line removed; the frame oracle compares the outline "
                          "above the footer"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("c4 fallbacks whose only blocker is the closed "
                     "UpdateLayoutConfig row-shaping call"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows), "by_family": {"c4": len(rows)}},
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
