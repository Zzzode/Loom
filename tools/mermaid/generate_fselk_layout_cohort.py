#!/usr/bin/env python3
"""Generate the RFC 0031 FS-elk layout-engine cohort ledger.

The terminal renders every flowchart with its own fixed layout, so the
upstream layout *engine* selection is non-terminal: which engine positions
the nodes never changes the painted cell geometry. The flow envelope
whitelist already accepts `layout: dagre`; FS-elk extends it to the single
bare value `layout: elk.rectpacking`.

Why only rectpacking, not bare `elk` / `elk.box`: every corpus fixture that
selects those two values either (a) crosses the envelope double-parse Work
ceiling documented as FS-1b (the same body draws on the single-parse body
track but not through the frontmatter envelope path), or (b) carries a
subgraph or an unrelated body shape debt that opening the envelope would
surface as unreviewed art (a new matrix error). Admitting the engine key
blindly would therefore either change nothing or add an error. The
rectpacking fixture is the only one whose envelope is the sole blocker and
whose body draws with comfortable Work headroom (verified by adding
comments and nodes without crossing the ceiling). Quoted engine values,
the `elk:` option namespace, and any unknown engine stay fail-closed.

The generator is renderer-free; the durable gtest enforces every state
live. It pins the manifest sha, scans the whole flowchart tree for elk
layouts to pin the selectable/excluded partition, and preserves an
existing review block.

Usage: python3 tools/mermaid/generate_fselk_layout_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fselk-layout-cohort.json")

SCHEMA = "loom_mermaid_fselk_layout_cohort/v1"
ADMITTED_ENGINE = "elk.rectpacking"

PINNED = {
    "flowchart/nightly-check/elk-container-algorithms/"
    "elk-rectpacking-layout-with-edges.mmd",
}

# Every other flowchart elk-layout fixture stays whole-source. The two
# container siblings cross the envelope Work ceiling (FS-1b); the three
# subgraph fixtures are held by the untrusted-compound guard and/or body
# debts. Opening the engine key for them adds no draw and can add an error.
EXCLUDED = {
    "flowchart/nightly-check/elk-container-algorithms/"
    "elk-box-layout-with-edges.mmd",
    "flowchart/visual-check/"
    "should-keep-the-true-entry-on-top-when-a-back-edge-feeds-it-79.mmd",
    "flowchart/nightly-check/elk-container-algorithms/"
    "elk-subgraph-with-box-algorithm.mmd",
    "flowchart/pr-check/3-layout-elk-subgraphs.mmd",
    "flowchart/visual-check/subgraph-titles-wider-than-children.mmd",
}


def split_frontmatter(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None, text


def has_subgraph(body):
    return re.search(r"(?im)^\s*subgraph\b", body) is not None


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_review(repo_root):
    review = {
        "reviewer_agent": (
            "independent read-only adversarial review (fresh general-purpose "
            "agent, FS-elk layout-engine envelope cohort, 2026-11-09, task "
            "a66265d289108d9b1, zero builds)"),
        "date": "2026-11-09",
        "verdict": "approved",
        "note": (
            "APPROVE with no MAJOR. The whitelist admits exactly one bare "
            "value, layout: elk.rectpacking, behind the exact bit predicate "
            "fields == (Layout | ElkEngine); bare elk/elk.box, quoted and "
            "unknown engine values, the elk: option namespace and any "
            "co-key all fail closed, and no existing profile predicate can "
            "fire with the new bit set. The narrowing is evidence-driven: a "
            "broader whitelist added a matrix error on the subgraph-bearing "
            "3-layout-elk-subgraphs fixture and changed nothing for elk-box/"
            "back-edge-79, which stay source behind the FS-1b envelope "
            "double-parse Work ceiling (their bodies already draw on the "
            "single-parse track). Frozen contracts unmoved: E6c 48-gap, "
            "Class285 260/25/0, body 170/182/48; only one production file and "
            "the flow envelope path change, no new Work spend. All seven new "
            "Flow6 source shas were recomputed and match; count arithmetic "
            "243->242 / 157->158 keeps 242+158=400 with errors 0. MINORs "
            "resolved before approval: the drift-guard regex now also sees "
            "quoted elk values (safe miss direction regardless), the "
            "namespace_entry branch was returned to its original one-liner "
            "once the elk namespace was removed, and the folded control was "
            "restored to its original bare-elk row. The elk option-namespace "
            "closed control pairs with an elk engine because the namespace "
            "rule is pre-existing and independently correct."),
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

    # Drift guard: enumerate every flowchart fixture that selects an elk
    # engine in its frontmatter. The admitted set must be exactly the one
    # rectpacking row; every other elk fixture must be a documented exclusion.
    # A new subgraph-free rectpacking profile would trip this guard so it
    # cannot be admitted or dropped silently.
    seen = set()
    for path in sorted((repo_root / CASES / "flowchart").rglob("*.mmd")):
        frontmatter, body = split_frontmatter(path.read_text())
        if frontmatter is None or not re.search(r"layout:\s*[\"']?elk", frontmatter):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        seen.add(rel)
    expected_seen = PINNED | EXCLUDED
    if seen != expected_seen:
        raise ValueError(
            f"elk fixture partition drift:\n  got={sorted(seen)}\n"
            f"  expected={sorted(expected_seen)}")

    # The single admitted row must use the exact bare rectpacking value with
    # no other config leaf, and must not carry a subgraph (the compound guard
    # would close it anyway).
    rows = []
    for rel in sorted(PINNED):
        frontmatter, body = split_frontmatter((repo_root / CASES / rel).read_text())
        config_leaves = re.findall(r"^  ([A-Za-z_][\w.]*):\s*(.*?)\s*$",
                                   frontmatter, re.M)
        if config_leaves != [("layout", ADMITTED_ENGINE)]:
            raise ValueError(f"admitted row envelope changed: {config_leaves}")
        if has_subgraph(body):
            raise ValueError(f"admitted row unexpectedly carries a subgraph: {rel}")
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": "whole-and-body",
            "expected_states": {
                "wide_240": "drawn",
                # Measured live: the small rectpacking body fits and draws even
                # at 40 columns (unlike the larger spacing cohort rows).
                "narrow_40": "drawn",
                "restored_240": "drawn",
            },
            "expected_body_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn",
                "restored_240": "drawn",
            },
            "ledger_note": (
                "FS-elk: non-terminal layout-engine selection; the terminal "
                "uses its own fixed layout and ignores the upstream engine, so "
                "the whole original draws once the envelope accepts bare "
                "layout: elk.rectpacking"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "flowchart-elk-layout-envelope",
        "whitelist": {
            "engine_values": (
                "only bare layout: elk.rectpacking is admitted; bare elk, "
                "elk.box, quoted engine values, the elk: option namespace and "
                "unknown engines fail closed"),
            "effect": (
                "engine selection only drives upstream layout; no terminal "
                "rendering effect"),
            "fs1b_note": (
                "bare elk / elk.box fixtures stay source behind the envelope "
                "double-parse Work ceiling (FS-1b); their bodies already draw "
                "on the single-parse body track"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": (
                "the one flowchart fixture whose sole blocker is the "
                "elk.rectpacking engine envelope leaf and whose body draws "
                "with Work headroom and no subgraph"),
            "roots": ["flowchart"],
            "excluded": sorted(EXCLUDED),
        },
        "review": build_review(repo_root),
        "counts": {
            "total": len(rows),
            "predicted_draw": len(rows),
            "predicted_blocked": 0,
            "predicted_original_draw": len(rows),
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
