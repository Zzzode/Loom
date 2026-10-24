#!/usr/bin/env python3
"""Verify the RFC 0031 E6c mandatory-gap roster against capability reports.

The 12 MermaidCapabilityReport tests never fail on a capability gap; instead
they serialize a "mandatory_gaps" array into their JSON reports and diff it
against the reviewed ledger tools/mermaid/e6c-mandatory-gap-roster.json. This
script is the out-of-tree half of that invariant:

  * it pins the roster by byte size and SHA-256 (constants parsed from
    tests/support/mermaid_gap_roster.hpp, the single source of truth);
  * for every build directory it loads each report's serialized
    "mandatory_gaps" array and diffs it against the roster both ways, so a
    renderer-driven gain OR loss fails until the ledger is updated;
  * --write regenerates gates from one build directory and rewrites the
    pinned size/SHA in the support header. Regeneration is maintenance-only:
    the new roster must be independently reviewed before it is trusted.

Run with python3 -I. Exit 0 when every preset matches, 1 otherwise.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ROSTER = ROOT / "tools" / "mermaid" / "e6c-mandatory-gap-roster.json"
HEADER = ROOT / "tests" / "support" / "mermaid_gap_roster.hpp"
SCHEMA = "e6c_mandatory_gap_roster/v1"

GATE_REPORTS = {
    "official_matrix": "mermaid-official-matrix.json",
    "merman_xy102": "mermaid-merman-xy102-C2-semantics.json",
    "class481": "mermaid-class481-native.json",
    "class285": "mermaid-official-class285-source-scenes.json",
    "class_endpoint29": "mermaid-class-endpoint29-source-scenes.json",
    "state85": "mermaid-state85-whole-source-scenes.json",
    "class_caption_starvation8": "mermaid-class-caption-starvation-source-scenes.json",
    "state_reverse_escape": "mermaid-state-narrow-reverse-source-scenes.json",
    "class_notes18": "mermaid-class-notes18-whole-sources.json",
    "requirement11_p1p6": "mermaid-requirement11-independent-controls.json",
    "requirement11_overflow": "mermaid-requirement11-overflow-peers-native.json",
}
# Flow400CompleteSources has no roster: counts-only gate.


def canonical(value: object) -> str:
    """Deterministic canonical JSON for one gap row.

    Object keys sort lexicographically and homogeneous string arrays (the
    per-row category lists) sort too. Rows themselves are compared as sets by
    the caller, matching tests/support/mermaid_gap_roster.hpp.
    """
    if isinstance(value, dict):
        members = ",".join(
            json.dumps(key) + ":" + canonical(value[key]) for key in sorted(value)
        )
        return "{" + members + "}"
    if isinstance(value, list):
        if all(isinstance(item, str) for item in value):
            elements = sorted(value)
        else:
            elements = value
        return "[" + ",".join(canonical(item) for item in elements) + "]"
    return json.dumps(value, separators=(",", ":"), ensure_ascii=False)


def row_set(rows: object) -> set[str]:
    if not isinstance(rows, list):
        raise ValueError("mandatory_gaps must be an array")
    return {canonical(row) for row in rows}


def read_header_pins() -> tuple[int, str]:
    text = HEADER.read_text(encoding="utf-8")
    size_match = re.search(r"kRosterBytes\s*=\s*(\d+)\s*;", text)
    sha_match = re.search(r'kRosterSha256\s*=\s*"([0-9a-f]{64})"\s*;', text)
    if not size_match or not sha_match:
        raise ValueError(f"cannot parse roster pins from {HEADER}")
    return int(size_match.group(1)), sha_match.group(1)


def write_header_pins(size: int, sha: str) -> None:
    text = HEADER.read_text(encoding="utf-8")
    text, n1 = re.subn(r"kRosterBytes\s*=\s*\d+\s*;", f"kRosterBytes = {size};", text)
    text, n2 = re.subn(
        r'kRosterSha256\s*=\s*"(?:[0-9a-f]{64})?"\s*;',
        f'kRosterSha256 = "{sha}";',
        text,
    )
    if n1 != 1 or n2 != 1:
        raise ValueError("failed rewriting roster pins in support header")
    HEADER.write_text(text, encoding="utf-8")


def collect_gates(build_dir: Path) -> dict[str, list[object]]:
    gates: dict[str, list[object]] = {}
    for gate, report_name in GATE_REPORTS.items():
        report_path = build_dir / report_name
        report = json.loads(report_path.read_text(encoding="utf-8"))
        if "mandatory_gaps" not in report:
            raise ValueError(f"{report_path} has no mandatory_gaps key")
        gates[gate] = report["mandatory_gaps"]
    return gates


def write_roster(gates: dict[str, list[object]]) -> None:
    payload = {
        "schema": SCHEMA,
        "description": (
            "RFC 0031 E6c reviewed ledger of mandatory-completion capability "
            "gaps still open in the native terminal Mermaid renderer. Generated "
            "from serialized report mandatory_gaps arrays, independently audited "
            "row by row, then frozen by byte size and SHA-256 in "
            "tests/support/mermaid_gap_roster.hpp. Renderer output is not "
            "expectation evidence: every change here requires ledger review."
        ),
        "review": {
            "status": "reviewed",
            "date": "2026-10-24",
            "phase": "RFC 0031 E6c",
            "note": (
                "Every row audited against its converted reporter, report and "
                "pinned authority source: exact error-message mapping for "
                "official_matrix, frozen ids/outcomes for the ten id/name/"
                "ordinal-keyed gates, counts reconciled with strict EXPECT pins."
            ),
        },
        "gates": {gate: gates[gate] for gate in GATE_REPORTS},
    }
    text = json.dumps(payload, indent=2, ensure_ascii=False) + "\n"
    ROSTER.write_text(text, encoding="utf-8")


def verify_build_dir(build_dir: Path, roster_gates: dict[str, list[object]]) -> list[str]:
    problems: list[str] = []
    if not build_dir.is_dir():
        return [f"build directory missing: {build_dir}"]
    observed = collect_gates(build_dir)
    for gate in GATE_REPORTS:
        expected = row_set(roster_gates[gate])
        actual = row_set(observed[gate])
        missing = sorted(expected - actual)
        unexpected = sorted(actual - expected)
        for row in missing:
            problems.append(f"{build_dir.name} {gate}: roster row not observed: {row}")
        for row in unexpected:
            problems.append(f"{build_dir.name} {gate}: observed row not in roster: {row}")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dirs",
        default="build/debug/tests/ui,build/release/tests/ui",
        help="comma-separated report directories",
    )
    parser.add_argument("--write", action="store_true",
                        help="regenerate roster and header pins from the first build dir")
    args = parser.parse_args()
    build_dirs = [ROOT / part for part in args.build_dirs.split(",") if part]

    if args.write:
        source_dir = build_dirs[0]
        gates = collect_gates(source_dir)
        total = sum(len(rows) for rows in gates.values())
        write_roster(gates)
        data = ROSTER.read_bytes()
        size = len(data)
        sha = hashlib.sha256(data).hexdigest()
        write_header_pins(size, sha)
        print(f"wrote {ROSTER.relative_to(ROOT)}: {total} gap rows")
        print(f"  bytes={size} sha256={sha}")
        print(f"pins rewritten in {HEADER.relative_to(ROOT)}")
        return 0

    problems: list[str] = []
    data = ROSTER.read_bytes()
    size = len(data)
    sha = hashlib.sha256(data).hexdigest()
    try:
        pin_size, pin_sha = read_header_pins()
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    if size != pin_size:
        problems.append(f"roster size {size} != pinned {pin_size}")
    if sha != pin_sha:
        problems.append(f"roster sha256 {sha} != pinned {pin_sha}")
    roster = json.loads(data)
    if roster.get("schema") != SCHEMA:
        problems.append(f"roster schema is {roster.get('schema')!r}, expected {SCHEMA}")
    gates = roster.get("gates")
    if not isinstance(gates, dict):
        problems.append("roster has no gates object")
        gates = {}
    if set(gates) != set(GATE_REPORTS):
        problems.append(f"roster gate keys {sorted(gates)} != {sorted(GATE_REPORTS)}")

    if not problems:
        for build_dir in build_dirs:
            dir_problems = verify_build_dir(build_dir, gates)
            problems.extend(dir_problems)
            if not dir_problems:
                counts = ", ".join(
                    f"{gate}={len(gates[gate])}" for gate in GATE_REPORTS
                )
                print(f"OK {build_dir.relative_to(ROOT)} ({counts})")

    if problems:
        for problem in problems:
            print(f"MISMATCH {problem}", file=sys.stderr)
        print(f"{len(problems)} mismatch(es)", file=sys.stderr)
        return 1
    print(f"roster {ROSTER.relative_to(ROOT)} bytes={size} sha256={sha}")
    print(f"verified across {len(build_dirs)} build director{'y' if len(build_dirs)==1 else 'ies'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
