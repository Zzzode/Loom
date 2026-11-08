#!/usr/bin/env python3
"""Generate the RFC 0031 FS-spacing cohort ledger.

`flowchart.nodeSpacing` and `flowchart.rankSpacing` only position dagre
ranks in the upstream SVG layout. The terminal uses its own fixed layout,
so they are non-terminal envelope leaves: a non-negative pixel integer
(bare or quoted) never changes the rendered geometry, and the whole
original is admitted once the envelope whitelist accepts the key.

The pinned four visual-check fixtures have a body that already draws and
a whole original held solely by the missing spacing profile; after
admission both tracks draw at 240/restored and keep ordinary source at
40 columns. The two nightly handDrawn lookalikes also set the keys but
carry `look: handDrawn`, a separate gate (FS-8), so they are not
selected and keep source.

Usage: python3 tools/mermaid/generate_spacing_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/spacing-cohort.json")

SCHEMA = "loom_spacing_cohort/v1"

PINNED = {
    "flowchart/visual-check/22-render-a-simple-flowchart-with-nodespacing-set-to-100.mmd",
    "flowchart/visual-check/23-render-a-simple-flowchart-with-rankspacing-set-to-100.mmd",
    "flowchart/visual-check/3258-should-render-subgraphs-with-large-nodespacing-and-rankspacing.mmd",
    "flowchart/visual-check/3258-should-render-subgraphs-with-main-graph-nodespacing-and-rankspacing.mmd",
}
SPACING_KEYS = {"nodeSpacing", "rankSpacing"}
# Indent-2 leaves already accepted by the envelope walker; any other
# indent-2 key (look/theme/...) is a hard visual gate that keeps source.
# "flowchart" is the indent-2 namespace entry whose indent-4 children hold
# the actual leaves.
SAFE_INDENT2 = {"layout", "look", "fontFamily", "securityLevel", "logLevel",
                "htmlLabels", "title", "description", "config", "flowchart"}
SAFE_INDENT4 = {"htmlLabels", "useMaxWidth", "titleTopMargin",
                "diagramPadding", "nodeSpacing", "rankSpacing"}


def split_frontmatter(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close])
    return None


def _unquote(value):
    scalar = value.strip()
    if len(scalar) >= 2 and scalar[0] == scalar[-1] and scalar[0] in ("'", '"'):
        return scalar[1:-1].strip()
    return scalar


def is_non_negative_int_scalar(value):
    scalar = _unquote(value)
    return bool(scalar) and all("0" <= ch <= "9" for ch in scalar)


def _is_bool_scalar(value):
    return _unquote(value) in ("true", "false")


# Value contracts mirroring flow_envelope_body's indent-2/indent-4 leaves,
# so the selector fails a row whose whitelisted key carries a value the
# production walker would reject. Spacing keys are validated separately.
INDENT2_VALUE_RULES = {
    "layout": lambda v: _unquote(v) == "dagre",
    "look": lambda v: v.strip() == "handDrawn",
    "securityLevel": lambda v: _unquote(v) == "loose",
    "logLevel": lambda v: v.strip() == "1",
    "htmlLabels": _is_bool_scalar,
}
INDENT4_VALUE_RULES = {
    "htmlLabels": _is_bool_scalar,
    "useMaxWidth": _is_bool_scalar,
    "titleTopMargin": lambda v: v.strip() == "0",
    "diagramPadding": lambda v: v.strip() == "0",
}


def spacing_only_profile(frontmatter):
    """True when every config key is an accepted envelope leaf and the
    only new leaves are nodeSpacing/rankSpacing with valid scalar values,
    with no hard indent-2 visual gate. The value rules mirror the
    production walker so an unsupported value (elk layout, strict
    security, non-zero margin, ...) disqualifies the row rather than being
    silently ignored; the corpus drift guard still backs this up."""
    saw_spacing = False
    for raw in frontmatter.splitlines():
        indent2 = re.match(r"^  ([A-Za-z_]+):(.*)$", raw)
        indent4 = re.match(r"^    ([A-Za-z_]+):(.*)$", raw)
        if indent4:
            key, value = indent4.group(1), indent4.group(2)
            if key not in SAFE_INDENT4:
                return False
            if key in SPACING_KEYS:
                if not is_non_negative_int_scalar(value):
                    return False
                saw_spacing = True
            elif key in INDENT4_VALUE_RULES and not INDENT4_VALUE_RULES[key](value):
                return False
        elif indent2:
            key, value = indent2.group(1), indent2.group(2)
            if key not in SAFE_INDENT2:
                return False
            # look/theme are hard visual gates: a handDrawn look or any
            # theme key disqualifies the row even with a spacing key.
            if key in ("look", "theme"):
                return False
            if key in INDENT2_VALUE_RULES and not INDENT2_VALUE_RULES[key](value):
                return False
    return saw_spacing


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, FS-spacing "
                           "envelope cohort, 2026-11-09, task "
                           "aee66c1df9b510318; final cross-cohort-migration "
                           "re-review task a91f43e0c0b606c1d)"),
        "date": "2026-11-09",
        "verdict": "approved",
        "note": (
            "Independent read-only adversarial review (fresh general-"
            "purpose agent, 2026-11-09): APPROVE with no MAJOR findings. "
            "The nodeSpacing/rankSpacing scalar parser and the narrow "
            "spacing_profile bit predicate are fail-closed and additive; "
            "every bypass attempt (sign, fraction, exponent, quote "
            "mismatch, empty, structured, indent-2 placement, look/theme "
            "gate, unknown sibling key) is rejected. Pre-existing envelope "
            "profiles, the body track (170/182/48), Class285 and the "
            "48-gap roster cannot move. Two MINORs (generator review "
            "preservation and selector value mirroring) and four NITs were "
            "resolved before approval. Four whole-and-body rows move whole "
            "967 -> 971 / fallback 1178 -> 1174; Flow 251 -> 247, "
            "admitted/mutated 149 -> 153. Final cross-cohort-migration "
            "re-review (2026-11-09, task a91f43e0c0b606c1d, read-only "
            "adversarial agent, zero builds): APPROVE with zero MAJOR and "
            "zero functional MINOR; three NITs, the stale-count comments "
            "were corrected and the shadowed defensive matrix branches "
            "were proven inert (no run_case, no gap-roster or Class285 "
            "impact)."),
    }
    # Preserve a recorded review across regeneration so re-running the
    # generator cannot silently reset an approved verdict.
    existing_path = repo_root / LEDGER
    if existing_path.exists():
        try:
            prior = json.loads(existing_path.read_text())
            if isinstance(prior.get("review"), dict):
                review = prior["review"]
        except json.JSONDecodeError:
            pass
    return review


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    # Drift guard: independently scan the corpus for every spacing-bearing
    # flowchart and require the selectable set to equal the pinned set.
    selected = set()
    for path in sorted((repo_root / CASES / "flowchart").rglob("*.mmd")):
        frontmatter = split_frontmatter(path.read_text())
        if frontmatter is None:
            continue
        if not re.search(r"nodeSpacing|rankSpacing", frontmatter):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        if spacing_only_profile(frontmatter):
            selected.add(rel)
    if selected != PINNED:
        raise ValueError(
            f"partition drift: selected={sorted(selected)} expected={sorted(PINNED)}")
    rows = []
    for rel in sorted(PINNED):
        observed = sha256_file(repo_root / CASES / rel)
        if observed != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": observed,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": "whole-and-body",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "source",
                "restored_240": "drawn",
            },
            "expected_body_states": {
                "wide_240": "drawn",
                "narrow_40": "source",
                "restored_240": "drawn",
            },
            "ledger_note": (
                "non-terminal nodeSpacing/rankSpacing envelope leaves admitted; "
                "terminal fixed layout ignores dagre rank spacing, so whole "
                "and body render identically at 240/restored and keep source "
                "at 40 columns"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "flowchart-spacing-envelope",
        "whitelist": {
            "leaves": (
                "flowchart.nodeSpacing and flowchart.rankSpacing as a "
                "non-negative pixel integer, bare or single/double quoted; "
                "negative, fractional, non-numeric or structured values fail "
                "closed"),
            "effect": (
                "dagre rank positioning only; no terminal rendering effect, "
                "disclosed through the existing non-terminal envelope path"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": (
                "visual-check flowchart fixture whose only unaccepted "
                "envelope leaves are non-negative nodeSpacing/rankSpacing "
                "scalars, with no look/theme hard gate; the handDrawn "
                "nightly variants stay excluded"),
            "roots": ["flowchart"],
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
