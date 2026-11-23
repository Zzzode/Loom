#!/usr/bin/env python3
"""Generate the RFC 0031 tree-view family renderer cohort ledger.

The tree-view renderer (mermaid_treeview_impl.cpp) projects the treeView-beta
grammar (indentation-based nodes with quoted or bare labels, optional icon(...)
and ``## description`` suffixes) to a bounded native outline that preserves the
source indentation. It has no icon/color/layout contract, so fixtures carrying
a frontmatter theme/look/config key or a body :::/classDef/style/%%{init}%%
statement stay on whole-source fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / frontmatter-gated / body-style-gated partition; manifest shas are
pinned; the review block is preserved across regeneration.

Note: one fixture closes its frontmatter with a trailing-whitespace fence
(``---      ``); the splitter tolerates that. D-455 later admits that row and
the empty-``icon()`` row into their own decision cohort, so both are excluded
from the frontmatter partition here and pinned in ``d455_admitted``.

Usage: python3 tools/mermaid/generate_treeview_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/tree-view")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/treeview-cohort.json")
SCHEMA = "loom_mermaid_treeview_cohort/v1"

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_treeview_cohort.cpp. Populated from live measurement;
# edited only after a live re-measure (engineering rule 18). All eight clean
# trees are compact enough to also draw at 40 columns on both tracks.
_NARROW_DRAW = {
    "tree-view/nightly-check/should-render-a-simple-treeview-diagram-with-quoted-labels.mmd",
    "tree-view/nightly-check/should-render-icon-overrides.mmd",
    "tree-view/pr-check/should-preserve-consecutive-spaces-and-unicode-in-labels.mmd",
    "tree-view/pr-check/should-render-a-complex-treeview-diagram-with-quoted-labels.mmd",
    "tree-view/pr-check/should-render-descriptions.mmd",
    "tree-view/pr-check/should-render-emoji-as-icons-with-the-default-icons-hidden.mmd",
    "tree-view/pr-check/should-render-the-unknown-icon-fallback-for-unregistered-icons.mmd",
    "tree-view/pr-check/should-render-with-multiple-roots-and-quoted-labels.mmd",
}
_NARROW_DRAW_BODY: set[str] = set(_NARROW_DRAW)
_BODY_SOURCE: set[str] = set()

_FENCE = re.compile(r"^---[ \t]*$")


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
                           "(fresh general-purpose agent, tree-view renderer "
                           "cohort, 2026-11-13, task ac919e19482fdb6ff)"),
        "date": "2026-11-13",
        "verdict": "approved",
        "note": ("APPROVE with zero MAJOR. The reviewer independently "
                 "confirmed the source-driven 8/5/2 partition (refuting the "
                 "brief's older 9/4/2 census): the custom-config fixture "
                 "closes its frontmatter with a trailing-whitespace fence "
                 "and is correctly frontmatter-gated. It ported tree_body, "
                 "the indent grid and validate_node and passed 22 boundary "
                 "probes with no fail-open, and verified every output push "
                 "uses the single guarded path. MINOR 1 (free text after a "
                 "closing quoted label was over-accepted) was tightened so a "
                 "quoted label may only be followed by icon(...)/## suffixes, "
                 "with regression cases; MINOR 2 added the missing bare-CR "
                 "test plus empty-label, double-icon, icon-after-description, "
                 "empty-icon, no-space-## and indent 0/3/5 coverage, and "
                 "direct look/style/init gate tests. NIT 1 (the shared "
                 "classifier peek requires an exact fence) and NIT 2 (the "
                 "combined header push, identical to the approved usecase "
                 "pattern and bounded by the node cap plus the downstream "
                 "1024-row guard) are safe-direction inherited behavior and "
                 "left unchanged to avoid touching a protected shared area; "
                 "NIT 3 is harmless defense-in-depth."),
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
    # D-455: the two residual rows moved into their own decision cohort.
    d455 = {
        "tree-view/pr-check/should-render-with-custom-config-and-quoted-labels.mmd",
        "tree-view/nightly-check/should-hide-default-icons-with-icon-none-and-icon.mmd",
    }
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

    moved = fm_gate & d455
    if len(moved) != 2:
        raise ValueError(f"D-455 rows drifted out of the frontmatter partition: {sorted(moved)}")
    fm_gate -= d455
    if (len(draw), len(fm_gate), len(body_gate)) != (8, 3, 2):
        raise ValueError(
            f"tree-view partition drift: draw={len(draw)} "
            f"frontmatter={len(fm_gate)} body_style={len(body_gate)} "
            f"(expected 8/3/2; the two D-455 rows moved out)")

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
            "family": "tree-view",
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
                "tree-view renderer projects the indented node tree to a "
                "bounded native outline; no visual gate is present so it "
                "draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "tree-view-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_treeview_impl.cpp",
            "grammar": ("treeView-beta; indented quoted/bare node labels with "
                        "optional icon(...) and ## description suffixes"),
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
            "rule": "tree-view fixtures with no frontmatter gate and no body style",
            "roots": ["tree-view"],
            "draw_count": len(draw),
            "frontmatter_gated_count": len(fm_gate),
            "frontmatter_gated": sorted(fm_gate),
            "d455_admitted_count": len(moved),
            "d455_admitted": sorted(moved),
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
