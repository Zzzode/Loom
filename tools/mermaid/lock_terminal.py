#!/usr/bin/env python3
"""Freeze reviewed terminal-test imports; never run from normal tests.

This records inventory only. It does not create drawing expectations, execute a
renderer, or approve changes to the imported source/assertion contract.
"""

import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--freeze-reviewed-inventory", action="store_true", required=True)
    args = parser.parse_args()
    assert args.freeze_reviewed_inventory
    root = Path(__file__).resolve().parents[2] / "tests/fixtures/mermaid_terminal"
    vendors = ["beautiful_mermaid", "mermaid_ascii", "merman", "mmdflux", "termaid"]
    lock = {"schema": 1, "vendors": [], "files": []}
    for vendor in vendors:
        path = root / vendor
        cases_path = path / "cases.json"
        cases_bytes = cases_path.read_bytes()
        cases = json.loads(cases_bytes)
        rows = cases["cases"]
        ids = [row["id"] for row in rows]
        if len(ids) != len(set(ids)):
            raise ValueError(f"Duplicate case IDs in {vendor}")
        for row in rows:
            actual = hashlib.sha256(row["input"].encode()).hexdigest()
            if actual != row["sha256"]:
                raise ValueError(f"Incorrect input digest: {row['id']}")
            if not row.get("provenance"):
                raise ValueError(f"Missing source provenance: {row['id']}")
        lock["vendors"].append({
            "name": vendor,
            "cases_sha256": hashlib.sha256(cases_bytes).hexdigest(),
            "case_count": len(rows),
            "unique_input_count": len({row["sha256"] for row in rows}),
            "case_ids_sha256": hashlib.sha256("\n".join(sorted(ids)).encode()).hexdigest(),
        })
        for asset in sorted(path.rglob("*")):
            if asset.is_symlink():
                raise ValueError(f"Symlink is not a vendored asset: {asset}")
            if not asset.is_file():
                continue
            contents = asset.read_bytes()
            lock["files"].append({
                "path": asset.relative_to(root).as_posix(),
                "bytes": len(contents),
                "sha256": hashlib.sha256(contents).hexdigest(),
            })
    contents = (json.dumps(lock, ensure_ascii=False, indent=2) + "\n").encode()
    (root / "inventory.lock.json").write_bytes(contents)
    print(f"{len(lock['files'])} files; inventory.lock.json SHA256 "
          f"{hashlib.sha256(contents).hexdigest()}")
    print("Review the complete inventory before updating the native trust anchor.")


if __name__ == "__main__":
    main()
