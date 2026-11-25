#!/usr/bin/env python3
"""Generate the D-470 re-baseline manifest mechanically.

Round 809 requires the manifest to be derived from measured diffs, never
hand-counted: every census row whose wide_240 state changed between the
pre-change roster and the new roster is listed with before, after, its
reviewed cause class and source SHA. The causes come from the reviewed
projection-draw cohort ledger; a changed row with no ledger cause aborts.

Usage:
  python3 tools/mermaid/d470_rebaseline_manifest.py \
      --old-roster <path-to-pre-change fallback_roster.json> \
      --new-roster tools/mermaid/fallback_roster.json \
      -o docs/dev/d470-rebaseline-manifest.json
"""

import argparse
import json
import pathlib
import sys

COHORT = pathlib.Path("tools/mermaid/d470-projection-draws-cohort.json")
CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")


def category_index(roster):
    index = {}
    for category, paths in roster["paths_by_category"].items():
        for path in paths:
            index[path] = category
    return index


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--old-roster", required=True)
    parser.add_argument("--new-roster", required=True)
    parser.add_argument("-o", "--output", required=True)
    args = parser.parse_args()

    old = json.loads(pathlib.Path(args.old_roster).read_text())
    new = json.loads(pathlib.Path(args.new_roster).read_text())
    cohort = json.loads(COHORT.read_text())
    causes = {row["path"]: row["cause"] for row in cohort["rows"]}
    old_by_path = category_index(old)
    new_by_path = category_index(new)

    flipped = sorted(set(old_by_path) - set(new_by_path))
    appeared = sorted(set(new_by_path) - set(old_by_path))
    if appeared:
        print(f"unexpected new fallback rows: {appeared}", file=sys.stderr)
        return 1

    rows = []
    for path in flipped:
        if path not in causes:
            print(f"flipped row without a reviewed cause: {path}", file=sys.stderr)
            return 1
        rows.append({
            "fixture": path,
            "before": "source_fallback",
            "after": "drawn",
            "before_category": old_by_path[path],
            "cause": causes[path],
            "source_sha256": cohort["rows"][[r["path"] for r in cohort["rows"]].index(path)][
                "sha256"],
        })
    if len(rows) != len(cohort["rows"]):
        print(
            f"manifest enumerates {len(rows)} rows but the cohort has "
            f"{len(cohort['rows'])}; a cohort row did not flip",
            file=sys.stderr,
        )
        return 1

    payload = {
        "schema": "loom_mermaid_d470_rebaseline_manifest/v1",
        "decision": cohort["decision"],
        "generated_from": {
            "old_roster_total_fallback": old["total_fallback"],
            "new_roster_total_fallback": new["total_fallback"],
            "flipped_rows": len(rows),
        },
        "rows": rows,
    }
    pathlib.Path(args.output).write_text(json.dumps(payload, indent=1) + "\n")
    print(f"wrote {args.output}: {len(rows)} flipped rows, "
          f"{old['total_fallback']} -> {new['total_fallback']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
