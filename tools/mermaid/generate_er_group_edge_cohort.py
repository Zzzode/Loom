#!/usr/bin/env python3
"""Generate the D295 ER subgraph group-edge admission ledger.

Why a separate ledger (and NOT an ER107 kPins extension): the six draw cases
carry the ER206 debt ``raw_entity_group_kind_collision_not_representable_
without_loss``, not ``active_config``. The ER107 kPins cohort is derived from
the sha-frozen ER206 prior by the exact predicate ``whole_model is None AND
debts == {active_config}`` (see tools/mermaid/verify_er107_admission.py), which
selects precisely the existing 107 and excludes every group-edge case. These
rows therefore cannot join kPins without rewriting a protected, cross-gate
authority. They are admitted through their own small, independently reviewed
cohort instead. The ER107 active_config pins, its three proposal JSON files and
the ER206 prior stay byte-for-byte unchanged.

Why the two parser forms are safe: ``parse_plain_er`` already projects
subgraph containers, nested groups, quoted group titles, top-level
``direction`` and entity-to-entity edges. D293 admits two more grammar forms
the upstream corpus uses to address a subgraph as a whole:

  * a relationship whose endpoint is a subgraph id (plain or quoted, with or
    without a separate ``["title"]``), and
  * a ``direction`` override nested inside a subgraph.

A group id is a valid relationship endpoint (the bounded router attaches the
edge to the enclosing group box it already measures), and an in-group
``direction`` is consumed as a non-terminal directive because the terminal ER
layout owns its own fixed arrangement; only the top-level direction sets
``diagram.direction``. No edge, node, label, cardinality marker or scope is
dropped.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under cases/er-diagram that declares at
  least one ``subgraph`` and carries a relationship line whose endpoint
  resolves to a declared subgraph id. The selector is renderer-free and
  reproduces exactly seven originals.

One live outcome, renderer-free:
  predicted-draw      - all seven rows whose whole original completes through
                        the bounded ER renderer at 240 columns once group
                        endpoints are admitted. RFC 0034 retired the former
                        handDrawn Work-ceiling exclusion (see below), so no row
                        is predicted-blocked any more and every live ``blocker``
                        is null.

Historical retired blocker (audit only, never a live exclusion):
  retired-blocker      - one SHA-pinned row (the handDrawn nested-direction
                        variant) used to stay whole-source on the envelope
                        double-parse Work ceiling: its envelope adds a ``look``
                        event on top of the most complex body in the group, so
                        the second whole-source parse exhausted the scene
                        budget before layout. RFC 0034's bounded two-pool
                        layout Work allowance (kErEnvelopeLayoutAllowance) now
                        funds that over-budget active-ER layout, so the row
                        draws; the ceiling survives only as the row's
                        ``retired_blocker`` record (retired_by "RFC 0034"). It
                        stays path/SHA bound because the budget fit is not
                        textually decidable; any addition or source change
                        fails this generator and needs a new independent review.

Per-width states are geometry facts enforced live by
tests/ui/test_mermaid_er_group_edge_cohort.cpp; ``restored_240`` always
equals ``wide_240``. Narrow (40-column) states are measured, never assumed.

Usage: python3 tools/mermaid/generate_er_group_edge_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/er-diagram")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/er-group-edge-cohort.json")

SCHEMA = "loom_mermaid_er_group_edge_cohort/v1"
BLOCKER_KIND = "er-envelope-double-parse-work-ceiling"
ROOT_REL = "er-diagram"

# Hard drift guards. A corpus change that alters the selected population or a
# pinned source makes the generator abort instead of silently re-bucketing.
EXPECTED_TOTAL = 7
# RFC 0034 retired the single handDrawn Work-ceiling exclusion: the bounded
# two-pool layout Work allowance lets that whole source draw, so no row is
# predicted-blocked any more. The historical ceiling is retained as an explicit
# retired-blocker audit record (one row), never as a live exclusion.
EXPECTED_BLOCKED = 0
EXPECTED_DRAW = 7
EXPECTED_RETIRED = 1

# A relationship cardinality marker, covering solid (--) and dotted (..) ER
# spellings. Only used to tell relationship lines from other statements.
_CARDINALITY = re.compile(r"(\|\|\.\.|\.\.\|\||\|\|--|--\|\||\}o--|o\{--|--o\{|"
                          r"\}o\.\.|\.\.o\{|\|\|--o\{|--|\.\.)")


def sha256_file(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def split_source(text: str):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return lines[1:close], "\n".join(lines[close + 1:])
    return None, text


def subgraph_ids(body: str):
    """Declared subgraph ids: quoted multi-word or bare identifier, ignoring
    any separate [\"title\"] suffix."""
    ids = set()
    for line in body.split("\n"):
        stripped = line.strip()
        if not stripped.startswith("subgraph"):
            continue
        rest = stripped[len("subgraph"):].strip()
        if rest.startswith('"'):
            match = re.match(r'"([^"]+)"', rest)
            if match:
                ids.add(match.group(1))
        else:
            match = re.match(r"([A-Za-z_]\w*)", rest)
            if match:
                ids.add(match.group(1))
    return ids


def relationship_references_group(body: str, group_ids) -> bool:
    for line in body.split("\n"):
        stripped = line.strip()
        if not _CARDINALITY.search(stripped):
            continue
        head = stripped.split(":", 1)[0]  # drop the role label
        for gid in group_ids:
            if f'"{gid}"' in head or re.search(
                    r'(?<![\w"])' + re.escape(gid) + r'(?![\w"])', head):
                return True
    return False


def collect(repo_root: pathlib.Path):
    selected = []
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        text = path.read_text()
        _, body = split_source(text)
        if "subgraph" not in body:
            continue
        group_ids = subgraph_ids(body)
        if not group_ids or not relationship_references_group(body, group_ids):
            continue
        selected.append(path.relative_to(
            repo_root / "tests/fixtures/mermaid_official/cases").as_posix())
    return selected


# Historical SHA-bound whole-source ceiling, RETIRED by RFC 0034. The body
# always drew; the whole original (frontmatter ``look: handDrawn`` plus the
# most complex nested body) used to exhaust the scene Work budget on the second
# envelope parse. RFC 0034's bounded two-pool layout Work allowance
# (kErEnvelopeLayoutAllowance) funds the over-budget layout, so the whole source
# now draws. The ceiling fact is retained here only as a retired-blocker audit
# record on the row; it is no longer a live exclusion and never re-buckets the
# prediction (the row is predicted-draw).
RETIRED_BLOCKER_CAPABILITY_SET = {
    "er-diagram/visual-check/nested-er-subgraphs-with-direction-override-handdrawn-look.mmd": (
        "er-envelope-double-parse-work-ceiling",
        "body completes after the D293 group-endpoint admission, but the whole original "
        "adds a look:handDrawn envelope event on top of the deepest nested body, and the "
        "second whole-source parse exhausts the ER scene Work budget before layout "
        "(pre-existing FS-1b double-parse ceiling). Retired by RFC 0034: the bounded "
        "two-pool layout Work allowance now funds this over-budget active-ER layout.",
    ),
}

# Per-width measured geometry facts (char-surface, height 1600). Enforced live
# by the gtest; restored_240 always equals wide_240.
NARROW_DRAW_WHOLE = {
    "er-diagram/nightly-check/should-render-subgraphs-with-quoted-ids.mmd",
    "er-diagram/nightly-check/should-render-relationships-between-subgraphs.mmd",
    "er-diagram/pr-check/should-render-relationships-between-subgraph-and-entity.mmd",
}


def build(repo_root: pathlib.Path):
    cases = repo_root / "tests/fixtures/mermaid_official/cases"
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    selected = collect(repo_root)
    if len(selected) != EXPECTED_TOTAL:
        raise ValueError(
            f"ER group-edge selected population drifted: expected {EXPECTED_TOTAL}, "
            f"got {len(selected)}; re-scope the ledger and obtain a new independent review")

    retired_paths = set(RETIRED_BLOCKER_CAPABILITY_SET)
    found_retired = {rel for rel in selected if rel in retired_paths}
    if found_retired != retired_paths:
        missing = sorted(retired_paths - set(selected))
        raise ValueError(f"ER group-edge retired capability set mismatch; missing={missing}")

    rows = []
    counts = {"total": 0, "predicted_draw": 0, "predicted_blocked": 0,
              "retired_work_ceiling": 0}
    for rel in selected:
        source_sha = sha256_file(cases / rel)
        retired = rel in retired_paths
        retired_kind, retired_detail = (
            RETIRED_BLOCKER_CAPABILITY_SET[rel] if retired else (None, None))
        if source_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        narrow_whole = "drawn" if rel in NARROW_DRAW_WHOLE else "source"
        # RFC 0034: every selected whole source now draws at 240 (the retired
        # handDrawn row included); narrow stays source for that row as measured.
        whole_wide = "drawn"
        body_wide = "drawn"
        body_narrow = narrow_whole
        counts["total"] += 1
        counts["predicted_draw"] += 1
        if retired:
            counts["retired_work_ceiling"] += 1
        rows.append({
            "path": rel,
            "family": "er-diagram",
            "sha256": source_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "has_config": False,
            "has_init": False,
            "prediction": "predicted-draw",
            "expected_states": {
                "wide_240": whole_wide,
                "narrow_40": narrow_whole,
                "restored_240": whole_wide,
            },
            "expected_body_states": {
                "wide_240": body_wide,
                "narrow_40": body_narrow,
                "restored_240": body_wide,
            },
            "blocker": None,
            "retired_blocker": (
                {
                    "kind": retired_kind,
                    "retired_by": "RFC 0034",
                    "site": "src/ui/visual/mermaid_relations_core_impl.cpp (ER envelope double-parse Work budget)",
                    "evidence": source_sha,
                    "detail": retired_detail,
                }
                if retired else None),
            "ledger_note": (
                "ER subgraph id admitted as a relationship endpoint; in-group direction "
                "consumed as a non-terminal directive"
                if not retired else
                "RFC 0034 retired the SHA-bound whole-source Work ceiling; the whole source "
                "now draws through the bounded two-pool layout Work allowance (narrow stays source)"),
        })

    if (counts["predicted_blocked"] != EXPECTED_BLOCKED or
            counts["predicted_draw"] != EXPECTED_DRAW or
            counts["retired_work_ceiling"] != EXPECTED_RETIRED):
        raise ValueError(f"ER group-edge partition drift: {counts}")

    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "d295-er-group-edge-admission",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256((repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "ER originals declaring a subgraph and carrying a relationship whose "
                    "endpoint resolves to a declared subgraph id",
            "roots": [ROOT_REL],
            "blocker_detection": "the single historical handDrawn envelope Work-ceiling "
                                 "row (nested-direction variant) is RETIRED by RFC 0034 (bounded "
                                 "two-pool layout Work); no live exclusion remains and there is "
                                 "no generator-time rendering; corpus or source drift aborts.",
            "width_policy": "wide_240 and restored_240 are pre-registered; narrow_40 is measured "
                            "per row (three simple group-edge rows draw at 40, the rest stay source).",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial D295 review "
                              "(fresh agent; own structural selector must reproduce the exact "
                              "7-row set over all ER originals with zero missing/extra, and the "
                              "committed gtest cohort gate must reproduce every per-width state "
                              "against the live renderer; review focus is atomicity and the "
                              "ER107 zero-diff boundary)",
            "date": "2026-11-17",
            "verdict": "pending",
            "note": "Finalized by the D295 independent review; see the cohort gate test and the "
                    "Stage B-D closure report. Six rows draw losslessly at 240. The seventh, a "
                    "SHA-bound handDrawn nested variant, formerly stayed whole-source on the "
                    "envelope double-parse Work ceiling while its body drew; RFC 0034 retired "
                    "that ceiling via the bounded two-pool layout Work allowance, so all seven "
                    "whole sources now draw (narrow stays source for the handDrawn row). This "
                    "ledger stays a non-authoritative char smoke; matrix authority is the owned "
                    "proof cohorts.",
        },
        "counts": counts,
        "blocker_vocabulary": [BLOCKER_KIND],
        "retired_blocker_vocabulary": [BLOCKER_KIND],
        "rows": rows,
    }


def main() -> int:
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
