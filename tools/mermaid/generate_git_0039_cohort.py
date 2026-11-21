#!/usr/bin/env python3
"""Generate the RFC 0039 gitGraph look/theme admission cohort ledger.

Renderer-free. The cohort is a **reviewed literal path set** pinned here: the
git family's non-namespaced look/theme shape class has 18 rows, but six of them
do not draw with the frontmatter stripped (a body branch/route-density limit),
so the 12/18 split is render-defined and cannot be classified from source
shape. The generator therefore hard-asserts only source-derivable facts:

  * the family is the whole git population (135 originals);
  * the cohort is the pinned 12 rows, each shape-valid against the closed
    look/theme tables (exactly one description block and one config block,
    indent-2 leaves inside {look, theme}, values inside the closed tables, no
    deeper key);
  * the non-namespaced look/theme shape class is 18 rows and the namespaced
    `config.gitGraph.*` debt is 44 rows.

The written/blocked split (70 drawn / 65 fallback after the phase) is a
measured figure recorded in the RFC and the closure doc, never asserted here.
The fallback roster and the matrix artifact are never read.

One whole-cohort path+sha digest is the `--check` pin (a reviewed generator
constant over manifest-pinned bytes); review records are authored in this file
and written into the ledger, never read back, so a hand-edited ledger fails
`--check`.

Twin rule (RFC 0037/0038 restated): the recorded delta is the source footer
lines minus the twin footer lines, compared line-by-line against the live
frame. No drawn git fixture is body-equivalent to a cohort row, so all 12 rows
use the synthesized config-free twin.

Usage: python3 tools/mermaid/generate_git_0039_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/git-0039-cohort.json")

SCHEMA = "loom_mermaid_git_0039_cohort/v1"
ROOTS = ("git",)
PHASE = "P1-look-theme"
ENABLED_PHASES = (PHASE,)

EXPECTED_COHORT = 12
EXPECTED_FAMILY = 135
EXPECTED_SHAPE_CLASS = 18
EXPECTED_NAMESPACED = 44

# Closed value tables (RFC 0039 section 1): corpus-bounded.
LOOK_VALUES = ("neo",)
THEME_VALUES = ("neo", "neo-dark", "redux", "redux-dark", "redux-color", "redux-dark-color")

NOTICE_HEADER = "GitGraph native projection v1"

# The measured, reviewed 12-row cohort: two bodies x six palettes.
COHORT = frozenset({
    "git/visual-check/all-commit-types-id-tag-and-merge-override-attributes-with-cherry-pick-tag-lr-neo-look-neo-dark-theme.mmd",
    "git/visual-check/all-commit-types-id-tag-and-merge-override-attributes-with-cherry-pick-tag-lr-neo-look-neo-theme.mmd",
    "git/visual-check/all-commit-types-id-tag-and-merge-override-attributes-with-cherry-pick-tag-lr-neo-look-redux-color-theme.mmd",
    "git/visual-check/all-commit-types-id-tag-and-merge-override-attributes-with-cherry-pick-tag-lr-neo-look-redux-dark-color-theme.mmd",
    "git/visual-check/all-commit-types-id-tag-and-merge-override-attributes-with-cherry-pick-tag-lr-neo-look-redux-dark-theme.mmd",
    "git/visual-check/all-commit-types-id-tag-and-merge-override-attributes-with-cherry-pick-tag-lr-neo-look-redux-theme.mmd",
    "git/visual-check/showbranches-false-and-showcommitlabel-false-hiding-branch-lines-and-commit-labels-lr-neo-look-neo-dark-theme.mmd",
    "git/visual-check/showbranches-false-and-showcommitlabel-false-hiding-branch-lines-and-commit-labels-lr-neo-look-neo-theme.mmd",
    "git/visual-check/showbranches-false-and-showcommitlabel-false-hiding-branch-lines-and-commit-labels-lr-neo-look-redux-color-theme.mmd",
    "git/visual-check/showbranches-false-and-showcommitlabel-false-hiding-branch-lines-and-commit-labels-lr-neo-look-redux-dark-color-theme.mmd",
    "git/visual-check/showbranches-false-and-showcommitlabel-false-hiding-branch-lines-and-commit-labels-lr-neo-look-redux-dark-theme.mmd",
    "git/visual-check/showbranches-false-and-showcommitlabel-false-hiding-branch-lines-and-commit-labels-lr-neo-look-redux-theme.mmd",
})

# sha256 over "path\tsource_sha256\n" rows in sorted order; a re-partition of
# the pinned cohort aborts generation.
COHORT_DIGEST = "46d27e97f208ebf508716a1c942a2ae3eec7090572568d70e97cc7d7c31225c5"

# Review records: authored here, written into the ledger.
PHASE_REVIEWS = {
    PHASE: {
        "reviewer_agent": "agent:ab81d8d24feb6dc37 rfc0039-p1-final-review (independent "
                          "adversarial code review; APPROVE; ran the generator --check, the "
                          "5-test gate and the E2a negative, re-derived the 135/18/44 counts, "
                          "and verified the closed tables, every rejection class, the footer "
                          "shape against the state/flow paths, the footer-string identity "
                          "across production/generator/ledger/gate, the drawn-row oracle's "
                          "sensitivity, and zero changes outside the four files plus three "
                          "new ones)",
        "date": "2026-11-22",
        "verdict": "approved",
        "note": "The review's two MAJOR items are the post-approval completion steps (record "
                "this approval, regenerate, run the matrix, then `--emit` the roster); they "
                "are carried out immediately after. Two LOW items applied test-only: the "
                "generator now mirrors the renderer's comment-strip/dequote scalar "
                "normalization, and a hermetic ctest runs the generator `--check` so the "
                "reviewed cohort pin is enforced automatically. Two NITs applied: the E2a "
                "negative comment now describes the local over-listing guard, and "
                "`git_inert_scalar` matches `state_scalar_text` exactly.",
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


def normalize_scalar(raw):
    """Mirror the renderer's `git_inert_scalar`: strip a trailing YAML comment
    outside quotes, trim, then dequote a matched pair. An unmatched quote stays
    literal (and therefore falls outside the closed tables)."""
    quote = ""
    cut = len(raw)
    for index, char in enumerate(raw):
        if quote:
            if char == quote:
                quote = ""
            continue
        if char in "\"'":
            quote = char
        elif char == "#" and (index == 0 or raw[index - 1] in " \t"):
            cut = index
            break
    value = raw[:cut].strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
        value = value[1:-1]
    return value


def parse_frontmatter(frontmatter):
    """Return (top_keys, config_leaves, error), mirroring the renderer's reader."""
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
        leaves[path] = normalize_scalar(value)
    return top, leaves, None


def notice_lines(leaves):
    """The bounded disclosure footer the renderer emits, in its fixed order."""
    lines = []
    if "look" in leaves:
        lines.append("config.look=neo -> source node style; not applied")
    if "theme" in leaves:
        lines.append(f"config.theme={leaves['theme']} -> colour palette; not applied")
    return [NOTICE_HEADER] + lines if lines else []


def validate_cohort_row(rel, top, leaves):
    """A pinned row must be exactly an admitted-leaf envelope."""
    if top.count("description") != 1 or "description" not in top:
        raise ValueError(f"{rel}: cohort row without a single description envelope")
    if top.count("config") != 1:
        raise ValueError(f"{rel}: cohort row without a single config block")
    for key in top:
        if key not in ("description", "config"):
            raise ValueError(f"{rel}: top-level key outside the envelope: {key}")
    if set(leaves) - {"look", "theme"}:
        raise ValueError(
            f"{rel}: leaf outside the admitted set: " + ",".join(sorted(set(leaves) - {"look", "theme"})))
    if "look" not in leaves or "theme" not in leaves:
        raise ValueError(f"{rel}: every cohort row carries both leaves")
    if leaves["look"] not in LOOK_VALUES:
        raise ValueError(f"{rel}: look outside the closed table: {leaves['look']}")
    if leaves["theme"] not in THEME_VALUES:
        raise ValueError(f"{rel}: theme outside the closed table: {leaves['theme']}")


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
            f"git family population drifted: expected {EXPECTED_FAMILY}, got {len(family)}")
    if len(COHORT) != EXPECTED_COHORT:
        raise ValueError(f"pinned cohort drifted: expected {EXPECTED_COHORT}, got {len(COHORT)}")
    missing = sorted(COHORT - set(family))
    if missing:
        raise ValueError("pinned cohort rows are missing from the family: " + ",".join(missing))

    shape_class = 0
    namespaced = 0
    classified = {}
    excluded = []
    for rel in family:
        frontmatter, _ = split_lines((cases / rel).read_text())
        if frontmatter is None:
            raise ValueError(f"git fixture without a frontmatter envelope: {rel}")
        top, leaves, error = parse_frontmatter(frontmatter)
        if error:
            raise ValueError(f"{rel}: {error}")
        namespaced_keys = {key for key in leaves if "." in key}
        if namespaced_keys and all(key.startswith("gitGraph.") for key in namespaced_keys):
            namespaced += 1
            excluded.append({"path": rel, "class": "namespaced config.gitGraph.*",
                             "reason": "semantic gitGraph settings stay closed"})
        elif not namespaced_keys and {"look", "theme"} & set(leaves):
            shape_class += 1
            if rel in COHORT:
                classified[rel] = (top, leaves)
            else:
                excluded.append({"path": rel,
                                 "class": "outside the reviewed cohort",
                                 "reason": "render-defined remainder: body branch/route "
                                           "density"})
        else:
            excluded.append({"path": rel, "class": "outside the reviewed cohort",
                             "reason": "render-defined remainder or another config shape"})
    if shape_class != EXPECTED_SHAPE_CLASS:
        raise ValueError(
            f"non-namespaced look/theme shape class drifted: expected "
            f"{EXPECTED_SHAPE_CLASS}, got {shape_class}")
    if namespaced != EXPECTED_NAMESPACED:
        raise ValueError(
            f"namespaced config.gitGraph.* population drifted: expected "
            f"{EXPECTED_NAMESPACED}, got {namespaced}")
    if len(classified) != EXPECTED_COHORT:
        raise ValueError(
            f"pinned cohort rows outside the shape class: expected {EXPECTED_COHORT}, "
            f"got {len(classified)}")

    rows = []
    for rel in sorted(COHORT):
        top, leaves = classified[rel]
        validate_cohort_row(rel, top, leaves)
        footer = notice_lines(leaves)
        if not footer:
            raise ValueError(f"cohort row without a disclosure footer: {rel}")
        rows.append({
            "path": rel,
            "family": "git",
            "phase": PHASE,
            "sha256": sha256_bytes((cases / rel).read_bytes()),
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "twin": {"kind": "synthetic"},
            "expected_prepend_lines": [],
            # The synthetic config-free twin carries no footer, so the whole
            # footer is an append after one blank separator line.
            "expected_footer_lines": [""] + footer,
            "prediction": "predicted-draw",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "source",
                "restored_240": "drawn",
            },
            "ledger_note": (
                "RFC 0039 gitGraph look/theme admission; validated and disclosed only, "
                "the frame equals the config-free twin's plus the bounded footer."),
        })

    digest = cohort_digest(rows)
    if COHORT_DIGEST not in ("PENDING", digest):
        raise ValueError(
            f"RFC 0039 cohort row set drifted: digest {digest} != pinned {COHORT_DIGEST}")

    return {
        "schema": SCHEMA,
        "rfc": 39,
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": "gitGraph fallback row whose frontmatter is a folded description "
                    "envelope whose config block carries only the closed look ({neo}) and "
                    "theme (six palettes) tables",
            "roots": list(ROOTS),
            "phase": PHASE,
            "cohort_pin": "reviewed literal path set (render-defined 12/18 split); one "
                          "atomic phase admitting both leaves",
            "cohort_digest": digest,
            "twin_rule": "the frame must equal the synthesized config-free twin's frame "
                         "plus the recorded footer delta, line by line; both sides are "
                         "rendered through the same sentinel-bearing wrapper",
            "corpus_twins": "none: no drawn git fixture is body-equivalent to a cohort row, "
                            "so all 12 rows use the config-free twin",
            "exclusions": "44 namespaced config.gitGraph.* rows (semantics debt) and 6 theme "
                          "rows whose bodies exceed the branch/route density stay fallback",
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
            "shape_class": shape_class,
            "namespaced": namespaced,
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
