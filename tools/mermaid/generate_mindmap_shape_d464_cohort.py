#!/usr/bin/env python3
"""Generate the D-464 mindmap shape and `::icon` annotation cohort ledger.

Renderer-free. Admits the five mindmap originals whose only unadmitted input
was the wider shape vocabulary or an `::icon(...)` annotation:

1. the anonymous root shorthand (`((This is a mindmap))`) used by the
   tidy-tree children fixture;
2. the bang (`))text((`) and cloud (`)text(`) delimiters, with and without a
   node id;
3. the hex (`{{text}}`) delimiter;
4. `::icon(...)` annotation lines, which attach to the preceding node.

The terminal projection has no dedicated cell frame for bang, cloud or hex,
and never draws an icon glyph. Each substitution is disclosed on the canonical
footer (`Mindmap native projection v1` + one line per substituted surface), in
a fixed order: layout, bang, cloud, hex, icons. A source carrying none of
these constructs keeps its frame byte-identical (no footer), which is what
preserves the existing mindmap and Shapes4 frames.

The art contract is unchanged: the gate pins the wide/narrow/restored states,
restored cell equality, the captions and the disclosure lines. narrow_40 is
source for every row (the footer is wider than the 40-column fence).

Usage: python3 tools/mermaid/generate_mindmap_shape_d464_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/mindmap-shape-d464-cohort.json")
SCHEMA = "loom_mermaid_mindmap_shape_d464_cohort/v1"
DECISION = "D-464"

TIDY = "config.layout=tidy-tree -> external layout engine; not applied"
BANG = "shape bang -> square frame; not applied"
CLOUD = "shape cloud -> rounded frame; not applied"
HEX = "shape hex -> square frame; not applied"
ICONS = "::icon annotations -> icon glyph; not applied"

ROWS = [
    ("mindmap/pr-check/4-tidy-tree-should-render-a-mindmap-with-children.mmd",
     [TIDY], ["This is a mindmap", "grandchild 6"]),
    ("mindmap/nightly-check/bang-and-cloud-shape.mmd",
     [BANG, CLOUD], ["Another bang", "A cloud"]),
    ("mindmap/nightly-check/bang-and-cloud-shape-with-icons.mmd",
     [BANG, CLOUD], ["Another bang", "A cloud"]),
    ("mindmap/nightly-check/a-root-with-an-icon.mmd",
     [ICONS], ["root"]),
    ("mindmap/nightly-check/all-shapes-with-mdi-fire-icon.mmd",
     [BANG, CLOUD, HEX, ICONS],
     ["MindMap", "I am a square", "I am a rounded square", "I am a circle",
      "I am a bang", "I am a cloud", "I am a hexagon", "I am the default shape"]),
]

REVIEW = {
    "reviewer_agent": (
        "one independent read-only adversarial review (task a4182dd596fc94a4e); "
        "round 1 REQUEST-CHANGES on the stale drawn_at_240 arithmetic, a "
        "pre-migration gates log cited as evidence, stale-linked D439/D444 "
        "binaries, ungated icon-payload rejection branches, the unasserted "
        "notice order and a row-guard tightening; all fixed and re-verified"),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": ("Five mindmap originals were blocked only by the wider shape "
             "vocabulary and the icon annotation. The substitutions are "
             "disclosed; no geometry or syntax is invented."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {fixture["path"]: fixture for fixture in manifest["fixtures"]}
    rows = []
    for path, notices, art in ROWS:
        observed = sha256_file(repo_root / CASES / path)
        if observed != manifest_rows[path]["sha256"]:
            raise ValueError(f"sha drift for {path}")
        rows.append({
            "path": path,
            "family": "mindmap",
            "sha256": observed,
            "manifest_sha256": manifest_rows[path]["sha256"],
            "prior_status": "source_fallback",
            "oracle": "native-frame",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "source_fallback",
                "restored_240": "drawn",
            },
            "expected_notices": notices,
            "expected_captions": art,
            "ledger_note": ("shape/icon grammar only; every substituted surface "
                            "is disclosed on the canonical footer"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "stage": "mindmap shape vocabulary and ::icon annotations",
        "admission": {
            "mechanism": ("the shaped tree parser admits `((`, `(`, `[`, `{{`, "
                          "`))` and `)` delimiters with an optional node id "
                          "(the id may be omitted only on the anonymous root "
                          "shorthand) and skips validated `::icon(...)` lines"),
            "mapping": ("bang and hex draw the square frame, cloud draws the "
                        "rounded frame, and an icon is never drawn; every "
                        "substitution is disclosed"),
            "scope": ("the mindmap shaped-tree layout only; the original "
                      "projection and every other family are untouched"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_file(repo_root / MANIFEST),
        },
        "selection": {
            "rule": ("the five mindmap originals still in the fallback roster "
                     "whose body parses once the wider shape vocabulary and the "
                     "icon annotation lines are admitted"),
            "cohort_digest": sha256_bytes("".join(
                f"{row['path']}\t{row['sha256']}\n"
                for row in sorted(rows, key=lambda row: row["path"])).encode()),
        },
        "counts": {"total": len(rows), "by_family": {"mindmap": len(rows)}},
        "review": REVIEW,
        "rows": rows,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    repo_root = pathlib.Path(args.repo_root)
    ledger = build(repo_root)
    payload = json.dumps(ledger, indent=2) + "\n"
    target = repo_root / LEDGER
    if args.check:
        if not target.exists() or target.read_text() != payload:
            print(f"ledger is stale: run without --check to rewrite {LEDGER}")
            return 1
        print(f"ledger current: {json.dumps(ledger['counts'])}")
        return 0
    target.write_text(payload)
    print(f"wrote {LEDGER}: {json.dumps(ledger['counts'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
