#!/usr/bin/env python3
"""Generate the RFC 0031 venn-family renderer cohort ledger.

The venn renderer (mermaid_venn_impl.cpp) projects the venn-beta grammar
(sets, unions, sizes, labels, nested text nodes) to a bounded native
indented outline. It has no circle/color contract, so fixtures carrying a
frontmatter theme/look/config key or an in-body style/classDef/::: statement
stay on whole-source fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / frontmatter-gated / body-style-gated partition; manifest shas are
pinned; the review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_venn_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/venn")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/venn-cohort.json")
SCHEMA = "loom_mermaid_venn_cohort/v1"

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_venn_cohort.cpp. Populated from live measurement.
NARROW_DRAW: set[str] = {
    "venn/nightly-check/10-should-render-a-venn-diagram-with-many-text-nodes.mmd",
    "venn/nightly-check/12-should-render-a-two-set-venn-with-asymmetric-sizes.mmd",
    "venn/nightly-check/4-should-render-a-venn-diagram-with-custom-set-sizes.mmd",
    "venn/nightly-check/5-should-render-a-venn-diagram-with-labels.mmd",
    "venn/pr-check/1-should-render-a-simple-two-set-venn-diagram.mmd",
    "venn/pr-check/17-should-render-a-venn-diagram-with-partial-pairwise-subsets.mmd",
    "venn/pr-check/2-should-render-a-three-set-venn-diagram.mmd",
    "venn/pr-check/3-should-render-a-venn-diagram-with-a-title.mmd",
    "venn/pr-check/6-should-render-a-venn-diagram-with-text-nodes.mmd",
    "venn/pr-check/8-should-render-a-venn-diagram-with-string-identifiers.mmd",
}


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



# RFC 0043: the admitted frontmatter shape is exactly one column-0 `config:`
# block whose sole leaf is an indent-2 `theme` in this family's closed table.
THEME_TABLE = {"dark", "redux-color", "redux-dark-color"}
TRIM_COLUMN0_KEY = False


def _theme_scalar(raw):
    """Mirror the C++ TU-local af_scalar copy (renderer-free)."""
    value = raw.strip(" \t")
    quote = None
    cut = None
    for n, c in enumerate(value):
        if quote is not None:
            if c == quote:
                quote = None
            continue
        if c in ("'", '"'):
            quote = c
            continue
        if c == "#" and (n == 0 or value[n - 1] in " \t"):
            cut = n
            break
    if cut is not None:
        value = value[:cut].strip(" \t")
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
        value = value[1:-1]
    return value


def frontmatter_theme(frontmatter):
    """Return ('none' | 'admitted' | 'gated', theme).

    Mirrors the C++ gate: a column-0 config block admits exactly one indent-2
    theme leaf from the closed table; every other visual key gates.
    """
    if frontmatter is None:
        return "none", None
    in_config = False
    saw_config = False
    theme = None
    for line in frontmatter.split("\n"):
        # Blank means spaces/tabs only. The renderers pre-reject any tab in the
        # source, so the C++ space-only key scan and this predicate agree on
        # every reachable input; no fixture in the three families has a tab.
        if line.strip(" \t") == "":
            continue
        key_start = len(line) - len(line.lstrip(" "))
        if key_start != 0:
            leaf = line[key_start:]
            if in_config:
                if leaf.startswith("#"):
                    continue
                colon = leaf.find(":")
                if colon < 0 or leaf[:colon] != "theme" or key_start != 2 or theme is not None:
                    return "gated", None
                value = _theme_scalar(leaf[colon + 1:])
                if value not in THEME_TABLE:
                    return "gated", None
                theme = value
            else:
                colon = leaf.find(":")
                key = leaf[:colon] if colon >= 0 else leaf
                if TRIM_COLUMN0_KEY:
                    key = key.strip(" ")
                if key in ("theme", "look", "config"):
                    return "gated", None
            continue
        if line.startswith("#"):
            continue
        colon = line.find(":")
        key = line[:colon] if colon >= 0 else line
        if TRIM_COLUMN0_KEY:
            key = key.strip(" ")
        if key == "config":
            if saw_config:
                return "gated", None
            saw_config = True
            in_config = True
        else:
            if key in ("theme", "look"):
                return "gated", None
            in_config = False
    if saw_config and theme is None:
        return "gated", None
    return ("admitted", theme) if theme is not None else ("none", None)


def gate_kind(text, frontmatter, body):
    """Return 'frontmatter', 'admitted', 'body_style' or None."""
    verdict, _theme = frontmatter_theme(frontmatter)
    if verdict == "gated":
        return "frontmatter"
    if re.search(r"^\s*style\s+\w", body, re.M) or "classDef" in body or ":::" in body:
        return "body_style"
    if verdict == "admitted":
        return "admitted"
    return None


def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, venn renderer "
                           "cohort, 2026-11-12, task ae64a3c38c96a9a8e)"),
        "date": "2026-11-12",
        "verdict": "approved",
        "note": ("APPROVE with zero MAJOR. The reviewer's Python port "
                 "reproduced the exact 11 draw / 7 frontmatter-gated / 3 "
                 "body-style-gated partition over all 21 fixtures and ran "
                 "114 adversarial cases. M2 (title bypassed the guarded "
                 "emission path) was fixed by routing the title through the "
                 "same wrap+guarded push with a combined row cap; N1 (loose "
                 "nesting) was tightened so sets/unions are depth 0 and text "
                 "nodes depth 1, with fail-closed regression tests. M1 "
                 "(quote-aware suffix scan) and M3 (C++/python gate parity on "
                 "hypothetical inputs only) are fail-closed with zero corpus "
                 "impact and sibling-renderer parity, deferred to a later "
                 "hardening batch."),
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

    draw, admitted, fm_gate, body_gate = set(), set(), set(), set()
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        text = path.read_text()
        rel = path.relative_to(
            repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        frontmatter, body = split_frontmatter(text)
        kind = gate_kind(text, frontmatter, body)
        if kind == "frontmatter":
            fm_gate.add(rel)
        elif kind == "body_style":
            body_gate.add(rel)
        elif kind == "admitted":
            admitted.add(rel)
        else:
            draw.add(rel)

    if (len(draw), len(admitted), len(fm_gate), len(body_gate)) != (11, 3, 3, 4):
        raise ValueError(
            f"venn partition drift: draw={len(draw)} "
            f"admitted={len(admitted)} frontmatter={len(fm_gate)} "
            f"body_style={len(body_gate)} (expected 11/3/3/4)")

    admitted_digest = hashlib.sha256("".join(
        f"{rel}\t{manifest_rows[rel]['sha256']}\n" for rel in sorted(admitted)
    ).encode()).hexdigest()

    rows = []
    for rel in sorted(draw):
        observed_sha = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        narrow = "drawn" if rel in NARROW_DRAW else "source"
        rows.append({
            "path": rel,
            "family": "venn",
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
                "venn renderer projects sets/unions/text to a bounded native "
                "outline; no style/theme gate is present so it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "venn-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_venn_impl.cpp",
            "grammar": "venn-beta; set/union names, :size, [\"label\"], nested text",
            "theme_gate": (
                "frontmatter theme/look/config or an in-body style/classDef/::: "
                "statement keeps the whole original on source fallback"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "venn fixtures with no frontmatter gate and no body style statement",
            "roots": ["venn"],
            "admission_rfc": 43,
            "draw_count": len(draw),
            "admitted_count": len(admitted),
            "admitted": sorted(admitted),
            "admitted_digest": admitted_digest,
            "family_total": len(draw) + len(admitted) + len(fm_gate) + len(body_gate),
            "frontmatter_gated_count": len(fm_gate),
            "frontmatter_gated": sorted(fm_gate),
            "body_style_gated_count": len(body_gate),
            "body_style_gated": sorted(body_gate),
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
