#!/usr/bin/env python3
"""Generate the RFC 0031 FS-5 G2 cylinder-family cohort ledger.

Second GROUPED FS-5 batch: three storage/cylinder contours share one
ledger, matrix run, review, and commit.

- Horizontal cylinder (direct-access storage): aliases h-cyl, das,
  horizontal-cylinder. A cylinder on its side - straight top/bottom
  rules, shallow end caps on the middle row, a centre vertical far-rim
  line (upstream shapes/tiltedCylinder.ts).
- Lined cylinder (disk storage): aliases lin-cyl, disk,
  lined-cylinder. The horizontal database cylinder REUSED plus a stacked
  disk line on the first body row (upstream shapes/linedCylinder.ts).
- Datastore (data-flow data store): aliases datastore, data-store. An
  open rectangle with only the top and bottom rules, no side walls
  (upstream shapes/datastore.ts).

Selection (mechanical, byte-level): every frontmatter flowchart original
under cases/flowchart whose body binds any of the eight aliases. Three
alias smoke fixtures draw (22/23/41); eight shape-themes connect-all
fixtures stay source via independent exotic-shape/theme debt (h-cyl rows
also bind hourglass; lin-cyl rows also bind document/notched-
pentagon/stacked-document). There is no datastore compound row.

Usage: python3 tools/mermaid/generate_fs5g2_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs5g2-cohort.json")

SCHEMA = "loom_mermaid_fs5g2_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-hcyl-compound",
    "flow-lincyl-compound",
]

WITNESS = {
    "flow-hcyl-compound": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a horizontal-cylinder node among other exotic "
        "shape aliases (hourglass) whose contours the terminal does not "
        "project (and theme config); the whole source stays on ordinary "
        "fallback until those shapes are admitted.",
    ),
    "flow-lincyl-compound": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a lined-cylinder node among other exotic shape "
        "aliases (document, notched-pentagon, stacked-document) and theme "
        "config; the whole source stays on ordinary fallback until those "
        "shapes are admitted.",
    ),
}

GROUP_RE = re.compile(
    r"shape:\s*(?:h-cyl|das|horizontal-cylinder|lin-cyl|disk|lined-cylinder|"
    r"datastore|data-store)\b")
HCYL_RE = re.compile(r"shape:\s*(?:h-cyl|das|horizontal-cylinder)\b")
LINSYL_RE = re.compile(r"shape:\s*(?:lin-cyl|disk|lined-cylinder)\b")

DRAW_FIXTURES = {
    "flowchart/nightly-check/shape-alias/"
    "22-all-h-cyl-das-horizontal-cylinder-should-render-same-shape.mmd":
        (["h-cyl", "das", "horizontal-cylinder"], "hcyl"),
    "flowchart/nightly-check/shape-alias/"
    "23-all-lin-cyl-disk-lined-cylinder-should-render-same-shape.mmd":
        (["lin-cyl", "disk", "lined-cylinder"], "lincyl"),
    "flowchart/nightly-check/shape-alias/"
    "41-all-datastore-data-store-should-render-same-shape.mmd":
        (["datastore", "data-store"], "datastore"),
}

# Draw rows that also paint native at narrow_40 (per-row; empty unless the
# live renderer confirms a fit).
NARROW_DRAW = {
    "flowchart/nightly-check/shape-alias/"
    "41-all-datastore-data-store-should-render-same-shape.mmd",
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
        if is_draw:
            kind = None
            shapes, _ = DRAW_FIXTURES[rel]
        elif HCYL_RE.search(body):
            kind = "flow-hcyl-compound"
            shapes = ["h-cyl", "das", "horizontal-cylinder"]
        elif LINSYL_RE.search(body):
            kind = "flow-lincyl-compound"
            shapes = ["lin-cyl", "disk", "lined-cylinder"]
        else:
            # No datastore compound row exists in the pinned corpus; every
            # remaining blocked row binds a cylinder alias above.
            raise ValueError(f"unclassified blocked row: {rel}")
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
            "shapes": shapes,
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if blocked else None),
            "ledger_note": (
                "FS-5 G2 cylinder-family contour admitted" if is_draw else
                "Blocked by compound exotic-shape/theme debt"),
        })
    if draw != 3 or blocked != 8:
        raise ValueError(f"partition drift: draw={draw} blocked={blocked}")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-5-g2-cylinder-family",
        "shape_projection": {
            "group": [
                {"shape": "horizontal cylinder",
                 "aliases": ["h-cyl", "das", "horizontal-cylinder"]},
                {"shape": "lined cylinder",
                 "aliases": ["lin-cyl", "disk", "lined-cylinder"]},
                {"shape": "datastore",
                 "aliases": ["datastore", "data-store"]},
            ],
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original binding a cylinder-family "
                    "shape (h-cyl/das/horizontal-cylinder, lin-cyl/disk/"
                    "lined-cylinder, or datastore/data-store)",
            "roots": ["flowchart"],
            "draw_authority": "the three alias smoke fixtures (22/23/41); "
                              "eight shape-themes rows carry independent "
                              "exotic-shape/theme debt",
            "width_policy": "draw rows paint body+original at 240 and "
                           "restored; narrow_40 is per-row.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(fresh general-purpose agent, D-066 FS-5 G2 "
                              "cylinder family, 2026-11-07)",
            "date": "2026-11-07",
            "verdict": "approved",
            "note": "Read-only adversarial review (zero builds; separate "
                    "fresh-context agent) of all three contours against "
                    "tiltedCylinder.ts/linedCylinder.ts/datastore.ts: "
                    "horizontal cylinder (sideways caps + centre far-rim "
                    "line), lined cylinder (vertical database cylinder "
                    "reused plus a stacked-disk line one row above the "
                    "label), and the open datastore (two rules, no side "
                    "walls -> top/bottom-only ports, matching the "
                    "Triangle port invariant; sideways edges detour onto "
                    "a rule and still render). Eight-alias fail-closed "
                    "vocabulary (disk/das/data-store checked for "
                    "collisions), zero-residual 3-draw/8-blocked partition "
                    "with two blocker kinds and per-row narrow (only the "
                    "short-label datastore draws at narrow_40). APPROVE: "
                    "no MAJOR; pre-commit additions: a datastore LR edge "
                    "regression (MINOR) plus header/dead-branch/comment "
                    "NITs, all applied. Additive contours, no Work/shared-"
                    "constant/router/gaps change, gaps stay 48, three "
                    "originals move pending 277->274.",
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
