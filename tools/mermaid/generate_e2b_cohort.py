#!/usr/bin/env python3
"""Generate the RFC 0031 E2b state inert-config pre-registration ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/state-diagram{,-v2} roots that
    - opens with a column-0 ``---`` frontmatter fence,
    - closes it and starts with ``stateDiagram`` / ``stateDiagram-v2``,
    - carries a top-level ``config:`` map whose indent-2 leaves belong to
      the E2b non-terminal whitelist with exact values
      (logLevel 0/1/2, fontFamily courier, securityLevel loose/sandbox),
    - carries only description/title/config top-level keys, folds the
      description as ``>-`` (the production gate accepts nothing else),
    - carries no nested config maps and no body ``%%{init...}%%``.

Each selected row is pre-registered predicted-draw or predicted-blocked.
Nine closed source-content blocker detectors classify the grammar/feature
debts; one sha-bound reviewed set (state-native-routing-debt) covers the
labeled-edge composite/fan-out plans the bounded router cannot close,
which no lexical detector can honestly predict. D223 retired five of the
lexical detectors (the constructs are native now) and added one envelope
description-tab detector; D228 retired the generic style-statement detector
and added one narrow unrepresentable-italic detector. Five fixed detectors
plus the routing set remain. Any residual selected row aborts generation:
zero unclassified rows. E2b pre-registers the
240-column admission only; narrow_40 is uniformly ``source`` and
restored_240 must equal wide_240.

Usage: python3 tools/mermaid/generate_e2b_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

import yaml

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/e2b-cohort.json")

SCHEMA = "loom_mermaid_e2b_cohort/v1"

WHITELIST = {
    "logLevel": {"0", "1", "2"},
    "fontFamily": {"courier"},
    "securityLevel": {"loose", "sandbox"},
}

BLOCKER_VOCABULARY = [
    "state-non-state-body",
    "state-composite-divider",
    "state-unrepresentable-italic",
    "state-click-statement",
    "state-envelope-description-tab",
    "state-native-routing-debt",
]

WITNESS = {
    "state-non-state-body": (
        "src/ui/visual/mermaid_relations_core_impl.cpp (state gate requires raw_body "
        "stateDiagram prefix)",
        "The body's first real line is not stateDiagram(-v2); the file belongs to a "
        "different family (e.g. a lone 'info' diagram) and the state envelope rejects it.",
    ),
    "state-composite-divider": (
        "src/ui/visual/mermaid_relations_impl.cpp (state edge parse; identifier('--') rejected)",
        "A standalone '--' concurrency-region divider reaches edge parsing and is rejected.",
    ),
    "state-unrepresentable-italic": (
        "src/ui/visual/mermaid_cells_impl.cpp SemanticCellStyle (no italic attribute)",
        "A classDef sets font-style:italic, but the terminal cell style model has only "
        "color/bold/underline and FTXUI has no italic attribute; admitting it would silently "
        "drop the requested style, so the whole source stays fallback. D228 parses the other "
        "state classDef/class/style properties (fill/stroke/color/border/bold) natively.",
    ),
    "state-click-statement": (
        "src/ui/visual/mermaid_relations_core_impl.cpp (no state click statement parse)",
        "A click callback/tooltip statement has no terminal interaction layer and no state "
        "statement parser; it stays whole-source.",
    ),
    "state-envelope-description-tab": (
        "src/ui/visual/mermaid_relations_core_impl.cpp (state folded-description validator) "
        "and mermaid_cells_impl.cpp unicode_text",
        "A real tab inside the folded frontmatter description is a control character the "
        "shared unicode_text validator rejects; the whole envelope stays source even though "
        "the body would parse.",
    ),
    "state-native-routing-debt": (
        "src/ui/visual/mermaid_relations_core_impl.cpp (relations_layout)",
        "The parser accepts the graph but the bounded native router cannot place the "
        "multiple labeled fan-out edges (including labeled composite entry/exit rows) "
        "without overlap. Not predictable lexically; sha-bound independent review set.",
    ),
}

# Sha-bound independently reviewed routing-debt set. Additions require a new
# review row; --check never grows it.
NATIVE_ROUTING_DEBT = {
    "state-diagram-v2/nightly-check/"
    "v2-should-render-a-simple-state-diagrams-with-labels.mmd":
        "4bafaf78477395eca79617efbbcdccc107453a67424b2282deb8ed3fce05e996",
    "state-diagram/pr-check/should-render-a-simple-state-diagrams-with-labels.mmd":
        "ba4df81bd51e6f5eb27e2155ab0a50ff8429e450a14f2b66db8ab766b1868255",
    "state-diagram-v2/pr-check/v2-should-render-composite-states.mmd":
        "f502ffd285c4cad4e13e08129e175cb31f9703ba1160a42cda1830a2cfea2a22",
    "state-diagram/pr-check/should-render-composite-states.mmd":
        "c512386e92a656dcc486e843065695f0384d51e62fe22434724d9b0b9690a46d",
}


def split_source(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return lines[1:close], lines[close + 1:]
    return None


def body_has_init(body_lines):
    return any(line.lstrip().startswith("%%{") for line in body_lines)


def folded_description_has_tab(frontmatter_lines):
    """Mirror the state envelope's folded-description tab rejection.

    Only the indented body of a `description: >-` block is inspected; a real
    tab there is a control character the shared unicode_text validator
    rejects, so the whole source stays fallback regardless of body grammar.
    """
    in_description = False
    for line in frontmatter_lines:
        if line.startswith("description:"):
            in_description = True
            continue
        if in_description:
            if line and not line.startswith(" "):
                in_description = False  # next column-0 key
            elif "\t" in line:
                return True
    return False


def whitelist_config(frontmatter_lines):
    """Return (ok, leaves) for the E2b structural contract."""
    try:
        parsed = yaml.safe_load("\n".join(frontmatter_lines))
    except yaml.YAMLError:
        return False, []
    if not isinstance(parsed, dict):
        return False, []
    top = set(parsed)
    if not top <= {"description", "title", "config"} or "config" not in top:
        return False, []
    if "description" in top:
        # Find the folded header line (description: >-). Plain descriptions
        # are rejected by the production state gate.
        header = next((l.split(":", 1)[1].strip()
                       for l in frontmatter_lines
                       if l.split(":", 1)[0].strip() == "description"), "")
        if header != ">-":
            return False, []
    config = parsed["config"]
    if not isinstance(config, dict) or not config:
        return False, []
    leaves = []
    for key, value in config.items():
        if key not in WHITELIST:
            return False, []
        if isinstance(value, bool) or isinstance(value, (dict, list)):
            return False, []
        text = str(value)
        if text not in WHITELIST[key]:
            return False, []
        leaves.append((key, text))
    return True, sorted(leaves)


def state_blocker(rel, source_sha256, body_lines):
    """Source-derived blocker in deterministic precedence; renderer-free."""
    real = [l.strip() for l in body_lines if l.strip() and not l.lstrip().startswith("%%")]
    first = real[0] if real else ""
    if first not in ("stateDiagram", "stateDiagram-v2"):
        return "state-non-state-body"
    for line in (l.strip() for l in body_lines):
        if line == "--":
            return "state-composite-divider"
    for line in (l.strip() for l in body_lines):
        if line.startswith("click") and (len(line) == 5 or line[5] in (" ", "\t")):
            return "state-click-statement"
    # D228: representable classDef/class/style (fill/stroke/color/border/
    # font-weight) now parse and draw. The one property the terminal cannot
    # model is font-style:italic, which still gates the whole source.
    for line in (l.strip().lower() for l in body_lines):
        if "font-style" in line and "italic" in line:
            return "state-unrepresentable-italic"
    body = "\n".join(body_lines)
    # D223 retired detectors: [[choice]], &lt;&lt;fork/join&gt;&gt;, the stray-tab
    # <br\t/> break, literal \n and #colon; edge escapes are now parsed and
    # drawn by the state renderer; their durable regression guard lives in
    # MermaidStateEnvelope native gtests, not in this renderer-free ledger.
    if rel in NATIVE_ROUTING_DEBT:
        if source_sha256 != NATIVE_ROUTING_DEBT[rel]:
            raise ValueError(
                f"State routing-debt source changed; independent review required: {rel}")
        return "state-native-routing-debt"
    return None


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def collect_root(cases, root_rel):
    rows = []
    root = cases / root_rel
    for path in sorted(root.rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        frontmatter, body = split
        if body_has_init(body):
            continue
        ok, leaves = whitelist_config(frontmatter)
        if not ok:
            continue
        source_sha = sha256_file(path)
        if folded_description_has_tab(frontmatter):
            kind = "state-envelope-description-tab"
        else:
            kind = state_blocker(rel, source_sha, body)
        rows.append({"path": rel, "leaves": leaves, "blocker": kind})
    return rows


def build(repo_root):
    cases = repo_root / CASES
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    roots = [
        ("state_v2", "state-diagram-v2"),
        ("state_v1", "state-diagram"),
    ]
    rows = []
    counts = {"state_v2": 0, "state_v1": 0,
              "predicted_draw": 0, "predicted_blocked": 0}
    for count_key, root_rel in roots:
        for entry in collect_root(cases, root_rel):
            kind = entry["blocker"]
            if kind is None:
                pass
            elif kind not in BLOCKER_VOCABULARY:
                raise ValueError(f"Blocker outside closed vocabulary: {kind}")
            blocked = kind is not None
            counts[count_key] += 1
            counts["predicted_blocked" if blocked else "predicted_draw"] += 1
            site, detail = WITNESS[kind] if blocked else ("", "")
            manifest_row = manifest_rows[entry["path"]]
            rows.append({
                "path": entry["path"],
                "family": "stateDiagram-v2" if count_key == "state_v2" else "stateDiagram",
                "sha256": sha256_file(cases / entry["path"]),
                "manifest_sha256": manifest_row["sha256"],
                "prior_status": manifest_row["raw"],
                "config_leaves": [{"key": k, "value": v} for k, v in entry["leaves"]],
                "has_init": False,
                "prediction": "predicted-blocked" if blocked else "predicted-draw",
                "expected_states": {
                    "wide_240": "source" if blocked else "drawn",
                    "narrow_40": "source",
                    "restored_240": "source" if blocked else "drawn",
                },
                "blocker": (
                    {"kind": kind, "site": site, "evidence": site, "detail": detail}
                    if blocked else None),
                "ledger_note": (
                    "E2b non-terminal config admission with disclosure footer"
                    if not blocked else
                    "Blocked by a closed source-content detector or the sha-bound "
                    "routing-debt review set"),
            })
    total = counts["state_v2"] + counts["state_v1"]
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "E2b",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256((repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter state original with a folded description and exactly the "
                    "E2b non-terminal config leaves (logLevel/fontFamily/securityLevel at "
                    "exact values), no nested maps, no body init",
            "roots": ["state-diagram-v2", "state-diagram"],
            "whitelist": {k: sorted(v) for k, v in WHITELIST.items()},
            "blocker_detection": "five fixed source-content detectors plus one "
                                 "path/SHA-bound reviewed routing-debt set; no "
                                 "generator-time rendering; residual rows abort. "
                                 "D223 retired the choice/fork-join/br-tab/newline/"
                                 "html-entity detectors (now native, guarded by "
                                 "MermaidStateEnvelope gtests) and added the folded-"
                                 "description-tab envelope detector. D228 retired the "
                                 "generic style-statement detector (representable "
                                 "classDef/class/style now parse and draw, guarded by "
                                 "MermaidStateStyles gtests), replacing it with a narrow "
                                 "unrepresentable-italic detector.",
            "width_policy": "E2b pre-registers the 240-column admission; narrow_40 is "
                            "uniformly 'source'.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial re-review agent, D228 state "
                              "style group (fresh general-purpose agent; full-diff audit, "
                              "independent 104-assertion probes plus a rerun of the author's "
                              "27-assertion dual-seam probe, 269/269 native suite, frozen "
                              "invariant and 51/10 partition verification); the D223 approval "
                              "covered only the 47/14 partition",
            "date": "2026-11-14",
            "verdict": "approved",
            "note": "D228 moves four more rows to predicted-draw by giving state its own "
                    "classDef/class/style parser and resolver (Rounded and nested entities "
                    "admitted, palette-role colors, border shorthand to stroke, bold to "
                    "pixels); the fifth style fixture stays blocked because font-style:italic "
                    "has no terminal cell attribute. Inline Node:::class edge syntax and a "
                    "direct fill/stroke/color:red stay whole-source fallback (fail-closed), "
                    "keeping the frozen State85 and StateScoped closed profiles byte-identical. "
                    "The legacy D223 seven rows and all frozen gates remain unchanged. Approved "
                    "51/10 partition and five-frame gate.",
        },
        "counts": {
            "total": total,
            "state_v2": counts["state_v2"],
            "state_v1": counts["state_v1"],
            "predicted_draw": counts["predicted_draw"],
            "predicted_blocked": counts["predicted_blocked"],
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
