#!/usr/bin/env python3
"""Generate the RFC 0031 FS-5 G3 polygon-family cohort ledger.

Third GROUPED FS-5 batch: five polygon contours share one ledger, matrix
run, review, and commit.

- Notched pentagon (loop limit): notch-pent/loop-limit/notched-pentagon.
  A pentagon with an inset top edge and one-row shoulders.
- Hourglass (collate): collate/hourglass. Top/bottom edges swept to a
  single-row centre waist (top/bottom-only ports).
- Lightning bolt (com link): bolt/com-link/lightning-bolt. A zig-zag bolt.
- Bow tie (stored data): bow-rect/stored-data/bow-tie-rectangle. The
  hourglass contour at cell resolution (top/bottom-only ports).
- Comment curly brace: comment/brace-l. A box with a left curly brace.

Selection (mechanical, byte-level): every frontmatter flowchart original
under cases/flowchart whose body binds any of the 13 aliases. Five alias
smoke fixtures draw (30/40/19/36/17); twenty shape-themes connect-all
fixtures stay source via independent exotic-shape/neo-theme debt (the
hourglass themes bind only hourglass but are independently blocked by
look:"neo", which the parser rejects).

Usage: python3 tools/mermaid/generate_fs5g3_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs5g3-cohort.json")

SCHEMA = "loom_mermaid_fs5g3_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-g3-pent-compound",
    "flow-g3-bolt-compound",
    "flow-g3-bowtie-compound",
    "flow-g3-hourglass-neo",
    "flow-g3-brace-compound",
]

WITNESS = {
    "flow-g3-pent-compound": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a notched-pentagon node among other exotic shape "
        "aliases (document, stacked-document) and neo theme config; whole "
        "source stays on ordinary fallback until those are admitted.",
    ),
    "flow-g3-bolt-compound": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a bolt node among other exotic shape aliases "
        "(documents, filled-circle, tagged-rectangle) and neo theme config.",
    ),
    "flow-g3-bowtie-compound": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a bow-tie node among other exotic shape aliases "
        "(crossed-circle, curved-trapezoid, tagged-document) and neo theme.",
    ),
    "flow-g3-hourglass-neo": (
        "src/domain/frontmatter config grammar (look whitelist)",
        "The whole original is rejected because the frontmatter sets "
        "look/theme neo, which the parser rejects (only handDrawn is a "
        "whitelisted look and theme is not an indent-2 key); independently, "
        "the body track also fails closed because every shape-data label in "
        "these theme fixtures is single-quoted and flow_shape_data accepts "
        "only double quotes. Either gate alone keeps the source on ordinary "
        "fallback after hourglass is admitted.",
    ),
    "flow-g3-brace-compound": (
        "src/ui/visual/mermaid_impl.cpp (flow_shape_data vocabulary)",
        "The source binds a brace/comment node among other exotic shape "
        "aliases (brace-r, braces, lined/tagged-document) and neo theme.",
    ),
}

GROUP_RE = re.compile(
    r"shape:\s*(?:notch-pent|loop-limit|notched-pentagon|collate|hourglass|"
    r"bolt|com-link|lightning-bolt|bow-rect|stored-data|bow-tie-rectangle|"
    r"comment|brace-l)\b")

FAMILY = [
    ("pent", re.compile(r"shape:\s*(?:notch-pent|loop-limit|notched-pentagon)\b"),
     "flow-g3-pent-compound", ["notch-pent", "loop-limit", "notched-pentagon"]),
    ("hourglass", re.compile(r"shape:\s*(?:collate|hourglass)\b"),
     "flow-g3-hourglass-neo", ["collate", "hourglass"]),
    ("bolt", re.compile(r"shape:\s*(?:bolt|com-link|lightning-bolt)\b"),
     "flow-g3-bolt-compound", ["bolt", "com-link", "lightning-bolt"]),
    ("bowtie", re.compile(r"shape:\s*(?:bow-rect|stored-data|bow-tie-rectangle)\b"),
     "flow-g3-bowtie-compound", ["bow-rect", "stored-data", "bow-tie-rectangle"]),
    ("brace", re.compile(r"shape:\s*(?:comment|brace-l)\b"),
     "flow-g3-brace-compound", ["comment", "brace-l"]),
]

DRAW_FIXTURES = {
    "flowchart/nightly-check/shape-alias/"
    "30-all-notch-pent-loop-limit-notched-pentagon-should-render-same-shape.mmd":
        (["notch-pent", "loop-limit", "notched-pentagon"], "pent"),
    "flowchart/nightly-check/shape-alias/"
    "40-all-collate-hourglass-should-render-same-shape.mmd":
        (["collate", "hourglass"], "hourglass"),
    "flowchart/nightly-check/shape-alias/"
    "19-all-bolt-com-link-lightning-bolt-should-render-same-shape.mmd":
        (["bolt", "com-link", "lightning-bolt"], "bolt"),
    "flowchart/nightly-check/shape-alias/"
    "36-all-bow-rect-stored-data-bow-tie-rectangle-should-render-same-shape.mmd":
        (["bow-rect", "stored-data", "bow-tie-rectangle"], "bowtie"),
    "flowchart/nightly-check/shape-alias/"
    "17-all-comment-brace-l-should-render-same-shape.mmd":
        (["comment", "brace-l"], "brace"),
}
NARROW_DRAW = {
    "flowchart/nightly-check/shape-alias/"
    "17-all-comment-brace-l-should-render-same-shape.mmd",
    "flowchart/nightly-check/shape-alias/"
    "40-all-collate-hourglass-should-render-same-shape.mmd",
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
        shapes = None
        kind = None
        for _, rx, blocker, aliases in FAMILY:
            if rx.search(body):
                shapes = aliases
                kind = None if is_draw else blocker
                break
        if shapes is None:
            raise ValueError(f"unclassified row: {rel}")
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
                "FS-5 G3 polygon-family contour admitted" if is_draw else
                "Blocked by compound exotic-shape/neo-theme debt"),
        })
    if draw != 5 or blocked != 20:
        raise ValueError(f"partition drift: draw={draw} blocked={blocked}")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-5-g3-polygon-family",
        "shape_projection": {"aliases_bound_in_cohort": 13, "contours": 5},
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original binding a G3 polygon "
                    "shape (notched-pentagon/loop-limit, collate/hourglass, "
                    "bolt/com-link, bow-rect/stored-data, comment/brace-l)",
            "roots": ["flowchart"],
            "draw_authority": "the five alias smoke fixtures (30/40/19/36/"
                              "17); twenty shape-themes rows carry "
                              "independent exotic-shape or neo-look debt",
            "width_policy": "draw rows paint body+original at 240 and "
                           "restored; narrow_40 is per-row.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(fresh general-purpose agent, D-066 FS-5 G3 "
                              "polygon family, 2026-11-07)",
            "date": "2026-11-07",
            "verdict": "approved",
            "note": "Read-only adversarial review (zero builds; separate "
                    "fresh-context agent) of five stylized cell projections "
                    "against trapezoidalPentagon/hourglass/lightningBolt/"
                    "bowTieRect/curlyBraceLeft.ts: notched pentagon, "
                    "hourglass and bow tie (centre one-row waist, "
                    "top/bottom-only ports), lightning bolt zig-zag, and the "
                    "left curly-brace comment. 13-alias fail-closed "
                    "vocabulary, five blocker families, per-row narrow "
                    "(brace/hourglass draw at narrow_40). First verdict "
                    "REQUEST-CHANGES: MAJOR was the brace interior-rule ┤ "
                    "unconditionally overwritten by the right-wall loop (a "
                    "dead write on every render), MINOR a half-width bolt "
                    "jog for short labels and incomplete hourglass blocker "
                    "evidence; fixed by splitting the brace right wall to "
                    "skip the mid row, painting the bolt jog full width, "
                    "documenting both the neo-look and single-quote body "
                    "gates for the hourglass themes, and adding exact-glyph "
                    "regression gtests; re-review APPROVE. Additive "
                    "contours, no Work/shared-constant/router/gaps change, "
                    "gaps stay 48, five originals move pending 274->269.",
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
