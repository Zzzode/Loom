#!/usr/bin/env python3
"""Generate the RFC 0031 FS-3 flowchart style-projection body cohort ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/flowchart root that
    - carries a column-0 YAML frontmatter envelope (so the whole original
      stays on ordinary source fallback through the existing envelope gate),
    - whose graph body carries a styling statement (classDef definition,
      `class <targets> <name>` assignment, single-target `style`, or
      linkStyle).

FS-3a graduated the already-reviewed FlowStyle4 projection machinery
(classDef/class/direct style -> palette hue roles, stroke-width >= 4 ->
bold border, verbatim projection legend) from its test-only preview gate
to production. Exactly four selected bodies render natively at 240
columns; they are byte-pinned by the INDEPENDENTLY AUTHORED immutable
authority asset
tests/fixtures/mermaid_native/flow_style4_source_models.proposal/v1.json
(four complete original Models plus four separate body Models, each with
its own oracle proof). This generator cross-checks that the renderer-free
draw classification agrees with that authority set exactly: a missing or
extra draw row aborts generation.

The other 66 selected bodies carry at least one feature the closed
projection grammar rejects (11 fixed source-content blockers below); the
sets partition the 70-row selection with zero residual. A selected body
carrying only UNCLASSIFIED debt aborts generation with a sentinel
outside the vocabulary rather than being silently mislabeled.

Draw bodies paint on the body track at 240 columns (wide + restored) and
stay on ordinary source fallback at narrow_40; whole originals draw too
except the handDrawn row (look: handDrawn stays an unadmitted metadata
profile, FS-8), and every draw row keeps ordinary source at narrow_40.
The live pixel/legend gate is tests/ui/test_mermaid_fs3_cohort.cpp; the
full-scene oracle (eight negatives per scene) is
MermaidFlowStyle4OraclePrototype in test_mermaid_class_styles.cpp.

Usage: python3 tools/mermaid/generate_fs3_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs3-cohort.json")
STYLE4_AUTHORITY = pathlib.Path(
    "tests/fixtures/mermaid_native/flow_style4_source_models.proposal.v1.json")

SCHEMA = "loom_mermaid_fs3_cohort/v1"

# Closed blocker vocabulary: every kind below is witnessed in the pinned
# 70-row selection. A selected row carrying none of these (and not pinned
# as a draw row by the style4 authority) aborts generation.
BLOCKER_VOCABULARY = [
    "flow-style-subgraph",
    "flow-style-init-directive",
    "flow-style-named-color",
    "flow-style-unsupported-css-key",
    "flow-style-linkstyle",
    "flow-style-non-rectangle-shape",
    "flow-style-fanout",
    "flow-style-inline-class-shorthand",
    "flow-style-default-class",
    "flow-style-special-edge",
    "flow-style-bad-identifier",
]

WITNESS = {
    "flow-style-subgraph": (
        "src/ui/visual/mermaid_impl.cpp (flow_style_collect rejects inside a "
        "group stack; subgraph routing is FS-7)",
        "The styled body declares a subgraph; the style collector refuses "
        "groups and container-target styling waits for the routing cohort.",
    ),
    "flow-style-init-directive": (
        "src/ui/visual/mermaid_impl.cpp (body %%{init:}%% acceptance is bounded "
        "to earlier cohorts)",
        "The body carries an init directive with a key outside the admitted "
        "flow init profile.",
    ),
    "flow-style-named-color": (
        "src/ui/visual/mermaid_style_impl.cpp (source_color)",
        "A fill/stroke/color value is a CSS color name; only hex forms and "
        "the literal white/none map to a palette role.",
    ),
    "flow-style-unsupported-css-key": (
        "src/ui/visual/mermaid_style_impl.cpp (parse_style_properties)",
        "The statement sets a property other than fill/stroke/color/"
        "stroke-width (font-weight, font-size, stroke-dasharray, background, "
        "border, ...); terminal projection has no such attribute.",
    ),
    "flow-style-linkstyle": (
        "src/ui/visual/mermaid_impl.cpp (flow_style_keyword omits linkStyle)",
        "linkStyle styles edges (including `linkStyle default`); edge style "
        "projection is a later cohort, so node styles alone cannot draw it.",
    ),
    "flow-style-non-rectangle-shape": (
        "src/ui/visual/mermaid_impl.cpp (paint_flow_styles paints only "
        "Shape::Rectangle)",
        "A style targets or the graph contains a non-rectangle vertex "
        "(stadium, circle, hexagon, cylinder, subroutine, trapezoid, "
        "diamond); shaped-node styling lands with FS-5.",
    ),
    "flow-style-fanout": (
        "src/ui/visual/mermaid_impl.cpp (flow edge fan-out is FS-4)",
        "The body uses the `A & B` fan-out edge syntax, an unadmitted edge "
        "form that keeps the whole statement out of the styled path.",
    ),
    "flow-style-inline-class-shorthand": (
        "src/ui/visual/mermaid_impl.cpp (flow_statement node grammar)",
        "The body attaches a class inline with the `node:::name` shorthand "
        "instead of a separate `class node name` assignment.",
    ),
    "flow-style-default-class": (
        "src/ui/visual/mermaid_impl.cpp (flow_style_collect rejects the name "
        "`default`)",
        "A classDef named `default` would implicitly style every unstyled "
        "node; implicit default classes stay rejected.",
    ),
    "flow-style-special-edge": (
        "src/ui/visual/mermaid_impl.cpp (flow link scanner)",
        "The body uses an unadmitted edge form (thick/cross/open arrowheads, "
        "dotted links, invisible links, labelled edge refs, animation "
        "markers).",
    ),
    "flow-style-bad-identifier": (
        "src/ui/visual/mermaid_impl.cpp (flow_style_identifier)",
        "A style target or class name is outside the bounded "
        "[A-Za-z_][A-Za-z0-9_]* subset (numeric-leading or hyphenated ids).",
    ),
}

STYLE_RE = re.compile(r"^\s*(classDef\b|linkStyle\b|style\s+\S|class\s+\S+\s+\S)", re.M)
ALLOWED_KEYS = {"fill", "stroke", "color", "stroke-width"}
# Physical-line, quote-unaware scan (same documented limitation as the FS-2
# detector): mermaid style statements cannot legally contain quoted
# commas/colons in the pinned corpus, and the durable cohort gtest enforces
# both directions (blocked bodies stay source, draw bodies render), so a
# misclassification cannot pass silently.
SPECIAL_EDGE_RE = re.compile(r"==>|==x|==o|-\.->|-\.-|--x|--o|~~~|@-->?|@\{")
SHAPE_RE = re.compile(r"\(\[|\[\[|\[\(|\{\{|\(\(|\\/|/\\|\{[A-Za-z0-9_ ]")


def split_source(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None


def line_kind(line):
    text = line.strip()
    if text.startswith("classDef"):
        return "classDef"
    if text.startswith("style") and not text.startswith("style-"):
        return "direct"
    if re.match(r"class\s+\S+\s+\S+$", text):
        return "assign"
    return None


def blockers(body):
    """Return the sorted blocker kinds; [] means predicted draw."""
    found = set()
    if re.search(r"^\s*subgraph\b", body, re.M):
        found.add("flow-style-subgraph")
    if re.search(r"%%\{", body):
        found.add("flow-style-init-directive")
    if re.search(r"^\s*linkStyle\b", body, re.M):
        found.add("flow-style-linkstyle")
    if re.search(r":::\s*[A-Za-z_]", body):
        found.add("flow-style-inline-class-shorthand")
    if re.search(r"classDef\s+default\b", body):
        found.add("flow-style-default-class")
    if SPECIAL_EDGE_RE.search(body):
        found.add("flow-style-special-edge")
    if re.search(r"&\s*\w", body):
        found.add("flow-style-fanout")
    if SHAPE_RE.search(body):
        found.add("flow-style-non-rectangle-shape")
    for raw in body.split("\n"):
        line = raw.strip()
        if line.endswith(";"):
            line = line[:-1].strip()
        kind = line_kind(line)
        if kind in ("classDef", "direct"):
            match = re.match(r"(?:classDef|style)\s+(\S+)\s+(.*)$", line)
            if not match:
                continue
            name, props = match.group(1), match.group(2)
            if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", name):
                found.add("flow-style-bad-identifier")
            for part in (p.strip() for p in props.split(",") if p.strip()):
                if ":" not in part:
                    return ["flow-style-unclassified-other-debt"]
                key, value = part.split(":", 1)
                key, value = key.strip().lower(), value.strip().lower()
                if key not in ALLOWED_KEYS:
                    found.add("flow-style-unsupported-css-key")
                elif key in ("fill", "stroke", "color") and not (
                        value.startswith("#") or value in ("white", "none")):
                    found.add("flow-style-named-color")
        elif kind == "assign":
            match = re.match(r"class\s+(\S+)\s+(\S+)$", line)
            if match:
                targets, name = match.group(1), match.group(2)
                if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", name) or any(
                        not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", target)
                        for target in targets.split(",")):
                    found.add("flow-style-bad-identifier")
    return sorted(found)


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def collect(repo_root, authority_paths, handdrawn_paths):
    rows = []
    for path in sorted((repo_root / CASES / "flowchart").rglob("*.mmd")):
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        frontmatter, body = split
        if not STYLE_RE.search(body):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        kinds = blockers(body)
        if rel in authority_paths:
            # Draw classification is owned by the independently authored
            # style4 authority, not by these detectors.
            if kinds:
                raise ValueError(
                    f"Authority draw row carries detected debt {kinds}: {rel}")
            blocked = False
        else:
            if not kinds:
                # Closed vocabulary tripwire.
                raise ValueError(
                    f"Selected row with no classified blocker: {rel}")
            if any(kind not in BLOCKER_VOCABULARY for kind in kinds):
                raise ValueError(f"Blocker outside closed vocabulary: {kinds}")
            blocked = True
        original_draws = not blocked and rel not in handdrawn_paths
        rows.append({"path": rel, "blocked": blocked, "kinds": kinds,
                     "original_draws": original_draws})
    return rows


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    authority = json.loads((repo_root / STYLE4_AUTHORITY).read_text())
    if authority["count"] != 4:
        raise ValueError("style4 authority must pin exactly four cases")
    authority_pins = {c["fixture"]: c for c in authority["cases"]}
    handdrawn_paths = {
        path for path, case in authority_pins.items()
        if "look: handDrawn" in case["frontmatter_verbatim"]}
    entries = collect(repo_root, set(authority_pins), handdrawn_paths)
    selected_paths = {row["path"] for row in entries}
    if not set(authority_pins).issubset(selected_paths):
        raise ValueError("style4 authority fixture missing in selection")
    draw_paths = {row["path"] for row in entries if not row["blocked"]}
    if draw_paths != set(authority_pins):
        raise ValueError(
            f"draw set {sorted(draw_paths)} != style4 authority "
            f"{sorted(authority_pins)}")

    rows = []
    n_draw = n_blocked = 0
    for entry in entries:
        blocked = entry["blocked"]
        path = entry["path"]
        pin = authority_pins.get(path)
        n_blocked += blocked
        n_draw += not blocked
        kind = entry["kinds"][0] if blocked else None
        site, detail = WITNESS[kind] if blocked else ("", "")
        manifest_row = manifest_rows[path]
        original_draws = entry["original_draws"]
        rows.append({
            "path": path,
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / path),
            "manifest_sha256": manifest_row["sha256"],
            "style4_source_sha256": pin["source_sha256"] if pin else "",
            "style4_body_sha256": pin["body_sha256"] if pin else "",
            "prior_status": manifest_row["raw"],
            "prediction": "predicted-blocked" if blocked else "predicted-draw",
            # The whole original draws except the handDrawn-look row (FS-8
            # metadata stays closed); every draw row keeps source at 40.
            "expected_states": {
                "wide_240": "source" if blocked or not original_draws else "drawn",
                "narrow_40": "source",
                "restored_240": "source" if blocked or not original_draws else "drawn",
            },
            # The frontmatter-free body draws at 240 for all four authority
            # rows and stays source at narrow_40.
            "expected_body_states": {
                "wide_240": "source" if blocked else "drawn",
                "narrow_40": "source",
                "restored_240": "source" if blocked else "drawn",
            },
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if blocked else None),
            "ledger_note": (
                "FS-3a classDef/class/direct-style projection admitted on the "
                "style4 authority (body track)"
                if not blocked else
                "Blocked by a closed source-content feature detector; the "
                "styling grammar itself is a production no-op only when the "
                "whole body is otherwise admissible"),
        })
    total = n_draw + n_blocked
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-3a-style-projection",
        "style4_authority": {
            "asset": str(STYLE4_AUTHORITY),
            "schema": authority["schema"],
            "count": authority["count"],
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original whose graph body carries "
                    "a classDef/class/style/linkStyle statement",
            "roots": ["flowchart"],
            "draw_authority": "the draw set must exactly equal the four "
                              "fixtures pinned by the immutable FlowStyle4 "
                              "source-model proposal asset",
            "blocker_detection": "eleven fixed source-content detectors; "
                                 "no generator-time rendering; residual rows "
                                 "abort",
            "width_policy": "authority rows draw body (and whole original "
                            "except handDrawn) at 240 wide/restored and keep "
                            "ordinary source at narrow_40.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(D-066 FS-3a style graduation, 2026-11-05)",
            "date": "2026-11-05",
            "verdict": "approved",
            "note": "Read-only adversarial review of the FS-3a production "
                    "graduation (classDef/class/direct-style projection, "
                    "fail-closed grammar unchanged; the handDrawn look stays "
                    "a test-facade preview), the renderer-free selection and "
                    "eleven blocker detectors with zero-residual abort, the "
                    "exact draw-set binding to the immutable FlowStyle4 "
                    "authority (four bodies, three originals), and the "
                    "body/original matrix wiring. APPROVE: flow-only blast "
                    "radius, no shared constants, gaps stay 48, three "
                    "originals move pending 312->309.",
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
