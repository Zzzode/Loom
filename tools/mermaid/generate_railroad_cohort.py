#!/usr/bin/env python3
"""Generate the RFC 0031 railroad-family renderer cohort ledger.

The railroad renderer (mermaid_railroad_impl.cpp) projects the four railroad
dialects (railroad-beta IR nested calls, and the railroad-abnf/ebnf/peg-beta
textual notations) to a bounded native outline: each grammar rule is
validated by a recursive-descent parser and its physical lines are
transcribed verbatim. It has no track-geometry or color contract, so fixtures
carrying a frontmatter theme/look/config key or a body style statement stay
on whole-source fallback.

Renderer-free; the durable gtest enforces every state live. Drift guards pin
the draw / frontmatter-gated / body-style-gated partition; manifest shas are
pinned; the review block is preserved across regeneration.

Usage: python3 tools/mermaid/generate_railroad_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases/railroad")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/railroad-cohort.json")
SCHEMA = "loom_mermaid_railroad_cohort/v1"
_FENCE = re.compile(r"^---[ \t]*$")

# Per-width geometry facts not derivable without rendering; enforced live by
# tests/ui/test_mermaid_railroad_cohort.cpp. Populated from live measurement;
# edited only after a live re-measure (engineering rule 18). At 40 columns
# only the two compact frames whose every line is <=40 columns (the one-rule
# IR smoke test and the PEG fixture, max line 23/40) keep a native surface on
# both tracks; the other six carry rule lines beyond 40 columns and stay
# source on the narrow track.
_NARROW_DRAW = {
    "railroad/pr-check/ir-a-simple-rule.mmd",
    "railroad/pr-check/peg-ordered-choice-and-suffixes.mmd",
}
_NARROW_DRAW_BODY: set[str] = set(_NARROW_DRAW)
_BODY_SOURCE: set[str] = set()


def split_frontmatter(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if _FENCE.fullmatch(lines[close]):
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None, text


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()



# RFC 0043: the admitted frontmatter shape is exactly one column-0 `config:`
# block whose sole leaf is an indent-2 `theme` in this family's closed table.
THEME_TABLE = {"dark"}
TRIM_COLUMN0_KEY = True


def _theme_scalar(raw):
    """Mirror the C++ TU-local af_scalar copy (renderer-free)."""
    value = raw.strip(" \t")
    quote = None
    cut = None
    for n, c in enumerate(value):
        if quote is not None:
            if c == quote:
                quote = None
            continue
        if c in ("'", '"'):
            quote = c
            continue
        if c == "#" and (n == 0 or value[n - 1] in " \t"):
            cut = n
            break
    if cut is not None:
        value = value[:cut].strip(" \t")
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
        value = value[1:-1]
    return value


def frontmatter_theme(frontmatter):
    """Return ('none' | 'admitted' | 'gated', theme).

    Mirrors the C++ gate: a column-0 config block admits exactly one indent-2
    theme leaf from the closed table; every other visual key gates.
    """
    if frontmatter is None:
        return "none", None
    in_config = False
    saw_config = False
    theme = None
    for line in frontmatter.split("\n"):
        # Blank means spaces/tabs only. The renderers pre-reject any tab in the
        # source, so the C++ space-only key scan and this predicate agree on
        # every reachable input; no fixture in the three families has a tab.
        if line.strip(" \t") == "":
            continue
        key_start = len(line) - len(line.lstrip(" "))
        if key_start != 0:
            leaf = line[key_start:]
            if in_config:
                if leaf.startswith("#"):
                    continue
                colon = leaf.find(":")
                if colon < 0 or leaf[:colon] != "theme" or key_start != 2 or theme is not None:
                    return "gated", None
                value = _theme_scalar(leaf[colon + 1:])
                if value not in THEME_TABLE:
                    return "gated", None
                theme = value
            else:
                colon = leaf.find(":")
                key = leaf[:colon] if colon >= 0 else leaf
                if TRIM_COLUMN0_KEY:
                    key = key.strip(" ")
                if key in ("theme", "look", "config"):
                    return "gated", None
            continue
        if line.startswith("#"):
            continue
        colon = line.find(":")
        key = line[:colon] if colon >= 0 else line
        if TRIM_COLUMN0_KEY:
            key = key.strip(" ")
        if key == "config":
            if saw_config:
                return "gated", None
            saw_config = True
            in_config = True
        else:
            if key in ("theme", "look"):
                return "gated", None
            in_config = False
    if saw_config and theme is None:
        return "gated", None
    return ("admitted", theme) if theme is not None else ("none", None)


def gate_kind(frontmatter, body):
    """Return 'frontmatter', 'admitted', 'body_style' or None."""
    verdict, _theme = frontmatter_theme(frontmatter)
    if verdict == "gated":
        return "frontmatter"
    if re.search(r"^\s*%%\{init", body, re.M) or \
            re.search(r"^\s*classDef", body, re.M) or ":::" in body or \
            re.search(r"^\s*style\s", body, re.M):
        return "body_style"
    if verdict == "admitted":
        return "admitted"
    return None


def build_review(repo_root):
    review = {
        "reviewer_agent": ("independent read-only adversarial review "
                           "(fresh general-purpose agent, railroad renderer "
                           "cohort, 2026-11-13, task a1214613ac79ed6d6)"),
        "date": "2026-11-13",
        "verdict": "approved",
        "note": ("APPROVE with zero MAJOR. The reviewer independently "
                 "reproduced the 8/1/0 partition across the four dialects, "
                 "ported both the IR recursive-descent parser and the three "
                 "text expression parsers to a simulator with an inverse "
                 "oracle, and ran 115 hand probes, 1,600 generated sources, "
                 "about 158,000 fuzzed/mutation/random-byte inputs and cap "
                 "boundary checks (256/257 rules, depth 64/65, 512/513 text "
                 "bytes, source 65536, width 240/241, Work=0) with no "
                 "fail-open, crash, hang, non-verbatim emission or "
                 "quadratic blowup. Two MINORs were applied: (1) a quoted "
                 "literal accepted C0 control bytes (NUL/ESC/VT) that the "
                 "terminal renders as invisible cells, so parse_string now "
                 "requires printable ASCII (0x20-0x7E) with three control "
                 "regression pins; (2) the ABNF repetition is documented as "
                 "the RFC 5234 prefix form '*(...)' only — a postfix '*' is "
                 "an EBNF/PEG convention and stays fail-closed, matching "
                 "the single ABNF fixture. The NITs (frontmatter-CRLF "
                 "safe-direction fallback, the re-sliced header line, the "
                 "nominal row-count off-by-one already re-gated downstream, "
                 "and lenient-but-faithful parses such as empty IR args) "
                 "are fail-closed or faithful with no corpus sample and "
                 "match the approved siblings, so they are unchanged. The "
                 "frontmatter key scanner trims before comparing, closing "
                 "the cynefin-era 'config : 42' spacing bypass from the "
                 "first draft."),
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
    return review


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}

    draw, admitted, fm_gate, body_gate = set(), set(), set(), set()
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
        elif kind == "admitted":
            admitted.add(rel)
        else:
            draw.add(rel)

    if (len(draw), len(admitted), len(fm_gate), len(body_gate)) != (8, 1, 0, 0):
        raise ValueError(
            f"railroad partition drift: draw={len(draw)} "
            f"admitted={len(admitted)} frontmatter={len(fm_gate)} "
            f"body_style={len(body_gate)} (expected 8/1/0/0)")

    admitted_digest = hashlib.sha256("".join(
        f"{rel}\t{manifest_rows[rel]['sha256']}\n" for rel in sorted(admitted)
    ).encode()).hexdigest()

    rows = []
    for rel in sorted(draw):
        observed_sha = sha256_file(
            repo_root / "tests/fixtures/mermaid_official/cases" / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
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
            "family": "railroad",
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": "whole-and-body",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn" if rel in _NARROW_DRAW else "source",
                "restored_240": "drawn",
            },
            "expected_body_states": body_states,
            "ledger_note": (
                "railroad renderer validates the four dialect grammars and "
                "transcribes the rules verbatim; no visual gate is present "
                "so it draws"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "railroad-family-renderer",
        "renderer": {
            "module": "src/ui/visual/mermaid_railroad_impl.cpp",
            "grammar": ("railroad-beta IR nested calls; railroad-abnf/ebnf/"
                        "peg-beta textual notations"),
            "theme_gate": (
                "a frontmatter theme/look/config key, an %%{init}%% directive "
                "or a body style/classDef/::: statement keeps the whole "
                "original on source fallback"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": "railroad fixtures with no frontmatter gate and no body style",
            "roots": ["railroad"],
            "admission_rfc": 43,
            "draw_count": len(draw),
            "admitted_count": len(admitted),
            "admitted": sorted(admitted),
            "admitted_digest": admitted_digest,
            "d453_admitted_count": 0,
            "d453_admitted": [],
            "family_total": len(draw) + len(admitted) + len(fm_gate) + len(body_gate),
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
