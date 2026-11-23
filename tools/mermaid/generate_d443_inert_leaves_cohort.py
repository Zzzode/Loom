#!/usr/bin/env python3
"""Generate the D-443 inert look/htmlLabels/layout admission cohort ledger.

Renderer-free. The six rows are the last requirement, ishikawa and venn
fallbacks whose bodies are byte-identical to their config-free twins and whose
only unsupported frontmatter shape is a closed inert leaf group:

* requirement -- `config.flowchart.htmlLabels` (with or without an already
  admitted `look`/`layout` leaf): the native requirement projection is a text
  tree and never emits HTML;
* ishikawa -- `config.look: handDrawn`: the fishbone projection paints its own
  borders and never reads the sketch style;
* venn -- `config.look: handDrawn` plus `handDrawnSeed` and `fontFamily:
  courier`: the venn projection paints its own circles and tokens.

Every leaf value is drawn from its closed corpus table. The footer says the
leaf was not applied, and the art must equal the config-free twin.

Usage: python3 tools/mermaid/generate_d443_inert_leaves_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/d443-inert-leaves-cohort.json")
SCHEMA = "loom_mermaid_d443_inert_leaves_cohort/v1"
DECISION = "D-443"

HEADERS = {
    "requirement": "Requirement native projection v1",
    "ishikawa": "Ishikawa native projection v1",
    "venn": "Venn native projection v1",
}
NOTICE_LIMIT = 96

# Per family: ordered (dotted config path, closed value table, footer template).
# The ordering mirrors each renderer's append site; new leaves are appended
# after every pre-existing leaf so no drawn frame's footer changes.
LEAF_ORDER = {
    "requirement": [
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
        ("config.flowchart.htmlLabels", {"false", "true"},
         "config.flowchart.htmlLabels={} -> native text labels; not applied"),
    ],
    "ishikawa": [
        ("config.theme", {"dark", "forest"}, "config.theme={} -> colour palette; not applied"),
        ("config.ishikawa.useMaxWidth", {"true", "false"},
         "config.ishikawa.useMaxWidth={} -> display sizing; not applied"),
        ("config.ishikawa.diagramPadding", {"50"},
         "config.ishikawa.diagramPadding={} -> diagram padding; not applied"),
        ("config.look", {"handDrawn"}, "config.look={} -> source node style; not applied"),
    ],
    "venn": [
        ("config.theme", {"dark", "redux-color", "redux-dark-color"},
         "config.theme={} -> colour palette; not applied"),
        ("config.look", {"handDrawn"}, "config.look={} -> source node style; not applied"),
        ("config.handDrawnSeed", "<seed>",
         "config.handDrawnSeed={} -> sketch jitter seed; not applied"),
        ("config.fontFamily", {"courier"},
         'config.fontFamily="courier" -> terminal font; not applied'),
    ],
}

COHORT = [
    ("requirement",
     "requirement/nightly-check/"
     "edge-labels-correctly-when-flowchart-htmllabels-is-false-handdrawn-look.mmd"),
    ("requirement",
     "requirement/nightly-check/"
     "edge-labels-correctly-when-flowchart-htmllabels-is-false-default.mmd"),
    ("requirement",
     "requirement/nightly-check/"
     "edge-labels-correctly-when-flowchart-htmllabels-is-false-elk-layout.mmd"),
    ("ishikawa", "ishikawa/pr-check/6-should-render-with-handdrawn-look.mmd"),
    ("venn",
     "venn/nightly-check/handdrawn/"
     "14-should-render-a-two-set-venn-diagram-with-handdrawn-look.mmd"),
    ("venn",
     "venn/pr-check/15-should-render-a-three-set-venn-with-handdrawn-look-and-title.mmd"),
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

REVIEW = {
    "reviewer_agent": (
        "independent read-only adversarial code review (fresh general-purpose "
        "agent, D-443 inert look/htmlLabels/layout admission, 2026-11-23)"),
    "date": "2026-11-23",
    "verdict": "approved",
    "note": (
        "Reviewer re-ran the generator --check entry and the live gate and "
        "verified: each of the three closed tables matches its renderer branch; "
        "the footer append sites keep every pre-existing leaf order; a namespace "
        "without its leaf fails closed; the twin is the same file with the whole "
        "config block stripped and the frame equality oracle excludes the footer; "
        "the negative probes stay whole-source."),
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


def split_frontmatter(text: str):
    """(frontmatter, body) or None when the exact `---` fence is absent."""
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None


def config_free_twin(text: str) -> str:
    """The same source with the whole top-level `config:` block removed."""
    parts = split_frontmatter(text)
    if parts is None:
        return text
    frontmatter, body = parts
    kept = []
    skipping = False
    for line in frontmatter.split("\n"):
        if skipping and line.startswith(" "):
            continue
        skipping = line.startswith("config:")
        if skipping:
            continue
        kept.append(line)
    return "---\n" + "\n".join(kept) + "\n---\n" + body


def frontmatter_leaves(text: str):
    """(dotted path, raw value, indent) triples; None when malformed."""
    parts = split_frontmatter(text)
    if parts is None:
        return None
    frontmatter, _ = parts
    leaves = []
    stack = []
    for line in frontmatter.split("\n"):
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
    return leaves


def _scalar(raw: str) -> str:
    value = raw.strip(" \t")
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
            value = value[:n].strip(" \t")
            break
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
        value = value[1:-1]
    return value


def seed_ok(value: str) -> bool:
    """Mirror the D-442 seed rule: 1..9 decimal digits, not all zero."""
    return bool(value) and len(value) <= 9 and value.isdigit() and value.strip("0") != ""


def _table(family: str, path: str):
    return next((allowed for leaf, allowed, _ in LEAF_ORDER[family] if leaf == path), None)


def admits_requirement(leaves) -> bool:
    top = set()
    config_seen = False
    fields = set()
    namespace = set()
    for path, raw, indent in leaves:
        parts = path.split(".")
        if len(parts) == 1:
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
        if not config_seen:
            return False
        if len(parts) == 2 and parts[0] == "config" and indent == 2 and not raw.strip():
            if path != "config.flowchart" or "flowchart" in namespace:
                return False
            namespace.add("flowchart")
            continue
        expected_indent = 4 if len(parts) == 3 else 2
        if len(parts) > 3 or indent != expected_indent or path in fields:
            return False
        table = _table("requirement", path)
        if table is None or _scalar(raw) not in table:
            return False
        if "flowchart" in parts and "flowchart" not in namespace:
            return False
        fields.add(path)
    if "description" not in top:
        return False
    if config_seen and not fields:
        return False
    if "flowchart" in namespace and "config.flowchart.htmlLabels" not in fields:
        return False
    return True


def admits_flat(leaves, family: str) -> bool:
    top = set()
    config_seen = False
    fields = set()
    namespace = set()
    for path, raw, indent in leaves:
        parts = path.split(".")
        if len(parts) == 1:
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
        if not config_seen:
            return False
        if len(parts) == 2 and parts[0] == "config" and indent == 2 and not raw.strip():
            # The ishikawa tuning namespace is the only nested block.
            if family != "ishikawa" or path != "config.ishikawa" or "ishikawa" in namespace:
                return False
            namespace.add("ishikawa")
            continue
        table = _table(family, path)
        if table is None or path in fields:
            return False
        value = _scalar(raw)
        if table == "<seed>":
            if not seed_ok(value):
                return False
        elif value not in table:
            return False
        if len(parts) == 3:
            if indent != 4 or parts[1] != "ishikawa" or "ishikawa" not in namespace:
                return False
        elif indent != 2:
            return False
        fields.add(path)
    if "description" not in top:
        return False
    if config_seen and not fields:
        return False
    if "ishikawa" in namespace and not any(
            leaf.startswith("config.ishikawa.") for leaf in fields):
        return False
    return True


def admits(family: str, leaves) -> bool:
    if family == "requirement":
        return admits_requirement(leaves)
    return admits_flat(leaves, family)


def footer_lines(family: str, leaves) -> list:
    mapping = {path: _scalar(raw) for path, raw, _ in leaves}
    header = HEADERS[family]
    lines = [header]
    for leaf, _, template in LEAF_ORDER[family]:
        if leaf in mapping:
            lines.append(template.format(mapping[leaf]))
    return lines if len(lines) > 1 else []


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                      for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


ADMISSION_SHAPE = {
    "requirement": ("description envelope + closed config leaves including the "
                    "indent-4 config.flowchart.htmlLabels leaf"),
    "ishikawa": "closed config.theme / config.ishikawa namespace / config.look leaves",
    "venn": "closed config.theme / config.look / config.handDrawnSeed / config.fontFamily leaves",
}


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    rows = []
    for family, rel in COHORT:
        source = (repo_root / CASES / rel).read_text()
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        leaves = frontmatter_leaves(source)
        if leaves is None or not admits(family, leaves):
            raise ValueError(f"{rel} is not admitted by the {family} gate shape")
        footer = footer_lines(family, leaves)
        if not footer:
            raise ValueError(f"{rel} carries no admitted leaf")
        body = strip_frontmatter(source)
        twin = config_free_twin(source)
        if strip_frontmatter(twin) != body:
            raise ValueError(f"{rel} twin body drift")
        if frontmatter_leaves(twin) is None:
            raise ValueError(f"{rel} twin frontmatter is malformed")
        rows.append({
            "path": rel,
            "family": family,
            "sha256": observed_sha,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": "source_fallback",
            "prediction": "predicted-draw",
            "track_scope": "whole-only",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "drawn" if rel in NARROW_DRAW else "source_fallback",
                "restored_240": "drawn",
            },
            "expected_footer_lines": footer,
            "twin": {
                "kind": "config-free",
                "path": rel,
                "sha256": observed_sha,
                "footer_lines": [],
            },
            "ledger_note": (
                "inert closed-leaf admission; the family projection never reads "
                "the leaf and the footer discloses it"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "admission": {
            "shape": ADMISSION_SHAPE,
            "footer_line_limit_bytes": NOTICE_LIMIT,
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("the config-free twin is the same fixture with the whole "
                          "top-level config block removed; the frame oracle strips "
                          "the source footer and compares cell-exact art"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("remaining requirement/ishikawa/venn fallbacks whose body is "
                     "byte-identical to the config-free twin and whose only "
                     "unsupported frontmatter shape is a closed inert leaf"),
            "cohort_digest": cohort_digest(rows),
        },
        "counts": {
            "total": len(rows),
            "by_family": {"requirement": 3, "ishikawa": 1, "venn": 2},
        },
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
    sys.exit(main())
