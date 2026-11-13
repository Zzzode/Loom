#!/usr/bin/env python3
"""Generate the RFC 0031 kanban-family renderer cohort ledger.

The kanban renderer (mermaid_kanban_impl.cpp) projects the kanban grammar
(section/card declarations `id[Label]` or a bare id, each with an optional
`@{ key: value }` annotation block) to a bounded native outline. It has no
color/layout contract, so fixtures carrying a frontmatter theme/look/config
key or a body style statement stay on whole-source fallback. The pinned
kanban corpus is eleven clean fixtures with description-only frontmatter.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / gated partition; manifest shas are pinned; the review block is
preserved across regeneration.

Usage: python3 tools/mermaid/generate_kanban_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/kanban")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/kanban-cohort.json")
SCHEMA = "loom_mermaid_kanban_cohort/v1"

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_kanban_cohort.cpp. Populated from live measurement;
# edited only after a live re-measure (engineering rule 18). The three
# compact boards also draw at 40 columns; the eight boards with long labels
# or many cards exceed the narrow frame and stay source there.
_NARROW_DRAW = {
    "kanban/nightly-check/7-should-handle-prioritization.mmd",
    "kanban/pr-check/1-should-render-a-kanban-with-a-single-section.mmd",
    "kanban/pr-check/2-should-render-a-kanban-with-multiple-sections.mmd",
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
                           "(fresh general-purpose agent, kanban renderer "
                           "cohort, 2026-11-13, task a112dbeca598240ec)"),
        "date": "2026-11-13",
        "verdict": "approved",
        "note": ("APPROVE with zero MAJOR. The reviewer independently "
                 "reproduced the exact 11/0/0 partition (50 cards, three "
                 "shapes: bare id, id+label, id+label+annotation), matched "
                 "all manifest shas and the live narrow set (the three "
                 "boards under 40 bytes), and probed every malformed card "
                 "shape with no fail-open. MINOR 1 (full visual-gate "
                 "branch tests), MINOR 2 (header-only frame and bare-CR "
                 "cases) and the NIT 5 empty-label/empty-annotation/"
                 "nested-brace boundaries were applied in the cohort "
                 "test. MINOR 3 disclosed a cross-suite effect: two "
                 "merman vendor kanban inputs (including a label-less "
                 "id@{...} card) flip from Charts/source to native draw "
                 "via the classifier; MermaidTerminal."
                 "CompleteImportedInputBaseline still passes and no test "
                 "pins their disposition, so the flip is safe and is "
                 "recorded here. The conservative classDef/::: prefix "
                 "gate is inherited unchanged from the approved "
                 "architecture/quadrant siblings and only forces a safe "
                 "source fallback; remaining NITs are safe-direction "
                 "whitespace/tolerance behavior with zero corpus impact."),
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

    if (len(draw), len(fm_gate), len(body_gate)) != (11, 0, 0):
        raise ValueError(
            f"kanban partition drift: draw={len(draw)} "
            f"frontmatter={len(fm_gate)} body_style={len(body_gate)} "
            f"(expected 11/0/0)")

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
            "family": "kanban",
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
                "kanban renderer projects cards and their annotations to a "
                "bounded native outline; no visual gate is present so it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "kanban-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_kanban_impl.cpp",
            "grammar": "kanban; id[Label] or bare id with optional @{ key: value }",
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
            "rule": "kanban fixtures with no frontmatter gate and no body style",
            "roots": ["kanban"],
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
