#!/usr/bin/env python3
"""Generate the RFC 0031 D-211 agentflow-family cohort ledger.

The agentflow renderer (mermaid_agentflow_impl.cpp) parses the
agentflow-beta graph grammar -- connector/flow/global declarations,
metadata-bearing nodes (`@{ ... }`), nested indentation containers and four
edge vocabularies (`-->`, `-.-`, `--x`, labeled branches) -- and projects a
bounded native outline. It never approximates an unrecognized source as a
flowchart: unknown statements, shapes, metadata keys or unresolved edge
endpoints reject the whole source.

The remaining gate is frontmatter-only: a strict column-0 `---` fence whose
YAML block carries any top-level key besides `description`, or a `config:`
block whose sole leaf is not an indent-2 in-table `theme`, keeps the whole
source on fallback. RFC 0041 admits the seven corpus theme values (disclosed,
never applied) and promotes 9 rows. The other 2 rows carry
`look` + `handDrawnSeed`; D-442 admits that closed inert pair in the sibling
ledger, so the family records them as **delegated**, not gated.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the four-way family partition (38 total / 27 native rows / 9 admitted /
0 gated / 2 delegated to the reviewed D-442 ledger) and the 27-row cohort;
manifest shas are pinned; the review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_agentflow_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/agentflow")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/agentflow-cohort.json")
SCHEMA = "loom_mermaid_agentflow_cohort/v1"
_FENCE = re.compile(r"^---$")

# Live-measured per-width geometry (engineering rule 18): these eight native
# fixtures exceed the 40-column native surface and fall back there, on both
# whole and body tracks. The other nineteen draw at 40 columns. Edited only
# after a live re-measure.
_NARROW_FALLBACK = {
    "agentflow/nightly-check/4b-should-render-the-input-value-pattern.mmd",
    "agentflow/nightly-check/8-should-redirect-cross-boundary-edges-to-the-collapsed-parent.mmd",
    "agentflow/visual-check/06-collapsing-a-container.mmd",
    "agentflow/visual-check/13-render-the-input-value-pattern.mmd",
    "agentflow/visual-check/11-redirect-cross-boundary-edges-to-the-collapsed-parent.mmd",
    "agentflow/visual-check/15-support-triage.mmd",
    "agentflow/visual-check/08-a-worked-example.mmd",
    "agentflow/pr-check/6-should-render-the-coffee-website-builder-diagram.mmd",
}

# Hard drift-guard tuple, measured through the production renderer.
_FAMILY_TOTAL = 38
_NATIVE_DRAW = 27
_ADMITTED = 9
_GATED = 0
_DELEGATED = 2

# The reviewed sibling ledger that admits the two closed look + seed rows
# (D-442). The family gate still rejects their shape; the partition records
# them as delegated so the family ledger cannot claim they are unrendered.
DELEGATED_LEDGER = pathlib.Path(
    "tools/mermaid/agentflow-handdrawn-d442-cohort.json")

# RFC 0041 closed theme table (the corpus); any other value keeps fallback.
_THEME_TABLE = ("base", "dark", "default", "forest", "neutral", "redux-color",
                "redux-dark-color")

# Measured 40-column draws among the admitted rows: none. The disclosure
# footer's longest line exceeds a 40-column viewport, so the renderer keeps
# the whole source there (the corpus twin draws because it has no footer).
_ADMITTED_NARROW_DRAW = set()


def split_frontmatter(text):
    """Return (frontmatter_lines_or_None, malformed_bool)."""
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, False
    for close in range(1, len(lines)):
        if _FENCE.fullmatch(lines[close]):
            return lines[1:close], False
        if lines[close].strip() == "---":
            return None, True  # indented fence cannot close the envelope
    return None, True  # unclosed fence


def _scalar(raw):
    """Mirror the C++ reader: trim, strip a trailing comment, dequote."""
    value = raw.strip(" \t")
    quote = ""
    cut = len(value)
    for index, char in enumerate(value):
        if quote:
            if char == quote:
                quote = ""
            continue
        if char in "'\"":
            quote = char
        elif char == "#" and (index == 0 or value[index - 1] in " \t"):
            cut = index
            break
    value = value[:cut].strip(" \t")
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "'\"":
        value = value[1:-1]
    return value


def classify(frontmatter, malformed):
    """Return (kind, leaves); kind is "drawn", "admitted" or "gated".

    Mirrors the RFC 0041 C++ gate: indented lines are interpreted only inside
    a `config:` block, where the sole admitted leaf is an indent-2 `theme` in
    the closed table; every other shape keeps today's whole-source fallback.
    """
    if malformed:
        return "gated", {}
    if frontmatter is None:
        return "drawn", {}
    in_config = False
    saw_config = False
    theme = None
    for line in frontmatter:
        # Tab indentation anywhere in the block rejects the envelope.
        leading = line[: len(line) - len(line.lstrip(" \t"))]
        if "\t" in leading:
            return "gated", {}
        key_start = next((i for i, c in enumerate(line) if c != " "), None)
        if key_start is None:
            continue  # blank line
        if key_start != 0:
            # Indented lines count only inside the config block; outside it
            # they stay ignored exactly as before.
            if in_config:
                leaf = line[key_start:]
                if not leaf.startswith("#"):
                    if ":" not in leaf:
                        return "gated", {}
                    if leaf.split(":", 1)[0] != "theme" or key_start != 2 or theme:
                        return "gated", {}
                    value = _scalar(leaf.split(":", 1)[1])
                    if value not in _THEME_TABLE:
                        return "gated", {}
                    theme = value
            continue
        column0 = line[key_start:]
        if column0.startswith("#"):
            continue  # YAML comment
        # Mirror the C++ gate: a column-0 non-comment line needs a colon.
        if ":" not in column0:
            return "gated", {}
        key = column0.split(":", 1)[0].strip()
        if key == "description":
            in_config = False
        elif key == "config":
            if saw_config:
                return "gated", {}
            saw_config = True
            in_config = True
        else:
            return "gated", {}
    if saw_config and theme is None:
        return "gated", {}
    return ("admitted", {"theme": theme}) if theme else ("drawn", {})


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_delegated(repo_root, gated):
    """Rows the family frontmatter gate rejects but a sibling cohort admits.

    The only such cohort is the reviewed D-442 `look: handDrawn` ledger. It is
    the single source of truth for that admission: this family generator must
    not duplicate the shape parsing, and must fail closed when the sibling
    ledger is missing, unapproved, pins different bytes, or names a row the
    family gate does not reject.
    """
    ledger_path = repo_root / DELEGATED_LEDGER
    ledger = json.loads(ledger_path.read_text())
    if ledger.get("schema") != "loom_mermaid_agentflow_handdrawn_d442_cohort/v1":
        raise ValueError(f"unexpected delegated ledger schema: {ledger_path}")
    review = ledger.get("review", {})
    if review.get("verdict") != "approved" or not review.get("reviewer_agent"):
        raise ValueError("delegated ledger is not approved by a named reviewer")
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_shas = {row["path"]: row["sha256"] for row in manifest["fixtures"]}
    delegated = set()
    for row in ledger.get("rows", []):
        rel = row["path"]
        if rel not in gated:
            raise ValueError(
                f"delegated ledger names a row the family gate does not reject: {rel}")
        if row["expected_states"]["wide_240"] != "drawn":
            raise ValueError(f"delegated row is not recorded as drawn: {rel}")
        observed = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed != row.get("sha256") or observed != manifest_shas.get(rel):
            raise ValueError(f"delegated row sha drift: {rel}")
        delegated.add(rel)
    if len(delegated) != _DELEGATED:
        raise ValueError(
            f"delegated cohort drifted: {len(delegated)} rows, expected {_DELEGATED}")
    return delegated


def build_review(repo_root):
    review = {
        "reviewer_agent": "pending independent in-session adversarial review",
        "date": "2026-11-14",
        "verdict": "pending",
        "note": ("Filled by the independent review agent; preserved across "
                 "regeneration once approved."),
    }
    existing = repo_root / LEDGER
    if existing.exists():
        try:
            prior = json.loads(existing.read_text())
            if isinstance(prior.get("review"), dict) and \
                    prior["review"].get("verdict") == "approved":
                review = prior["review"]
        except json.JSONDecodeError:
            pass
    return review


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    draw, admitted, gated = set(), set(), set()
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        text = path.read_text()
        rel = path.relative_to(
            repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        frontmatter, malformed = split_frontmatter(text)
        kind, _ = classify(frontmatter, malformed)
        {"drawn": draw, "admitted": admitted, "gated": gated}[kind].add(rel)

    # The family gate still rejects the D-442 look+seed shape; the reviewed
    # sibling ledger owns its admission, so subtract those rows from `gated`
    # and record them as delegated.
    delegated = load_delegated(repo_root, gated)
    gated -= delegated

    if (len(draw), len(admitted), len(gated), len(delegated)) != (
            _NATIVE_DRAW, _ADMITTED, _GATED, _DELEGATED) or \
            len(draw) + len(admitted) + len(gated) + len(delegated) != _FAMILY_TOTAL:
        raise ValueError(
            f"agentflow partition drift: draw={len(draw)} admitted={len(admitted)} "
            f"gated={len(gated)} delegated={len(delegated)} "
            f"total={len(draw) + len(admitted) + len(gated) + len(delegated)} "
            f"(expected {_NATIVE_DRAW}/{_ADMITTED}/{_GATED}/{_DELEGATED}/"
            f"{_FAMILY_TOTAL})")
    unknown_narrow = _NARROW_FALLBACK - draw
    if unknown_narrow:
        raise ValueError(f"narrow set references non-drawing fixtures: {unknown_narrow}")
    if not _ADMITTED_NARROW_DRAW <= admitted:
        raise ValueError("admitted narrow set references non-admitted fixtures")
    admitted_digest = hashlib.sha256("".join(
        f"{rel}\t{sha256_file(repo_root / 'tests/fixtures/mermaid_official/cases' / rel)}\n"
        for rel in sorted(admitted)).encode()).hexdigest()

    rows = []
    for rel in sorted(draw):
        observed_sha = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        narrow = "source" if rel in _NARROW_FALLBACK else "drawn"
        rows.append({
            "path": rel,
            "family": "agentflow",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": "whole-and-body",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": narrow,
                "restored_240": "drawn",
            },
            # Description-only frontmatter strips to a body that renders via
            # the same grammar path with identical width behavior.
            "expected_body_states": {
                "wide_240": "drawn",
                "narrow_40": narrow,
                "restored_240": "drawn",
            },
            "ledger_note": (
                "agentflow renderer validates declarations, metadata and "
                "edges against the closed grammar and transcribes a bounded "
                "outline; no config/theme frontmatter gate is present so it "
                "draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "agentflow-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_agentflow_impl.cpp",
            "grammar": (
                "agentflow-beta header; connector/flow/global declarations; "
                "id[\"label\"]/id(\"label\") nodes; @{ shape/view/value/params/"
                "returns/protocol/endpoint/token_required/connectorRef/"
                "instruction/model } metadata; -->, -.-, --x and labeled edges"),
            "theme_gate": (
                "RFC 0041: a column-0 `config:` block whose sole leaf is an "
                "indent-2 `theme` in the closed corpus table is validated and "
                "disclosed; any other key, an indented/unclosed fence, a "
                "second config line, or a config leaf outside that shape keeps "
                "the whole original on source fallback"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": (
                "agentflow fixtures whose frontmatter is absent, description-only, "
                "or a single config block whose sole leaf is an admitted theme; "
                "the 27 native originals are cohort rows and RFC 0041 promotes "
                "the 9 theme-only rows through the sibling ledger; the 2 closed "
                "look + seed rows are admitted by the reviewed D-442 ledger and "
                "are recorded here as delegated, not gated"),
            "roots": ["agentflow"],
            "family_total": _FAMILY_TOTAL,
            "native_draw_count": _NATIVE_DRAW,
            "admitted_count": _ADMITTED,
            "admitted": sorted(admitted),
            "admitted_digest": admitted_digest,
            "gated_count": _GATED,
            "gated": sorted(gated),
            "delegated_count": _DELEGATED,
            "delegated": sorted(delegated),
            "delegated_ledger": DELEGATED_LEDGER.as_posix(),
            "narrow_40_draw_count": _NATIVE_DRAW - len(_NARROW_FALLBACK),
            "narrow_40_fallback_count": len(_NARROW_FALLBACK),
        },
        "review": build_review(repo_root),
        "counts": {
            "total": len(rows),
            "predicted_draw": len(rows),
            "predicted_blocked": 0,
            "predicted_original_draw": len(rows),
        },
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
