#!/usr/bin/env python3
"""Generate the RFC 0031 gantt-family renderer cohort ledger.

The gantt renderer (mermaid_gantt_impl.cpp) projects the gantt grammar
(sections; status tags; ids; absolute/clock/epoch points; durations;
after/until dependencies; milestones) to a bounded native structural text
table without calendar math. It has no color contract, so any themed
fixture stays on whole-source fallback, and the two corpus fixtures with
deliberately malformed dates must fail closed.

Renderer-free; the durable gtest enforces every state live. Drift guards
pin the draw / themed / fail-closed partition; manifest shas are pinned;
the review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_gantt_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/gantt")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/gantt-cohort.json")
SCHEMA = "loom_mermaid_gantt_cohort/v1"

# The two corpus fixtures whose malformed task dates (a 3-digit year and a
# 1-digit, non-zero-padded day) the strict shape validator must reject.
FAIL_CLOSED = {
    "gantt/nightly-check/should-fail-rendering-a-gantt-chart-for-issue-1060-with-invalid-date.mmd",
    "gantt/nightly-check/should-handle-dates-with-year-typo-like-202-instead-of-2024-issue-5496.mmd",
}

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_gantt_cohort.cpp. Populated from live measurement.
NARROW_DRAW: set[str] = {
    "gantt/nightly-check/should-handle-milliseconds.mmd",
    "gantt/nightly-check/should-handle-multiple-dependencies-syntax-with-after-and-until.mmd",
    "gantt/nightly-check/should-handle-numeric-timestamps-with-dateformat-x-2.mmd",
    "gantt/nightly-check/should-handle-numeric-timestamps-with-dateformat-x.mmd",
    "gantt/nightly-check/should-handle-seconds-only-format-with-tickinterval-issue-5496.mmd",
    "gantt/nightly-check/should-render-a-gantt-diagram-excluding-friday-and-saturday-in-yyyy-mm-dd-hh-mm-ss-format.mmd",
    "gantt/nightly-check/should-render-a-gantt-diagram-excluding-friday-and-saturday.mmd",
    "gantt/nightly-check/should-render-a-gantt-diagram-excluding-saturday-and-sunday.mmd",
    "gantt/nightly-check/should-render-a-gantt-diagram-with-tick-is-1-day-and-topaxis-is-true.mmd",
    "gantt/nightly-check/should-render-a-gantt-diagram-with-tick-is-1-day.mmd",
    "gantt/nightly-check/should-render-a-gantt-diagram-with-tick-is-1-month.mmd",
    "gantt/nightly-check/should-render-a-gantt-diagram-with-tick-is-1-week.mmd",
    "gantt/nightly-check/should-render-a-gantt-diagram-with-tick-is-15-minutes.mmd",
    "gantt/nightly-check/should-render-a-gantt-diagram-with-tick-is-2-milliseconds.mmd",
    "gantt/nightly-check/should-render-a-gantt-diagram-with-tick-is-2-seconds.mmd",
    "gantt/nightly-check/should-render-a-gantt-diagram-with-tick-is-6-hours.mmd",
    "gantt/nightly-check/should-render-multi-line-excludes-issue-6270.mmd",
    "gantt/nightly-check/should-render-only-the-day-when-using-dateformat-d.mmd",
    "gantt/nightly-check/should-render-when-compact-is-true.mmd",
    "gantt/nightly-check/should-render-when-theres-a-hashtag-in-a-section-is-true.mmd",
    "gantt/nightly-check/should-render-when-theres-a-hashtag-in-the-task-data.mmd",
    "gantt/nightly-check/should-render-when-theres-a-hashtag-in-the-title.mmd",
    "gantt/nightly-check/should-render-when-theres-a-semicolon-in-a-section-is-true.mmd",
    "gantt/nightly-check/should-render-when-theres-a-semicolon-in-the-task-data.mmd",
    "gantt/nightly-check/should-render-when-theres-a-semicolon-in-the-title.mmd",
    "gantt/pr-check/multiple-dependencies-syntax.mmd",
    "gantt/pr-check/should-render-a-gantt-chart-usemaxwidth-false.mmd",
    "gantt/pr-check/should-render-a-gantt-chart.mmd",
    "gantt/pr-check/should-render-a-gantt-diagram-excluding-a-specific-date-in-yyyy-mm-dd-hh-mm-ss-format.mmd",
    "gantt/pr-check/should-render-a-gantt-diagram-excluding-saturday-and-sunday-in-yyyy-mm-dd-hh-mm-ss-format.mmd",
    "gantt/pr-check/should-render-a-gantt-diagram-with-a-vert-tag.mmd",
    "gantt/pr-check/should-render-a-gantt-diagram-with-data-labels-at-the-top-when-topaxis-is-true.mmd",
    "gantt/pr-check/should-render-a-gantt-diagram-with-tick-is-1-week-with-the-day-starting-on-monday.mmd",
    "gantt/pr-check/should-render-a-gantt-diagram-with-very-large-intervals-skipping-excludes-if-interval-5-years.mmd",
    "gantt/pr-check/should-render-vertical-markers-without-creating-extra-rows-and-show-their-labels.mmd",
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
    if frontmatter is not None and re.search(r"^\s*(theme|look)\s*:", frontmatter, re.M):
        return True
    return "%%{" in body and "theme" in body


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, gantt renderer "
                           "cohort, 2026-11-12, task a5f5a7ca0477aa5e0)"),
        "date": "2026-11-12",
        "verdict": "approved",
        "note": ("APPROVE with zero MAJOR; one MINOR and four NITs all "
                 "resolved before commit. M1: a zero-span milestone now "
                 "transcribes as the milestone marker instead of 'end 0', "
                 "with a span-text regression assertion. N1: leading-id cap "
                 "checked before insertion so both id limits are eight. N2: "
                 "after/until keywords matched case-insensitively via "
                 "starts_keyword, with an UPPER-case regression. N3: section "
                 "cap semantic documented in-code. N4: is_clock comment "
                 "corrected to shape-only validation. The reviewer's "
                 "independent parser reimplementation reproduced the exact "
                 "39 draw / 11 themed / 2 fail-closed partition."),
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

    draw, themed, fail_closed = set(), set(), set()
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        text = path.read_text()
        rel = path.relative_to(repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        frontmatter, body = split_frontmatter(text)
        if rel in FAIL_CLOSED:
            fail_closed.add(rel)
        elif is_themed(text, frontmatter, body):
            themed.add(rel)
        else:
            draw.add(rel)

    # Drift guards: the partition is a reviewed fact, not an assumption.
    if len(draw) != 39 or len(themed) != 11 or len(fail_closed) != 2:
        raise ValueError(
            f"gantt partition drift: draw={len(draw)} themed={len(themed)} "
            f"fail_closed={len(fail_closed)} (expected 39/11/2)")
    if set(FAIL_CLOSED) != fail_closed:
        raise ValueError("gantt fail-closed fixture set drift")

    rows = []
    for rel in sorted(draw):
        observed_sha = sha256_file(repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        narrow = "drawn" if rel in NARROW_DRAW else "source"
        rows.append({
            "path": rel,
            "family": "gantt",
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
                "gantt renderer projects title/section/task schedule tokens "
                "to a bounded native structural text table; no theme key is "
                "present so the whole original draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "gantt-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_gantt_impl.cpp",
            "grammar": ("gantt; dateFormat/axisFormat/tickInterval/excludes/"
                        "todayMarker directives; section; task tags, ids, "
                        "points, durations, after/until, milestone"),
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
            "rule": "gantt fixtures with no theme/look gate and valid date shapes",
            "roots": ["gantt"],
            "draw_count": len(draw),
            "themed_excluded_count": len(themed),
            "themed_excluded": sorted(themed),
            "fail_closed_count": len(fail_closed),
            "fail_closed": sorted(fail_closed),
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
