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
    "flow-g6-classic-glyph-budget",
]

WITNESS = {
    "flow-g6-classic-glyph-budget": (
        "src/ui/visual/mermaid_impl.cpp (classic glyph router/Work budget)",
        "Every bound shape (including curved-trapezoid, mapped in FS-5 G7) "
        "and the single-quoted label are now accepted, but five heavy classic "
        "glyph contours fully interconnected in one graph exceed the bounded "
        "classic-shape routing/Work ceiling: the low-port glyphs (triangle, "
        "hourglass, bow-tie, datastore expose only two ports) cannot route the "
        "ten edges without crossing. This is quote-independent (the same "
        "graph fails with double quotes); the body stays on ordinary fallback "
        "pending the budget recalibration, and the neo frontmatter keeps the "
        "whole original source regardless.",
    ),
}

# FS-sq follow-up: single-quoted shape-data labels are admitted, so the
# four-node connect-all groups now draw their BODY. Their neo look/theme
# frontmatter still rejects the whole original (no admission profile), so
# they are body-only rows. The five-node groups remain blocked by the
# classic-glyph routing budget.
BODY_ONLY_PREFIXES = {"07", "18", "27", "37"}

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
    "02": "flow-g6-classic-glyph-budget",
    "12": "flow-g6-classic-glyph-budget",
    "22": "flow-g6-classic-glyph-budget",
    "32": "flow-g6-classic-glyph-budget",
    "03": "flow-g6-classic-glyph-budget",
    "13": "flow-g6-classic-glyph-budget",
    "24": "flow-g6-classic-glyph-budget",
    "33": "flow-g6-classic-glyph-budget",
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
    whole_draw = body_only = blocked = 0
    for path in sorted((repo_root / CASES / "flowchart").rglob("*.mmd")):
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        _, body = split
        if not GROUP_RE.search(body):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        is_whole_draw = rel in DRAW_FIXTURES
        prefix = path.name.split("-", 1)[0]
        is_body_only = (not is_whole_draw) and prefix in BODY_ONLY_PREFIXES
        if not is_whole_draw and not is_body_only and prefix not in BLOCKED_KIND:
            raise ValueError(f"unclassified G6 row: {rel}")
        kind = None if (is_whole_draw or is_body_only) else BLOCKED_KIND[prefix]
        if is_whole_draw:
            shapes = DRAW_FIXTURES[rel]
        else:
            shapes = sorted(set(re.findall(r"shape:\s*([a-z-]+)", body)))
        whole_draw += is_whole_draw
        body_only += is_body_only
        blocked += (not is_whole_draw and not is_body_only)
        site, detail = WITNESS[kind] if kind else ("", "")
        narrow = "drawn" if is_whole_draw and rel in NARROW_DRAW else "source"
        # Body-only rows draw the body at every measured width but keep the
        # whole original source behind the neo frontmatter gate.
        body_narrow = "drawn" if is_body_only else narrow
        body_wide = "drawn" if (is_whole_draw or is_body_only) else "source"
        whole_wide = "drawn" if is_whole_draw else "source"
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / rel),
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": (
                "predicted-draw" if (is_whole_draw or is_body_only)
                else "predicted-blocked"),
            "track_scope": "body-only" if is_body_only else "whole-and-body",
            "expected_states": {
                "wide_240": whole_wide,
                "narrow_40": narrow,
                "restored_240": whole_wide,
            },
            "expected_body_states": {
                "wide_240": body_wide,
                "narrow_40": body_narrow,
                "restored_240": body_wide,
            },
            "shapes": shapes,
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if kind else None),
            "ledger_note": (
                "FS-5 G6 circle-family contour admitted" if is_whole_draw
                else "FS-sq single-quoted label admitted; body draws, whole "
                     "original stays neo-gated (body-only)" if is_body_only
                else "Blocked by remaining exotic-shape/budget debt"),
        })
    if whole_draw != 3 or body_only != 4 or blocked != 8:
        raise ValueError(
            f"partition drift: whole_draw={whole_draw} body_only={body_only} "
            f"blocked={blocked}")
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
                              "circle family, 2026-11-07; FS-sq single-quote "
                              "follow-up re-reviewed 2026-11-08, task "
                              "ad678f3037149e883)",
            "date": "2026-11-08",
            "verdict": "approved",
            "note": ("Original 2026-11-07 review approved the circle-family "
                     "geometry (filled/framed/crossed circles), including the "
                     "crossed-circle label-less glyph, odd-diameter X "
                     "symmetry, and framed-circle faithfulness. The 2026-11-08 "
                     "FS-sq review (independent read-only adversarial agent, "
                     "APPROVE) verified the single-quoted shape-data label "
                     "admission and this ledger migration: the four "
                     "07/18/27/37 connect-all rows move from blocked to "
                     "body-only draw (whole original stays neo-gated), and "
                     "all eight remaining blocked rows carry the "
                     "quote-independent classic-glyph routing-budget blocker "
                     "(the curved-trapezoid alias was already mapped in G7). "
                     "Its two MINOR findings are resolved: the helper comment "
                     "no longer claims byte-identical double-quote trimming, "
                     "and the stale curved-trapezoid blocker vocabulary is "
                     "retired."),
        },
        "counts": {
            "total": whole_draw + body_only + blocked,
            "predicted_draw": whole_draw + body_only,
            "predicted_blocked": blocked,
            "predicted_original_draw": whole_draw,
            "predicted_body_only_draw": body_only,
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
