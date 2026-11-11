#!/usr/bin/env python3
"""Generate the RFC 0031 journey-family renderer cohort ledger.

The journey renderer (mermaid_journey_impl.cpp) projects the structured
user-journey grammar (title / section / `label: score: actors`) to a bounded
native text table. It has no color contract, so any fixture carrying a theme
— frontmatter `config.theme`/`look` or an in-body `%%{init ... theme ...}%%`
directive — stays on ordinary whole-source fallback.

The generator is renderer-free; the durable gtest enforces every state live.
It pins manifest shas, scans the whole journey tree to pin the 10 clean /
8 themed partition (drift guard), and preserves an existing review block.

Usage: python3 tools/mermaid/generate_journey_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/journey")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/journey-cohort.json" )
SCHEMA = "loom_mermaid_journey_cohort/v1"

# Per-width geometry facts not derivable without rendering; the small journey
# bodies that fit 40 columns draw narrow, the long-label ones stay source.
# Enforced live by tests/ui/test_mermaid_journey_cohort.cpp.
NARROW_DRAW = {
    "journey/nightly-check/left-margin-150px.mmd",
    "journey/nightly-check/title-styling.mmd",
    "journey/nightly-check/usemaxwidth-true.mmd",
    "journey/pr-check/should-render-a-user-journey-chart.mmd",
    "journey/pr-check/should-render-a-user-journey-diagram-when-usemaxwidth-is-false.mmd",
}


def split_frontmatter(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None, text


def is_themed(text, frontmatter, body):
    if frontmatter is not None and re.search(r"theme|look\s*:", frontmatter):
        return True
    return "%%{" in body and "theme" in body


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, journey renderer "
                           "cohort, 2026-11-11, task a9c9dfef3711bc7f6, "
                           "zero builds)"),
        "date": "2026-11-11",
        "verdict": "approved",
        "note": (
            "Initial verdict REQUEST-CHANGES; all findings resolved before "
            "approval. MAJOR M1: score parse moved from throwing std::stoi to "
            "non-throwing std::from_chars with full-ptr/errc and 0..5 checks, "
            "so an over-long digit run fails closed (regression test added). "
            "MINOR m1: theme gate is key-based on frontmatter theme/look and "
            "directive-local in-body %%{init theme}%% scans, so a task label "
            "containing the word theme is not falsely rejected (tests added). "
            "m2: title/section/label/actor now pass unicode_text. m3: this "
            "review id replaced the placeholder. m4: the durable gate now "
            "renders and pins the body track separately and asserts the "
            "restored frame. NITs: task cap renamed kJourneyMaxTasksPerSection "
            "(per-section bound), CRLF trailing CR stripped, section names "
            "length-capped. The matrix hook only ever emits ledger statuses for "
            "exact ledger paths and replaces the run_case observation 1:1, so "
            "the 2738 observation total and the flow 48-gap/Class285 frozen "
            "contracts are unreachable from this change."),
    }
    existing = repo_root / LEDGER
    if existing.exists():
        try:
            prior = json.loads(existing.read_text())
            if isinstance(prior.get("review"), dict):
                review = prior["review"]
        except json.JSONDecodeError:
            pass
    return review


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    clean, themed = set(), set()
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        frontmatter, body = split_frontmatter(path.read_text())
        rel = path.relative_to(repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        (themed if is_themed(path.read_text(), frontmatter, body) else clean).add(rel)

    if len(clean) != 10 or len(themed) != 8:
        raise ValueError(
            f"journey partition drift: clean={len(clean)} themed={len(themed)} "
            f"(expected 10/8)")

    rows = []
    for rel in sorted(clean):
        observed_sha = sha256_file(repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        narrow = "drawn" if rel in NARROW_DRAW else "source"
        rows.append({
            "path": rel,
            "family": "journey",
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
            "expected_body_states": {
                "wide_240": "drawn",
                "narrow_40": narrow,
                "restored_240": "drawn",
            },
            "ledger_note": (
                "journey renderer projects title/section/task/score/actors to "
                "a bounded native text table; no theme key is present so the "
                "whole original draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "journey-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_journey_impl.cpp",
            "grammar": "journey header; title; section; label: score [: actors]",
            "theme_gate": (
                "frontmatter theme/look or an in-body %%{init theme}%% "
                "directive keeps the whole original on source fallback"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256((repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "journey fixtures with no theme/look visual gate",
            "roots": ["journey"],
            "clean_count": len(clean),
            "themed_excluded_count": len(themed),
            "themed_excluded": sorted(themed),
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
