#!/usr/bin/env python3
"""Generate the RFC 0031 treemap-family renderer cohort ledger.

The treemap renderer (mermaid_treemap_impl.cpp) projects the indentation
tree of quoted containers and numeric leaves to a bounded native indented
outline, preserving hierarchy and every source value verbatim. It has no
color or number-format contract, so classDef/::: styled fixtures and
frontmatter config (treemap.valueFormat) fixtures stay on whole-source
fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards
pin the draw / styled / config partition; manifest shas are pinned; the
review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_treemap_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/treemap")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/treemap-cohort.json")
SCHEMA = "loom_mermaid_treemap_cohort/v1"

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_treemap_cohort.cpp. Populated from live measurement.
NARROW_DRAW: set[str] = {
    "treemap/nightly-check/17-should-render-treemap-with-readable-text-on-dark-theme.mmd",
    "treemap/nightly-check/18-should-render-treemap-with-readable-text-on-neutral-theme.mmd",
    "treemap/nightly-check/2-should-render-a-hierarchical-treemap.mmd",
    "treemap/nightly-check/6-should-handle-multiple-levels-of-nesting.mmd",
    "treemap/nightly-check/8e-should-handle-simple-thousands-separator.mmd",
    "treemap/nightly-check/8g-should-handle-scientific-notation-format.mmd",
    "treemap/pr-check/1-should-render-a-basic-treemap.mmd",
    "treemap/pr-check/11-should-handle-comments.mmd",
    "treemap/pr-check/5-should-render-with-a-forest-theme.mmd",
}


def split_frontmatter(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None, text


def partition(text, frontmatter, body):
    """Return 'styled', 'config' or 'draw' from the visual-contract gates."""
    if frontmatter is not None and re.search(r"^\s*(theme|look|config)\s*:",
                                             frontmatter, re.M):
        return "config"
    # classDef statements or ::: node style tags are a color/stroke contract.
    for line in body.split("\n"):
        stripped = line.strip()
        if stripped.startswith("classDef") or ":::" in stripped:
            return "styled"
    if "%%{" in body and "theme" in body:
        return "styled"
    return "draw"


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, treemap renderer "
                           "cohort, 2026-11-12, task a85ecf9e80f2a47e7)"),
        "date": "2026-11-12",
        "verdict": "approved",
        "note": ("APPROVE with zero MAJOR; two MINORs resolved before commit. "
                 "M1: a value node is now enforced as a leaf (a node nested "
                 "deeper than an open leaf fails closed) with regression "
                 "coverage for leaf-child rejection and valid siblings. M2: "
                 "depth-32 and node-512 cap exhaustion now have direct "
                 "fail-closed tests. The reviewer's independent Python "
                 "reference model reproduced the exact 10 draw / 10 styled / "
                 "6 config partition across all 26 fixtures. Five NITs are "
                 "intentional sibling-renderer parity (N1/N5) or "
                 "over-rejection in the safe direction (N2), with N3/N4 "
                 "harmless; kept as-is."),
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

    groups = {"draw": set(), "styled": set(), "config": set()}
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        text = path.read_text()
        rel = path.relative_to(
            repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        frontmatter, body = split_frontmatter(text)
        groups[partition(text, frontmatter, body)].add(rel)

    if (len(groups["draw"]), len(groups["styled"]), len(groups["config"])) != (10, 10, 6):
        raise ValueError(
            f"treemap partition drift: draw={len(groups['draw'])} "
            f"styled={len(groups['styled'])} config={len(groups['config'])} "
            "(expected 10/10/6)")

    rows = []
    for rel in sorted(groups["draw"]):
        observed_sha = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        narrow = "drawn" if rel in NARROW_DRAW else "source"
        rows.append({
            "path": rel,
            "family": "treemap",
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
                "treemap renderer projects the quoted container/leaf "
                "hierarchy to a bounded native indented outline; no "
                "classDef/config gate is present so the whole original draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "treemap-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_treemap_impl.cpp",
            "grammar": "treemap[-beta]; quoted containers and numeric leaves over 2-space indentation",
            "theme_gate": (
                "frontmatter theme/look/config or an in-body classDef/::: "
                "style keeps the whole original on source fallback"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "treemap fixtures with no classDef/::: style and no frontmatter config",
            "roots": ["treemap"],
            "draw_count": len(groups["draw"]),
            "styled_excluded_count": len(groups["styled"]),
            "styled_excluded": sorted(groups["styled"]),
            "config_excluded_count": len(groups["config"]),
            "config_excluded": sorted(groups["config"]),
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
