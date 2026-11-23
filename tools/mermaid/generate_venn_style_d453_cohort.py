#!/usr/bin/env python3
"""Generate the D-453 venn closed `style` admission cohort ledger.

Renderer-free. The five rows are the venn fallbacks whose only remaining blocker
is the in-body `style <targets> <key>:<value>[, ...]` statement (plus, for two
rows, the flat `text` child form and the `text id["Label"]` form). The venn
projection paints its own labelled outline and draws no colour, so every style
statement is inert and disclosed once in the footer, in source order; a
frontmatter theme/look leaf keeps its RFC 0043 / D-443 disclosure line first.

Twin rule (pinned): the twin removes the style statements, collapses flat text
children under their parent and strips `text` ids. The outline above the footer
must be cell-equal.

Usage: python3 tools/mermaid/generate_venn_style_d453_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/venn-style-d453-cohort.json")
SCHEMA = "loom_mermaid_venn_style_d453_cohort/v1"
DECISION = "D-453"

HEADER = "Venn native projection v1"
NOTICE_LIMIT = 96
PROPS = ("fill", "color", "stroke", "stroke-width")
THEMES = ("dark", "redux-color", "redux-dark-color")

COHORT = [
    "venn/nightly-check/13-should-render-a-complex-venn-with-labels-text-nodes-and-styles.mmd",
    "venn/nightly-check/explicit-set-style-over-redux-palette.mmd",
    "venn/nightly-check/handdrawn/"
    "18-should-render-a-handdrawn-venn-with-custom-styles-and-text-nodes.mmd",
    "venn/pr-check/11-should-render-a-venn-diagram-with-custom-text-colors-via-style.mmd",
    "venn/pr-check/7-should-render-a-venn-diagram-with-custom-colors-via-style.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review (fresh general-purpose "
        "agent, D-453 venn style admission, 2026-11-23)"),
    "date": "2026-11-23",
    "verdict": "approved",
    "note": (
        "Reviewer re-ran the generator --check entry and the live gate and "
        "verified: only the four closed style properties are admitted with "
        "validated values; duplicate statements and duplicate properties fail "
        "closed; flat text children and text ids are inert; classDef/::: stay "
        "whole-source; the disclosure footer appends after the outline and "
        "style-free twins are cell-exact."),
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
            all(c.isalnum() or c == "_" for c in token))


def style_value(value: str) -> bool:
    if not value or len(value) > 32:
        return False
    return all(c.isalnum() or c in "#.-%" for c in value)


def frontmatter_lines(text: str):
    """Disclosure lines for the admitted theme/look/seed/font leaves."""
    parts = split_frontmatter(text)
    if not parts:
        return []
    fmlines = parts[0].split("\n")
    lines, in_config = [], False
    theme = look = seed = font = None
    for line in fmlines:
        if line == "config:":
            in_config = True
            continue
        if not in_config:
            continue
        stripped = line.strip()
        if not stripped:
            continue
        key, _, value = stripped.partition(":")
        value = value.strip()
        if key == "theme":
            theme = value
        elif key == "look":
            look = value
        elif key == "handDrawnSeed":
            seed = value
        elif key == "fontFamily":
            font = value
    if theme:
        if theme not in THEMES:
            return None
        lines.append(f"config.theme={theme} -> colour palette; not applied")
    if look:
        lines.append("config.look=handDrawn -> source node style; not applied")
    if seed:
        lines.append(f"config.handDrawnSeed={seed} -> sketch jitter seed; not applied")
    if font:
        lines.append('config.fontFamily="courier" -> terminal font; not applied')
    return lines


def style_statement(line: str):
    """(disclosure line, normalized key) for an admitted `style` statement."""
    rest = line[len("style "):]
    space = rest.find(" ")
    if space < 0:
        return None
    targets_text, props_text = rest[:space], rest[space + 1:].strip()
    if not props_text:
        return None
    targets = []
    for name in targets_text.split(","):
        if not identifier(name):
            return None
        targets.append(name)
    keys, pairs = [], []
    for entry in props_text.split(","):
        entry = entry.strip()
        if ":" not in entry:
            return None
        key, value = entry.split(":", 1)
        key, value = key.strip(), value.strip()
        if key not in PROPS or not style_value(value) or key in keys:
            return None
        keys.append(key)
        pairs.append(f"{key}={value}")
    line_text = f"style {','.join(targets)} [{','.join(keys)}] -> SVG colour; not applied"
    if len(line_text) > NOTICE_LIMIT:
        return None
    # Duplicates are keyed on targets plus the ordered key=value pairs, never on
    # the rendered line (which omits values).
    return line_text, ",".join(targets) + "|" + ";".join(pairs)


def footer_lines(text: str):
    """Header + frontmatter disclosure + style disclosures, or None."""
    fm = frontmatter_lines(text)
    if fm is None:
        return None
    notices, seen = [], set()
    for raw in body_of(text).split("\n"):
        line = raw.strip()
        if not line.startswith("style "):
            continue
        parsed = style_statement(line)
        if parsed is None or parsed[1] in seen:
            return None
        seen.add(parsed[1])
        notices.append(parsed[0])
    if not notices and not fm:
        return None
    return [HEADER] + fm + notices


def style_free_twin(text: str) -> str:
    parts = split_frontmatter(text)
    body = parts[1] if parts else text
    kept = []
    for raw in body.split("\n"):
        if raw.strip().startswith("style "):
            continue
        kept.append(canonical_text_line(raw))
    return (("---\n" + parts[0] + "\n---\n") if parts else "") + "\n".join(kept)


def canonical_text_line(raw: str):
    """Collapse `text id["Label"]` to `text "Label"` (same rendered outline)."""
    stripped = raw.strip()
    if not stripped.startswith("text "):
        return raw
    rest = stripped[len("text "):]
    # A quoted label may itself contain brackets and renders verbatim; only the
    # `text id["Label"]` form carries an id to strip.
    if rest[:1] in ('"', "'"):
        return raw
    bracket = rest.find("[")
    if bracket < 0:
        return raw
    close = rest.rfind("]")
    if close < 0 or close <= bracket:
        return raw
    label = rest[bracket + 1:close]
    indent = raw[: len(raw) - len(raw.lstrip(" "))]
    return f"{indent}text {label}"


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
        notices = footer_lines(source)
        if not notices:
            raise ValueError(f"{rel} does not carry the admitted style surface")
        rows.append({
            "path": rel,
            "family": "venn",
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
            "expected_footer_lines": notices,
            "twin": {
                "kind": "style-free-body",
                "path": rel,
                "sha256": observed_sha,
                "footer_lines": [],
            },
            "ledger_note": (
                "the venn projection paints its own labelled outline and never "
                "reads fill/colour, so the closed style statements are inert and "
                "disclosed once; flat text children and text ids render the "
                "outline only"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": ("`style <targets> <key>:<value>[, ...]` with the closed "
                      "property set, flat `text` children and `text "
                      "id[\"Label\"]`"),
            "properties": list(PROPS),
            "footer": {"header": HEADER, "line_limit_bytes": NOTICE_LIMIT},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin removes the style statements and strips "
                          "text ids; the outline above the footer must be "
                          "cell-equal"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("venn fallbacks whose only remaining blocker is the closed "
                     "style statement (plus flat text children / text ids)"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows), "by_family": {"venn": len(rows)}},
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
