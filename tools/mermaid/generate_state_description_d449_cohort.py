#!/usr/bin/env python3
"""Generate the D-449 state `state X: Description` admission cohort ledger.

Renderer-free. The four rows are the state originals whose only remaining
blocker was the upstream declaration spelling `state X: Description`, which
attaches a description fragment to a state already introduced by a transition.
Their config leaves (look: neo, theme: X) were admitted by RFC 0038.

Twin rule: the config-free twin is the same fixture with the whole config block
removed; the art above the footer must be cell-equal.

Usage: python3 tools/mermaid/generate_state_description_d449_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/state-description-d449-cohort.json")
SCHEMA = "loom_mermaid_state_description_d449_cohort/v1"
DECISION = "D-449"

HEADER = "State native projection v1"
NOTICE_LIMIT = 96
COHORT_DIR = "state-diagram-v2/nightly-check"
COHORT = [
    "states-with-descriptions-neo-look-neo-dark-theme.mmd",
    "states-with-descriptions-neo-look-neo-theme.mmd",
    "states-with-descriptions-neo-look-redux-dark-theme.mmd",
    "states-with-descriptions-neo-look-redux-theme.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review (fresh general-purpose "
        "agent, D-449 state description declarations, 2026-11-23)"),
    "date": "2026-11-23",
    "verdict": "approved",
    "note": (
        "Reviewer re-ran the generator --check entry and the live gate and "
        "verified: the colon form is consumed before the duplicate/stray-token "
        "checks; it carries no alias, brace or marker; the description fragment "
        "goes through the same state_description_label validation; the twin "
        "oracle is cell-exact and narrow 40 stays source."),
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


def strip_frontmatter(text: str) -> str:
    parts = split_frontmatter(text)
    return parts[1] if parts else text


def config_free_twin(text: str) -> str:
    parts = split_frontmatter(text)
    if parts is None:
        return text
    frontmatter, body = parts
    kept = []
    skipping = False
    for line in frontmatter.split("\n"):
        if skipping and line.startswith(" "):
            continue
        skipping = line.startswith("config:")
        if skipping:
            continue
        kept.append(line)
    return "---\n" + "\n".join(kept) + "\n---\n" + body


def footer_lines(theme: str) -> list:
    return [HEADER, "config.look=neo -> source node style; not applied",
            f"config.theme={theme} -> colour palette; not applied"]


def theme_of(frontmatter: str):
    for line in frontmatter.split("\n"):
        stripped = line.strip()
        if stripped.startswith("theme:"):
            return stripped.split(":", 1)[1].strip()
    return None


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                      for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    for name in COHORT:
        rel = f"{COHORT_DIR}/{name}"
        source = (repo_root / CASES / rel).read_text()
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        parts = split_frontmatter(source)
        theme = theme_of(parts[0]) if parts else None
        if theme is None or "state State1: Description 1" not in source:
            raise ValueError(f"{rel} does not carry the admitted shapes")
        rows.append({
            "path": rel,
            "family": "state-diagram-v2",
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
            "expected_footer_lines": footer_lines(theme),
            "twin": {
                "kind": "config-free",
                "path": rel,
                "sha256": observed_sha,
                "footer_lines": [],
            },
            "ledger_note": (
                "`state X: Description` declares a description fragment for an "
                "already-introduced state; the leaf disclosures are RFC 0038's"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": "`state <id>: <description>` declaration form",
            "footer": {"header": HEADER, "line_limit_bytes": NOTICE_LIMIT},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the config-free twin is the same fixture with the whole "
                          "config block removed; the art above the footer must be "
                          "cell-equal"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("state originals whose only remaining blocker is the "
                     "`state X: Description` declaration form"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows), "by_family": {"state-diagram-v2": len(rows)}},
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
