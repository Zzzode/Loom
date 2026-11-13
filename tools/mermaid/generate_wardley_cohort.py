#!/usr/bin/env python3
"""Generate the RFC 0031 wardley-family renderer cohort ledger.

The wardley renderer (mermaid_wardley_impl.cpp) projects the wardley-beta
grammar (a strict two-space indented map frame: anchors/components with
normalized coordinates and label offsets, value-chain and flow links,
evolve/note/annotation directives, evolution-axis renames and pipeline
blocks) to a bounded native outline that preserves the source indentation.
It has no map-canvas, coordinate-geometry or color contract, so fixtures
carrying a frontmatter theme/look/config key or a body style statement stay
on whole-source fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / frontmatter-gated / body-style-gated partition; manifest shas are
pinned; the review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_wardley_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/wardley")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/wardley-cohort.json")
SCHEMA = "loom_mermaid_wardley_cohort/v1"
_FENCE = re.compile(r"^---[ \t]*$")

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_wardley_cohort.cpp. Populated from live measurement;
# edited only after a live re-measure (engineering rule 18). At 40 columns
# only the two compact link/canvas frames (longest line 33 columns) keep a
# native surface on both tracks; the other four frames contain coordinate and
# note lines beyond 40 columns and stay source on the narrow track.
_NARROW_DRAW = {
    "wardley/pr-check/4-should-render-link-types-and-annotations.mmd",
    "wardley/pr-check/5-should-render-custom-canvas-size.mmd",
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
                           "(fresh general-purpose agent, wardley renderer "
                           "cohort, 2026-11-13, task aef27af8b85e12728)"),
        "date": "2026-11-13",
        "verdict": "approved",
        "note": ("APPROVE with zero MAJOR. The reviewer independently "
                 "reproduced the exact 6/4/0 partition with two separate "
                 "parsers, confirmed fixture 6's only non-ASCII byte is in "
                 "its frontmatter description (the body is pure ASCII) and "
                 "the narrow-40 set from line lengths. It ported every "
                 "validator to a simulator plus an independent inverse "
                 "checker and ran 167 deterministic probes, ~800,000 fuzzed "
                 "and mutation inputs and a 300,000+ visual-gate fuzz with "
                 "no fail-open, no hang and no verbatim/order invariant "
                 "break, and explicitly re-attacked the frontmatter "
                 "'config : 42' spacing seam (this renderer trims the key "
                 "from the first draft). One MINOR was applied: size and "
                 "the annotation grid index used the signed-integer pair "
                 "helper and so accepted a negative value, which is outside "
                 "the grammar (a leading minus is legal only for label "
                 "offsets); they now use an unsigned pair and two "
                 "regression pins lock it. The NITs (quoted YAML visual "
                 "keys, frontmatter-CRLF safe-direction fallback, the "
                 "duplicated fence predicate) are shared safe-direction "
                 "behavior with no corpus sample and are unchanged. The "
                 "untrimmed frontmatter-key pattern in twelve earlier "
                 "renderers remains a separate cross-family follow-up."),
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

    if (len(draw), len(fm_gate), len(body_gate)) != (6, 4, 0):
        raise ValueError(
            f"wardley partition drift: draw={len(draw)} "
            f"frontmatter={len(fm_gate)} body_style={len(body_gate)} "
            f"(expected 6/4/0)")

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
            "family": "wardley",
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
                "wardley renderer projects the strict two-space map frame to "
                "a bounded native outline; no visual gate is present so it "
                "draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "wardley-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_wardley_impl.cpp",
            "grammar": "wardley-beta; two-space anchored map frame, links, "
                       "evolve/notes/annotations, evolution, pipelines",
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
            "rule": "wardley fixtures with no frontmatter gate and no body style",
            "roots": ["wardley"],
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
