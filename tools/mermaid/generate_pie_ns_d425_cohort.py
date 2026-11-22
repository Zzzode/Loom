#!/usr/bin/env python3
"""Generate the pie closed-config-namespace cohort ledger (D-425).

Renderer-free. Selection (semantic, source-state markers only): a pie original
with a reviewed folded description (`description: >-`) whose `config:` block
carries only the closed leaves

    logLevel                0..3 / trace..fatal
    pie.highlightSlice      unquoted single token (hover or a slice name)
    pie.legendPosition      bottom
    pie.donutHole           signed decimal (out-of-range values included: the
                            terminal projection has no donut hole either way)
    pie.textPosition        0..1
    pie.useMaxWidth         true / false

Each leaf is a pixel-only presentation the terminal cannot express or a
logger-only key, so every one is disclosed and inert: the drawn art must equal
the config-free twin's art byte for byte. The criterion is the source's own
state marker (the leaf), never a render-difference proxy.

Usage: python3 tools/mermaid/generate_pie_ns_d425_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/pie-ns-d425-cohort.json")
SCHEMA = "loom_mermaid_pie_ns_d425_cohort/v1"
EXPECTED_TOTAL = 8
HEADER = "Pie native projection v1"
LEAVES = [
    ("logLevel", "config.logLevel=", " -> logger only; not applied"),
    ("pie.donutHole", "config.pie.donutHole=", " -> donut hole; not applied"),
    ("pie.highlightSlice", "config.pie.highlightSlice=", " -> slice highlight; not applied"),
    ("pie.legendPosition", "config.pie.legendPosition=", " -> legend placement; not applied"),
    ("pie.textPosition", "config.pie.textPosition=", " -> label radius; not applied"),
    ("pie.useMaxWidth", "config.pie.useMaxWidth=", " -> responsive scaling; not applied"),
]

COHORT_DIGEST = "1a8a8a667f715cdede98e665e19cd83db198a3e9a1fe064639f14badbcff4f6f"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def cohort_digest(rows) -> str:
    payload = "".join(f"{row['path']}\t{row['sha256']}\n" for row in rows)
    return sha256_bytes(payload.encode())


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None, text
    end = text.find("\n---\n", 4)
    if end == -1:
        raise ValueError("unterminated frontmatter")
    return text[4:end], text[end + 5 :]


def namespace_leaves(frontmatter: str):
    """Return {leaf: value} for the closed pie namespace, or None."""
    leaves = {}
    state = None  # "description" | "config" | "pie"
    for line in frontmatter.split("\n"):
        stripped = line.strip()
        if state is None:
            if line == "description: >-":
                state = "description"
                continue
            return None
        if state == "description":
            if line == "config:":
                state = "config"
                continue
            if not line.startswith("  ") or line.startswith("   ") or not stripped:
                return None
            continue
        if state == "config":
            if line == "  pie:":
                state = "pie"
                continue
            if line.startswith("  logLevel: ") and not line.startswith("   "):
                leaves["logLevel"] = stripped[len("logLevel:"):].strip()
                continue
            return None
        if state == "pie":
            if not line.startswith("    ") or line.startswith("     "):
                return None
            key, sep, value = stripped.partition(":")
            if not sep:
                return None
            full = "pie." + key
            if full not in {name for name, _, _ in LEAVES} or full in leaves:
                return None
            leaves[full] = value.strip()
    return leaves or None


def footer_lines(leaves) -> list[str]:
    lines = [HEADER]
    for key, prefix, suffix in LEAVES:
        if key in leaves:
            lines.append(prefix + leaves[key] + suffix)
    return lines


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    rows = []
    for path in sorted((cases / "pie").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        frontmatter, body = split_frontmatter(path.read_text())
        if frontmatter is None:
            continue
        leaves = namespace_leaves(frontmatter)
        if leaves is None:
            continue
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "leaves": leaves,
                "expected_footer_lines": footer_lines(leaves),
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
                "ledger_note": (
                    "D-425 closed pie config namespace: every admitted leaf is a "
                    "pixel-only presentation the terminal cannot express or a "
                    "logger-only key; the art must equal the config-free twin."),
            }
        )
    if len(rows) != EXPECTED_TOTAL:
        raise ValueError(f"cohort drifted: expected {EXPECTED_TOTAL}, got {len(rows)}")
    rows.sort(key=lambda row: row["path"])
    digest = cohort_digest(rows)
    if COHORT_DIGEST != "PLACEHOLDER" and digest != COHORT_DIGEST:
        raise ValueError(f"cohort row set drifted: digest {digest} != {COHORT_DIGEST}")
    manifest_sha = sha256_bytes(manifest_bytes)
    if MANIFEST_SHA256 != "PLACEHOLDER" and manifest_sha != MANIFEST_SHA256:
        raise ValueError(f"manifest changed without review: {manifest_sha}")
    counts = {"total": len(rows)}
    for key, _, _ in LEAVES:
        counts[key] = sum(1 for row in rows if key in row["leaves"])
    return {
        "schema": SCHEMA,
        "decision": "D-425",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "pie original with a reviewed folded description and a "
                    "config block whose only leaves are the closed pie namespace",
            "cohort_digest": digest,
            "table": {name: True for name, _, _ in LEAVES},
            "exclusions": "pie theme rows stay with the RFC 0042 profile; the "
                          "four accTitle/accDescr theme rows need the pie body "
                          "accessibility no-op and stay whole-source",
        },
        "review": {
            "reviewer_agent": "TL exemption 2026-11-22: family slice opened without a "
                              "design gate; the gate is this test document plus the "
                              "twin-equality oracle it pins",
            "verdict": "approved",
        },
        "counts": counts,
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
    print("cohort digest", ledger["selection"]["cohort_digest"])
    print("manifest sha256", ledger["upstream"]["manifest_sha256"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
