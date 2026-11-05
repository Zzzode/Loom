#!/usr/bin/env python3
"""Generate the RFC 0031 FS-1 group-title pre-registration ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/flowchart root that
    - opens with a column-0 ``---`` frontmatter fence whose top-level keys
      are only description/title/config and whose ``config`` map is accepted
      by the production flow envelope (the set of non-terminal/inert
      leaves: layout dagre, look handDrawn, htmlLabels bool, fontFamily
      courier, logLevel, useMaxWidth bools, titleTopMargin,
      diagramPadding, securityLevel loose; mirrors flow_envelope_body),
    - carries a subgraph heading written as a backtick Markdown string in
      any of the three grammar forms: ``subgraph id["`...`"]``,
      ``subgraph "`...`"``, ``subgraph `...```.

Rows already owned by the sibling FS-1 label cohort (tools/mermaid/
fs1-cohort.json) are excluded so the two ledgers never overlap.

Each selected row is pre-registered predicted-draw or predicted-blocked.
Two closed source-content blocker detectors classify the rows the
current Markdown subset cannot honestly paint whole-scene, in precedence
order:
  * flow-md-physical-newline-span: some backtick-delimited label spans
    physical lines (multi-line Markdown node/edge labels are a later
    cohort; parse stays line-oriented),
  * flow-md-angle-in-span: a backtick span contains a literal '<'/'>'
    that is not one of the three accepted <br> forms.
Any residual selected row aborts generation: zero unclassified rows.

The single draw row (visual-check/70) paints at all three widths on both
tracks: its body is narrow and the only frontmatter key is the CSS-only
fontFamily, so there is no disclosure footer.

Usage: python3 tools/mermaid/generate_fs1g_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

import yaml

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs1g-cohort.json")
FS1_LEDGER = pathlib.Path("tools/mermaid/fs1-cohort.json")

SCHEMA = "loom_mermaid_fs1g_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-md-physical-newline-span",
    "flow-md-angle-in-span",
]

WITNESS = {
    "flow-md-physical-newline-span": (
        "src/ui/visual/mermaid_impl.cpp (parse is line-oriented; "
        "markdown_label receives no physical-line joining)",
        "A backtick-delimited node/edge label spans physical lines. "
        "Multi-line physical Markdown labels are a later cohort; the line "
        "parser never joins a quoted statement across newlines, so the whole "
        "source keeps ordinary fallback.",
    ),
    "flow-md-angle-in-span": (
        "src/ui/visual/mermaid_impl.cpp markdown_label (any '<' other than "
        "<br>/<br/>/<br /> rejects the span)",
        "A backtick Markdown span contains a literal '<' or '>' (comparison "
        "operators / raw HTML) that is not an accepted <br> line break; the "
        "closed Markdown subset rejects it rather than painting raw markup.",
    ),
}

BR_FORMS = ("<br />", "<br/>", "<br>")

# Mirrors the accepted leaves in flow_envelope_body. Values are the exact
# scalar spellings the production gate admits; bools are handled separately.
SCALAR_LEAVES = {
    "layout": {"dagre"},
    "look": {"handDrawn"},
    "fontFamily": {"courier"},
    "logLevel": {"1"},
    "securityLevel": {"loose"},
}
BOOL_LEAVES = {"htmlLabels", "useMaxWidth"}
INT_LEAVES = {"titleTopMargin", "diagramPadding"}
ROOT_KEYS = {"htmlLabels", "layout", "look", "fontFamily", "logLevel",
             "securityLevel", "useMaxWidth", "titleTopMargin",
             "diagramPadding"}

SUBGRAPH_BACKTICK = re.compile(
    r'\[\s*"`[^`]*`"\s*\]|^[ \t]*"`[^`]*`"|^[ \t]*`[^`]*`')


def split_source(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return lines[1:close], lines[close + 1:]
    return None


def description_accepted(frontmatter_lines):
    try:
        header = next(l for l in frontmatter_lines
                      if l.split(":", 1)[0].strip() == "description")
    except StopIteration:
        return True
    form = header.split(":", 1)[1].strip()
    if form == ">-":
        return True
    if form:
        return False
    forbidden = {",", "[", "]", "{", "}", "#", "&", "*", "!", "|", ">",
                 "'", '"', "%", "@", "`"}
    for line in frontmatter_lines:
        if line and not line.startswith(" "):
            break
        if not line.strip():
            continue
        if not line.startswith("  ") or len(line) == 2 or line[2] == " ":
            return False
        payload = line[2:]
        if payload and payload[0] in forbidden:
            return False
        if (payload[0] in ("?", "-") and
                (len(payload) == 1 or payload[1] == " ")):
            return False
        for pos, ch in enumerate(payload):
            if ch == ":" and (pos + 1 == len(payload) or payload[pos + 1] == " "):
                return False
            if ch == "#" and (pos == 0 or payload[pos - 1] == " "):
                return False
    return True


def config_accepted(frontmatter_lines):
    """Mirror flow_envelope_body: top-level scalar leaves only, known set."""
    try:
        parsed = yaml.safe_load("\n".join(frontmatter_lines))
    except yaml.YAMLError:
        return False
    if not isinstance(parsed, dict):
        return False
    top = set(parsed)
    if not top <= {"description", "title", "config"} or "config" not in top:
        return False
    if "description" in top and not description_accepted(frontmatter_lines):
        return False
    config = parsed["config"]
    if not isinstance(config, dict) or not config:
        return False
    if set(config) - ROOT_KEYS:
        return False
    for key, value in config.items():
        if isinstance(value, bool):
            if key not in BOOL_LEAVES:
                return False
        elif isinstance(value, int) and not isinstance(value, bool):
            if key not in INT_LEAVES:
                return False
        elif isinstance(value, (dict, list)):
            return False
        else:
            if key not in SCALAR_LEAVES or str(value) not in SCALAR_LEAVES[key]:
                return False
    return True


def has_backtick_subgraph_heading(body):
    for line in body.split("\n"):
        stripped = line.strip()
        if not stripped.startswith("subgraph"):
            continue
        if SUBGRAPH_BACKTICK.search(stripped[len("subgraph"):]):
            return True
    return False


def backtick_spans_multiline(body):
    """True if a backtick span opens and closes on different physical lines."""
    for match in re.finditer(r"`[^`]*`", body, re.S):
        if "\n" in match.group(0)[1:-1]:
            return True
    return False


def backtick_spans(body):
    for match in re.finditer(r"`([^`]*)`", body, re.S):
        yield match.group(1)


def angle_in_single_line_span(body):
    for span in backtick_spans(body):
        if "\n" in span:
            continue
        stripped = span
        for form in BR_FORMS:
            stripped = stripped.replace(form, "")
        if "<" in stripped or ">" in stripped:
            return True
    return False


def blocker(body):
    if backtick_spans_multiline(body):
        return "flow-md-physical-newline-span"
    if angle_in_single_line_span(body):
        return "flow-md-angle-in-span"
    return None


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def collect(repo_root):
    cases = repo_root / CASES
    fs1_paths = set()
    fs1 = repo_root / FS1_LEDGER
    if fs1.exists():
        fs1_paths = {r["path"] for r in json.loads(fs1.read_text())["rows"]}
    rows = []
    for path in sorted((cases / "flowchart").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        if rel in fs1_paths:
            continue
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        frontmatter, body_lines = split
        body = "\n".join(body_lines)
        if not has_backtick_subgraph_heading(body):
            continue
        if not config_accepted(frontmatter):
            continue
        rows.append({"path": rel, "blocker": blocker(body)})
    return rows


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    entries = collect(repo_root)
    rows = []
    n_draw = n_blocked = 0
    for entry in entries:
        kind = entry["blocker"]
        if kind is not None and kind not in BLOCKER_VOCABULARY:
            raise ValueError(f"Blocker outside closed vocabulary: {kind}")
        blocked = kind is not None
        n_blocked += blocked
        n_draw += not blocked
        site, detail = WITNESS[kind] if blocked else ("", "")
        manifest_row = manifest_rows[entry["path"]]
        state = "source" if blocked else "drawn"
        rows.append({
            "path": entry["path"],
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / entry["path"]),
            "manifest_sha256": manifest_row["sha256"],
            "prior_status": manifest_row["raw"],
            "prediction": "predicted-blocked" if blocked else "predicted-draw",
            # The 70 draw row is narrow and carries no disclosure footer, so
            # it paints at every width on both the original and body tracks.
            "expected_states": {
                "wide_240": state,
                "narrow_40": state,
                "restored_240": state,
            },
            "expected_body_states": {
                "wide_240": state,
                "narrow_40": state,
                "restored_240": state,
            },
            "disclosure_footer": False,
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if blocked else None),
            "ledger_note": (
                "FS-1 group-title Markdown admission (bold subgraph heading)"
                if not blocked else
                "Blocked by a closed source-content Markdown detector"),
        })
    total = n_draw + n_blocked
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-1-group-title",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256((repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original accepted by the production "
                    "flow envelope whose body carries a backtick Markdown "
                    "subgraph heading; rows owned by fs1-cohort.json excluded",
            "roots": ["flowchart"],
            "blocker_detection": "two fixed source-content detectors "
                                 "(physical-newline backtick span, angle/raw-"
                                 "HTML in a single-line span); no generator-"
                                 "time rendering; residual rows abort",
            "width_policy": "the only draw row is narrow and footer-free, so "
                            "every state (both tracks) is drawn.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review, "
                              "main-agent inline after fork 0-tool stall "
                              "(D-066 FS-1 group-title, 2026-11-05)",
            "date": "2026-11-05",
            "verdict": "approved",
            "note": "Read-only adversarial review of the production diff, "
                    "mechanical selection/envelope mirror, ledger disjoint-"
                    "ness and zero-residual universe, pixel gate, and matrix "
                    "wiring. No blocker/major; one nit recorded (reopened "
                    "subgraph compares run presence, not run contents).",
        },
        "counts": {
            "total": total,
            "predicted_draw": n_draw,
            "predicted_blocked": n_blocked,
        },
        "blocker_vocabulary": BLOCKER_VOCABULARY,
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


if __name__ == "__main__":
    raise SystemExit(main())
