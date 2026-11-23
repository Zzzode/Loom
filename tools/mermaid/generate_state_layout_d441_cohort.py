#!/usr/bin/env python3
"""Generate the state closed-layout cohort ledger (D-441).

Selection: the three state originals whose frontmatter is a folded description
plus a closed `config` block carrying only the corpus-backed `layout: elk`
selector (one of them also carries the already-admitted `fontFamily: courier`).
The terminal runs its own fixed directed routing, so the engine selector is
inert and disclosed under `State native projection v1`; the body is drawn as
the native state projection.

Usage: python3 tools/mermaid/generate_state_layout_d441_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/state-layout-d441-cohort.json")
SCHEMA = "loom_mermaid_state_layout_d441_cohort/v1"
COHORT_DIGEST = "033b701ef3c3c12b792972f7dd6c477d154829424a7091027e273a79306d8fd1"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"
PATHS = (
    "state-diagram-v2/nightly-check/elk/elk-composite-entry-default-centering.mmd",
    "state-diagram-v2/nightly-check/elk/elk-start-end-edge-centering.mmd",
    "state-diagram-v2/pr-check/composite-with-a-loop-starts-on-its-start-state.mmd",
)
LEAVES = ("fontFamily", "layout")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None
    end = text.find("\n---\n", 4)
    if end == -1:
        return None
    return text[4:end], text[end + 5 :]


def parse(frontmatter: str):
    """Mirror of the closed state envelope over the admitted leaf table."""
    section = None
    leaves = {}
    for line in frontmatter.split("\n"):
        if not line.strip():
            continue
        if not line.startswith(" "):
            key = line.split(":", 1)[0]
            if key == "description":
                section = "description"
                continue
            if key == "config":
                section = "config"
                continue
            return None
        if section == "description":
            continue
        if section != "config":
            return None
        indent = len(line) - len(line.lstrip(" "))
        if indent != 2:
            return None
        key, sep, value = line.strip().partition(":")
        if not sep:
            return None
        key, value = key.strip(), value.strip()
        if key not in LEAVES or key in leaves:
            return None
        if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
            value = value[1:-1]
        if key == "layout" and value != "elk":
            return None
        if key == "fontFamily" and value != "courier":
            return None
        leaves[key] = value
    if "layout" not in leaves:
        return None
    return leaves


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for rel in PATHS:
        path = cases / rel
        split = split_frontmatter(path.read_text())
        if split is None:
            raise ValueError(f"missing frontmatter: {rel}")
        leaves = parse(split[0])
        if leaves is None:
            raise ValueError(f"shape drifted: {rel}")
        footer = ["State native projection v1"]
        if "fontFamily" in leaves:
            footer.append('config.fontFamily="courier" -> terminal font; not applied')
        footer.append("config.layout=elk -> external layout engine; not applied")
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "config_leaves": leaves,
                "expected_footer_lines": footer,
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
        "decision": "D-441",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "state original whose frontmatter is a folded description plus a "
                    "closed config block carrying the corpus-backed layout: elk selector "
                    "(optionally with the admitted fontFamily: courier leaf)",
            "cohort_digest": digest,
            "exclusions": "other engine values, unknown leaves, nested maps, init "
                          "directives and off-grid indentation stay whole-source",
        },
        "review": {
            "reviewer_agent": "TL exemption 2026-11-23: family slice without a design "
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
