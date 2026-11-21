#!/usr/bin/env python3
"""Generate the RFC 0042 pie look/theme admission cohort ledger.

Renderer-free. Classifying a pie fixture needs more than its frontmatter: the
four body-blocked theme rows (dark/default/forest/neutral, carrying
`accTitle:`/`accDescr:`) share the admitted frontmatter shape, so the
admissible set is "frontmatter matches (shape AND value table) and the body
carries no accTitle:/accDescr: statement". Counts are pinned (24 originals /
7 drawn / 5 admitted / 12 other).

Per-row `expected_states.narrow_40` is NOT source-derivable: like the other
cohort generators it comes from a reviewed, measured set. Per rule 34 the
disclosure footer changes the whole-surface 40-column predicate, so the value
must be measured live and never inherited from a twin. The roster and the
matrix artifact are never read; review records are authored here and written
into the ledger, never read back.

Usage: python3 tools/mermaid/generate_pie_0042_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/pie-0042-cohort.json")
SCHEMA = "loom_mermaid_pie_0042_cohort/v1"
PHASE = "P1-theme-admission"
ENABLED_PHASES = (PHASE,)

EXPECTED = {"originals": 24, "drawn": 7, "admitted": 5, "other": 12}

THEME_TABLE = ("dark", "redux", "redux-dark", "redux-color", "redux-dark-color")
NOTICE_HEADER = "Pie native projection v1"

# The family's drawn rows are the reviewed Pie7 whole-scene pins (hardcoded in
# tests/ui/test_mermaid_official.cpp); the corpus twin must be one of them.
PIE7 = {
    "pie/nightly-check/should-render-a-simple-pie-diagram-with-long-labels.mmd",
    "pie/nightly-check/should-render-pie-slices-only-for-non-zero-values-but-shows-all-legends.mmd",
    "pie/pr-check/should-render-a-pie-diagram-with-a-long-title-without-clipping.mmd",
    "pie/pr-check/should-render-a-pie-diagram-with-showdata.mmd",
    "pie/pr-check/should-render-a-simple-pie-diagram-with-capital-letters-for-labels.mmd",
    "pie/pr-check/should-render-a-simple-pie-diagram.mmd",
    "pie/pr-check/simple-sports.mmd",
}

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

COHORT_DIGEST = "3caa028a13c1ee9d7abab6a743266d3f8784b1854cac6c3cb8a847d274483205"

PHASE_REVIEWS = {
    PHASE: {
        "reviewer_agent": (
            "independent read-only adversarial code review (fresh general-purpose "
            "agent, RFC 0042 pie theme admission, 2026-11-22; APPROVE with zero "
            "MAJOR; 2 MINOR + 3 NIT resolved before the phase commit)"),
        "date": "2026-11-22",
        "verdict": "approved",
        "note": (
            "Verified against the live gate: the PieTheme profile matches the RFC "
            "shape exactly, PieConfig/PieDescription are byte-unchanged, the footer "
            "budget is byte-identical to the six sibling paths, the corpus/synthetic "
            "twin gate recomputes sha and body equivalence, and the 0/5 narrow state "
            "is live-asserted. Items fixed: the RFC placement sentence now says the "
            "branch sits after the Pie7 block (it cannot shadow the special branch), "
            "the generator strips CR before classifying, the emission block re-checks "
            "the 96-byte cap for sibling parity, and the in-table no-description "
            "negative pins the body-family guard."),
    },
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def split_frontmatter(text: str):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return [line.removesuffix("\r") for line in lines[1:close]], "\n".join(
                lines[close + 1:])
    return None, text


def theme_of(frontmatter):
    """Return the admitted theme value or None, mirroring the C++ profile.

    `description: >-` literal, payload lines indented exactly two spaces with a
    non-space/non-tab/non-# third character, then exactly one `config:` line
    and exactly one literal `  theme: <value>` leaf.
    """
    if frontmatter is None or not frontmatter or frontmatter[0] != "description: >-":
        return None
    section = "description"
    have_payload = False
    theme = None
    for raw in frontmatter[1:]:
        if section == "description":
            if raw == "config:":
                section = "config"
                continue
            if len(raw) <= 2 or raw[0] != " " or raw[1] != " " or raw[2] in " \t#":
                return None
            have_payload = True
        else:
            if theme is not None or not raw.startswith("  theme: "):
                return None
            value = raw[len("  theme: "):]
            if not value or any(c in " \t" for c in value):
                return None
            if value[0] in "'\"" or any(c in "{}[]&*!|>#'\"\\" for c in value):
                return None
            if value not in THEME_TABLE:
                return None
            theme = value
    if not have_payload or theme is None:
        return None
    return theme


def body_of(text: str) -> str:
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[close + 1:])
    return text


def body_is_projectable(body: str) -> bool:
    for line in body.split("\n"):
        stripped = line.strip()
        if stripped.startswith("accTitle:") or stripped.startswith("accDescr:"):
            return False
    return True


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n" for entry in entries)
    return sha256_bytes(payload.encode())


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    originals = sorted(path.relative_to(cases).as_posix()
                       for path in (cases / "pie").rglob("*.mmd"))
    if len(originals) != EXPECTED["originals"]:
        raise ValueError(f"pie population drifted: {len(originals)}")

    admitted = {}
    for rel in originals:
        text = (cases / rel).read_text()
        frontmatter, body = split_frontmatter(text)
        theme = theme_of(frontmatter)
        if theme is not None and body_is_projectable(body):
            admitted[rel] = theme
    if len(admitted) != EXPECTED["admitted"]:
        raise ValueError(f"pie admitted set drifted: {len(admitted)}")

    # Corpus twin: an admitted body identical to a drawn Pie7 fixture's body,
    # after frontmatter removal.
    if not PIE7 <= set(originals):
        raise ValueError("Pie7 pin references a missing fixture")
    if len(PIE7) != EXPECTED["drawn"]:
        raise ValueError("Pie7 pin size drifted")
    admitted_bodies = {rel: body_of((cases / rel).read_text()) for rel in admitted}
    # Prefer the sports fixture when several drawn rows share a body (the RFC's
    # pinned corpus twin).
    drawn_by_body = {}
    for rel in sorted(PIE7, key=lambda r: (r != "pie/pr-check/simple-sports.mmd", r)):
        drawn_by_body.setdefault(
            sha256_bytes(body_of((cases / rel).read_text()).encode()), rel)

    rows = []
    corpus_twins = 0
    for rel in sorted(admitted):
        body = admitted_bodies[rel]
        twin_rel = drawn_by_body.get(sha256_bytes(body.encode()))
        if twin_rel:
            twin = {"kind": "corpus", "path": twin_rel,
                    "sha256": sha256_file(cases / twin_rel)}
            corpus_twins += 1
        else:
            twin = {"kind": "synthetic"}
        rows.append({
            "path": rel,
            "family": "pie",
            "phase": PHASE,
            "sha256": sha256_file(cases / rel),
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "twin": twin,
            "expected_prepend_lines": [],
            "expected_footer_lines": [
                "", NOTICE_HEADER,
                f"config.theme={admitted[rel]} -> colour palette; not applied"],
            "prediction": "predicted-draw",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn" if rel in NARROW_DRAW else "source",
                "restored_240": "drawn",
            },
            "ledger_note": (
                "RFC 0042 pie theme admission; validated and disclosed only, the "
                "frame equals its twin's plus the bounded footer."),
        })
    if corpus_twins != 1:
        raise ValueError(f"pie corpus-twin count drifted: {corpus_twins}")

    if len(rows) + len(PIE7) != EXPECTED["originals"] - EXPECTED["other"]:
        raise ValueError("pie partition does not reconcile")
    digest = cohort_digest(rows)
    if COHORT_DIGEST not in ("PENDING", digest):
        raise ValueError(f"RFC 0042 cohort row set drifted: digest {digest} != {COHORT_DIGEST}")

    return {
        "schema": SCHEMA,
        "rfc": 42,
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": "the reviewed folded description followed by exactly one `config:` "
                    "block whose sole leaf is an indent-2 in-table `theme`, with a body "
                    "carrying no accTitle:/accDescr: statement",
            "roots": ["pie"],
            "phase": PHASE,
            "cohort_digest": digest,
            "family_total": EXPECTED["originals"],
            "native_draw_count": EXPECTED["drawn"],
            "admitted_count": EXPECTED["admitted"],
            "other_count": EXPECTED["other"],
            "twin_rule": "the frame must equal the twin's frame (corpus twin when one "
                         "exists, else the config-free synthetic twin) plus the recorded "
                         "footer delta, line by line, at wide width only",
            "corpus_twins": "one row (the `dark` sports pie) is body-identical to "
                            "pie/pr-check/simple-sports.mmd; the four twelve-slice redux "
                            "rows have no drawn body-equivalent",
            "exclusions": "the 8 pie.* semantic rows and the 4 accTitle:/accDescr: "
                          "body-blocked theme rows stay whole-source",
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
            "narrow_40_draw": sum(1 for row in rows
                                  if row["expected_states"]["narrow_40"] == "drawn"),
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
