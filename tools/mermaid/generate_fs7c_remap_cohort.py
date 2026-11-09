#!/usr/bin/env python3
"""Generate the RFC 0031 FS-7c collapsed-subgraph endpoint-remap ledger.

Slice C of the collapsed-subgraph work. Slice B (fs7b-collapsed-cohort)
admits collapse cases whose external edges already target the group id.
These two fixtures instead have an edge whose endpoint names a HIDDEN
descendant:

- a vertex nested inside the collapsed group, or
- a non-collapsed nested group swallowed by the collapsed ancestor.

The flow_layout projection layer remaps such an endpoint onto the
collapsed ancestor's own Group endpoint, so the edge terminates on the
single collapsed rectangle rather than a hidden node. The same remap also
lets the four redux-color-subgraphs bodies draw (Slice B fail-closed them
on the hidden endpoint), so the cohort has six rows:

- two description-only visual-check fixtures, whole-and-body;
- four redux-theme fixtures, body-only — their whole originals stay
  source behind the `theme: redux` frontmatter gate.

The generator is renderer-free; the durable gtest enforces every state
live. It pins manifest shas, asserts its rows are exactly the full
out-of-scope set declared by the FS-7b ledger (no double ownership), and
preserves an existing review block.

Usage: python3 tools/mermaid/generate_fs7c_remap_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs7c-remap-cohort.json")
FS7B_LEDGER = pathlib.Path("tools/mermaid/fs7b-collapsed-cohort.json")

SCHEMA = "loom_mermaid_fs7c_collapsed_remap_cohort/v1"

# Slice C remap also unlocks the four redux-color-subgraphs bodies: Slice B
# fail-closed them on the single hidden endpoint, and once remap exists their
# body renders the same correct collapse geometry. Their WHOLE originals stay
# source behind the `theme: redux` frontmatter gate, so they are body-only.
#
# path -> dict(scope, note, whole, body) state triples
ROWS = {
    "flowchart/visual-check/should-redirect-an-edge-targeting-a-deeply-nested-node-to-the-outermost-collapsed-subgraph.mmd":
        {"scope": "whole-and-body",
         "note": "a vertex endpoint nested inside the collapsed group is "
                 "remapped to the collapsed ancestor Group endpoint",
         "whole": ("drawn", "drawn", "drawn"),
         "body": ("drawn", "drawn", "drawn")},
    "flowchart/visual-check/should-collapse-nested-subgraphs-to-the-outermost-collapsed-ancestor.mmd":
        {"scope": "whole-and-body",
         "note": "a nested-group endpoint swallowed by the collapsed ancestor "
                 "is remapped to the collapsed ancestor Group endpoint",
         "whole": ("drawn", "drawn", "drawn"),
         "body": ("drawn", "drawn", "drawn")},
    "flowchart/nightly-check/redux-color-subgraphs/03-should-render-collapsed-subgraph-containers.mmd":
        {"scope": "body-only",
         "note": "remap lets the body draw the collapse geometry; whole "
                 "original stays source behind the theme: redux frontmatter",
         "whole": ("source", "source", "source"),
         "body": ("drawn", "drawn", "drawn")},
    "flowchart/nightly-check/redux-color-subgraphs/07-should-render-collapsed-subgraph-containers.mmd":
        {"scope": "body-only",
         "note": "remap lets the body draw the collapse geometry; whole "
                 "original stays source behind the theme: redux frontmatter",
         "whole": ("source", "source", "source"),
         "body": ("drawn", "drawn", "drawn")},
    "flowchart/nightly-check/redux-color-subgraphs/11-should-render-collapsed-subgraph-containers.mmd":
        {"scope": "body-only",
         "note": "remap lets the body draw the collapse geometry; whole "
                 "original stays source behind the theme: redux frontmatter",
         "whole": ("source", "source", "source"),
         "body": ("drawn", "drawn", "drawn")},
    "flowchart/nightly-check/redux-color-subgraphs/15-should-render-collapsed-subgraph-containers.mmd":
        {"scope": "body-only",
         "note": "remap lets the body draw the collapse geometry; whole "
                 "original stays source behind the theme: redux frontmatter",
         "whole": ("source", "source", "source"),
         "body": ("drawn", "drawn", "drawn")},
}

DIRECTIVE_RE = re.compile(
    r'^\s*[A-Za-z_][\w-]*@\{\s*view\s*:\s*(?:"collapsed"|collapsed)\s*\}\s*$')


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def has_directive(repo_root, rel):
    text = (repo_root / CASES / rel).read_text()
    return any(DIRECTIVE_RE.match(line) for line in text.splitlines())


def build_review(repo_root):
    review = {
        "reviewer_agent": (
            "independent read-only adversarial review (fresh general-purpose "
            "agent, FS-7c collapsed endpoint remap, 2026-11-09, task "
            "ae53da6338bbf34d2, zero builds); r1 approved the 2-fixture remap, "
            "r2 approved the 6-row body-only expansion"),
        "date": "2026-11-09",
        "verdict": "approved",
        "note": (
            "r1 (2 rows) approved the projection that rewrites a hidden "
            "vertex/nested-group endpoint onto the collapsed ancestor Group "
            "endpoint, with the tagged router routing the synthetic Edge so "
            "Group-kind sealing applies while rich-label binding keeps the "
            "original index. r2 confirmed the expansion to six rows: the same "
            "remap unlocks the four redux-color-subgraphs BODIES, which are "
            "body-only because the flow envelope whitelist has no theme key and "
            "the terminal has no redux color contract, so the whole originals "
            "stay source with no loss of expressible semantics. Findings "
            "closed before approval: remapped self-connections are dropped only "
            "when an endpoint was actually remapped (an authored self-loop on a "
            "fully visible vertex is preserved, locked by a regression test); "
            "double-collapse stays fail-closed (documented known limit); the "
            "RoutedEdge Edge-copy budget increase on group-endpoint graphs is "
            "re-verified by dual-preset exact-budget gates. Frozen contracts "
            "unmoved: E6c 48-gap, Class285, sizeof(Group)==80, no new "
            "unconditional Work spend, non-collapsed graphs take the original "
            "path, and the ledger owns exactly the FS-7b out-of-scope set with "
            "no double ownership. No MAJOR/MINOR left open at approval."),
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

    # Ownership guard: FS-7c now owns every fixture the FS-7b ledger declares
    # out-of-scope (the two Slice C remap rows plus the four redux rows that
    # remap also unlocks on the body track), so the two cohorts never
    # double-own a fixture and a new collapsed fixture trips the guard.
    fs7b = json.loads((repo_root / FS7B_LEDGER).read_text())
    fs7b_out_of_scope = set(fs7b["selection"]["out_of_scope"].keys())
    if set(ROWS) != fs7b_out_of_scope:
        raise ValueError(
            "FS-7c rows must equal the full FS-7b out-of-scope set:\n"
            f"  fs7c={sorted(ROWS)}\n  fs7b_out_of_scope={sorted(fs7b_out_of_scope)}")

    rows = []
    whole_draws = 0
    for rel, spec in ROWS.items():
        if rel not in manifest_rows:
            raise ValueError(f"ledger row missing from manifest: {rel}")
        if not has_directive(repo_root, rel):
            raise ValueError(f"row no longer carries a collapse directive: {rel}")
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        whole_draw = spec["scope"] == "whole-and-body"
        whole_draws += int(whole_draw)
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
            "ledger_note": "FS-7c: " + spec["note"],
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-7c-collapsed-endpoint-remap",
        "grammar_projection": {
            "endpoint_remap": (
                "a hidden vertex or nested-group endpoint is rewritten to the "
                "collapsed ancestor's Group endpoint; same-leaf internal edges "
                "and remapped self-connections are dropped and different-leaf "
                "edges stay fail-closed"
            ),
            "known_limit_double_collapse": (
                "when both an outer and a nested inner group carry collapse "
                "directives, the outer layout short-circuit leaves the inner "
                "group without a region box, so an edge remapped onto it fails "
                "closed to whole source; supporting it would require remapping "
                "to the outermost box-owning collapsed ancestor (no corpus "
                "fixture uses double collapse)"
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
                "collapsed-subgraph originals whose external edge targets a "
                "hidden descendant and needs collapsed-ancestor remap"
            ),
            "roots": ["flowchart"],
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
