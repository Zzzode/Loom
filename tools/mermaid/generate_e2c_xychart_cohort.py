#!/usr/bin/env python3
"""Generate the RFC 0031 E2c pre-registration ledger (xychart).

Why a separate ledger: a description-only frontmatter envelope is
documentation-only and family-agnostic. D236 extends the envelope admission
that was previously pie-only so a folded ``description`` (no config, no title,
no init) also admits an ``xychart`` body whose geometry the bounded renderer
already completes. The renderer-free ledger below pre-registers which of those
sources draw and which stay whole-source.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under cases/xychart that
    - opens/closes with column-0 ``---`` fences,
    - carries exactly one top-level key, ``description`` (a title or config key
      is out of scope and keeps the whole source),
    - folds that description with the ``>-`` header and two-space, tab-free
      payload lines (mirrors the production envelope validator),
    - carries no ``%%{init...}%%`` directive in the body,
    - has a body whose first real statement starts with ``xychart``.

Two outcomes, both renderer-free:
  predicted-draw      - every selected row NOT in the SHA-bound capability set.
  predicted-blocked   - one of five SHA-pinned rows whose body the bounded
                        renderer cannot complete at the cohort capture
                        geometry (four use inline per-point quoted labels the
                        terminal model cannot project, one is an over-limit
                        overlaid multi-series plot). The set is path/SHA bound
                        like the state native routing-debt set because
                        "completable by the bounded renderer" is not textually
                        decidable; any addition or
                        source change fails this generator and needs a new
                        independent review row.

E2c pre-registers the 240-column admission only; ``narrow_40`` is uniformly
``source`` and ``restored_240`` must equal ``wide_240``.

Usage: python3 tools/mermaid/generate_e2c_xychart_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/e2c-xychart-cohort.json")

SCHEMA = "loom_mermaid_e2c_xychart_cohort/v1"
BLOCKER_KIND = "xychart-body-capability-debt"
ROOT_REL = "xychart"

# Hard drift guards. A corpus change that alters the selected population or a
# pinned source makes the generator abort instead of silently re-bucketing.
EXPECTED_TOTAL = 16
EXPECTED_BLOCKED = 5
EXPECTED_DRAW = 11

# SHA-bound reviewed capability set. Each body is accepted by the folded-
# description envelope but does not complete through the bounded xychart
# renderer under the cohort's exact capture predicate (cell/background
# difference with no raw "xychart" token left in the frame). The gaps are
# inline per-point quoted labels (`line [v "label"]`, no terminal projection)
# and one over-limit overlaid multi-series source. The set is path/SHA bound
# rather than a lexical detector because "completable by the bounded renderer"
# is not textually decidable — a horizontal xychart with the same point-label
# spelling does complete — so any addition or source change aborts generation
# and needs a new independent review row.
BLOCKED_CAPABILITY_SET = {
    "xychart/nightly-check/should-render-a-horizontal-line-chart-with-point-labels.mmd": (
        "c39a0b63196992134d062cd6f35db724718d88a6f764a8792326c5ee77a28728",
        "inline per-point quoted labels in a series plus a body title; "
        "the terminal renderer has no per-point label projection.",
    ),
    "xychart/nightly-check/should-render-a-line-chart-with-point-labels.mmd": (
        "ae171482c4bec484f2ce09b90c9e701352726693880b45fce324292813dea0ef",
        "inline per-point quoted labels (line [v \"label\"]) and a body title; "
        "the terminal renderer has no per-point label projection.",
    ),
    "xychart/nightly-check/should-render-a-line-chart-with-mixed-labels-some-points-labeled-some-not.mmd": (
        "5c4107714fc02154ef21650701bfb5a8d3b2b9fbd1076dfddbcaa797e511f6b1",
        "sparse inline per-point quoted labels; "
        "the terminal renderer has no per-point label projection.",
    ),
    "xychart/nightly-check/should-render-multiple-lines-where-only-one-has-labels.mmd": (
        "8a9d16b33715e0c93f3d4108902564abaf303a78ff3206c268212b0cd7c43aed",
        "inline point labels on a multi-series plot; "
        "the terminal renderer has no per-point label projection.",
    ),
    "xychart/pr-check/multiple-plots-can-be-rendered.mmd": (
        "e89f2d0b4ee840beff6569ed8463d528f6e8b51db82d3d0c54d01e157bf67ce2",
        "six overlaid line/bar series do not complete under the bounded "
        "multi-series renderer profile at the cohort capture geometry.",
    ),
}


def sha256_file(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def split_source(text: str):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return lines[1:close], lines[close + 1:]
    return None


def body_has_init(body_lines) -> bool:
    return any(line.lstrip().startswith("%%{") for line in body_lines)


def folded_description_only(frontmatter_lines) -> bool:
    """Mirror the production description envelope: exactly one description key,
    folded `>-`, with non-empty two-space, tab-free payload lines."""
    keys = [line.split(":", 1)[0].strip()
            for line in frontmatter_lines if line and not line.startswith(" ")]
    if keys != ["description"]:
        return False
    header = next((line.split(":", 1)[1].strip()
                   for line in frontmatter_lines
                   if line.split(":", 1)[0].strip() == "description"), "")
    if header != ">-":
        return False
    payload = False
    for line in frontmatter_lines:
        if not line or line.startswith("description"):
            continue
        if line.find("\t") != -1 or not line.startswith("  ") or line.startswith("   "):
            return False
        payload = True
    return payload


def body_first_keyword(body_lines) -> str:
    for line in body_lines:
        stripped = line.strip()
        if stripped and not stripped.startswith("%%"):
            return stripped.split()[0] if stripped.split() else ""
    return ""


def collect(cases: pathlib.Path):
    selected = []
    for path in sorted((cases / ROOT_REL).rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        parts = split_source(path.read_text())
        if parts is None:
            continue
        frontmatter, body = parts
        if body_has_init(body):
            continue
        if not folded_description_only(frontmatter):
            continue
        if not body_first_keyword(body).startswith("xychart"):
            continue
        selected.append(rel)
    return selected


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    selected = collect(cases)
    if len(selected) != EXPECTED_TOTAL:
        raise ValueError(
            f"E2c selected population drifted: expected {EXPECTED_TOTAL}, got {len(selected)}; "
            "re-scope the ledger and obtain a new independent review")

    blocked_paths = set(BLOCKED_CAPABILITY_SET)
    found_blocked = {rel for rel in selected if rel in blocked_paths}
    if found_blocked != blocked_paths:
        missing = sorted(blocked_paths - set(selected))
        extra = sorted(set(selected) & blocked_paths - found_blocked)
        raise ValueError(f"E2c capability set mismatch; missing={missing} extra={extra}")

    rows = []
    counts = {"total": 0, "predicted_draw": 0, "predicted_blocked": 0}
    for rel in selected:
        source_sha = sha256_file(cases / rel)
        blocked = rel in blocked_paths
        if blocked:
            pinned_sha, detail = BLOCKED_CAPABILITY_SET[rel]
            if source_sha != pinned_sha:
                raise ValueError(f"E2c pinned source changed without review: {rel}")
        counts["total"] += 1
        counts["predicted_blocked" if blocked else "predicted_draw"] += 1
        wide = "source" if blocked else "drawn"
        rows.append({
            "path": rel,
            "family": "xychart",
            "sha256": source_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "has_config": False,
            "has_init": False,
            "prediction": "predicted-blocked" if blocked else "predicted-draw",
            "expected_states": {
                "wide_240": wide,
                "narrow_40": "source",
                "restored_240": wide,
            },
            "blocker": (
                {
                    "kind": BLOCKER_KIND,
                    "site": "src/ui/visual/mermaid_charts_impl.cpp (bounded xychart renderer)",
                    "evidence": pinned_sha,
                    "detail": detail,
                }
                if blocked else None),
            "ledger_note": (
                "E2c folded-description envelope admission; body completed by the renderer"
                if not blocked else
                "SHA-bound reviewed capability set; envelope accepted but body not completable"),
        })

    if (counts["predicted_blocked"] != EXPECTED_BLOCKED or
            counts["predicted_draw"] != EXPECTED_DRAW):
        raise ValueError(f"E2c partition drift: {counts}")

    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "E2c",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256((repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "xychart frontmatter original with exactly one folded (>-) description key, "
                    "no config/title/init, and an xychart body",
            "roots": [ROOT_REL],
            "blocker_detection": "one SHA-bound reviewed body-capability set "
                                 "(four inline per-point quoted-label cases and one over-limit "
                                 "overlaid multi-series case); "
                                 "no generator-time rendering; corpus or source drift aborts.",
            "width_policy": "E2c pre-registers the 240-column admission; narrow_40 is "
                            "uniformly 'source'.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial D236 review "
                              "(fresh general-purpose agent; own source-derived selector and an "
                              "independent gtest-predicate-equivalent render probe reproduced the "
                              "16-row set and the 11/5 partition with zero mismatches, plus a "
                              "stash A/B proving no new over-admission); no prior E2c partition",
            "date": "2026-11-15",
            "verdict": "approved",
            "note": "D236 extends the folded-description envelope from pie-only to family-"
                    "agnostic so eleven description-only xychart originals draw; five SHA-bound "
                    "rows stay whole-source on inline per-point label / over-limit series "
                    "capability limits. Approved 11/5 partition.",
        },
        "counts": counts,
        "blocker_vocabulary": [BLOCKER_KIND],
        "rows": rows,
    }


def main() -> int:
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
