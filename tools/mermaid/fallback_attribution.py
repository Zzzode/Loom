#!/usr/bin/env python3
"""Reproducible ER/mermaid whole-source fallback attribution (TechLead D-325).

The long-term goal is no longer "zero fallback" but "every fallback is
explained, locked and cannot drift silently". This module is the single source
of truth for that:

* an ordered, deterministic, total classifier over the raw fixture bytes —
  every whole-source fallback row maps to exactly one registry category, there
  is no implicit "unclassified" bucket;
* a causality/policy label per category so we never over-claim:
    permanent         structurally impossible in a static terminal
                      (interactivity, dynamic timing, sandbox security);
    foreign-engine    needs an external layout/visual engine whose output the
                      native orthogonal projection cannot byte-faithfully
                      reproduce without weakening the "never mis-draw foreign
                      semantics" invariant (neo/handDrawn/ELK/HTML/dagre/icons);
    debt               a parser/projection gap that could in principle be built;
    debt-candidate     a narrow debt that may admit a compact owned cohort;
    family-residual    bounded to one family's projection surface; no shared
                      cross-family debt isolated at the D-325 census;
* three modes:
    --emit  MATRIX PATH -o OUT   (re)generate the committed fallback roster
                                  from a fresh OfficialMatrix JSON;
    --self-test                   hermetic, no matrix needed: re-run every
                                  detector against the committed roster's
                                  fixtures and prove the classification is
                                  reproducible, total and count-stable;
    --matrix MATRIX PATH          live drift gate: classify a fresh matrix and
                                  fail on any unregistered fallback path, any
                                  reclassified path, or any count change.

The classifier is intentionally text-based over the immutable fixture bytes so
the result is reproducible without rendering. It does NOT claim every detector
is a sufficient blocker: where a feature co-occurs with both drawn and fallback
rows (e.g. edge animation metadata), the category records it as the primary
suspected construct with an honest causality label rather than a proven cause.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CASES_ROOT = "tests/fixtures/mermaid_official/cases"
ROSTER_PATH = Path("tools/mermaid/fallback_roster.json")
WIDE_STATE = "wide_240"
FALLBACK = "source_fallback"


def _look(s: str):
    m = re.search(r'(?m)^\s*look:\s*["\']?(\w+)', s)
    return m.group(1) if m else None


@dataclass(frozen=True)
class Detector:
    category: str
    pattern: str  # human-readable predicate description
    reason: str
    causality: str  # permanent | foreign-engine | debt | debt-candidate | family-residual
    regex: str | None = None
    fn: str | None = None  # named custom detector key


# Ordered detectors. First match wins; order is part of the contract and is
# pinned by the self-test, so reordering changes classifications and fails the
# gate until the roster is regenerated and reviewed.
DETECTORS: tuple[Detector, ...] = (
    Detector("click_interactive_callback",
             "a line begins with `click <id> href/callback`",
             "interactive hyperlink navigation or JS callback; a static terminal "
             "has no click target, so the binding cannot be represented",
             "permanent", regex=r"(?m)^\s*click\b"),
    Detector("edge_animation_metadata",
             "edge carries `@-->` or `@{ animation: ... }` metadata",
             "dynamic edge animation timing has no static frame; the renderer "
             "ignores timing, and fallback here tracks a co-occurring construct, "
             "so this is a primary suspect not a proven sufficient blocker",
             "permanent", regex=r"@--|@\{"),
    Detector("special_link_markers",
             "edge uses ~~~ / ==x / ==o / -.-o / x-- / o-- markers",
             "arrowhead/link marker glyphs outside the native marker set; the "
             "narrowest observed debt and a possible compact owned cohort",
             "debt-candidate", regex=r"~~~|==x|==o|-\.-o|x--|o--"),
    Detector("icon_font_glyphs",
             "references fa: / fontawesome / registered-icon glyphs",
             "external icon/font library glyphs are unavailable in the terminal "
             "cell alphabet",
             "foreign-engine", regex=r"fa:\w|fontawesome|registered-icon"),
    Detector("git_cherry_pick",
             "gitGraph declares a cherry-pick operation",
             "cherry-pick graph operation is not projected by the native git renderer",
             "debt", regex=r"cherry-?pick"),
    Detector("linkstyle_directive",
             "a linkStyle directive styles an edge",
             "per-edge linkStyle directives are not projected onto native cells",
             "debt", regex=r"linkStyle"),
    Detector("securitylevel_directive",
             "config sets securityLevel (loose/sandbox)",
             "HTML/sandbox security posture has no meaningful static equivalent; "
             "downgrading it would change the security contract",
             "permanent", regex=r"securityLevel"),
    Detector("look_neo",
             "frontmatter look: neo",
             "neo design-system styling cannot be byte-faithfully reproduced by "
             "the native orthogonal projection",
             "foreign-engine", fn="look_neo"),
    Detector("look_handdrawn",
             "frontmatter look: handDrawn",
             "handDrawn roughness/relief engine is explicitly not reproduced "
             "(see mermaid_er_impl.cpp native projection notice)",
             "foreign-engine", fn="look_handdrawn"),
    Detector("look_classic_other",
             "frontmatter look is a non-default, non-neo, non-handDrawn value",
             "other non-default look variant requiring foreign visual styling",
             "foreign-engine", fn="look_classic_other"),
    Detector("elk_layout_engine",
             "config selects ELK layout (layout: elk / layoutEngine / elk-layout)",
             "ELK layout engine output differs from native directed orthogonal "
             "routing; routing is downgraded, so rows needing ELK geometry fall back",
             "foreign-engine",
             regex=r'layout:\s*elk|layoutEngine|"elk"|elk-layout'),
    Detector("htmllabels_false",
             "config sets htmlLabels: false",
             "the HTML label engine path is outside the native Markdown label subset",
             "foreign-engine", regex=r"htmlLabels:\s*false"),
    Detector("subgraph_nesting",
             "body declares a subgraph",
             "subgraph nesting / id-shadow / direction projection surface",
             "debt", regex=r"\bsubgraph\b"),
    Detector("classdef_styling",
             "body uses classDef or ::: class shorthand",
             "classDef / class-shorthand styling surface is only partially projected",
             "debt", regex=r"classDef|:::"),
    Detector("dagre_layout_tuning",
             "dagre layout or spacing/curve tuning parameters",
             "dagre-specific spacing/ranking/curve parameters are foreign layout knobs",
             "foreign-engine",
             regex=r'layout:\s*["\']?dagre|nodeSpacing|rankSpacing|diagramPadding|curve'),
    Detector("multiline_label",
             "label contains an explicit \\\\n or <br> break",
             "explicit multiline label break handling surface",
             "debt", regex=r"\\n|<br"),
)


def _custom(name: str, s: str) -> bool:
    look = _look(s)
    if name == "look_neo":
        return look == "neo"
    if name == "look_handdrawn":
        return look == "handDrawn"
    if name == "look_classic_other":
        return bool(look) and look not in ("default", "neo", "handDrawn")
    raise KeyError(name)


def detector_matches(det: Detector, source: str) -> bool:
    if det.regex is not None:
        return re.search(det.regex, source, re.IGNORECASE if det.category ==
                         "icon_font_glyphs" else 0) is not None
    return _custom(det.fn, source)


def classify(source: str, family: str) -> str:
    """Total classifier: returns exactly one category id for any source."""
    for det in DETECTORS:
        if detector_matches(det, source):
            return det.category
    return f"family_residual:{family}"


def all_category_ids() -> list[str]:
    ids = [d.category for d in DETECTORS]
    return ids


def load_matrix(path: Path):
    data = json.loads(path.read_bytes())
    return [r for r in data["cases"] if r.get("track") == "original"]


def fallback_rows(rows) -> list:
    return [r for r in rows
            if r.get("observed", {}).get(WIDE_STATE) == FALLBACK]


def fixture_source(rel_path: str) -> str:
    return (ROOT / CASES_ROOT / rel_path).read_text()


def emit_roster(matrix_path: Path) -> dict:
    rows = fallback_rows(load_matrix(matrix_path))
    by_category: dict[str, list[str]] = {}
    for row in rows:
        rel = row["fixture"]
        category = classify(fixture_source(rel), row["type"])
        by_category.setdefault(category, []).append(rel)
    for paths in by_category.values():
        paths.sort()
    counts = {cat: len(paths) for cat, paths in by_category.items()}
    return {
        "schema": "loom_mermaid_fallback_roster/v1",
        "description": "Reproducible whole-source fallback attribution. Every "
                       "source_fallback original maps to exactly one category; "
                       "family_residual:* means no shared cross-family debt was "
                       "isolated at the D-325 census. Regenerate with "
                       "fallback_attribution.py --emit, do not hand-edit.",
        "total_fallback": len(rows),
        "category_counts": dict(sorted(counts.items())),
        "causality": {d.category: d.causality for d in DETECTORS},
        "detector_order": [d.category for d in DETECTORS],
        "optional_gains": [
            {
                "category": "special_link_markers",
                "note": "narrowest observed debt; a future compact owned cohort "
                        "may close it like RFC 0033 group-edge. Recorded per "
                        "TechLead D-325, not actioned during registry landing.",
            }
        ],
        "paths_by_category": dict(sorted(by_category.items())),
    }


def self_test(roster_path: Path) -> None:
    roster = json.loads(roster_path.read_bytes())
    # Detector order is part of the pinned contract.
    need(roster["detector_order"] == [d.category for d in DETECTORS],
         "detector_order changed; regenerate and review the roster")
    counts = roster["category_counts"]
    paths_by_category = roster["paths_by_category"]
    # Every roster path re-classifies to its pinned category from live bytes.
    total = 0
    for category, paths in paths_by_category.items():
        for rel in paths:
            family = rel.split("/", 1)[0]
            observed = classify(fixture_source(rel), family)
            need(observed == category,
                 f"classification drift for {rel}: roster={category} live={observed}")
            total += 1
    need(total == roster["total_fallback"],
         f"roster path total {total} != pinned total {roster['total_fallback']}")
    # Counts reconcile exactly with the path lists (no unclassified, no gap).
    recomputed = {cat: len(p) for cat, p in paths_by_category.items()}
    need(recomputed == counts, "category_counts do not match path list lengths")
    need(sum(counts.values()) == total, "category count sum != roster total")
    # Causality labels cover every non-residual category.
    for cat in counts:
        if cat.startswith("family_residual:"):
            continue
        need(cat in roster["causality"], f"missing causality label for {cat}")
    print(f"fallback roster self-test OK: {total} fallback rows, "
          f"{len(counts)} categories, zero unclassified, classification reproducible")


def drift_check(matrix_path: Path, roster_path: Path) -> None:
    roster = json.loads(roster_path.read_bytes())
    pinned: dict[str, str] = {}
    for cat, paths in roster["paths_by_category"].items():
        for rel in paths:
            pinned[rel] = cat
    live: dict[str, str] = {}
    for row in fallback_rows(load_matrix(matrix_path)):
        rel = row["fixture"]
        live[rel] = classify(fixture_source(rel), row["type"])
    new = sorted(set(live) - set(pinned))
    removed = sorted(set(pinned) - set(live))
    reclassified = sorted(
        (p for p in set(live) & set(pinned) if live[p] != pinned[p]))
    need(not new, f"unregistered fallback paths (add a reviewed registry entry): {new[:10]}")
    need(not reclassified, f"reclassified fallback paths (review predicate/roster): {reclassified[:10]}")
    need(len(live) == roster["total_fallback"],
         f"fallback count drifted: live {len(live)} != pinned {roster['total_fallback']} "
         f"(removed={len(removed)})")
    print(f"fallback drift check OK: {len(live)} fallback rows match the roster; "
          f"no unregistered or reclassified fallback")


def need(condition: bool, reason: str) -> None:
    if not condition:
        raise ValueError(reason)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--emit", metavar="MATRIX",
                    help="generate a roster JSON from an OfficialMatrix file")
    ap.add_argument("-o", "--output", help="roster output path for --emit")
    ap.add_argument("--self-test", action="store_true",
                    help="hermetic reproducibility/exhaustiveness check")
    ap.add_argument("--matrix", metavar="MATRIX",
                    help="live matrix for the unregistered-fallback drift gate")
    ap.add_argument("--roster", default=str(ROOT / ROSTER_PATH),
                    help="committed roster path")
    args = ap.parse_args()
    roster_path = Path(args.roster)
    try:
        if args.emit:
            if not args.output:
                ap.error("--emit requires -o/--output")
            roster = emit_roster(Path(args.emit))
            Path(args.output).write_text(
                json.dumps(roster, indent=2, ensure_ascii=False) + "\n")
            print(f"wrote {args.output}: {roster['total_fallback']} fallback rows, "
                  f"{len(roster['category_counts'])} categories")
            return 0
        if args.self_test:
            self_test(roster_path)
            return 0
        if args.matrix:
            drift_check(Path(args.matrix), roster_path)
            return 0
        ap.error("choose --emit, --self-test or --matrix")
    except (ValueError, OSError, KeyError, json.JSONDecodeError) as error:
        print("fallback attribution failed: " + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
