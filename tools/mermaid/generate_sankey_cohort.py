#!/usr/bin/env python3
"""Generate the RFC 0031 D-211 sankey-family cohort ledger.

The sankey renderer (mermaid_sankey_impl.cpp) handles the `sankey-beta`
dialect: a header followed by three-field CSV link rows
(`source,target,value`) projected to a bounded native outline. It does not
implement sankey flow geometry or color, so any config/theme frontmatter
keeps the whole source on fallback.

Renderer-free; the durable gtest enforces every state live. A hard drift
guard pins the family partition (18 total / 1 native / 17 gated) and the
single cohort row with manifest sha pins; the review block is preserved
across regeneration.

Usage: python3 tools/mermaid/generate_sankey_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/sankey")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/sankey-cohort.json")
SCHEMA = "loom_mermaid_sankey_cohort/v1"
_FENCE = re.compile(r"^---$")

# Hard drift-guard tuple, measured through the production renderer. Only the
# single sankey-beta, description-only fixture is reachable; the 17 sankey
# dialect sources all carry config.sankey layout/color overrides.
_FAMILY_TOTAL = 18
_NATIVE_DRAW = 1
_GATED = 17


def split_frontmatter(text):
    """Return (frontmatter_lines_or_None, malformed_bool)."""
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, False
    for close in range(1, len(lines)):
        if _FENCE.fullmatch(lines[close]):
            return lines[1:close], False
        if lines[close].strip() == "---":
            return None, True  # indented fence cannot close the envelope
    return None, True  # unclosed fence


def frontmatter_gated(frontmatter, malformed):
    if malformed:
        return True
    if frontmatter is None:
        return False
    for line in frontmatter:
        leading = line[: len(line) - len(line.lstrip(" \t"))]
        if "\t" in leading:
            return True
        key_start = next((i for i, c in enumerate(line) if c != " "), None)
        if key_start is None:
            continue
        if key_start != 0:
            continue
        column0 = line[key_start:]
        if column0.startswith("#"):
            continue
        if ":" not in column0:
            return True
        key = column0.split(":", 1)[0].strip()
        if key != "description":
            return True
    return False


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_review(repo_root):
    review = {
        "reviewer_agent": "pending independent in-session adversarial review",
        "date": "2026-11-14",
        "verdict": "pending",
        "note": ("Filled by the independent review agent; preserved across "
                 "regeneration once approved."),
    }
    existing = repo_root / LEDGER
    if existing.exists():
        try:
            prior = json.loads(existing.read_text())
            if isinstance(prior.get("review"), dict) and \
                    prior["review"].get("verdict") == "approved":
                review = prior["review"]
        except json.JSONDecodeError:
            pass
    return review


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    draw, gated = set(), set()
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        text = path.read_text()
        rel = path.relative_to(
            repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        frontmatter, malformed = split_frontmatter(text)
        if frontmatter_gated(frontmatter, malformed):
            gated.add(rel)
        else:
            draw.add(rel)

    if (len(draw), len(gated)) != (_NATIVE_DRAW, _GATED) or \
            len(draw) + len(gated) != _FAMILY_TOTAL:
        raise ValueError(
            f"sankey partition drift: draw={len(draw)} gated={len(gated)} "
            f"total={len(draw) + len(gated)} "
            f"(expected {_NATIVE_DRAW}/{_GATED}/{_FAMILY_TOTAL})")

    rows = []
    for rel in sorted(draw):
        observed_sha = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        # The single reachable link is short enough to draw at 40 columns.
        rows.append({
            "path": rel,
            "family": "sankey",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": "whole-and-body",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn",
                "restored_240": "drawn",
            },
            "expected_body_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn",
                "restored_240": "drawn",
            },
            "ledger_note": (
                "sankey-beta CSV link validated against the closed three-"
                "field grammar and transcribed as a bounded outline; no "
                "config/theme frontmatter gate is present so it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "sankey-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_sankey_impl.cpp",
            "grammar": (
                "sankey-beta header; strict two-space indented CSV link rows "
                "source,target,value with printable free-text node labels and "
                "a bounded positive integer value"),
            "theme_gate": (
                "a column-0 frontmatter fence carrying any top-level key "
                "besides description, or an indented/unclosed fence, keeps "
                "the whole original on source fallback"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": (
                "sankey fixtures without a config/theme frontmatter gate; "
                "only the description-only sankey-beta original is reachable"),
            "roots": ["sankey"],
            "family_total": _FAMILY_TOTAL,
            "native_draw_count": _NATIVE_DRAW,
            "gated_count": _GATED,
            "gated": sorted(gated),
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
