#!/usr/bin/env python3
"""Generate the D-457 body-style cohort ledger.

Renderer-free. The three rows are the last body-style fallbacks of two
families whose terminal projections plot labels only:

* quadrant-chart: a body `classDef <name> <closed props>` statement, a
  `:::class` suffix on a data point and the inline per-point style suffix
  (`radius`, `color`, `stroke-color`, `stroke-width`) are validated, dropped
  and disclosed — the outline keeps the plain `name (x, y)` row.
* tree-view: a `:::class` annotation on a node row is validated, dropped and
  disclosed — the outline keeps the plain node row.

Twin rule (pinned): the twin is the same fixture with the styling surface
removed (the classDef/`:::class`/inline-style text), so the art must be
cell-equal and the only difference is the disclosure footer.

Usage: python3 tools/mermaid/generate_style_d457_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/style-d457-cohort.json")
SCHEMA = "loom_mermaid_style_d457_cohort/v1"
DECISION = "D-457"

QUADRANT_HEADER = "Quadrant native projection v1"
TREE_HEADER = "TreeView native projection v1"
CLASS_NOTICE = "body class annotations -> highlight colour; not applied"

IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_-]{0,63}")

COHORT = [
    ("quadrant-chart/pr-check/it-should-render-data-points-with-styles-classes.mmd",
     "quadrant-chart", QUADRANT_HEADER, "quadrant-style-stripped-body"),
    ("quadrant-chart/pr-check/it-should-render-data-points-with-styles.mmd",
     "quadrant-chart", QUADRANT_HEADER, "quadrant-style-stripped-body"),
    ("tree-view/pr-check/should-render-class-annotations-for-highlighting.mmd",
     "tree-view", TREE_HEADER, "treeview-class-stripped-body"),
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review, fresh general-purpose "
        "agent, D-457 quadrant/tree-view body styles, 2026-11-24: round 1 "
        "REQUEST-CHANGES (point-name widening, tree-view fail-open after "
        "icon()/##, notice budget, ledger prose, mirror scope), round 2 "
        "REQUEST-CHANGES (UTF-8 truncation boundary, literal prefix width, "
        "doc wording), round 3 APPROVE after fixes (task aa20b69e9a9c097f5, "
        "resumed for each re-check)"),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": (
        "The roster regeneration and the d117 counts are deferred to the "
        "post-approval OfficialMatrix rerun; both landed in the same commit."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def style_entry_ok(key: str, value: str) -> bool:
    if key == "radius":
        return value.isdigit() and len(value) <= 3
    if key in ("color", "stroke-color"):
        return re.fullmatch(r"#[0-9a-fA-F]{6}", value) is not None
    if key == "stroke-width":
        return re.fullmatch(r"\d{1,3}px", value) is not None
    return False


def style_list(text: str):
    """The closed point-style property list, or None when it does not parse."""
    keys = []
    pos = 0
    while True:
        while pos < len(text) and text[pos] == " ":
            pos += 1
        if pos >= len(text):
            break
        colon = text.find(":", pos)
        if colon < 0:
            return None
        key = text[pos:colon].strip(" ")
        comma = text.find(",", colon + 1)
        value = text[colon + 1:comma if comma >= 0 else len(text)].strip(" ")
        if not key or not style_entry_ok(key, value) or key in keys:
            return None
        keys.append(key)
        if comma < 0:
            break
        pos = comma + 1
    return keys or None


def body_lines(text: str):
    """The body lines after an optional frontmatter block (the renderers scan
    the body only; a frontmatter-only `:::` must not create ledger lines)."""
    lines = text.split("\n")
    if lines and lines[0] == "---":
        for close in range(1, len(lines)):
            if lines[close] == "---":
                return lines[close + 1:]
    return lines


NOTICE_LIMIT = 128


def disclosed_name(name: str, tail: str) -> str:
    """The renderer truncates the disclosed name on a UTF-8 codepoint boundary
    when the closed notice budget cannot carry it. The budget is measured in
    bytes, exactly like the C++ `kQuadrantNoticeLimit` arithmetic."""
    prefix = len(b"point ")
    encoded = name.encode("utf-8")
    if prefix + len(encoded) + len(tail) <= NOTICE_LIMIT:
        return name
    marker = 3  # "..."
    budget = NOTICE_LIMIT - len(tail) - prefix - marker
    if budget < 0:
        raise ValueError("notice tail exceeds the closed budget")
    cut = encoded[:budget]
    while cut:
        try:
            cut.decode("utf-8")
            break
        except UnicodeDecodeError:
            cut = cut[:-1]
    return cut.decode("utf-8") + "..."


def quadrant_notices(text: str):
    """The body notices of the two admitted quadrant rows, in source order."""
    notices = []
    for raw in body_lines(text):
        content = raw.strip(" ")
        if not content or content.startswith("%%"):
            continue
        if content.startswith("classDef "):
            rest = content[len("classDef "):].strip(" ")
            name, _, props = rest.partition(" ")
            if not IDENT.fullmatch(name) or style_list(props) is None:
                raise ValueError(f"unadmitted classDef shape: {content}")
            notices.append(f"body classDef={name} -> colour/radius; not applied")
            continue
        if content.startswith("title ") or content.startswith("x-axis ") or \
                content.startswith("y-axis ") or content.startswith("quadrant-"):
            if ":::" in content:
                raise ValueError(f"an annotated label line gates: {content}")
            continue
        if "[" not in content:
            continue
        open_at = content.index("[")
        close_at = content.index("]")
        prefix = content[:open_at].strip(" ")
        if not prefix.endswith(":"):
            raise ValueError(f"unadmitted point shape: {content}")
        name_part = prefix[:-1].strip(" ")
        point_class = None
        if ":::" in name_part:
            name_part, _, point_class = name_part.rpartition(":::")
            name_part = name_part.strip(" ")
            if not IDENT.fullmatch(point_class) or ":::" in name_part:
                raise ValueError(f"unadmitted class annotation: {content}")
        if not name_part or ":" in name_part:
            raise ValueError(f"unadmitted point name: {content}")
        suffix = content[close_at + 1:].strip(" ")
        props = style_list(suffix) if suffix else None
        if suffix and props is None:
            raise ValueError(f"unadmitted inline style: {content}")
        if point_class or props:
            parts = []
            if point_class:
                parts.append(f"class={point_class}")
            if props:
                parts.append(f"styles={len(props)}")
            tail = f" -> {', '.join(parts)}; not applied"
            notices.append(f"point {disclosed_name(name_part, tail)}{tail}")
    return notices


def treeview_notices(text: str):
    had_class = False
    for raw in body_lines(text):
        content = raw.strip(" ")
        if not content or content.startswith("%%"):
            continue
        if content == "treeView-beta":
            continue
        if ":::" in content:
            if "icon(" in content or "##" in content:
                raise ValueError(f"an annotation after a suffix gates: {content}")
            had_class = True
    return [CLASS_NOTICE] if had_class else []


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                      for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    for rel, family, header, twin_kind in COHORT:
        text = (repo_root / CASES / rel).read_text()
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        if family == "quadrant-chart":
            notices = quadrant_notices(text)
        else:
            notices = treeview_notices(text)
        if not notices:
            raise ValueError(f"{rel} carries no admitted styling surface")
        rows.append({
            "path": rel,
            "family": family,
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
            "expected_footer_lines": [header] + notices,
            "twin": {"kind": twin_kind, "path": rel},
            "ledger_note": (
                "the projection plots labels only, so the validated body "
                "styling is dropped from the outline and disclosed in the "
                "footer (" + ("once per statement" if family == "quadrant-chart"
                              else "once per source") +
                "); the style-stripped twin renders identical art"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": ("a body `classDef <name> <closed props>` statement, a "
                      "`:::class` suffix on a quadrant data point or a "
                      "tree-view node, and the closed inline point-style "
                      "suffix (`radius`, `color`, `stroke-color`, "
                      "`stroke-width`)"),
            "footer": {"quadrant_header": QUADRANT_HEADER,
                       "treeview_header": TREE_HEADER,
                       "line_limit_bytes": 128},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the twin is the same fixture with the styling "
                          "surface removed; the art above the footer must be "
                          "cell-equal"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("the last body-style fallbacks of quadrant-chart and "
                     "tree-view, whose only non-admitted inputs are the "
                     "closed body styling surface"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {"total": len(rows),
                   "by_family": {family: sum(1 for row in rows
                                             if row["family"] == family)
                                 for family in dict.fromkeys(r[1] for r in COHORT)}},
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
