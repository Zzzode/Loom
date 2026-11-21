#!/usr/bin/env python3
"""Generate the RFC 0041 agentflow look/theme admission cohort ledger.

Renderer-free. The nine admitted rows are source-classifiable (a `config:` block
whose sole leaf is an indent-2 in-table `theme`) and every one of them is
body-identical to an already-drawn fixture, so this generator also verifies the
corpus twin mapping from the fixture bytes and pins each row's twin path and
sha. The admitted set is pinned by a digest that must equal the family ledger's
`admitted_digest`.

Per-row `expected_states.narrow_40` is NOT source-derivable: like the other
cohort generators it comes from a reviewed, measured set. The P1 measurement is
0 of 9 drawing at 40 columns: the disclosure footer's longest line exceeds a
40-column viewport, so the container-kind rows (whose footerless corpus twin
draws at 40) fall back once admitted, and the triage twin is already too wide.
The live gate is the authority. The roster and the matrix artifact are never
read; review records are authored here and written into the ledger, never read
back.

Usage: python3 tools/mermaid/generate_agentflow_0041_cohort.py [--check]
"""

import argparse
import hashlib
import importlib.util
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/agentflow-0041-cohort.json")
FAMILY_LEDGER = pathlib.Path("tools/mermaid/agentflow-cohort.json")
SCHEMA = "loom_mermaid_agentflow_0041_cohort/v1"
PHASE = "P1-theme-admission"
ENABLED_PHASES = (PHASE,)

EXPECTED = {"originals": 38, "drawn": 27, "admitted": 9, "gated": 2}
EXPECTED_TWINS = {
    "agentflow/visual-check/10-every-container-kind.mmd": 5,
    "agentflow/visual-check/15-support-triage.mmd": 4,
}

# Measured 40-column draws among the admitted rows: none. The footer's longest
# line exceeds a 40-column viewport, so every admitted row keeps the whole
# source there (the footerless corpus twin draws).
NARROW_DRAW = set()

NOTICE_HEADER = "Agentflow native projection v1"
THEME_TABLE = ("base", "dark", "default", "forest", "neutral", "redux-color",
               "redux-dark-color")

COHORT_DIGEST = "5874b783d99a18a8c7c14af4cf9371bf0646d018304546b40688d6fc3f267dfb"

PHASE_REVIEWS = {
    PHASE: {
        "reviewer_agent": (
            "independent read-only adversarial code review (fresh general-purpose "
            "agent, RFC 0041 agentflow theme admission, 2026-11-22; APPROVE with "
            "zero MAJOR; 3 MINOR + 2 NIT all resolved before the phase commit)"),
        "date": "2026-11-22",
        "verdict": "approved",
        "note": (
            "Verified against the live gate: the config-block-scoped grammar is a "
            "strict widening for exactly one shape, the closed table is exact in C++ "
            "and Python, the footer budget is byte-identical to the five sibling "
            "paths, the corpus-twin gate recomputes body equivalence and sha, and the "
            "narrow_40 assertion is live (0 of 9 draw at 40 columns, so the ledger is "
            "measured not transcribed). MINORs fixed: the generator docstring now "
            "states the measured 0/9 disposition, the RFC qualifies which rows the "
            "footer flips (5 container-kind; the triage twin is already too wide), and "
            "the family test header no longer claims 11 gated rows."),
    },
}


def _family_generator():
    spec = importlib.util.spec_from_file_location(
        "generate_agentflow_cohort",
        pathlib.Path(__file__).resolve().parent / "generate_agentflow_cohort.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


FAMILY = _family_generator()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def body_of(text: str) -> str:
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[close + 1:])
    return text


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n" for entry in entries)
    return sha256_bytes(payload.encode())


def notice_lines(theme: str):
    return [NOTICE_HEADER,
            f"config.theme={theme} -> colour palette; not applied"]


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    originals = sorted(path.relative_to(cases).as_posix()
                       for path in (cases / "agentflow").rglob("*.mmd"))
    if len(originals) != EXPECTED["originals"]:
        raise ValueError(f"agentflow population drifted: {len(originals)}")

    drawn, admitted, gated = set(), set(), set()
    leaves = {}
    for rel in originals:
        text = (cases / rel).read_text()
        frontmatter, malformed = FAMILY.split_frontmatter(text)
        kind, found = FAMILY.classify(frontmatter, malformed)
        {"drawn": drawn, "admitted": admitted, "gated": gated}[kind].add(rel)
        if kind == "admitted":
            leaves[rel] = found
    if (len(drawn), len(admitted), len(gated)) != (
            EXPECTED["drawn"], EXPECTED["admitted"], EXPECTED["gated"]):
        raise ValueError(
            f"agentflow partition drifted: drawn={len(drawn)} admitted={len(admitted)} "
            f"gated={len(gated)}")

    # Corpus twin mapping: every admitted body equals a drawn fixture's body.
    drawn_by_body = {}
    for rel in sorted(drawn):
        drawn_by_body.setdefault(
            sha256_bytes(body_of((cases / rel).read_text()).encode()), rel)
    twin_count = {}
    twins = {}
    for rel in sorted(admitted):
        twin = drawn_by_body.get(sha256_bytes(body_of((cases / rel).read_text()).encode()))
        if twin is None:
            raise ValueError(f"{rel}: no body-equivalent drawn corpus twin")
        twins[rel] = twin
        twin_count[twin] = twin_count.get(twin, 0) + 1
    if twin_count != EXPECTED_TWINS:
        raise ValueError(f"corpus twin mapping drifted: {twin_count}")

    family = json.loads((repo_root / FAMILY_LEDGER).read_text())
    if family["selection"]["admitted_digest"] != sha256_bytes("".join(
            f"{rel}\t{sha256_file(cases / rel)}\n" for rel in sorted(admitted)).encode()):
        raise ValueError("family ledger admitted_digest does not match this cohort")
    if not NARROW_DRAW <= admitted:
        raise ValueError("narrow-draw set references a non-admitted row")

    rows = []
    for rel in sorted(admitted):
        theme = leaves[rel]["theme"]
        if theme not in THEME_TABLE:
            raise ValueError(f"{rel}: theme outside the closed table")
        rows.append({
            "path": rel,
            "family": "agentflow",
            "phase": PHASE,
            "sha256": sha256_file(cases / rel),
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "twin": {"kind": "corpus",
                     "path": twins[rel],
                     "sha256": sha256_file(cases / twins[rel])},
            "expected_prepend_lines": [],
            # Blank separator, header, then the theme line.
            "expected_footer_lines": [""] + notice_lines(theme),
            "prediction": "predicted-draw",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn" if rel in NARROW_DRAW else "source",
                "restored_240": "drawn",
            },
            "ledger_note": (
                "RFC 0041 agentflow theme admission; validated and disclosed only, "
                "the frame equals its corpus twin's plus the bounded footer."),
        })

    if len(rows) != EXPECTED["admitted"]:
        raise ValueError(f"cohort total drifted: {len(rows)}")
    digest = cohort_digest(rows)
    if COHORT_DIGEST not in ("PENDING", digest):
        raise ValueError(f"RFC 0041 cohort row set drifted: digest {digest} != {COHORT_DIGEST}")

    return {
        "schema": SCHEMA,
        "rfc": 41,
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": "a column-0 `config:` block whose sole leaf is an indent-2 `theme` "
                    "with a value inside the family's closed corpus table",
            "roots": ["agentflow"],
            "phase": PHASE,
            "cohort_digest": digest,
            "family_total": EXPECTED["originals"],
            "native_draw_count": EXPECTED["drawn"],
            "admitted_count": EXPECTED["admitted"],
            "gated_count": EXPECTED["gated"],
            "admitted_digest": family["selection"]["admitted_digest"],
            "twin_rule": "the frame must equal the body-identical corpus twin's frame plus "
                         "the recorded footer delta, line by line, both sides through the "
                         "same sentinel-bearing wrapper",
            "corpus_twins": "5 rows share agentflow/visual-check/10-every-container-kind.mmd "
                            "and 4 share agentflow/visual-check/15-support-triage.mmd; all "
                            "nine bodies are byte-identical after frontmatter removal",
            "exclusions": "the 2 rows carrying look + handDrawnSeed stay whole-source (a "
                          "seed is a geometry input, not an inert disclosure)",
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
