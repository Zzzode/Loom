#!/usr/bin/env python3
"""Generate the RFC 0037 requirement-family config-admission cohort ledger.

Renderer-free. The classifier partitions the requirement family's fallback
rows into the four classes RFC 0037 measured:

  * admitted-key-only rows (the cohort): a `description:` envelope whose
    `config:` subtree carries only the F0 leaves (`htmlLabels`, `logLevel`,
    `fontFamily`) plus the admitted `look` (P1), `theme` (P2) and the top-level
    `title:` (P3);
  * `config.layout` rows (elk layout engine) -- capability debt;
  * `config.flowchart.*` (namespaced subtree) -- closed by the D-066 F0
    boundary;
  * malformed envelopes -- fail closed.

Classification priority is fixed as `title -> P3`, else `theme -> P2`, else
`look -> P1` so a later phase can never reclassify an earlier phase's rows
(the six title rows also carry `theme`).

Only `ENABLED_PHASES` is emitted; the counts of every phase stay pinned. Each
enabled phase's row set is pinned by a digest over (relative path, source
sha256) so an earlier phase cannot drift while the totals stay equal. Review
records are authored here and written to the ledger, never read back from it,
so a hand-edited ledger fails `--check`.

Usage: python3 tools/mermaid/generate_requirement_0037_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/requirement-0037-cohort.json")

SCHEMA = "loom_mermaid_requirement_0037_cohort/v1"
ROOT_REL = "requirement"

ALL_PHASES = ("P1-look", "P2-theme", "P3-title")
ENABLED_PHASES = ("P1-look", "P2-theme", "P3-title")

# Pinned population per phase; asserted whether or not the phase is enabled.
EXPECTED = {"P1-look": 23, "P2-theme": 91, "P3-title": 6}
EXPECTED_COHORT = 120
# Rows outside the cohort, by class. The population is the whole requirement
# family (172 originals), classified by source shape, so the pins stay stable
# when a phase promotes rows and the derived fallback roster shrinks.
EXPECTED_FAMILY = 172
EXCLUDED_EXPECTED = {
    "layout: elk": 24,
    "namespaced config.flowchart.*": 2,
    "malformed envelope": 2,
    "already drawn (no admitted key)": 24,
}

# Closed value tables (RFC 0037 section 1): corpus-bounded.
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

NOTICE_HEADER = "Requirement native projection v1"

# The certified drawn requirement cohort (24 fixtures, matrix generation
# ee75f003... at RFC 0035 closure). A row may only use a corpus twin from this
# set; everything else uses the synthesized config-free twin.
DRAWN_TWINS = frozenset({
    "requirement/nightly-check/a-not-so-simple-requirement-diagram-default.mmd",
    "requirement/nightly-check/a-not-so-simple-requirement-diagram-without-htmllabels-default.mmd",
    "requirement/nightly-check/a-requirement-diagram-with-bt-direction-default.mmd",
    "requirement/nightly-check/a-requirement-diagram-with-empty-information-default.mmd",
    "requirement/nightly-check/a-requirement-diagram-with-lr-direction-default.mmd",
    "requirement/nightly-check/a-requirement-diagram-with-rl-direction-default.mmd",
    "requirement/nightly-check/a-requirement-diagram-with-tb-direction-default.mmd",
    "requirement/nightly-check/a-simple-requirement-diagram-default.mmd",
    "requirement/nightly-check/a-simple-requirement-diagram-without-htmllabels-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-and-without-information-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-long-and-short-text-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-long-and-short-text-without-htmllabels-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-markdown-text-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-markdown-text-without-htmllabels-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-quoted-text-for-spaces-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-styles-applied-from-class-statement-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-styles-applied-from-class-statement-without-htmllabels-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-styles-applied-from-classes-with-shorthand-syntax-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-styles-applied-from-classes-with-shorthand-syntax-without-htmllabels-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-styles-applied-from-style-statement-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-styles-applied-from-style-statement-without-htmllabels-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-styles-applied-from-the-default-class-and-other-styles-default.mmd",
    "requirement/nightly-check/requirements-and-elements-with-styles-applied-from-the-default-class-and-other-styles-without-htmllabels-default.mmd",
    "requirement/pr-check/sample.mmd",
})

# Per-phase row-set digests: sha256 over "path\tsource_sha256\n" rows in sorted
# order. Pinned when a phase is enabled; a re-partition aborts generation.
PHASE_DIGESTS = {
    "P1-look": "86b3c00211c5d84da3344235f8580555d102ccd256f8c01bd4cc5e0471a94ab5",
    "P2-theme": "7aa1dc5b3dc81105227882699b94f3158f528116e9f7ad5a8497438e9d5c8842",
    "P3-title": "154086e874f2b27dd9972db91654f7fbad78a20b51209669aee6ed631a0fb8ea",
}

# Per-phase review records: authored here, written into the ledger.
PHASE_REVIEWS = {
    "P3-title": {
        "reviewer_agent": "agent:c4f1a7e29b3d60851 rfc0037-p3-final-review (independent "
                          "adversarial code review; r1 REQUEST-CHANGES -> r2 APPROVE; verified "
                          "the ER-decoder fix and the seven decoder-branch negatives, "
                          "title+footer and title+D204-direct-style composition, six-file scope "
                          "with protected-file/manifest immutability, generator --check, 6/6 "
                          "cohort gate, 10/10 F0 suite, and an official matrix red only on the "
                          "pending verdict)",
        "date": "2026-11-21",
        "verdict": "approved",
        "note": "P3 projects the top-level title through the shared relation-title seam. "
                "Reviewer disposition: the first draft decoded with the config-leaf helper; "
                "the ER decoder (er_yaml_value + Kind::Text) is now used and the seven "
                "non-text/malformed scalars are asserted as fallbacks.",
    },
    "P2-theme": {
        "reviewer_agent": "agent:aa19624ebdb712b6a rfc0037-p2-final-review (independent "
                          "adversarial code review; APPROVE; verified the 4-file scope and "
                          "protected-file/manifest immutability, the closed 7-palette table and "
                          "F0/P1 footer byte-identity, an independent 172/23/91/6 re-derivation "
                          "with the 7-corpus/84-synthetic split, a ported audit of the synthetic "
                          "config-free twin, generator --check, cohort gate 4/4, F0 suite 10/10, "
                          "and an official matrix red only on the pending verdict)",
        "date": "2026-11-21",
        "verdict": "approved",
        "note": "P2 admits the closed theme palette table; title rows keep whole source. "
                "Reviewer NITs applied: the gate's config-free twin now skips blank lines like "
                "the generator, a corpus twin may not be the row itself, and a quoted palette "
                "scalar is asserted.",
    },
    "P1-look": {
        "reviewer_agent": "agent:ab5e96a11461da018 rfc0037-p1-final-review (independent "
                          "adversarial code review; r1 REQUEST-CHANGES -> r2/r3 APPROVE; verified "
                          "scope, the closed look table, generator --check, the 23-row "
                          "corpus-twin gate, F0 footer byte-identity and the official-matrix "
                          "pending-refusal)",
        "date": "2026-11-21",
        "verdict": "approved",
        "note": "P1 admits config.look only; theme/title rows keep whole source. Reviewer "
                "dispositions: the RFC disclosure example was aligned to the implemented "
                "string, unbalanced-quote/empty-config/title negatives were added, the "
                "synthetic-twin composition was deferred to P2, and the title negatives were "
                "made well-formed up to the single violated rule.",
    },
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def split_lines(text: str):
    """Split `---`-fenced frontmatter; returns (frontmatter, body) or a reason."""
    if not text.startswith("---"):
        return None, "malformed envelope: no column-0 opening fence"
    lines = text.split("\n")
    if lines[0].rstrip() != "---":
        return None, "malformed envelope: opening fence is not column 0"
    for close in range(1, len(lines)):
        if lines[close].rstrip() == "---":
            return lines[1:close], "\n".join(lines[close + 1:])
    return None, "malformed envelope: no column-0 closing fence"


def parse_frontmatter(frontmatter):
    """Return (top_keys, config_leaves, error).

    config_leaves maps a dotted leaf path to its scalar value, so a namespaced
    subtree (`flowchart.htmlLabels`) is visible as such. A folded or plain
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
    if "logLevel" in leaves:
        lines.append(f"config.logLevel={leaves['logLevel']} -> logger only; not applied")
    if "fontFamily" in leaves:
        lines.append('config.fontFamily="courier" -> terminal font; not applied')
    if "htmlLabels" in leaves:
        lines.append(f"config.htmlLabels={leaves['htmlLabels']} -> native text labels; not applied")
    if "look" in leaves:
        lines.append(f"config.look={leaves['look']} -> source node style; not applied")
    if "theme" in leaves:
        lines.append(f"config.theme={leaves['theme']} -> colour palette; not applied")
    return [NOTICE_HEADER] + lines if lines else []


def name_candidates(rel):
    """Name-derived twin candidates, most specific first."""
    directory, _, name = rel.rpartition("/")
    stem = name[: -len(".mmd")] if name.endswith(".mmd") else name
    prefix = directory + "/" if directory else ""
    if "-neo-look-" in stem and stem.endswith("-theme"):
        base = stem.split("-neo-look-")[0]
    elif stem.endswith("-handdrawn-look"):
        base = stem[: -len("-handdrawn-look")]
    else:
        return []
    return [f"{prefix}{base}{suffix}.mmd" for suffix in ("-default", "-without-htmllabels-default")]


def twin_candidate(rel, cases, source_leaves):
    """Resolve the body-equivalent drawn corpus twin, if one exists.

    Name-derived siblings first, then a deterministic scan of the pinned drawn
    set. A candidate is only accepted when its own footer is a prefix of the
    source's footer, so the recorded delta is a pure append.
    """
    source_footer = notice_lines(source_leaves)
    for candidate in name_candidates(rel):
        if candidate not in DRAWN_TWINS or not (cases / candidate).is_file():
            continue
        if body_digest(cases / candidate) != body_digest(cases / rel):
            continue
        front, _ = split_lines((cases / candidate).read_text())
        _, twin_leaves, _ = parse_frontmatter(front)
        twin_footer = notice_lines(twin_leaves)
        if source_footer[: len(twin_footer)] == twin_footer:
            return candidate
    for candidate in sorted(DRAWN_TWINS):
        if not (cases / candidate).is_file():
            continue
        if body_digest(cases / candidate) != body_digest(cases / rel):
            continue
        front, _ = split_lines((cases / candidate).read_text())
        _, twin_leaves, _ = parse_frontmatter(front)
        twin_footer = notice_lines(twin_leaves)
        if source_footer[: len(twin_footer)] == twin_footer:
            return candidate
    return None


def frontmatter_title(path):
    """The projected title, mirroring the renderer's dequote/trim reader."""
    front, _ = split_lines(path.read_text())
    if front is None:
        raise ValueError(f"no frontmatter: {path}")
    for raw in front:
        if raw.startswith("title:"):
            value = raw[len("title:"):].strip()
            if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
                value = value[1:-1]
            if not value:
                raise ValueError(f"empty title: {path}")
            return value
    raise ValueError(f"no title: {path}")


def body_digest(path):
    """Digest over the non-blank, per-line-stripped body."""
    text = path.read_text()
    parts, _ = split_lines(text)
    body = text
    if parts is not None:
        lines = text.split("\n")
        for close in range(1, len(lines)):
            if lines[close].rstrip() == "---":
                body = "\n".join(lines[close + 1:])
                break
    normalized = "\n".join(line.strip() for line in body.split("\n") if line.strip())
    return sha256_bytes(normalized.encode())


def classify(top, leaves):
    """Return (phase, reason) or (None, exclusion reason)."""
    if "layout" in leaves:
        return None, "layout: elk", "layout: elk"
    namespaced = sorted(key for key in leaves if "." in key)
    if namespaced:
        if all(key.startswith("flowchart.") for key in namespaced):
            return (None, "namespaced config.flowchart.*",
                    "namespaced config.flowchart.*")
        return None, "other", "namespaced config subtree outside flowchart.*"
    allowed = {"htmlLabels", "logLevel", "fontFamily", "look", "theme"}
    if set(leaves) - allowed:
        return (None, "other",
                "unexpected config leaf: " + ",".join(sorted(set(leaves) - allowed)))
    if leaves.get("look") is not None and leaves["look"] not in LOOK_VALUES:
        return None, "other", "look value outside the closed table: " + leaves["look"]
    if leaves.get("theme") is not None and leaves["theme"] not in THEME_VALUES:
        return None, "other", "theme value outside the closed table: " + leaves["theme"]
    for key in ("description", "config", "title"):
        if top.count(key) > 1:
            return None, "other", f"duplicate top-level key: {key}"
    if [key for key in top if key not in ("description", "config", "title")]:
        return None, "other", "top-level key outside the envelope"
    if "description" not in top:
        return None, "other", "no description envelope"
    # Fixed priority so a later phase cannot reclassify earlier rows.
    if "title" in top:
        return "P3-title", None, "top-level title projection"
    if "theme" in leaves:
        return "P2-theme", None, "closed theme palette admission"
    if "look" in leaves:
        return "P1-look", None, "closed look table admission"
    return None, "other", "no admitted key"


def phase_digest(entries):
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n" for entry in entries)
    return sha256_bytes(payload.encode())


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    family = sorted(
        path.relative_to(cases).as_posix()
        for path in (cases / ROOT_REL).rglob("*.mmd"))

    buckets = {phase: [] for phase in ALL_PHASES}
    excluded = []
    for rel in family:
        text = (cases / rel).read_text()
        frontmatter, remainder = split_lines(text)
        if frontmatter is None:
            excluded.append({"path": rel, "class": "malformed envelope", "reason": remainder})
            continue
        top, leaves, error = parse_frontmatter(frontmatter)
        if error:
            excluded.append({"path": rel, "class": "malformed envelope", "reason": error})
            continue
        phase, exclusion, reason = classify(top, leaves)
        if phase is None:
            if exclusion == "other" and reason == "no admitted key":
                exclusion = "already drawn (no admitted key)"
            excluded.append({"path": rel, "class": exclusion, "reason": reason})
            continue
        buckets[phase].append((rel, top, leaves))

    for phase, expected in EXPECTED.items():
        found = len(buckets[phase])
        if found != expected:
            raise ValueError(
                f"RFC 0037 {phase} population drifted: expected {expected}, got {found}; "
                "re-scope the ledger and obtain a new independent review")
    if sum(len(rows) for rows in buckets.values()) != EXPECTED_COHORT:
        raise ValueError("cohort total drifted")
    if len(family) != EXPECTED_FAMILY:
        raise ValueError(
            f"requirement family population drifted: expected {EXPECTED_FAMILY}, "
            f"got {len(family)}")
    counts_by_reason = {}
    for entry in excluded:
        key = entry["class"]
        counts_by_reason[key] = counts_by_reason.get(key, 0) + 1
    for reason, expected in EXCLUDED_EXPECTED.items():
        if counts_by_reason.get(reason, 0) != expected:
            raise ValueError(
                f"excluded class {reason!r} drifted: expected {expected}, "
                f"got {counts_by_reason.get(reason, 0)}")

    rows = []
    counts = {"total": 0, "predicted_draw": 0, "by_phase": {}}
    for phase in ENABLED_PHASES:
        entries = buckets[phase]
        digest = phase_digest([
            {"path": rel, "sha256": sha256_bytes((cases / rel).read_bytes())}
            for rel, _, _ in sorted(entries)])
        if PHASE_DIGESTS.get(phase) not in (None, digest):
            raise ValueError(
                f"RFC 0037 {phase} row set drifted: digest {digest} != pinned "
                f"{PHASE_DIGESTS[phase]}")
        for rel, top, leaves in sorted(entries):
            prepend = []
            twin = None
            if phase == "P3-title":
                # The title rows use the *title-free* twin: the same source with
                # the `title:` line removed, so its own look/theme footer stays.
                kind = {"kind": "title-free"}
                prepend = [frontmatter_title(cases / rel), ""]
                source_footer = notice_lines(leaves)
                footer_lines = []
            else:
                twin = twin_candidate(rel, cases, leaves)
                if twin is None:
                    kind = {"kind": "synthetic"}
                else:
                    kind = {"kind": "corpus", "path": twin,
                            "sha256": sha256_bytes((cases / twin).read_bytes())}
                twin_leaves = {}
                if twin is not None:
                    twin_front, _ = split_lines((cases / twin).read_text())
                    _, twin_leaves, _ = parse_frontmatter(twin_front)
                source_footer = notice_lines(leaves)
                twin_footer = notice_lines(twin_leaves)
                if source_footer[: len(twin_footer)] != twin_footer:
                    raise ValueError(f"twin footer is not a prefix of the source footer: {rel}")
                delta = source_footer[len(twin_footer):]
                footer_lines = ([] if twin_footer else [""]) + delta
            counts["total"] += 1
            counts["predicted_draw"] += 1
            counts["by_phase"][phase] = counts["by_phase"].get(phase, 0) + 1
            rows.append({
                "path": rel,
                "family": "requirement",
                "phase": phase,
                "sha256": sha256_bytes((cases / rel).read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "prior_status": manifest_rows[rel]["raw"],
                "twin": kind,
                "expected_prepend_lines": prepend,
                "expected_footer_lines": footer_lines,
                "prediction": "predicted-draw",
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
                "ledger_note": (
                    "RFC 0037 P1 config.look admission; validated and disclosed only, "
                    "the frame equals the config-free twin's plus the bounded footer."
                    if phase == "P1-look" else
                    "RFC 0037 P2 closed theme-palette admission; validated and disclosed "
                    "only."
                    if phase == "P2-theme" else
                    "RFC 0037 P3 title projection through the shared relation-title seam."),
            })

    return {
        "schema": SCHEMA,
        "rfc": 37,
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": "requirement fallback row whose frontmatter is a description envelope "
                    "whose config subtree carries only the F0 leaves plus the admitted look "
                    "(P1), theme (P2) and top-level title (P3)",
            "roots": [ROOT_REL],
            "phase_partition": "renderer-free classification with pinned per-phase "
                               "denominators; only ENABLED_PHASES is emitted",
            "classification_priority": "title -> P3, else theme -> P2, else look -> P1",
            "enabled_phases": list(ENABLED_PHASES),
            "phase_digests": {phase: phase_digest([
                {"path": rel, "sha256": sha256_bytes((cases / rel).read_bytes())}
                for rel, _, _ in sorted(buckets[phase])]) for phase in ALL_PHASES},
            "exclusions": "layout: elk (24), namespaced config.flowchart.* (2) and malformed "
                          "envelopes (2) stay whole-source fallback",
            "width_policy": "wide_240 admission only; narrow_40 stays 'source'.",
        },
        "review": {
            "phases": PHASE_REVIEWS,
            "verdict": "approved" if all(
                phase in PHASE_REVIEWS and PHASE_REVIEWS[phase]["verdict"] == "approved"
                for phase in ENABLED_PHASES) else "pending",
        },
        "counts": counts,
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
    for phase in ALL_PHASES:
        print(phase, "digest", ledger["selection"]["phase_digests"][phase])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
