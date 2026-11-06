#!/usr/bin/env python3
"""Generate the RFC 0031 FS-4h inline-HTML flowchart label cohort ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/flowchart root that
    - carries a column-0 YAML frontmatter envelope,
    - whose body contains an inline HTML emphasis/image tag in a node label
      (<strong> or <img>; <br> line breaks were already admitted).

FS-4h projects a closed inline-HTML subset in quoted/unquoted labels:
<strong>/<b> wrappers are stripped and their inner text retained; void
<img ...> elements are consumed. Other tags fail closed. Of the pinned
selection, the <img> fixtures still keep whole source because the node
statement scanner cannot delimit an unquoted bracket label that carries
embedded double quotes (4023) or an image-only node (4439); that scanner
limit is one flow-html-image-label blocker.

Every draw body paints at all three frames (240 wide, 40 narrow and the
restored wide) because these are short single/two-edge graphs; five of
the eight also admit the whole original at every frame. The three
remaining draw bodies keep whole-original source through their
frontmatter (one dagre profile, two handDrawn-look rows).

Usage: python3 tools/mermaid/generate_fs4h_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs4h-cohort.json")

SCHEMA = "loom_mermaid_fs4h_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-html-image-label",
]

WITNESS = {
    "flow-html-image-label": (
        "src/ui/visual/mermaid_impl.cpp (flow node statement scanner)",
        "The label embeds an <img> element; the node scanner cannot delimit "
        "an unquoted bracket label carrying the attribute's embedded quotes "
        "(or an image-only node has no retained text), so the whole source "
        "stays on ordinary fallback even though the label-level <img> "
        "projection itself is admitted.",
    ),
}

STRONG_OR_IMG_RE = re.compile(r"<\s*(?:strong|img)\b", re.IGNORECASE)
IMG_RE = re.compile(r"<\s*img\b", re.IGNORECASE)

# The five draw rows whose whole original is admitted at every frame. The
# other three draw bodies keep whole-original source through their
# frontmatter (dagre compound profile / handDrawn look).
ORIGINAL_DRAW = {
    "flowchart/nightly-check/dagre/6-dagre-should-render-non-escaped-with-html-labels.mmd",
    "flowchart/visual-check/36-should-render-escaped-without-html-labels.mmd",
    "flowchart/visual-check/37-should-render-non-escaped-with-html-labels.mmd",
    "flowchart/visual-check/5-should-render-escaped-without-html-labels.mmd",
    "flowchart/visual-check/6-should-render-non-escaped-with-html-labels.mmd",
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
    entries = []
    for path in sorted((repo_root / CASES / "flowchart").rglob("*.mmd")):
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        frontmatter, body = split
        if not STRONG_OR_IMG_RE.search(body):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        blocked = bool(IMG_RE.search(body))
        original_draws = (not blocked) and rel in ORIGINAL_DRAW
        if not blocked and rel not in ORIGINAL_DRAW:
            # Every other draw body must be a documented frontmatter hold.
            if "handDrawn" not in frontmatter and "dagre" not in frontmatter:
                raise ValueError(f"undocumented non-original draw row: {rel}")
        entries.append((rel, blocked, original_draws))
    if not ORIGINAL_DRAW <= {r for r, _, _ in entries}:
        raise ValueError("pinned original-draw row missing from selection")
    n_draw = sum(1 for _, blocked, _ in entries if not blocked)
    n_blocked = len(entries) - n_draw
    n_original = sum(1 for _, _, od in entries if od)
    if n_draw != 8 or n_blocked != 2 or n_original != 5:
        raise ValueError(
            f"partition drift: draw={n_draw} blocked={n_blocked} "
            f"original_draw={n_original}")
    rows = []
    for rel, blocked, original_draws in entries:
        kind = "flow-html-image-label" if blocked else None
        site, detail = WITNESS[kind] if blocked else ("", "")
        body = split_source((repo_root / CASES / rel).read_text())[1]
        tags = sorted({t.lower() for t in re.findall(r"<\s*(strong|img)\b", body, re.I)})
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / rel),
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-blocked" if blocked else "predicted-draw",
            # Short graphs paint at every frame.
            "expected_states": {
                "wide_240": "drawn" if original_draws else "source",
                "narrow_40": "drawn" if original_draws else "source",
                "restored_240": "drawn" if original_draws else "source",
            },
            "expected_body_states": {
                "wide_240": "source" if blocked else "drawn",
                "narrow_40": "source" if blocked else "drawn",
                "restored_240": "source" if blocked else "drawn",
            },
            "tags": tags,
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if blocked else None),
            "ledger_note": (
                "FS-4h closed inline-HTML label projection on the body track"
                if not blocked else
                "Blocked: the node scanner cannot delimit this <img> label"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-4h-inline-html-labels",
        "html_projection": {
            "admitted_tags": ["br", "strong", "b", "img"],
            "strong": "wrapper stripped, inner text retained",
            "img": "void element consumed with no text",
            "other_tags": "fail closed",
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original whose body carries an "
                    "inline <strong> or <img> label tag",
            "roots": ["flowchart"],
            "blocker_detection": "one fixed source-content detector "
                                 "(image-label scanner limit); no "
                                 "generator-time rendering; residual rows abort",
            "width_policy": "draw rows paint at 240, 40 and the restored "
                           "frame; originals paint only for the five "
                           "admitted-frontmatter rows.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(D-066 FS-4h inline HTML labels, 2026-11-06)",
            "date": "2026-11-06",
            "verdict": "approved",
            "note": "Read-only adversarial review of the closed inline-HTML "
                    "label projection (strong/b stripped with text retained, "
                    "void img consumed, all other tags fail closed), the "
                    "single image-label blocker with zero residual, and the "
                    "body/original matrix wiring. APPROVE: label-only blast "
                    "radius, no shared constants, gaps stay 48, five "
                    "originals move pending 292->287.",
        },
        "counts": {
            "total": n_draw + n_blocked,
            "predicted_draw": n_draw,
            "predicted_blocked": n_blocked,
            "predicted_original_draw": n_original,
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
