#!/usr/bin/env python3
"""Generate the RFC 0031 timeline-family renderer cohort ledger.

The timeline renderer (mermaid_timeline_impl.cpp) projects the timeline
grammar (`timeline [TD]`, title, optional sections, `period : event : ...`
with colon continuations and <br> breaks) to a bounded native linear text
table. It has no color contract, so any themed fixture stays on whole-source
fallback.

Renderer-free; the durable gtest enforces every state live. Drift guard pins
the 7 clean / 23 themed partition; manifest shas are pinned; the review block
is preserved across regeneration.

Usage: python3 tools/mermaid/generate_timeline_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/timeline")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/timeline-cohort.json")
SCHEMA = "loom_mermaid_timeline_cohort/v1"

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_timeline_cohort.cpp. The short datasets fit 40
# columns; the long/multi-section/stacked ones stay source at narrow width.
# Fixture 11 joined the drawn set after M1: colon-splitting its continuation
# lines replaced one 52-cell event row with two shorter rows (~33 cells),
# fitting the 40-column terminal.
NARROW_DRAW = {
    "timeline/nightly-check/13-should-render-a-simple-vertical-timeline-with-no-specific-sections.mmd",
    "timeline/nightly-check/11-should-render-timeline-with-many-stacked-events-and-proper-timeline-line-length.mmd",
    "timeline/pr-check/1-should-render-a-simple-timeline-with-no-specific-sections.mmd",
    "timeline/pr-check/12-should-render-timeline-with-proper-vertical-line-lengths-for-all-columns.mmd",
}


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

import importlib.util


def _journeyfam_tables():
    """Load the RFC 0040 shared tables from an explicit path.

    The hermetic ctests run `python3 -I`, which implies `-P`, so a plain
    sibling import fails; this is the pattern the admission verifiers use.
    """
    spec = importlib.util.spec_from_file_location(
        "journeyfam_tables", pathlib.Path(__file__).resolve().parent / "journeyfam_tables.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


TABLES = _journeyfam_tables()


def admitted_digest(repo_root, paths):
    """sha256 over sorted `path<TAB>source_sha256` rows: the reviewed set pin."""
    cases = repo_root / "tests/fixtures/mermaid_official/cases"
    payload = "".join(f"{p}\t{sha256_file(cases / p)}\n" for p in sorted(paths))
    return hashlib.sha256(payload.encode()).hexdigest()




def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, timeline renderer "
                           "cohort, 2026-11-11, task a7f26a8ecbe8991e6)"),
        "date": "2026-11-11",
        "verdict": "approved",
        "note": ("APPROVE WITH CONDITIONS, all conditions resolved before "
                 "commit. B1: this review block replaces the placeholder id. "
                 "M1: colon continuation lines now split on ':' via the "
                 "shared append_events path, matching the period-line grammar "
                 "(corpus max 9 events/period, cap 16, so the 7-row draw "
                 "partition is unchanged). M2: theme-gate shape differences "
                 "vs the journey renderer produce zero corpus mismatches; "
                 "documented in the renderer comments and kept key-based. "
                 "M3: durable fail-closed tests added for continuation "
                 "before period, unknown statement/header, event/period/"
                 "section cap exhaustion, 65-cell hard word, and "
                 "all-whitespace events. N1: <br> markers are now matched "
                 "case-insensitively like upstream HTML tag matching, with "
                 "regression coverage. N2/N3 accepted as-is."),
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

    clean, admitted, themed = set(), set(), set()
    buckets = {"drawn": clean, "admitted": admitted, "themed": themed}
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        rel = path.relative_to(repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        kind, _ = TABLES.classify("timeline", path.read_text())
        buckets[kind].add(rel)

    # RFC 0040 four-way partition (timeline has no fail_closed rows).
    if len(clean) != 7 or len(admitted) != 16 or len(themed) != 7:
        raise ValueError(
            f"timeline partition drift: drawn={len(clean)} admitted={len(admitted)} "
            f"themed={len(themed)} (expected 7/16/7)")
    if len(clean) + len(admitted) + len(themed) != 30:
        raise ValueError("timeline four-way partition does not sum to 30")

    rows = []
    for rel in sorted(clean):
        observed_sha = sha256_file(repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        narrow = "drawn" if rel in NARROW_DRAW else "source"
        rows.append({
            "path": rel,
            "family": "timeline",
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
                "timeline renderer projects title/section/period/events to a "
                "bounded native text table; no theme key is present so the "
                "whole original draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "timeline-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_timeline_impl.cpp",
            "grammar": "timeline [TD]; title; section; period : event [: event]",
            "theme_gate": (
                "RFC 0040: theme/look at indent 2 inside config is validated "
                "against the closed tables and disclosed; any other visual key "
                "and an in-body %%{init theme}%% directive keep the whole "
                "original on source fallback"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256((repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "timeline fixtures with no theme/look visual gate",
            "roots": ["timeline"],
            "clean_count": len(clean),
            "themed_excluded_count": len(themed),
            "themed_excluded": sorted(themed),
            "admitted_count": len(admitted),
            "admitted": sorted(admitted),
            "admitted_digest": admitted_digest(repo_root, admitted),
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
