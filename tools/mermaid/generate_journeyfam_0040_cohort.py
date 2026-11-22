#!/usr/bin/env python3
"""Generate the RFC 0040 journey-family look/theme admission cohort ledger.

Renderer-free. The three families' admitted sets are source-classifiable (their
non-namespaced look/theme rows all draw once the frontmatter gate is lifted),
so this generator derives them and pins each set by a digest that must equal
the corresponding existing cohort ledger's `admitted_digest`. It also pins the
whole-cohort path+sha digest, so a re-partition or a hand-edited ledger fails
`--check`.

Per-row `expected_states.narrow_40` is NOT source-derivable; like the three
existing generators it comes from a reviewed, measured set (empty unless the
phase measures a row drawing at 40 columns).

The roster and the matrix artifact are never read. Review records are authored
here and written into the ledger, never read back.

Usage: python3 tools/mermaid/generate_journeyfam_0040_cohort.py [--check]
"""

import argparse
import hashlib
import importlib.util
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/journeyfam-0040-cohort.json")
SCHEMA = "loom_mermaid_journeyfam_0040_cohort/v1"
PHASE = "P1-look-theme"
ENABLED_PHASES = (PHASE,)

EXPECTED = {
    "timeline": {"originals": 30, "admitted": 16,
                 "digest": "c20195a1ee4fcfe3e86bd8552f5f9cc83bd7ec7df3c934b288fe1889c383d349"},
    "gantt": {"originals": 52, "admitted": 11,
              "digest": "026f49b9cf460574be0b83ecd7d02e1c069d3e5026c133dc3218af5ce52a8bd6"},
    "journey": {"originals": 18, "admitted": 4,
                "digest": "d910db07953ea1379be6cb51ab0cb74104cb59dfea1bcf8f47ca7348c675a463"},
}
EXPECTED_TOTAL = 31

# Measured narrow-width draws (empty until the phase measures a row at 40
# columns; the ledger's expected_states are checked live by the gate).
NARROW_DRAW: set[str] = set()

COHORT_DIGEST = "cfdb3e80c92e566d3a4ec0d64aa72e824dddbd7b6807a12e8642d71c0246ac6b"

PHASE_REVIEWS = {
    PHASE: {
        "reviewer_agent": (
            "independent read-only adversarial code review (fresh general-purpose "
            "agent, RFC 0040 journey-family admission, 2026-11-22; round 1 "
            "REQUEST-CHANGES with 1 BLOCKER + 1 MAJOR + 2 MINOR + 4 NIT, all "
            "resolved, round 2 APPROVE)"),
        "date": "2026-11-22",
        "verdict": "approved",
        "note": (
            "Round 1: the delivered tree could not pass OfficialMatrix while the "
            "ledger stayed pending (this record closes it); the gantt/journey "
            "batteries did not implement the RFC section 3 per-family probe set. "
            "Fix: one FamilyBattery run per family covering every admitted value, "
            "sibling-only/unknown/case/empty, duplicate, quoted, trailing comment, "
            "unmatched quote, indent 0/2/4, themeVariables, description payload, "
            "admitted+in-body init, and the literal footer block (blank separator, "
            "header, look-before-theme) independent of the ledger; the drawn-row "
            "oracle pins 7+39+10=56 rows; the Python fence matcher is tightened to "
            "the C++ exact match. Round 2 verified each fix and the three TU "
            "helpers byte-identical; no protected file touched."),
    },
}


def _tables():
    spec = importlib.util.spec_from_file_location(
        "journeyfam_tables", pathlib.Path(__file__).resolve().parent / "journeyfam_tables.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


TABLES = _tables()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def cohort_digest(entries):
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n" for entry in entries)
    return sha256_bytes(payload.encode())


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    rows = []
    per_family = {}
    for family, expected in EXPECTED.items():
        originals = sorted(path.relative_to(cases).as_posix()
                           for path in (cases / family).rglob("*.mmd"))
        if len(originals) != expected["originals"]:
            raise ValueError(
                f"{family} family population drifted: expected {expected['originals']}, "
                f"got {len(originals)}")
        admitted = []
        for rel in originals:
            kind, leaves = TABLES.classify(family, (cases / rel).read_text())
            if kind != "admitted":
                continue
            footer = TABLES.notice_lines(family, leaves)
            if not footer:
                raise ValueError(f"{rel}: admitted row without a disclosure footer")
            admitted.append((rel, leaves, footer))
        if len(admitted) != expected["admitted"]:
            raise ValueError(
                f"{family} admitted set drifted: expected {expected['admitted']}, "
                f"got {len(admitted)}")
        digest = cohort_digest([{"path": rel, "sha256": sha256_bytes((cases / rel).read_bytes())}
                                for rel, _, _ in sorted(admitted)])
        if digest != expected["digest"]:
            raise ValueError(f"{family} admitted digest drifted: {digest} != {expected['digest']}")
        per_family[family] = {"originals": len(originals), "admitted": len(admitted),
                              "digest": digest}
        for rel, leaves, footer in sorted(admitted):
            rows.append({
                "path": rel,
                "family": family,
                "phase": PHASE,
                "sha256": sha256_bytes((cases / rel).read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "prior_status": manifest_rows[rel]["raw"],
                "twin": {"kind": "synthetic"},
                "expected_prepend_lines": [],
                # Blank separator, header, then one line per leaf (look, theme).
                "expected_footer_lines": [""] + footer,
                "prediction": "predicted-draw",
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "drawn" if rel in NARROW_DRAW else "source",
                    "restored_240": "drawn",
                },
                "ledger_note": (
                    f"RFC 0040 {family} look/theme admission; validated and disclosed only, "
                    "the frame equals the config-free twin's plus the bounded footer."),
            })

    rows.sort(key=lambda row: row["path"])
    if len(rows) != EXPECTED_TOTAL:
        raise ValueError(f"cohort total drifted: expected {EXPECTED_TOTAL}, got {len(rows)}")
    digest = cohort_digest(rows)
    if COHORT_DIGEST not in ("PENDING", digest):
        raise ValueError(f"RFC 0040 cohort row set drifted: digest {digest} != {COHORT_DIGEST}")

    return {
        "schema": SCHEMA,
        "rfc": 40,
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": "a `theme`/`look` key at indent 2 inside the single `config:` block, with "
                    "a value inside the family's closed corpus table and at most one per key",
            "roots": list(EXPECTED),
            "phase": PHASE,
            "cohort_digest": digest,
            "families": per_family,
            "twin_rule": "the frame must equal the synthesized config-free twin's frame plus "
                         "the recorded footer delta, line by line, both sides through the same "
                         "sentinel-bearing wrapper",
            "corpus_twins": "none: no drawn fixture of the three families is body-equivalent "
                            "to an admitted row",
            "pass_through": "every non-theme/look frontmatter key keeps the renderers' "
                            "long-standing pass-through (11 drawn rows rely on it)",
            "exclusions": "RFC 0047 admits the inert leading %%{init}%% directives "
                          "(timeline 5, journey 4) through the sibling cohort; the two "
                          "remaining timeline directives (disableMulticolor, themeVariables) "
                          "and the two malformed-date gantt rows stay whole-source",
            "width_policy": "wide_240 admission only; narrow_40 is measured per row.",
        },
        "review": {
            "phases": PHASE_REVIEWS,
            "verdict": "approved" if all(
                phase in PHASE_REVIEWS and PHASE_REVIEWS[phase]["verdict"] == "approved"
                for phase in ENABLED_PHASES) else "pending",
        },
        "counts": {
            "total": len(rows),
            "predicted_draw": len(rows),
            "timeline": per_family.get("timeline", {}).get("admitted", 0),
            "gantt": per_family.get("gantt", {}).get("admitted", 0),
            "journey": per_family.get("journey", {}).get("admitted", 0),
        },
        "rows": rows,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
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
    print("cohort digest", ledger["selection"]["cohort_digest"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
