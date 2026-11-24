#!/usr/bin/env python3
"""Generate the D-460 ER body style-admission cohort ledger.

Renderer-free. The fifteen rows are every ER fallback whose only non-admitted
inputs are the closed ER style surface:

* `classDef <id> <css>` / `style <id-list> <css>` / `class <id-list> <class>`
  statements over the closed CSS table (hex or named colour, `stroke-width`,
  `font-size`, `font-weight`) and
* one or more inline `:::class` annotations on an entity declaration.

The ER projection paints no class surface and owns a fixed orthogonal layout,
so every accepted statement is disclosed once in the footer and dropped; the
style-stripped twin must render identical art above the footer.

Twin rule (pinned): the twin is the same fixture with the style statements
removed and the `:::class` suffixes cut; the art above the footer is cell-equal
and the footer tail is exactly the disclosed records in source order.

Usage: python3 tools/mermaid/generate_er_d460_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/er-d460-cohort.json")
SCHEMA = "loom_mermaid_er_d460_cohort/v1"
DECISION = "D-460"

HEADER = "ER native projection v1"
RECORD_SUFFIX = " -> class surface; not applied"

# The reviewed projection-profile triggers reachable from this cohort: a
# `config:` block carrying `look` or `htmlLabels` activates the ER presentation
# profile. (The C++ leaf table also accepts `theme`/`themeCSS`; no cohort
# fixture carries them, and the live gate is the authority either way.)
PROFILE_KEYS = ("look", "htmlLabels")
NEW_NAMED_COLOURS = ("lightblue", "pink", "green", "grey", "blue")

COHORT = [
    "er-diagram/visual-check/entities-with-styles-applied-from-class-statement-default.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-class-statement-without-htmllabels-default.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-class-statement-elk-layout.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-class-statement-handdrawn-look.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-class-statement-without-htmllabels-elk-layout.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-class-statement-without-htmllabels-handdrawn-look.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-style-statement-default.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-style-statement-without-htmllabels-default.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-style-statement-elk-layout.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-style-statement-handdrawn-look.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-style-statement-without-htmllabels-elk-layout.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-style-statement-without-htmllabels-handdrawn-look.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-the-default-class-and-other-styles-default.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-the-default-class-and-other-styles-elk-layout.mmd",
    "er-diagram/visual-check/entities-with-styles-applied-from-the-default-class-and-other-styles-handdrawn-look.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "two independent read-only adversarial reviews, fresh general-purpose "
        "agents, D-460 ER body style admission, 2026-11-24. Correctness/blast "
        "radius: round 1 REQUEST-CHANGES (BLOCKER ER206 ordinal bypass; "
        "BLOCKER two stale oracles; MINOR legend-suffix loss, footer limits, "
        "per-cell styles), round 2 REQUEST-CHANGES (BLOCKER envelope "
        "construct-rejection pins 87/10 missed; MINOR path-only ordinal "
        "binding), round 3 REQUEST-CHANGES (BLOCKER the C3 fix compared the "
        "raw fixture sha against the canonical-record manifest sha), round 4 "
        "APPROVE after the canonical tuple fix (task ab84de2ca4d731a22). "
        "Contract/process/oracle strength: round 1 REQUEST-CHANGES (BLOCKER "
        "ledger pending; MAJOR d117 counts 1745/400; MAJOR `layout` misstated "
        "as a projection trigger; MINOR alternatives/disclosure/width-bound/"
        "has_projection), round 2 APPROVE (task a18a34f8b7b6f3961) "
        "conditional on the mechanical flip and the green matrix/suite runs."),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": ("Approval is void if the post-flip release OfficialMatrix, the "
             "dual-preset byte gate or the full debug mermaid suite is red. "
             "The fallback roster is regenerated with `--emit` from the "
             "approved matrix rerun (never hand-edited); the d117 totals land "
             "in the same commit. Measured after approval: release "
             "OfficialMatrix 2145 originals 1745 drawn / 400 source_fallback, "
             "drawn_at_240 1559, gaps 48, ER summary 68 decoded + 123 pending "
             "+ 15 cohort = 206 admitted."),
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


def profile_active(front) -> bool:
    """True when the frontmatter activates the ER presentation profile."""
    in_config = False
    for raw in front or []:
        line = raw.rstrip()
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if not line.startswith((" ", "\t")):
            in_config = line.split(":", 1)[0].strip() == "config"
            continue
        if in_config and line.strip().split(":", 1)[0].strip() in PROFILE_KEYS:
            return True
    return False


def css_new_surface(values: str) -> bool:
    """True when a CSS declaration list needs the D-460 vocabulary.

    The pre-D-460 closed table already accepted hex colours, the named colour
    `red`, `stroke-width` and `stroke-dasharray`; anything else (`font-size`,
    `font-weight`, the other named colours) is newly admitted and therefore
    disclosed even in a profile-free source.
    """
    for field in values.split(","):
        key, sep, value = field.partition(":")
        if not sep:
            return True
        key, value = key.strip(), value.strip()
        if key in ("font-size", "font-weight"):
            return True
        if key in ("fill", "stroke", "color"):
            if not value.startswith("#") and value in NEW_NAMED_COLOURS:
                return True
    return False


def style_notices(text: str):
    """Mirror of the renderer's ER style disclosure records.

    A top-level statement line carrying `classDef`/`style`/`class`, or any line
    with an inline `:::class` suffix, is accepted. Whether it is *disclosed*
    depends on the pre-D-460 behaviour the slice must preserve byte-for-byte:
    a source with an active presentation profile discloses every accepted
    input; a profile-free source already admitted the old surface silently, so
    it discloses only the newly admissible vocabulary (`font-size`,
    `font-weight` and the named colours other than `red`; a `class` binding or
    inline `:::` carries no CSS and stays silent there). Each record is the
    trimmed authored line (TAB rendered as `\\t`) followed by the fixed suffix;
    entity-body lines inside `{ ... }` are members, not statements.

    Known simplifications against the C++ parser (none of them reachable from
    the fifteen pinned fixtures, where this mirror and the live render agree
    record-for-record; the live gate is the authority):

    * statement detection is a prefix test on the trimmed line, so a keyword
      followed by a TAB (`classDef\\tfoo`) would be a statement in the renderer
      but not here;
    * `:::` is recognised anywhere on the line rather than at the declaration
      positions `er_inline_classes` accepts, so an annotation inside a quoted
      label would be over-reported here;
    * a trailing `\\r` is stripped by `rstrip`, and brace depth is counted over
      raw characters (including braces inside quoted labels), while the
      renderer tracks entity bodies through its cursor;
    * the CSS value list is split at the first space after the keyword, so a
      target list written with spaces (`style c, p fill:red`) would be
      mis-split here;
    * the profile trigger set is the two keys this cohort uses; the C++ leaf
      table also accepts `theme`/`themeCSS`.
    """
    front, body = split_frontmatter(text)
    profile = profile_active(front)
    records = []
    depth = 0
    for raw in body.split("\n"):
        line = raw.rstrip("\r").strip(" \t")
        if not line or line.startswith("%%"):
            continue
        if depth:
            depth += line.count("{") - line.count("}")
            continue
        depth += line.count("{") - line.count("}")
        statement = (
            line.startswith("classDef ")
            or line.startswith("style ")
            or (line.startswith("class ") and not line.startswith("classDef "))
        )
        inline = ":::" in line
        if not statement and not inline:
            continue
        disclosed = profile
        if not disclosed and statement and not line.startswith("class "):
            disclosed = css_new_surface(line.split(" ", 1)[1].split(" ", 1)[-1])
        if not disclosed:
            continue
        records.append(line.replace("\t", "\\t") + RECORD_SUFFIX)
    return records


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
        notices = style_notices(text)
        if not notices:
            raise ValueError(f"{rel} carries no admitted style statement")
        rows.append({
            "path": rel,
            "family": "er-diagram",
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
            "expected_footer_header": HEADER,
            "expected_style_notices": notices,
            "twin": {"kind": "style-stripped", "path": rel},
            "ledger_note": (
                "the ER projection paints no class surface; each disclosed "
                "input is recorded once and the stripped twin renders "
                "identical art"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": ("closed `classDef` / `class` / `style` statements over "
                      "the ER CSS table (hex or named colour, stroke-width, "
                      "font-size, font-weight) and inline `:::class` entity "
                      "annotations"),
            "disclosure": ("a presentation-profile source discloses every "
                           "accepted input; a profile-free source (whose old "
                           "surface was already admitted silently) discloses "
                           "only the newly admissible vocabulary, so its art "
                           "stays byte-identical"),
            "footer": {"header": HEADER, "record_suffix": RECORD_SUFFIX},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin is the same fixture with the style "
                          "statements removed and the `:::class` suffixes cut; "
                          "the art above the footer must be cell-equal"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("every remaining er-diagram fallback whose only "
                     "non-admitted inputs are the closed ER style surface"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows), "by_family": {"er-diagram": len(rows)}},
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
