#!/usr/bin/env python3
"""Generate the RFC 0031 FS-5 G6 circle-family cohort ledger.

Sixth GROUPED FS-5 batch: three circular flowchart contours share one
ledger, matrix run, review, and commit. All reuse the circular ring
silhouette (classic_ring).

- Filled circle (f-circ/junction/filled-circle; filledCircle.ts): a
  fixed-size solid disc with the label cleared upstream.
- Framed/stop circle (fr-circ/framed-circle/stop; stateEnd.ts): a UML
  final state - outer ring with a filled inner core (~10/14 diameter).
- Crossed circle (cross-circ/summary/crossed-circle; crossedCircle.ts):
  a fixed-size label-less ring with two crossing diameters (an inscribed
  X); the label is cleared upstream like the filled circle.

Selection (mechanical, byte-level): every frontmatter flowchart original
under cases/flowchart whose body binds any of the nine aliases. Three
alias smoke fixtures (16/28/37) draw on both tracks; twelve shape-themes
connect-all files stay source. After this batch the 02/12/22/32 and
07/18/27/37 groups bind only already-mapped shapes and are held solely by
neo look/theme config plus single-quoted shape-data labels; the
03/13/24/33 group still binds the unmapped curved trapezoid.

Usage: python3 tools/mermaid/generate_fs5g6_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs5g6-cohort.json")

SCHEMA = "loom_mermaid_fs5g6_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-g6-curved-trapezoid-debt",
    "flow-g6-neo-quoted-only",
]

WITNESS = {
    "flow-g6-curved-trapezoid-debt": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a crossed-circle node among the still-unmapped "
        "curved-trapezoid/display alias (and neo config); the whole source "
        "stays on ordinary fallback until that shape is admitted.",
    ),
    "flow-g6-neo-quoted-only": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "Every bound shape is mapped, but the whole original is rejected by "
        "neo look/theme config and the body independently by single-quoted "
        "shape-data labels (flow_shape_data accepts only double quotes); the "
        "source stays on ordinary fallback.",
    ),
}

GROUP_RE = re.compile(
    r"shape:\s*(?:fr-circ|framed-circle|stop|f-circ|junction|"
    r"filled-circle|cross-circ|summary|crossed-circle)\b")

DRAW_FIXTURES = {
    "flowchart/nightly-check/shape-alias/"
    "16-all-fr-circ-framed-circle-stop-should-render-same-shape.mmd":
        ["fr-circ", "framed-circle", "stop"],
    "flowchart/nightly-check/shape-alias/"
    "28-all-f-circ-junction-filled-circle-should-render-same-shape.mmd":
        ["f-circ", "junction", "filled-circle"],
    "flowchart/nightly-check/shape-alias/"
    "37-all-cross-circ-summary-crossed-circle-should-render-same-shape.mmd":
        ["cross-circ", "summary", "crossed-circle"],
}

# Filled after observing the renderer; the durable cohort gate pins each.
# Only fixture 28 (fixed-size nine-cell discs, independent of its labels)
# fits a 40-column canvas; the labelled framed/crossed circles do not.
NARROW_DRAW = {
    "flowchart/nightly-check/shape-alias/"
    "28-all-f-circ-junction-filled-circle-should-render-same-shape.mmd",
    "flowchart/nightly-check/shape-alias/"
    "37-all-cross-circ-summary-crossed-circle-should-render-same-shape.mmd",
}

BLOCKED_KIND = {
    "02": "flow-g6-neo-quoted-only",
    "12": "flow-g6-neo-quoted-only",
    "22": "flow-g6-neo-quoted-only",
    "32": "flow-g6-neo-quoted-only",
    "07": "flow-g6-neo-quoted-only",
    "18": "flow-g6-neo-quoted-only",
    "27": "flow-g6-neo-quoted-only",
    "37": "flow-g6-neo-quoted-only",
    "03": "flow-g6-curved-trapezoid-debt",
    "13": "flow-g6-curved-trapezoid-debt",
    "24": "flow-g6-curved-trapezoid-debt",
    "33": "flow-g6-curved-trapezoid-debt",
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
        _, body = split
        if not GROUP_RE.search(body):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        is_draw = rel in DRAW_FIXTURES
        prefix = path.name.split("-", 1)[0]
        if not is_draw and prefix not in BLOCKED_KIND:
            raise ValueError(f"unclassified G6 row: {rel}")
        kind = None if is_draw else BLOCKED_KIND[prefix]
        if not is_draw:
            shapes = sorted(set(re.findall(r"shape:\s*([a-z-]+)", body)))
        else:
            shapes = DRAW_FIXTURES[rel]
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
                "FS-5 G6 circle-family contour admitted" if is_draw
                else "Blocked by remaining exotic-shape/theme debt"),
        })
    if draw != 3 or blocked != 12:
        raise ValueError(f"partition drift: draw={draw} blocked={blocked}")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-5-g6-circle-family",
        "shape_projection": {
            "group": [
                {"shape": "filled circle",
                 "aliases": ["f-circ", "junction", "filled-circle"],
                 "contour": "fixed-size solid ring-silhouette disc, no label"},
                {"shape": "framed circle",
                 "aliases": ["fr-circ", "framed-circle", "stop"],
                 "contour": "outer ring with a filled ~10/14 inner core"},
                {"shape": "crossed circle",
                 "aliases": ["cross-circ", "summary", "crossed-circle"],
                 "contour": "fixed-size label-less ring with two crossing "
                            "diameters (inscribed X); label cleared "
                            "upstream, as for the filled circle"},
            ],
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original binding a circle-family "
                    "shape alias",
            "roots": ["flowchart"],
            "draw_authority": "the three alias smoke fixtures (16/28/37)",
            "width_policy": "draw rows paint body+original at 240 and "
                           "restored; narrow_40 is per-row.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(fresh general-purpose agent, D-066 FS-5 G6 "
                              "circle family, 2026-11-07)",
            "date": "2026-11-07",
            "verdict": "approved",
            "note": ("Independent read-only adversarial review (fresh "
                     "general-purpose agent, 2026-11-07): initially REQUEST "
                     "CHANGES. Addressed before approval: the crossed circle "
                     "is now the fixed-size label-less glyph upstream "
                     "specifies (its node.label is cleared, like the filled "
                     "circle), not a label-sized ring; the inscribed X is "
                     "built by mirroring the top half so it stays symmetric "
                     "for every odd diameter (the prior linear interpolation "
                     "biased D == 5 mod 8), pinned by a top/bottom pixel "
                     "symmetry guard; the X-endpoint extension to the "
                     "straight wall cells is documented as the terminal 2:1 "
                     "cell-aspect convention. The framed circle geometry "
                     "and label retention were verified exactly faithful to "
                     "stateEnd.ts."),
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
