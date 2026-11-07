#!/usr/bin/env python3
"""Generate the RFC 0031 FS-5 G5 wave-document cohort ledger.

Fifth GROUPED FS-5 batch: four wave-edged document contours share one
ledger, matrix run, review, and commit. All four carry a sine-wave
bottom edge (upstream generateFullSineWavePoints, 0.8 of a cycle), which
projects at cell resolution as a single centred one-cell trough on the
baseline with a horizontal floor one row below.

- Document (doc/document; waveEdgedRectangle.ts): plain rectangle with
  the wavy bottom.
- Stacked document (docs/documents/st-doc/stacked-document;
  multiWaveEdgedRectangle.ts): the stacked-rectangle back-sheet peek
  plus a wavy bottom on the front chamber.
- Lined document (lin-doc/lined-document; linedWaveEdgedRect.ts): wave
  document with an interior book-binding rule one column inside the
  left edge.
- Tagged document (tag-doc/tagged-document;
  taggedWaveEdgedRectangle.ts): wave document with a small right-side
  tag pointing outward at the chamber's middle row.

Selection (mechanical, byte-level): every frontmatter flowchart original
under cases/flowchart whose body binds any of the ten aliases. Four
alias smoke fixtures (20/33/29/38) and the document-shape-label-clearance
visual regression draw (the latter only on the body track - its
wrappingWidth/minNodeWidth config stays rejected at the whole-original
gate). Sixteen shape-themes connect-all files stay source: each binds a
document shape among still-unmapped aliases (filled-circle; crossed
circle / curved trapezoid; right brace) or, for the 04/15/23/34 group,
binds only already-mapped shapes and is rejected independently by neo
look/theme config plus single-quoted shape-data labels.

Usage: python3 tools/mermaid/generate_fs5g5_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs5g5-cohort.json")

SCHEMA = "loom_mermaid_fs5g5_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-g5-circle-debt",
    "flow-g5-cross-curve-debt",
    "flow-g5-brace-r-debt",
    "flow-g5-neo-quoted-only",
]

WITNESS = {
    "flow-g5-circle-debt": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a documents node among still-unmapped filled-circle "
        "aliases (and neo look/theme config); the whole source stays on "
        "ordinary fallback until those shapes are admitted.",
    ),
    "flow-g5-cross-curve-debt": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a tagged-document node among still-unmapped "
        "crossed-circle and curved-trapezoid aliases (and neo config); the "
        "whole source stays on ordinary fallback.",
    ),
    "flow-g5-brace-r-debt": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds document nodes among still-unmapped right-brace "
        "aliases brace-r/braces (and neo config); the whole source stays on "
        "ordinary fallback.",
    ),
    "flow-g5-neo-quoted-only": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "Every bound shape is mapped, but the whole original is rejected by "
        "neo look/theme config and the body independently by single-quoted "
        "shape-data labels (flow_shape_data accepts only double quotes); the "
        "source stays on ordinary fallback.",
    ),
}

GROUP_RE = re.compile(
    r"shape:\s*(?:doc|document|docs|documents|st-doc|stacked-document|"
    r"lin-doc|lined-document|tag-doc|tagged-document)\b")

# Four alias smoke fixtures draw on both tracks; the label-clearance
# visual regression draws only on the body track.
DRAW_BOTH = {
    "flowchart/nightly-check/shape-alias/"
    "20-all-doc-document-should-render-same-shape.mmd":
        ["doc", "document"],
    "flowchart/nightly-check/shape-alias/"
    "33-all-docs-documents-st-doc-stacked-document-should-render-same-shape.mmd":
        ["docs", "documents", "st-doc", "stacked-document"],
    "flowchart/nightly-check/shape-alias/"
    "29-all-lin-doc-lined-document-should-render-same-shape.mmd":
        ["lin-doc", "lined-document"],
    "flowchart/nightly-check/shape-alias/"
    "38-all-tag-doc-tagged-document-should-render-same-shape.mmd":
        ["tag-doc", "tagged-document"],
}
DRAW_BODY_ONLY = {
    "flowchart/visual-check/document-shape-label-clearance.mmd":
        ["doc", "docs", "lin-doc", "tag-doc"],
}

# Rows whose narrow_40 render also draws (filled after observing the
# renderer; the durable cohort gate pins each value). The two-node smoke
# fixtures with short labels fit a 40-column canvas; fixture 33 (17-char
# stacked-document labels) and the long-label clearance regression do not.
NARROW_DRAW = {
    "flowchart/nightly-check/shape-alias/"
    "20-all-doc-document-should-render-same-shape.mmd",
    "flowchart/nightly-check/shape-alias/"
    "29-all-lin-doc-lined-document-should-render-same-shape.mmd",
    "flowchart/nightly-check/shape-alias/"
    "38-all-tag-doc-tagged-document-should-render-same-shape.mmd",
}

# Blocker kind per connect-all file group.
BLOCKED_KIND = {
    "02": "flow-g5-circle-debt",
    "12": "flow-g5-circle-debt",
    "22": "flow-g5-circle-debt",
    "32": "flow-g5-circle-debt",
    "03": "flow-g5-cross-curve-debt",
    "13": "flow-g5-cross-curve-debt",
    "24": "flow-g5-cross-curve-debt",
    "33": "flow-g5-cross-curve-debt",
    "05": "flow-g5-brace-r-debt",
    "14": "flow-g5-brace-r-debt",
    "25": "flow-g5-brace-r-debt",
    "35": "flow-g5-brace-r-debt",
    "04": "flow-g5-neo-quoted-only",
    "15": "flow-g5-neo-quoted-only",
    "23": "flow-g5-neo-quoted-only",
    "34": "flow-g5-neo-quoted-only",
}


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
        body_only = rel in DRAW_BODY_ONLY
        is_draw = rel in DRAW_BOTH or body_only
        if rel not in DRAW_BOTH and rel not in DRAW_BODY_ONLY:
            prefix = path.name.split("-", 1)[0]
            if prefix not in BLOCKED_KIND:
                raise ValueError(f"unclassified G5 row: {rel}")
        kind = None if is_draw else BLOCKED_KIND[path.name.split("-", 1)[0]]
        shapes = DRAW_BOTH.get(rel) or DRAW_BODY_ONLY.get(rel)
        if not is_draw:
            bound = sorted(set(re.findall(r"shape:\s*([a-z-]+)", body)))
            shapes = bound
        draw += is_draw
        blocked += (not is_draw)
        site, detail = WITNESS[kind] if not is_draw else ("", "")
        narrow = "drawn" if is_draw and rel in NARROW_DRAW else "source"
        original_wide = "drawn" if rel in DRAW_BOTH else "source"
        original_narrow = "drawn" if rel in DRAW_BOTH and rel in NARROW_DRAW else "source"
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / rel),
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw" if is_draw else "predicted-blocked",
            "track_scope": "body-only" if body_only else "body-and-original",
            "expected_states": {
                "wide_240": original_wide,
                "narrow_40": original_narrow,
                "restored_240": original_wide,
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
                "FS-5 G5 wave-document contour admitted" if is_draw
                else "Blocked by compound exotic-shape/theme debt"),
        })
    if draw != 5 or blocked != 16:
        raise ValueError(f"partition drift: draw={draw} blocked={blocked}")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-5-g5-wave-documents",
        "shape_projection": {
            "wave_bottom": "one centred one-cell trough on the baseline "
                           "with a horizontal floor one row below",
            "group": [
                {"shape": "document", "aliases": ["doc", "document"],
                 "contour": "rectangle with a sine-wave bottom edge"},
                {"shape": "stacked document",
                 "aliases": ["docs", "documents", "st-doc",
                             "stacked-document"],
                 "contour": "stacked back-sheet peek (one row top, one "
                            "column right) with a wave bottom on the front "
                            "chamber; the back right wall extends down to the "
                            "trough floor depth, so its corner sits one row "
                            "below the front baseline (matches the upstream "
                            "multiWaveEdgedRectangle outer path, whose right "
                            "edge spans the wave amplitude)"},
                {"shape": "lined document",
                 "aliases": ["lin-doc", "lined-document"],
                 "contour": "wave document with an interior left binding rule"},
                {"shape": "tagged document",
                 "aliases": ["tag-doc", "tagged-document"],
                 "contour": "wave document with a small right-pointing side tag"},
            ],
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original binding a wave-document "
                    "shape alias",
            "roots": ["flowchart"],
            "draw_authority": "the four alias smoke fixtures (20/33/29/38) "
                              "plus the document-shape-label-clearance body",
            "width_policy": "draw rows paint body at 240 and restored "
                           "(originals only for the alias fixtures); "
                           "narrow_40 is per-row.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(fresh general-purpose agent, D-066 FS-5 G5 "
                              "wave documents, 2026-11-07)",
            "date": "2026-11-07",
            "verdict": "approved",
            "note": ("Independent read-only adversarial review (fresh "
                     "general-purpose agent, 2026-11-07): initially REQUEST "
                     "CHANGES. MAJOR fixed before approval: on even-width "
                     "document/lined-document boxes the bottom port column "
                     "(width/2) sat one column right of the trough centre "
                     "((width-1)/2), so a bottom-entering edge arrow stopped "
                     "on a blocked blank; ports now use the trough axis, "
                     "pinned by an even-width BT pixel-column regression "
                     "(mutation-verified red). MINOR resolved by decision: "
                     "the stacked document back wall deliberately extends to "
                     "trough-floor depth with its corner one row below the "
                     "front baseline, matching the upstream outer path; "
                     "pinned in this contour record. NITs applied (braces, "
                     "blocked-branch comment)."),
        },
        "counts": {
            "total": draw + blocked,
            "predicted_draw": draw,
            "predicted_blocked": blocked,
            "predicted_original_draw": 4,
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
