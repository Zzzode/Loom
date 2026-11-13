#!/usr/bin/env python3
"""Generate the RFC 0031 eventmodeling-family renderer cohort ledger.

The eventmodeling renderer (mermaid_eventmodeling_impl.cpp) projects the
eventmodeling grammar (ordered `tf` frames with inline data, data-block
references and source relations, plus named data blocks) to a bounded native
outline that preserves the source column layout. It has no event-stream
geometry or color contract, so fixtures carrying a frontmatter
theme/look/config key or a body style statement stay on whole-source
fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / frontmatter-gated / body-style-gated partition; manifest shas are
pinned; the review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_eventmodeling_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/eventmodeling")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/eventmodeling-cohort.json")
SCHEMA = "loom_mermaid_eventmodeling_cohort/v1"
_FENCE = re.compile(r"^---[ \t]*$")

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_eventmodeling_cohort.cpp. Populated from live
# measurement; edited only after a live re-measure (engineering rule 18).
# Every eventmodeling frame has short lines (max 37 columns), so all six
# fixtures keep a native surface at 40 columns on both tracks.
_NARROW_DRAW = {
    "eventmodeling/pr-check/state-change-pattern.mmd",
    "eventmodeling/pr-check/state-view-pattern.mmd",
    "eventmodeling/pr-check/translation-pattern.mmd",
    "eventmodeling/pr-check/with-data-block-reference.mmd",
    "eventmodeling/pr-check/with-multiple-source-relations.mmd",
    "eventmodeling/pr-check/with-qualified-names.mmd",
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
                           "(fresh general-purpose agent, eventmodeling "
                           "renderer cohort, 2026-11-14, task "
                           "a1616614c326e9b96)"),
        "date": "2026-11-14",
        "verdict": "approved",
        "note": ("Final verdict APPROVE after one re-review. The first pass "
                 "returned REQUEST CHANGES on two MAJORs, both fixed in this "
                 "family's validator. M1: is_dotted_name did not constrain "
                 "the first character of each segment, so digit-led names "
                 "('1', '1A', 'A.1B', '[[B.2]]', 'data 1', a digit-led data "
                 "key) drew; it is now a per-segment state machine requiring "
                 "an identifier-start character. M2: an empty data block "
                 "('data D { }') drew; a block now requires at least one "
                 "indent-2 entry before its closing brace (block_has_entry). "
                 "Three MINORs were tightened in the same pass: an inline "
                 "data block and a [[reference]] are each allowed at most "
                 "once while ->> relations may still chain; a data entry "
                 "allows only one optional trailing comma (the radar-style "
                 "position-aware rule, so 'k: v,,' fails); and a data header "
                 "must be followed by its own brace rather than be "
                 "overwritten by another header or frame. The re-review ran "
                 "680,000+ fuzz/mutation cases against a strict grammar "
                 "oracle: the v1-to-v2 differential is tightening-only with "
                 "zero new accepts, all six corpus fixtures still draw with "
                 "verbatim output and pinned shas, and no invariant is "
                 "violated. The retained NITs (no required space between a "
                 "name and its suffix, trim-based whitespace tolerance, and "
                 "the over-broad ':::' substring gate) are fail-safe "
                 "inherited shapes shared with the approved siblings and are "
                 "unchanged per the blast-radius rule. The trim-before-colon "
                 "frontmatter gate and printable-ASCII literal hardening are "
                 "confirmed."),
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

    if (len(draw), len(fm_gate), len(body_gate)) != (6, 0, 0):
        raise ValueError(
            f"eventmodeling partition drift: draw={len(draw)} "
            f"frontmatter={len(fm_gate)} body_style={len(body_gate)} "
            f"(expected 6/0/0)")

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
            "family": "eventmodeling",
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
                "eventmodeling renderer validates the frames and data blocks "
                "and transcribes them verbatim; no visual gate is present so "
                "it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "eventmodeling-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_eventmodeling_impl.cpp",
            "grammar": "eventmodeling; tf frames, relations, inline/ref data, data blocks",
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
            "rule": "eventmodeling fixtures with no frontmatter gate and no body style",
            "roots": ["eventmodeling"],
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
