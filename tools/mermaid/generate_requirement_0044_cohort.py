#!/usr/bin/env python3
"""Generate the RFC 0044 requirement `layout: elk` admission cohort ledger.

Renderer-free. The 23 rows are the requirement fallbacks whose only unsupported
config leaf is `layout: elk` (plus an already-admitted `htmlLabels: false` on
eight of them) and whose bodies are byte-identical to an already-drawn non-ELK
row. The native requirement projection is a text tree and draws no geometry, so
the ELK engine selection is inert; the footer says the engine was not applied.

Twin selection rule (pinned): prefer a footer-free drawn twin (a drawn sibling
whose frontmatter carries no `config:` block, so it emits no notices); otherwise
use a drawn twin whose footer block is a byte-prefix of the source footer (the
three rows whose only drawn siblings carry `htmlLabels: false`). The chosen
twins are pinned literally; the generator re-derives them and fails on drift.

`expected_states.narrow_40` is measured live (rule 34: the footer changes the
whole-surface 40-column predicate) and never inherited from the twin.

Usage: python3 tools/mermaid/generate_requirement_0044_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/requirement-0044-cohort.json")
SCHEMA = "loom_mermaid_requirement_0044_cohort/v1"
PHASE = "P1-layout"

HEADER = "Requirement native projection v1"
NOTICE_LIMIT = 96

# The live gate's admitted leaves and their closed tables (RFC 0037 + D-066 F0
# + RFC 0044). The new leaf is appended last in the footer order.
LEAF_ORDER = (
    ("config.logLevel", {"0", "1", "2"}, "config.logLevel={} -> logger only; not applied"),
    ("config.fontFamily", {"courier"},
     'config.fontFamily="courier" -> terminal font; not applied'),
    ("config.htmlLabels", {"false", "true"},
     "config.htmlLabels={} -> native text labels; not applied"),
    ("config.look", {"classic", "neo", "handDrawn"},
     "config.look={} -> source node style; not applied"),
    ("config.theme", {"default", "neo", "neo-dark", "redux", "redux-dark", "redux-color",
                      "redux-dark-color"},
     "config.theme={} -> colour palette; not applied"),
    ("config.layout", {"elk"}, "config.layout={} -> external layout engine; not applied"),
)

COHORT_DIR = "requirement/nightly-check"

# Reviewed literal path set, 23 rows.
COHORT = [
    "a-not-so-simple-requirement-diagram-elk-layout.mmd",
    "a-not-so-simple-requirement-diagram-without-htmllabels-elk-layout.mmd",
    "a-requirement-diagram-with-bt-direction-elk-layout.mmd",
    "a-requirement-diagram-with-empty-information-elk-layout.mmd",
    "a-requirement-diagram-with-lr-direction-elk-layout.mmd",
    "a-requirement-diagram-with-rl-direction-elk-layout.mmd",
    "a-requirement-diagram-with-tb-direction-elk-layout.mmd",
    "a-simple-requirement-diagram-elk-layout.mmd",
    "a-simple-requirement-diagram-without-htmllabels-elk-layout.mmd",
    "requirements-and-elements-with-and-without-information-elk-layout.mmd",
    "requirements-and-elements-with-long-and-short-text-elk-layout.mmd",
    "requirements-and-elements-with-long-and-short-text-without-htmllabels-elk-layout.mmd",
    "requirements-and-elements-with-markdown-text-elk-layout.mmd",
    "requirements-and-elements-with-markdown-text-without-htmllabels-elk-layout.mmd",
    "requirements-and-elements-with-quoted-text-for-spaces-elk-layout.mmd",
    "requirements-and-elements-with-styles-applied-from-class-statement-elk-layout.mmd",
    "requirements-and-elements-with-styles-applied-from-class-statement-without-htmllabels-elk-layout.mmd",
    "requirements-and-elements-with-styles-applied-from-classes-with-shorthand-syntax-elk-layout.mmd",
    "requirements-and-elements-with-styles-applied-from-classes-with-shorthand-syntax-without-htmllabels-elk-layout.mmd",
    "requirements-and-elements-with-styles-applied-from-style-statement-elk-layout.mmd",
    "requirements-and-elements-with-styles-applied-from-style-statement-without-htmllabels-elk-layout.mmd",
    "requirements-and-elements-with-styles-applied-from-the-default-class-and-other-styles-elk-layout.mmd",
    "requirements-and-elements-with-styles-applied-from-the-default-class-and-other-styles-without-htmllabels-elk-layout.mmd",
]

# Reviewed twin mapping: (twin path, kind). Three rows have no footer-free twin.
PINNED_TWINS = {
    "a-not-so-simple-requirement-diagram-elk-layout.mmd":
        ("a-not-so-simple-requirement-diagram-default.mmd", "footer-free"),
    "a-not-so-simple-requirement-diagram-without-htmllabels-elk-layout.mmd":
        ("a-not-so-simple-requirement-diagram-default.mmd", "footer-free"),
    "a-requirement-diagram-with-bt-direction-elk-layout.mmd":
        ("a-requirement-diagram-with-bt-direction-default.mmd", "footer-free"),
    "a-requirement-diagram-with-empty-information-elk-layout.mmd":
        ("a-requirement-diagram-with-empty-information-default.mmd", "footer-free"),
    "a-requirement-diagram-with-lr-direction-elk-layout.mmd":
        ("a-requirement-diagram-with-lr-direction-default.mmd", "footer-free"),
    "a-requirement-diagram-with-rl-direction-elk-layout.mmd":
        ("a-requirement-diagram-with-rl-direction-default.mmd", "footer-free"),
    "a-requirement-diagram-with-tb-direction-elk-layout.mmd":
        ("a-requirement-diagram-with-tb-direction-default.mmd", "footer-free"),
    "a-simple-requirement-diagram-elk-layout.mmd":
        ("a-simple-requirement-diagram-default.mmd", "footer-free"),
    "a-simple-requirement-diagram-without-htmllabels-elk-layout.mmd":
        ("a-simple-requirement-diagram-default.mmd", "footer-free"),
    "requirements-and-elements-with-and-without-information-elk-layout.mmd":
        ("requirements-and-elements-with-and-without-information-default.mmd",
         "footer-free"),
    "requirements-and-elements-with-long-and-short-text-elk-layout.mmd":
        ("requirements-and-elements-with-long-and-short-text-default.mmd", "footer-free"),
    "requirements-and-elements-with-long-and-short-text-without-htmllabels-elk-layout.mmd":
        ("requirements-and-elements-with-long-and-short-text-default.mmd", "footer-free"),
    "requirements-and-elements-with-markdown-text-elk-layout.mmd":
        ("requirements-and-elements-with-markdown-text-default.mmd", "footer-free"),
    "requirements-and-elements-with-markdown-text-without-htmllabels-elk-layout.mmd":
        ("requirements-and-elements-with-markdown-text-default.mmd", "footer-free"),
    "requirements-and-elements-with-quoted-text-for-spaces-elk-layout.mmd":
        ("requirements-and-elements-with-quoted-text-for-spaces-default.mmd",
         "footer-free"),
    "requirements-and-elements-with-styles-applied-from-class-statement-elk-layout.mmd":
        ("requirements-and-elements-with-styles-applied-from-class-statement-default.mmd",
         "footer-free"),
    "requirements-and-elements-with-styles-applied-from-class-statement-without-htmllabels-elk-layout.mmd":
        ("requirements-and-elements-with-styles-applied-from-class-statement-without-htmllabels-default.mmd",
         "footer-prefix"),
    "requirements-and-elements-with-styles-applied-from-classes-with-shorthand-syntax-elk-layout.mmd":
        ("requirements-and-elements-with-styles-applied-from-classes-with-shorthand-syntax-default.mmd",
         "footer-free"),
    "requirements-and-elements-with-styles-applied-from-classes-with-shorthand-syntax-without-htmllabels-elk-layout.mmd":
        ("requirements-and-elements-with-styles-applied-from-classes-with-shorthand-syntax-default.mmd",
         "footer-free"),
    "requirements-and-elements-with-styles-applied-from-style-statement-elk-layout.mmd":
        ("requirements-and-elements-with-styles-applied-from-style-statement-default.mmd",
         "footer-free"),
    "requirements-and-elements-with-styles-applied-from-style-statement-without-htmllabels-elk-layout.mmd":
        ("requirements-and-elements-with-styles-applied-from-style-statement-without-htmllabels-default.mmd",
         "footer-prefix"),
    "requirements-and-elements-with-styles-applied-from-the-default-class-and-other-styles-elk-layout.mmd":
        ("requirements-and-elements-with-styles-applied-from-the-default-class-and-other-styles-default.mmd",
         "footer-free"),
    "requirements-and-elements-with-styles-applied-from-the-default-class-and-other-styles-without-htmllabels-elk-layout.mmd":
        ("requirements-and-elements-with-styles-applied-from-the-default-class-and-other-styles-without-htmllabels-default.mmd",
         "footer-prefix"),
}

EXPECTED = {"originals": 172, "drawn": 144, "admitted": 23, "remaining": 5}

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

PHASE_REVIEWS = {
    PHASE: {
        "reviewer_agent": (
            "independent read-only adversarial code review (fresh general-purpose "
            "agent, RFC 0044 requirement elk admission, 2026-11-22; r1 "
            "REQUEST-CHANGES on one MAJOR, fixed, then APPROVE)"),
        "date": "2026-11-22",
        "verdict": "approved",
        "note": (
            "Reviewer ran the live gate and the two family tests. Verified the "
            "Layout bit and indent-2 branch, the append site's height/Work guards "
            "and the styled-row legend order, the three verdict flips (each with a "
            "layout: dagre negative), the matrix branch position, and the twin "
            "composition (footer_lines includes the header; strip then append). "
            "MAJOR fixed before approval: the generator's admits()/scalar mirror was "
            "naive (no indent check, no quote-aware comment/dequote) and would have "
            "misclassified on a corpus refresh; it now mirrors requirement_inert_scalar "
            "and rejects indent != 2, duplicates and namespaced entries, with direct "
            "probes matching every C++ battery verdict and all three --check entries "
            "reproducing byte-identically. Residual (documented, no action): admits() "
            "does not mirror the title ink/width rules, and no 0044 row carries a "
            "title."),
    },
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def strip_frontmatter(text: str) -> str:
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[close + 1:])
    return text


def _strip_comment(value: str) -> str:
    """Mirror er_strip_comment: a quote-aware `#` comment; the kept prefix is
    trimmed of spaces/tabs/CR, the no-comment case is returned untouched."""
    quote = None
    for n, char in enumerate(value):
        if quote is not None:
            if char == quote:
                quote = None
            continue
        if char in ("'", '"'):
            quote = char
            continue
        if char == "#" and (n == 0 or value[n - 1] in " \t"):
            return value[:n].strip(" \t\r")
    return value


def _scalar(raw: str) -> str:
    """Mirror requirement_inert_scalar: comment strip, spaces-only trim, then
    dequote of an outer same-character pair."""
    value = _strip_comment(raw).strip(" ")
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
        value = value[1:-1]
    return value


def frontmatter_leaves(text: str):
    """(dotted path, raw value, indent) triples; None when malformed."""
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    leaves = []
    stack = []
    for index in range(1, len(lines)):
        line = lines[index]
        if line == "---":
            return leaves
        if not line.strip() or line.strip().startswith("#"):
            continue
        match = re.match(r"^(\s*)([A-Za-z_][\w.\-]*)\s*:(.*)$", line)
        if not match:
            continue  # folded payload continuation
        indent = len(match.group(1))
        key = match.group(2)
        value = match.group(3)
        while stack and stack[-1][0] >= indent:
            stack.pop()
        stack.append((indent, key))
        leaves.append((".".join(part for _, part in stack), value, indent))
    return None  # unclosed fence


def footer_lines(leaves) -> list:
    """The renderer's notice block for an admitted frontmatter, or [] when the
    envelope carries no config block (no notices are emitted)."""
    mapping = {path: _scalar(value) for path, value, _ in leaves}
    if "config" not in mapping:
        return []
    lines = [HEADER]
    for path, _, template in LEAF_ORDER:
        if path in mapping:
            lines.append(template.format(mapping[path]))
    return lines if len(lines) > 1 else []


def admits(text: str) -> bool:
    """Source-shape mirror of the live requirement envelope gate."""
    if not text.startswith("---\n"):
        return False
    leaves = frontmatter_leaves(text)
    if leaves is None:
        return False
    top = set()
    fields = set()
    config_seen = False
    for path, raw, indent in leaves:
        parts = path.split(".")
        if len(parts) == 1:
            # parse_envelope rejects a duplicated top-level key.
            if path in top:
                return False
            top.add(path)
            if path == "config":
                if config_seen:
                    return False
                config_seen = True
            elif path not in ("description", "title"):
                return False
            continue
        # Only indent-2 leaves are interpreted; deeper entries are namespaced
        # and rejected, and the block must already be open.
        if not config_seen or len(parts) != 2 or indent != 2:
            return False
        if path in fields:
            return False
        fields.add(path)
        table = next((allowed for leaf, allowed, _ in LEAF_ORDER if leaf == path), None)
        if table is None or _scalar(raw) not in table:
            return False
    if "description" not in top:
        return False
    if config_seen and not fields:
        return False
    return True


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                      for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


def build_review(repo_root):
    review = PHASE_REVIEWS[PHASE]
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

    census = {}
    for key, ledger, field in (("rfc0037", "requirement-0037-cohort.json", "rows"),
                               ("f0", "requirement-f0-cohort.json", "rows"),
                               ("base", "requirement-cohort.json", "rows")):
        parsed = json.loads((repo_root / "tools/mermaid" / ledger).read_text())
        census[key] = len(parsed[field])
    base_selection = json.loads(
        (repo_root / "tools/mermaid" / "requirement-cohort.json").read_text())["selection"]
    # The base cohort's native_draw_count (16) covers the four style rows plus
    # the twelve pre-existing clean rows that no ledger lists.
    drawn = census["rfc0037"] + census["f0"] + base_selection["native_draw_count"]
    originals = base_selection["family_total"]
    totals = {"originals": originals, "drawn": drawn,
              "admitted": len(COHORT),
              "remaining": originals - drawn - len(COHORT)}
    if totals != EXPECTED:
        raise ValueError(f"requirement census drift: {totals} != {EXPECTED}")

    # Candidate twins: body-identical admitted requirement rows with no layout
    # leaf (a layout leaf means it is another cohort row).
    candidates = []
    for path in sorted((repo_root / CASES / "requirement").rglob("*.mmd")):
        rel = path.relative_to(repo_root / CASES).as_posix()
        text = path.read_text()
        if not admits(text):
            continue
        leaves = frontmatter_leaves(text)
        if any(leaf == "config.layout" for leaf, _, _ in leaves):
            continue
        candidates.append((rel, text, leaves))

    rows = []
    for name in COHORT:
        rel = f"{COHORT_DIR}/{name}"
        text = (repo_root / CASES / rel).read_text()
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        leaves = frontmatter_leaves(text)
        mapping = {path: _scalar(raw) for path, raw, _ in leaves}
        if mapping.get("config.layout") != "elk":
            raise ValueError(f"{rel} does not carry layout: elk")
        if not admits(text):
            raise ValueError(f"{rel} is not admitted by the gate shape")
        source_footer = footer_lines(leaves)
        body = strip_frontmatter(text)
        footer_free = None
        prefix = None
        for cand_rel, cand_text, cand_leaves in candidates:
            if cand_rel == rel or strip_frontmatter(cand_text) != body:
                continue
            cand_footer = footer_lines(cand_leaves)
            if not cand_footer:
                footer_free = footer_free or (cand_rel, cand_footer)
            elif cand_footer == source_footer[:-1]:
                prefix = prefix or (cand_rel, cand_footer)
        if footer_free:
            twin_rel, twin_footer = footer_free
            kind = "footer-free"
        elif prefix:
            twin_rel, twin_footer = prefix
            kind = "footer-prefix"
        else:
            raise ValueError(f"{rel} has no admissible drawn twin")
        pinned = PINNED_TWINS[name]
        if (twin_rel, kind) != (f"{COHORT_DIR}/{pinned[0]}", pinned[1]):
            raise ValueError(f"{rel} twin drift: {(twin_rel, kind)} != {pinned}")
        twin_sha = sha256_file(repo_root / CASES / twin_rel)
        if twin_sha != manifest_rows[twin_rel]["sha256"]:
            raise ValueError(f"twin sha drift for {twin_rel}")
        narrow = "drawn" if rel in NARROW_DRAW else "source_fallback"
        rows.append({
            "path": rel,
            "family": "requirement",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": "source_fallback",
            "prediction": "predicted-draw",
            "track_scope": "whole-only",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": narrow,
                "restored_240": "drawn",
            },
            "expected_footer_lines": source_footer,
            "twin": {
                "kind": kind,
                "path": twin_rel,
                "sha256": twin_sha,
                "footer_lines": twin_footer,
            },
            "ledger_note": (
                "inert config.layout leaf admitted by RFC 0044; the native text "
                "projection draws no geometry, the engine is never applied, and "
                "the footer discloses it"),
        })

    return {
        "schema": SCHEMA,
        "rfc": 44,
        "phase": PHASE,
        "admission": {
            "shape": ("description envelope + config subtree carrying only the "
                      "D-066 F0 / RFC 0037 leaves plus exactly one layout: elk"),
            "theme_tables": {path.split(".", 1)[1]: sorted(values)
                             for path, values, _ in LEAF_ORDER},
            "footer": {
                "header": HEADER,
                "leaf_order": [path for path, _, _ in LEAF_ORDER],
                "line_limit_bytes": NOTICE_LIMIT,
            },
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("prefer a footer-free drawn twin (no config block); else a "
                          "drawn twin whose footer block is a byte-prefix of the "
                          "source footer; strip the twin footer, then append the "
                          "source footer"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("requirement fallback rows whose only unsupported config leaf "
                     "is layout: elk and whose body is byte-identical to a drawn "
                     "non-ELK sibling"),
            "census": census,
            "path_pin": [f"{COHORT_DIR}/{name}" for name in COHORT],
            "cohort_digest": cohort_digest(rows),
            "twin_kinds": {
                "footer-free": sum(1 for row in rows if row["twin"]["kind"] == "footer-free"),
                "footer-prefix": sum(1 for row in rows
                                     if row["twin"]["kind"] == "footer-prefix"),
            },
        },
        "counts": {
            "originals": totals["originals"],
            "drawn": totals["drawn"],
            "admitted": totals["admitted"],
            "remaining": totals["remaining"],
        },
        "review": build_review(repo_root),
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
