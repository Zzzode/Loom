#!/usr/bin/env python3
"""Generate the RFC 0031 FS-5a label-only shape-data flowchart cohort ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/flowchart root that
    - carries a column-0 YAML frontmatter envelope,
    - whose body uses the newer per-node shape-data syntax with `label`
      as the FIRST key and no `shape:` key:  id@{ label: "text" }.

FS-5a teaches the parser that this label-only metadata form is a plain
rectangular labeled node (the previous parser accepted only
`@{shape: x, label: ...}`). The key order matters: an icon-first block
(`@{ icon: ..., label: ...}`) or an explicit non-rectangle shape is a
different feature and stays on ordinary source fallback. In the pinned
corpus every selected body draws after the change with zero residual,
so this cohort is all-draw (4 / 0).

Each row paints body and whole-original at 240 (wide + restored); the
one small edge-link graph also paints at narrow_40, the three wider
multi-node chains keep ordinary source there.

Usage: python3 tools/mermaid/generate_fs5a_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs5a-cohort.json")

SCHEMA = "loom_mermaid_fs5a_cohort/v1"

# `label` as the first metadata key, optional whitespace around key and
# colon (mermaid tolerates `label :`). Icon-first/shape-first blocks are a
# different production and must not match.
LABEL_FIRST_RE = re.compile(r"@\{[ \t]*label[ \t]*:")

# Only the one edge-link graph is narrow enough to paint at 40 columns.
NARROW_DRAW = {
    "flowchart/visual-check/should-be-possible-to-use-syntax-to-add-labels-with-trail-spaces-and-edge-link.mmd",
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
    for path in sorted((repo_root / CASES / "flowchart").rglob("*.mmd")):
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        frontmatter, body = split
        if not LABEL_FIRST_RE.search(body):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        narrow = rel in NARROW_DRAW
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / rel),
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn" if narrow else "source",
                "restored_240": "drawn",
            },
            "expected_body_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn" if narrow else "source",
                "restored_240": "drawn",
            },
            "blocker": None,
            "ledger_note": "FS-5a label-only @{ label: } shape-data node admitted "
                           "as a rectangular labeled node",
        })
    if len(rows) != 4:
        raise ValueError(f"selection drift: {len(rows)} rows (expected 4)")
    if not {r["path"] for r in rows} >= NARROW_DRAW:
        raise ValueError("narrow-draw pin missing from selection")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-5a-label-only-shape-data",
        "shape_data_projection": {
            "form": "id@{ label: \"text\" }",
            "projects_to": "rectangular labeled node",
            "label_must_be_first_key": True,
            "rejected": ["@{ icon: ... }", "@{ shape: <non-rect>, ... }",
                         "icon-first @{ icon, label }"],
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original whose body binds a node "
                    "with @{ label: ... } as the first shape-data key and no "
                    "shape key",
            "roots": ["flowchart"],
            "draw_authority": "every selected body renders at 240 after the "
                              "parser change; zero residual (all-draw cohort)",
            "width_policy": "all rows paint body+original at 240/restored; "
                           "only the pinned edge-link graph paints at 40.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(D-066 FS-5a label-only shape-data, 2026-11-06)",
            "date": "2026-11-06",
            "verdict": "approved",
            "note": "Read-only adversarial review of the label-first @{} "
                    "branch (rectangular node, quoted label, optional "
                    "whitespace, icon/shape-first blocks still fail closed), "
                    "the key-ordered zero-residual selection, and the "
                    "body/original matrix wiring. APPROVE: node-parser-only "
                    "blast radius, no new shape contour, no shared constants, "
                    "gaps stay 48, four originals move pending 287->283.",
        },
        "counts": {
            "total": len(rows),
            "predicted_draw": len(rows),
            "predicted_blocked": 0,
            "predicted_original_draw": len(rows),
        },
        "blocker_vocabulary": [],
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
