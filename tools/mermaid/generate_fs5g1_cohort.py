#!/usr/bin/env python3
"""Generate the RFC 0031 FS-5 G1 card+folder shape cohort ledger.

First GROUPED FS-5 batch (two geometrically related rectilinear contours
share one ledger, matrix run, review, and commit): the notched-rectangle
card (aliases card, notch-rect, notched-rectangle; top-left corner cut by
a one-cell diagonal) and the folder/directory (aliases folder, directory;
a body rectangle with a top-left tab one row tall).

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/flowchart root that
    - carries a column-0 YAML frontmatter envelope,
    - whose body binds a node to a card OR folder shape via any of the
      five aliases (regex below).

Two selected bodies draw: the card alias smoke fixture (13) and the
folder alias smoke fixture (42), both bare frontmatter with no other
debt; they paint body+original at 240 wide and restored. At narrow_40 the
folder fixture also paints native (short labels, two nodes fit), while
the card fixture keeps ordinary source (its 17-char label needs a
21-cell box). The four shape-themes "connect-all-shapes" fixtures
(06/16/26/36) bind card together with the still-
unmapped lin-rect/text shapes and theme config, so they stay on
ordinary source (blocker flow-card-folder-compound), zero residual.

Usage: python3 tools/mermaid/generate_fs5g1_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs5g1-cohort.json")

SCHEMA = "loom_mermaid_fs5g1_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-card-folder-compound",
]

WITNESS = {
    "flow-card-folder-compound": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a card/folder node among other exotic shape "
        "aliases (lin-rect/lined-rectangle, text) whose contours the "
        "terminal does not project (and theme config); the whole source "
        "stays on ordinary fallback until those shapes are admitted.",
    ),
}

GROUP_RE = re.compile(
    r"shape:\s*(?:card|notch-rect|notched-rectangle|folder|directory)\b")

# The two fixtures that draw: bare frontmatter, only the group aliases.
DRAW_FIXTURES = {
    "flowchart/nightly-check/shape-alias/"
    "13-all-notched-rectangle-card-notch-rect-should-render-same-shape.mmd":
        ["notched-rectangle", "card", "notch-rect"],
    "flowchart/nightly-check/shape-alias/"
    "42-all-folder-directory-should-render-same-shape.mmd":
        ["folder", "directory"],
}

# Draw fixtures whose two short nodes also fit (and paint) at narrow_40.
# The card fixture carries a 17-char label (min box width 21, three nodes)
# and keeps ordinary source at narrow; the folder fixture (max label 9,
# two nodes) paints native even at 40 columns.
NARROW_DRAW = {
    "flowchart/nightly-check/shape-alias/"
    "42-all-folder-directory-should-render-same-shape.mmd",
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
        is_draw = rel in DRAW_FIXTURES
        if is_draw and "theme" in frontmatter:
            raise ValueError(f"draw fixture unexpectedly carries theme: {rel}")
        draw += is_draw
        blocked += (not is_draw)
        kind = None if is_draw else "flow-card-folder-compound"
        site, detail = WITNESS[kind] if blocked else ("", "")
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
            "shapes": DRAW_FIXTURES.get(rel, ["card", "notch-rect",
                                             "notched-rectangle"]),
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if blocked else None),
            "ledger_note": (
                "FS-5 G1 card/folder contour admitted" if is_draw else
                "Blocked by compound exotic-shape/theme debt around the "
                "card/folder node"),
        })
    if draw != 2 or blocked != 4:
        raise ValueError(f"partition drift: draw={draw} blocked={blocked}")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-5-g1-card-folder-shapes",
        "shape_projection": {
            "group": [
                {"shape": "notched-rectangle card",
                 "aliases": ["card", "notch-rect", "notched-rectangle"],
                 "contour": "rectangle with the top-left corner cut by a "
                            "one-cell diagonal"},
                {"shape": "folder/directory",
                 "aliases": ["folder", "directory"],
                 "contour": "body rectangle with a top-left tab one row "
                            "tall (~0.38 width)"},
            ],
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original that binds a node to a "
                    "card (card/notch-rect/notched-rectangle) or folder "
                    "(folder/directory) shape",
            "roots": ["flowchart"],
            "draw_authority": "the two alias smoke fixtures with no other "
                              "shape/theme debt; the four shape-themes rows "
                              "carry compound exotic-shape debt "
                              "(lin-rect, text) and theme config",
            "width_policy": "draw rows paint body+original at 240 and "
                           "restored; narrow_40 is per-row - the folder row "
                           "draws (fits), the card row keeps ordinary source.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(fresh general-purpose agent, D-066 FS-5 G1 "
                              "card+folder grouped batch, 2026-11-07)",
            "date": "2026-11-07",
            "verdict": "approved",
            "note": "Read-only adversarial review (zero builds; separate "
                    "fresh-context agent) of both contours against the "
                    "upstream card.ts/folder.ts polygons, the five-alias "
                    "fail-closed vocabulary, generic classic sizing, and the "
                    "zero-residual 2-draw/4-blocked partition with its "
                    "per-row narrow split (folder draws at narrow_40, card "
                    "keeps source). First verdict REQUEST-CHANGES with two "
                    "MAJORs invisible on the edgeless alias smoke fixtures: "
                    "the folder top port outside cell was a blocked "
                    "bbox-interior blank (router dropped it) and folder had "
                    "no left/right ports; fixed by unblocking the single "
                    "above-body-edge centre cell in classic_port_corridors "
                    "(verified strictly right of the tab for all widths) and "
                    "giving folder the normal four cardinal ports, plus TB/LR "
                    "edge-attachment regression gtests. MINOR (narrow-policy "
                    "doc text) and a clamp-comment NIT also fixed; re-review "
                    "APPROVE. Additive contours, no Work/shared-constant/"
                    "router/gaps change, gaps stay 48, two originals move "
                    "pending 279->277.",
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
