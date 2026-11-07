#!/usr/bin/env python3
"""Generate the RFC 0031 FS-5 G7 misc-shape cohort ledger.

Seventh GROUPED FS-5 batch: three remaining standalone-alias flowchart
contours share one ledger, matrix run, review, and commit.

- Shaded/lined process (lin-rect/lined-rectangle/lin-proc/lined-process/
  shaded-process; shadedProcess.ts): a main chamber with a one-column
  outer frame strip on the left.
- Curved trapezoid / display (curv-trap/curved-trapezoid/display;
  curvedTrapezoid.ts): a two-row chevron point on the left feeding a
  straight body with a half-round cap on the right.
- Paper tape / flag (flag/paper-tape; waveRectangle.ts): a rectangle
  whose top and bottom edges are both sine waves (a crest and a trough).

Selection (mechanical, byte-level): every frontmatter flowchart original
under cases/flowchart whose body binds any of the ten aliases. Three
alias smoke fixtures (15/24/35) draw on both tracks. Eight shape-themes
connect-all rows stay source: after this batch the 03/13/24/33 group
binds only already-mapped shapes (held by neo config + single quotes),
while 06/16/26/36 still bind the unmapped `text` short-name shape. The
visual-check selected-shape-wrapping regression stays source: its shape
nodes carry explicit <br/> line breaks and classic shapes admit only a
single label row, independent of its wrapping config.

Usage: python3 tools/mermaid/generate_fs5g7_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs5g7-cohort.json")

SCHEMA = "loom_mermaid_fs5g7_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-g7-neo-quoted-only",
    "flow-g7-text-shape-debt",
    "flow-g7-multiline-shape-label",
]

WITNESS = {
    "flow-g7-neo-quoted-only": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "Every bound shape is mapped, but the whole original is rejected by "
        "neo look/theme config and the body independently by single-quoted "
        "shape-data labels (flow_shape_data accepts only double quotes).",
    ),
    "flow-g7-text-shape-debt": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a lined-process node among the still-unmapped "
        "`text` short-name shape (plus neo config / single-quoted labels); "
        "the whole source stays on ordinary fallback until text is admitted.",
    ),
    "flow-g7-multiline-shape-label": (
        "src/ui/visual/mermaid_impl.cpp (node_size single-row guard)",
        "The shape nodes carry explicit <br/> line breaks; classic glyph "
        "shapes admit only a single label row (node_size returns empty for a "
        "multi-row label), so the body stays on ordinary fallback regardless "
        "of the wrapping/min-node-width frontmatter config.",
    ),
}

GROUP_RE = re.compile(
    r"shape:\s*(?:lin-rect|lined-rectangle|lin-proc|lined-process|"
    r"shaded-process|curv-trap|curved-trapezoid|display|flag|paper-tape)"
    r"(?![\w-])")

DRAW_FIXTURES = {
    "flowchart/nightly-check/shape-alias/"
    "15-all-lin-rect-lined-rectangle-lin-proc-lined-process-shaded-process-should-render-same-shape.mmd":
        ["lin-rect", "lined-rectangle", "lin-proc", "lined-process",
         "shaded-process"],
    "flowchart/nightly-check/shape-alias/"
    "24-all-curv-trap-display-curved-trapezoid-should-render-same-shape.mmd":
        ["curv-trap", "display", "curved-trapezoid"],
    "flowchart/nightly-check/shape-alias/"
    "35-all-flag-paper-tape-should-render-same-shape.mmd":
        ["flag", "paper-tape"],
}

# Only fixture 35's two short nodes fit a 40-column canvas.
NARROW_DRAW = {
    "flowchart/nightly-check/shape-alias/"
    "35-all-flag-paper-tape-should-render-same-shape.mmd",
}

# Explicit blockers for non-alias rows (the connect-all prefix groups plus
# the one visual-check regression).
BLOCKED_KIND = {
    "03": "flow-g7-neo-quoted-only",
    "13": "flow-g7-neo-quoted-only",
    "24": "flow-g7-neo-quoted-only",
    "33": "flow-g7-neo-quoted-only",
    "06": "flow-g7-text-shape-debt",
    "16": "flow-g7-text-shape-debt",
    "26": "flow-g7-text-shape-debt",
    "36": "flow-g7-text-shape-debt",
}
WRAPPING_ROW = "flowchart/visual-check/selected-shape-wrapping.mmd"


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
        _, body = split
        if not GROUP_RE.search(body):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        is_draw = rel in DRAW_FIXTURES
        kind = None
        if not is_draw:
            if rel == WRAPPING_ROW:
                kind = "flow-g7-multiline-shape-label"
            else:
                prefix = path.name.split("-", 1)[0]
                if prefix not in BLOCKED_KIND:
                    raise ValueError(f"unclassified G7 row: {rel}")
                kind = BLOCKED_KIND[prefix]
        shapes = DRAW_FIXTURES.get(rel)
        if not is_draw:
            shapes = sorted(set(re.findall(r"shape:\s*([a-z0-9-]+)", body)))
        draw += is_draw
        blocked += (not is_draw)
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
                "FS-5 G7 misc-shape contour admitted" if is_draw
                else "Blocked by independent shape/config/label debt"),
        })
    if draw != 3 or blocked != 9:
        raise ValueError(f"partition drift: draw={draw} blocked={blocked}")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-5-g7-misc-shapes",
        "shape_projection": {
            "group": [
                {"shape": "shaded/lined process",
                 "aliases": ["lin-rect", "lined-rectangle", "lin-proc",
                             "lined-process", "shaded-process"],
                 "contour": "main chamber with a one-column outer frame "
                            "strip on the left"},
                {"shape": "curved trapezoid (display)",
                 "aliases": ["curv-trap", "curved-trapezoid", "display"],
                 "contour": "left two-row chevron point, right half-round "
                            "cap; minimum eleven columns"},
                {"shape": "paper tape (flag)",
                 "aliases": ["flag", "paper-tape"],
                 "contour": "rectangle with a sine-wave crest on the top "
                            "edge and trough on the bottom edge"},
            ],
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original binding an FS-5 G7 "
                    "shape alias",
            "roots": ["flowchart"],
            "draw_authority": "the three alias smoke fixtures (15/24/35)",
            "width_policy": "draw rows paint body+original at 240 and "
                           "restored; narrow_40 is per-row.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(fresh general-purpose agent, D-066 FS-5 G7 "
                              "misc shapes, 2026-11-07)",
            "date": "2026-11-07",
            "verdict": "approved",
            "note": ("Independent read-only adversarial review (fresh "
                     "general-purpose agent, 2026-11-07): APPROVE with no "
                     "MAJOR geometry defect. Addressed before commit: "
                     "bottom-entering (BT) edge cases added for all three "
                     "shapes plus row-level contour assertions and an "
                     "even-width paper-tape crest-axis alignment guard; the "
                     "selection regex rejects hyphen-suffixed near-misses; "
                     "stale cohort header comment corrected. The eleven "
                     "earlier fail-closed negatives that borrowed paper-tape "
                     "moved to the still-unmapped cloud shape."),
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
