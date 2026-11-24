#!/usr/bin/env python3
"""Generate the D-470 Flow400 theme-group supplemental authority.

TL round 804 authorizes a *group* Flow400 re-baseline: the eight
`flowchart/nightly-check/redux-color-subgraphs/` originals whose only
non-admitted input is `config.theme` from the four redux palettes. The base
authority (`flow400_native_whole_source.v1.json`, 400 sources / 8 complete
models / 392 pending) is left untouched; this supplement supplies the eight
complete typed models and their source envelopes.

The two distinct bodies are built literally:
* A (TB, five sibling subgraphs) — 8 nodes, one cylinder, 7 edges;
* B (LR, nested subgraphs) — 4 nodes, 3 edges.
Both models were certified against the production renderer with the
test-private Flow400 oracle (0 geometry errors, 4 mutations) before this
generator was written.

Usage:
  python3 tools/mermaid/generate_flow400_theme8_supplement.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
BASE_ASSET = pathlib.Path("tests/fixtures/mermaid_native/flow400_native_whole_source.v1.json")
CONFIG21_ASSET = pathlib.Path(
    "tests/fixtures/mermaid_native/flow400_config21_native_supplement.v1.json")
TYPED4_ASSET = pathlib.Path(
    "tests/fixtures/mermaid_native/flow400_typed_source4_formal_supplement.v1.json")
CLASSIC_ASSET = pathlib.Path(
    "tests/fixtures/mermaid_native/flow400_classic_native_supplement.v1.json")
OUTPUT = pathlib.Path("tests/fixtures/mermaid_native/flow400_theme8_native_supplement.v1.json")

SCHEMA = "flow400_theme8_native_supplement/v1"
QUALIFICATION = (
    "Source-pinned complete typed Models for the four redux palettes require "
    "independent Native and Official actual runs before admission; the base "
    "authority and the four earlier supplements stay untouched.")

MUTATIONS = [
    "cylinder_to_circle",
    "first_label_to_absent",
    "first_edge_endpoint_to_self",
    "extra_ink",
]

SIBLINGS = ["Ingest", "Transform", "Store", "Serve", "Observe"]


def sha(text):
    return hashlib.sha256(text.encode()).hexdigest()


def model_siblings():
    nodes = [
        ("A", "Fetch", "rectangle"),
        ("B", "Validate", "rectangle"),
        ("C", "Normalise", "rectangle"),
        ("D", "Enrich", "rectangle"),
        ("E", "Warehouse", "cylinder"),
        ("F", "API", "rectangle"),
        ("G", "Cache", "rectangle"),
        ("H", "Metrics", "rectangle"),
    ]
    edges = [
        ("A", "B"), ("C", "D"), ("F", "G"), ("B", "C"), ("D", "E"), ("E", "F"), ("F", "H"),
    ]
    groups = [
        ("Ingest", ["A", "B"]),
        ("Transform", ["C", "D"]),
        ("Store", ["E"]),
        ("Serve", ["F", "G"]),
        ("Observe", ["H"]),
    ]
    return model_json("TB", nodes, edges, groups)


def model_nested():
    nodes = [
        ("A", "one", "rectangle"),
        ("B", "two", "rectangle"),
        ("C", "three", "rectangle"),
        ("D", "four", "rectangle"),
    ]
    edges = [("A", "B"), ("B", "C"), ("C", "D")]
    return model_json("LR", nodes, edges, None, nested=True)


def model_json(direction, nodes, edges, flat_groups, nested=False):
    node_rows = [[ident, {"id": ident, "label": label, "shape": shape}]
                 for ident, label, shape in nodes]
    edge_rows = [{"source": source, "target": target, "label": "", "style": "solid",
                  "hasArrowStart": False, "hasArrowEnd": True}
                 for source, target in edges]
    if nested:
        subgraphs = [
            {"id": "Outer", "label": "Outer", "direction": "", "nodeIds": [], "children": [
                {"id": "InnerOne", "label": "InnerOne", "direction": "",
                 "nodeIds": ["A", "B"], "children": []},
                {"id": "InnerTwo", "label": "InnerTwo", "direction": "",
                 "nodeIds": ["C"], "children": []},
            ]},
            {"id": "Sibling", "label": "Sibling", "direction": "",
             "nodeIds": ["D"], "children": []},
        ]
    else:
        subgraphs = [{"id": name, "label": name, "direction": "", "nodeIds": members,
                      "children": []} for name, members in flat_groups]
    document = {"direction": direction, "nodes": node_rows, "edges": edge_rows,
                "subgraphs": subgraphs}
    return json.dumps(document, separators=(",", ":"))


def envelope_of(source):
    assert source.startswith("---\n")
    close = source.find("\n---\n", 4)
    text = source[:close + 5]
    body = source[close + 5:]
    lines = text[4:-5].split("\n")
    root_keys = [line.split(":", 1)[0] for line in lines if line and not line.startswith(" ")]
    description_form = ("folded >-" if any(line.startswith("description: >-")
                                           for line in lines) else "plain_multiline")
    theme = None
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("theme:"):
            theme = stripped.split(":", 1)[1].strip()
    header = next(line.strip() for line in body.split("\n") if line.strip())
    return {
        "profile": "description_config_theme",
        "bytes": len(text.encode()),
        "sha256": sha(text),
        "text": text,
        "description_form": description_form,
        "root_keys": root_keys,
        "config": {"theme": theme},
        "body_header": header,
        "body_sha256": sha(body),
    }


def build(repo_root):
    base = json.loads((repo_root / BASE_ASSET).read_text())
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    base_rows = {row["fixture"]: row for row in base["cases"]}

    group = [
        ("01-should-render-subgraphs-subgraph-containers.mmd", model_siblings),
        ("02-should-render-nested-subgraph-containers.mmd", model_nested),
        ("05-should-render-subgraphs-subgraph-containers.mmd", model_siblings),
        ("06-should-render-nested-subgraph-containers.mmd", model_nested),
        ("09-should-render-subgraphs-subgraph-containers.mmd", model_siblings),
        ("10-should-render-nested-subgraph-containers.mmd", model_nested),
        ("13-should-render-subgraphs-subgraph-containers.mmd", model_siblings),
        ("14-should-render-nested-subgraph-containers.mmd", model_nested),
    ]

    cases = []
    for name, builder in group:
        fixture = f"flowchart/nightly-check/redux-color-subgraphs/{name}"
        base_row = base_rows[fixture]
        if base_row["model_json"] is not None:
            raise ValueError(f"base row already complete: {fixture}")
        source = (repo_root / CASES / fixture).read_bytes()
        if len(source) != base_row["source_bytes"] or sha(source.decode()) != base_row["source_sha256"]:
            raise ValueError(f"source drift: {fixture}")
        if base_row["source_sha256"] != manifest_rows[fixture]["sha256"]:
            raise ValueError(f"manifest drift: {fixture}")
        model = builder()
        cases.append({
            "case_id": f"original_{base_row['ordinal']:03d}_redux_theme",
            "base_authority": base_row,
            "source": source.decode(),
            "source_sha256": base_row["source_sha256"],
            "typed_model_json": model,
            "typed_model_sha256": sha(model),
            "envelope": envelope_of(source.decode()),
            "native_requirement": {"exact_rejections": 4, "mutations": MUTATIONS},
            "formal_status": "unadmitted_design_only",
        })

    def asset_sha(path):
        return sha((repo_root / path).read_text())

    return {
        "schema": SCHEMA,
        "qualification": QUALIFICATION,
        "base_authority": {
            "asset_sha256": asset_sha(BASE_ASSET),
            "manifest_sha256": asset_sha(MANIFEST),
            "all_sources": 400,
            "complete_models": 8,
            "pending": 392,
        },
        "prior_authority": {
            "config21_asset_sha256": asset_sha(CONFIG21_ASSET),
            "typed_source4_asset_sha256": asset_sha(TYPED4_ASSET),
            "classic_asset_sha256": asset_sha(CLASSIC_ASSET),
            "aggregate_complete_before_theme8": 39,
            "aggregate_pending_before_theme8": 361,
        },
        "formal_counts": {
            "cases": len(cases),
            "aggregate_complete_after_all_official_runs": 39 + len(cases),
            "aggregate_pending_after_all_official_runs": 361 - len(cases),
            "ultimate_complete_required": 400,
            "ultimate_pending_required": 0,
            "official_credit_before_execution": 0,
        },
        "cases": cases,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = pathlib.Path(args.repo_root)
    payload = json.dumps(build(root), indent=1) + "\n"
    target = root / OUTPUT
    if args.check:
        if not target.exists() or target.read_text() != payload:
            print(f"supplement is stale: run without --check to rewrite {OUTPUT}")
            return 1
        print("supplement current")
        return 0
    target.write_text(payload)
    print(f"wrote {OUTPUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
