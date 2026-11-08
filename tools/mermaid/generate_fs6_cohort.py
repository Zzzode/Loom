#!/usr/bin/env python3
"""Generate the RFC 0031 FS-6 flowchart FontAwesome-icon body cohort ledger.

Selection (mechanical, byte-level, never derived from renderer output):
  every unchanged upstream original under the pinned Mermaid 12.2.0
  cases/flowchart root that
    - carries a column-0 YAML frontmatter envelope (so the whole original
      stays on ordinary source fallback through the envelope gate),
    - whose text carries a FontAwesome marker (fa:fa-<name>, with an
      optional single-letter style prefix fas/fab/fal/far/fak).

FS-6 projects a recognized marker to one fixed terminal marker cell
(U+25A0) because the terminal has no icon font, retaining the human
label text. The admitted icon vocabulary is the closed set {car, code}.
A selected row draws its body iff it has no marker outside that
vocabulary AND every in-body `%%{init ...}%%` directive is on the
whitelisted non-terminal set (single-key flowchart.htmlLabels or
logLevel scalar grammar; RFC 0031 FS-init). In the pinned corpus the
remaining body blockers are exactly two closed detectors, giving a
38 body-draw / 16 blocked partition with zero residual:
  * flow-fa-unknown-icon: a marker names an icon other than car/code
    (bell, coffee, github, cogs, server, database, ...).
  * flow-fa-theme-config: an in-body %%{init ...}%% directive carries a
    payload outside the whitelist (e.g. a `theme` key). The four
    new-syntax theme fixtures are the only such rows.

The frontmatter `theme:` key never blocks the body track (frontmatter is
stripped before body rendering); it keeps the WHOLE original on source
through the envelope gate. Four legacy fixtures therefore draw body
only (RFC 0031 FS-init also consumes their accTitle/accDescr scalars);
they joined the 17 pre-existing body-only rows. RFC 0031 FS-spacing later
promoted two of those body-only rows (22/23 nodeSpacing/rankSpacing) to
whole-and-body, leaving 19 body-only rows while 19 body draws also admit
their whole originals.

Per-width states are pinned path sets: the narrow 40-column frame draws
only for the simple graphs that fit it; the durable cohort gtest
enforces every state live, so a geometry or vocabulary drift fails
loudly even though the generator itself never renders.

Draw bodies paint on the body track at 240 (wide + restored); 19 of the
38 body-draw rows also draw the whole original at 240 (17 with admitted
frontmatter plus the 2 FS-spacing spacing-leaf rows); the other 19 keep
whole-original source (15 pre-existing plus the 4 FS-init body-only
rows). The handDrawn-look rows keep whole-original source with FS-8.

Usage: python3 tools/mermaid/generate_fs6_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/fs6-cohort.json")

SCHEMA = "loom_mermaid_fs6_cohort/v1"

BLOCKER_VOCABULARY = [
    "flow-fa-unknown-icon",
    "flow-fa-theme-config",
]

WITNESS = {
    "flow-fa-unknown-icon": (
        "src/ui/visual/mermaid_impl.cpp (label() closed icon vocabulary)",
        "The body binds an icon outside the projected {car, code} set "
        "(bell, coffee, github, twitter, facebook, cogs, circle, server, "
        "database, creative-commons); the terminal has no glyph for it.",
    ),
    "flow-fa-theme-config": (
        "src/ui/visual/mermaid_impl.cpp (flow_init_directive whitelist)",
        "An in-body %%{init ...}%% directive carries a payload outside the "
        "closed non-terminal whitelist (single-key flowchart.htmlLabels or "
        "logLevel 0..5/named levels); the pinned rows add a `theme` key, so "
        "the whole directive rejects and the body stays on ordinary "
        "fallback.",
    ),
}

MARKER_RE = re.compile(r"fa[bklrs]?:fa-([A-Za-z0-9-]+)")
ADMITTED_ICONS = {"car", "code"}
LOG_LEVEL_NAMES = {"trace", "debug", "info", "warn", "error", "fatal"}

# RFC 0031 FS-init: four legacy fixtures whose body directive is a
# single-key logLevel payload (now whitelisted) and whose accTitle/
# accDescr scalars are consumed. Their frontmatter still declares
# `theme:`, so the body draws while the whole original stays source.
BODY_ONLY_DRAW = {
    "flowchart/visual-check/dark-theme-legacy-graph.mmd",
    "flowchart/visual-check/default-theme-legacy-graph.mmd",
    "flowchart/visual-check/forest-theme-legacy-graph.mmd",
    "flowchart/visual-check/neutral-theme-legacy-graph.mmd",
}

DIRECTIVE_RE = re.compile(r"^\s*%%\{init:(.*)\}%%\s*$")


def _normalize_single_quotes(payload):
    """Mirror of normalize_directive_quotes in mermaid_impl.cpp: single
    quotes become double quotes (backslash rejected, embedded double
    quote escaped); double-quoted strings pass through verbatim."""
    out = []
    at = 0
    while at < len(payload):
        q = payload[at]
        if q not in ("'", '"'):
            out.append(q)
            at += 1
            continue
        begin = at
        at += 1
        while at < len(payload) and payload[at] != q:
            if q == "'" and payload[at] == "\\":
                return None
            at += 1
        if at == len(payload):
            return None
        if q == '"':
            out.append(payload[begin:at + 1])
        else:
            text = payload[begin + 1:at]
            out.append('"' + text.replace('"', '\\"') + '"')
        at += 1
    return "".join(out)


def admitted_init_directive(line):
    """Mirror of flow_init_directive's closed one-key whitelist."""
    match = DIRECTIVE_RE.match(line)
    if not match:
        return False
    normalized = _normalize_single_quotes(match.group(1).strip())
    if normalized is None:
        return False
    try:
        payload = json.loads(normalized)
    except json.JSONDecodeError:
        return False
    if not isinstance(payload, dict) or len(payload) != 1:
        return False
    (key, value), = payload.items()
    if key == "logLevel":
        if isinstance(value, bool):
            return False
        if isinstance(value, int):
            return 0 <= value <= 5
        return isinstance(value, str) and value in LOG_LEVEL_NAMES
    if key == "flowchart":
        return (isinstance(value, dict) and len(value) == 1 and
                "htmlLabels" in value and isinstance(value["htmlLabels"], bool))
    return False

# Pinned per-row width/frontmatter geometry. These facts are not derivable
# from source bytes without rendering, so they are pinned here and enforced
# bidirectionally by tests/ui/test_mermaid_fs6_cohort.cpp (every pinned
# state must match the live renderer at all three widths); a fixture drift
# fails --check and/or the gtest.
ORIGINAL_DRAW = {
    "flowchart/nightly-check/classic/01-38-should-render-a-flowchart-when-usemaxwidth-is-true-default.mmd",
    "flowchart/nightly-check/classic/02-39-should-render-a-flowchart-when-usemaxwidth-is-false.mmd",
    "flowchart/nightly-check/dagre/02-1-dagre-should-render-a-simple-flowchart.mmd",
    "flowchart/nightly-check/dagre/03-1-dagre-should-render-a-simple-flowchart.mmd",
    "flowchart/nightly-check/dagre/2-dagre-should-render-a-simple-flowchart-with-diagrampadding-set-to-0.mmd",
    "flowchart/nightly-check/dagre/6088-1-should-handle-diamond-shape-intersections.mmd",
    "flowchart/nightly-check/dagre/6088-3-should-handle-diamond-shape-intersections.mmd",
    "flowchart/nightly-check/dagre/61-dagre-fontawesome-icons-in-edge-labels.mmd",
    "flowchart/nightly-check/v2/01-7-should-render-a-flowchart-when-usemaxwidth-is-true-default.mmd",
    "flowchart/nightly-check/v2/02-8-should-render-a-flowchart-when-usemaxwidth-is-false.mmd",
    "flowchart/visual-check/1-should-render-a-simple-flowchart-no-htmllabels.mmd",
    "flowchart/visual-check/1-should-render-a-simple-flowchart.mmd",
    "flowchart/visual-check/15-should-render-a-simple-flowchart-with-comments.mmd",
    "flowchart/visual-check/2-should-render-a-simple-flowchart-with-diagrampadding-set-to-0.mmd",
    "flowchart/visual-check/33-should-render-a-simple-flowchart-with-diagrampadding-set-to-0.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels-2.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels.mmd",
}
BODY_NARROW_DRAW = {
    "flowchart/nightly-check/dagre/61-dagre-fontawesome-icons-in-edge-labels.mmd",
    "flowchart/nightly-check/handdrawn/fontawesome-icons-in-edge-labels.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels-2.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels.mmd",
}
# RFC 0031 FS-spacing: these two draw bodies were whole-source solely
# because nodeSpacing/rankSpacing had no envelope admission profile. Once
# those leaves are whitelisted they draw the whole original too.
SPACING_WHOLE_DRAW = {
    "flowchart/visual-check/22-render-a-simple-flowchart-with-nodespacing-set-to-100.mmd",
    "flowchart/visual-check/23-render-a-simple-flowchart-with-rankspacing-set-to-100.mmd",
}
ORIGINAL_NARROW_DRAW = {
    "flowchart/nightly-check/dagre/61-dagre-fontawesome-icons-in-edge-labels.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels-2.mmd",
    "flowchart/visual-check/61-fontawesome-icons-in-edge-labels.mmd",
}


def split_source(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None


def body_blocker(body):
    # The body track renders frontmatter-free text, so only in-body
    # features can block it: an unknown icon or a non-whitelisted
    # directive. The frontmatter theme gate belongs to the whole track.
    icons = set(MARKER_RE.findall(body))
    if icons - ADMITTED_ICONS:
        return "flow-fa-unknown-icon"
    for line in body.splitlines():
        if "%%{" in line and not admitted_init_directive(line):
            return "flow-fa-theme-config"
    return None


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    entries = []
    for path in sorted((repo_root / CASES / "flowchart").rglob("*.mmd")):
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        frontmatter, body = split
        if not MARKER_RE.search(text):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        kind = body_blocker(body)
        blocked = kind is not None
        body_only = (not blocked) and rel in BODY_ONLY_DRAW
        original_draws = (not blocked) and not body_only and (
            rel in ORIGINAL_DRAW or rel in SPACING_WHOLE_DRAW)
        # Geometry pin consistency checks: a narrow-draw path must draw wide.
        if rel in BODY_NARROW_DRAW and blocked:
            raise ValueError(f"body narrow-draw path is blocked: {rel}")
        if rel in ORIGINAL_NARROW_DRAW and not original_draws:
            raise ValueError(f"original narrow-draw path is not an original draw: {rel}")
        if body_only and not re.search(r"^\s*theme:", frontmatter, re.M):
            raise ValueError(f"body-only row lacks the frontmatter theme gate: {rel}")
        if body_only and not re.search(r"^\s*acc(Title|Descr)\s*:", body, re.M):
            raise ValueError(f"body-only row lacks accessibility statements: {rel}")
        entries.append((rel, blocked, body_only, original_draws, kind))
    # The pinned draw sets must be subsets of the selection, and the closed
    # detectors must reproduce the partition exactly (zero residual).
    selected = {rel for rel, _, _, _, _ in entries}
    if not ORIGINAL_DRAW <= selected:
        raise ValueError("pinned original-draw path missing from selection")
    if not BODY_ONLY_DRAW <= selected:
        raise ValueError("pinned body-only path missing from selection")
    if not SPACING_WHOLE_DRAW <= selected:
        raise ValueError("pinned spacing whole-draw path missing from selection")
    if ORIGINAL_DRAW & BODY_ONLY_DRAW or SPACING_WHOLE_DRAW & BODY_ONLY_DRAW:
        raise ValueError("whole-draw and body-only pin sets overlap")
    draw_paths = {rel for rel, blocked, _, _, _ in entries if not blocked}
    if not draw_paths:
        raise ValueError("no draw rows")
    # Every pinned whole-draw row must be a mechanical draw row.
    if not (ORIGINAL_DRAW | SPACING_WHOLE_DRAW) <= draw_paths:
        raise ValueError("pinned whole-draw row is mechanically blocked")
    rows = []
    n_draw = n_blocked = n_original_draw = 0
    n_body_only_scope = n_fs_init = 0
    for rel, blocked, body_only, original_draws, kind in entries:
        n_blocked += blocked
        n_draw += not blocked
        n_original_draw += original_draws
        n_fs_init += body_only
        # A draw row whose whole original stays source is body-only scope.
        # The 17 pre-existing rows already had this disposition before
        # FS-init; the four pinned FS-init rows are the new members.
        body_only_scope = (not blocked) and not original_draws
        n_body_only_scope += body_only_scope
        site, detail = WITNESS[kind] if blocked else ("", "")
        manifest_row = manifest_rows[rel]
        body_narrow = (not blocked) and rel in BODY_NARROW_DRAW
        original_narrow = original_draws and rel in ORIGINAL_NARROW_DRAW
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": sha256_file(repo_root / CASES / rel),
            "manifest_sha256": manifest_row["sha256"],
            "prior_status": manifest_row["raw"],
            "prediction": "predicted-blocked" if blocked else "predicted-draw",
            "track_scope": "body-only" if body_only_scope else "whole-and-body",
            "expected_states": {
                "wide_240": "drawn" if original_draws else "source",
                "narrow_40": "drawn" if original_narrow else "source",
                "restored_240": "drawn" if original_draws else "source",
            },
            "expected_body_states": {
                "wide_240": "source" if blocked else "drawn",
                "narrow_40": "drawn" if body_narrow else "source",
                "restored_240": "source" if blocked else "drawn",
            },
            "icons": sorted(set(MARKER_RE.findall((repo_root / CASES / rel).read_text()))),
            "blocker": (
                {"kind": kind, "site": site, "evidence": site, "detail": detail}
                if blocked else None),
            "ledger_note": (
                "FS-init body-only: whitelisted logLevel directive and "
                "accTitle/accDescr scalars admitted; the frontmatter theme "
                "gate keeps the whole original source"
                if body_only else
                "FS-spacing: non-terminal nodeSpacing/rankSpacing envelope "
                "leaves admitted, so the whole original now draws"
                if rel in SPACING_WHOLE_DRAW else
                "FS-6 FontAwesome marker projected to a fixed terminal cell on "
                "the body track"
                if not blocked else
                "Blocked by a closed source-content feature detector; the "
                "marker vocabulary itself is only admitted for car/code"),
        })
    if (n_draw != 38 or n_blocked != 16 or n_original_draw != 19 or
            n_body_only_scope != 19 or n_fs_init != 4):
        raise ValueError(
            f"partition drift: draw={n_draw} blocked={n_blocked} "
            f"original_draw={n_original_draw} body_only_scope="
            f"{n_body_only_scope} fs_init={n_fs_init}")
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "FS-6-fontawesome-icons",
        "icon_projection": {
            "marker": "U+25A0 BLACK SQUARE",
            "admitted_icons": sorted(ADMITTED_ICONS),
            "style_prefixes": ["fa", "fas", "fab", "fal", "far", "fak"],
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "frontmatter flowchart original whose text binds a "
                    "FontAwesome fa:fa-<name> marker",
            "roots": ["flowchart"],
            "blocker_detection": "two fixed body-content detectors "
                                 "(unknown icon, non-whitelisted init "
                                 "directive); no generator-time rendering; "
                                 "residual rows abort",
            "width_policy": "draw bodies paint at 240; the pinned simple "
                           "graphs also paint at narrow_40; whole originals "
                           "paint for the 19 admitted rows (17 frontmatter "
                           "plus the 2 FS-spacing leaves); the four FS-init "
                           "body-only rows keep whole source behind the "
                           "frontmatter theme gate.",
        },
        "review": {
            "reviewer_agent": "independent read-only adversarial review "
                              "(D-066 FS-6 fontawesome icons, 2026-11-06; "
                              "FS-init follow-up 2026-11-08 task "
                              "a84b30aa96dc983db; FS-spacing follow-up "
                              "2026-11-09 task aee66c1df9b510318; final "
                              "cross-cohort-migration re-review task "
                              "a91f43e0c0b606c1d)",
            "date": "2026-11-09",
            "verdict": "approved",
            "note": ("Original 2026-11-06 review approved the closed {car, "
                     "code} icon projection (fixed marker cell, retained "
                     "human text, unknown markers fail closed), the "
                     "zero-residual selection, the pinned per-width geometry "
                     "sets, and the body/original matrix wiring. The "
                     "2026-11-08 FS-init follow-up admitted single-quoted "
                     "logLevel directives and accTitle/accDescr scalars: "
                     "four legacy theme fixtures migrated blocked -> "
                     "body-only draw. The 2026-11-09 FS-spacing follow-up "
                     "whitelists non-terminal nodeSpacing/rankSpacing "
                     "envelope leaves: the 22/23 nodespacing/rankspacing "
                     "fixtures migrate body-only -> whole-and-body, so "
                     "whole originals move 17 -> 19 (body-only scope 21 -> "
                     "19); the 38/16 body partition and fixture sha pins "
                     "are unchanged. Re-reviewed within the FS-spacing "
                     "adversarial review (APPROVE, no MAJOR). Final "
                     "cross-cohort-migration re-review (2026-11-09, task "
                     "a91f43e0c0b606c1d, read-only adversarial agent, zero "
                     "builds): APPROVE with zero MAJOR and zero functional "
                     "MINOR; three NITs, the stale-count comments were "
                     "corrected and the shadowed defensive matrix branches "
                     "were proven inert (no run_case, no gap-roster or "
                     "Class285 impact)."),
        },
        "counts": {
            "total": n_draw + n_blocked,
            "predicted_draw": n_draw,
            "predicted_blocked": n_blocked,
            "predicted_original_draw": n_original_draw,
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
