#!/usr/bin/env python3
"""Generate the RFC 0031 D204 requirement-family style cohort ledger.

The requirement renderer (mermaid_requirement_impl.cpp) accepts the common
Mermaid style vocabulary inside requirement bodies -- classDef profiles,
`class` assignments, direct `style` statements and the `name:::class`
shorthand -- validates every property/color/class/target strictly and
projects the result onto the shared ClassDirectStyles seam (palette hue
roles plus a disclosure legend). Body style statements therefore no longer
block native rendering.

The remaining gate is frontmatter-only, mirroring the pre-D-395
requirement_envelope:
  * a strict column-0 `---` fence whose YAML block carries any top-level key
    besides `description` (config/theme/title/...) stays on whole-source
    fallback;
  * an indented or unclosed fence is malformed and fails closed.

D-066 F0 (D-395) supersedes this classifier for eight inert-config rows: the
live envelope now also accepts `config:` carrying only htmlLabels (bool),
logLevel 0-2 or fontFamily courier, and those eight rows are governed by
tools/mermaid/requirement-f0-cohort.json. The 172/16/156 partition and the
`gated` list emitted here are therefore the frozen D-204 snapshot, not the
live partition (172/24/148); regenerating without an F0-aware classifier would
re-gate the eight rows.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the family partition (172 total / 16 native / 156 gated) and the four-row
style cohort; manifest shas are pinned; the review block is preserved across
regeneration.

Usage: python3 tools/mermaid/generate_requirement_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/requirement")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/requirement-cohort.json")
SCHEMA = "loom_mermaid_requirement_cohort/v1"
_FENCE = re.compile(r"^---$")
_TOP_KEY = re.compile(r"^([A-Za-z_][\w -]*?)\s*:")
_BODY_STYLE = re.compile(r"^\s*(classDef|class|style)(?:\s|$)")

# Live-measured per-width geometry (engineering rule 18): none of the four
# styled requirement originals keeps a native surface at 40 columns, so the
# narrow state is source_fallback on both tracks. Edited only after a live
# re-measure.
_NARROW_DRAW: set[str] = set()

# Hard drift-guard tuples. The partition is measured through the production
# renderer; these tuples turn any silent accept/reject regression into a
# generator failure.
_FAMILY_TOTAL = 172
_NATIVE_DRAW = 16
_GATED = 156
_COHORT_ROWS = 4


def split_frontmatter(text):
    """Return (frontmatter_lines_or_None, gated_malformed_bool)."""
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, False
    for close in range(1, len(lines)):
        if _FENCE.fullmatch(lines[close]):
            return lines[1:close], False
        # A non-column-0 fence candidate (e.g. "  ---") can never close the
        # envelope; parse_envelope rejects it, so the source stays gated.
        if lines[close].strip() == "---":
            return None, True
    return None, True  # unclosed fence


def bare_header_rejected(text):
    # Mirror requirement_bare_header: without a leading envelope the first
    # physical line must be the folded requirementDiagram keyword. A leading
    # blank line (the corpus theme fixture) rejects the whole source.
    first = text.split("\n", 1)[0].strip()
    return first.lower() != "requirementdiagram"


def frontmatter_gated(frontmatter, malformed):
    if malformed:
        return True
    if frontmatter is None:
        return False
    for line in frontmatter:
        # Mirror parse_envelope: a tab in the leading indentation anywhere in
        # the block and a column-0 non-comment line without a colon reject it.
        leading = line[:len(line) - len(line.lstrip(" 	"))]
        if "	" in leading:
            return True
        if line and not line[0].isspace() and not line.lstrip().startswith("#"):
            if ":" not in line:
                return True
        match = _TOP_KEY.match(line)
        if match and match.group(1) != "description":
            return True
    return False


def has_body_style(body):
    for line in body.split("\n"):
        if _BODY_STYLE.match(line) or ":::" in line:
            return True
    return False


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_review(repo_root):
    review = {
        "reviewer_agent": "pending independent in-session adversarial review",
        "date": "2026-11-14",
        "verdict": "pending",
        "note": ("Filled by the independent review agent; preserved across "
                 "regeneration once approved."),
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

    draw, gated = set(), set()
    styled_draws = set()
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        text = path.read_text()
        rel = path.relative_to(
            repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        frontmatter, malformed = split_frontmatter(text)
        body = text if frontmatter is None else text.split("\n---\n", 1)[-1]
        if frontmatter is None and bare_header_rejected(text):
            gated.add(rel)
            continue
        if frontmatter_gated(frontmatter, malformed):
            gated.add(rel)
            continue
        draw.add(rel)
        if has_body_style(body):
            styled_draws.add(rel)

    if (len(draw), len(gated)) != (_NATIVE_DRAW, _GATED) or \
            len(draw) + len(gated) != _FAMILY_TOTAL:
        raise ValueError(
            f"requirement partition drift: draw={len(draw)} gated={len(gated)} "
            f"total={len(draw) + len(gated)} "
            f"(expected {_NATIVE_DRAW}/{_GATED}/{_FAMILY_TOTAL})")
    if len(styled_draws) != _COHORT_ROWS:
        raise ValueError(
            f"requirement style cohort drift: styled_draws={len(styled_draws)} "
            f"(expected {_COHORT_ROWS})")

    rows = []
    for rel in sorted(styled_draws):
        observed_sha = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        narrow = "drawn" if rel in _NARROW_DRAW else "source"
        rows.append({
            "path": rel,
            "family": "requirement",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": "whole-and-body",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": narrow,
                "restored_240": "drawn",
            },
            # Description-only frontmatter strips to an identical body that
            # renders through the same path.
            "expected_body_states": {
                "wide_240": "drawn",
                "narrow_40": narrow,
                "restored_240": "drawn",
            },
            "ledger_note": (
                "requirement renderer validates the classDef/class/style/::: "
                "statements and projects them onto the shared palette-role "
                "style seam with a disclosure legend; no config/theme "
                "frontmatter gate is present so it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "requirement-family-style-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_requirement_impl.cpp",
            "grammar": (
                "requirementDiagram declarations, fields, seven relation "
                "keywords, classDef/class/style statements and ::: shorthand"),
            "theme_gate": (
                "a column-0 frontmatter fence carrying any top-level key "
                "besides description, or an indented/unclosed fence, keeps "
                "the whole original on source fallback; body style "
                "statements are validated and rendered, not gated"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": (
                "requirement fixtures without a config/theme/title "
                "frontmatter gate; the cohort rows are the four whose bodies "
                "carry style statements"),
            "roots": ["requirement"],
            "family_total": _FAMILY_TOTAL,
            "native_draw_count": _NATIVE_DRAW,
            "gated_count": _GATED,
            "pre_existing_native_clean_count": _NATIVE_DRAW - _COHORT_ROWS,
            "style_cohort_count": _COHORT_ROWS,
            "gated": sorted(gated),
            "superseded_note": (
                "Frozen D-204 snapshot: the 172/16/156 counts and the gated list "
                "predate D-066 F0. Since D-395 the live envelope admits "
                "description + config{htmlLabels|logLevel 0-2|fontFamily courier}; "
                "the eight htmlLabels rows listed in gated are governed by "
                "tools/mermaid/requirement-f0-cohort.json, and the live partition "
                "is 172 total / 24 native / 148 gated."),
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
