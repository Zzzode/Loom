#!/usr/bin/env python3
"""Generate the D-458 usecase body-grammar cohort ledger.

Renderer-free. The eight rows are the last usecase fallbacks: the complete
use-case bodies whose only non-admitted inputs are the closed body styling
statements (`classDef`, `class`, `style`), one trailing `:::class` annotation
on a declaration / boundary title / json block close, flat `@{ ... }`
properties, single-line json payloads and inline `json <id>@{ ... }` blocks.

The usecase projection is a verbatim text outline: it draws no colour, line
weight, shape or animation, so those surfaces are validated, dropped from the
outline and disclosed. Everything else is transcribed as authored.

Twin rule (pinned): the twin is the same fixture with the styling statements,
the `:::class` annotations and the `@{...}` properties removed; the art above
the footer must be cell-equal.

Usage: python3 tools/mermaid/generate_usecase_body_d458_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/usecase-body-d458-cohort.json")
SCHEMA = "loom_mermaid_usecase_body_d458_cohort/v1"
DECISION = "D-458"

HEADER = "Usecase native projection v1"
CLASS_NOTICE = "body :::class annotations -> colour/line weight; not applied"
ANNOTATION_NOTICE = "body @{} properties -> shape/type/animation; not applied"
THEMES = ("base", "dark", "default", "forest", "neutral", "redux", "redux-dark",
          "redux-color", "redux-dark-color")
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
    "usecase/nightly-check/07-edge-styles-and-animation.mmd",
    "usecase/nightly-check/08-json-table.mmd",
    "usecase/nightly-check/12-boundary-contained-actors.mmd",
    "usecase/nightly-check/14-element-styling-precedence.mmd",
    "usecase/nightly-check/16-json-order-and-values.mmd",
    "usecase/nightly-check/20-full-featured-order-system.mmd",
    "usecase/pr-check/complete-hand-drawn-rendering-contract.mmd",
    "usecase/pr-check/small-representative-visual-snapshot.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review, fresh general-purpose "
        "agent, D-458 usecase body grammar, 2026-11-24: round 1 "
        "REQUEST-CHANGES (BLOCKER quoted `@{...}` silently dropped; blast "
        "radius on four drawn rows; blind twin; empty annotation; class scope; "
        "trailing comma; braces inside json strings; mirror four-colon "
        "divergence), round 2 APPROVE after fixes (task aba09a2cce8422b7f, "
        "resumed for the re-check)"),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": (
        "The roster regeneration and the d117 counts are deferred to the "
        "post-approval OfficialMatrix rerun; both landed in the same commit."),
}

IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]{0,127}")
HEX = re.compile(r"#(?:[0-9a-fA-F]{3}|[0-9a-fA-F]{6})")
PX = re.compile(r"\d{1,3}px")

STYLE_KEYS = ("fill", "stroke", "stroke-width")


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
    """The admitted config disclosure lines in canonical order, or None."""
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
    return lines


def strip_class(text: str):
    """One trailing `:::class` annotation; raises on a malformed shape."""
    at = text.rfind(":::")
    if at < 0:
        return None, text
    if text.find(":::") != at:
        raise ValueError(f"second class annotation: {text}")
    name = text[at + 3:].strip(" ")
    if not IDENT.fullmatch(name):
        raise ValueError(f"malformed class annotation: {text}")
    prefix = text[:at].rstrip(" ")
    if not prefix:
        raise ValueError(f"empty class prefix: {text}")
    return name, prefix


def strip_annotations(text: str):
    """Remove every unquoted flat `@{ ... }` property block; quoted text that
    contains `@{` is authored label content and is kept."""
    out = []
    pos = 0
    had = False
    in_quote = False
    while pos < len(text):
        c = text[pos]
        if c == '"':
            in_quote = not in_quote
            out.append(c)
            pos += 1
            continue
        if not in_quote and text.startswith("@{", pos):
            close = text.find("}", pos + 2)
            if close < 0:
                raise ValueError(f"unclosed annotation: {text}")
            inside = text[pos + 2:close]
            if not inside.strip(" ") or "{" in inside:
                raise ValueError(f"empty or nested annotation: {text}")
            while out and out[-1] == " ":
                out.pop()
            pos = close + 1
            had = True
            continue
        out.append(c)
        pos += 1
    if in_quote:
        raise ValueError(f"unbalanced quotes: {text}")
    return had, "".join(out).strip(" ")


def style_props(text: str) -> bool:
    seen = []
    pos = 0
    any_key = False
    expect_entry = False
    while True:
        while pos < len(text) and text[pos] == " ":
            pos += 1
        if pos >= len(text):
            return (not expect_entry) and any_key
        colon = text.find(":", pos)
        if colon < 0:
            return False
        key = text[pos:colon].strip(" ")
        comma = text.find(",", colon + 1)
        value = text[colon + 1:comma if comma >= 0 else len(text)].strip(" ")
        if key in ("fill", "stroke"):
            ok = HEX.fullmatch(value) is not None
        elif key == "stroke-width":
            ok = PX.fullmatch(value) is not None
        else:
            return False
        if not ok or key in seen:
            return False
        seen.append(key)
        any_key = True
        if comma < 0:
            break
        pos = comma + 1
        expect_entry = True
    return any_key


def identifier_list(text: str) -> bool:
    text = text.strip(" ")
    if not text:
        return False
    items = [item.strip(" ") for item in text.split(",")]
    return all(IDENT.fullmatch(item) for item in items)


def json_payload_line(payload: str, content: str) -> None:
    if not payload or not payload.startswith('"'):
        raise ValueError(f"malformed json payload: {content}")
    depth = 0
    in_quote = False
    for c in payload:
        if c == '"':
            in_quote = not in_quote
            continue
        if in_quote:
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth < 0:
                raise ValueError(f"unbalanced json payload: {content}")
    if depth != 0 or in_quote:
        raise ValueError(f"unbalanced json payload: {content}")


def short_name(text: str) -> str:
    return text if len(text) <= 40 else text[:40] + "..."


def logical_statements(body: str):
    """Join physical lines across quoted line breaks, mirroring the renderer:
    a statement ends when its quote count is even."""
    current = []
    quotes = 0
    for raw in body.split("\n"):
        line = raw.rstrip("\r")
        outside_quote = quotes % 2 == 0
        if outside_quote and line.strip(" ").startswith("%%"):
            if current:
                stmt = " ".join(current).strip(" ")
                if stmt:
                    yield stmt
                current = []
                quotes = 0
            if line:
                yield line.strip(" ")
            continue
        if current:
            current.append(" ")
        current.append(line)
        quotes += line.count('"')
        if quotes % 2 == 0:
            stmt = "".join(current).strip(" ")
            if stmt:
                yield stmt
            current = []
            quotes = 0
    if quotes % 2 != 0:
        raise ValueError("unclosed quote")


def body_notices(text: str):
    """The body disclosure lines in the renderer's encounter order."""
    _, body = split_frontmatter(text)
    notices = []
    class_noted = annotation_noted = False
    in_json = False
    for content in logical_statements(body):
        if not content or content.startswith("%%"):
            continue
        if content == "usecase-beta":
            continue
        if in_json:
            if content == "}":
                in_json = False
                continue
            if content.startswith("}:::"):
                name, prefix = strip_class(content)
                if prefix != "}":
                    raise ValueError(f"malformed json close: {content}")
                if not class_noted:
                    notices.append(CLASS_NOTICE)
                    class_noted = True
                in_json = False
                continue
            continue
        name, unclassed = strip_class(content)
        if name is not None and not class_noted:
            notices.append(CLASS_NOTICE)
            class_noted = True
        if unclassed.startswith("json ") and "@{" in unclassed:
            rest = unclassed[5:].strip(" ")
            open_at = rest.find("@{")
            if open_at <= 0 or not IDENT.fullmatch(rest[:open_at].strip(" ")):
                raise ValueError(f"malformed json opener: {content}")
            payload = rest[open_at + 2:].strip(" ")
            if not payload:
                in_json = True
            elif not payload.endswith("}"):
                raise ValueError(f"malformed inline json: {content}")
            else:
                json_payload_line(payload[:-1].strip(" "), content)
            continue
        had_annotation, plain = strip_annotations(unclassed)
        if had_annotation and not annotation_noted:
            notices.append(ANNOTATION_NOTICE)
            annotation_noted = True
        if plain.startswith("classDef "):
            rest = plain[9:].strip(" ")
            names, _, props = rest.partition(" ")
            if not IDENT.fullmatch(names) and not identifier_list(names):
                raise ValueError(f"malformed classDef: {content}")
            if not style_props(props):
                raise ValueError(f"malformed classDef props: {content}")
            notices.append(f"body classDef={short_name(names)} -> "
                           "colour/line weight; not applied")
        elif plain.startswith("class "):
            rest = plain[6:].strip(" ")
            ids, _, classes = rest.partition(" ")
            if not identifier_list(ids) or not identifier_list(classes):
                raise ValueError(f"malformed class statement: {content}")
            notices.append(f"body class={short_name(ids)} -> "
                           "colour/line weight; not applied")
        elif plain.startswith("style "):
            rest = plain[6:].strip(" ")
            ident, _, props = rest.partition(" ")
            if not IDENT.fullmatch(ident) or not style_props(props):
                raise ValueError(f"malformed style statement: {content}")
            notices.append(f"body style={ident} -> colour/line weight; not applied")
    return notices


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                      for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    for rel in COHORT:
        text = (repo_root / CASES / rel).read_text()
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        config_lines = frontmatter_lines(text)
        if config_lines is None:
            raise ValueError(f"{rel} does not carry the admitted frontmatter")
        notices = body_notices(text)
        if not notices:
            raise ValueError(f"{rel} carries no admitted body surface")
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
            "expected_footer_lines": [HEADER] + config_lines + notices,
            "twin": {"kind": "body-styling-stripped", "path": rel},
            "ledger_note": (
                "the projection transcribes logical statements only: the "
                "validated classDef/class/style statements, `:::class` "
                "annotations and `@{...}` properties are dropped and disclosed, "
                "and the style-stripped twin renders identical art"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": ("closed body `classDef` / `class` / `style` styling "
                      "statements, one trailing `:::class` annotation on a "
                      "declaration / boundary title / edge / json close, flat "
                      "`@{...}` properties, and single-line json payloads"),
            "footer": {"header": HEADER, "line_limit_bytes": 96},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin is the same fixture with the styling "
                          "statements, `:::class` annotations and `@{...}` "
                          "properties removed; the art above the footer must "
                          "be cell-equal"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("the last usecase fallbacks whose only non-admitted "
                     "inputs are the closed body styling surface"),
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
