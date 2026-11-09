#!/usr/bin/env python3
"""Generate the RFC 0031 FS-7b collapsed-subgraph cohort ledger.

A subgraph annotated `<id>@{ view: collapsed }` renders as one titled
rectangle: its descendant vertices, nested frames and internal edges are
hidden, and external edges terminate on the collapsed node. Slice B
admits the cases whose external edges already target the group id (no
endpoint remap); Slice C (endpoint remap onto a collapsed ancestor) is a
separate, later cohort.

Three corpus rows draw on the BODY track after the directive parses:

- two visual-check rows draw the WHOLE original too (description-only
  frontmatter, no envelope gate): whole-and-body;
- one handDrawn nightly row draws the body only: its whole original
  stays source behind the `look: handDrawn` (FS-8) frontmatter gate,
  which the body track never sees.

The other six collapsed fixtures stay on ordinary source for reasons
independent of this directive and are pinned in OUT_OF_SCOPE so any
corpus change to the collapsed set trips this generator's drift guard:

- four redux-color-subgraphs rows carry redux theme/neo config debt;
- two Slice C visual-check rows need collapsed-ancestor endpoint remap.

The generator is renderer-free: it cannot know draw/source, so the
durable gtest enforces every state live. It does enforce (a) manifest sha
pins, (b) that the whole-line collapse-directive set in the corpus is
exactly the union of the admitted and out-of-scope rows, and (c) review
verdict preservation across regeneration.

Usage: python3 tools/mermaid/generate_fs7b_collapsed_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs7b-collapsed-cohort.json")

SCHEMA = "loom_mermaid_fs7b_collapsed_subgraph_cohort/v1"

# Each row pins its full per-width state triples measured by CellCapture
# against the live renderer (wide_240 / narrow_40 / restored_240). Collapsed
# graphs are much narrower than the expanded form, so unlike FS-7a the narrow
# frame is NOT uniformly source — single-node and the handDrawn body still fit
# and draw at 40 columns. States are measured, never assumed.
#
# path -> dict(scope, whole=(w,n,r), body=(w,n,r), blocker)
ROWS = {
    "flowchart/visual-check/should-render-a-collapsed-subgraph-as-a-single-node-with-edges-redirected-to-it.mmd":
        {"scope": "whole-and-body", "blocker": None,
         "whole": ("drawn", "drawn", "drawn"),
         "body": ("drawn", "drawn", "drawn")},
    "flowchart/visual-check/should-render-a-collapsed-subgraph-alongside-an-expanded-one.mmd":
        {"scope": "whole-and-body", "blocker": None,
         "whole": ("drawn", "source", "drawn"),
         "body": ("drawn", "source", "drawn")},
    "flowchart/nightly-check/handdrawn/should-redirect-a-collapsed-subgraph-using-the-handdrawn-look.mmd":
        {"scope": "body-only",
         "blocker": "whole original stays source behind the look: handDrawn "
                    "(FS-8) frontmatter gate; the body track has no "
                    "frontmatter and draws",
         "whole": ("source", "source", "source"),
         "body": ("drawn", "drawn", "drawn")},
}

# Collapsed fixtures that remain on source for reasons outside this cohort.
OUT_OF_SCOPE = {
    "flowchart/nightly-check/redux-color-subgraphs/03-should-render-collapsed-subgraph-containers.mmd":
        "redux theme/neo config debt; the collapse directive is not the sole blocker",
    "flowchart/nightly-check/redux-color-subgraphs/07-should-render-collapsed-subgraph-containers.mmd":
        "redux theme/neo config debt; the collapse directive is not the sole blocker",
    "flowchart/nightly-check/redux-color-subgraphs/11-should-render-collapsed-subgraph-containers.mmd":
        "redux theme/neo config debt; the collapse directive is not the sole blocker",
    "flowchart/nightly-check/redux-color-subgraphs/15-should-render-collapsed-subgraph-containers.mmd":
        "redux theme/neo config debt; the collapse directive is not the sole blocker",
    "flowchart/visual-check/should-redirect-an-edge-targeting-a-deeply-nested-node-to-the-outermost-collapsed-subgraph.mmd":
        "Slice C: a vertex endpoint must remap onto the collapsed ancestor",
    "flowchart/visual-check/should-collapse-nested-subgraphs-to-the-outermost-collapsed-ancestor.mmd":
        "Slice C: a nested-group endpoint must remap onto the collapsed ancestor",
}

# A whole-line statement of the form `<id>@{ view: collapsed }`. The value
# accepts the bare or double-quoted spelling (the parser accepts both); the
# description prose mentioning the directive is wrapped text, never a
# whole-line statement, so this does not match it.
DIRECTIVE_RE = re.compile(
    r'^\s*[A-Za-z_][\w-]*@\{\s*view\s*:\s*(?:"collapsed"|collapsed)\s*\}\s*$')


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def corpus_collapsed_set(repo_root):
    found = set()
    for path in sorted((repo_root / CASES / "flowchart").rglob("*.mmd")):
        text = path.read_text()
        # Only count a directive that appears as a code statement, not prose.
        if any(DIRECTIVE_RE.match(line) for line in text.splitlines()):
            found.add(path.relative_to(repo_root / CASES).as_posix())
    return found


def build_review(repo_root):
    review = {
        "reviewer_agent": (
            "independent read-only adversarial review (fresh general-purpose "
            "agent, FS-7b Slice B collapsed-subgraph, 2026-11-09, task "
            "acf9dfcbd2cb83b20, zero builds); r1 REQUEST-CHANGES -> r2 APPROVE"),
        "date": "2026-11-09",
        "verdict": "approved",
        "note": (
            "r1 found two MAJORs, both fixed before approval: (1) an edge "
            "joining vertices hidden inside two DIFFERENT collapsed groups "
            "was silently dropped; the projection now distinguishes same-leaf "
            "internal edges (dropped) from different-leaf edges (fail-closed, "
            "deferred to Slice C remap), and the collapsed group's own visible "
            "endpoint is no longer misclassified as hidden; (2) the parser "
            "accept/reject matrix was untested, now locked by "
            "test_mermaid_fs7b_directive through the public render API with "
            "CellCapture comparisons. MINOR: the corpus drift regex now "
            "accepts both bare and double-quoted collapsed. Frozen contracts "
            "verified: no ungated hidden_vertex access, the non-collapsed path "
            "takes no new branch, sizeof(Group) stays 80 with no new "
            "unconditional Work spend, and the three fixtures are absent from "
            "the E6c 48-gap roster and every other cohort ledger. r2 APPROVE "
            "with no remaining MAJOR/MINOR/NIT."),
    }
    existing_path = repo_root / LEDGER
    if existing_path.exists():
        try:
            prior = json.loads(existing_path.read_text())
            if isinstance(prior.get("review"), dict):
                review = prior["review"]
        except json.JSONDecodeError:
            pass
    return review


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    # Drift guard: the corpus collapse set must equal the pinned partition.
    observed = corpus_collapsed_set(repo_root)
    expected = set(ROWS) | set(OUT_OF_SCOPE)
    if observed != expected:
        raise ValueError(
            f"collapsed-fixture partition drift:\n  observed={sorted(observed)}\n"
            f"  expected={sorted(expected)}")

    rows = []
    for rel, spec in ROWS.items():
        if rel not in manifest_rows:
            raise ValueError(f"ledger row missing from manifest: {rel}")
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        whole_draw = spec["scope"] == "whole-and-body"
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": spec["scope"],
            "expected_states": {
                "wide_240": spec["whole"][0],
                "narrow_40": spec["whole"][1],
                "restored_240": spec["whole"][2],
            },
            "expected_body_states": {
                "wide_240": spec["body"][0],
                "narrow_40": spec["body"][1],
                "restored_240": spec["body"][2],
            },
            "ledger_note": (
                "FS-7b collapsed-subgraph directive admitted; the group renders "
                "as a single titled rectangle"
                if whole_draw else
                "FS-7b admits the body only; " + spec["blocker"]
            ),
        })
    whole_draws = sum(1 for spec in ROWS.values()
                      if spec["scope"] == "whole-and-body")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-7b-collapsed-subgraph",
        "grammar_projection": {
            "collapse_directive": (
                "a trailing <id>@{ view: collapsed } statement collapses an "
                "already-declared, non-deferred group to one titled rectangle; "
                "descendant vertices/frames/internal edges are hidden and "
                "external edges terminate on the collapsed node"
            ),
            "slice_b_scope": (
                "external edges already target the group id; endpoint remap "
                "for hidden descendants is deferred to Slice C"
            ),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": (
                "the collapsed-subgraph originals whose sole body blocker is "
                "the unparsed view:collapsed directive and which need no "
                "endpoint remap"
            ),
            "roots": ["flowchart"],
            "out_of_scope": OUT_OF_SCOPE,
        },
        "review": build_review(repo_root),
        "counts": {
            "total": len(ROWS),
            "predicted_draw": len(ROWS),
            "predicted_blocked": 0,
            "predicted_original_draw": whole_draws,
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
