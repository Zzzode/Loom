#!/usr/bin/env python3
"""Generate the RFC 0031 radar-family renderer cohort ledger.

The radar renderer (mermaid_radar_impl.cpp) projects the radar-beta grammar
(axes, curves as inline numeric arrays or braced named blocks, and scalar
display directives) to a bounded native outline that preserves the source
indentation. It has no polygon geometry, radial scale or color contract, so
fixtures carrying a frontmatter theme/look/config key or a body style
statement stay on whole-source fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / frontmatter-gated / body-style-gated partition; manifest shas are
pinned; the review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_radar_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/radar")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/radar-cohort.json")
SCHEMA = "loom_mermaid_radar_cohort/v1"
_FENCE = re.compile(r"^---[ \t]*$")

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_radar_cohort.cpp. Populated from live measurement;
# edited only after a live re-measure (engineering rule 18). At 40 columns
# the two compact frames whose every line is <=23 columns (the simple and
# multiple-curve smoke tests) keep a native surface on both tracks; the
# complex frame carries a 46-column axis/value line and stays source narrow.
_NARROW_DRAW = {
    "radar/pr-check/should-render-a-radar-diagram-with-multiple-curves.mmd",
    "radar/pr-check/should-render-a-simple-radar-diagram.mmd",
}
_NARROW_DRAW_BODY: set[str] = set(_NARROW_DRAW)
_BODY_SOURCE: set[str] = set()


def split_frontmatter(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if _FENCE.fullmatch(lines[close]):
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None, text


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def gate_kind(frontmatter, body):
    """Return 'frontmatter', 'body_style' or None."""
    if frontmatter is not None and re.search(r"^\s*(theme|look|config)\s*:",
                                             frontmatter, re.M):
        return "frontmatter"
    if re.search(r"^\s*%%\{init", body, re.M) or \
            re.search(r"^\s*classDef", body, re.M) or ":::" in body or \
            re.search(r"^\s*style\s", body, re.M):
        return "body_style"
    return None


def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, radar renderer "
                           "cohort, 2026-11-14, task a78aba37d54c5d977)"),
        "date": "2026-11-14",
        "verdict": "approved",
        "note": ("Final verdict APPROVE after one re-review. The first pass "
                 "returned REQUEST CHANGES on one MAJOR and one MINOR, both "
                 "in this family's validator. MAJOR: is_value_list treated "
                 "any empty comma slot as skippable, so leading "
                 "('{,1}'), internal ('{1,,2}') and repeated ('{1,,}') "
                 "commas drew even though only a single optional trailing "
                 "comma is grammar-legal (the reviewer found 4,980 distinct "
                 "fail-open shapes across 26,334 combinations); it now "
                 "accepts an empty piece only as the final slot after a "
                 "real element, with inline and block regression pins. "
                 "MINOR: an empty multi-line curve block was accepted while "
                 "an inline '{}' was rejected; a block must now contain at "
                 "least one value line before its closing brace. The "
                 "reviewer independently reproduced the 3/3/0 partition with "
                 "two methods, ran 140 hand probes, the 26,334-case "
                 "differential and 60,000 fuzzed inputs with no crash, hang "
                 "or non-verbatim emission, and confirmed the trim-before-"
                 "colon frontmatter gate and printable-ASCII literal "
                 "hardening. The NITs (quoted YAML visual keys shared with "
                 "three approved siblings, over-broad ':::' substring gate, "
                 "the nominal row-count off-by-one re-gated downstream, and "
                 "asymmetric body-gate prefixes) are fail-safe or faithful "
                 "with no corpus sample and are unchanged per the "
                 "blast-radius rule."),
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

    draw, fm_gate, body_gate = set(), set(), set()
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        text = path.read_text()
        rel = path.relative_to(
            repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        frontmatter, body = split_frontmatter(text)
        kind = gate_kind(frontmatter, body)
        if kind == "frontmatter":
            fm_gate.add(rel)
        elif kind == "body_style":
            body_gate.add(rel)
        else:
            draw.add(rel)

    if (len(draw), len(fm_gate), len(body_gate)) != (3, 3, 0):
        raise ValueError(
            f"radar partition drift: draw={len(draw)} "
            f"frontmatter={len(fm_gate)} body_style={len(body_gate)} "
            f"(expected 3/3/0)")

    rows = []
    for rel in sorted(draw):
        observed_sha = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        if rel in _BODY_SOURCE:
            body_states = {"wide_240": "source", "narrow_40": "source",
                           "restored_240": "source"}
        else:
            body_states = {
                "wide_240": "drawn",
                "narrow_40": "drawn" if rel in _NARROW_DRAW_BODY else "source",
                "restored_240": "drawn",
            }
        rows.append({
            "path": rel,
            "family": "radar",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": "whole-and-body",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn" if rel in _NARROW_DRAW else "source",
                "restored_240": "drawn",
            },
            "expected_body_states": body_states,
            "ledger_note": (
                "radar renderer validates the axes, curves and scalar "
                "directives and transcribes them verbatim; no visual gate is "
                "present so it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "radar-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_radar_impl.cpp",
            "grammar": "radar-beta; axes, inline/braced curves, scalar directives",
            "theme_gate": (
                "a frontmatter theme/look/config key, an %%{init}%% directive "
                "or a body style/classDef/::: statement keeps the whole "
                "original on source fallback"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "radar fixtures with no frontmatter gate and no body style",
            "roots": ["radar"],
            "draw_count": len(draw),
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
