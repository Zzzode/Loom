#!/usr/bin/env python3
"""Generate the RFC 0046 layout/display tuning admission cohort ledger.

Renderer-free. The 11 rows are the fallback originals whose only unadmitted
frontmatter is a closed-value layout/display tuning namespace:

  ishikawa      config.ishikawa.useMaxWidth {true,false} / diagramPadding {50}
  architecture  config.architecture.randomize {true,false} / seed {42} /
                idealEdgeLengthMultiplier {3}
  state (v1/v2) config.state.useMaxWidth {true,false}

None of the three renderers reads these keys; the footer discloses that the
tuning was not applied.

Twins: two rows have a clean corpus twin (same body, drawn without a footer);
the other nine use the synthetic config-free twin (delete the `config:` line
and every following line indented at least two spaces, up to the next column-0
key). The synthetic twin's config-free check is structural — never a substring
search, because the descriptions legitimately mention the key names in prose.

`expected_states.narrow_40` is measured live (rule 34) and never inherited.

Mirror boundary (rule 37): the state model in `admits` is deliberately
stricter than `state_source_envelope` — it additionally requires a description
and rejects a body init directive, which the state gate checks through
`parse_envelope`; the ishikawa and architecture gates require neither. The 11
curated rows satisfy the stricter model, and the C++ gate is the admission
authority.

Usage: python3 tools/mermaid/generate_layouttuning_0046_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/layouttuning-0046-cohort.json")
SCHEMA = "loom_mermaid_layouttuning_0046_cohort/v1"
PHASE = "P1-layout-tuning"

HEADERS = {
    "ishikawa": "Ishikawa native projection v1",
    "architecture": "Architecture native projection v1",
    "state": "State native projection v1",
}
NOTICE_LIMIT = 96

# family -> (originals, drawn_before, admitted_here)
FAMILY_COUNTS = {
    "ishikawa": (13, 9, 3),
    "architecture": (20, 16, 4),
    "state-diagram": (42, 28, 2),
    "state-diagram-v2": (103, 52, 2),
}
GLOBAL = {"originals": 2145, "drawn_before": 1517, "admitted": 11,
          "fallback_before": 628, "fallback_after": 617}

# family -> (row path, namespace, leaves[(key, value)], twin)
COHORT = [
    ("ishikawa",
     "ishikawa/nightly-check/should-render-usemaxwidth-false.mmd",
     [("useMaxWidth", "false")], "synthetic"),
    ("ishikawa",
     "ishikawa/nightly-check/should-render-usemaxwidth-true.mmd",
     [("useMaxWidth", "true")], "synthetic"),
    ("ishikawa",
     "ishikawa/pr-check/9-should-render-with-custom-diagrampadding.mmd",
     [("diagramPadding", "50")], "corpus",
     "ishikawa/pr-check/1-should-render-a-simple-ishikawa-diagram.mmd"),
    ("architecture",
     "architecture/pr-check/should-render-a-deterministic-layout-for-a-complex-deeply-nested-diagram.mmd",
     [("randomize", "false")], "synthetic"),
    ("architecture",
     "architecture/pr-check/should-render-a-deterministic-layout-with-an-explicit-seed-override.mmd",
     [("seed", "42")], "synthetic"),
    ("architecture",
     "architecture/pr-check/should-render-with-randomize-true-without-errors.mmd",
     [("randomize", "true")], "synthetic"),
    ("architecture",
     "architecture/visual-check/should-render-a-linear-chain-with-an-increased-idealedgelengthmultiplier.mmd",
     [("idealEdgeLengthMultiplier", "3")], "corpus",
     "architecture/visual-check/should-render-a-linear-chain-with-default-fcose-knobs.mmd"),
    ("state-diagram",
     "state-diagram/nightly-check/usemaxwidth-false.mmd",
     [("useMaxWidth", "false")], "synthetic"),
    ("state-diagram",
     "state-diagram/nightly-check/usemaxwidth-true.mmd",
     [("useMaxWidth", "true")], "synthetic"),
    ("state-diagram-v2",
     "state-diagram-v2/nightly-check/v2-usemaxwidth-false.mmd",
     [("useMaxWidth", "false")], "synthetic"),
    ("state-diagram-v2",
     "state-diagram-v2/nightly-check/v2-usemaxwidth-true.mmd",
     [("useMaxWidth", "true")], "synthetic"),
]

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

PHASE_REVIEWS = {
    PHASE: {
        "reviewer_agent": "agent:code-review (independent adversarial, read-only)",
        "date": "2026-11-22",
        "verdict": "approved",
        "note": (
            "r1 REQUEST-CHANGES (the 0046 matrix branch was shadowed by the "
            "architecture branch for 4/11 rows; the ishikawa gate and mirror "
            "disagreed on theme+namespace exclusivity; the architecture mirror "
            "skipped description payload lines the gate inspects; a bare "
            "`config` without a colon opened the block; stale design-decisions "
            "and matrix comments; title double-count; mirror-boundary, "
            "structural clean-row detection and composition-coverage NITs) -> "
            "fixed -> r2 APPROVE, re-verified against the affected suites."
        ),
    },
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def _strip_comment(value: str) -> str:
    """Mirror the family scalar helpers: quote-aware `#`; the kept prefix is
    trimmed of spaces/tabs/CR, the no-comment case is returned untouched."""
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


def _scalar(raw: str) -> str:
    value = _strip_comment(raw.strip(" \t\r"))
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ("'", '"'):
        value = value[1:-1]
    return value


def frontmatter_events(text: str):
    """(path, raw value, indent, namespace_entry) events; None when malformed."""
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None
    events = []
    stack = []
    in_description = False
    for index in range(1, len(lines)):
        line = lines[index]
        if line == "---":
            return events
        if not line.strip() or line.strip().startswith("#"):
            continue
        indent = len(line) - len(line.lstrip(" "))
        if indent < len(line) and line[indent] == "\t":
            return None  # the family scans reject a tab indentation
        if in_description:
            if indent > 0:
                continue
            in_description = False
        body = line[indent:]
        colon = body.find(":")
        if colon < 0:
            return None
        key = body[:colon].strip(" \t")
        raw = body[colon + 1:]
        if raw and raw[0] not in " \t":
            return None
        value = raw.strip(" \t\r")
        if indent == 0:
            stack = []
            events.append((key, value, 0, _strip_comment(value) == ""))
            if key == "description":
                in_description = True
            continue
        while stack and stack[-1][0] >= indent:
            stack.pop()
        stack.append((indent, key))
        path = ".".join(part for _, part in stack)
        events.append((path, value, indent, _strip_comment(value) == ""))
    return None  # unclosed fence


def _body_has_init(text: str) -> bool:
    lines = text.split("\n")
    close = next((i for i in range(1, len(lines)) if lines[i] == "---"), None)
    if close is None:
        return False
    for line in lines[close + 1:]:
        stripped = line.lstrip(" \t")
        start = line.find("%%{")
        if stripped.startswith("%%{") and line.find("}%%", start) != -1:
            return True
    return False


# family -> namespace name and closed leaf/value table
GATE_MODELS = {
    "ishikawa": ("ishikawa", {"useMaxWidth": {"true", "false"},
                              "diagramPadding": {"50"}}),
    "architecture": ("architecture", {"randomize": {"true", "false"},
                                      "seed": {"42"},
                                      "idealEdgeLengthMultiplier": {"3"}}),
    "state": ("state", {"useMaxWidth": {"true", "false"}}),
}


def admits(text: str, family: str):
    """Return the admitted leaves in canonical order, or None."""
    namespace, table = GATE_MODELS[family]
    if not text.startswith("---\n") or _body_has_init(text):
        return None
    events = frontmatter_events(text)
    if events is None:
        return None
    description_seen = config_seen = namespace_seen = False
    leaves = {}
    order = []
    for path, value, indent, entry in events:
        if indent == 0:
            if path == "description":
                if description_seen:
                    return None
                description_seen = True
                continue
            if path == "config":
                if config_seen or not entry:
                    return None
                config_seen = True
                continue
            return None
        if not config_seen:
            return None
        if entry:
            if path != namespace or indent != 2 or namespace_seen:
                return None
            namespace_seen = True
            continue
        if indent != 4 or not namespace_seen:
            return None
        key = path.split(".")[-1]
        if path != f"{namespace}.{key}" or key not in table or key in leaves:
            return None
        scalar = _scalar(value)
        if scalar not in table[key]:
            return None
        leaves[key] = scalar
        order.append(key)
    if not description_seen or not config_seen or not namespace_seen or not leaves:
        return None
    # The gates emit the footer in a canonical order regardless of source order.
    canonical = [key for key in table if key in leaves]
    return [(key, leaves[key]) for key in canonical]


def config_free_twin(text: str) -> str:
    """Delete the `config:` line and every following indent>=2 line up to the
    next column-0 key (or the closing fence)."""
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return text
    out = [lines[0]]
    skipping = False
    for line in lines[1:]:
        if skipping and len(line) >= 2 and line[0] == " " and line[1] == " ":
            continue
        skipping = False
        if line.startswith("config:"):
            skipping = True
            continue
        out.append(line)
    return "\n".join(out)


def twin_is_config_free(text: str) -> bool:
    """Structural check: no `config:` line, no namespace/leaf line remains."""
    close = text.find("\n---\n", 4)
    if not text.startswith("---\n") or close < 0:
        return False
    for line in text[4:close].split("\n"):
        stripped = line.lstrip(" ")
        if line.startswith("config:") or stripped.startswith(("ishikawa:", "architecture:",
                                                             "state:", "useMaxWidth:",
                                                             "diagramPadding:", "randomize:",
                                                             "seed:", "idealEdgeLengthMultiplier:")):
            return False
    return True


def footer_lines(family: str, leaves) -> list:
    header = HEADERS["state" if family.startswith("state-diagram") else family]
    lines = [header]
    for key, value in leaves:
        if key == "useMaxWidth" and family.startswith("state-diagram"):
            lines.append(f"config.state.useMaxWidth={value} -> display sizing; not applied")
        elif key == "useMaxWidth":
            lines.append(f"config.ishikawa.useMaxWidth={value} -> display sizing; not applied")
        elif key == "diagramPadding":
            lines.append(f"config.ishikawa.diagramPadding={value} -> diagram padding; not applied")
        else:
            lines.append(f"config.architecture.{key}={value} -> layout tuning; not applied")
    return lines


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n"
                      for entry in sorted(entries, key=lambda entry: entry["path"]))
    return sha256_bytes(payload.encode())


def build_review(repo_root):
    review = PHASE_REVIEWS[PHASE]
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

    if len(COHORT) != GLOBAL["admitted"]:
        raise ValueError("cohort size drift")
    per_family = {}
    for entry in COHORT:
        per_family[entry[0]] = per_family.get(entry[0], 0) + 1
    for family, (originals, drawn_before, admitted) in FAMILY_COUNTS.items():
        if per_family.get(family, 0) != admitted:
            raise ValueError(f"{family} admitted count drift")
        if drawn_before + admitted > originals:
            raise ValueError(f"{family} arithmetic drift")

    rows = []
    for entry in COHORT:
        family, rel, expected_leaves = entry[0], entry[1], entry[2]
        twin_kind = entry[3]
        text = (repo_root / CASES / rel).read_text()
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        leaves = admits(text, "state" if family.startswith("state-diagram") else family)
        if leaves is None:
            raise ValueError(f"{rel} is not admitted by the {family} gate shape")
        if leaves != [tuple(pair) for pair in expected_leaves]:
            raise ValueError(f"{rel} leaf set drift: {leaves}")
        if twin_kind == "corpus":
            twin_rel = entry[4]
            twin_text = (repo_root / CASES / twin_rel).read_text()
            if frontmatter_stripped(twin_text) != frontmatter_stripped(text):
                raise ValueError(f"{rel} corpus twin is not body-identical")
            twin = {"kind": "corpus", "path": twin_rel,
                    "sha256": sha256_file(repo_root / CASES / twin_rel)}
        else:
            twin_source = config_free_twin(text)
            if not twin_is_config_free(twin_source):
                raise ValueError(f"{rel} synthetic twin still carries config")
            twin = {"kind": "synthetic",
                    "rule": ("delete the config: line and every following indent>=2 "
                             "line up to the next column-0 key"),
                    "sha256": sha256_bytes(twin_source.encode())}
        narrow = "drawn" if rel in NARROW_DRAW else "source_fallback"
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
                "narrow_40": narrow,
                "restored_240": "drawn",
            },
            "expected_footer_lines": footer_lines(family, leaves),
            "twin": twin,
            "ledger_note": (
                "closed-value layout/display tuning namespace admitted by RFC "
                "0046; the renderer never reads the key and the footer discloses "
                "that the tuning was not applied"),
        })

    return {
        "schema": SCHEMA,
        "rfc": 46,
        "phase": PHASE,
        "admission": {
            "shape": ("folded description envelope + one config block carrying "
                      "exactly one tuning namespace and its closed leaves"),
            "tables": {
                "config.ishikawa.useMaxWidth": ["true", "false"],
                "config.ishikawa.diagramPadding": ["50"],
                "config.architecture.randomize": ["true", "false"],
                "config.architecture.seed": ["42"],
                "config.architecture.idealEdgeLengthMultiplier": ["3"],
                "config.state.useMaxWidth": ["true", "false"],
            },
            "footer": {"headers": sorted(set(HEADERS.values())),
                       "line_limit_bytes": NOTICE_LIMIT},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("corpus twin where a body-identical drawn fixture exists "
                          "(2 rows); synthetic config-free twin otherwise (9 rows)"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("fallback rows whose only unadmitted config leaf is a closed-value "
                     "layout/display tuning namespace"),
            "families": {family: {"originals": originals,
                                  "drawn_before": drawn_before,
                                  "admitted": admitted}
                         for family, (originals, drawn_before, admitted) in
                         FAMILY_COUNTS.items()},
            "global": GLOBAL,
            "path_pin": [entry[1] for entry in COHORT],
            "cohort_digest": cohort_digest(rows),
            "remaining_by_category": {
                "family_residual:ishikawa": 0,
                "family_residual:architecture": 0,
                "family_residual:state-diagram": 7,
                "family_residual:state-diagram-v2": 20,
                "dagre_layout_tuning": 17,
            },
        },
        "counts": {
            "originals": sum(v[0] for v in FAMILY_COUNTS.values()),
            "drawn": sum(v[1] for v in FAMILY_COUNTS.values()),
            "admitted": GLOBAL["admitted"],
            "remaining": sum(v[0] - v[1] - v[2] for v in FAMILY_COUNTS.values()),
        },
        "review": build_review(repo_root),
        "rows": rows,
    }


def frontmatter_stripped(text: str) -> str:
    if not text.startswith("---\n"):
        return text
    close = text.find("\n---\n", 4)
    return text if close < 0 else text[close + 5:]


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
