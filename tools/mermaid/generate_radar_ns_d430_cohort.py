#!/usr/bin/env python3
"""Generate the radar closed-frontmatter cohort ledger (D-430).

Selection: a radar original whose frontmatter is exactly a folded description
plus `config:` carrying only closed shapes from the corpus: `theme: base`;
`themeVariables` with `fontSize` (uint), `cScale0` (#RRGGBB) and/or a nested
`radar.axisColor` (#RRGGBB); `radar` with `marginTop` (uint) and/or
`axisScaleFactor` (0..10 decimal).

The native projection transcribes an outline and draws no polygon, colour or
font size, so every leaf is inert and disclosed. The oracle is exact: the art
above the footer equals the config-stripped twin's art.

Usage: python3 tools/mermaid/generate_radar_ns_d430_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/radar-ns-d430-cohort.json")
SCHEMA = "loom_mermaid_radar_ns_d430_cohort/v1"
HEADER = "Radar native projection v1"
HEX = re.compile(r"^#(?:[0-9a-fA-F]{6})$")
COHORT_DIGEST = "2f0919506f544b339a0d2ebf70a367b59e1ad3fc33fc9d0d5b36ab2b4c23abb0"
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
    leaves = {}
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
            if line.startswith("  themeVariables:") and "themeVariables" not in seen:
                seen.add("themeVariables")
                section = "themevars"
                continue
            if line.startswith("  radar:") and "radar" not in seen:
                seen.add("radar")
                section = "radar"
                continue
            if line.startswith("  theme: "):
                value = line[len("  theme: "):].strip()
                if "theme" in seen or value != "base":
                    return None
                seen.add("theme")
                theme = value
                continue
            return None
        if section == "themevars":
            if line.startswith("    radar:") and "radar" not in seen:
                seen.add("radar")
                section = "radar"
                continue
            key, sep, value = line[4:].partition(":")
            if not line.startswith("    ") or line.startswith("     ") or not sep:
                return None
            key = key.strip()
            value = unquote(value.strip())
            if key == "fontSize" and value.isdigit() and "fontSize" not in leaves:
                leaves["themeVariables.fontSize"] = value
            elif key == "cScale0" and HEX.match(value) and "cScale0" not in leaves:
                leaves["themeVariables.cScale0"] = value
            else:
                return None
            continue
        if section == "radar":
            key, sep, value = line.strip().partition(":")
            indent = len(line) - len(line.lstrip(" "))
            if not sep or indent not in (4, 6):
                return None
            key = key.strip()
            value = unquote(value.strip())
            if (indent == 6 and key == "axisColor" and HEX.match(value)
                    and "themeVariables.radar.axisColor" not in leaves):
                leaves["themeVariables.radar.axisColor"] = value
            elif (indent == 4 and key == "marginTop" and value.isdigit()
                    and "radar.marginTop" not in leaves):
                leaves["radar.marginTop"] = value
            elif (indent == 4 and key == "axisScaleFactor"
                    and "radar.axisScaleFactor" not in leaves):
                if not re.match(r"^\d+(?:\.\d+)?$", value) or float(value) > 10.0:
                    return None
                leaves["radar.axisScaleFactor"] = value
            else:
                return None
            continue
        return None
    if theme is None and not leaves:
        return None
    return theme, leaves


def footer_lines(theme, leaves) -> list[str]:
    lines = [HEADER]
    if theme:
        lines.append(f"config.theme={theme} -> colour palette; not applied")
    if "themeVariables.fontSize" in leaves:
        lines.append(f"config.themeVariables.fontSize={leaves['themeVariables.fontSize']}"
                     " -> label font size; not applied")
    if "themeVariables.cScale0" in leaves:
        lines.append(f"config.themeVariables.cScale0={leaves['themeVariables.cScale0']}"
                     " -> series palette colour; not applied")
    if "themeVariables.radar.axisColor" in leaves:
        lines.append("config.themeVariables.radar.axisColor="
                     f"{leaves['themeVariables.radar.axisColor']} -> axis colour; not applied")
    if "radar.marginTop" in leaves:
        lines.append(f"config.radar.marginTop={leaves['radar.marginTop']}"
                     " -> layout margin; not applied")
    if "radar.axisScaleFactor" in leaves:
        lines.append(f"config.radar.axisScaleFactor={leaves['radar.axisScaleFactor']}"
                     " -> axis scale factor; not applied")
    return lines


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for path in sorted((cases / "radar").rglob("*.mmd")):
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
    if len(rows) != 3:
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
        "decision": "D-430",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "radar original whose frontmatter is a folded description plus "
                    "closed theme/themeVariables/config.radar shapes",
            "cohort_digest": digest,
            "theme_values": ["base"],
            "exclusions": "look keys, unknown theme names, unknown leaves, init "
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
