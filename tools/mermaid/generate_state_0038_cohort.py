#!/usr/bin/env python3
"""Generate the RFC 0038 state-family look/theme admission cohort ledger.

Renderer-free. The cohort is a **reviewed literal path set** pinned here, not a
classifier: the 28/56 split inside the look/theme rows is render-defined (the
existing E2b `state_blocker` detector over-selects by 16 on these rows), so the
set was measured with a render probe
(docs/dev/d400-next-family-reachability.md) and is pinned exactly. The
generator therefore hard-asserts only source-derivable facts:

  * the family is the whole state population (145 originals, both roots);
  * the cohort is the pinned 28 rows, each shape-valid against the closed
    look/theme tables and the E2b admitted leaves;
  * the documented exclusions keep their counts -- `layout: elk` 4,
    namespaced `config.state.*` 4 and the one row whose folded description
    carries a literal tab.

The written/blocked split (80 drawn / 65 fallback after the phase) is a
measured figure recorded in the RFC and the closure doc, never asserted here;
a config-shape classifier cannot derive it. The fallback roster is never read.

One whole-cohort path+sha digest is the `--check` pin; review records are
authored in this file and written into the ledger, never read back, so a
hand-edited ledger fails `--check`.

Twin rule (RFC 0037 restated): the recorded delta is the source footer lines
minus the twin footer lines, compared line-by-line against the live frame.
Corpus twins would take precedence, but at implementation time every
body-equivalent drawn state fixture carries `logLevel`/`fontFamily` disclosure
lines that are not a prefix of the source footer, so all 28 rows use the
synthesized config-free twin.

Usage: python3 tools/mermaid/generate_state_0038_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/state-0038-cohort.json")

SCHEMA = "loom_mermaid_state_0038_cohort/v1"
ROOTS = ("state-diagram", "state-diagram-v2")
PHASE = "P1-look-theme"
ENABLED_PHASES = (PHASE,)

# Pinned population (source-derivable) and the measured family denominator.
EXPECTED_COHORT = 28
EXPECTED_FAMILY = 145
EXCLUDED_EXPECTED = {
    "layout: elk": 4,
    "namespaced config.state.*": 4,
    "description tab": 1,
}

# Closed value tables (RFC 0038 section 1): corpus-bounded.
LOOK_VALUES = ("classic", "neo", "handDrawn")
THEME_VALUES = (
    "default",
    "neo",
    "neo-dark",
    "redux",
    "redux-dark",
    "redux-color",
    "redux-dark-color",
)
ADMITTED_LEAVES = frozenset({"logLevel", "fontFamily", "securityLevel", "look", "theme"})

NOTICE_HEADER = "State native projection v1"

# The measured, reviewed 28-row cohort (19 state-diagram-v2 + 9 state-diagram).
# Blocked only by a frontmatter `config:` carrying `look` and/or `theme`.
COHORT = frozenset({
    "state-diagram-v2/nightly-check/a-simple-state-diagram-classic-look-default-theme.mmd",
    "state-diagram-v2/nightly-check/composite-states-neo-look-neo-dark-theme.mmd",
    "state-diagram-v2/nightly-check/composite-states-neo-look-neo-theme.mmd",
    "state-diagram-v2/nightly-check/composite-states-neo-look-redux-dark-theme.mmd",
    "state-diagram-v2/nightly-check/composite-states-neo-look-redux-theme.mmd",
    "state-diagram-v2/nightly-check/nested-composite-containers-handdrawn-look-redux-color-theme.mmd",
    "state-diagram-v2/nightly-check/nested-composite-containers-handdrawn-look-redux-dark-color-theme.mmd",
    "state-diagram-v2/nightly-check/nested-composite-containers-redux-color-theme.mmd",
    "state-diagram-v2/nightly-check/nested-composite-containers-redux-dark-color-theme.mmd",
    "state-diagram-v2/nightly-check/nested-composite-containers-redux-dark-theme.mmd",
    "state-diagram-v2/nightly-check/nested-composite-containers-redux-theme.mmd",
    "state-diagram-v2/nightly-check/redux-color-theme.mmd",
    "state-diagram-v2/nightly-check/redux-dark-color-theme.mmd",
    "state-diagram-v2/nightly-check/redux-dark-theme.mmd",
    "state-diagram-v2/nightly-check/redux-theme.mmd",
    "state-diagram-v2/nightly-check/special-state-types-neo-look-neo-dark-theme.mmd",
    "state-diagram-v2/nightly-check/special-state-types-neo-look-neo-theme.mmd",
    "state-diagram-v2/nightly-check/special-state-types-neo-look-redux-dark-theme.mmd",
    "state-diagram-v2/nightly-check/special-state-types-neo-look-redux-theme.mmd",
    "state-diagram/nightly-check/a-simple-state-diagram-neo-look-neo-dark-theme.mmd",
    "state-diagram/nightly-check/a-simple-state-diagram-neo-look-neo-theme.mmd",
    "state-diagram/nightly-check/a-simple-state-diagram-neo-look-redux-dark-theme.mmd",
    "state-diagram/nightly-check/a-simple-state-diagram-neo-look-redux-theme.mmd",
    "state-diagram/nightly-check/a-state-with-a-note-neo-look-neo-dark-theme.mmd",
    "state-diagram/nightly-check/a-state-with-a-note-neo-look-neo-theme.mmd",
    "state-diagram/nightly-check/a-state-with-a-note-neo-look-redux-dark-theme.mmd",
    "state-diagram/nightly-check/a-state-with-a-note-neo-look-redux-theme.mmd",
    "state-diagram/nightly-check/should-render-a-simple-state-diagram.mmd",
})

# sha256 over "path\tsource_sha256\n" rows in sorted order; a re-partition of
# the pinned cohort aborts generation.
COHORT_DIGEST = "5e855cf9eae7d82c2864732789835bfb8136646f06b33c321841299cad1593c5"

# Review records: authored here, written into the ledger.
PHASE_REVIEWS = {
    PHASE: {
        "reviewer_agent": "agent:acde9271b5b1810a9 rfc0038-p1-final-review (independent "
                          "adversarial code review; APPROVE; re-derived the 145/28/4/4/1 "
                          "source counts, the 132/61/28 -> 43 and 160-28=132 negative floors, "
                          "and the corpus-twin impossibility; verified the closed tables, "
                          "duplicate/indent/namespace rejection, footer order and E2b "
                          "byte-identity, Work accounting, generator --check, the 5-test gate, "
                          "and zero diffs on every protected file, the manifest and the E2b "
                          "ledger)",
        "date": "2026-11-21",
        "verdict": "approved",
        "note": "Reviewer findings, all applied test-only before the ledger was regenerated: "
                "a quoted theme scalar positive with its dequoted footer, securityLevel added "
                "to the footer-order composition probe (pinning the append-after-existing "
                "rule), extra tab-indent / trailing-comment / mismatched-quote negatives, and "
                "a corrected over-/under-listing comment on the E2b negative. The reviewer "
                "confirmed no production mis-admission path and no assertion on the "
                "render-defined drawn/blocked split.",
    },
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def split_lines(text: str):
    """Split `---`-fenced frontmatter; returns (frontmatter_lines, body) or None."""
    if not text.startswith("---"):
        return None, text
    lines = text.split("\n")
    if lines[0].rstrip() != "---":
        return None, text
    for close in range(1, len(lines)):
        if lines[close].rstrip() == "---":
            return lines[1:close], "\n".join(lines[close + 1:])
    return None, text


def parse_frontmatter(frontmatter):
    """Return (top_keys, config_leaves, error).

    `config_leaves` maps a dotted leaf path to its scalar value, so a
    namespaced subtree (`state.useMaxWidth`) stays visible. A folded or plain
    scalar block (the `description:` payload) is skipped, never read as a leaf.
    """
    top = []
    leaves = {}
    in_block = False
    in_config = False
    stack = []
    for raw in frontmatter:
        if not raw.strip():
            continue
        indent = len(raw) - len(raw.lstrip(" "))
        key, _, value = raw.strip().partition(":")
        value = value.strip()
        if indent == 0:
            in_block = False
            in_config = False
            stack = []
            top.append(key)
            if key == "config":
                in_config = True
            elif key == "description" and (not value or value[0] in ">|"):
                in_block = True
            continue
        if in_block:
            continue
        if not in_config:
            return top, leaves, "malformed envelope: indented value without config"
        if indent not in (2, 4) or (indent == 4 and not stack):
            return top, leaves, "malformed envelope: config indentation outside two levels"
        while stack and indent <= stack[-1][0]:
            stack.pop()
        path = ".".join([entry[1] for entry in stack] + [key])
        stack.append((indent, key))
        if len(stack) > 2:
            return top, leaves, "malformed envelope: config deeper than two levels"
        if not value:
            continue  # parent container
        leaves[path] = value
    return top, leaves, None


def notice_lines(leaves):
    """The bounded disclosure footer the renderer emits, in its fixed order."""
    lines = []
    if "fontFamily" in leaves:
        lines.append('config.fontFamily="courier" -> terminal font; not applied')
    if "logLevel" in leaves:
        lines.append(f"config.logLevel={leaves['logLevel']} -> logger only; not applied")
    if "securityLevel" in leaves:
        lines.append(
            f'config.securityLevel="{leaves["securityLevel"]}" -> no interaction layer; '
            "not applied")
    if "look" in leaves:
        lines.append(f"config.look={leaves['look']} -> source node style; not applied")
    if "theme" in leaves:
        lines.append(f"config.theme={leaves['theme']} -> colour palette; not applied")
    return [NOTICE_HEADER] + lines if lines else []


def description_has_tab(frontmatter):
    """True when the folded/plain description payload carries a literal tab."""
    in_block = False
    for raw in frontmatter:
        if not raw.strip():
            continue
        indent = len(raw) - len(raw.lstrip(" "))
        if indent == 0:
            key, _, value = raw.strip().partition(":")
            value = value.strip()
            in_block = key == "description" and (not value or value[0] in ">|")
            continue
        if in_block and "\t" in raw:
            return True
    return False


def validate_cohort_row(top, leaves):
    """A pinned row must be exactly an admitted-leaf envelope carrying look or theme."""
    if top.count("description") != 1 or "description" not in top:
        raise ValueError("cohort row without a single description envelope")
    if top.count("config") != 1:
        raise ValueError("cohort row without a single config block")
    for key in top:
        if key not in ("description", "config", "title"):
            raise ValueError(f"cohort row with a top-level key outside the envelope: {key}")
    if not ({"look", "theme"} & set(leaves)):
        raise ValueError("cohort row carries neither look nor theme")
    if set(leaves) - ADMITTED_LEAVES:
        raise ValueError(
            "cohort row with a leaf outside the admitted set: "
            + ",".join(sorted(set(leaves) - ADMITTED_LEAVES)))
    if leaves.get("look") is not None and leaves["look"] not in LOOK_VALUES:
        raise ValueError("cohort row look outside the closed table: " + leaves["look"])
    if leaves.get("theme") is not None and leaves["theme"] not in THEME_VALUES:
        raise ValueError("cohort row theme outside the closed table: " + leaves["theme"])


def cohort_digest(entries):
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n" for entry in entries)
    return sha256_bytes(payload.encode())


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    family = sorted(
        path.relative_to(cases).as_posix()
        for root in ROOTS
        for path in (cases / root).rglob("*.mmd"))
    if len(family) != EXPECTED_FAMILY:
        raise ValueError(
            f"state family population drifted: expected {EXPECTED_FAMILY}, got {len(family)}")
    if len(COHORT) != EXPECTED_COHORT:
        raise ValueError(f"pinned cohort drifted: expected {EXPECTED_COHORT}, got {len(COHORT)}")
    missing = sorted(COHORT - set(family))
    if missing:
        raise ValueError("pinned cohort rows are missing from the family: " + ",".join(missing))

    excluded = []
    counts_by_class = {}
    rows = []
    for rel in family:
        frontmatter, _ = split_lines((cases / rel).read_text())
        if frontmatter is None:
            raise ValueError(f"state fixture without a frontmatter envelope: {rel}")
        top, leaves, error = parse_frontmatter(frontmatter)
        if error:
            raise ValueError(f"{rel}: {error}")
        if rel in COHORT:
            validate_cohort_row(top, leaves)
            footer = notice_lines(leaves)
            if not footer:
                raise ValueError(f"cohort row without a disclosure footer: {rel}")
            rows.append({
                "path": rel,
                "family": "state",
                "phase": PHASE,
                "sha256": sha256_bytes((cases / rel).read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "prior_status": manifest_rows[rel]["raw"],
                "twin": {"kind": "synthetic"},
                "expected_prepend_lines": [],
                # The synthetic config-free twin carries no footer, so the
                # whole footer is an append after one blank separator line.
                "expected_footer_lines": [""] + footer,
                "prediction": "predicted-draw",
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
                "ledger_note": (
                    "RFC 0038 config.look/config.theme admission; validated and disclosed "
                    "only, the frame equals the config-free twin's plus the bounded footer."),
            })
            continue
        # Documented exclusions, classified from source shape only.
        if "layout" in leaves:
            excluded.append({"path": rel, "class": "layout: elk", "reason": "layout leaf"})
        elif any(key.startswith("state.") for key in leaves):
            excluded.append({"path": rel, "class": "namespaced config.state.*",
                             "reason": "namespaced state subtree stays closed"})
        elif description_has_tab(frontmatter):
            excluded.append({"path": rel, "class": "description tab",
                             "reason": "description payload carries a literal tab"})
        else:
            excluded.append({"path": rel, "class": "outside the reviewed cohort",
                             "reason": "render-defined remainder: body capability or an "
                                       "unadmitted value"})

    for entry in excluded:
        counts_by_class[entry["class"]] = counts_by_class.get(entry["class"], 0) + 1
    for name, expected in EXCLUDED_EXPECTED.items():
        if counts_by_class.get(name, 0) != expected:
            raise ValueError(
                f"excluded class {name!r} drifted: expected {expected}, "
                f"got {counts_by_class.get(name, 0)}")
    if len(rows) != EXPECTED_COHORT:
        raise ValueError(f"cohort rows drifted: expected {EXPECTED_COHORT}, got {len(rows)}")

    rows.sort(key=lambda row: row["path"])
    digest = cohort_digest(rows)
    if COHORT_DIGEST not in ("PENDING", digest):
        raise ValueError(
            f"RFC 0038 cohort row set drifted: digest {digest} != pinned {COHORT_DIGEST}")

    return {
        "schema": SCHEMA,
        "rfc": 38,
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": "stateDiagram(-v2) fallback row whose frontmatter is a folded "
                    "description envelope whose config block carries only the E2b "
                    "admitted leaves plus the closed look and theme tables",
            "roots": list(ROOTS),
            "phase": PHASE,
            "cohort_pin": "reviewed literal path set (render-defined split); one atomic "
                          "phase admitting both leaves",
            "cohort_digest": digest,
            "twin_rule": "the frame must equal the synthesized config-free twin's frame "
                         "plus the recorded footer delta, line by line",
            "corpus_twins": "none: every body-equivalent drawn state fixture carries "
                            "logLevel/fontFamily disclosure lines that are not a prefix of "
                            "the source footer, so all 28 rows use the config-free twin",
            "exclusions": "layout: elk (4), namespaced config.state.* (4), the folded "
                          "description carrying a literal tab (1); the remaining rows are "
                          "body-capability fallbacks",
            "width_policy": "wide_240 admission only; narrow_40 stays 'source'.",
        },
        "review": {
            "phases": PHASE_REVIEWS,
            "verdict": "approved" if all(
                phase in PHASE_REVIEWS and PHASE_REVIEWS[phase]["verdict"] == "approved"
                for phase in ENABLED_PHASES) else "pending",
        },
        "counts": {
            "total": len(rows),
            "predicted_draw": len(rows),
            "family": len(family),
            "state_v2": sum(1 for row in rows if row["path"].startswith("state-diagram-v2/")),
            "state_v1": sum(1 for row in rows if row["path"].startswith("state-diagram/")),
        },
        "excluded": excluded,
        "rows": rows,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
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
    print("cohort digest", ledger["selection"]["cohort_digest"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
