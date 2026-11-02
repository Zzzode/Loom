#!/usr/bin/env python3
"""Build a source-verified inventory of original Mermaid matrix gaps.

The matrix provides classifications, not necessarily root causes. This tool
keeps unknown causes explicit and does not turn observed renderer output into
an expectation or an admission.
"""

import argparse
import collections
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CASES = ROOT / "tests/fixtures/mermaid_official/cases"
MANIFEST = ROOT / "tests/fixtures/mermaid_official/manifest.json"


def inventory(matrix_path):
    matrix = json.loads(matrix_path.read_text(encoding="utf-8"))
    manifest_bytes = MANIFEST.read_bytes()
    manifest_hash = hashlib.sha256(manifest_bytes).hexdigest()
    if matrix.get("schema") != 1 or matrix.get("manifest_sha256") != manifest_hash:
        raise ValueError("matrix schema or manifest hash does not match the pinned corpus")
    fixtures = json.loads(manifest_bytes)["fixtures"]
    # Use the reviewed manifest's path names as the authority, not untrusted
    # report paths. Check its complete inventory as well as individual hashes.
    fixture_by_path = {entry["path"]: entry for entry in fixtures}
    paths = set(fixture_by_path)
    if len(paths) != 2145 or len(fixtures) != 2145:
        raise ValueError("unexpected or duplicate official manifest entries")
    rows = [item for item in matrix["cases"] if item["track"] == "original"]
    if len(rows) != len(paths) or {item["fixture"] for item in rows} != paths:
        raise ValueError("matrix originals do not match the pinned manifest")
    if len({item["fixture"] for item in rows}) != len(rows):
        raise ValueError("duplicate matrix original")
    gaps = []
    outcomes = collections.Counter()
    for row in sorted(rows, key=lambda x: x["fixture"]):
        relative = Path(row["fixture"])
        path = (CASES / relative).resolve(strict=True)
        if (CASES / relative).is_symlink() or not path.is_relative_to(CASES.resolve()) or not path.is_file():
            raise ValueError(f"unsafe or missing fixture: {relative}")
        source_sha = hashlib.sha256(path.read_bytes()).hexdigest()
        if source_sha != row["source_sha256"] or source_sha != fixture_by_path[row["fixture"]]["sha256"]:
            raise ValueError(f"source hash mismatch: {relative}")
        outcome = row["observed"]["wide_240"]
        if outcome not in ("drawn", "source_fallback", "error"):
            raise ValueError(f"unknown matrix outcome: {outcome}")
        outcomes[outcome] += 1
        if outcome == "drawn":
            continue
        errors = row.get("errors", [])
        diagnostic = "unclassified"
        if outcome == "source_fallback":
            diagnostic = "ordinary_source_observed"
        elif "Complete mapped original still uses ordinary source at240" in errors:
            diagnostic = "mapped_positive_source_at_240"
        elif any("unreviewed art or failed ordinary/restore protocol" in e for e in errors):
            diagnostic = "unreviewed_art_or_source_restore_mismatch"
        elif any("whole-null art/ordinary/restore unmet" in e for e in errors):
            diagnostic = "whole_null_art_or_source_restore_mismatch"
        elif any("Expected exact ordinary-code source fallback" in e for e in errors):
            diagnostic = "expected_source_fallback_mismatch"
        family_observations = {
            key: value["actual"] for key, value in row.items()
            if isinstance(value, dict) and isinstance(value.get("actual"), dict)
        }
        gaps.append({
            "fixture": row["fixture"], "source_sha256": source_sha,
            "family": row["type"], "wide_240_outcome": outcome,
            "observed": row["observed"],
            "matrix_reason": row["decision_reasons"]["wide_240"],
            "matrix_errors": errors, "diagnostic": diagnostic,
            "family_observations": family_observations,
            "first_confirmed_blocker": None,
            "blocker_status": "unknown_not_root_cause_diagnosed",
            "next_owner": "P0 source and runtime diagnosis",
        })
    return {
        "schema": "loom_mermaid_matrix_gap_inventory/v1",
        "matrix_sha256": hashlib.sha256(matrix_path.read_bytes()).hexdigest(),
        "manifest_sha256": manifest_hash,
        "upstream_tag": matrix["upstream_tag"],
        "scope": "original sources at 240 columns; fallback and errors earn no drawing credit",
        "counts": dict(sorted(outcomes.items())),
        "diagnostic_counts": dict(sorted(collections.Counter(x["diagnostic"] for x in gaps).items())),
        "gaps": gaps,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("matrix", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = inventory(args.matrix)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({"counts": data["counts"], "diagnostic_counts": data["diagnostic_counts"], "output": str(args.output)}))


if __name__ == "__main__":
    main()
