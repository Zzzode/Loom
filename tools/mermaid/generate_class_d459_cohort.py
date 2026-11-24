#!/usr/bin/env python3
"""Generate the D-459 class-diagram shared style-admission cohort ledger.

Renderer-free. The 25 rows are every remaining class-diagram fallback. Their
only non-admitted inputs are one of:

* `cssClass "<id-list>" <class>` naming a stylesheet class this source never
  declares with `classDef` (one or several comma-separated targets),
* one trailing `:::class` annotation on a `class` declaration, with or without
  the member-body opener `{ /* ... */ }`,
* a `link` / `callback` interaction binding, which has no static rendering
  surface in a terminal (FS-2 precedent).

An assignment naming an absent `classDef` is inert: upstream attaches a CSS
class attribute and the pinned stylesheet carries no rule for an undeclared
name, so the drawing is identical to the same source with the statement
removed. Each inert statement is disclosed once in the reviewed legend.

Twin rule (pinned): the twin is the same fixture with the `cssClass`
statements, the `:::class` annotations and the `link`/`callback` statements
removed; the art above the legend must be cell-equal.

Usage: python3 tools/mermaid/generate_class_d459_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/class-d459-cohort.json")
SCHEMA = "loom_mermaid_class_d459_cohort/v1"
DECISION = "D-459"

# The reviewed legend terminator; every record line above it holds " ->".
LEGEND_KEY = (
    "key: hue roles use palette; width>=4=bold border; color on fill not "
    "projected; monochrome/ANSI16 may share shades; source TAB=\\t")
UNDEFINED_SUFFIX = " undefined; not applied"
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]{0,63}")

COHORT = [
    "class-diagram/nightly-check/10-should-render-a-simple-class-diagram-with-clickable-callback-2.mmd",
    "class-diagram/nightly-check/10-should-render-a-simple-class-diagram-with-clickable-callback-3.mmd",
    "class-diagram/nightly-check/10-should-render-a-simple-class-diagram-with-clickable-callback.mmd",
    "class-diagram/nightly-check/13-should-render-a-simple-class-diagram-with-css-classes-applied-2.mmd",
    "class-diagram/nightly-check/13-should-render-a-simple-class-diagram-with-css-classes-applied-3.mmd",
    "class-diagram/nightly-check/13-should-render-a-simple-class-diagram-with-css-classes-applied.mmd",
    "class-diagram/nightly-check/14-should-render-a-simple-class-diagram-with-css-classes-applied-directly-2.mmd",
    "class-diagram/nightly-check/14-should-render-a-simple-class-diagram-with-css-classes-applied-directly-3.mmd",
    "class-diagram/nightly-check/14-should-render-a-simple-class-diagram-with-css-classes-applied-directly.mmd",
    "class-diagram/nightly-check/15-should-render-a-simple-class-diagram-with-css-classes-applied-to-multiple-classes.mmd",
    "class-diagram/nightly-check/15-should-render-a-simple-class-diagram-with-css-classes-applied-two-multiple-classes-2.mmd",
    "class-diagram/nightly-check/15-should-render-a-simple-class-diagram-with-css-classes-applied-two-multiple-classes.mmd",
    "class-diagram/nightly-check/9-should-render-a-simple-class-diagram-with-clickable-link-2.mmd",
    "class-diagram/nightly-check/9-should-render-a-simple-class-diagram-with-clickable-link-3.mmd",
    "class-diagram/nightly-check/9-should-render-a-simple-class-diagram-with-clickable-link.mmd",
    "class-diagram/nightly-check/dagre/dagre-10-should-render-a-simple-class-diagram-with-clickable-callback.mmd",
    "class-diagram/nightly-check/dagre/dagre-13-should-render-a-simple-class-diagram-with-css-classes-applied.mmd",
    "class-diagram/nightly-check/dagre/dagre-14-should-render-a-simple-class-diagram-with-css-classes-applied-directly.mmd",
    "class-diagram/nightly-check/dagre/dagre-15-should-render-a-simple-class-diagram-with-css-classes-applied-two-multiple-classes.mmd",
    "class-diagram/nightly-check/dagre/dagre-9-should-render-a-simple-class-diagram-with-clickable-link.mmd",
    "class-diagram/nightly-check/handdrawn/hd-10-should-render-a-simple-class-diagram-with-clickable-callback.mmd",
    "class-diagram/nightly-check/handdrawn/hd-13-should-render-a-simple-class-diagram-with-css-classes-applied.mmd",
    "class-diagram/nightly-check/handdrawn/hd-14-should-render-a-simple-class-diagram-with-css-classes-applied-directly.mmd",
    "class-diagram/nightly-check/handdrawn/hd-15-should-render-a-simple-class-diagram-with-css-classes-applied-two-multiple-classes.mmd",
    "class-diagram/nightly-check/handdrawn/hd-9-should-render-a-simple-class-diagram-with-clickable-link.mmd",
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
# Only the interaction-binding rows fit 40 columns: every row that discloses an
# inert assignment also gains a legend line wider than the narrow viewport and
# stays source there (re-measured, engineering rule 18).
NARROW_DRAW = {
    "class-diagram/nightly-check/10-should-render-a-simple-class-diagram-with-clickable-callback-2.mmd",
    "class-diagram/nightly-check/10-should-render-a-simple-class-diagram-with-clickable-callback-3.mmd",
    "class-diagram/nightly-check/10-should-render-a-simple-class-diagram-with-clickable-callback.mmd",
    "class-diagram/nightly-check/9-should-render-a-simple-class-diagram-with-clickable-link-2.mmd",
    "class-diagram/nightly-check/9-should-render-a-simple-class-diagram-with-clickable-link-3.mmd",
    "class-diagram/nightly-check/9-should-render-a-simple-class-diagram-with-clickable-link.mmd",
    "class-diagram/nightly-check/dagre/dagre-10-should-render-a-simple-class-diagram-with-clickable-callback.mmd",
    "class-diagram/nightly-check/dagre/dagre-9-should-render-a-simple-class-diagram-with-clickable-link.mmd",
    "class-diagram/nightly-check/handdrawn/hd-10-should-render-a-simple-class-diagram-with-clickable-callback.mmd",
    "class-diagram/nightly-check/handdrawn/hd-9-should-render-a-simple-class-diagram-with-clickable-link.mmd",
}

REVIEW = {
    "reviewer_agent": (
        "two independent read-only adversarial reviews, fresh general-purpose "
        "agents, D-459 class shared style admission, 2026-11-24: seam review "
        "round 1 REQUEST-CHANGES (BLOCKER ledger pending; MAJOR built-in/"
        "implicit `default` selector names judged inert; MAJOR case-only "
        "classDef mismatch silently inert; MINOR charge/coverage/nits), round "
        "2 APPROVE after fixes (task a0bd8b12fafda36c5); ledger/integration "
        "review round 1 REQUEST-CHANGES (BLOCKER Class StageC pins 285/25 "
        "broken by the cohort branch; MINOR docs/ledger nits), round 2 "
        "REQUEST-CHANGES (MAJOR drawn_at_240 double-counted the 25 rows), round "
        "3 APPROVE after the counter and report fixes (task "
        "a29e4b1f54dd53c6e)"),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": ("Measured after approval: release OfficialMatrix 2145 originals "
             "1730 drawn / 415 source_fallback, drawn_at_240 1544, gaps 48, "
             "Class summary 260 decoded + 0 pending + 25 cohort = 285 admitted."),
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


def legend_line(verbatim: str, class_name: str) -> str:
    return verbatim.replace("\t", "\\t") + " -> class:" + class_name + UNDEFINED_SUFFIX


def legend_lines(text: str):
    """Mirror of the Class legend for the admitted inert statements.

    `cssClass "<id-list>" <class>` emits one record per statement (a target list
    shares its verbatim), `class <id>:::<class> [{ }]` emits the declaration
    record, and `link`/`callback` are non-visual no-ops with no record.
    """
    _, body = split_frontmatter(text)
    records: list[str] = []
    seen: set[str] = set()
    member_body = False
    for raw in body.split("\n"):
        physical = raw.rstrip("\r")
        # The C++ line splitter trims spaces and tabs; the legend verbatim keeps
        # the physical line exactly as authored (including its indentation).
        line = physical.strip(" \t")
        if not line:
            continue
        if member_body:
            if line == "}":
                member_body = False
            continue
        if line.startswith("%%"):
            continue
        if line.startswith("classDef ") or line.startswith("style "):
            raise ValueError(f"unexpected definition in a D-459 row: {line}")
        if line.startswith("cssClass "):
            rest = line[8:].strip(" ")
            if not rest.startswith('"'):
                raise ValueError(f"malformed cssClass: {line}")
            close = rest.find('"', 1)
            if close < 0:
                raise ValueError(f"malformed cssClass: {line}")
            targets = [item.strip(" ") for item in rest[1:close].split(",")]
            if not targets or any(not IDENT.fullmatch(item) for item in targets):
                raise ValueError(f"malformed cssClass target list: {line}")
            name = rest[close + 1:].strip(" ")
            if not IDENT.fullmatch(name):
                raise ValueError(f"malformed cssClass name: {line}")
            # The renderer dedups on the physical verbatim, not the trimmed key.
            if physical not in seen:
                seen.add(physical)
                records.append(legend_line(physical, name))
            continue
        if line.startswith("link ") or line.startswith("callback "):
            parts = line.split(" ")
            if len(parts) < 4 or not parts[2].startswith('"') or \
                    not parts[3].startswith('"'):
                raise ValueError(f"malformed interaction binding: {line}")
            continue
        if line.startswith("class ") and ":::" in line:
            head, _, tail = line.partition(":::")
            class_name = tail.strip(" ")
            if class_name.endswith("{}"):
                class_name = class_name[:-2].strip(" ")
            elif class_name.endswith("{"):
                class_name = class_name[:-1].strip(" ")
            if not class_name or ":::" in class_name or " " in class_name:
                raise ValueError(f"malformed class annotation: {line}")
            if not head[6:].strip(" "):
                raise ValueError(f"empty class name: {line}")
            records.append(legend_line(physical, class_name))
            if tail.strip(" ").endswith("{"):
                member_body = True
            continue
        if line.startswith("class ") and line.endswith("{"):
            member_body = True
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
        records = legend_lines(text)
        rows.append({
            "path": rel,
            "family": "class-diagram",
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
            "expected_legend_lines": records + [LEGEND_KEY] if records else [],
            "twin": {"kind": "inert-styling-stripped", "path": rel},
            "ledger_note": (
                "the undefined stylesheet class is inert (no classDef rule) and "
                "the interaction binding has no terminal surface; the stripped "
                "twin renders identical art"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": ("a `cssClass \"<id-list>\" <class>` statement naming a "
                      "class the source never declares, one trailing `:::class` "
                      "annotation on a `class` declaration (optionally followed "
                      "by the member-body opener), and `link` / `callback` "
                      "interaction bindings"),
            "legend": {
                "record": "<statement> -> class:<name> undefined; not applied",
                "terminator": LEGEND_KEY,
            },
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin is the same fixture with the cssClass "
                          "statements, the :::class annotations and the "
                          "link/callback statements removed; the art above the "
                          "legend must be cell-equal"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("every remaining class-diagram fallback whose only "
                     "non-admitted inputs are the inert undefined-class "
                     "surface and non-visual interaction bindings"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows), "by_family": {"class-diagram": len(rows)}},
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
