#!/usr/bin/env python3
"""Generate the mindmap closed-layout cohort ledger (D-439).

Selection: a mindmap original whose frontmatter is a folded description plus
the closed `config.layout` namespace with exactly `layout: tidy-tree`, and
whose body stays inside the admitted tree grammar (no `::icon(...)` annotation
and no anonymous `((...))` top-level root shorthand). The tidy-tree engine is
an external layout engine the terminal projection does not run, so the leaf is
inert and disclosed under `Mindmap native projection v1`; the body is
transcribed as the native tree.

Usage: python3 tools/mermaid/generate_mindmap_tidy_d439_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/mindmap-tidy-d439-cohort.json")
SCHEMA = "loom_mermaid_mindmap_tidy_d439_cohort/v1"
COHORT_DIGEST = "36d3a7cf96fba3cd978cf781e3d3dce98c8b0d8ce7dec6611ef0f5cfbead3193"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"
FOOTER = [
    "Mindmap native projection v1",
    "config.layout=tidy-tree -> external layout engine; not applied",
]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None
    end = text.find("\n---\n", 4)
    if end == -1:
        return None
    return text[4:end], text[end + 5 :]


def frontmatter_ok(frontmatter: str) -> bool:
    """Mirror of the C++ closed parser: description, then config -> layout."""
    seen = False
    folded = False
    payload = False
    config_seen = False
    layout_seen = False
    block_indent = 0
    for line in frontmatter.split("\n"):
        if not line.strip():
            continue
        if folded and not config_seen and line.startswith(" "):
            indent = len(line) - len(line.lstrip(" "))
            if block_indent == 0:
                block_indent = indent
            if indent != block_indent:
                return False
            payload = True
            continue
        if config_seen:
            if line != "  layout: tidy-tree" or layout_seen:
                return False
            layout_seen = True
            continue
        if line == "config:":
            if not seen:
                return False
            config_seen = True
            continue
        if seen or not line.startswith("description: "):
            return False
        seen = True
        value = line[len("description: ") :]
        if value == ">-":
            folded = True
        elif not value:
            return False
    if not seen or (folded and not payload) or not config_seen or not layout_seen:
        return False
    return True


def body_in_grammar(body: str) -> bool:
    """The admitted tree grammar: no icon annotations, no anonymous root."""
    for line in body.split("\n"):
        stripped = line.lstrip(" ")
        if not stripped:
            continue
        if "::icon(" in stripped:
            return False
        if stripped.startswith("(("):
            return False
    return True


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}
    rows = []
    for path in sorted((cases / "mindmap").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        split = split_frontmatter(path.read_text())
        if split is None:
            continue
        if not frontmatter_ok(split[0]):
            continue
        if not body_in_grammar(split[1]):
            continue
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "expected_footer_lines": list(FOOTER),
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
            }
        )
    if len(rows) != 3:
        raise ValueError(f"cohort drifted: {len(rows)} rows: {[r['path'] for r in rows]}")
    rows.sort(key=lambda row: row["path"])
    digest = sha256_bytes("".join(f"{r['path']}\t{r['sha256']}\n" for r in rows).encode())
    if COHORT_DIGEST != "PLACEHOLDER" and digest != COHORT_DIGEST:
        raise ValueError(f"cohort drifted: {digest}")
    manifest_sha = sha256_bytes(manifest_bytes)
    if MANIFEST_SHA256 != "PLACEHOLDER" and manifest_sha != MANIFEST_SHA256:
        raise ValueError(f"manifest changed: {manifest_sha}")
    return {
        "schema": SCHEMA,
        "decision": "D-439",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "mindmap original whose frontmatter is a folded description plus the "
                    "closed config.layout namespace with exactly layout: tidy-tree, and "
                    "whose body uses no icon annotation and no anonymous ((...)) root",
            "cohort_digest": digest,
            "exclusions": "other layout values, other config keys, init directives, "
                          "::icon(...) annotations and the ((...)) top-level shorthand "
                          "stay whole-source",
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
