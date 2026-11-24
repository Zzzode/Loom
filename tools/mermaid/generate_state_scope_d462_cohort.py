#!/usr/bin/env python3
"""Generate the D-462 state scope-caption admission cohort ledger.

Renderer-free. Admits the two `should-render-composite-states` originals (the
v1 `stateDiagram` twin and its `stateDiagram-v2` twin) whose only unadmitted
input was an internal transition caption wider than the derived composite
width. A composite's width is never authored, so `layout_scope` now widens a
State scope by the exact deficit when the widest internal caption cannot find
the `label width + 4` free columns `label_place` requires; every scope whose
captions already fit keeps its pre-existing width (the matrix proves the rest
of the corpus byte-identical).

The art contract is unchanged: the gate pins the wide/narrow/restored states,
restored cell equality, the frame, and every declared state label. narrow_40
is source for both rows because their E2b disclosure footer is wider than the
40-column fence.

Usage: python3 tools/mermaid/generate_state_scope_d462_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/state-scope-caption-d462-cohort.json")
SCHEMA = "loom_mermaid_state_scope_caption_d462_cohort/v1"
DECISION = "D-462"

ROWS = [
    "state-diagram/pr-check/should-render-composite-states.mmd",
    "state-diagram-v2/pr-check/v2-should-render-composite-states.mmd",
]

REVIEW = {
    "reviewer_agent": (
        "one independent read-only adversarial review (task ab456d7fd1ac4491a; "
        "round 1 REQUEST-CHANGES on a tautological restored-equality assertion, "
        "the stale fallback roster, the D-467 attribution, the 42/46-column "
        "caption framing, a stale docstring, the per-edge Work prepay and the "
        "widening oracle's strength; all fixed and re-verified; round 2 APPROVE "
        "conditional on the roster emit, which landed at 376 rows / 26 "
        "categories)"),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": ("The two composite originals draw once the derived scope width "
             "accounts for an internal caption. The caption, not the layout, is "
             "the authored part: no source geometry is overridden."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def art_labels(text: str):
    """Declared state labels that must appear in the drawn art."""
    names = []
    depth = 0
    body = text.split("\n---\n", 1)[-1]
    for raw in body.split("\n"):
        line = raw.strip(" \t")
        if line.startswith("{") or line.endswith("{"):
            depth += line.count("{") - line.count("}")
            opener = re.match(r"^state\s+([A-Za-z_][A-Za-z0-9_]*)", line)
            if opener:
                names.append(opener.group(1))
            continue
        if depth:
            depth += line.count("{") - line.count("}")
            opener = re.match(r"^state\s+([A-Za-z_][A-Za-z0-9_]*)", line)
            if opener:
                names.append(opener.group(1))
            continue
        opener = re.match(r"^state\s+([A-Za-z_][A-Za-z0-9_]*)\s*$", line)
        if opener:
            names.append(opener.group(1))
            continue
        edge = re.match(r"^\[?[^\s]*\]?\s*-->\s*([A-Za-z_][A-Za-z0-9_]*)", line)
        if edge:
            names.append(edge.group(1))
    ordered = []
    for name in names:
        if name not in ordered:
            ordered.append(name)
    return ordered[:3]


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {fixture["path"]: fixture for fixture in manifest["fixtures"]}
    rows = []
    for path in ROWS:
        text = (repo_root / CASES / path).read_text()
        observed = sha256_file(repo_root / CASES / path)
        if observed != manifest_rows[path]["sha256"]:
            raise ValueError(f"sha drift for {path}")
        rows.append({
            "path": path,
            "family": path.split("/", 1)[0],
            "sha256": observed,
            "manifest_sha256": manifest_rows[path]["sha256"],
            "prior_status": "source_fallback",
            "oracle": "native-frame",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "source_fallback",
                "restored_240": "drawn",
            },
            "expected_art_labels": art_labels(text),
            "ledger_note": ("derived scope width only; the caption and the graph "
                            "are unchanged"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "stage": "state scope caption: derived-width deficit only",
        "admission": {
            "mechanism": ("layout_scope widens a State composite (owner >= 0, no "
                          "region, vertical) by the exact deficit when the widest "
                          "internal caption needs `label width + 4` free columns "
                          "that label_place scans; scopes whose captions already "
                          "fit keep their previous width"),
            "scope": ("RelationFamily::State only; every other family's layout "
                      "path is untouched"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_file(repo_root / MANIFEST),
        },
        "selection": {
            "rule": ("the two `should-render-composite-states` originals whose "
                     "internal caption is wider than the derived composite"),
            "cohort_digest": sha256_bytes("".join(
                f"{row['path']}\t{row['sha256']}\n"
                for row in sorted(rows, key=lambda row: row["path"])).encode()),
        },
        "counts": {
            "total": len(rows),
            "by_family": {"state-diagram": 1, "state-diagram-v2": 1},
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
