#!/usr/bin/env python3
"""Generate the RFC 0031 FS-2 flowchart `click` body cohort ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/flowchart root that
    - carries a column-0 YAML frontmatter envelope (so the whole original
      stays on ordinary source fallback through the existing envelope gate),
    - whose graph body binds a node with a `click <id> ...` interaction
      statement (hyperlink, href, or callback form).

The terminal has no interaction layer; FS-2 consumes a well-formed click
binding as a non-visual no-op. A selected row is predicted-draw on the
frontmatter-free body track only when the body carries no OTHER unsupported
feature. In the pinned click universe every other selected body (14 rows)
carries a classDef/style/class/linkStyle statement, so one closed
renderer-free blocker detector classifies the non-drawing rows:
  * flow-click-style-classdef: a classDef/style/class/linkStyle statement
    (CSS-to-terminal projection is the FS-3 cohort).
Every selected click body without that statement is a draw row; the two sets
partition the 16-row selection with zero residual.

Draw rows paint on the body track at 240 columns (wide + restored) and stay
on ordinary source fallback at narrow_40 (the six-child fan-out is wider than
40 columns). Whole originals stay source at every width (frontmatter is not
admitted). The live pixel/no-leak gate is tests/ui/test_mermaid_fs2_cohort.cpp.

Usage: python3 tools/mermaid/generate_fs2_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs2-cohort.json")

SCHEMA = "loom_mermaid_fs2_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-click-style-classdef",
]

WITNESS = {
    "flow-click-style-classdef": (
        "src/ui/visual/mermaid_impl.cpp (classDef/style projection is FS-3)",
        "The click-bearing body carries a classDef/style/class/linkStyle "
        "statement; CSS-to-terminal styling is a later cohort, so consuming "
        "the click no-op alone cannot draw the body.",
    ),
}

CLICK_RE = re.compile(r"^\s*click\s+\S", re.M)
# Line-level styling statements. Anchored at the statement keyword so the
# words "class"/"style" appearing inside a click tooltip string cannot match.
STYLE_RE = re.compile(
    r"^\s*(classDef\b|linkStyle\b|style\s+\S|class\s+\S+\s+\S)", re.M)


def split_source(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return lines[1:close], lines[close + 1:]
    return None


# Tripwire for every other known unsupported feature a click-bearing body can
# carry. It never fires in the pinned 16-row universe (all non-drawing rows
# carry classDef/style first), but if a future fixture presented one WITHOUT a
# style statement it must abort generation (sentinel outside the vocabulary)
# rather than be silently mislabeled predicted-draw.
#
# Line-anchored, quote-unaware limitation: STYLE_RE/OTHER_DEBT_REGEX scan
# physical line prefixes and do not mask quoted or multi-line backtick label
# contents. A label whose own physical line began with "classDef"/"style "
# could false-positive (false blocked), or an unlisted debt inside such a
# label could false-negative (false draw). Both directions are caught loudly
# by the dedicated cohort gtest (blocked rows must stay source, draw rows
# must render), so no pinned row can be misclassified silently; broaden these
# detectors with quote masking only if such a fixture is ever selected.
OTHER_DEBT_RE = re.compile(
    r"^\s*subgraph\b"                # nested-container routing (FS-7)
    r"|%%\{"                          # body init directive
    r"|fa:fa-"                        # font-awesome glyph (FS-6)
    r"|==>|-\.->|--x|--o"             # edge residuals (FS-4)
    r"|\(\[|\[\[|\[\(|\{\{|\(\("      # non-rectangle shapes (FS-5)
    , re.M)


def blocker(body):
    if STYLE_RE.search(body):
        return "flow-click-style-classdef"
    if OTHER_DEBT_RE.search(body):
        # Outside the closed vocabulary: build() aborts so the maintainer
        # classifies the newly-seen debt explicitly.
        return "flow-click-unclassified-other-debt"
    return None


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def collect(repo_root):
    cases = repo_root / CASES
    rows = []
    for path in sorted((cases / "flowchart").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        _frontmatter, body_lines = split
        body = "\n".join(body_lines)
        if not CLICK_RE.search(body):
            continue
        rows.append({"path": rel, "blocker": blocker(body)})
    return rows


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    entries = collect(repo_root)
    rows = []
    n_draw = n_blocked = 0
    for entry in entries:
        kind = entry["blocker"]
        if kind is not None and kind not in BLOCKER_VOCABULARY:
            raise ValueError(f"Blocker outside closed vocabulary: {kind}")
        blocked = kind is not None
        n_blocked += blocked
        n_draw += not blocked
        site, detail = WITNESS[kind] if blocked else ("", "")
        manifest_row = manifest_rows[entry["path"]]
        rows.append({
            "path": entry["path"],
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / entry["path"]),
            "manifest_sha256": manifest_row["sha256"],
            "prior_status": manifest_row["raw"],
            "prediction": "predicted-blocked" if blocked else "predicted-draw",
            # Whole originals keep source at every width: their frontmatter is
            # not admitted by the flow envelope profile.
            "expected_states": {
                "wide_240": "source",
                "narrow_40": "source",
                "restored_240": "source",
            },
            # The frontmatter-free body draws at 240; the six-child fan-out is
            # wider than 40 columns so narrow stays on ordinary fallback.
            "expected_body_states": {
                "wide_240": "source" if blocked else "drawn",
                "narrow_40": "source",
                "restored_240": "source" if blocked else "drawn",
            },
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if blocked else None),
            "ledger_note": (
                "FS-2 click interaction binding admitted as a non-visual "
                "no-op on the body track"
                if not blocked else
                "Blocked by a closed source-content feature detector; the "
                "click statement itself is a no-op"),
        })
    total = n_draw + n_blocked
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-2-click",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256((repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original whose graph body binds a "
                    "click interaction statement",
            "roots": ["flowchart"],
            "blocker_detection": "six fixed source-content detectors "
                                 "(subgraph, init, fa icon, classDef/style, "
                                 "non-rectangle shape, special edge); no "
                                 "generator-time rendering; residual rows abort",
            "width_policy": "whole original stays source at all widths; body "
                            "draws at 240 wide/restored and stays source at "
                            "narrow_40 (fan-out wider than 40 columns).",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(D-066 FS-2 click no-op, 2026-11-05)",
            "date": "2026-11-05",
            "verdict": "approved",
            "note": "Read-only adversarial review of the production click "
                    "no-op branch (hyper/href/callback forms, fail-closed "
                    "target whitelist), the mechanical selection/blocker "
                    "detectors, ledger disjointness/zero-residual abort, body "
                    "pixel/no-leak gate, and body-only matrix wiring. "
                    "APPROVE: flow-only blast radius, no shared constants, "
                    "two target body rows, gaps stay 48.",
        },
        "counts": {
            "total": total,
            "predicted_draw": n_draw,
            "predicted_blocked": n_blocked,
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
