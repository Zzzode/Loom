#!/usr/bin/env python3
"""Generate the RFC 0043 small-family theme admission cohort ledger.

Renderer-free. The six rows are the last frontmatter-only fallbacks whose sole
blocker is an inert `config.theme` leaf in one of three families (ishikawa,
venn, railroad). Each family admits exactly one shape: a column-0 `config:`
block whose only leaf is an indent-2 `theme` from that family's closed table;
the accepted value is disclosed through a bounded footer and never painted.

The generator pins the six paths, their themes, their measured per-width
states, the literal footer lines and the twin used by the gate. Corpus twins
are verified body-identical after frontmatter stripping; the two rows with no
drawn body-equivalent use the synthetic config-free twin.

Per-row `expected_states.narrow_40` is NOT source-derivable: per rule 34 the
disclosure footer changes the whole-surface 40-column predicate, so it is
recorded from a live measurement (0/6 draw, all `source_fallback`) and
re-asserted live by the gate.

Usage: python3 tools/mermaid/generate_smallfam_0043_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/smallfam-0043-cohort.json")
SCHEMA = "loom_mermaid_smallfam_0043_cohort/v1"
PHASE = "P1-theme-admission"

THEME_TABLES = {
    "ishikawa": ("dark", "forest"),
    "venn": ("dark", "redux-color", "redux-dark-color"),
    "railroad": ("dark",),
}
NOTICE_HEADERS = {
    "ishikawa": "Ishikawa native projection v1",
    "venn": "Venn native projection v1",
    "railroad": "Railroad native projection v1",
}
NOTICE_LINE_LIMIT = 96
FOOTER_CHARGE = "size*4 + sizeof(std::string) + 32"
EMISSION_CHARGE = "(1 + n) * (4*sizeof(std::string) + 256) + bytes*2 + 256"

# Reviewed literal path set: family, theme, twin. Corpus twins must be drawn
# family rows whose post-frontmatter body is byte-identical.
COHORT = [
    {
        "path": "ishikawa/pr-check/7-should-render-with-forest-theme.mmd",
        "family": "ishikawa",
        "theme": "forest",
        "twin": {
            "kind": "corpus",
            "path": "ishikawa/pr-check/1-should-render-a-simple-ishikawa-diagram.mmd",
        },
    },
    {
        "path": "ishikawa/pr-check/8-should-render-with-dark-theme.mmd",
        "family": "ishikawa",
        "theme": "dark",
        "twin": {
            "kind": "corpus",
            "path": "ishikawa/pr-check/1-should-render-a-simple-ishikawa-diagram.mmd",
        },
    },
    {
        "path": "venn/nightly-check/9-should-render-with-dark-theme.mmd",
        "family": "venn",
        "theme": "dark",
        "twin": {"kind": "synthetic"},
    },
    {
        "path": "venn/nightly-check/three-sets-redux-color-theme.mmd",
        "family": "venn",
        "theme": "redux-color",
        "twin": {
            "kind": "corpus",
            "path": ("venn/pr-check/16-should-render-a-venn-diagram-with-a-3-set-union-"
                     "without-explicit-pairwise-subsets.mmd"),
        },
    },
    {
        "path": "venn/nightly-check/three-sets-redux-dark-color-theme.mmd",
        "family": "venn",
        "theme": "redux-dark-color",
        "twin": {
            "kind": "corpus",
            "path": ("venn/pr-check/16-should-render-a-venn-diagram-with-a-3-set-union-"
                     "without-explicit-pairwise-subsets.mmd"),
        },
    },
    {
        "path": "railroad/pr-check/ir-dark-theme.mmd",
        "family": "railroad",
        "theme": "dark",
        "twin": {"kind": "synthetic"},
    },
]

EXPECTED = {"originals": 43, "drawn": 26, "admitted": 6, "other": 11}

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

PHASE_REVIEWS = {
    PHASE: {
        "reviewer_agent": (
            "independent read-only adversarial code review (fresh general-purpose "
            "agent, RFC 0043 small-family theme admission, 2026-11-22; APPROVE "
            "with zero BLOCKER/MAJOR; 3 MINOR + 1 NIT resolved before the phase "
            "commit and re-verified by the gate)"),
        "date": "2026-11-22",
        "verdict": "approved",
        "note": (
            "Reviewer ran the live binaries: the gate, the three family cohort "
            "tests and the four hermetic --check entries; the only red was the "
            "intentionally pending review verdict this record now supplies. "
            "Verified gate fidelity against git diff and af_frontmatter_gated "
            "(indented visual keys still gate, column-0 passthrough closes the "
            "block, folded payload before/after config, fence rules, railroad "
            "column-0 trim only, tabs pre-rejected), af_scalar parity and Work "
            "charges, footer budgets with venn's 512-row cap including title "
            "rows, generator/C++ verdict parity, digest order, non-vacuous "
            "tests, and matrix branch placement. Items fixed: the quoted/"
            "comment/blank scalar positives now run for every family, dead "
            "census code removed from the four generators, the venn signature "
            "line restored, and the generator's tab-blank parity documented."),
    },
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def strip_frontmatter(text: str) -> str:
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[close + 1:])
    return text


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                     for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


def notice_lines(family: str, theme: str):
    return [NOTICE_HEADERS[family],
            f"config.theme={theme} -> colour palette; not applied"]


def build_review(repo_root):
    review = PHASE_REVIEWS[PHASE]
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

    # The per-family census is read from the three family ledgers (the
    # filesystem partition is asserted independently by the C++ gate); this
    # only reconciles the four-way totals against the pinned 43/26/6/11.
    family_counts = {}
    for family, ledger in (("ishikawa", "ishikawa-cohort.json"),
                           ("venn", "venn-cohort.json"),
                           ("railroad", "railroad-cohort.json")):
        parsed = json.loads((repo_root / "tools/mermaid" / ledger).read_text())
        selection = parsed["selection"]
        admitted = sum(1 for entry in COHORT if entry["family"] == family)
        if admitted != selection["admitted_count"]:
            raise ValueError(
                f"{family} admitted count {admitted} != family ledger "
                f"{selection['admitted_count']}")
        family_counts[family] = {
            "drawn": selection["draw_count"],
            "admitted": admitted,
            "other": selection["family_total"] - selection["draw_count"] - admitted,
        }

    totals = {key: sum(counts[key] for counts in family_counts.values())
              for key in ("drawn", "admitted", "other")}
    totals["originals"] = totals["drawn"] + totals["admitted"] + totals["other"]
    if totals != EXPECTED:
        raise ValueError(f"small-family census drift: {totals} != {EXPECTED}")

    rows = []
    for entry in COHORT:
        rel = entry["path"]
        family = entry["family"]
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        if entry["theme"] not in THEME_TABLES[family]:
            raise ValueError(f"theme {entry['theme']} not in {family} table")
        twin = dict(entry["twin"])
        if twin["kind"] == "corpus":
            twin_sha = sha256_file(repo_root / CASES / twin["path"])
            if twin_sha != manifest_rows[twin["path"]]["sha256"]:
                raise ValueError(f"twin sha drift for {twin['path']}")
            if strip_frontmatter((repo_root / CASES / rel).read_text()) != \
                    strip_frontmatter((repo_root / CASES / twin["path"]).read_text()):
                raise ValueError(f"{rel} is not body-identical to {twin['path']}")
            twin["sha256"] = twin_sha
        narrow = "drawn" if rel in NARROW_DRAW else "source_fallback"
        rows.append({
            "path": rel,
            "family": family,
            "theme": entry["theme"],
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": "source_fallback",
            "prediction": "predicted-draw",
            "track_scope": "whole-only",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": narrow,
                "restored_240": "drawn",
            },
            "expected_footer_lines": notice_lines(family, entry["theme"]),
            "twin": twin,
            "ledger_note": (
                "inert config.theme leaf admitted by the RFC 0043 closed table; "
                "the renderer never reads the value and the footer discloses it"),
        })

    return {
        "schema": SCHEMA,
        "rfc": 43,
        "phase": PHASE,
        "admission": {
            "shape": ("exactly one column-0 `config:` block whose sole leaf is an "
                      "indent-2 `theme` in the family's closed table"),
            "theme_tables": {family: list(table) for family, table in THEME_TABLES.items()},
            "footer": {
                "headers": NOTICE_HEADERS,
                "line_template": "config.theme=<value> -> colour palette; not applied",
                "line_limit_bytes": NOTICE_LINE_LIMIT,
                "per_line_charge": FOOTER_CHARGE,
                "emission_charge": EMISSION_CHARGE,
            },
            "width_policy": "wide_240 admission only; narrow_40 measured live",
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("theme-only frontmatter rows of the three key-gated small "
                     "families, admitted through one closed per-family table"),
            "families": sorted(THEME_TABLES),
            "census": family_counts,
            "path_pin": [entry["path"] for entry in COHORT],
            "cohort_digest": cohort_digest(rows),
            "twin_rule": ("wide-only frame equality = twin frame + the literal footer "
                          "delta through the same sentinel wrapper; narrow measured "
                          "live, never inherited from the twin"),
        },
        "counts": {
            "originals": totals["originals"],
            "drawn": totals["drawn"],
            "admitted": totals["admitted"],
            "other": totals["other"],
        },
        "review": build_review(repo_root),
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
    return 0


if __name__ == "__main__":
    sys.exit(main())
