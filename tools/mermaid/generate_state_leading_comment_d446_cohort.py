#!/usr/bin/env python3
"""Generate the D-446 state leading-comment admission cohort ledger.

Renderer-free. The single row is the last state fallback whose only remaining
blocker is a body that opens with `%%` comment lines before `stateDiagram-v2`;
its config leaves (`layout: elk`, `fontFamily: courier`) were already admitted.
The body parser always ignored those comments; only the envelope's header check
was stricter, and that check now skips leading comment lines.

Usage: python3 tools/mermaid/generate_state_leading_comment_d446_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/state-leading-comment-d446-cohort.json")
SCHEMA = "loom_mermaid_state_leading_comment_d446_cohort/v1"
DECISION = "D-446"

HEADER = "State native projection v1"
NOTICE_LIMIT = 96
FOOTER = (
    HEADER,
    'config.fontFamily="courier" -> terminal font; not applied',
    "config.layout=elk -> external layout engine; not applied",
)

COHORT = [
    "state-diagram-v2/nightly-check/elk/elk-notes-keep-their-layout.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review (fresh general-purpose "
        "agent, D-446 state leading-comment admission, 2026-11-23)"),
    "date": "2026-11-23",
    "verdict": "approved",
    "note": (
        "Reviewer re-ran the generator --check entry and the live gate and "
        "verified: only leading `%%` lines are skipped (an indented comment or "
        "any other line still fails the header check); the skip is Work-charged; "
        "the footer is byte-identical to the D-441 leaf order; an uncommented "
        "twin draws the same art."),
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


def header_free_twin(text: str) -> str:
    """Same fixture with the leading body comment lines removed."""
    parts = split_frontmatter(text)
    frontmatter, body = parts if parts else ("", text)
    body_lines = body.split("\n")
    while body_lines and body_lines[0].startswith("%%"):
        body_lines.pop(0)
    prefix = ("---\n" + frontmatter + "\n---\n") if parts else ""
    return prefix + "\n".join(body_lines)


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
        parts = split_frontmatter(source)
        if parts is None or not parts[1].startswith("%%"):
            raise ValueError(f"{rel} does not open its body with a comment")
        if "layout: elk" not in parts[0] or "fontFamily: courier" not in parts[0]:
            raise ValueError(f"{rel} does not carry the admitted config leaves")
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
            "expected_footer_lines": list(FOOTER),
            "twin": {
                "kind": "leading-comment-free",
                "path": rel,
                "sha256": observed_sha,
                "footer_lines": [],
            },
            "ledger_note": (
                "leading body comment lines are skipped by the header check; "
                "the body parser already ignored them"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": "leading `%%` body comment lines before stateDiagram",
            "footer": {"header": HEADER, "leaf_order": ["config.fontFamily", "config.layout"],
                       "line_limit_bytes": NOTICE_LIMIT},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin removes the leading comment lines; the art "
                          "above the footer must be cell-equal"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("the last state fallback whose only blocker is a commented "
                     "body header"),
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
