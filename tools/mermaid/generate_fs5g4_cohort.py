#!/usr/bin/env python3
"""Generate the RFC 0031 FS-5 G4 stacked/tagged rectangle cohort ledger.

Fourth GROUPED FS-5 batch: two offset-card contours share one ledger,
matrix run, review, and commit. Both extend their box by one column beyond
the front rectangle to paint a feature outside the front chamber.

- Stacked rectangle (multi-process): procs/processes/st-rect/
  stacked-rectangle. A front rectangle with a second sheet peeking at the
  top-right (upstream shapes/multiRect.ts rectOffset).
- Tagged rectangle (tagged process): tag-rect/tag-proc/tagged-rectangle/
  tagged-process. A rectangle with a folded corner tag extending one
  column at the bottom-right (upstream shapes/taggedRect.ts).

Selection (mechanical, byte-level): every frontmatter flowchart original
under cases/flowchart whose body binds any of the eight aliases. Two
alias smoke fixtures draw (34/39); four shape-themes connect-all fixtures
(02/12/22/32) bind a tagged rectangle plus the unmapped documents/
filled-circle shapes and neo config, so they stay source. There is no
stacked-rectangle compound row.

Usage: python3 tools/mermaid/generate_fs5g4_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs5g4-cohort.json")

SCHEMA = "loom_mermaid_fs5g4_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-g4-tag-compound",
]

WITNESS = {
    "flow-g4-tag-compound": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a tagged-rectangle node among other exotic shape "
        "aliases (documents, filled-circle) whose contours the terminal does "
        "not project (and neo theme config); the whole source stays on "
        "ordinary fallback until those shapes are admitted.",
    ),
}

GROUP_RE = re.compile(
    r"shape:\s*(?:procs|processes|st-rect|stacked-rectangle|tag-rect|"
    r"tag-proc|tagged-rectangle|tagged-process)\b")
TAG_RE = re.compile(
    r"shape:\s*(?:tag-rect|tag-proc|tagged-rectangle|tagged-process)\b")

DRAW_FIXTURES = {
    "flowchart/nightly-check/shape-alias/"
    "34-all-procs-processes-st-rect-stacked-rectangle-should-render-same-shape.mmd":
        ["procs", "processes", "st-rect", "stacked-rectangle"],
    "flowchart/nightly-check/shape-alias/"
    "39-all-tag-rect-tag-proc-tagged-rectangle-tagged-process-should-render-same-shape.mmd":
        ["tag-rect", "tag-proc", "tagged-rectangle", "tagged-process"],
}
NARROW_DRAW = set()


def split_source(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    draw = blocked = 0
    for path in sorted((repo_root / CASES / "flowchart").rglob("*.mmd")):
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        frontmatter, body = split
        if not GROUP_RE.search(body):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        is_draw = rel in DRAW_FIXTURES
        if is_draw and "theme" in frontmatter:
            raise ValueError(f"draw fixture unexpectedly carries theme: {rel}")
        draw += is_draw
        blocked += (not is_draw)
        shapes = DRAW_FIXTURES.get(rel)
        kind = None
        if not is_draw:
            if not TAG_RE.search(body):
                raise ValueError(f"unclassified blocked row: {rel}")
            kind = "flow-g4-tag-compound"
            shapes = ["tag-rect", "tag-proc", "tagged-rectangle", "tagged-process"]
        site, detail = WITNESS[kind] if not is_draw else ("", "")
        narrow = "drawn" if is_draw and rel in NARROW_DRAW else "source"
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / rel),
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw" if is_draw else "predicted-blocked",
            "expected_states": {
                "wide_240": "drawn" if is_draw else "source",
                "narrow_40": narrow,
                "restored_240": "drawn" if is_draw else "source",
            },
            "expected_body_states": {
                "wide_240": "drawn" if is_draw else "source",
                "narrow_40": narrow,
                "restored_240": "drawn" if is_draw else "source",
            },
            "shapes": shapes,
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if not is_draw else None),
            "ledger_note": (
                "FS-5 G4 stacked/tagged rectangle contour admitted" if is_draw
                else "Blocked by compound exotic-shape/theme debt"),
        })
    if draw != 2 or blocked != 4:
        raise ValueError(f"partition drift: draw={draw} blocked={blocked}")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-5-g4-stacked-tagged-rectangles",
        "shape_projection": {
            "group": [
                {"shape": "stacked rectangle",
                 "aliases": ["procs", "processes", "st-rect",
                             "stacked-rectangle"],
                 "contour": "front rectangle with a back sheet peeking at "
                            "the top-right"},
                {"shape": "tagged rectangle",
                 "aliases": ["tag-rect", "tag-proc", "tagged-rectangle",
                             "tagged-process"],
                 "contour": "rectangle with a folded corner tag extending "
                            "one column at the bottom-right"},
            ],
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original binding a stacked or "
                    "tagged rectangle shape",
            "roots": ["flowchart"],
            "draw_authority": "the two alias smoke fixtures (34/39); the "
                              "four shape-themes rows carry independent "
                              "documents/filled-circle and neo theme debt",
            "width_policy": "draw rows paint body+original at 240 and "
                           "restored; narrow_40 is per-row.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(fresh general-purpose agent, D-066 FS-5 G4 "
                              "stacked/tagged rectangles, 2026-11-07)",
            "date": "2026-11-07",
            "verdict": "approved",
            "note": ("Independent read-only adversarial review (fresh "
                     "general-purpose agent, 2026-11-07): APPROVE with no "
                     "MAJOR. Addressed before commit: tagged tip glyph "
                     "corrected to the up-and-left corner matching the "
                     "upstream right triangle; stacked back-sheet top cell "
                     "uses a horizontal edge glyph; right-direction (LR "
                     "outgoing / RL incoming) edge cases added for both "
                     "shapes; generator blocker predicate made row-level."),
        },
        "counts": {
            "total": draw + blocked,
            "predicted_draw": draw,
            "predicted_blocked": blocked,
            "predicted_original_draw": draw,
        },
        "blocker_vocabulary": BLOCKER_VOCABULARY,
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
