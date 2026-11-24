#!/usr/bin/env python3
"""Generate the D-461 state-v2 admission cohort ledger (stages 1-3).

Renderer-free. Stage 1 admits the ten `state-diagram-v2` originals whose only
non-admitted input was the fixed 1M layout Work primary: the eight
`user-styled-*` composite containers and the two
`should-render-edge-labels-correctly*` grids, now funded by the bounded
canvas-derived state envelope. Stage 2 admits three vocabulary rows: the two
`click <id> "<url>" "<tooltip>"` fixtures (non-visual binding, FS-2) and the
one `font-style:italic` classDef.

Oracles:

* `noop-twin`: removing the `click` line must leave the frame cell-identical
  (the binding paints nothing).
* `style-twin`: replacing the dropped `font-style:italic` value with an equally
  unpainted `font-weight:normal` must leave the art above the legend
  cell-identical (the legend text differs only because it echoes the authored
  verbatim).
* `native-frame`: the layout Work envelope changed, not the art contract; the
  gate pins the wide/narrow/restored states, restored cell equality, and that
  every authored style statement appears verbatim in the legend.
* `crossing-frame`: routing geometry only; the row draws through the crossing
  retry pass (a failed state pass is undone from its pre-image and re-routed
  with perpendicular crossings). Structural assertions match `native-frame`.

Six routing-geometry rows (stage 3) are admitted through the crossing retry;
the one labeled composite whose 42-column edge label is wider than its scope and
the bare `info` directive remain under `deferred`.

Usage: python3 tools/mermaid/generate_state_d461_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/state-d461-cohort.json")
SCHEMA = "loom_mermaid_state_d461_cohort/v1"
DECISION = "D-461"

NIGHTLY = "state-diagram-v2/nightly-check/"
PR = "state-diagram-v2/pr-check/"

# Measured live at 40 columns (rule 34); only this grid still draws there.
NARROW_DRAW = {"should-render-edge-labels-correctly-with-multiple-states.mmd"}

BUDGET = [
    NIGHTLY + "user-styled-composite-containers-redux-theme.mmd",
    NIGHTLY + "user-styled-composite-containers-redux-dark-theme.mmd",
    NIGHTLY + "user-styled-composite-containers-redux-color-theme.mmd",
    NIGHTLY + "user-styled-composite-containers-redux-dark-color-theme.mmd",
    NIGHTLY + "user-styled-with-regions-composite-containers-redux-theme.mmd",
    NIGHTLY + "user-styled-with-regions-composite-containers-redux-dark-theme.mmd",
    NIGHTLY + "user-styled-with-regions-composite-containers-redux-color-theme.mmd",
    NIGHTLY + "user-styled-with-regions-composite-containers-redux-dark-color-theme.mmd",
    NIGHTLY + "should-render-edge-labels-correctly.mmd",
    NIGHTLY + "should-render-edge-labels-correctly-with-multiple-states.mmd",
]
NOOP = [
    NIGHTLY + "clickable-state-with-tooltip.mmd",
    NIGHTLY + "clickable-state-with-tooltip-handdrawn-look.mmd",
]
STYLE = [NIGHTLY + "v2-can-have-multiple-classes-applied-to-multiple-states.mmd"]
# Stage 3: routing geometry. These draw only through the crossing retry pass.
CROSSING = [
    NIGHTLY + "basic-states-neo-look-neo-theme.mmd",
    NIGHTLY + "basic-states-neo-look-neo-dark-theme.mmd",
    NIGHTLY + "basic-states-neo-look-redux-theme.mmd",
    NIGHTLY + "basic-states-neo-look-redux-dark-theme.mmd",
    NIGHTLY + "v2-should-render-a-simple-state-diagrams-with-labels.mmd",
    PR + "should-render-edge-labels-correctly-with-multiple-transitions.mmd",
]
# The failing edge carries a 42-column label (`Yet another long long öong öong
# öong label`) inside a composite narrower than the label: the
# terminal projection keeps labels in-row inside their owning scope, so the
# label cannot be placed without widening the composite past its authored
# bounds.
DEFERRED_LABEL_WIDTH = [PR + "v2-should-render-composite-states.mmd"]

REVIEW = {
    "reviewer_agent": ("stage 3: one independent read-only adversarial review "
                       "(task a7878335a4df1179a; round 1 REQUEST-CHANGES on the "
                       "classic stateDiagram leak, note-border crossings, port "
                       "pre-images, compound-scope painter/step agreement and "
                       "stale Work pins; all fixed and re-verified). Stage 1-2 "
                       "was reviewed by two independent adversarial agents, fresh "
                       "general-purpose agents, D-461 state-v2 stage 1-2, "
                       "2026-11-24. Gate/render lens (task a600ac6947bd3d80d): "
                       "REQUEST-CHANGES then APPROVE after the art proof "
                       "(`expected_art_labels`, frame glyph, >= 4 art rows) and "
                       "the e2a/e2b ledger migration. Correctness lens (task "
                       "a5d9c76642d33eaa0): REQUEST-CHANGES on the pending "
                       "verdict, the stale 0038 E2b footer pin 53 -> 55, the "
                       "roster, doc Work-rate wording and the e2b detector "
                       "mirror; all items addressed or rebutted in the same "
                       "slice (the residual MINORs -- the E2a narrow field and "
                       "the E2b declaration-list mirror -- were fixed after the "
                       "verdict and re-checked by the generators' --check). "
                       "Final pre-commit review (task ab05c66c561a29d02, fresh "
                       "read-only adversarial agent): APPROVE with no "
                       "BLOCKER/MAJOR; its MINOR/NIT items were all addressed in "
                       "the same slice (design-record Work delta 13,580, the "
                       "cross-owner scoping wording, `Canvas::segment` routed "
                       "through the undo log, a per-row oracle partition pin in "
                       "the gate, stage labels and a dead generator branch)."),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": ("Stages 1-3: the bounded canvas-derived state layout envelope, "
             "the two vocabulary admissions (`click` and `font-style`) and the "
             "crossing retry that admits six routing-geometry rows. One labeled "
             "composite (42-column edge label wider than its owning scope) stays "
             "under `deferred`; the bare `info` directive left the state family "
             "in D-461 stage 4 (its own family and cohort ledger)."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def style_statements(text: str):
    """Authored top-level style statements, in source order.

    The state legend echoes each applied declaration verbatim, so the gate can
    assert containment without mirroring the CSS grammar.
    """
    body = text.split("\n---\n", 1)[-1]
    out = []
    depth = 0
    for raw in body.split("\n"):
        line = raw.strip(" \t")
        if not line or line.startswith("%%"):
            continue
        if line.startswith("{") or line.endswith("{"):
            depth += line.count("{") - line.count("}")
            continue
        if depth:
            depth += line.count("{") - line.count("}")
            continue
        if re.match(r"^(classDef|class|style)\s", line):
            out.append(line.replace("\t", "\\t"))
    return out


def art_labels(text: str):
    """Declared state labels that must appear in the drawn art (first three).

    Derived from the body only: `state <Name>` declarations and the target of
    an outgoing transition. The gate asserts each appears above the legend, so
    a render that keeps only the disclosure text (legend/notices) but loses the
    boxes cannot pass.
    """
    names = []
    depth = 0
    body = text.split("\n---\n", 1)[-1]
    for raw in body.split("\n"):
        line = raw.strip(" \t")
        if line.startswith("{") or line.endswith("{"):
            depth += line.count("{") - line.count("}")
            opener = re.match(r"^state\s+([A-Za-z_][A-Za-z0-9_]*)", line)
            if opener:
                names.append(opener.group(1))
            continue
        if depth:
            depth += line.count("{") - line.count("}")
            opener = re.match(r"^state\s+([A-Za-z_][A-Za-z0-9_]*)", line)
            if opener:
                names.append(opener.group(1))
            continue
        opener = re.match(r"^state\s+([A-Za-z_][A-Za-z0-9_]*)\s*$", line)
        if opener:
            names.append(opener.group(1))
            continue
        # `X: Description` declaration form (D-449): the box shows the
        # description, so that is the visible label.
        described = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(\S.*)$", line)
        if described:
            names.append(described.group(2).strip())
            continue
        edge = re.match(r"^\[?[^\s]*\]?\s*-->\s*([A-Za-z_][A-Za-z0-9_]*)", line)
        if edge:
            names.append(edge.group(1))
    ordered = []
    for name in names:
        if name not in ordered:
            ordered.append(name)
    return ordered[:3]


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                      for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    for path, oracle in ([(p, "native-frame") for p in BUDGET] +
                         [(p, "noop-twin") for p in NOOP] +
                         [(p, "style-twin") for p in STYLE] +
                         [(p, "crossing-frame") for p in CROSSING]):
        text = (repo_root / CASES / path).read_text()
        observed = sha256_file(repo_root / CASES / path)
        if observed != manifest_rows[path]["sha256"]:
            raise ValueError(f"sha drift for {path}")
        row = {
            "path": path,
            "family": "state-diagram-v2",
            "sha256": observed,
            "manifest_sha256": manifest_rows[path]["sha256"],
            "prior_status": "source_fallback",
            "oracle": oracle,
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn" if pathlib.PurePosixPath(path).name in NARROW_DRAW
                else "source_fallback",
                "restored_240": "drawn",
            },
            "expected_style_statements": style_statements(text),
            "expected_art_labels": art_labels(text),
        }
        row["ledger_note"] = {
            "native-frame": "layout Work envelope only; art contract unchanged",
            "noop-twin": "the click binding paints nothing; the twin is cell-equal",
            "style-twin": "font-style is dropped; art above the legend is twin-equal",
            "crossing-frame": ("routing geometry only; the row draws through the "
                               "crossing retry pass"),
        }[oracle]
        rows.append(row)
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "stage": "1-3: state layout envelope, click bindings, font-style, crossing retry",
        "admission": {
            "envelope": ("relations_layout enables the two-pool meter for "
                         "RelationFamily::State from the actual canvas: "
                         "state_envelope_layout_allowance(w,h) = w*h*192 + 4096"),
            "click": ("`click <id> \"<url>\" [\"<tooltip>\"]`: closed grammar, "
                      "declared-target resolution, consumed and dropped (FS-2)"),
            "font-style": "`font-style: italic|normal` validated and dropped",
            "crossing": ("a failed state routing pass is undone from a pre-image "
                         "log and retried once with perpendicular crossings and "
                         "no free-region label prefilter; the retry is inert when "
                         "the first pass succeeds"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("every state-diagram-v2 fallback whose remaining blocker "
                     "is the fixed 1M layout primary, the click/font-style "
                     "vocabulary, or routing geometry that the crossing retry "
                     "closes"),
            "cohort_digest": cohort_digest(rows),
        },
        "deferred": {
            "stage3_label_width": DEFERRED_LABEL_WIDTH,
        },
        "counts": {"total": len(rows), "by_family": {"state-diagram-v2": len(rows)}},
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
