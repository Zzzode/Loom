#!/usr/bin/env python3
"""Generate the RFC 0031 quadrant-family renderer cohort ledger.

The quadrant renderer (mermaid_quadrant_impl.cpp) projects the quadrantChart
grammar (title, x/y-axis labels, quadrant-1..4 labels, named [x, y] data
points) to a bounded native outline. It has no geometry/color contract, so
fixtures carrying a frontmatter theme/look/config key or a non-admitted
%%{init}%% visual directive stay on whole-source fallback. The body
classDef / `:::class` / inline point-style surface is admitted by the D-457
cohort (validated, dropped and disclosed), so those rows are outside this
partition.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / init-gated / body-style-gated partition; manifest shas are pinned;
the review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_quadrant_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/quadrant-chart")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/quadrant-cohort.json")
SCHEMA = "loom_mermaid_quadrant_cohort/v1"

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_quadrant_cohort.cpp. Populated from live measurement.
# The bare-header fixture draws narrow only on the whole track: its frontmatter
# makes the mermaid/text surfaces differ at 40 cols, while its one-line body
# renders identically to plain text there.
_NARROW_DRAW_WHOLE = {
    "quadrant-chart/nightly-check/"
    "should-render-both-axes-labels-on-the-left-and-bottom-if-both-axes-have-only-one-label.mmd",
    "quadrant-chart/nightly-check/should-render-if-only-chart-type-is-provided.mmd",
    "quadrant-chart/pr-check/"
    "should-render-x-axis-labels-in-the-center-if-x-axis-has-two-labels.mmd",
}
_NARROW_DRAW_BODY = {
    "quadrant-chart/nightly-check/"
    "should-render-both-axes-labels-on-the-left-and-bottom-if-both-axes-have-only-one-label.mmd",
    "quadrant-chart/pr-check/"
    "should-render-x-axis-labels-in-the-center-if-x-axis-has-two-labels.mmd",
}
# The header-only fixture's body is a single "quadrantChart" line: the native
# frame is surface-identical to a plain text code block at every width, so its
# body track stays source even though the whole source (with frontmatter) is
# distinguishable and draws.
_BODY_SOURCE = {
    "quadrant-chart/nightly-check/should-render-if-only-chart-type-is-provided.mmd",
}

# D-457: the two body-style rows moved into their own decision cohort (the
# closed classDef / `:::class` / inline point-style surface is validated,
# dropped and disclosed by that cohort's gate).
D457 = {
    "quadrant-chart/pr-check/it-should-render-data-points-with-styles-classes.mmd",
    "quadrant-chart/pr-check/it-should-render-data-points-with-styles.mmd",
}

# Inline point style keys. A key must start on a non-identifier boundary and
# be followed by optional spaces and a colon. This detector only classifies
# the historical body-style partition; D-457 owns admission.
_STYLE_RE = re.compile(
    r"(?<![A-Za-z0-9_-])(?:stroke-color|stroke-width|radius|color)\s*:")


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


def has_point_style(suffix):
    return _STYLE_RE.search(suffix) is not None


# D-450 admits the leading closed %%{init}%% tuning/theme directive for the
# five fixtures below; they leave this ledger for
# tools/mermaid/quadrant-init-d450-cohort.json and draw natively.
D450_COHORT = {
    "quadrant-chart/nightly-check/should-able-to-render-x-axis-on-bottom.mmd",
    "quadrant-chart/pr-check/"
    "should-able-to-render-x-axis-on-bottom-and-y-axis-on-right.mmd",
    "quadrant-chart/pr-check/should-able-to-render-y-axis-on-right-side.mmd",
    "quadrant-chart/pr-check/should-use-all-the-config.mmd",
    "quadrant-chart/pr-check/should-use-all-the-theme-variable.mmd",
}


def gate_kind(frontmatter, body):
    """Return 'frontmatter', 'init', 'body_style' or None."""
    if frontmatter is not None and re.search(r"^\s*(theme|look|config)\s*:",
                                             frontmatter, re.M):
        return "frontmatter"
    for raw in body.split("\n"):
        line = raw.lstrip(" ")
        if line.startswith("%%{init") or line.startswith("classDef") or \
                ":::" in line:
            if line.startswith("%%{init"):
                return "init"
            return "body_style"
        bracket = line.find("]")
        if bracket != -1 and has_point_style(line[bracket + 1:]):
            return "body_style"
    return None


def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, quadrant renderer "
                           "cohort, 2026-11-13, task ac14877d1a0f3b761)"),
        "date": "2026-11-13",
        "verdict": "approved",
        "note": ("APPROVE with zero MAJOR. The reviewer independently "
                 "reproduced the exact 7 draw / 5 init-gated / 2 "
                 "body-style-gated partition over all 14 fixtures with two "
                 "from-scratch analyses in /tmp, ported the parser line by "
                 "line and ran 50+ adversarial probes (22 gate-boundary "
                 "cases included) with all results matching. M1 was the "
                 "pending-to-approved process flip of this block. N1 (the "
                 "C++ gate skips literal spaces while the Python regex uses "
                 "\\s*) and N2 (frontmatter closing-fence edge shape) are "
                 "hypothetical-input-only divergences that both fail closed "
                 "with zero corpus impact, deferred to a later hardening "
                 "batch alongside the venn M1/M3 items; N3 coordinate/CRLF "
                 "regression cases and the N4 trim_view lifetime comment "
                 "were applied in the cohort test and renderer."),
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

    draw, init_gate, body_gate, fm_gate = set(), set(), set(), set()
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        text = path.read_text()
        rel = path.relative_to(
            repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        if rel in D450_COHORT:
            continue  # owned by the D-450 leading-init cohort
        frontmatter, body = split_frontmatter(text)
        kind = gate_kind(frontmatter, body)
        if kind == "frontmatter":
            fm_gate.add(rel)
        elif kind == "init":
            init_gate.add(rel)
        elif kind == "body_style":
            body_gate.add(rel)
        else:
            draw.add(rel)

    moved = body_gate & D457
    if len(moved) != 2:
        raise ValueError(
            f"D-457 rows drifted out of the body-style partition: {sorted(moved)}")
    body_gate -= D457
    if (len(draw), len(init_gate), len(body_gate), len(fm_gate)) != (7, 0, 0, 0):
        raise ValueError(
            f"quadrant partition drift: draw={len(draw)} init={len(init_gate)} "
            f"body_style={len(body_gate)} frontmatter={len(fm_gate)} "
            f"(expected 7/0/0/0 after D-457)")

    rows = []
    for rel in sorted(draw):
        observed_sha = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        whole_narrow = "drawn" if rel in _NARROW_DRAW_WHOLE else "source"
        body_narrow = "drawn" if rel in _NARROW_DRAW_BODY else "source"
        if rel in _BODY_SOURCE:
            body_states = {"wide_240": "source", "narrow_40": "source",
                           "restored_240": "source"}
        else:
            body_states = {"wide_240": "drawn", "narrow_40": body_narrow,
                           "restored_240": "drawn"}
        rows.append({
            "path": rel,
            "family": "quadrant-chart",
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
                "quadrant renderer projects axes/quadrants/points to a bounded "
                "native outline; no visual gate is present so it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "quadrant-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_quadrant_impl.cpp",
            "grammar": ("quadrantChart; title, x-axis/y-axis labels, "
                        "quadrant-1..4 labels, Name: [x, y] points"),
            "theme_gate": (
                "frontmatter theme/look/config or a non-admitted %%{init}%% "
                "directive keeps the whole original on source fallback; the "
                "body classDef / ::: / inline point-style surface is owned by "
                "the D-457 cohort"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": ("quadrant-chart fixtures with no frontmatter/init gate and "
                     "no body style statement"),
            "roots": ["quadrant-chart"],
            "draw_count": len(draw),
            "init_gated_count": len(init_gate),
            "init_gated": sorted(init_gate),
            "body_style_gated_count": len(body_gate),
            "body_style_gated": sorted(body_gate),
            "d457_admitted_count": len(moved),
            "d457_admitted": sorted(moved),
            "frontmatter_gated_count": len(fm_gate),
            "frontmatter_gated": sorted(fm_gate),
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
