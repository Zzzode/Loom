#!/usr/bin/env python3
"""Generate the D-456 usecase themed-envelope cohort ledger.

Renderer-free. The nine rows are the themed usecase originals whose only
blockers were the top-level `config.theme` / `config.look` leaves, the
`config.usecase.useMaxWidth: true` / `colorScheme: rotate` leaves, the quoted
`systemBoundary "Title"` label and the `json <id>@{ ... }` block. The usecase
projection is a verbatim text outline that never reads a palette, so every
accepted leaf is inert and disclosed; the boundary title and the JSON block are
transcribed as authored.

Twin rule (pinned): the twin is the same fixture with the frontmatter removed.
The art above the footer must be cell-equal.

Usage: python3 tools/mermaid/generate_usecase_themed_d456_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/usecase-themed-d456-cohort.json")
SCHEMA = "loom_mermaid_usecase_themed_d456_cohort/v1"
DECISION = "D-456"

HEADER = "Usecase native projection v1"
THEMES = ("base", "dark", "default", "forest", "neutral", "redux", "redux-dark",
          "redux-color", "redux-dark-color")
# Canonical notice order: theme, look, then the user-space leaves in this order.
LEAF_ORDER = [
    ("actorFontSize", ("16",), "SVG layout tuning"),
    ("actorFontFamily", ("Arial",), "SVG layout tuning"),
    ("actorFontWeight", ("bold",), "SVG layout tuning"),
    ("usecaseFontSize", ("14",), "SVG layout tuning"),
    ("usecaseFontFamily", ("Georgia",), "SVG layout tuning"),
    ("nodeSpacing", ("60",), "SVG layout tuning"),
    ("rankSpacing", ("70",), "SVG layout tuning"),
    ("diagramPadding", ("24",), "SVG layout tuning"),
    ("useMaxWidth", ("false", "true"), "SVG layout tuning"),
    ("colorScheme", ("rotate",), "SVG layout tuning"),
]

COHORT = [
    "usecase/nightly-check/themed-elements-redux-color-theme.mmd",
    "usecase/nightly-check/themed-elements-redux-dark-color-theme.mmd",
    "usecase/pr-check/palette-rotation-redux-color-theme.mmd",
    "usecase/pr-check/palette-rotation-redux-dark-color-theme.mmd",
    "usecase/pr-check/themed-elements-base-theme.mmd",
    "usecase/pr-check/themed-elements-dark-theme.mmd",
    "usecase/pr-check/themed-elements-default-theme.mmd",
    "usecase/pr-check/themed-elements-forest-theme.mmd",
    "usecase/pr-check/themed-elements-neutral-theme.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review, fresh general-purpose "
        "agent, D-456 usecase themed envelope, 2026-11-24: round 1 "
        "REQUEST-CHANGES (mirror divergence, json prefix capture, look twin "
        "coverage, boundary keyword, CMake/ledger nits), round 2 APPROVE after "
        "fixes (task adc1bfd4d2bc4513d, resumed for the re-check)"),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": (
        "The roster regeneration and the d117 counts are deferred to the "
        "post-approval OfficialMatrix rerun; both landed in the same commit."),
    "decisions": {
        "D-458": (
            "the body grammar admits flat `@{...}` annotations, so these rows "
            "now drop them from the transcription and append one "
            "`body @{} properties` disclosure line; the admission itself is "
            "unchanged"),
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
            return lines[1:close], "\n".join(lines[close + 1:])
    return None, text


def frontmatter_lines(text: str):
    """The admitted disclosure lines in canonical order, or None when gated.

    Mirrors `usecase_frontmatter` in mermaid_usecase_impl.cpp: `description`
    (bare or ` >-` folded with an indented payload) is required, the only
    indent-2 keys are `usecase` / `look` / `theme`, indent-4 leaves require a
    preceding `usecase` namespace, and a namespace or config block that carries
    nothing still gates.
    """
    frontmatter, _ = split_frontmatter(text)
    if frontmatter is None:
        return None
    theme = look = None
    leaves = {}
    saw_config = saw_description = folded = namespace_seen = False
    payload = False
    block_indent = None
    in_config = False
    for raw in frontmatter:
        line = raw.rstrip("\r")
        if not line.strip(" "):
            continue
        key_start = len(line) - len(line.lstrip(" "))
        if folded and not in_config and line.startswith(" "):
            indent = len(line) - len(line.lstrip(" "))
            if block_indent is None:
                block_indent = indent
            if indent != block_indent:
                return None
            payload = True
            continue
        if key_start != 0:
            if not in_config:
                return None
            leaf = line[key_start:]
            if leaf.startswith("#"):
                continue
            key, sep, value = leaf.partition(":")
            if not sep:
                return None
            value = value.strip(" ")
            if key_start == 2 and key == "usecase" and value == "":
                if namespace_seen:
                    return None
                namespace_seen = True
            elif key_start == 2 and key == "look":
                if look is not None or value != "handDrawn":
                    return None
                look = "handDrawn"
            elif key_start == 2 and key == "theme":
                if theme is not None or value not in THEMES:
                    return None
                theme = value
            elif key_start == 4 and namespace_seen:
                entry = next((item for item in LEAF_ORDER if item[0] == key), None)
                if entry is None or value not in entry[1] or key in leaves:
                    return None
                leaves[key] = value
            else:
                return None
            continue
        if line.startswith("#"):
            continue
        key, sep, rest = line.partition(":")
        if not sep:
            return None
        if key == "config":
            if saw_config:
                return None
            saw_config = True
            in_config = True
        elif key == "description":
            if saw_description:
                return None
            saw_description = True
            folded = rest == " >-"
            in_config = False
        else:
            return None
    if not saw_description:
        return None
    if folded and not payload:
        return None
    if saw_config and not leaves and theme is None and look is None:
        return None
    if namespace_seen and not leaves:
        return None
    lines = []
    if theme:
        lines.append(f"config.theme={theme} -> colour palette; not applied")
    if look:
        lines.append("config.look=handDrawn -> source node style; not applied")
    for key, values, suffix in LEAF_ORDER:
        if key in leaves:
            lines.append(f"config.usecase.{key}={leaves[key]} -> {suffix}; not applied")
    if not lines:
        return None
    return lines


ANNOTATION_NOTICE = "body @{} properties -> shape/type/animation; not applied"


def body_notices(body: str):
    """D-458 appended the body disclosure to these rows: every flat `@{...}`
    annotation (the `json <id>@{` block opener is structural and does not
    count) is now dropped from the transcription and disclosed once."""
    had_annotation = False
    for raw in body.split("\n"):
        line = raw.strip(" ")
        if not line or line.startswith("%%"):
            continue
        if line.startswith("json ") and "@{" in line:
            continue
        # The renderer only strips an unquoted `@{...}`; a quoted label that
        # contains `@{` is authored text.
        in_quote = False
        for at, c in enumerate(line):
            if c == '"':
                in_quote = not in_quote
            elif not in_quote and line.startswith("@{", at):
                had_annotation = True
        if ":::" in line or line.startswith(("classDef ", "class ", "style ")):
            raise ValueError(f"unexpected body styling in a D-456 row: {line}")
    return [ANNOTATION_NOTICE] if had_annotation else []


def body_shape_ok(body: str) -> bool:
    """The two body shapes this cohort adds: a quoted boundary and a json block."""
    quoted_boundary = re.search(r'systemBoundary\s+"[^"\n]+"\s*$', body, re.M)
    json_block = re.search(r'json\s+[A-Za-z_][A-Za-z0-9_]*@\{\s*\n(?:[ \t]*"[^\n]*\n)+[ \t]*\}',
                           body)
    return bool(quoted_boundary and json_block)


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
        notices = frontmatter_lines(source)
        if not notices:
            raise ValueError(f"{rel} does not carry the admitted envelope")
        _, body = split_frontmatter(source)
        if not body_shape_ok(body):
            raise ValueError(f"{rel} does not carry the admitted body shapes")
        body_lines = body_notices(body)
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
            "expected_footer_lines": [HEADER] + notices + body_lines,
            "twin": {
                "kind": "frontmatter-free-body",
                "path": rel,
                "sha256": observed_sha,
                "footer_lines": [],
            },
            "ledger_note": (
                "the usecase projection transcribes statements verbatim and "
                "never reads a palette or a source-node style, so the closed "
                "frontmatter leaves are inert and disclosed; the quoted "
                "boundary title and the JSON block are authored text"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": ("the closed `config.theme` / `config.look: handDrawn` "
                      "leaves, the closed `config.usecase` leaves including "
                      "`useMaxWidth: true` and `colorScheme: rotate`, a quoted "
                      "`systemBoundary \"Title\"` label and a "
                      "`json <id>@{ ... }` block"),
            "footer": {"header": HEADER, "line_limit_bytes": 96},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin is the same fixture with the frontmatter "
                          "removed; the art above the footer must be cell-equal"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("usecase originals whose only non-admitted inputs are the "
                     "closed themed-envelope leaves, the quoted boundary title "
                     "and the JSON block"),
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
    raise SystemExit(main())
