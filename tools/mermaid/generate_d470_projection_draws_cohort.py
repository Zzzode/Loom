#!/usr/bin/env python3
"""Generate the D-470 reviewed projection-draw cohort ledger.

TL round 809 classifies the rows the palette opening and the bounded flow
Work envelopes admit without giving them a new Model: the twelve
redux-color-subgraph originals (four subgraph-container, four nested, four
collapsed siblings), two otherwise-pending originals whose Work outgrew the
fixed 1M primary and four whose frontmatter prepayment exceeds it. Measurement after round 806 showed
the palette disclosure footer is unclaimed ink for the Flow400 geometry
oracle for BOTH the nested and the subgraph-container bodies, so none of the
eight can use the Model path. Each row
is admitted on reviewed source-byte identity with a per-row concrete cause;
none of them enters the Flow400 Model path.

Selection is mechanical (renderer-free):
  * the eight `redux-color-subgraphs` originals whose only non-admitted
    input is `config.theme` (`01/05/09/13` subgraph containers and
    `02/06/10/14` nested),
  * the four whose bodies mark a subgraph `@{ view: collapsed }`
    (`03/07/11/15`),
  * the two Flow400 originals the D-470 envelope renders natively
    (visual-check issue-4648 and dagre 7827 keep-entry-node),
  * the four originals whose frontmatter parse prepayment exceeds the fixed
    primary and now render inside the bounded parse-stage pool
    (6080/6088-4 diamond intersections, forest-theme-legacy-graph, v2-18
    nested subgraphs).
Every row is pinned to the official manifest bytes. The per-row `cause` is
the reviewed explanation; `footer` names the disclosure the render must
carry (theme palette, root htmlLabels, or none).

Usage: python3 tools/mermaid/generate_d470_projection_draws_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/d470-projection-draws-cohort.json")

SCHEMA = "loom_mermaid_d470_projection_draws_cohort/v1"

REDUX = "flowchart/nightly-check/redux-color-subgraphs/"

SIBLINGS_CAUSE = (
    "five-sibling-subgraph original blocked only by config.theme; the required "
    "palette disclosure footer is unclaimed ink for the Flow400 geometry "
    "oracle, so the row admits on reviewed source-byte identity instead of a "
    "Model")
NESTED_CAUSE = (
    "nested subgraph original blocked only by config.theme; the required "
    "palette disclosure footer is unclaimed ink for the Flow400 geometry "
    "oracle, so the row admits on reviewed source-byte identity instead of a "
    "Model")
COLLAPSED_CAUSE = (
    "collapsed subgraph (`@{ view: collapsed }`) renders through the reviewed "
    "FS-7b projection; the flow Model schema cannot express a collapsed flag "
    "and the palette footer blocks the geometry oracle, so the row admits on "
    "reviewed source-byte identity")
ISSUE4648_CAUSE = (
    "sibling subgraphs with per-group directions; the whole source needs more "
    "than the fixed 1M Work primary and renders natively inside the bounded "
    "D-470 canvas envelope under the admitted strict htmlLabels profile (no "
    "footer: the strict profile is non-terminal)")
PARSE_POOL_CAUSE = (
    "frontmatter flow source whose reviewed full-profile parse prepayment "
    "exceeds the fixed 1M Work primary; the whole source renders natively "
    "inside the bounded parse-stage pool, so it admits on reviewed source-byte "
    "identity without a Model")
DAGRE7827_CAUSE = (
    "recursive flow nested in a subgraph under config.layout=dagre; the whole "
    "source renders natively inside the bounded D-470 canvas envelope and the "
    "untrusted-compound guard still applies, with no footer")

ROWS = [
    (REDUX + "01-should-render-subgraphs-subgraph-containers.mmd", SIBLINGS_CAUSE,
     {"theme": "redux"}),
    (REDUX + "05-should-render-subgraphs-subgraph-containers.mmd", SIBLINGS_CAUSE,
     {"theme": "redux"}),
    (REDUX + "09-should-render-subgraphs-subgraph-containers.mmd", SIBLINGS_CAUSE,
     {"theme": "redux"}),
    (REDUX + "13-should-render-subgraphs-subgraph-containers.mmd", SIBLINGS_CAUSE,
     {"theme": "redux"}),
    (REDUX + "02-should-render-nested-subgraph-containers.mmd", NESTED_CAUSE,
     {"theme": "redux"}),
    (REDUX + "06-should-render-nested-subgraph-containers.mmd", NESTED_CAUSE,
     {"theme": "redux"}),
    (REDUX + "10-should-render-nested-subgraph-containers.mmd", NESTED_CAUSE,
     {"theme": "redux"}),
    (REDUX + "14-should-render-nested-subgraph-containers.mmd", NESTED_CAUSE,
     {"theme": "redux"}),
    (REDUX + "03-should-render-collapsed-subgraph-containers.mmd", COLLAPSED_CAUSE,
     {"theme": "redux"}),
    (REDUX + "07-should-render-collapsed-subgraph-containers.mmd", COLLAPSED_CAUSE,
     {"theme": "redux"}),
    (REDUX + "11-should-render-collapsed-subgraph-containers.mmd", COLLAPSED_CAUSE,
     {"theme": "redux"}),
    (REDUX + "15-should-render-collapsed-subgraph-containers.mmd", COLLAPSED_CAUSE,
     {"theme": "redux"}),
    ("flowchart/visual-check/"
     "issue-4648-sibling-subgraphs-with-different-directions-and-external-connections.mmd",
     ISSUE4648_CAUSE, None),
    ("flowchart/nightly-check/dagre/"
     "dagre-should-keep-the-entry-node-on-top-of-a-recursive-flow-nested-in-a-subgraph-with-"
     "dagre-keepentrynodeontop-true-7827.mmd",
     DAGRE7827_CAUSE, None),
    ("flowchart/nightly-check/dagre/6080-should-handle-diamond-shape-intersections.mmd",
     PARSE_POOL_CAUSE, None),
    ("flowchart/nightly-check/dagre/6088-4-should-handle-diamond-shape-intersections.mmd",
     PARSE_POOL_CAUSE, None),
    ("flowchart/visual-check/forest-theme-legacy-graph.mmd", PARSE_POOL_CAUSE,
     {"theme": "forest"}),
    ("flowchart/visual-check/"
     "v2-18-should-render-nested-subgraphs-with-edge-from-cluster-containing-extractable-"
     "subgraph.mmd",
     PARSE_POOL_CAUSE, None),
]


def sha(text):
    return hashlib.sha256(text.encode()).hexdigest()


def theme_of(source):
    for line in source.split("\n"):
        stripped = line.strip()
        if stripped.startswith("theme:"):
            value = stripped.split(":", 1)[1].strip()
            if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
                value = value[1:-1]
            return value
    return None


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for path, cause, footer in ROWS:
        source = (repo_root / CASES / path).read_text()
        pinned = manifest_rows[path]["sha256"]
        if sha(source) != pinned:
            raise ValueError(f"source drift: {path}")
        palette = theme_of(source)
        if footer is not None and footer.get("theme") is not None and palette is None:
            raise ValueError(f"missing palette: {path}")
        rows.append({
            "path": path,
            "family": path.split("/", 1)[0],
            "sha256": pinned,
            "manifest_sha256": pinned,
            "cause": cause,
            "footer": (None if footer is None else
                       {"theme": palette} if footer.get("theme") is not None else footer),
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "source_fallback",
                "restored_240": "drawn",
            },
        })
    return {
        "schema": SCHEMA,
        "decision": "TL round 809: classify, do not add Models",
        "selection": {
            "rule": "palette-opening and D-470 envelope projection draws with no "
                    "Flow400 Model; source bytes pinned to the official manifest",
            "rows": len(rows),
        },
        "review": {
            "reviewer_agent": "independent adversarial reviewer (Claude Code, "
                              "model deepseek-v4.1-flash)",
            "date": "2026-11-25",
            "verdict": "approved",
            "note": "Round 1 REQUEST-CHANGES (CMake blast radius accidentally "
                    "deleting unrelated registrations, generator/ledger verdict "
                    "split, cache miss/hit parity for envelope-funded flow rows, "
                    "stale 14-row comments) and round 2 APPROVE after every "
                    "finding was fixed. Every row is a projection draw, not a "
                    "Model admission: the durable gate renders each row at "
                    "240/40/240 and pins the disclosure footer and source bytes.",
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
