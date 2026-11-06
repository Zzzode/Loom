#!/usr/bin/env python3
"""Generate the RFC 0031 FS-6 flowchart FontAwesome-icon body cohort ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/flowchart root that
    - carries a column-0 YAML frontmatter envelope (so the whole original
      stays on ordinary source fallback through the envelope gate),
    - whose text carries a FontAwesome marker (fa:fa-<name>, with an
      optional single-letter style prefix fas/fab/fal/far/fak).

FS-6 projects a recognized marker to one fixed terminal marker cell
(U+25A0) because the terminal has no icon font, retaining the human
label text. The admitted icon vocabulary is the closed set {car, code}.
A selected row draws iff it has no marker outside that vocabulary AND no
independent whole-source blocker. In the pinned corpus the other
blockers are exactly two closed detectors, giving a 34 draw / 20
blocked partition with zero residual:
  * flow-fa-unknown-icon: a marker names an icon other than car/code
    (bell, coffee, github, cogs, server, database, ...).
  * flow-fa-theme-config: the frontmatter declares a `theme:` key or the
    body carries an `%%{init ...}%%` directive (theme/init visual
    payloads are Track A / later cohorts).

Per-width states are pinned path sets: the narrow 40-column frame draws
only for the simple graphs that fit it; the durable cohort gtest
enforces every state live, so a geometry or vocabulary drift fails
loudly even though the generator itself never renders.

Draw bodies paint on the body track at 240 (wide + restored); 17 of the
34 rows also draw the whole original at 240 (their frontmatter is
admitted); the other 17 keep whole-original source. The handDrawn-look
rows keep whole-original source with FS-8.

Usage: python3 tools/mermaid/generate_fs6_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs6-cohort.json")

SCHEMA = "loom_mermaid_fs6_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-fa-unknown-icon",
    "flow-fa-theme-config",
]

WITNESS = {
    "flow-fa-unknown-icon": (
        "src/ui/visual/mermaid_impl.cpp (label() closed icon vocabulary)",
        "The body binds an icon outside the projected {car, code} set "
        "(bell, coffee, github, twitter, facebook, cogs, circle, server, "
        "database, creative-commons); the terminal has no glyph for it.",
    ),
    "flow-fa-theme-config": (
        "src/ui/visual/mermaid_impl.cpp (flow envelope profile)",
        "The source declares a frontmatter `theme:` key or an in-body "
        "%%{init ...}%% directive; themed visual payloads are not admitted "
        "by the flow envelope profile (Track A / a later cohort).",
    ),
}

MARKER_RE = re.compile(r"fa[bklrs]?:fa-([A-Za-z0-9-]+)")
ADMITTED_ICONS = {"car", "code"}

# Pinned per-row width/frontmatter geometry. These facts are not derivable
# from source bytes without rendering, so they are pinned here and enforced
# bidirectionally by tests/ui/test_mermaid_fs6_cohort.cpp (every pinned
# state must match the live renderer at all three widths); a fixture drift
# fails --check and/or the gtest.
ORIGINAL_DRAW = {
    "flowchart/nightly-check/classic/01-38-should-render-a-flowchart-when-usemaxwidth-is-true-default.mmd",
    "flowchart/nightly-check/classic/02-39-should-render-a-flowchart-when-usemaxwidth-is-false.mmd",
    "flowchart/nightly-check/dagre/02-1-dagre-should-render-a-simple-flowchart.mmd",
    "flowchart/nightly-check/dagre/03-1-dagre-should-render-a-simple-flowchart.mmd",
    "flowchart/nightly-check/dagre/2-dagre-should-render-a-simple-flowchart-with-diagrampadding-set-to-0.mmd",
    "flowchart/nightly-check/dagre/6088-1-should-handle-diamond-shape-intersections.mmd",
    "flowchart/nightly-check/dagre/6088-3-should-handle-diamond-shape-intersections.mmd",
    "flowchart/nightly-check/dagre/61-dagre-fontawesome-icons-in-edge-labels.mmd",
    "flowchart/nightly-check/v2/01-7-should-render-a-flowchart-when-usemaxwidth-is-true-default.mmd",
    "flowchart/nightly-check/v2/02-8-should-render-a-flowchart-when-usemaxwidth-is-false.mmd",
    "flowchart/visual-check/1-should-render-a-simple-flowchart-no-htmllabels.mmd",
    "flowchart/visual-check/1-should-render-a-simple-flowchart.mmd",
    "flowchart/visual-check/15-should-render-a-simple-flowchart-with-comments.mmd",
    "flowchart/visual-check/2-should-render-a-simple-flowchart-with-diagrampadding-set-to-0.mmd",
    "flowchart/visual-check/33-should-render-a-simple-flowchart-with-diagrampadding-set-to-0.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels-2.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels.mmd",
}
BODY_NARROW_DRAW = {
    "flowchart/nightly-check/dagre/61-dagre-fontawesome-icons-in-edge-labels.mmd",
    "flowchart/nightly-check/handdrawn/fontawesome-icons-in-edge-labels.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels-2.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels.mmd",
}
ORIGINAL_NARROW_DRAW = {
    "flowchart/nightly-check/dagre/61-dagre-fontawesome-icons-in-edge-labels.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels-2.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels.mmd",
}


def split_source(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None


def blocker(frontmatter, body):
    icons = set(MARKER_RE.findall(body))
    if icons - ADMITTED_ICONS:
        return "flow-fa-unknown-icon"
    if re.search(r"^\s*theme:", frontmatter, re.M) or "%%{" in body:
        return "flow-fa-theme-config"
    return None


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    entries = []
    for path in sorted((repo_root / CASES / "flowchart").rglob("*.mmd")):
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        frontmatter, body = split
        if not MARKER_RE.search(text):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        kind = blocker(frontmatter, body)
        blocked = kind is not None
        original_draws = (not blocked) and rel in ORIGINAL_DRAW
        # Geometry pin consistency checks: a narrow-draw path must draw wide.
        if rel in BODY_NARROW_DRAW and blocked:
            raise ValueError(f"body narrow-draw path is blocked: {rel}")
        if rel in ORIGINAL_NARROW_DRAW and not original_draws:
            raise ValueError(f"original narrow-draw path is not an original draw: {rel}")
        entries.append((rel, blocked, original_draws, kind))
    # The pinned draw sets must be subsets of the selection, and the closed
    # detectors must reproduce the partition exactly (zero residual).
    selected = {rel for rel, _, _, _ in entries}
    if not ORIGINAL_DRAW <= selected:
        raise ValueError("pinned original-draw path missing from selection")
    draw_paths = {rel for rel, blocked, _, _ in entries if not blocked}
    if not draw_paths:
        raise ValueError("no draw rows")
    # Every pinned original-draw row must be a mechanical draw row.
    if not ORIGINAL_DRAW <= draw_paths:
        raise ValueError("pinned original-draw row is mechanically blocked")
    # Every mechanical draw row not pinned as an original draw must keep whole
    # source (its frontmatter is not admitted): enforced by count below.
    rows = []
    n_draw = n_blocked = n_original_draw = 0
    for rel, blocked, original_draws, kind in entries:
        n_blocked += blocked
        n_draw += not blocked
        n_original_draw += original_draws
        site, detail = WITNESS[kind] if blocked else ("", "")
        manifest_row = manifest_rows[rel]
        body_narrow = (not blocked) and rel in BODY_NARROW_DRAW
        original_narrow = original_draws and rel in ORIGINAL_NARROW_DRAW
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / rel),
            "manifest_sha256": manifest_row["sha256"],
            "prior_status": manifest_row["raw"],
            "prediction": "predicted-blocked" if blocked else "predicted-draw",
            "expected_states": {
                "wide_240": "drawn" if original_draws else "source",
                "narrow_40": "drawn" if original_narrow else "source",
                "restored_240": "drawn" if original_draws else "source",
            },
            "expected_body_states": {
                "wide_240": "source" if blocked else "drawn",
                "narrow_40": "drawn" if body_narrow else "source",
                "restored_240": "source" if blocked else "drawn",
            },
            "icons": sorted(set(MARKER_RE.findall((repo_root / CASES / rel).read_text()))),
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if blocked else None),
            "ledger_note": (
                "FS-6 FontAwesome marker projected to a fixed terminal cell on "
                "the body track"
                if not blocked else
                "Blocked by a closed source-content feature detector; the "
                "marker vocabulary itself is only admitted for car/code"),
        })
    if n_draw != 34 or n_blocked != 20 or n_original_draw != 17:
        raise ValueError(
            f"partition drift: draw={n_draw} blocked={n_blocked} "
            f"original_draw={n_original_draw}")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-6-fontawesome-icons",
        "icon_projection": {
            "marker": "U+25A0 BLACK SQUARE",
            "admitted_icons": sorted(ADMITTED_ICONS),
            "style_prefixes": ["fa", "fas", "fab", "fal", "far", "fak"],
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original whose text binds a "
                    "FontAwesome fa:fa-<name> marker",
            "roots": ["flowchart"],
            "blocker_detection": "two fixed source-content detectors "
                                 "(unknown icon, theme/init config); no "
                                 "generator-time rendering; residual rows abort",
            "width_policy": "draw bodies paint at 240; the pinned simple "
                           "graphs also paint at narrow_40; whole originals "
                           "paint only for the 17 admitted-frontmatter rows.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(D-066 FS-6 fontawesome icons, 2026-11-06)",
            "date": "2026-11-06",
            "verdict": "approved",
            "note": "Read-only adversarial review of the closed {car, code} "
                    "icon projection (fixed marker cell, retained human "
                    "text, unknown markers fail closed), the two-detector "
                    "zero-residual selection, the pinned per-width geometry "
                    "sets, and the body/original matrix wiring. APPROVE: "
                    "label-only blast radius, no shared constants, gaps stay "
                    "48, 17 originals move pending 309->292.",
        },
        "counts": {
            "total": n_draw + n_blocked,
            "predicted_draw": n_draw,
            "predicted_blocked": n_blocked,
            "predicted_original_draw": n_original_draw,
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
