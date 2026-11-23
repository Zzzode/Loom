#!/usr/bin/env python3
"""Generate the D-451 c4 closed styling/accessibility admission cohort ledger.

Renderer-free. The six rows are the c4 fallbacks whose only blockers are the
closed `UpdateElementStyle(alias, $attr...)` / `UpdateRelStyle(from, to,
$attr...)` calls and scalar `accTitle:`/`accDescr:` no-ops. The native c4
projection is an outline that draws no geometry or colour, so every admitted
attribute is inert and disclosed line by line in source order; the
accessibility scalars are validated no-ops like the state and usecase families.

Twin rule (pinned): the twin is the same fixture with the admitted styling and
accessibility lines removed. The outline above the footer must be cell-equal.

Usage: python3 tools/mermaid/generate_c4_styling_d451_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/c4-styling-d451-cohort.json")
SCHEMA = "loom_mermaid_c4_styling_d451_cohort/v1"
DECISION = "D-451"

HEADER = "C4 native projection v1"
NOTICE_LIMIT = 96

COHORT = [
    "c4/nightly-check/c4-4-should-render-a-simple-c4dynamic-diagram.mmd",
    "c4/nightly-check/styling/update-relationship-style.mmd",
    "c4/nightly-check/update-element-shape-override.mmd",
    "c4/pr-check/c4-1-should-render-a-simple-c4context-diagram.mmd",
    "c4/pr-check/c4-3-should-render-a-simple-c4component-diagram.mmd",
    "c4/pr-check/update-element-style-colors.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review (fresh general-purpose "
        "agent, D-451 c4 styling/accessibility admission, 2026-11-23)"),
    "date": "2026-11-23",
    "verdict": "approved",
    "note": (
        "Reviewer re-ran the generator --check entry and the live gate and "
        "verified: only closed $name=value attributes are admitted after the "
        "exact identifier count; duplicate attributes fail closed; the "
        "accessibility scalars are duplicate-gated no-ops; the disclosure "
        "footer appends after the D-445 line; the styling-free twin oracle is "
        "cell-exact above the footer."),
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


def body_of(text: str) -> str:
    parts = split_frontmatter(text)
    return parts[1] if parts else text


def split_args(text: str):
    """Split a call's argument list on top-level commas (quotes tracked)."""
    parts, quote, start = [], "", 0
    for at, c in enumerate(text):
        if quote:
            if c == quote:
                quote = ""
            continue
        if c in "\"'":
            quote = c
            continue
        if c == "(" or c == ")":
            return None
        if c == ",":
            parts.append(text[start:at])
            start = at + 1
    if quote:
        return None
    parts.append(text[start:])
    return [part.strip() for part in parts]


def is_identifier(token: str) -> bool:
    return (bool(token) and (token[0].isalpha() or token[0] == "_") and
            all(c.isalnum() or c == "_" for c in token))


def attribute_name(token: str):
    """`$name=value` -> `$name`, or None."""
    if not token.startswith("$") or "=" not in token:
        return None
    name, value = token.split("=", 1)
    ident = name[1:]
    if not ident or not (ident[0].isalpha() or ident[0] == "_"):
        return None
    if not all(c.isalnum() or c == "_" for c in ident):
        return None
    if value is None or value == "":
        return None
    return name


def styling_disclosures(text: str):
    """Disclosure lines for the admitted styling/accessibility statements."""
    disclosures = []
    saw_title = saw_descr = False
    for raw in body_of(text).split("\n"):
        line = raw.strip()
        if line.startswith("accTitle") or line.startswith("accDescr"):
            is_title = line.startswith("accTitle")
            rest = line[8:].lstrip()
            if not rest.startswith(":") or len(rest) < 2:
                return None
            value = rest[1:].strip()
            if not value:
                return None
            if (is_title and saw_title) or (not is_title and saw_descr):
                return None
            if is_title:
                saw_title = True
            else:
                saw_descr = True
            continue
        for keyword, id_count in (("UpdateElementStyle", 1), ("UpdateRelStyle", 2)):
            if not line.startswith(keyword + "("):
                continue
            if not line.endswith(")"):
                return None
            args = split_args(line[len(keyword) + 1:-1])
            if args is None or len(args) <= id_count:
                return None
            ids = args[:id_count]
            if not all(is_identifier(token) for token in ids):
                return None
            target = f"{keyword}({', '.join(ids)})."
            seen = set()
            for token in args[id_count:]:
                name = attribute_name(token)
                if name is None or name in seen:
                    return None
                seen.add(name)
                disclosures.append(f"{target}{name} -> SVG styling; not applied")
            continue
    return disclosures


def styling_free_twin(text: str) -> str:
    parts = split_frontmatter(text)
    body = parts[1] if parts else text
    kept = [line for line in body.split("\n")
            if not line.strip().startswith(("UpdateElementStyle", "UpdateRelStyle",
                                           "accTitle", "accDescr"))]
    return (("---\n" + parts[0] + "\n---\n") if parts else "") + "\n".join(kept)


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
        notices = styling_disclosures(source)
        if not notices:
            raise ValueError(f"{rel} does not carry the admitted statement shape")
        rows.append({
            "path": rel,
            "family": "c4",
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
            "expected_footer_lines": [HEADER] + notices,
            "twin": {
                "kind": "styling-free-body",
                "path": rel,
                "sha256": observed_sha,
                "footer_lines": [],
            },
            "ledger_note": (
                "the c4 projection is an outline with no colour or geometry, so "
                "the closed styling attributes are inert and disclosed one line "
                "per attribute; the accessibility scalars are validated no-ops"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": ("`UpdateElementStyle(alias, $attr...)`, "
                      "`UpdateRelStyle(from, to, $attr...)` with at least one "
                      "closed `$name=value` attribute, and scalar "
                      "`accTitle:`/`accDescr:` no-ops"),
            "footer": {"header": HEADER, "line_limit_bytes": NOTICE_LIMIT},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin is the same fixture with the styling and "
                          "accessibility lines removed; the frame oracle "
                          "compares the outline above the footer"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("c4 fallbacks whose only blockers are the closed styling "
                     "calls and scalar accessibility declarations"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows), "by_family": {"c4": len(rows)}},
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
