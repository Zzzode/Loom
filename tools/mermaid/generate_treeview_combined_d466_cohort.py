#!/usr/bin/env python3
"""Generate the D-466 tree-view combined-annotation cohort ledger.

Renderer-free. Admits the one tree-view original that combines three per-node
annotations on the same line:

    App.tsx :::highlight icon(fa:bell) ## main component

D-457 already admitted a trailing `:::class`; the combined form places the
class between the label and the existing `icon(...)`/`## description`
suffixes. The terminal draws no highlight colour, so the class token is
validated as an identifier, dropped from the transcribed row and disclosed in
the footer; the icon and description suffixes remain verbatim (the projection
never draws an icon and keeps description text). Nothing else changes: the
trailing-class form, the icon()/## grammar and the gate partition behave as
before.

The art contract is unchanged: the gate pins the wide/narrow/restored states,
restored cell equality, the transcribed row and the disclosure footer. narrow_40
is source because the annotated node line is wider than the 40-column fence.

Usage: python3 tools/mermaid/generate_treeview_combined_d466_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/treeview-combined-d466-cohort.json")
SCHEMA = "loom_mermaid_treeview_combined_d466_cohort/v1"
DECISION = "D-466"

ROWS = [
    ("tree-view/nightly-check/should-render-combined-annotations.mmd",
     ["App.tsx icon(fa:bell) ## main component", "index.js ## entry point"]),
]

REVIEW = {
    "reviewer_agent": (
        "one independent read-only adversarial review (task ab1e1ff5db298db0c); "
        "round 1 REQUEST-CHANGES required the family-closure release/dual-preset "
        "byte gate and full mermaid suite (no code defect found), plus a "
        "D-457 transcription pin, provenance, the ER reclassification note and "
        "three NITs; all addressed and re-verified"),
    "date": "2026-11-25",
    "verdict": "approved",
    "note": ("One tree-view row combines a class annotation with the existing "
             "icon/description suffixes. Only the class token is dropped, and "
             "it is disclosed."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {fixture["path"]: fixture for fixture in manifest["fixtures"]}
    rows = []
    for path, excerpts in ROWS:
        observed = sha256_file(repo_root / CASES / path)
        if observed != manifest_rows[path]["sha256"]:
            raise ValueError(f"sha drift for {path}")
        rows.append({
            "path": path,
            "family": "tree-view",
            "sha256": observed,
            "manifest_sha256": manifest_rows[path]["sha256"],
            "prior_status": "source_fallback",
            "oracle": "native-frame",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "source_fallback",
                "restored_240": "drawn",
            },
            "expected_excerpts": excerpts,
            "ledger_note": ("the class token is dropped and disclosed; icon and "
                            "description suffixes stay verbatim"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "stage": "tree-view combined node annotations",
        "admission": {
            "mechanism": ("one `:::class` token may follow the label directly, "
                          "before the existing `icon(...)`/`## description` "
                          "suffixes; it is validated as an identifier, dropped "
                          "and disclosed"),
            "scope": ("the tree-view node annotation stripper only; the "
                      "trailing-class form and every other family are "
                      "untouched"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_file(repo_root / MANIFEST),
        },
        "selection": {
            "rule": "the one tree-view original whose body places `:::class` before its suffix tokens",
            "cohort_digest": sha256_bytes("".join(
                f"{row['path']}\t{row['sha256']}\n"
                for row in sorted(rows, key=lambda row: row["path"])).encode()),
        },
        "counts": {"total": len(rows), "by_family": {"tree-view": len(rows)}},
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
