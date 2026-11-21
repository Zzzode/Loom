#!/usr/bin/env python3
"""Generate the RFC 0035 reviewed-cohort ledger (xychart envelope admission).

Why a separate ledger: RFC 0031 E2c owns description-only sources (no config).
RFC 0035 admits a folded ``description`` that coexists with a ``config`` block
whose leaves the existing ``xy_visual_envelope`` profile already accepts, plus
three later waves (show*: false suppression, the dataLabelColor role, and one
description+init row). Promotion uses the code-side reviewed cohort pattern,
never the pinned manifest.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under cases/xychart that
    - opens/closes with column-0 ``---`` fences,
    - carries ``description: >-`` as its first key with non-empty two-space,
      tab-free, blank-line-free payload lines,
    - optionally carries exactly one ``config:`` block after the description
      and nothing else at top level,
    - has a body whose first real statement starts with ``xychart``.

Phase partition (renderer-free classification, then a pinned denominator):
  P1-envelope          description+config, no widened key at all.
  P2-show-false        config uses one of the four axis show* flags as false
                       (or the spark-bar row that sets all of them).
  P3-data-label-color  config carries themeVariables.xyChart.dataLabelColor.
  P4-init              description only, with a leading ``%%{init: ...}%%``.
  capability-excluded  labelRotation (geometry), and the two signed-value
                       rows pinned by SHA (negative bars + data labels).

Only ENABLED_PHASES is emitted: the matrix branch must not claim a draw that
the renderer does not yet produce. Each later phase flips its entry together
with the code change and a new independent review.

Usage: python3 tools/mermaid/generate_xychart_0035_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/xychart-0035-cohort.json")

SCHEMA = "loom_mermaid_xychart_0035_cohort/v1"
ROOT_REL = "xychart"
ENABLED_PHASES = ("P1-envelope",)

EXPECTED = {
    "P1-envelope": 17,
    "P2-show-false": 5,
    "P3-data-label-color": 1,
    "P4-init": 1,
}

# SHA-bound capability rows: accepted by the envelope grammar but not
# completable/faithful through the bounded renderer. Path/SHA bound because
# "faithful at the terminal" is not textually decidable; any change aborts.
CAPABILITY_EXCLUDED = {
    "xychart/nightly-check/should-render-negative-and-decimal-values-with-correct-labels-for-vertical-xy-chart.mmd":
        "signed-value bar labels (negative data with showDataLabel); terminal "
        "label placement for signed bars is not implemented.",
    "xychart/pr-check/should-render-negative-and-decimal-values-with-correct-labels-for-horizontal-xy-chart.mmd":
        "signed-value bar labels (negative data with showDataLabel); terminal "
        "label placement for signed bars is not implemented.",
}

REVIEW = {
    "reviewer_agent": "agent:afdd5fdaf6f59d4dd independent P1 code review "
                      "(fresh general-purpose agent; own source selector reproduced the "
                      "17-row set and an independent render probe reproduced drawn 30/18)",
    "date": "2026-11-21",
    "verdict": "approved",
    "note": "P1 admits the description+config envelope only; P2/P3/P4 rows and the "
            "capability exclusions keep whole source. Reviewer dispositions: CRLF aligned "
            "with the D236 normalization precedent, boundary set extended, signed-value "
            "capability rows pinned.",
}


def sha256_file(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def split_source(text: str):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return lines[1:close], lines[close + 1:]
    return None


def first_statement(body_lines) -> str:
    for line in body_lines:
        stripped = line.strip()
        if stripped and not stripped.startswith("%%"):
            return stripped.split()[0] if stripped.split() else ""
    return ""


def description_shape(frontmatter_lines):
    """Return (ok, config_start_index) for the RFC 0035 description grammar."""
    if not frontmatter_lines or frontmatter_lines[0] != "description: >-":
        return False, None
    payload = False
    config_start = None
    for index in range(1, len(frontmatter_lines)):
        line = frontmatter_lines[index]
        if line == "config:":
            config_start = index
            break
        if not line or not line.strip():
            return False, None  # no blank line or whitespace-only payload
        if "\t" in line or not line.startswith("  ") or line.startswith("   ") \
                or line.lstrip(" ").startswith("#"):
            return False, None
        payload = True
    return payload, config_start


def config_leaves(config_lines):
    stack = []
    leaves = []
    for line in config_lines:
        if not line:
            continue
        indent = len(line) - len(line.lstrip(" "))
        key, _, value = line.strip().partition(":")
        while stack and indent <= stack[-1][0]:
            stack.pop()
        path = ".".join([entry[1] for entry in stack] + [key])
        stack.append((indent, key))
        leaves.append((path, value.strip()))
    return leaves


def classify(frontmatter_lines, body_lines):
    """Return (phase, reason) or (None, reason) when the row is excluded."""
    if any(line.lstrip().startswith("%%{") for line in body_lines):
        return "P4-init", "description-only frontmatter with a leading init directive"
    has_description, config_start = description_shape(frontmatter_lines)
    if not has_description or config_start is None:
        return None, "not a description+config envelope"
    for line in frontmatter_lines[config_start + 1:]:
        if line and not line.startswith(" "):
            return None, "unexpected top-level key after config"
    leaves = config_leaves(frontmatter_lines[config_start + 1:])
    if not leaves:
        return None, "empty config block"
    names = {path.split(".")[-1] for path, _ in leaves}
    if "labelRotation" in names:
        return None, "label rotation is capability debt (OQ-1 ruling)"
    if "dataLabelColor" in names:
        return "P3-data-label-color", "themeVariables.xyChart.dataLabelColor role"
    if any(value == "false" and path.split(".")[-1].startswith("show") for path, value in leaves):
        return "P2-show-false", "axis show* flag set to false (suppression)"
    return "P1-envelope", "description+config inside the existing profile"


def collect(cases: pathlib.Path):
    buckets = {}
    excluded = []
    for path in sorted((cases / ROOT_REL).rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        parts = split_source(path.read_text())
        if parts is None:
            continue
        frontmatter, body = parts
        if not first_statement(body).startswith("xychart"):
            continue
        if not frontmatter or not frontmatter[0].startswith("description:"):
            continue
        phase, reason = classify(frontmatter, body)
        if phase is None:
            excluded.append({"path": rel, "reason": reason})
            continue
        if rel in CAPABILITY_EXCLUDED:
            excluded.append({"path": rel, "reason": CAPABILITY_EXCLUDED[rel]})
            continue
        buckets.setdefault(phase, []).append(rel)
    return buckets, excluded


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    buckets, excluded = collect(cases)
    for phase, expected in EXPECTED.items():
        found = len(buckets.get(phase, []))
        if found != expected:
            raise ValueError(
                f"RFC 0035 {phase} population drifted: expected {expected}, got {found}; "
                "re-scope the ledger and obtain a new independent review")
    if set(buckets) - set(EXPECTED):
        raise ValueError(f"unexpected phases: {sorted(set(buckets) - set(EXPECTED))}")

    rows = []
    counts = {"total": 0, "predicted_draw": 0, "by_phase": {}}
    for phase in ENABLED_PHASES:
        for rel in buckets.get(phase, []):
            source_sha = sha256_file(cases / rel)
            counts["total"] += 1
            counts["predicted_draw"] += 1
            counts["by_phase"][phase] = counts["by_phase"].get(phase, 0) + 1
            rows.append({
                "path": rel,
                "family": "xychart",
                "phase": phase,
                "sha256": source_sha,
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "prior_status": manifest_rows[rel]["raw"],
                "prediction": "predicted-draw",
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
                "ledger_note": "RFC 0035 description+config envelope admission; "
                               "the config profile is unchanged.",
            })

    return {
        "schema": SCHEMA,
        "rfc": 35,
        "phase": "P1-envelope",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256((repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "xychart original whose folded (>-) description is followed by exactly "
                    "one config block already inside the xy_visual_envelope profile",
            "roots": [ROOT_REL],
            "phase_partition": "renderer-free classification with pinned per-phase "
                               "denominators; only ENABLED_PHASES is emitted",
            "enabled_phases": list(ENABLED_PHASES),
            "capability_detection": "labelRotation is capability debt by OQ-1 ruling; the two "
                                    "signed-value rows are path-pinned capability debt",
            "width_policy": "wide_240 admission only; narrow_40 stays 'source'.",
        },
        "review": REVIEW,
        "counts": counts,
        "excluded": excluded,
        "rows": rows,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
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
