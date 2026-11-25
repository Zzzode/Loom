#!/usr/bin/env python3
"""Generate the D-814 flow `look: handDrawn` disclosure cohort ledger.

The twenty-two flowchart originals in `nightly-check/handdrawn/` whose only
undmitted frontmatter leaf is the rough.js sketch skin (`config.look:
handDrawn`, optionally with `config.handDrawnSeed`) now render their structure
natively. The terminal never paints sketch strokes, so the leaf is
medium-inert: every art row above the disclosure footer equals the config-free
body twin, and the footer says so. Each row is admitted on reviewed
source-byte identity (no Flow400 Model) because the disclosure footer is
unclaimed ink for the Flow400 geometry oracle.

Selection is mechanical and renderer-free:
  * every `flowchart/` fixture in the official manifest whose frontmatter
    carries `look: handDrawn` (bare, single- or double-quoted) and, when
    present, a `handDrawnSeed` that is a positive decimal of at most nine
    digits;
  * the row must be named in the fixed ROWS table below, so a future look-row
    that the envelope profile still rejects cannot silently enter the cohort;
  * the expected notices are derived from the keys the source actually
    carries, never from the ledger.

Usage: python3 tools/mermaid/generate_flow_look_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/flow-look-cohort.json")

SCHEMA = "loom_mermaid_flow_look_cohort/v1"

CAUSE = (
    "config.look=handDrawn is the rough.js sketch skin; a terminal draws table "
    "glyphs, so the leaf is medium-inert: every art row above the disclosure "
    "footer equals the config-free body twin, and the footer names the "
    "unapplied skin. The disclosure footer is unclaimed ink for the Flow400 "
    "geometry oracle, so the row admits on reviewed source-byte identity "
    "instead of a Model")

ROWS = (
    "01-fdh39-should-render-a-flowchart-when-usemaxwidth-is-false.mmd",
    "02-fdh38-should-render-a-flowchart-when-usemaxwidth-is-true-default.mmd",
    "chaining-of-nodes.mmd",
    "fontawesome-icons-in-edge-labels.mmd",
    "multiple-nodes-and-chaining-in-one-statement.mmd",
    "render-a-simple-flowchart-with-nodespacing-set-to-100.mmd",
    "render-a-simple-flowchart-with-rankspacing-set-to-100.mmd",
    "should-honor-minimum-edge-length-as-specified-by-the-user.mmd",
    "should-not-slice-off-edges-that-are-to-the-left-of-the-left-most-vertex.mmd",
    "should-redirect-a-collapsed-subgraph-using-the-handdrawn-look.mmd",
    "should-render-a-simple-flowchart-no-htmllabels.mmd",
    "should-render-a-simple-flowchart-with-comments.mmd",
    "should-render-a-simple-flowchart-with-diagrampadding-set-to-0.mmd",
    "should-render-a-simple-flowchart-with-line-breaks.mmd",
    "should-render-a-simple-flowchart-with-trapezoid-and-inverse-trapezoid-vertex-options.mmd",
    "should-render-escaped-without-html-labels.mmd",
    "should-render-labels-with-numbers-at-the-start.mmd",
    "should-render-non-escaped-with-html-labels.mmd",
    "should-render-subgraphs-with-a-title-starting-with-a-digit.mmd",
    "should-render-subgraphs.mmd",
    "testing-the-label-width-in-percy.mmd",
    "text-color-from-classes.mmd",
)

ROOT = "flowchart/nightly-check/handdrawn/"


def sha(text):
    return hashlib.sha256(text.encode()).hexdigest()


def frontmatter(source):
    if not source.startswith("---"):
        return ""
    end = source.find("\n---", 3)
    return source[3:end] if end != -1 else source[3:]


def scalar(value):
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
        return value[1:-1]
    return value


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for name in ROWS:
        path = ROOT + name
        if path not in manifest_rows:
            raise ValueError(f"row missing from the official manifest: {path}")
        source = (repo_root / CASES / path).read_text()
        pinned = manifest_rows[path]["sha256"]
        if sha(source) != pinned:
            raise ValueError(f"source drift: {path}")
        fm = frontmatter(source)
        look = re.search(r"^\s*look\s*:\s*(\S+)\s*$", fm, re.M)
        if look is None or scalar(look.group(1)) != "handDrawn":
            raise ValueError(f"row does not carry look: handDrawn: {path}")
        notes = ["config.look=handDrawn -> source node style; not applied"]
        seed = re.search(r"^\s*handDrawnSeed\s*:\s*(\S+)\s*$", fm, re.M)
        if seed is not None:
            value = scalar(seed.group(1))
            if not re.fullmatch(r"[1-9][0-9]{0,8}", value):
                raise ValueError(f"row carries an unadmitted handDrawnSeed: {path}")
            notes.append(f"config.handDrawnSeed={value} -> sketch jitter seed; not applied")
        rows.append({
            "path": path,
            "family": "flowchart",
            "sha256": pinned,
            "manifest_sha256": pinned,
            "cause": CAUSE,
            "notices": notes,
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "source_fallback",
                "restored_240": "drawn",
            },
        })
    rows.sort(key=lambda row: row["path"])
    return {
        "schema": SCHEMA,
        "decision": "D-814: medium-inert sketch look admitted with disclosure",
        "selection": {
            "rule": "flowchart original whose config carries look: handDrawn "
                    "(plus an optional positive handDrawnSeed) and whose body is "
                    "otherwise admissible; the disclosure footer is unclaimed "
                    "ink for the Flow400 geometry oracle",
            "rows": len(rows),
        },
        "review": {
            "reviewer_agent": "independent adversarial review (Claude Code subagent, D-814 medium-inert look cohort, 2026-11-26; r1 REQUEST-CHANGES -> r2 REQUEST-CHANGES -> r3 REQUEST-CHANGES -> r4 REQUEST-CHANGES -> r5 APPROVE)",
            "date": "2026-11-26",
            "verdict": "approved",
            "note": "Five adversarial rounds against the working tree: r1 "
                    "REQUEST-CHANGES (matrix branch not wired into the flow "
                    "accumulator, twelve-row overlap with the FS-3a/FS-4h/FS-6/"
                    "FS-7b ledgers, ledger-approval vs generator-check conflict, "
                    "missing twin assertion), r2 REQUEST-CHANGES (re-baselined "
                    "ledger metadata described the old look gate), r3 "
                    "REQUEST-CHANGES (spacing cohort framed look as a hard gate, "
                    "FS-6 policy arithmetic, stale comments), r4 REQUEST-CHANGES "
                    "(spacing addendum dropped by review preservation, two "
                    "comment/docstring residuals), r5 APPROVE. The durable gate "
                    "renders every row at 240/40/240 and compares every art row "
                    "above the footer with the config-free body twin.",
        },
        "rows": rows,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = pathlib.Path(args.repo_root)
    payload = json.dumps(build(root), indent=1) + "\n"
    target = root / LEDGER
    if args.check:
        if not target.exists() or target.read_text() != payload:
            print(f"ledger is stale: run without --check to rewrite {LEDGER}")
            return 1
        print("ledger current")
        return 0
    target.write_text(payload)
    print(f"wrote {LEDGER}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
