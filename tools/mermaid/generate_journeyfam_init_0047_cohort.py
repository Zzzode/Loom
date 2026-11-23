#!/usr/bin/env python3
"""Generate the RFC 0047 journey-family in-body init directive cohort ledger.

Renderer-free. Two duties:

1. Run the single-source boundary probe artifact
   `journeyfam-init-0047-probes.json` through the Python mirror
   (`journeyfam_tables.classify` + `notice_lines`) and fail on any disagreement.
   The C++ gate reads the same artifact, so both sides are driven by one case
   list (efficiency rule 37).
2. Derive the 11-row cohort (journey 4, timeline 7, gantt 0 — the last two are
   the D-454 nested `timeline`/`themeVariables` rows) from `classify`, pin it by
   a path+sha digest and by `selection.probes_sha256`, and write the ledger.

Per-row `expected_states.narrow_40` is NOT source-derivable; like the sibling
generators it comes from a reviewed, measured set (empty unless the phase
measures a row drawing at 40 columns).

The roster and the matrix artifact are never read. Review records are authored
here and written into the ledger, never read back.

Usage: python3 tools/mermaid/generate_journeyfam_init_0047_cohort.py [--check]
"""

import argparse
import hashlib
import importlib.util
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/journeyfam-init-0047-cohort.json")
PROBES = pathlib.Path("tools/mermaid/journeyfam-init-0047-probes.json")
SCHEMA = "loom_mermaid_journeyfam_init_0047_cohort/v1"
PHASE = "P1-initializers"

# D-454 adds the two timeline rows whose nested `timeline`/`themeVariables`
# payloads are now inside the closed grammar.
EXPECTED = {"journey": 4, "timeline": 7, "gantt": 0}
EXPECTED_TOTAL = 11
EXPECTED_KINDS = {"init_admitted", "admitted", "drawn", "themed"}

# Measured narrow-width draws (empty until the phase measures a row at 40
# columns; the ledger's expected_states are checked live by the gate).
NARROW_DRAW: set[str] = set()

COHORT_DIGEST = "258216d09aec53f4f1da13951711c01dcfbe28d5ea41f5eb4b88014eafa1dac7"
PROBES_SHA256 = "c920f603f238bd5b085d7c103d7e539a5f4787bd7bba9fed00cead9c060f8330"

PHASE_REVIEWS = {
    PHASE: {
        "reviewer_agent": (
            "independent read-only adversarial code review (fresh general-purpose "
            "agent, RFC 0047 journey-family init-directive admission, 2026-11-22)"),
        "date": "2026-11-22",
        "verdict": "approved",
        "note": (
            "Filled at phase closure: the probe artifact review, the per-row "
            "frame = twin + footer evidence, and the paired flip/negative audit."),
    },
}

# Decision-scope reviews that extend this cohort after its RFC phase. Each
# record names its own reviewer and artifact scope; the ledger verdict is the
# conjunction of every record.
DECISION_REVIEWS = {
    "D-454": {
        "reviewer_agent": (
            "independent read-only adversarial code review (fresh general-purpose "
            "agent, D-454 timeline nested init payloads, 2026-11-24; round 1 "
            "REQUEST-CHANGES with a mirror-divergence blocker plus five findings, "
            "round 2 re-review of the fixes, round 3 verification of the "
            "provenance record and the join bound)"),
        "date": "2026-11-24",
        "scope": (
            "nested `timeline.disableMulticolor` / `themeVariables.cScaleN` "
            "admission, cross-directive leaf-identity duplicate rejection, "
            "canonical cScale index, spanning-directive join bounded at 2048 "
            "bytes, and the classify_mermaid continuation peek"),
        "verdict": "approved",
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


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n" for entry in entries)
    return sha256_bytes(payload.encode())


def assemble(entry) -> str:
    """The one assembly recipe both sides use: fences included verbatim."""
    return entry["frontmatter"] + "\n".join(entry["body"]) + "\n"


def run_probes(repo_root: pathlib.Path) -> str:
    raw = (repo_root / PROBES).read_bytes()
    document = json.loads(raw)
    if document.get("schema") != "loom_journeyfam_init_0047_probes/v1":
        raise ValueError("probe artifact schema drifted")
    allowed = {"family", "frontmatter", "body", "expected_kind", "expected_footer", "note"}
    seen = set()
    for index, entry in enumerate(document["entries"]):
        if (not isinstance(entry, dict) or (set(entry) - allowed)
                or (allowed - set(entry))):
            raise ValueError(f"probe {index}: field set drifted")
        family = entry["family"]
        if family not in EXPECTED:
            raise ValueError(f"probe {index}: unknown family {family!r}")
        if entry["expected_kind"] not in EXPECTED_KINDS:
            raise ValueError(f"probe {index}: unknown kind {entry['expected_kind']!r}")
        if entry["expected_kind"] != entry["expected_kind"].strip():
            raise ValueError(f"probe {index}: kind whitespace")
        if not isinstance(entry["body"], list) or not entry["body"]:
            raise ValueError(f"probe {index}: body must be a non-empty list")
        footer = entry["expected_footer"]
        if not isinstance(footer, list) or any(not isinstance(line, str) for line in footer):
            raise ValueError(f"probe {index}: footer must be a string list")
        if entry["expected_kind"] in ("init_admitted", "admitted") and not footer:
            raise ValueError(f"probe {index}: admitted probe without a footer")
        if entry["expected_kind"] in ("drawn", "themed") and footer:
            raise ValueError(f"probe {index}: non-admitted probe with a footer")
        key = (family, entry["frontmatter"], tuple(entry["body"]))
        if key in seen:
            raise ValueError(f"probe {index}: duplicate case")
        seen.add(key)
        kind, leaves = TABLES.classify(family, assemble(entry))
        if kind != entry["expected_kind"]:
            raise ValueError(
                f"probe {index} ({entry['note']}): mirror kind {kind} != "
                f"{entry['expected_kind']}")
        if kind == "init_admitted":
            lines = TABLES.notice_lines(family, {}, leaves)
        else:
            lines = TABLES.notice_lines(family, leaves)
        if lines != footer:
            raise ValueError(
                f"probe {index} ({entry['note']}): mirror footer {lines} != {footer}")
    return sha256_bytes(raw)


def build(repo_root: pathlib.Path):
    probes_sha = run_probes(repo_root)
    cases = repo_root / CASES
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    rows = []
    per_family = {}
    for family, expected in EXPECTED.items():
        selected = []
        for path in sorted((cases / family).rglob("*.mmd")):
            rel = path.relative_to(cases).as_posix()
            kind, leaves = TABLES.classify(family, path.read_text())
            if kind != "init_admitted":
                continue
            footer = TABLES.notice_lines(family, {}, leaves)
            if not footer:
                raise ValueError(f"{rel}: admitted directive row without a footer")
            selected.append((rel, leaves, footer))
        if len(selected) != expected:
            raise ValueError(f"{family} init cohort drifted: expected {expected}, "
                             f"got {len(selected)}")
        for rel, leaves, footer in selected:
            rows.append({
                "path": rel,
                "family": family,
                "phase": PHASE,
                "sha256": sha256_bytes((cases / rel).read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "prior_status": manifest_rows[rel]["raw"],
                "directive": {
                    name.replace("init_", ""): value
                    for name, value in leaves.items()
                    if name != "init_nested_idents"},
                "twin": {"kind": "synthetic",
                         "rule": "delete the frontmatter and every leading "
                                 "%%{init...}%% line"},
                "expected_prepend_lines": [],
                # Blank separator, header, then the directive lines in canonical
                # logLevel-before-theme order.
                "expected_footer_lines": [""] + footer,
                "prediction": "predicted-draw",
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "drawn" if rel in NARROW_DRAW else "source",
                    "restored_240": "drawn",
                },
                "ledger_note": (
                    f"RFC 0047 {family} leading %%{{init}}%% admission; the "
                    "directives are logger/palette-only and not applied, so the "
                    "frame equals the directive-free twin's plus the bounded footer."),
            })
        per_family[family] = len(selected)

    rows.sort(key=lambda row: row["path"])
    if len(rows) != EXPECTED_TOTAL:
        raise ValueError(f"cohort total drifted: expected {EXPECTED_TOTAL}, got {len(rows)}")
    digest = cohort_digest(rows)
    if digest != COHORT_DIGEST:
        raise ValueError(f"cohort row set drifted: digest {digest} != {COHORT_DIGEST}")
    if probes_sha != PROBES_SHA256:
        raise ValueError(
            f"probe artifact changed without review: sha256 {probes_sha} != "
            f"{PROBES_SHA256}")

    return {
        "schema": SCHEMA,
        "rfc": 47,
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": "a leading %%{init: {...}}%% directive (single-line or "
                    "spanning) over logLevel/theme, and since D-454 the closed "
                    "nested `timeline.disableMulticolor` / "
                    "`themeVariables.cScaleN` leaves, all inside the family's "
                    "closed tables, with no frontmatter visual leaf and no "
                    "repeated leaf across directives",
            "roots": list(EXPECTED),
            "phase": PHASE,
            "cohort_digest": digest,
            "probes_sha256": probes_sha,
            "families": per_family,
            "exclusions": "every other content key, named colours, deeper "
                          "nesting, non-canonical cScale indexes and repeated "
                          "leaves stay whole-source",
            "width_policy": "wide_240 admission only; narrow_40 is measured per row.",
        },
        "review": {
            "phases": PHASE_REVIEWS,
            "decisions": DECISION_REVIEWS,
            "probes_sha256": probes_sha,
            "verdict": (
                "approved"
                if all(record["verdict"] == "approved"
                       for record in [*PHASE_REVIEWS.values(), *DECISION_REVIEWS.values()])
                else "pending"),
        },
        "counts": {
            "total": len(rows),
            "predicted_draw": len(rows),
            "journey": per_family.get("journey", 0),
            "timeline": per_family.get("timeline", 0),
            "gantt": per_family.get("gantt", 0),
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
    print("probes sha256", ledger["selection"]["probes_sha256"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
