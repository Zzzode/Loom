#!/usr/bin/env python3
"""Generate the cynefin closed-frontmatter cohort ledger (D-431).

Selection: a cynefin original whose frontmatter is exactly a folded description
plus `config:` with closed shapes: `cynefin.{boundaryAmplitude, seed, width,
height, showDomainDescriptions}`, or `theme: base` with
`themeVariables.cynefin.{complexBg, clearBg, boundaryColor}`.

The native projection transcribes statements and draws no boundary curve,
colour, size or the automatic domain descriptions, so every leaf is inert and
disclosed under `Cynefin native projection v1`. The oracle is exact: the art
above the footer equals the config-stripped twin's art.

Usage: python3 tools/mermaid/generate_cynefin_ns_d431_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/cynefin-ns-d431-cohort.json")
SCHEMA = "loom_mermaid_cynefin_ns_d431_cohort/v1"
HEADER = "Cynefin native projection v1"
HEX = re.compile(r"^#(?:[0-9a-fA-F]{6})$")
DECIMAL = re.compile(r"^-?\d+(?:\.\d+)?$")
COHORT_DIGEST = "da252278ed3c2a99b699f4735a8dcbc3fd49521192b5b71803b8c5498136b303"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def unquote(value: str) -> str:
    if len(value) >= 2 and value[0] in "'\"" and value[-1] == value[0]:
        return value[1:-1]
    return value


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None
    end = text.find("\n---\n", 4)
    if end == -1:
        raise ValueError("unterminated frontmatter")
    return text[4:end]


def parse(frontmatter: str):
    section = None
    seen = set()
    theme = None
    leaves = []
    for line in frontmatter.split("\n"):
        if line == "---":
            continue
        if line.startswith("description:"):
            section = "description"
            continue
        if line == "config:":
            section = "config"
            continue
        if section == "description":
            if not line.startswith("  ") or line.startswith("   ") or not line.strip():
                return None
            continue
        if section == "config":
            if line == "  cynefin:" and "cynefin" not in seen:
                seen.add("cynefin")
                section = "cynefin"
                continue
            if line == "  themeVariables:" and "themeVariables" not in seen:
                seen.add("themeVariables")
                section = "themevars"
                continue
            if line.startswith("  theme: "):
                value = line[len("  theme: "):].strip()
                if "theme" in seen or value != "base":
                    return None
                seen.add("theme")
                theme = value
                continue
            return None
        if section == "cynefin":
            key, sep, value = line[4:].partition(":")
            if not sep or not line.startswith("    ") or line.startswith("     "):
                return None
            key = key.strip()
            value = unquote(value.strip())
            if key in ("boundaryAmplitude", "seed", "width", "height") and DECIMAL.match(value):
                leaves.append(f"config.cynefin.{key}={value} -> layout geometry; not applied")
            elif key == "showDomainDescriptions" and value in ("true", "false"):
                leaves.append(
                    "config.cynefin.showDomainDescriptions="
                    f"{value} -> automatic domain descriptions; not applied")
            else:
                return None
            if f"cynefin.{key}" in seen:
                return None
            seen.add(f"cynefin.{key}")
            continue
        if section == "themevars":
            if line == "    cynefin:" and "cyvars" not in seen:
                seen.add("cyvars")
                section = "cyvars"
                continue
            return None
        if section == "cyvars":
            key, sep, value = line[6:].partition(":")
            if not sep or not line.startswith("      ") or line.startswith("       "):
                return None
            key = key.strip()
            value = unquote(value.strip())
            if key not in ("complexBg", "clearBg", "boundaryColor") or not HEX.match(value):
                return None
            if f"cyvar.{key}" in seen:
                return None
            seen.add(f"cyvar.{key}")
            suffix = (" -> domain boundary colour; not applied"
                      if key == "boundaryColor" else " -> domain fill colour; not applied")
            leaves.append(f"config.themeVariables.cynefin.{key}={value}{suffix}")
            continue
        return None
    if theme is None and not leaves:
        return None
    return theme, leaves


def footer_lines(theme, leaves):
    lines = [HEADER]
    if theme:
        lines.append(f"config.theme={theme} -> colour palette; not applied")
    lines.extend(leaves)
    return lines


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for path in sorted((cases / "cynefin").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        frontmatter = split_frontmatter(path.read_text())
        if frontmatter is None:
            continue
        parsed = parse(frontmatter)
        if parsed is None:
            continue
        theme, leaves = parsed
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "theme": theme,
                "leaves": leaves,
                "expected_footer_lines": footer_lines(theme, leaves),
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
            }
        )
    if len(rows) != 5:
        raise ValueError(f"cohort drifted: {len(rows)} rows")
    rows.sort(key=lambda row: row["path"])
    digest = sha256_bytes("".join(f"{r['path']}\t{r['sha256']}\n" for r in rows).encode())
    if COHORT_DIGEST != "PLACEHOLDER" and digest != COHORT_DIGEST:
        raise ValueError(f"cohort drifted: {digest}")
    manifest_sha = sha256_bytes(manifest_bytes)
    if MANIFEST_SHA256 != "PLACEHOLDER" and manifest_sha != MANIFEST_SHA256:
        raise ValueError(f"manifest changed: {manifest_sha}")
    return {
        "schema": SCHEMA,
        "decision": "D-431",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "cynefin original whose frontmatter is a folded description plus "
                    "closed cynefin/themeVariables shapes",
            "cohort_digest": digest,
            "theme_values": ["base"],
            "exclusions": "look keys, other theme names, unknown leaves, init "
                          "directives and style statements stay whole-source",
        },
        "review": {
            "reviewer_agent": "TL exemption 2026-11-22: family slice without a design "
                              "gate; the gate is the test document plus the twin oracle",
            "verdict": "approved",
        },
        "counts": {"total": len(rows)},
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
