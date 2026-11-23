#!/usr/bin/env python3
"""Generate the D-447 usecase accessibility + closed-namespace cohort ledger.

Renderer-free. The single row is the last usecase fallback whose only blockers
are the closed `config.usecase` tuning namespace and the in-body
`accTitle:`/block-form `accDescr { ... }` accessibility metadata. The usecase
projection is a statement outline, so the tuning leaves are inert (disclosed),
and the accessibility declarations are validated no-ops like the state family's.

Twin rule (pinned): the twin is the same fixture with the whole frontmatter
config block and the accTitle/accDescr declarations removed; its outline must
equal the source's outline above the disclosure footer.

Usage: python3 tools/mermaid/generate_usecase_accessibility_d447_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/usecase-accessibility-d447-cohort.json")
SCHEMA = "loom_mermaid_usecase_accessibility_d447_cohort/v1"
DECISION = "D-447"

HEADER = "Usecase native projection v1"
NOTICE_LIMIT = 96
LEAVES = (
    ("actorFontSize", "16"),
    ("actorFontFamily", "Arial"),
    ("actorFontWeight", "bold"),
    ("usecaseFontSize", "14"),
    ("usecaseFontFamily", "Georgia"),
    ("nodeSpacing", "60"),
    ("rankSpacing", "70"),
    ("diagramPadding", "24"),
    ("useMaxWidth", "false"),
)

COHORT = [
    "usecase/nightly-check/10-configuration-and-accessibility.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review (fresh general-purpose "
        "agent, D-447 usecase accessibility + namespace admission, 2026-11-23)"),
    "date": "2026-11-23",
    "verdict": "approved",
    "note": (
        "Reviewer re-ran the generator --check entry and the live gate and "
        "verified: the namespace leaf table is exact per value; a namespace "
        "without a leaf fails closed; accTitle/accDescr are scalar or block "
        "validated no-ops with duplicate rejection; the disclosure footer is "
        "appended after the outline; the twin oracle removes config and the acc "
        "declarations and the outline matches."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def split_frontmatter(text: str):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None


def footer_lines() -> list:
    lines = [HEADER]
    for key, value in LEAVES:
        lines.append(f"config.usecase.{key}={value} -> SVG layout tuning; not applied")
    return lines


def acc_free_twin(text: str) -> str:
    """Same fixture minus the config block and the acc declarations."""
    parts = split_frontmatter(text)
    frontmatter, body = parts if parts else ("", text)
    kept = []
    skipping = False
    for line in frontmatter.split("\n"):
        if skipping and line.startswith(" "):
            continue
        skipping = line.startswith("config:")
        if skipping:
            continue
        kept.append(line)
    out_body = []
    in_descr = False
    for line in body.split("\n"):
        stripped = line.strip()
        if in_descr:
            if stripped == "}":
                in_descr = False
            continue
        if stripped.startswith("accTitle"):
            continue
        if stripped.startswith("accDescr"):
            if stripped == "accDescr {":
                in_descr = True
            continue
        out_body.append(line)
    prefix = ("---\n" + "\n".join(kept) + "\n---\n") if parts else ""
    return prefix + "\n".join(out_body)


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                      for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    for rel in COHORT:
        source = (repo_root / CASES / rel).read_text()
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        if "accDescr {" not in source or "accTitle:" not in source:
            raise ValueError(f"{rel} does not carry the admitted accessibility shape")
        for key, value in LEAVES:
            if f"{key}: {value}" not in source:
                raise ValueError(f"{rel} is missing the admitted leaf {key}")
        rows.append({
            "path": rel,
            "family": "usecase",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": "source_fallback",
            "prediction": "predicted-draw",
            "track_scope": "whole-only",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn" if rel in NARROW_DRAW else "source_fallback",
                "restored_240": "drawn",
            },
            "expected_footer_lines": [""] + footer_lines(),
            "twin": {
                "kind": "config-acc-free",
                "path": rel,
                "sha256": observed_sha,
                "footer_lines": [],
            },
            "ledger_note": (
                "closed config.usecase tuning leaves are inert for the outline "
                "projection and disclosed; accTitle/accDescr are validated no-ops"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": "description envelope + `config.usecase` closed tuning "
                     "namespace + scalar/block accessibility declarations",
            "leaf_table": {key: value for key, value in LEAVES},
            "footer": {"header": HEADER, "line_limit_bytes": NOTICE_LIMIT},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin removes the config block and the acc "
                          "declarations; the outline above the footer must be "
                          "cell-equal"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("the last usecase fallback whose only blockers are the "
                     "closed tuning namespace and the accessibility no-ops"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows), "by_family": {"usecase": len(rows)}},
        "review": REVIEW,
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
