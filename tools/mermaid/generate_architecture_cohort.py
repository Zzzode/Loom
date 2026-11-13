#!/usr/bin/env python3
"""Generate the RFC 0031 architecture-family renderer cohort ledger.

The architecture renderer (mermaid_architecture_impl.cpp) projects the
architecture-beta grammar (group/service/junction declarations with optional
icon and label and `in` containment, L/R/T/B port edges in directed,
undirected and labeled forms, group-boundary edges, and align directives) to
a bounded native outline. It has no layout/edge-routing/color contract, so
fixtures carrying a frontmatter theme/look/config key stay on whole-source
fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / frontmatter-gated partition; manifest shas are pinned; the review
block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_architecture_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/architecture")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/architecture-cohort.json")
SCHEMA = "loom_mermaid_architecture_cohort/v1"

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_architecture_cohort.cpp. Populated from live
# measurement; edited only after a live re-measure (engineering rule 18).
# The same ten compact fixtures draw at 40 columns on both tracks; the six
# larger/deeply-nested diagrams exceed the narrow native frame and stay
# source there.
_NARROW_DRAW = {
    "architecture/nightly-check/"
    "should-align-siblings-in-a-row-when-their-edges-feed-a-common-downstream-node.mmd",
    "architecture/nightly-check/"
    "should-render-a-grid-via-combined-row-column-alignments.mmd",
    "architecture/nightly-check/"
    "should-render-an-architecture-diagram-with-directional-arrows.mmd",
    "architecture/nightly-check/"
    "should-render-a-simple-architecture-diagram-with-titleandaccessibilities.mmd",
    "architecture/nightly-check/should-render-edges-at-correct-length.mmd",
    "architecture/nightly-check/"
    "should-stack-three-same-port-databases-in-a-column-without-overlap.mmd",
    "architecture/pr-check/"
    "should-render-an-architecture-diagram-with-edge-labels.mmd",
    "architecture/pr-check/"
    "should-render-an-architecture-diagram-with-split-directioning.mmd",
    "architecture/pr-check/"
    "should-render-a-simple-architecture-diagram-with-groups.mmd",
    "architecture/visual-check/"
    "should-render-a-linear-chain-with-default-fcose-knobs.mmd",
}
_NARROW_DRAW_WHOLE: set[str] = set(_NARROW_DRAW)
_NARROW_DRAW_BODY: set[str] = set(_NARROW_DRAW)
# Body tracks whose native frame is surface-identical to a plain text code
# block at every width (none in this family: every clean body has content).
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
    """Return 'frontmatter' or None. architecture-beta has no in-body visual
    statements in the pinned corpus: no %%{init}%%, classDef, ::: or style."""
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
                           "(fresh general-purpose agent, architecture "
                           "renderer cohort, 2026-11-13, task "
                           "a5602134897c4359d)"),
        "date": "2026-11-13",
        "verdict": "approved",
        "note": ("APPROVE with zero MAJOR. The reviewer independently "
                 "reproduced the exact 16 draw / 4 frontmatter-gated / 0 "
                 "body-gated partition, enumerated 18 body statement "
                 "shapes (187 statements, zero false rejects on clean "
                 "fixtures), ported the validators and ran 64 targeted "
                 "probes, 22 final probes and 60000 fuzz cases with no "
                 "fail-open or crash. MINOR 1 (source/target endpoint "
                 "order, missing target port, empty edge label "
                 "regression pins), MINOR 2 (icon-only declaration "
                 "coverage) and the NIT 6 arrow-in-label case were "
                 "applied in the cohort test. MINOR 3 (conservative "
                 "classDef/::: prefix gating inherited from the quadrant "
                 "renderer) only ever forces a safe source fallback, "
                 "matches the approved sibling behavior and is deferred "
                 "to a shared hardening batch; NIT 4 (whitespace "
                 "tolerance) and NIT 5 (frontmatter block-scalar "
                 "false-positive) are safe-direction, zero-corpus-impact "
                 "and the Python/C++ gates stay identical."),
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

    if (len(draw), len(fm_gate), len(body_gate)) != (16, 4, 0):
        raise ValueError(
            f"architecture partition drift: draw={len(draw)} "
            f"frontmatter={len(fm_gate)} body_style={len(body_gate)} "
            f"(expected 16/4/0)")

    rows = []
    for rel in sorted(draw):
        observed_sha = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        whole_narrow = "drawn" if rel in _NARROW_DRAW_WHOLE else "source"
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
            "family": "architecture",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": "whole-and-body",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": whole_narrow,
                "restored_240": "drawn",
            },
            "expected_body_states": body_states,
            "ledger_note": (
                "architecture renderer projects groups/services/junctions/"
                "edges/aligns to a bounded native outline; no visual gate is "
                "present so it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "architecture-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_architecture_impl.cpp",
            "grammar": ("architecture-beta; group/service/junction with "
                        "optional icon/label and in-containment, L/R/T/B "
                        "directed/undirected/labeled edges, align row/column"),
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
            "rule": ("architecture fixtures with no frontmatter gate and no "
                     "body style statement"),
            "roots": ["architecture"],
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
