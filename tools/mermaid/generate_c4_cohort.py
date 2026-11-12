#!/usr/bin/env python3
"""Generate the RFC 0031 c4-family renderer cohort ledger.

The c4 renderer (mermaid_c4_impl.cpp) projects the C4 element/relation
model (C4Context/Container/Component/Deployment/Dynamic headers, typed
element calls, Rel* relations, brace-delimited nesting) to a bounded
native indented outline, preserving every element and relationship as
text. It has no box/arrow/color/layout contract, so fixtures carrying
Update*Style/UpdateLayoutConfig stay on whole-source fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards
pin the draw / style partition; manifest shas are pinned; the review
block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_c4_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/c4")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/c4-cohort.json")
SCHEMA = "loom_mermaid_c4_cohort/v1"

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_c4_cohort.cpp. Populated from live measurement.
NARROW_DRAW: set[str] = {
    "c4/nightly-check/relationships/bidirectional-relationship.mmd",
    "c4/nightly-check/relationships/rel-back.mmd",
    "c4/nightly-check/relationships/technology-and-description.mmd",
    "c4/pr-check/directional-relationships.mmd",
    "c4/pr-check/dynamic-diagram.mmd",
}

STYLE_RE = re.compile(r"Update(?:Element|Rel)Style|UpdateLayoutConfig")


def split_frontmatter(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None, text


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, c4 renderer "
                           "cohort, 2026-11-12, task a1ee3ba2e69a1f2c3)"),
        "date": "2026-11-12",
        "verdict": "approved",
        "note": ("Initial REQUEST-CHANGES; re-reviewed to APPROVE after fixes. "
                 "MINOR-1: the title now goes through the same protected "
                 "emission path (row/width/Work guards) and a duplicate title "
                 "fails closed. NIT-1/NIT-2: header comment corrected and "
                 "depth/statement cap tests added. NIT-3: relation extra "
                 "arguments are now strictly quoted or $attributes, matching "
                 "the element path. NIT-4: comment syntax corrected to %%. "
                 "NIT-5 python/C++ gate parity kept, guarded by the durable "
                 "gtest. The reviewer's Python reference model reproduced the "
                 "exact 23 draw / 9 gated partition across all 32 fixtures; a "
                 "follow-up unified the row cap over the combined title plus "
                 "statement output."),
    }
    existing = repo_root / LEDGER
    if existing.exists():
        try:
            prior = json.loads(existing.read_text())
            if isinstance(prior.get("review"), dict):
                review = prior["review"]
        except json.JSONDecodeError:
            pass
    return review


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    draw, styled = set(), set()
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        text = path.read_text()
        rel = path.relative_to(
            repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        frontmatter, body = split_frontmatter(text)
        is_style = bool(STYLE_RE.search(body))
        if not is_style and frontmatter is not None and re.search(
                r"^\s*(theme|look|config)\s*:", frontmatter, re.M):
            is_style = True
        (styled if is_style else draw).add(rel)

    if (len(draw), len(styled)) != (23, 9):
        raise ValueError(
            f"c4 partition drift: draw={len(draw)} styled={len(styled)} "
            "(expected 23/9)")

    rows = []
    for rel in sorted(draw):
        observed_sha = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        narrow = "drawn" if rel in NARROW_DRAW else "source"
        rows.append({
            "path": rel,
            "family": "c4",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": "whole-and-body",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": narrow,
                "restored_240": "drawn",
            },
            "expected_body_states": {
                "wide_240": "drawn",
                "narrow_40": narrow,
                "restored_240": "drawn",
            },
            "ledger_note": (
                "c4 renderer projects the typed element/relation model to a "
                "bounded native indented outline; no style/layout gate is "
                "present so the whole original draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "c4-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_c4_impl.cpp",
            "grammar": ("C4{Context,Container,Component,Deployment,Dynamic}; "
                        "typed element calls, Rel*/BiRel relations, brace blocks"),
            "theme_gate": (
                "frontmatter theme/look/config or an in-body "
                "Update*Style/UpdateLayoutConfig statement keeps the whole "
                "original on source fallback"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "c4 fixtures with no Update*Style/UpdateLayoutConfig or frontmatter gate",
            "roots": ["c4"],
            "draw_count": len(draw),
            "styled_excluded_count": len(styled),
            "styled_excluded": sorted(styled),
        },
        "review": build_review(repo_root),
        "counts": {
            "total": len(rows),
            "predicted_draw": len(rows),
            "predicted_blocked": 0,
            "predicted_original_draw": len(rows),
        },
        "rows": rows,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo-root", default=".")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
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


if __name__ == "__main__":
    raise SystemExit(main())
