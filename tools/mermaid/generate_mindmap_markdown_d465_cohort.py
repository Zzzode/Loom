#!/usr/bin/env python3
"""Generate the D-465 mindmap markdown-string and escaped-break cohort ledger.

Renderer-free. Admits the last two mindmap body rows:

1. `3-tidy-tree-should-render-a-mindmap-with-different-shapes.mmd` — its only
   unadmitted inputs were the escaped break `&lt;br/>` inside a default-node
   caption (accepted as a row break) and two things D-464 already admitted
   (shapes, `::icon`). Its shaped canvas (221x37) also exhausted the fixed 1M
   Work primary before the drawing fees, so the shaped layout now funds its
   canvas from a bounded canvas-derived pool
   (`w*h*(2*sizeof(Cell)+17) + 262144`), exactly the charge it will make.
2. `formatted-label-with-linebreak-and-a-wrapping-label-and-emojis.mmd` —
   backtick markdown-string labels carrying `**bold**`/`*italic*` spans and a
   physical line break. The text is kept, the emphasis is dropped and
   disclosed, and each explicit line is drawn as its own row.

A source carrying none of these constructs charges exactly what it charged
before (the envelope only lifts the fixed primary when it is exhausted).

The art contract is unchanged: the gate pins the wide/narrow/restored states,
restored cell equality, the captions and the disclosure lines. narrow_40 is
source for both rows (their art is wider than the 40-column fence).

Usage: python3 tools/mermaid/generate_mindmap_markdown_d465_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/mindmap-markdown-d465-cohort.json")
SCHEMA = "loom_mermaid_mindmap_markdown_d465_cohort/v1"
DECISION = "D-465"

ICONS = "::icon annotations -> icon glyph; not applied"
EMPHASIS = "markdown emphasis -> plain text; not applied"

ROWS = [
    ("mindmap/pr-check/3-tidy-tree-should-render-a-mindmap-with-different-shapes.mmd",
     [ICONS],
     ["On effectiveness", "and features",
      "British popular psychology author Tony Buzan", "I am a cloud", "I am a bang"]),
    ("mindmap/pr-check/formatted-label-with-linebreak-and-a-wrapping-label-and-emojis.mmd",
     [EMPHASIS],
     ["Start with", "second line", "The dog in", "hog..."]),
]

REVIEW = {
    "reviewer_agent": (
        "one independent read-only adversarial review (task a691a0c10b0b8b073); "
        "round 1 REQUEST-CHANGES on the Shapes4 deep-chain budget negative that "
        "the envelope now draws (BLOCKER), a 40-unit pre-envelope charge, CRLF "
        "leakage, an uncapped default row count, a misplaced CMake comment and "
        "this placeholder provenance; all fixed and re-verified"),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": ("The last two mindmap body rows: an escaped row break and a "
             "markdown-string label. The text is preserved; only emphasis is "
             "dropped, and it is disclosed."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {fixture["path"]: fixture for fixture in manifest["fixtures"]}
    rows = []
    for path, notices, captions in ROWS:
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
            "expected_notices": notices,
            "expected_captions": captions,
            "ledger_note": ("row breaks are drawn; markdown emphasis is dropped "
                            "and disclosed; the shaped canvas is funded from a "
                            "bounded canvas-derived pool"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "stage": "mindmap markdown-string labels, escaped row break and canvas envelope",
        "admission": {
            "mechanism": ("a backtick markdown-string bracket payload may span "
                          "physical lines; `**bold**`/`*italic*` spans are "
                          "stripped and every other marker form rejects. Any "
                          "caption may split on the exact escape `&lt;br/>`. "
                          "The shaped layout enables a canvas-derived Work "
                          "envelope before its area-proportional charges"),
            "bound": ("mindmap_envelope_layout_allowance(w,h) = "
                      "w*h*(2*sizeof(Cell)+17) + 262144, i.e. exactly the "
                      "canvas fee plus one unit per emitted cell and a fixed "
                      "margin; a tree that fits the primary never reaches the "
                      "clamp"),
            "scope": ("the mindmap shaped-tree layout only; the original "
                      "projection and every other family are untouched"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_file(repo_root / MANIFEST),
        },
        "selection": {
            "rule": ("the two mindmap originals still in the fallback roster "
                     "after D-464"),
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
