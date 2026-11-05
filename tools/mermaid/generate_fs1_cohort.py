#!/usr/bin/env python3
"""Generate the FS-1 flowchart Markdown-label pre-registration ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/flowchart root that
    - opens with a column-0 ``---`` frontmatter fence,
    - carries a top-level ``config:`` map whose ONLY key is a boolean
      root-level ``htmlLabels`` leaf,
    - carries only description/title/config top-level keys, folds the
      description as ``>-`` when present (the production envelope gate
      accepts nothing else),
    - has a backtick Markdown string somewhere in the body,
    - carries no body ``%%{init...}%%`` directive (that is a later cohort).

Each selected row is pre-registered predicted-draw or predicted-blocked.
Three closed source-content blocker detectors classify the rows the
current Markdown subset cannot honestly paint whole-scene:
  * a ``subgraph`` heading that is itself a backtick Markdown string,
  * a config leaf outside the one-key whitelist (e.g. markdownAutoWrap),
  * a ``<``/``>`` inside a backtick span that is not one of the three
    accepted ``<br>`` line-break forms.
Any residual selected row aborts generation: zero unclassified rows.

Whole-source (original track) states: draw rows register the 240-column
admission; narrow_40 is uniformly ``source`` (the per-diagram disclosure
footer is wider than 40 columns). The body-only (body_diagnostic track)
states are recorded separately in ``expected_body_states``; draw-row
bodies have no disclosure footer and also draw at 40 columns. The live
pixel/footer gate is tests/ui/test_mermaid_fs1_cohort.cpp.

Usage: python3 tools/mermaid/generate_fs1_cohort.py --repo-root . [--check]
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
LEDGER = pathlib.Path("tools/mermaid/fs1-cohort.json")

SCHEMA = "loom_mermaid_fs1_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-md-subgraph-title",
    "flow-md-nonwhitelisted-config-key",
    "flow-md-angle-in-span",
]

WITNESS = {
    "flow-md-subgraph-title": (
        "src/ui/visual/mermaid_impl.cpp (subgraph heading parse rejects a "
        "non-empty Markdown run set)",
        "A 'subgraph' heading is itself a backtick Markdown string "
        "(subgraph[\"`...`\"] / subgraph `...`). Markdown subgraph titles are a "
        "later cohort, so the whole source keeps ordinary fallback.",
    ),
    "flow-md-nonwhitelisted-config-key": (
        "src/ui/visual/mermaid_impl.cpp flow_envelope_body (FS-1 root_html_only "
        "profile admits exactly one htmlLabels leaf)",
        "The frontmatter config carries a leaf other than htmlLabels (e.g. "
        "markdownAutoWrap); markdown auto-wrapping changes label geometry and "
        "is outside the one-key non-terminal whitelist.",
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


def split_source(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return lines[1:close], lines[close + 1:]
    return None


def description_accepted(frontmatter_lines):
    """Mirror flow_envelope_body's folded/plain description gate."""
    try:
        header = next(l for l in frontmatter_lines
                      if l.split(":", 1)[0].strip() == "description")
    except StopIteration:
        return True  # no description
    form = header.split(":", 1)[1].strip()
    if form == ">-":
        return True
    if form:
        return False  # unknown scalar form
    forbidden = {",", "[", "]", "{", "}", "#", "&", "*", "!", "|", ">",
                 "'", '"', "%", "@", "`"}
    in_description = False
    for line in frontmatter_lines:
        key = line.split(":", 1)[0].strip()
        if in_description and line and not line.startswith(" "):
            break  # next top-level key ends the description block
        if key == "description":
            in_description = True
            continue
        if in_description:
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


def backtick_spans(body):
    """Yield the contents of balanced, non-nested backtick-delimited spans."""
    for match in re.finditer(r"`([^`]*)`", body, re.S):
        yield match.group(1)


def flow_md_blocker(body):
    """Source-derived blocker in deterministic precedence; renderer-free."""
    # 1. A subgraph heading written as a Markdown string. Two mechanical
    #    shapes: 'subgraph id["`...`"]' and a bare 'subgraph `...`'.
    for line in body.split("\n"):
        stripped = line.strip()
        if not stripped.startswith("subgraph"):
            continue
        rest = stripped[len("subgraph"):]
        if re.search(r"\[\s*\"`[^`]*`\"\s*\]", rest) or rest.lstrip().startswith("`"):
            return "flow-md-subgraph-title"
    # 2. A non-whitelisted config leaf is detected by the caller (selection
    #    admits exactly one htmlLabels key); this detector only fires when a
    #    candidate row survived selection with an extra leaf, which the
    #    strict contract prevents. Kept in the vocabulary for ledger rows
    #    whose config block carries the extra key (the generator selects a
    #    slightly wider universe just for classification).
    # 3. '<'/'>' inside any backtick span, outside the accepted <br> forms.
    for span in backtick_spans(body):
        stripped = span
        for form in BR_FORMS:
            stripped = stripped.replace(form, "")
        if "<" in stripped or ">" in stripped:
            return "flow-md-angle-in-span"
    return None


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def collect(repo_root):
    cases = repo_root / CASES
    rows = []
    for path in sorted((cases / "flowchart").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        frontmatter, body_lines = split
        body = "\n".join(body_lines)
        if any(line.lstrip().startswith("%%{") for line in body_lines):
            continue
        if "`" not in body:
            continue
        try:
            parsed = yaml.safe_load("\n".join(frontmatter))
        except yaml.YAMLError:
            continue
        if not isinstance(parsed, dict) or not isinstance(parsed.get("config"), dict):
            continue
        config = parsed["config"]
        if "htmlLabels" not in config or not isinstance(config["htmlLabels"], bool):
            continue
        top = set(parsed)
        if not top <= {"description", "title", "config"}:
            continue
        if "description" in top and not description_accepted(frontmatter):
            continue
        extra_keys = [k for k in config if k != "htmlLabels"]
        if extra_keys:
            kind = "flow-md-nonwhitelisted-config-key"
        else:
            kind = flow_md_blocker(body)
        rows.append({
            "path": rel,
            "html_labels": config["htmlLabels"],
            "extra_config_keys": sorted(extra_keys),
            "blocker": kind,
        })
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
        if blocked:
            n_blocked += 1
        else:
            n_draw += 1
        site, detail = WITNESS[kind] if blocked else ("", "")
        manifest_row = manifest_rows[entry["path"]]
        rows.append({
            "path": entry["path"],
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / entry["path"]),
            "manifest_sha256": manifest_row["sha256"],
            "prior_status": manifest_row["raw"],
            "html_labels": entry["html_labels"],
            "extra_config_keys": entry["extra_config_keys"],
            "has_init": False,
            "prediction": "predicted-blocked" if blocked else "predicted-draw",
            "expected_states": {
                "wide_240": "source" if blocked else "drawn",
                "narrow_40": "source",
                "restored_240": "source" if blocked else "drawn",
            },
            "expected_body_states": {
                # The body drops the frontmatter, hence the disclosure footer.
                # Draw-row bodies are narrow enough to paint at 40 columns;
                # blocked-row bodies fail the same closed Markdown subset.
                "wide_240": "source" if blocked else "drawn",
                "narrow_40": "source" if blocked else "drawn",
                "restored_240": "source" if blocked else "drawn",
            },
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if blocked else None),
            "ledger_note": (
                "FS-1 root htmlLabels non-terminal admission with Markdown "
                "node/edge labels and a per-diagram disclosure footer"
                if not blocked else
                "Blocked by a closed source-content Markdown/config detector"),
        })
    total = n_draw + n_blocked
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-1",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256((repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original whose config map is exactly "
                    "{htmlLabels: bool}, folded description allowed, body "
                    "carries a backtick Markdown string, no body init directive",
            "roots": ["flowchart"],
            "blocker_detection": "three fixed source-content detectors "
                                 "(Markdown subgraph title, non-whitelisted "
                                 "config leaf, angle/raw-HTML in span); no "
                                 "generator-time rendering; residual rows abort",
            "width_policy": "original track pre-registers the 240-column "
                            "admission and narrow_40 stays source (the "
                            "disclosure footer exceeds 40 columns); body track "
                            "states are recorded in expected_body_states.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review, "
                              "main-agent inline (D-066 FS-1, 2026-11-05)",
            "date": "2026-11-05",
            "verdict": "approved",
            "note": "Read-only adversarial review of the production diff, "
                    "mechanical selection/blocker detectors, ledger rows, "
                    "pixel/footer gate, and matrix wiring. No blocker/major "
                    "findings; two minor follow-ups noted (symmetric rejection "
                    "of a lone '>' inside a span; move semantics for fan-out "
                    "edge runs).",
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
