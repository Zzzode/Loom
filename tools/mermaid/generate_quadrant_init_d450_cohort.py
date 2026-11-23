#!/usr/bin/env python3
"""Generate the D-450 quadrant leading-init admission cohort ledger.

Renderer-free. The five rows are the quadrant-chart fallbacks whose only
blocker is a leading body `%%{init: {...}}%%` directive carrying either the
closed `quadrantChart` sizing/axis-position knobs or the quadrant
`themeVariables` colours. The native projection is a bounded outline that draws
no axes, points or colours, so every admitted key is inert and disclosed in a
footer in source order.

Twin rule (pinned): the twin is the same fixture with the admitted directive
line removed. The outline above the footer must be cell-equal to the twin's.

Usage: python3 tools/mermaid/generate_quadrant_init_d450_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/quadrant-init-d450-cohort.json")
SCHEMA = "loom_mermaid_quadrant_init_d450_cohort/v1"
DECISION = "D-450"

HEADER = "Quadrant native projection v1"
NOTICE_LIMIT = 96
TUNING_KEYS = (
    "chartWidth", "chartHeight", "titlePadding", "titleFontSize", "quadrantPadding",
    "quadrantTextTopPadding", "quadrantLabelFontSize", "quadrantInternalBorderStrokeWidth",
    "quadrantExternalBorderStrokeWidth", "xAxisLabelPadding", "xAxisLabelFontSize",
    "yAxisLabelPadding", "yAxisLabelFontSize", "pointTextPadding", "pointLabelFontSize",
    "pointRadius",
)
POSITIONS = {"xAxisPosition": {"top", "bottom"}, "yAxisPosition": {"left", "right"}}
THEME_KEYS = (
    "quadrant1Fill", "quadrant2Fill", "quadrant3Fill", "quadrant4Fill", "quadrant1TextFill",
    "quadrant2TextFill", "quadrant3TextFill", "quadrant4TextFill", "quadrantPointFill",
    "quadrantPointTextFill", "quadrantXAxisTextFill", "quadrantYAxisTextFill",
    "quadrantInternalBorderStrokeFill", "quadrantExternalBorderStrokeFill", "quadrantTitleFill",
)

COHORT = [
    "quadrant-chart/nightly-check/should-able-to-render-x-axis-on-bottom.mmd",
    "quadrant-chart/pr-check/"
    "should-able-to-render-x-axis-on-bottom-and-y-axis-on-right.mmd",
    "quadrant-chart/pr-check/should-able-to-render-y-axis-on-right-side.mmd",
    "quadrant-chart/pr-check/should-use-all-the-config.mmd",
    "quadrant-chart/pr-check/should-use-all-the-theme-variable.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review (fresh general-purpose "
        "agent, D-450 quadrant leading-init admission, 2026-11-23)"),
    "date": "2026-11-23",
    "verdict": "approved",
    "note": (
        "Reviewer re-ran the generator --check entry and the live gate and "
        "verified: only a leading directive is consumed; the quadrantChart "
        "knobs and quadrant theme colours are closed tables; duplicates, "
        "unknown keys, nested values and non-hex colours fail closed; the "
        "directive-free twin oracle is cell-exact above the footer."),
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


def split_top(text: str):
    """Split an object interior on top-level commas (quotes and braces tracked)."""
    parts, depth, quote, start = [], 0, "", 0
    for at, c in enumerate(text):
        if quote:
            if c == quote:
                quote = ""
            continue
        if c in "\"'":
            quote = c
            continue
        if c == "{":
            depth += 1
            continue
        if c == "}":
            depth -= 1
            if depth < 0:
                return None
            continue
        if c == "," and depth == 0:
            parts.append(text[start:at])
            start = at + 1
    if quote or depth != 0:
        return None
    parts.append(text[start:])
    return parts


def scalar(text: str):
    text = text.strip()
    if len(text) >= 2 and text[0] == '"' and text[-1] == '"':
        text = text[1:-1]
    if not text or any(c in text for c in '"\'{}[]'):
        return None
    return text


def object_interior(text: str):
    text = text.strip()
    if len(text) < 2 or text[0] != "{" or text[-1] != "}":
        return None
    return text[1:-1]


def init_footer(text: str):
    """Footer lines for the admitted leading directive, or None."""
    body = strip_frontmatter(text)
    first = body.split("\n", 1)[0].rstrip("\r").strip()
    if not first.startswith("%%{init"):
        return None
    if not first.startswith("%%{") or not first.endswith("}%%") or len(first) < 8:
        return None
    inner = first[3:-3].strip()
    if inner == "init":
        return []
    if not inner.startswith("init:"):
        return None
    interior = object_interior(inner[5:])
    if interior is None:
        return None
    entries = split_top(interior)
    if entries is None:
        return None
    notices, saw = [], set()
    for entry in entries:
        if ":" not in entry:
            return None
        key, value = entry.split(":", 1)
        key = scalar(key)
        if key is None or key in saw:
            return None
        saw.add(key)
        inner_interior = object_interior(value)
        if inner_interior is None:
            return None
        inner_entries = split_top(inner_interior)
        if inner_entries is None:
            return None
        seen = set()
        for item in inner_entries:
            if ":" not in item:
                return None
            name, raw = item.split(":", 1)
            name = scalar(name)
            raw = scalar(raw)
            if name is None or raw is None or name in seen:
                return None
            seen.add(name)
            if key == "quadrantChart":
                if name in POSITIONS:
                    if raw not in POSITIONS[name]:
                        return None
                    suffix = "SVG layout/axis tuning; not applied"
                elif name in TUNING_KEYS and raw.isdigit() and len(raw) <= 9:
                    suffix = "SVG layout/axis tuning; not applied"
                else:
                    return None
                notices.append(f"%%{{init}}%% quadrantChart.{name}={raw} -> {suffix}")
            elif key == "themeVariables":
                if name not in THEME_KEYS:
                    return None
                if len(raw) != 7 or raw[0] != "#" or \
                        any(c not in "0123456789abcdefABCDEF" for c in raw[1:]):
                    return None
                notices.append(
                    f"%%{{init}}%% themeVariables.{name}={raw} -> colour palette; not applied")
            else:
                return None
    return notices


def init_free_twin(text: str) -> str:
    parts = split_frontmatter(text)
    body = parts[1] if parts else text
    lines = body.split("\n")
    if lines and lines[0].strip().startswith("%%{init"):
        lines = lines[1:]
    return (("---\n" + parts[0] + "\n---\n") if parts else "") + "\n".join(lines)


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
        notices = init_footer(source)
        if notices is None:
            raise ValueError(f"{rel} does not carry the admitted directive shape")
        rows.append({
            "path": rel,
            "family": "quadrant-chart",
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
            "expected_footer_lines": [HEADER] + notices if notices else [],
            "twin": {
                "kind": "init-free-body",
                "path": rel,
                "sha256": observed_sha,
                "footer_lines": [],
            },
            "ledger_note": (
                "the quadrant projection draws no axes, points or colours, so "
                "the closed quadrantChart knobs and quadrant theme colours are "
                "inert and disclosed in source order"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": "leading body `%%{init: {...}}%%` with `quadrantChart` "
                     "sizing/axis-position knobs and quadrant `themeVariables` colours",
            "position_table": {key: sorted(values) for key, values in POSITIONS.items()},
            "tuning_keys": list(TUNING_KEYS),
            "theme_keys": list(THEME_KEYS),
            "footer": {"header": HEADER, "line_limit_bytes": NOTICE_LIMIT},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin is the same fixture with the leading "
                          "directive line removed; the frame oracle compares the "
                          "outline above the footer"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("quadrant-chart fallbacks whose only blocker is a leading "
                     "closed `%%{init}%%` tuning/theme directive"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows), "by_family": {"quadrant-chart": len(rows)}},
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
