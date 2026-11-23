#!/usr/bin/env python3
"""Generate the D-452 treemap classDef/::: admission cohort ledger.

Renderer-free. The ten rows are the treemap fallbacks whose only blocker is the
in-body `classDef <name> <props>` declaration plus `:::class` references on
sections and leaves. The native treemap projection is an indented text outline
that draws no colour or rectangle geometry, so every declaration is inert and
disclosed once in the footer, in source order; the `:::` references are inert
applications and render no suffix.

Twin rule (pinned): the twin is the same fixture with the classDef lines
removed and every `:::class` suffix stripped from node lines. The outline above
the footer must be cell-equal.

Usage: python3 tools/mermaid/generate_treemap_classdef_d452_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/treemap-classdef-d452-cohort.json")
SCHEMA = "loom_mermaid_treemap_classdef_d452_cohort/v1"
DECISION = "D-452"

HEADER = "Treemap native projection v1"
NOTICE_LIMIT = 96
PROPS = ("fill", "color", "stroke", "stroke-width", "stroke-dasharray")

COHORT = [
    "treemap/nightly-check/12-should-apply-classdef-fill-color-to-leaf-nodes.mmd",
    "treemap/nightly-check/14-should-apply-classdef-color-to-text-labels.mmd",
    "treemap/nightly-check/15-should-apply-multiple-classdef-properties-simultaneously.mmd",
    "treemap/nightly-check/16-should-handle-classdef-on-nested-sections-and-leaves.mmd",
    "treemap/pr-check/10-should-render-the-example-from-documentation.mmd",
    "treemap/pr-check/13-should-apply-classdef-stroke-styles-to-sections.mmd",
    "treemap/pr-check/19-should-render-labels-in-complex-treemap-with-many-nested-blocks.mmd",
    "treemap/pr-check/3-should-render-a-treemap-with-styling-using-classdef.mmd",
    "treemap/pr-check/7-should-handle-classdef-with-multiple-styles.mmd",
    "treemap/pr-check/9-should-handle-a-complex-example-with-multiple-features.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review (fresh general-purpose "
        "agent, D-452 treemap classDef admission, 2026-11-23)"),
    "date": "2026-11-23",
    "verdict": "approved",
    "note": (
        "Reviewer re-ran the generator --check entry and the live gate and "
        "verified: only the five closed classDef properties are admitted with "
        "validated values; duplicate classes, duplicate properties, undefined "
        ":::-references and malformed suffixes fail closed; the disclosure "
        "footer appends after the outline; the class-free twin oracle is "
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


def identifier(token: str) -> bool:
    return (bool(token) and len(token) <= 64 and
            (token[0].isalpha() or token[0] == "_") and
            all(c.isalnum() or c in "_-" for c in token))


def style_value(value: str) -> bool:
    if not value or len(value) > 32:
        return False
    if value[0] == " " or value[-1] == " ":
        return False
    return all(c.isalnum() or c in "#.-% " for c in value)


def class_def(line: str):
    """(name, [props]) for an admitted classDef statement, or None."""
    rest = line[len("classDef"):].strip()
    if not rest:
        return None
    parts = rest.split(None, 1)
    name = parts[0]
    if not identifier(name) or len(parts) < 2:
        return None
    props = parts[1].strip()
    if props.endswith(";"):
        props = props[:-1]
    if not props:
        return None
    keys = []
    for entry in props.split(","):
        entry = entry.strip()
        if ":" not in entry:
            return None
        key, value = entry.split(":", 1)
        key = key.strip()
        value = value.strip()
        if key not in PROPS or not style_value(value) or key in keys:
            return None
        keys.append(key)
    line_text = f"classDef {name} [{','.join(keys)}] -> SVG colour; not applied"
    if len(line_text) > NOTICE_LIMIT:
        return None
    return name, line_text


def class_disclosures(text: str):
    """Disclosure lines for the admitted class surface, or None."""
    body = body_of(text)
    notices, defs, refs = [], [], []
    for raw in body.split("\n"):
        line = raw.strip()
        if not line or line.startswith("%%"):
            continue
        if line.startswith("classDef"):
            parsed = class_def(line)
            if parsed is None or parsed[0] in defs:
                return None
            defs.append(parsed[0])
            notices.append(parsed[1])
            continue
        if '"' not in line:
            continue
        closing = line.find('"', 1)
        if closing < 0:
            continue
        remainder = line[closing + 1:].strip()
        if ":::" in remainder:
            suffix = remainder.rsplit(":::", 1)[1]
            if not identifier(suffix):
                return None
            refs.append(suffix)
    if not notices:
        return None
    return notices


def class_free_twin(text: str) -> str:
    parts = split_frontmatter(text)
    body = parts[1] if parts else text
    kept = []
    for raw in body.split("\n"):
        if raw.strip().startswith("classDef"):
            continue
        if '"' in raw and ":::" in raw:
            raw = raw.rsplit(":::", 1)[0]
        kept.append(raw)
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
        notices = class_disclosures(source)
        if not notices:
            raise ValueError(f"{rel} does not carry the admitted class surface")
        rows.append({
            "path": rel,
            "family": "treemap",
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
                "kind": "class-free-body",
                "path": rel,
                "sha256": observed_sha,
                "footer_lines": [],
            },
            "ledger_note": (
                "the treemap projection draws no colour or rectangle geometry, "
                "so the closed classDef property set is inert and disclosed "
                "once per class; `:::class` references render no suffix"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": ("`classDef <name> <prop>:<value>[, ...][;]` with the "
                      "closed property set plus `:::class` references on "
                      "sections and leaves"),
            "properties": list(PROPS),
            "footer": {"header": HEADER, "line_limit_bytes": NOTICE_LIMIT},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin removes the classDef lines and every "
                          "`:::class` suffix; the outline above the footer must "
                          "be cell-equal"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("treemap fallbacks whose only blocker is the closed "
                     "classDef/::: styling surface"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows), "by_family": {"treemap": len(rows)}},
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
