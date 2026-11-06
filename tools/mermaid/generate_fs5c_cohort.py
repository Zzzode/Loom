#!/usr/bin/env python3
"""Generate the RFC 0031 FS-5c flipped-triangle shape cohort ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/flowchart root that
    - carries a column-0 YAML frontmatter envelope,
    - whose body binds a node to a flipped-triangle (manual-file) shape via
      one of the three aliases: flip-tri, manual-file, flipped-triangle.

FS-5c adds the flipped-triangle contour (the vertical mirror of the
extract triangle: a flat edge on top, the apex at the bottom centre;
upstream packages/mermaid/.../shapes/flippedTriangle.ts at the pinned
commit gives the polygon). Exactly one selected body draws: the
three-node alias smoke fixture (31), which has no other debt and paints
at 240 wide + restored but not at narrow_40. The four shape-themes
"connect-all-shapes" fixtures bind the shape among dozens of other
exotic contours and theme config, so they stay on ordinary source;
they are recorded blocked with a compound blocker so the ledger's
draw/block partition is zero-residual.

Usage: python3 tools/mermaid/generate_fs5c_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs5c-cohort.json")

SCHEMA = "loom_mermaid_fs5c_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-flip-tri-compound",
]

WITNESS = {
    "flow-flip-tri-compound": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a flipped-triangle node among many other exotic "
        "shape aliases (and theme config) whose contours the terminal does "
        "not project; the whole source stays on ordinary fallback until "
        "those shapes are admitted.",
    ),
}

FLIP_TRI_RE = re.compile(
    r"shape:\s*(?:flip-tri|manual-file|flipped-triangle)\b")

# The one fixture that draws: a bare frontmatter and only the three
# flipped-triangle aliases.
DRAW_FIXTURE = (
    "flowchart/nightly-check/shape-alias/"
    "31-all-flip-tri-manual-file-flipped-triangle-should-render-same-shape.mmd")


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
        if not FLIP_TRI_RE.search(body):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        is_draw = rel == DRAW_FIXTURE
        if is_draw and "theme" in frontmatter:
            raise ValueError(f"draw fixture unexpectedly carries theme: {rel}")
        draw += is_draw
        blocked += (not is_draw)
        kind = None if is_draw else "flow-flip-tri-compound"
        site, detail = WITNESS[kind] if blocked else ("", "")
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / rel),
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw" if is_draw else "predicted-blocked",
            "expected_states": {
                "wide_240": "drawn" if is_draw else "source",
                "narrow_40": "source",
                "restored_240": "drawn" if is_draw else "source",
            },
            "expected_body_states": {
                "wide_240": "drawn" if is_draw else "source",
                "narrow_40": "source",
                "restored_240": "drawn" if is_draw else "source",
            },
            "shapes": ["flip-tri", "manual-file", "flipped-triangle"],
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if blocked else None),
            "ledger_note": (
                "FS-5c flipped-triangle contour admitted" if is_draw else
                "Blocked by compound exotic-shape/theme debt around the "
                "flipped-triangle node"),
        })
    if draw != 1 or blocked != 4:
        raise ValueError(f"partition drift: draw={draw} blocked={blocked}")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-5c-flipped-triangle-shape",
        "shape_projection": {
            "aliases": ["flip-tri", "manual-file", "flipped-triangle"],
            "contour": "vertical mirror of the extract triangle: flat edge "
                        "on top, apex at the bottom centre",
            "upstream_witness": "packages/mermaid/src/rendering-util/"
                                "rendering-elements/shapes/flippedTriangle.ts "
                                "at mermaid@12.2.0 (1fb78698)",
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original that binds a node to a "
                    "flipped-triangle (manual-file) shape (any of three "
                    "aliases)",
            "roots": ["flowchart"],
            "draw_authority": "the single alias smoke fixture with no other "
                              "shape/theme debt; the four shape-themes rows "
                              "carry compound exotic-shape debt",
            "width_policy": "the draw row paints body+original at 240 and "
                           "restored; it keeps ordinary source at narrow_40.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(fresh general-purpose agent, D-066 FS-5c "
                              "flipped-triangle, 2026-11-07)",
            "date": "2026-11-07",
            "verdict": "approved",
            "note": "Read-only adversarial review (zero builds; separate "
                    "fresh-context agent) of the new contour against the "
                    "upstream flippedTriangle.ts polygon (flat edge on top, "
                    "apex at the bottom centre), the three-alias fail-closed "
                    "vocabulary (upward triangle identity retained), the "
                    "exhaustive Shape switches/branches, the five-frame "
                    "Flow400 admission wiring parity with FS-5b, and the "
                    "zero-residual 1-draw/4-blocked partition. APPROVE: no "
                    "MAJOR/MINOR; additive contour, no Work/shared-constant/"
                    "router/gaps change, gaps stay 48, one original moves "
                    "pending 282->281. One NIT applied pre-commit: the ASCII "
                    "geometry glyph table maps the new apex glyph to 'v' for "
                    "symmetry (latent path; the ASCII profile rejects "
                    "non-rectangle shapes).",
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
