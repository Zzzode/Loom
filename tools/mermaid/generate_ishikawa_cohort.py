#!/usr/bin/env python3
"""Generate the RFC 0031 ishikawa-family renderer cohort ledger.

The ishikawa renderer (mermaid_ishikawa_impl.cpp) projects the ishikawa-beta
grammar (a relative-indentation tree: an effect root followed by cause
categories and nested sub-causes, each a bare label) to a bounded native
outline that preserves the source indentation. It has no fishbone geometry or
color contract, so fixtures carrying a frontmatter theme/look/config key or a
body style statement stay on whole-source fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / frontmatter-gated / body-style-gated partition; manifest shas are
pinned; the review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_ishikawa_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/ishikawa")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/ishikawa-cohort.json")
SCHEMA = "loom_mermaid_ishikawa_cohort/v1"
_FENCE = re.compile(r"^---[ \t]*$")

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_ishikawa_cohort.cpp. Populated from live measurement;
# edited only after a live re-measure (engineering rule 18). All seven clean
# fishbone trees are compact enough to also draw at 40 columns on both tracks.
_NARROW_DRAW = {
    "ishikawa/pr-check/1-should-render-a-simple-ishikawa-diagram.mmd",
    "ishikawa/pr-check/2-should-render-with-many-causes-on-both-sides.mmd",
    "ishikawa/pr-check/3-should-render-with-deeply-nested-causes.mmd",
    "ishikawa/pr-check/4-should-render-with-a-single-cause.mmd",
    "ishikawa/pr-check/5-should-render-with-no-children-root-only.mmd",
    "ishikawa/pr-check/12-should-render-correctly-when-effect-is-indented-more-than-causes.mmd",
    "ishikawa/pr-check/13-should-render-a-very-deep-nested-diagram.mmd",
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



# RFC 0043: the admitted frontmatter shape is exactly one column-0 `config:`
# block whose sole leaf is an indent-2 `theme` in this family's closed table.
THEME_TABLE = {"dark", "forest"}
# RFC 0046: the same block may instead carry exactly one `ishikawa` namespace
# with these closed tuning leaves.
LAYOUT_TABLE = {"useMaxWidth": {"true", "false"}, "diagramPadding": {"50"}}
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


def frontmatter_shape(frontmatter):
    """Return (verdict, theme, layout_leaves).

    verdict is 'none' | 'admitted' (RFC 0043 theme leaf) | 'layout' (RFC 0046
    namespace leaves) | 'gated'. Mirrors the C++ gate: a column-0 config block
    admits exactly one indent-2 theme leaf from the closed table and/or exactly
    one indent-2 `ishikawa` namespace with closed indent-4 tuning leaves; every
    other visual key or shape gates.
    """
    if frontmatter is None:
        return "none", None, {}
    in_config = False
    saw_config = False
    saw_namespace = False
    theme = None
    leaves = {}
    for line in frontmatter.split("\n"):
        # Blank means spaces/tabs only. The renderers pre-reject any tab in the
        # source, so the C++ space-only key scan and this predicate agree on
        # every reachable input; no fixture in the family has a tab.
        if line.strip(" \t") == "":
            continue
        key_start = len(line) - len(line.lstrip(" "))
        if key_start != 0:
            leaf = line[key_start:]
            if in_config:
                if leaf.startswith("#"):
                    continue
                colon = leaf.find(":")
                if colon < 0:
                    return "gated", None, {}
                key = leaf[:colon]
                value = _theme_scalar(leaf[colon + 1:])
                if key_start == 2 and key == "theme":
                    if theme is not None or value not in THEME_TABLE:
                        return "gated", None, {}
                    theme = value
                elif key_start == 2 and key == "ishikawa" and value == "":
                    if saw_namespace:
                        return "gated", None, {}
                    saw_namespace = True
                elif key_start == 4 and saw_namespace and key in LAYOUT_TABLE:
                    if key in leaves or value not in LAYOUT_TABLE[key]:
                        return "gated", None, {}
                    leaves[key] = value
                else:
                    return "gated", None, {}
            else:
                colon = leaf.find(":")
                key = leaf[:colon] if colon >= 0 else leaf
                if TRIM_COLUMN0_KEY:
                    key = key.strip(" ")
                if key in ("theme", "look", "config"):
                    return "gated", None, {}
            continue
        if line.startswith("#"):
            continue
        colon = line.find(":")
        key = line[:colon] if colon >= 0 else line
        if TRIM_COLUMN0_KEY:
            key = key.strip(" ")
        if key == "config":
            if saw_config:
                return "gated", None, {}
            saw_config = True
            in_config = True
        else:
            if key in ("theme", "look"):
                return "gated", None, {}
            in_config = False
    if saw_config and theme is None and not leaves:
        return "gated", None, {}
    if saw_namespace and not leaves:
        return "gated", None, {}
    if theme is not None and leaves:
        return "gated", None, {}
    if theme is not None:
        return "admitted", theme, {}
    if leaves:
        return "layout", None, leaves
    return "none", None, {}


def gate_kind(frontmatter, body):
    """Return 'frontmatter', 'admitted', 'layout', 'body_style' or None."""
    verdict, _theme, _leaves = frontmatter_shape(frontmatter)
    if verdict == "gated":
        return "frontmatter"
    if re.search(r"^\s*%%\{init", body, re.M) or \
            re.search(r"^\s*classDef", body, re.M) or ":::" in body or \
            re.search(r"^\s*style\s", body, re.M):
        return "body_style"
    if verdict in ("admitted", "layout"):
        return verdict
    return None


REVIEW_NOTE = (
    "APPROVE with zero MAJOR at the 2026-11-13 census (7 clean / 2 "
    "theme-admitted / 4 frontmatter-gated / 0 body-gated; the reviewer "
    "reproduced the partition, ported the renderer to a Python simulator, ran "
    "32 adversarial probes with no fail-open and applied every MINOR). RFC "
    "0043 admitted the 2 theme rows and RFC 0046 (2026-11-22) the 3 closed "
    "`config.ishikawa.*` tuning rows, so the live partition is "
    "13 = 7 + 2 + 3 + 1 + 0.")


def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, ishikawa renderer "
                           "cohort, 2026-11-13, task aa67aef2934adf864)"),
        "date": "2026-11-13",
        "verdict": "approved",
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
    # The reviewer/date/verdict are preserved; the note always states the
    # current live partition so it cannot go stale as cohorts are admitted.
    review = dict(review)
    review["note"] = REVIEW_NOTE
    return review


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    draw, admitted, layout, fm_gate, body_gate = set(), set(), set(), set(), set()
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
        elif kind == "admitted":
            admitted.add(rel)
        elif kind == "layout":
            layout.add(rel)
        else:
            draw.add(rel)

    # RFC 0046 admitted the 3 tuning rows: 13 = 7 + 2 + 3 + 1 + 0.
    if (len(draw), len(admitted), len(layout), len(fm_gate), len(body_gate)) != \
            (7, 2, 3, 1, 0):
        raise ValueError(
            f"ishikawa partition drift: draw={len(draw)} "
            f"admitted={len(admitted)} layout={len(layout)} "
            f"frontmatter={len(fm_gate)} body_style={len(body_gate)} "
            f"(expected 7/2/3/1/0)")

    layout_digest = hashlib.sha256("".join(
        f"{rel}\t{manifest_rows[rel]['sha256']}\n" for rel in sorted(layout)
    ).encode()).hexdigest()

    admitted_digest = hashlib.sha256("".join(
        f"{rel}\t{manifest_rows[rel]['sha256']}\n" for rel in sorted(admitted)
    ).encode()).hexdigest()

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
            "family": "ishikawa",
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
                "ishikawa renderer projects the relative-indentation cause "
                "tree to a bounded native outline; no visual gate is present "
                "so it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "ishikawa-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_ishikawa_impl.cpp",
            "grammar": "ishikawa-beta; relative-indented bare cause labels",
            "theme_gate": (
                "an unrecognized frontmatter theme/look/config shape, an "
                "%%{init}%% directive or a body style/classDef/::: statement "
                "keeps the whole original on source fallback; RFC 0043 admits "
                "the closed theme table and RFC 0046 the closed "
                "`ishikawa` tuning namespace, both footer-disclosed"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": ("ishikawa fixtures with no frontmatter gate (the RFC 0043 "
                     "theme and RFC 0046 tuning cohorts are admitted) and no "
                     "body style"),
            "roots": ["ishikawa"],
            "admission_rfc": 43,
            "draw_count": len(draw),
            "admitted_count": len(admitted),
            "admitted": sorted(admitted),
            "admitted_digest": admitted_digest,
            "layout_admitted_count": len(layout),
            "layout_admitted": sorted(layout),
            "layout_admitted_digest": layout_digest,
            "family_total": (len(draw) + len(admitted) + len(layout) +
                             len(fm_gate) + len(body_gate)),
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
