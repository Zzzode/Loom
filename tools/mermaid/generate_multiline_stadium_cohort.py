#!/usr/bin/env python3
"""Generate the RFC 0031 stadium multiline-label cohort ledger.

Explicit authored line breaks (`<br>`/`<br/>`/`<br />`, decoded to a
newline upstream) were rejected for every classic glyph by the single-row
node_size guard. The stadium contour is parameterised by its top/bottom
rows, so it scales cleanly to a taller box; this batch admits multiline
stadium labels by sizing height to the row count. Fixed-silhouette
classics (rings, triangles, wave shapes) keep the single-row guard and
fail closed.

The single unlocked corpus row is the BODY track of
stadium-tall-labels.mmd. Its whole original stays on ordinary fallback
under the strict whole-source envelope path. The fixture's frontmatter
combines top-level `config.htmlLabels: false` (RootHtml bit) with
`fontFamily: courier` (Font bit); the whole-source admission profiles
cover each key alone but not the Font|RootHtml combination, so the
envelope parser rejects the whole source (the same pending disposition
it had before this change). The multiline capability therefore unlocks
only the relaxed body track: this is a body-only ledger row with no
Flow400 whole-source admission, leaving whole-original pending/admitted
counts and the 48-gap roster untouched.

Usage: python3 tools/mermaid/generate_multiline_stadium_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/multiline-stadium-cohort.json")

SCHEMA = "loom_multiline_stadium_cohort/v1"

BODY_ONLY_ROW = (
    "flowchart/visual-check/stadium-tall-labels.mmd"
)


def split_source(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None, text


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    if BODY_ONLY_ROW not in manifest_rows:
        raise ValueError(f"ledger row missing from manifest: {BODY_ONLY_ROW}")
    observed = sha256_file(repo_root / CASES / BODY_ONLY_ROW)
    if observed != manifest_rows[BODY_ONLY_ROW]["sha256"]:
        raise ValueError(f"sha drift for {BODY_ONLY_ROW}")
    rows = [{
        "path": BODY_ONLY_ROW,
        "family": "flowchart",
        "sha256": observed,
        "manifest_sha256": manifest_rows[BODY_ONLY_ROW]["sha256"],
        "prior_status": manifest_rows[BODY_ONLY_ROW]["raw"],
        "prediction": "predicted-draw",
        "track_scope": "body-only",
        "expected_states": {  # whole original stays source (config gate)
            "wide_240": "source",
            "narrow_40": "source",
            "restored_240": "source",
        },
        "expected_body_states": {
            "wide_240": "drawn",
            "narrow_40": "source",
            "restored_240": "drawn",
        },
        "ledger_note": (
            "stadium multiline <br/> label draws body-only; whole original "
            "rejected because top-level htmlLabels=false (RootHtml) + "
            "fontFamily=courier (Font) matches no whole-source admission "
            "profile (Font|RootHtml combination unsupported)"
        ),
    }]
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "stadium-multiline-label",
        "label_projection": {
            "break_spellings": ["<br>", "<br/>", "<br />"],
            "behaviour": ("decoded to a newline; stadium box height grows by "
                          "one row per line; non-height-scaling classic shapes "
                          "keep single-row-only fail-closed behaviour"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": ("the flowchart original whose body fallback is solely the "
                     "classic single-row label guard on a stadium node"),
            "roots": ["flowchart"],
        },
        "review": {
            "reviewer_agent": ("independent read-only adversarial review (fresh "
                              "general-purpose agent, stadium multiline cohort, "
                              "2026-11-08)"),
            "date": "2026-11-08",
            "verdict": "approved",
            "note": ("Independent read-only adversarial review (fresh "
                     "general-purpose agent, 2026-11-08): production node_size "
                     "change verified minimal and correct; tall stadiums keep "
                     "caps/walls clear of the centred multi-row label and all "
                     "non-stadium classics stay fail-closed. The body-only "
                     "rationale was corrected to the real cause: the whole "
                     "original is rejected because top-level htmlLabels=false "
                     "(RootHtml) + fontFamily=courier (Font) matches no "
                     "whole-source admission profile."),
        },
        "counts": {
            "total": 1,
            "predicted_draw": 1,
            "predicted_blocked": 0,
            "predicted_original_draw": 0,
        },
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
