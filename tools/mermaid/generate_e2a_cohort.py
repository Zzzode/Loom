#!/usr/bin/env python3
"""Generate the RFC 0031 E2a pre-registration ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/{git,state-diagram,state-diagram-v2} roots that
    - opens with a column-0 ``---`` frontmatter fence,
    - declares no top-level ``config:`` key,
    - carries no ``%%{init...}%%`` directive in the body.

Each selected row is pre-registered as predicted-draw or predicted-blocked.
Four blocker classes come from fixed source-content detectors; two classes
(native parent-route debt and State composite execution debt) are independently
reviewed execution sets. D235 retired a fifth fixed detector,
gitgraph-empty-quoted-tag: an explicit empty quoted tag now parses to no label
and those three rows draw, guarded by the MermaidGitGraphEmptyTag native
gtests. D-435 retired the gitgraph-title-not-projected detector: the frontmatter
title is now projected as the frame's first row (the D-435 cohort ledger owns
that positive). State entries are bound to explicit path/SHA pairs;
new or changed next-line-brace sources require review. For GitGraph,
parse_gitgraph accepts the body but plan_parent_routes cannot close an edge. That set is sha-bound to
this pinned corpus and additions require a new review row. The original
16-row attested set was retired by the bounded strict-first parent-router
retry (14 rows now draw; the two LR 9-branch rows are capacity debt and
classify via the branch-count detector), so the reviewed set is currently
empty. The classification vocabulary is closed and enforced by the C++
cohort gate.

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
    "gitgraph-multiline-quoted-branch-label",
    "gitgraph-canvas-dimension-overflow",
    "gitgraph-native-parent-route-debt",
    "state-composite-brace-on-next-line",
    "state-composite-divider",
]

# Source-profile witnesses; remaining execution gaps are distinguished from grammar rejection.
WITNESS = {
    "gitgraph-multiline-quoted-branch-label": (
        "src/ui/visual/mermaid_gitgraph_impl.cpp:55-64",
        "Cursor::quoted scans one physical line; a quoted label split across lines cannot close.",
    ),
    "gitgraph-canvas-dimension-overflow": (
        "src/ui/visual/mermaid_gitgraph_impl.cpp:434-443 (checked_dimension); "
        ":457-637 (plan_parent_routes)",
        "With nine declared branches the TB/BT lane layout is 276 columns and is "
        "rejected by checked_dimension (kWidthLimit=240); the LR layouts are 218 columns "
        "but the bounded strict-first parent router (strict pass plus one relaxed "
        "whole-plan retry) still cannot close an edge in the packed ten-lane canvas.",
    ),
    "gitgraph-native-parent-route-debt": (
        "src/ui/visual/mermaid_gitgraph_impl.cpp:457-637 (plan_parent_routes)",
        "parse_gitgraph accepts the body but plan_parent_routes cannot close one edge "
        "in the bounded canvas, so layout_gitgraph returns nullopt. The 2026-10-24 "
        "attested set of 16 rows was retired by the strict-first retry: 14 draw through "
        "the relaxed pass and the two nine-branch LR rows are canvas capacity debt; the "
        "reviewed set is empty until a future corpus expansion attests new rows.",
    ),
    "state-composite-brace-on-next-line": (
        "src/ui/visual/mermaid_relations_core_impl.cpp (State composite parse and relations_layout)",
        "The historical next-line-brace cohort remains whole-scene execution debt: "
        "the State parser now accepts a standalone next-line opener, but the three "
        "complete pinned originals still fall back at 240 columns. This source-profile "
        "tag does not assert a lexical rejection or establish a layout root cause.",
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
# plan_parent_routes (parser accepts; the bounded parent router cannot close
# a parent edge for these interleaved branch/merge/cherry-pick graphs).
# These are not detectable by a lexical source pattern short of re-running the
# router, so they are recorded as a reviewed set; the C++ gate empirically
# enforces that each keeps whole source. Additions require a new review row.
#
# The 16 rows attested on 2026-10-24 were all retired by the strict-first
# parent-router retry (T1-A): the 14 interleaved branch/merge/cherry-pick
# rows now draw through the one bounded relaxed whole-plan pass, and the two
# LR nine-branch rows remain blocked as canvas capacity debt and classify
# through the > GIT_BRANCH_LIMIT detector below. The set is empty until a
# future corpus expansion attests genuinely new router debt.
NATIVE_ROUTE_DEBT: set[str] = set()

# Only these independently observed complete originals retain the historical
# next-line-brace tag. The grammar now accepts that opener; neither its presence
# nor a future source revision predicts a geometry failure without new review.
# D-461 retires the two rows whose only remaining blocker was the fixed 1M
# layout Work primary (the bounded canvas-derived state envelope funds them);
# they move to REVIEWED_BRACE_DRAW below. The `multiple-transitions` row stays:
# its blocker is routing geometry, not budget, and it is deferred to D-461
# stage 3.
STATE_COMPOSITE_EXECUTION_DEBT: dict[str, str] = {}

# D-461: reviewed next-line-brace originals admitted to draw (each sha-bound, so
# a source revision requires a new independent review). They are the two
# `should-render-edge-labels-correctly*` grids whose composite containers use
# the next-line-brace opener and whose whole render cost (1.66M-2.65M) exceeds
# the fixed 1M primary.
# D-461 measured narrow exception: this grid still draws at 40 columns (the
# narrow set is measured live, rule 34). The E2a ledger otherwise pre-registers
# only the 240-column admission.
NARROW_DRAW = {
    "state-diagram-v2/nightly-check/should-render-edge-labels-correctly-with-multiple-states.mmd",
}

REVIEWED_BRACE_DRAW = {
    # D-461 stage 3 retires the last routing-geometry entry: the crossing retry
    # closes the multiple-transitions fan-out.
    "state-diagram-v2/pr-check/should-render-edge-labels-correctly-with-multiple-transitions.mmd":
        "b6ef22abdcd251e866c6ab5a9ee7d68d98e1092a8d65f294de849d4aa3b68c4a",
    "state-diagram-v2/nightly-check/should-render-edge-labels-correctly-with-multiple-states.mmd":
        "1e359f909af210d7338db0674c6f86daaf25b5a3959bd70b0649e92ee0c8f3f3",
    "state-diagram-v2/nightly-check/should-render-edge-labels-correctly.mmd":
        "8f5f84b56734e20aa522613ae397585b37aaa248cae2880bd85bcfa39134733c",
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
    # D-435 retires the title detector: the frontmatter title is projected as
    # the frame's first row (positive owned by the D-435 cohort ledger).
    for line in body_lines:
        stripped = line.strip()
        if stripped.startswith("branch") and odd_quotes(line):
            return "gitgraph-multiline-quoted-branch-label"
    # D235: the explicit empty quoted tag (`tag: ""`) now parses natively via
    # Cursor::take_tag (an empty string carries no glyphs and projects to no
    # label, identical to an omitted tag), so the former empty-quoted-tag
    # detector is retired; its regression guard is MermaidGitGraphEmptyTag.
    # The execution-attested reviewed set takes precedence over the textual
    # branch-count detector when non-empty (it is currently empty; see the
    # NATIVE_ROUTE_DEBT note). The two LR 9-branch rows therefore classify as
    # canvas capacity debt here alongside the TB/BT 9-branch rows.
    if rel in NATIVE_ROUTE_DEBT:
        return "gitgraph-native-parent-route-debt"
    branches = sum(
        1 for line in body_lines
        if re.match(r"^\s*branch\b", line) and not line.lstrip().startswith("//")
    )
    if branches > GIT_BRANCH_LIMIT:
        return "gitgraph-canvas-dimension-overflow"
    return None


def state_blocker(rel, source_sha256, body_lines):
    if rel in STATE_COMPOSITE_EXECUTION_DEBT:
        if source_sha256 != STATE_COMPOSITE_EXECUTION_DEBT[rel]:
            raise ValueError(f"State execution-debt source changed; independent review required: {rel}")
        return "state-composite-brace-on-next-line"
    if rel in REVIEWED_BRACE_DRAW:
        if source_sha256 != REVIEWED_BRACE_DRAW[rel]:
            raise ValueError(
                f"Reviewed next-line-brace source changed; independent review required: {rel}")
        return None
    if any(line.strip() == "{" for line in body_lines):
        raise ValueError(f"Unreviewed next-line-brace source; independent review required: {rel}")
    # RFC 0048 retires the standalone-divider blocker: a concurrent-region
    # body now parses and draws (the reviewed cohort is the RFC 0048 ledger).
    # Region-local shapes that still fail closed (cross-region edges, empty
    # regions, a direction after a divider, more than eight regions) are
    # guarded by the renderer's own gates, not by a source-shape detector.
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
                        else state_blocker(rel, sha256_file(path), body))
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
                    "narrow_40": "drawn" if entry["path"] in NARROW_DRAW else "source",
                    "restored_240": wide,
                },
                "blocker": (
                    {"kind": kind, "site": site, "evidence": site, "detail": detail}
                    if blocked else None),
                "ledger_note": (
                    "E2a description-only admission" if not blocked else
                    "Pending complete-original execution; no grammar/layout root cause claimed"
                    if kind == "state-composite-brace-on-next-line" else
                    "Blocked by a non-envelope body-grammar or capacity witness"),
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
            "blocker_detection": "three fixed source-content detectors plus three execution-attested reviewed sets (native parent-route debt, path/SHA-bound State composite debt, and the path/SHA-bound next-line-brace draw witnesses D-461 admits); no generator-time rendering; new execution-debt entries require independent review. D235 retired the empty-quoted-tag detector (empty tag now parses to no label, guarded by MermaidGitGraphEmptyTag); D-435 retired the title detector (the frontmatter title is projected as the frame's first row); D-461 retired the two budget-blocked next-line-brace draws.",
            "width_policy": "E2a pre-registers the 240-column admission; narrow_40 is 'source' except the one D-461-measured grid that also draws at 40.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial D235 re-review "
                              "(fresh general-purpose agent; full-diff audit, independent "
                              "14-case dual-seam probe, own whole-corpus HEAD-vs-working A/B, "
                              "frozen-invariant verification); prior agent:rfc0031-e2a-ledger-"
                              "review covered the 56/13 partition",
            "date": "2026-11-14",
            "verdict": "approved",
            "note": "D235 retires the gitgraph-empty-quoted-tag detector: Cursor::take_tag "
                    "now accepts an explicit empty quoted tag (tag: \"\") which carries no "
                    "glyphs and projects to no label, identical to an omitted tag. Three "
                    "gitGraph cherry-pick originals (two TB/LR orientations and a BT) move to "
                    "predicted-draw, guarded by MermaidGitGraphEmptyTag native gtests. The "
                    "other six blockers and all frozen gates remain unchanged. Approved "
                    "59/10 partition. D-461 (2026-11-24) retires the two "
                    "should-render-edge-labels-correctly* next-line-brace rows: their only "
                    "remaining blocker was the fixed 1M layout Work primary, now funded by "
                    "the bounded canvas-derived state envelope (see "
                    "tools/mermaid/state-d461-cohort.json and test_mermaid_state_d461_cohort). "
                    "D-461 stage 3's crossing retry then admits the multiple-transitions "
                    "fan-out too, so no row carries the brace tag any more.",
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
