#!/usr/bin/env python3
"""Verify the RFC 0031 E1 working-tree baseline.

Reads `tools/mermaid/rfc0031-baseline.json` and checks every recorded
modified/untracked file against its SHA-256. This is how the Linux PSS host
proves it is measuring the same pre-E1 tree that the macOS host froze.

Usage:
  python3 tools/mermaid/verify_rfc0031_baseline.py
  python3 tools/mermaid/verify_rfc0031_baseline.py --json

Exit code 0 when every entry matches, 1 otherwise.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / "tools" / "mermaid" / "rfc0031-baseline.json"


def digest(path: Path) -> str | None:
    try:
        if path.is_symlink():
            return hashlib.sha256(os.readlink(path).encode()).hexdigest()
        return hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError:
        return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    args = ap.parse_args()

    if not BASELINE.exists():
        print(f"missing baseline: {BASELINE}", file=sys.stderr)
        return 1
    data = json.loads(BASELINE.read_text(encoding="utf-8"))
    entries = data.get("entries", [])
    mismatches: list[dict] = []
    checked = 0
    for entry in entries:
        rel = entry.get("path", "")
        if not rel or rel == "tools/mermaid/rfc0031-baseline.json":
            continue
        path = ROOT / rel
        if entry.get("kind") == "deleted" or entry.get("missing"):
            if path.exists() or path.is_symlink():
                mismatches.append({"path": rel, "reason": "expected missing"})
            checked += 1
            continue
        actual = digest(path)
        expected = entry.get("sha256")
        if actual is None:
            mismatches.append({"path": rel, "reason": "unreadable"})
        elif expected and actual != expected:
            mismatches.append({"path": rel, "reason": "sha256 mismatch",
                               "expected": expected, "actual": actual})
        checked += 1

    result = {
        "baseline": str(BASELINE.relative_to(ROOT)),
        "head": data.get("head"),
        "checked": checked,
        "mismatches": mismatches,
        "ok": not mismatches,
    }
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"baseline {result['baseline']} head={result['head']}")
        print(f"checked {checked} entries, {len(mismatches)} mismatch(es)")
        for mismatch in mismatches[:20]:
            print(f"  {mismatch['path']}: {mismatch['reason']}")
        if len(mismatches) > 20:
            print(f"  ... {len(mismatches) - 20} more")
    return 0 if not mismatches else 1


if __name__ == "__main__":
    sys.exit(main())
