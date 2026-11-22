#!/usr/bin/env python3
"""Generate the RFC 0031 architecture-family renderer cohort ledger.

The architecture renderer (mermaid_architecture_impl.cpp) projects the
architecture-beta grammar (group/service/junction declarations with optional
icon and label and `in` containment, L/R/T/B port edges in directed,
undirected and labeled forms, group-boundary edges, and align directives) to
a bounded native outline. It has no layout/edge-routing/color contract, so
fixtures carrying a frontmatter theme/look/config key stay on whole-source
fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / frontmatter-gated partition; manifest shas are pinned; the review
block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_architecture_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/architecture")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/architecture-cohort.json")
SCHEMA = "loom_mermaid_architecture_cohort/v1"

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_architecture_cohort.cpp. Populated from live
# measurement; edited only after a live re-measure (engineering rule 18).
# The same ten compact fixtures draw at 40 columns on both tracks; the six
# larger/deeply-nested diagrams exceed the narrow native frame and stay
# source there.
_NARROW_DRAW = {
    "architecture/nightly-check/"
    "should-align-siblings-in-a-row-when-their-edges-feed-a-common-downstream-node.mmd",
    "architecture/nightly-check/"
    "should-render-a-grid-via-combined-row-column-alignments.mmd",
    "architecture/nightly-check/"
    "should-render-an-architecture-diagram-with-directional-arrows.mmd",
    "architecture/nightly-check/"
    "should-render-a-simple-architecture-diagram-with-titleandaccessibilities.mmd",
    "architecture/nightly-check/should-render-edges-at-correct-length.mmd",
    "architecture/nightly-check/"
    "should-stack-three-same-port-databases-in-a-column-without-overlap.mmd",
    "architecture/pr-check/"
    "should-render-an-architecture-diagram-with-edge-labels.mmd",
    "architecture/pr-check/"
    "should-render-an-architecture-diagram-with-split-directioning.mmd",
    "architecture/pr-check/"
    "should-render-a-simple-architecture-diagram-with-groups.mmd",
    "architecture/visual-check/"
    "should-render-a-linear-chain-with-default-fcose-knobs.mmd",
}
_NARROW_DRAW_WHOLE: set[str] = set(_NARROW_DRAW)
_NARROW_DRAW_BODY: set[str] = set(_NARROW_DRAW)
# RFC 0046 tuning rows measured at 40 columns: the seed override and the
# increased-edge-length linear chain fit the narrow body frame (their whole
# sources carry the disclosure footer and stay source there).
_NARROW_DRAW_BODY |= {
    "architecture/pr-check/"
    "should-render-a-deterministic-layout-with-an-explicit-seed-override.mmd",
    "architecture/visual-check/"
    "should-render-a-linear-chain-with-an-increased-idealedgelengthmultiplier.mmd",
}
# Body tracks whose native frame is surface-identical to a plain text code
# block at every width (none in this family: every clean body has content).
_BODY_SOURCE: set[str] = set()


def split_frontmatter(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None, text


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


# RFC 0046: the one admitted frontmatter shape is a column-0 `config:` block
# carrying exactly one `architecture` namespace with its closed tuning leaves.
# Structural mirror of the C++ scan; any other config/theme/look shape gates.
_TUNING = {"randomize": {"true", "false"}, "seed": {"42"},
           "idealEdgeLengthMultiplier": {"3"}}


def _strip_comment(value):
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


def _scalar(raw):
    value = _strip_comment(raw.strip(" \t\r"))
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
        value = value[1:-1]
    return value


def admits_tuning(frontmatter):
    """True when the block is exactly the admitted architecture tuning shape."""
    if frontmatter is None:
        return False
    in_config = saw_config = saw_namespace = in_description = False
    leaves = {}
    for line in frontmatter.split("\n"):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        indent = len(line) - len(line.lstrip(" "))
        if indent < len(line) and line[indent] == "\t":
            return False
        if in_description:
            if indent > 0:
                # The C++ scan still gates a payload line that begins with a
                # visual key; ordinary prose is ignored.
                stripped = line.lstrip()
                if stripped.startswith(("theme:", "look:", "config:")) or \
                        stripped in ("theme", "look", "config"):
                    return False
                continue
            in_description = False
        body = line[indent:]
        colon = body.find(":")
        if colon < 0:
            return False
        key, raw = body[:colon], body[colon + 1:]
        if raw and raw[0] not in " \t":
            return False
        value = _scalar(raw)
        if indent == 0:
            in_config = False
            if key == "description":
                in_description = True
            elif key == "config":
                if saw_config or value != "":
                    return False
                saw_config = True
                in_config = True
            elif key in ("theme", "look"):
                return False
        elif not in_config:
            if key in ("theme", "look", "config"):
                return False
        elif indent == 2 and key == "architecture" and value == "":
            if saw_namespace:
                return False
            saw_namespace = True
        elif indent == 4 and saw_namespace and key in _TUNING:
            if key in leaves or value not in _TUNING[key]:
                return False
            leaves[key] = value
        else:
            return False
    if not saw_config:
        return False
    if saw_namespace and not leaves:
        return False
    if saw_config and not saw_namespace:
        return False
    return bool(leaves)


def gate_kind(frontmatter, body):
    """Return 'frontmatter' or None. architecture-beta has no in-body visual
    statements in the pinned corpus: no %%{init}%%, classDef, ::: or style.
    RFC 0046: the closed tuning namespace is admitted (draws with a footer)."""
    if frontmatter is not None and not admits_tuning(frontmatter) and \
            re.search(r"^\s*(theme|look|config)\s*:", frontmatter, re.M):
        return "frontmatter"
    if re.search(r"^\s*%%\{init", body, re.M) or \
            re.search(r"^\s*classDef", body, re.M) or ":::" in body or \
            re.search(r"^\s*style\s", body, re.M):
        return "body_style"
    return None


# Always-current narrative for the preserved review block.
REVIEW_NOTE = (
    "APPROVE with zero MAJOR at the 2026-11-13 census (16 draw / 4 "
    "frontmatter-gated / 0 body-gated; the reviewer independently reproduced "
    "the partition, ported the validators and ran targeted, final and 60000 "
    "fuzz probes with no fail-open or crash; MINOR 1/2 and NIT 6 applied, "
    "MINOR 3/NIT 4/NIT 5 safe-direction). RFC 0046 (2026-11-22) later "
    "admitted the 4 `config.architecture.*` tuning rows through the closed "
    "namespace gate with a disclosure footer, so the live partition is "
    "20 draw / 0 frontmatter-gated / 0 body-gated.")


def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, architecture "
                           "renderer cohort, 2026-11-13, task "
                           "a5602134897c4359d)"),
        "date": "2026-11-13",
        "verdict": "approved",
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
    # The reviewer/date/verdict are preserved; the note always states the
    # current live partition so it cannot go stale as cohorts are admitted.
    review = dict(review)
    review["note"] = REVIEW_NOTE
    return review


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    draw, fm_gate, body_gate = set(), set(), set()
    for path in sorted((repo_root / CASES).rglob("*.mmd")):
        text = path.read_text()
        rel = path.relative_to(
            repo_root / "tests/fixtures/mermaid_official/cases").as_posix()
        frontmatter, body = split_frontmatter(text)
        kind = gate_kind(frontmatter, body)
        if kind == "frontmatter":
            fm_gate.add(rel)
        elif kind == "body_style":
            body_gate.add(rel)
        else:
            draw.add(rel)

    # RFC 0046 admitted the 4 frontmatter-gated tuning rows: 20/0/0.
    if (len(draw), len(fm_gate), len(body_gate)) != (20, 0, 0):
        raise ValueError(
            f"architecture partition drift: draw={len(draw)} "
            f"frontmatter={len(fm_gate)} body_style={len(body_gate)} "
            f"(expected 20/0/0)")

    rows = []
    for rel in sorted(draw):
        observed_sha = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        whole_narrow = "drawn" if rel in _NARROW_DRAW_WHOLE else "source"
        if rel in _BODY_SOURCE:
            body_states = {"wide_240": "source", "narrow_40": "source",
                           "restored_240": "source"}
        else:
            body_states = {
                "wide_240": "drawn",
                "narrow_40": "drawn" if rel in _NARROW_DRAW_BODY else "source",
                "restored_240": "drawn",
            }
        rows.append({
            "path": rel,
            "family": "architecture",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": "whole-and-body",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": whole_narrow,
                "restored_240": "drawn",
            },
            "expected_body_states": body_states,
            "ledger_note": (
                "architecture renderer projects groups/services/junctions/"
                "edges/aligns to a bounded native outline; no visual gate is "
                "present so it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "architecture-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_architecture_impl.cpp",
            "grammar": ("architecture-beta; group/service/junction with "
                        "optional icon/label and in-containment, L/R/T/B "
                        "directed/undirected/labeled edges, align row/column"),
            "theme_gate": (
                "an unrecognized frontmatter theme/look/config shape, an "
                "%%{init}%% directive or a body style/classDef/::: statement "
                "keeps the whole original on source fallback; RFC 0046 admits "
                "exactly one `architecture` tuning namespace, disclosed "
                "through a footer"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": ("architecture fixtures with no frontmatter gate (the RFC "
                     "0046 tuning namespace is admitted) and no body style "
                     "statement"),
            "roots": ["architecture"],
            "draw_count": len(draw),
            "frontmatter_gated_count": len(fm_gate),
            "frontmatter_gated": sorted(fm_gate),
            "body_style_gated_count": len(body_gate),
            "body_style_gated": sorted(body_gate),
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
