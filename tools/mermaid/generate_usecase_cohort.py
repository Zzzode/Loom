#!/usr/bin/env python3
"""Generate the RFC 0031 usecase-family renderer cohort ledger.

The usecase renderer (mermaid_usecase_impl.cpp) projects the usecase-beta
grammar (actors, use cases, system boundaries, notes and the UML edge set)
to a bounded native outline. It has no layout/color contract, so fixtures
carrying a frontmatter theme/look/config key or a body style statement
(classDef/:::/style/%%{init}%%) stay on whole-source fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / frontmatter-gated / body-style-gated partition; manifest shas are
pinned; the review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_usecase_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/usecase")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/usecase-cohort.json")
SCHEMA = "loom_mermaid_usecase_cohort/v1"

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_usecase_cohort.cpp. Populated from live measurement;
# edited only after a live re-measure (engineering rule 18). The eight compact
# diagrams (including the header-only empty frame) also draw at 40 columns;
# the larger/label-rich ones stay source there. D-458 removed
# `15-animation-variants` from this set: its `@{...}` annotations are now
# dropped from the outline and replaced by a 56-column body disclosure line,
# so the frame no longer fits 40 columns and falls back there (re-measured).
_NARROW_DRAW = {
    "usecase/nightly-check/01-basic-use-cases.mmd",
    "usecase/nightly-check/05-association-markers.mmd",
    "usecase/nightly-check/06-uml-relationships.mmd",
    "usecase/nightly-check/11-order-independent-refinement.mmd",
    "usecase/nightly-check/13-minimum-edge-lengths.mmd",
    "usecase/nightly-check/17-direction-bottom-up.mmd",
    "usecase/nightly-check/18-direction-right-to-left.mmd",
    "usecase/pr-check/empty-diagram.mmd",
}
_NARROW_DRAW_BODY: set[str] = set(_NARROW_DRAW)
_BODY_SOURCE: set[str] = set()


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
                           "(fresh general-purpose agent, usecase renderer "
                           "cohort, 2026-11-13, task a37954a81a81ed523)"),
        "date": "2026-11-13",
        "verdict": "approved",
        "note": ("APPROVE with zero MAJOR. The reviewer independently "
                 "reproduced the exact 14/12/6 partition with a from-scratch "
                 "frontmatter splitter (a ::: file also carrying frontmatter "
                 "counts as frontmatter), ported every validator and ran 60+ "
                 "adversarial probes with no fail-open across the 32 fixtures. "
                 "MINOR 1 (a quote inside a %% comment could swallow later "
                 "statements) was fixed by treating comment lines as inert "
                 "while outside an open quote, with a regression test. MINOR 2 "
                 "edge whitelist: the semicolon reject and the requirement "
                 "that ..> carry : include/extend were applied (both proven "
                 "corpus-safe); the proposed single-connector count was "
                 "rejected because a labeled edge A -- \"label\" --> B "
                 "legitimately has two dash runs, so a chained edge remains an "
                 "accepted verbatim outline with no incorrect transcription. "
                 "The unreachable quoted-first-token branch in "
                 "validate_declaration (NIT) was deleted; anonymous quoted use "
                 "cases are handled on the raw statement. The conservative "
                 "classDef/::: prefix gate is inherited byte-identically from "
                 "the approved architecture/kanban siblings and only forces a "
                 "safe source fallback. Remaining NITs are zero-corpus-impact "
                 "C++/Python whitespace tolerances guarded by the drift "
                 "guard."),
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

    D458 = {
        "usecase/nightly-check/07-edge-styles-and-animation.mmd",
        "usecase/nightly-check/08-json-table.mmd",
        "usecase/nightly-check/12-boundary-contained-actors.mmd",
        "usecase/nightly-check/14-element-styling-precedence.mmd",
        "usecase/nightly-check/16-json-order-and-values.mmd",
        "usecase/nightly-check/20-full-featured-order-system.mmd",
        "usecase/pr-check/complete-hand-drawn-rendering-contract.mmd",
        "usecase/pr-check/small-representative-visual-snapshot.mmd",
    }
    D456 = {
        "usecase/nightly-check/themed-elements-redux-color-theme.mmd",
        "usecase/nightly-check/themed-elements-redux-dark-color-theme.mmd",
        "usecase/pr-check/palette-rotation-redux-color-theme.mmd",
        "usecase/pr-check/palette-rotation-redux-dark-color-theme.mmd",
        "usecase/pr-check/themed-elements-base-theme.mmd",
        "usecase/pr-check/themed-elements-dark-theme.mmd",
        "usecase/pr-check/themed-elements-default-theme.mmd",
        "usecase/pr-check/themed-elements-forest-theme.mmd",
        "usecase/pr-check/themed-elements-neutral-theme.mmd",
    }
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

    moved = fm_gate & D456
    if len(moved) != 9:
        raise ValueError(f"D-456 rows drifted out of the frontmatter partition: {sorted(moved)}")
    fm_gate -= D456
    # D-458: the eight remaining rows moved into their own decision cohort
    # (six body-style rows plus the two complete-contract rows).
    moved458 = (fm_gate | body_gate) & D458
    if len(moved458) != 8:
        raise ValueError(f"D-458 rows drifted out of the gated partitions: {sorted(moved458)}")
    fm_gate -= D458
    body_gate -= D458
    if (len(draw), len(fm_gate), len(body_gate)) != (14, 1, 0):
        raise ValueError(
            f"usecase partition drift: draw={len(draw)} "
            f"frontmatter={len(fm_gate)} body_style={len(body_gate)} "
            f"(expected 14/1/0 after D-458)")

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
            "family": "usecase",
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
                "usecase renderer projects actors/use-cases/boundaries/edges "
                "to a bounded native outline; no visual gate is present so "
                "it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "usecase-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_usecase_impl.cpp",
            "grammar": ("usecase-beta; direction, actor, use cases, "
                        "systemBoundary/end, notes, UML edges and @{...} "
                        "metadata"),
            "theme_gate": (
                "a frontmatter theme/look/config key or an in-body "
                "%%{init}%% directive keeps the whole original on source "
                "fallback; the closed body styling surface is owned by the "
                "D-458 cohort"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "usecase fixtures with no frontmatter gate and no body style",
            "roots": ["usecase"],
            "draw_count": len(draw),
            "frontmatter_gated_count": len(fm_gate),
            "frontmatter_gated": sorted(fm_gate),
            "d456_admitted_count": len(moved),
            "d456_admitted": sorted(moved),
            "d458_admitted_count": len(moved458),
            "d458_admitted": sorted(moved458),
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
