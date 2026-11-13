#!/usr/bin/env python3
"""Generate the RFC 0031 cynefin-family renderer cohort ledger.

The cynefin renderer (mermaid_cynefin_impl.cpp) projects the cynefin-beta
grammar (a strict two-space indented frame: the five domain keywords, quoted
items, title/accessibility directives and labeled domain transitions) to a
bounded native outline that preserves the source indentation. It has no
boundary-geometry or color contract, so fixtures carrying a frontmatter
theme/look/config key or a body style statement stay on whole-source fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / frontmatter-gated / body-style-gated partition; manifest shas are
pinned; the review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_cynefin_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/cynefin")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/cynefin-cohort.json")
SCHEMA = "loom_mermaid_cynefin_cohort/v1"
_FENCE = re.compile(r"^---[ \t]*$")

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_cynefin_cohort.cpp. Populated from live measurement;
# edited only after a live re-measure (engineering rule 18). At 40 columns
# four compact frames keep a native surface; the transition/accessibility
# fixtures carry lines long enough that the wrapped native frame coincides
# with the text surface, so they stay source on the narrow track.
_NARROW_DRAW = {
    "cynefin/pr-check/should-render-a-simple-cynefin-diagram-with-all-five-domains.mmd",
    "cynefin/pr-check/should-render-an-empty-cynefin-framework.mmd",
    "cynefin/pr-check/should-render-cynefin-with-confusion-domain-items-without-overflow.mmd",
    "cynefin/pr-check/should-render-cynefin-with-many-items-per-domain.mmd",
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
                           "(fresh general-purpose agent, cynefin renderer "
                           "cohort, 2026-11-13, task ad43d9261d381626d)"),
        "date": "2026-11-13",
        "verdict": "approved",
        "note": ("Final verdict APPROVE after one re-review. The first pass "
                 "returned REQUEST CHANGES on a single MAJOR: a frontmatter "
                 "key with whitespace before the mapping colon "
                 "('config : 42', legal YAML) bypassed the key-based visual "
                 "gate because the key tail was not trimmed, so an "
                 "unrenderable visual contract could draw. The fix trims the "
                 "key after the colon cut in mermaid_cynefin_impl.cpp; three "
                 "RED-then-GREEN regression pins cover config/look/theme "
                 "spacing variants, and the re-review confirmed 21 spacing "
                 "and near-lookalike variants all fail closed with zero "
                 "generator/C++ semantic divergence. The same untrimmed-key "
                 "pattern exists in the already-approved ishikawa/treeview "
                 "siblings but has no live trigger; per the blast-radius "
                 "rule it is left for a separate cross-family follow-up, not "
                 "this change. The reviewer independently reproduced the "
                 "exact 7/5/0 partition with two separate parsers, ported "
                 "the validator to a simulator, ran 160 deterministic probes "
                 "and 300,000 fuzzed inputs with no hang, no fail-open and "
                 "no verbatim/order invariant break, matched all 30 gtest "
                 "literals 30/30, and confirmed the single guarded push, the "
                 "prefix-scoped 1:1 hook and the frozen contracts. The "
                 "three NITs (indented header accepted, frontmatter-CRLF "
                 "safe-direction fallback, space-only text transcribed) are "
                 "fail-closed or faithful, have no corpus sample and match "
                 "the approved siblings, so they are unchanged."),
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

    if (len(draw), len(fm_gate), len(body_gate)) != (7, 5, 0):
        raise ValueError(
            f"cynefin partition drift: draw={len(draw)} "
            f"frontmatter={len(fm_gate)} body_style={len(body_gate)} "
            f"(expected 7/5/0)")

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
            "family": "cynefin",
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
                "cynefin renderer projects the strict two-space domain/item "
                "frame to a bounded native outline; no visual gate is present "
                "so it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "cynefin-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_cynefin_impl.cpp",
            "grammar": "cynefin-beta; two-space domains, quoted items, "
                       "directives and labeled transitions",
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
            "rule": "cynefin fixtures with no frontmatter gate and no body style",
            "roots": ["cynefin"],
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
