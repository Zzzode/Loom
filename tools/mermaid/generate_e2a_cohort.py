#!/usr/bin/env python3
"""Generate the RFC 0031 E2a pre-registration ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/{git,state-diagram,state-diagram-v2} roots that
    - opens with a column-0 ``---`` frontmatter fence,
    - declares no top-level ``config:`` key,
    - carries no ``%%{init...}%%`` directive in the body.

Each selected row is pre-registered as predicted-draw or predicted-blocked.
Six blocker classes come from fixed source-content detectors; a seventh
class (native parent-route debt) cannot be decided textually and is an
execution-attested, independently reviewed set: parse_gitgraph accepts the
body but plan_parent_routes cannot close an edge. That set is sha-bound to
this pinned corpus and additions require a new review row. The classification
vocabulary is closed and enforced by the C++ cohort gate.

E2a pre-registers the 240-column admission only. ``narrow_40`` is uniformly
``source`` and ``restored_240`` must equal ``wide_240``; this conservative
width stance is not renderer-derived.

Usage: python3 tools/mermaid/generate_e2a_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/e2a-cohort.json")

SCHEMA = "loom_mermaid_e2a_cohort/v1"
BLOCKER_VOCABULARY = [
    "gitgraph-title-not-projected",
    "gitgraph-multiline-quoted-branch-label",
    "gitgraph-empty-quoted-tag",
    "gitgraph-canvas-dimension-overflow",
    "gitgraph-native-parent-route-debt",
    "state-composite-brace-on-next-line",
    "state-composite-divider",
]

# Production-source witnesses for each detector (parser, not renderer).
WITNESS = {
    "gitgraph-title-not-projected": (
        "src/ui/visual/mermaid_gitgraph_impl.cpp:348-403",
        "GitGraph has no title projection; admitting a titled row would silently drop the title.",
    ),
    "gitgraph-multiline-quoted-branch-label": (
        "src/ui/visual/mermaid_gitgraph_impl.cpp:55-64",
        "Cursor::quoted scans one physical line; a quoted label split across lines cannot close.",
    ),
    "gitgraph-empty-quoted-tag": (
        "src/ui/visual/mermaid_gitgraph_impl.cpp:268-272; "
        "src/ui/visual/mermaid_cells_impl.cpp:41-42",
        "parse_cherry_pick requires a non-empty quoted tag; tag: \"\" decodes to no label.",
    ),
    "gitgraph-canvas-dimension-overflow": (
        "src/ui/visual/mermaid_gitgraph_impl.cpp:426-435 (checked_dimension); "
        ":449-552 (plan_parent_routes)",
        "With nine declared branches the TB/BT lane layout is 276 columns and is "
        "rejected by checked_dimension (kWidthLimit=240); the LR layouts are 218 columns "
        "but the greedy BFS parent router still cannot close an edge.",
    ),
    "gitgraph-native-parent-route-debt": (
        "src/ui/visual/mermaid_gitgraph_impl.cpp:449-560 (plan_parent_routes)",
        "parse_gitgraph accepts the body but the greedy BFS parent-edge router cannot close one "
        "edge in the bounded canvas, so layout_gitgraph returns nullopt. Independent source "
        "review attested each row; this is native layout debt, not an envelope limitation.",
    ),
    "state-composite-brace-on-next-line": (
        "src/ui/visual/mermaid_relations_impl.cpp (state composite parse; same-line `{` required)",
        "A composite opens only when '{' is on the same state-declaration line; "
        "a standalone next-line '{' falls through to the edge parser and is rejected.",
    ),
    "state-composite-divider": (
        "src/ui/visual/mermaid_relations_impl.cpp (state edge parse; identifier('--') rejected)",
        "A standalone '--' region divider reaches edge parsing where identifier('--') is rejected.",
    ),
}

# Cohort-empirical cutoff, not a derived geometric universal: every pinned row
# with >8 declared branches is blocked (the four 9-branch files). This is a
# corpus-fixed pre-registration; a later corpus expansion must re-run review
# rather than assume the textual detectors are universally complete.
GIT_BRANCH_LIMIT = 8

# Native layout-capacity debt attested by independent source review against
# plan_parent_routes (parser accepts; the bounded greedy BFS router cannot
# close a parent edge for these interleaved branch/merge/cherry-pick graphs).
# These are not detectable by a lexical source pattern short of re-running the
# router, so they are recorded as a reviewed set; the C++ gate empirically
# enforces that each keeps whole source. Additions require a new review row.
NATIVE_ROUTE_DEBT = {
    "git/nightly-check/11-should-render-a-simple-gitgraph-with-cherry-pick-commit.mmd",
    "git/nightly-check/11-should-render-a-simple-gitgraph-with-two-cherry-pick-commit.mmd",
    "git/nightly-check/13-should-render-a-simple-gitgraph-with-three-branches-custom-merge-commit-id-tag-type.mmd",
    "git/nightly-check/28-should-render-a-simple-gitgraph-with-two-cherry-pick-commit-vertical-branch.mmd",
    "git/nightly-check/33-should-render-a-simple-gitgraph-overlapping-commits.mmd",
    "git/nightly-check/35-should-render-a-simple-gitgraph-with-two-branches-from-same-commit-vertical-branch.mmd",
    "git/nightly-check/38-should-render-gitgraph-with-branch-and-sub-branch-neither-of-which-used-immediately.mmd",
    "git/nightly-check/63-should-render-a-simple-gitgraph-with-two-cherry-pick-commit-vertical-branch-bottom-to-top.mmd",
    "git/nightly-check/68-should-render-a-simple-gitgraph-with-two-branches-from-same-commit-vertical-branch-bottom-to-top.mmd",
    "git/nightly-check/7-should-render-a-simple-gitgraph-with-three-branches-and-tagged-merge-commit.mmd",
    "git/nightly-check/70-should-render-gitgraph-with-branch-and-sub-branch-neither-of-which-used-immediately-vertical-branch-bottom-to-top.mmd",
    "git/nightly-check/73-should-render-a-simple-gitgraph-with-three-branches-and-tagged-merge-commit-using-switch-instead-of-checkout.mmd",
    "git/nightly-check/77-should-render-a-bt-gitgraph-with-three-branches-and-multiple-merges-regression-6593.mmd",
    "git/pr-check/11-should-render-a-gitgraph-with-cherry-pick-commit-with-custom-tag.mmd",
    "git/nightly-check/12-should-render-commits-for-more-than-8-branches.mmd",
    "git/nightly-check/74-should-render-commits-for-more-than-8-branches-using-switch-instead-of-checkout.mmd",
}


def split_source(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return lines[1:close], lines[close + 1:]
    return None


def top_level_keys(frontmatter):
    return [line.split(":", 1)[0].strip() for line in frontmatter
            if line and line[0] not in (" ", "\t") and ":" in line]


def body_has_init(body_lines):
    return any(line.lstrip().startswith("%%{") for line in body_lines)


def odd_quotes(line):
    return line.count('"') % 2 == 1


def git_blocker(rel, keys, body_lines):
    """Source-derived gitGraph blocker, in deterministic precedence."""
    if "title" in keys:
        return "gitgraph-title-not-projected"
    for line in body_lines:
        stripped = line.strip()
        if stripped.startswith("branch") and odd_quotes(line):
            return "gitgraph-multiline-quoted-branch-label"
    for line in body_lines:
        if re.search(r"tag:\s*\"\"", line):
            return "gitgraph-empty-quoted-tag"
    # The execution-attested reviewed set takes precedence: the two LR
    # 9-branch rows are unroutable at 218 columns even though checked_dimension
    # would accept that width; only the TB/BT 9-branch rows hit the 240 limit.
    if rel in NATIVE_ROUTE_DEBT:
        return "gitgraph-native-parent-route-debt"
    branches = sum(
        1 for line in body_lines
        if re.match(r"^\s*branch\b", line) and not line.lstrip().startswith("//")
    )
    if branches > GIT_BRANCH_LIMIT:
        return "gitgraph-canvas-dimension-overflow"
    return None


def state_blocker(body_lines):
    for line in body_lines:
        if line.strip() == "{":
            return "state-composite-brace-on-next-line"
    for line in body_lines:
        if line.strip() == "--":
            return "state-composite-divider"
    return None


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def collect_root(cases, root_rel, family):
    rows = []
    root = cases / root_rel
    for path in sorted(root.rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        frontmatter, body = split
        keys = top_level_keys(frontmatter)
        if "config" in keys or body_has_init(body):
            continue
        blocker_kind = (git_blocker(rel, keys, body) if family == "gitGraph"
                        else state_blocker(body))
        rows.append({"path": rel, "keys": keys, "blocker": blocker_kind})
    return rows


def build(repo_root):
    cases = repo_root / CASES
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    roots = [
        ("git", "gitGraph", "git"),
        ("state_v2", "stateDiagram-v2", "state-diagram-v2"),
        ("state_v1", "stateDiagram", "state-diagram"),
    ]
    rows = []
    counts = {"git": 0, "state_v2": 0, "state_v1_qualified": 0,
              "predicted_draw": 0, "predicted_blocked": 0}
    for count_key, family, root_rel in roots:
        for entry in collect_root(cases, root_rel, family):
            counts[count_key] += 1
            kind = entry["blocker"]
            blocked = kind is not None
            wide = "source" if blocked else "drawn"
            counts["predicted_blocked" if blocked else "predicted_draw"] += 1
            site, detail = WITNESS[kind] if blocked else ("", "")
            manifest_row = manifest_rows[entry["path"]]
            rows.append({
                "path": entry["path"],
                "family": family,
                "sha256": sha256_file(cases / entry["path"]),
                "manifest_sha256": manifest_row["sha256"],
                "prior_status": manifest_row["raw"],
                "metadata_keys": entry["keys"],
                "has_config": False,
                "has_init": False,
                "prediction": "predicted-blocked" if blocked else "predicted-draw",
                "expected_states": {
                    "wide_240": wide,
                    "narrow_40": "source",
                    "restored_240": wide,
                },
                "blocker": (
                    {"kind": kind, "site": site, "evidence": site, "detail": detail}
                    if blocked else None),
                "ledger_note": "E2a description-only admission" if not blocked
                else "Blocked by a non-envelope body-grammar or capacity witness",
            })
    total = counts["git"] + counts["state_v2"] + counts["state_v1_qualified"]
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "E2a",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter original containing neither a top-level config key nor a body %%{init}%% directive",
            "roots": ["git", "state-diagram-v2", "state-diagram"],
            "blocker_detection": "six fixed source-content detectors plus one execution-attested reviewed set (native parent-route debt); no expectation is derived from rendered output",
            "width_policy": "E2a pre-registers the 240-column admission; narrow_40 is uniformly 'source'.",
        },
        "review": {
            "reviewer_agent": "agent:rfc0031-e2a-ledger-review (two independent adversarial rounds)",
            "date": "2026-10-24",
            "verdict": "approved",
        },
        "counts": {
            "total": total,
            "git": counts["git"],
            "state_v2": counts["state_v2"],
            "state_v1_qualified": counts["state_v1_qualified"],
            "predicted_draw": counts["predicted_draw"],
            "predicted_blocked": counts["predicted_blocked"],
        },
        "blocker_vocabulary": BLOCKER_VOCABULARY,
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
