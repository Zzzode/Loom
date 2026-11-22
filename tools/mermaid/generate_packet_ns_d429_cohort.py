#!/usr/bin/env python3
"""Generate the packet closed-namespace cohort ledger (D-429).

Renderer-free. Selection: a packet original whose frontmatter is exactly a
folded description, an optional quoted `title:`, and `config:` > `packet:` with
leaves from {showBits, bitOrder, bitsPerRow}.

The native projection transcribes fields, never a bit grid, so the three leaves
are inert and disclosed; the frontmatter title is honoured as a transcribed
`title <text>` line. The oracle is exact: the art above the footer equals the
config-stripped twin's art, and only the disclosure footer differs.

Usage: python3 tools/mermaid/generate_packet_ns_d429_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/packet-ns-d429-cohort.json")
SCHEMA = "loom_mermaid_packet_ns_d429_cohort/v1"
EXPECTED_TOTAL = 2
HEADER = "Packet native projection v1"
LEAVES = [
    ("bitOrder", " -> bit numbering direction; not applied"),
    ("bitsPerRow", " -> ruler row grouping; not applied"),
    ("showBits", " -> per-bit position rulers; not applied"),
]

COHORT_DIGEST = "aca34405d0d3e6c100f7ba833adabf86e9e2e9c00473f9c81fdcc17bb4c1507e"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def cohort_digest(rows) -> str:
    return sha256_bytes("".join(f"{r['path']}\t{r['sha256']}\n" for r in rows).encode())


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None, text
    end = text.find("\n---\n", 4)
    if end == -1:
        raise ValueError("unterminated frontmatter")
    return text[4:end], text[end + 5 :]


def parse(frontmatter: str):
    section = None
    title = None
    leaves = {}
    for line in frontmatter.split("\n"):
        if line == "---":
            continue
        if line.startswith("description:"):
            section = "description"
            continue
        if line.startswith("title:"):
            if title is not None or section == "config":
                return None
            value = line[len("title:") :].strip()
            if len(value) < 2 or value[0] != '"' or value[-1] != '"':
                return None
            title = value[1:-1]
            continue
        if line == "config:":
            section = "config"
            continue
        if section == "description":
            if not line.startswith("  ") or line.startswith("   ") or not line.strip():
                return None
            continue
        if section == "config":
            if line != "  packet:":
                return None
            section = "packet"
            continue
        if section == "packet":
            if not line.startswith("    ") or line.startswith("     "):
                return None
            key, sep, value = line.strip().partition(":")
            if not sep or key in leaves:
                return None
            leaves[key] = value.strip()
            continue
        return None
    if not leaves:
        return None
    for key in leaves:
        if key not in {name for name, _ in LEAVES}:
            return None
    return title, leaves


def footer_lines(leaves) -> list[str]:
    lines = [HEADER]
    for key, suffix in LEAVES:
        if key in leaves:
            lines.append(f"config.packet.{key}={leaves[key]}{suffix}")
    return lines


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for path in sorted((cases / "packet").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        frontmatter, body = split_frontmatter(path.read_text())
        if frontmatter is None:
            continue
        parsed = parse(frontmatter)
        if parsed is None:
            continue
        title, leaves = parsed
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "title": title,
                "leaves": leaves,
                "expected_footer_lines": footer_lines(leaves),
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
            }
        )
    if len(rows) != EXPECTED_TOTAL:
        raise ValueError(f"cohort drifted: expected {EXPECTED_TOTAL}, got {len(rows)}")
    rows.sort(key=lambda row: row["path"])
    digest = cohort_digest(rows)
    if COHORT_DIGEST != "PLACEHOLDER" and digest != COHORT_DIGEST:
        raise ValueError(f"cohort drifted: {digest}")
    manifest_sha = sha256_bytes(manifest_bytes)
    if MANIFEST_SHA256 != "PLACEHOLDER" and manifest_sha != MANIFEST_SHA256:
        raise ValueError(f"manifest changed without review: {manifest_sha}")
    return {
        "schema": SCHEMA,
        "decision": "D-429",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "packet original whose frontmatter is a folded description, "
                    "an optional quoted title and config: > packet: with closed leaves",
            "cohort_digest": digest,
            "leaves": [name for name, _ in LEAVES],
            "exclusions": "theme/look keys, unknown packet leaves, init directives "
                          "and style statements stay whole-source",
        },
        "review": {
            "reviewer_agent": "TL exemption 2026-11-22: family slice without a design "
                              "gate; the gate is the test document plus the twin oracle",
            "verdict": "approved",
        },
        "counts": {
            "total": len(rows),
            "titles": sum(1 for row in rows if row["title"]),
            "leaves": {name: sum(1 for row in rows if name in row["leaves"])
                       for name, _ in LEAVES},
        },
        "rows": rows,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = pathlib.Path(args.repo_root)
    ledger = build(root)
    rendered = json.dumps(ledger, indent=2, ensure_ascii=False) + "\n"
    target = root / LEDGER
    if args.check:
        if target.read_text() != rendered:
            print("ledger is out of date", file=sys.stderr)
            return 1
        print("ledger current:", json.dumps(ledger["counts"]))
        return 0
    target.write_text(rendered)
    print(json.dumps(ledger["counts"], indent=2))
    print("cohort digest", ledger["selection"]["cohort_digest"])
    print("manifest sha256", ledger["upstream"]["manifest_sha256"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
