#!/usr/bin/env python3
"""Generate the RFC 0031 FS-7a subgraph-grammar cohort ledger.

Two parser gaps kept subgraph fixtures on ordinary fallback even when
every other grammar feature they use was already admitted:

1. Direction-less header. A `flowchart`/`graph` header with no direction
   defaults to TB (Direction::Down), the same layout input as an explicit
   TD header, but parse() rejected any group container under that profile.
2. Forward group references. An edge may name a subgraph before its
   `subgraph` declaration (mermaid/dagre resolve cluster edges after the
   full parse). The parser now pre-registers every fixed-id subgraph as a
   deferred shell; the shell is activated, never left deferred, when its
   declaration is reached.

The seven unlocked corpus rows all draw on the BODY track. Three also
draw as whole originals because their frontmatter already matches an
admission profile (description-only, or the layout: dagre envelope); the
other four stay whole-source pending behind independent, unchanged
gates:

- the 7827 recursive-flow row exhausts the 1,000,000 Work ceiling
  (loom::ui::mermaid::kWorkLimit) on the whole-source envelope render
  path. Every frontmatter key it uses is individually admitted and its
  bare body draws, so no grammar or config gate remains — only the
  render budget, the deferred FS-1b gas-meter recalibration;
- the two 3258 spacing rows carry flowchart.nodeSpacing/rankSpacing,
  which has no whole-source admission profile;
- the self-loops row carries flowchart.subGraphTitleMargin, likewise
  without an admission profile.

The generator preserves an existing ledger's review block when one is
present, so re-running it after approval cannot silently reset the
verdict; a fresh ledger defaults to REVIEW_PENDING.

Usage: python3 tools/mermaid/generate_fs7a_subgraph_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs7a-subgraph-cohort.json")

SCHEMA = "loom_mermaid_fs7a_subgraph_cohort/v1"

# path -> ("whole-and-body" | "body-only", whole-source blocker note or None)
ROWS = {
    "flowchart/nightly-check/dagre/3-dagre-a-link-with-correct-arrowhead-to-a-subgraph.mmd":
        ("whole-and-body", None),
    "flowchart/visual-check/3-a-link-with-correct-arrowhead-to-a-subgraph.mmd":
        ("whole-and-body", None),
    "flowchart/visual-check/5059-should-render-when-subgraph-contains-only-subgraphs-has-link-to-outside-and-itself-is-part-of-a-link.mmd":
        ("whole-and-body", None),
    "flowchart/nightly-check/dagre/dagre-should-keep-the-entry-node-on-top-of-a-recursive-flow-nested-in-a-subgraph-with-dagre-keepentrynodeontop-true-7827.mmd":
        ("body-only",
         "whole-source envelope render rejects at the 1,000,000 Work "
         "ceiling (kWorkLimit); all its frontmatter keys are admitted and "
         "the bare body draws, so only the deferred FS-1b gas-meter "
         "recalibration remains (also listed in "
         "docs/dev/mermaid-unsuccessful-cases.md)"),
    "flowchart/visual-check/3258-should-render-subgraphs-with-large-nodespacing-and-rankspacing.mmd":
        ("body-only",
         "whole source rejected: flowchart.nodeSpacing/rankSpacing plus a "
         "diagram title match no whole-source admission profile"),
    "flowchart/visual-check/3258-should-render-subgraphs-with-main-graph-nodespacing-and-rankspacing.mmd":
        ("body-only",
         "whole source rejected: flowchart.nodeSpacing/rankSpacing plus a "
         "diagram title match no whole-source admission profile"),
    "flowchart/visual-check/should-render-self-loops.mmd":
        ("body-only",
         "whole source rejected: flowchart.subGraphTitleMargin has no "
         "whole-source admission profile"),
}


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    for rel, (scope, blocker) in ROWS.items():
        if rel not in manifest_rows:
            raise ValueError(f"ledger row missing from manifest: {rel}")
        observed = sha256_file(repo_root / CASES / rel)
        if observed != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        whole_draw = scope == "whole-and-body"
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": observed,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": scope,
            "expected_states": {
                "wide_240": "drawn" if whole_draw else "source",
                "narrow_40": "source",
                "restored_240": "drawn" if whole_draw else "source",
            },
            "expected_body_states": {
                "wide_240": "drawn",
                "narrow_40": "source",
                "restored_240": "drawn",
            },
            "ledger_note": (
                "FS-7a subgraph grammar (direction-less header and forward "
                "group references) admitted"
                if whole_draw else
                "FS-7a subgraph grammar admits the body only; " + blocker
            ),
        })
    whole_draws = sum(1 for scope, _ in ROWS.values() if scope == "whole-and-body")
    review = {
        "reviewer_agent": (
            "independent read-only adversarial review (fresh "
            "general-purpose agent, FS-7a subgraph grammar, 2026-11-08)"),
        "date": "2026-11-08",
        "verdict": "REVIEW_PENDING",
        "note": "Filled by the independent review before family closure.",
    }
    # Preserve a recorded review across regeneration so re-running the
    # generator cannot silently reset an approved verdict.
    existing_path = repo_root / LEDGER
    if existing_path.exists():
        try:
            prior = json.loads(existing_path.read_text())
            if isinstance(prior.get("review"), dict):
                review = prior["review"]
        except json.JSONDecodeError:
            pass
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-7a-subgraph-grammar",
        "grammar_projection": {
            "direction_less_header": (
                "flowchart/graph without a direction defaults to TB; group "
                "containers are byte-identical to an explicit TD header"
            ),
            "forward_group_reference": (
                "fixed-id subgraphs are pre-registered as deferred shells so "
                "edges may target a group before its declaration; closed "
                "group-endpoint grammar (scalar, solid, one-way, unlabelled, "
                "length one) is unchanged"
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
                "the subgraph fallback originals whose sole body blocker is "
                "the direction-less-header guard or a forward group reference"
            ),
            "roots": ["flowchart"],
        },
        "review": review,
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
