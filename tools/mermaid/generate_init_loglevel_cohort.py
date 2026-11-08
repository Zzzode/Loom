#!/usr/bin/env python3
"""Generate the RFC 0031 FS-init logLevel/accessibility cohort ledger.

Four legacy "graph TD" theme fixtures (dark/default/forest/neutral) fell
back on two body-only parser gaps, both non-visual:

1. The body `%%{init ...}%%` directive only accepted a strict,
   double-quoted `{"flowchart":{"htmlLabels":bool}}` payload. The legacy
   spelling is single-quoted JSON5 (`{ 'logLevel': 0} }`); logLevel only
   selects logger verbosity and has no rendering effect.
2. The bare flowchart parser rejected `accTitle:`/`accDescr:` scalar
   statements, which upstream exposes only to screen readers.

Both are now consumed through closed whitelists (the directive
normalizer mirrors the class family's class_init_meta; accessibility
scalars mirror its class_accessibility contract), so the four bodies
draw. Their whole originals stay source: the frontmatter sets
`config.theme`, which matches no whole-source envelope admission
profile. The four "new-syntax" siblings are not selected: their
directive also carries a `theme` key (outside the whitelist) and their
bodies bind x/o edge markers, so they stay fail-closed.

Usage: python3 tools/mermaid/generate_init_loglevel_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/init-loglevel-cohort.json")

SCHEMA = "loom_init_loglevel_cohort/v1"

LOG_LEVEL_NAMES = {"trace", "debug", "info", "warn", "error", "fatal"}
BODY_ONLY_FILES = {
    "flowchart/visual-check/dark-theme-legacy-graph.mmd",
    "flowchart/visual-check/default-theme-legacy-graph.mmd",
    "flowchart/visual-check/forest-theme-legacy-graph.mmd",
    "flowchart/visual-check/neutral-theme-legacy-graph.mmd",
}

DIRECTIVE_RE = re.compile(r"^\s*%%\{init:(.*)\}%%\s*$")


def split_source(text):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return "\n".join(lines[1:close]), "\n".join(lines[close + 1:])
    return None, text


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def normalize_single_quotes(payload):
    """Mirror the C++ normalizer: single-quoted strings become double
    quoted (backslash rejected, embedded double quote escaped); anything
    else passes through verbatim."""
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


def is_loglevel_only_directive(line):
    match = DIRECTIVE_RE.match(line)
    if not match:
        return False
    normalized = normalize_single_quotes(match.group(1).strip())
    if normalized is None:
        return False
    try:
        payload = json.loads(normalized)
    except json.JSONDecodeError:
        return False
    if set(payload.keys()) != {"logLevel"}:
        return False
    value = payload["logLevel"]
    if isinstance(value, bool):
        return False
    if isinstance(value, int):
        return 0 <= value <= 5
    return isinstance(value, str) and value in LOG_LEVEL_NAMES


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {f["path"]: f for f in manifest["fixtures"]}
    selected = []
    for path in sorted((repo_root / CASES / "flowchart").rglob("*.mmd")):
        text = path.read_text()
        split = split_source(text)
        if split is None:
            continue
        _, body = split
        directives = [line for line in body.splitlines() if "%%{" in line]
        if len(directives) != 1 or not is_loglevel_only_directive(directives[0]):
            continue
        rel = path.relative_to(repo_root / CASES).as_posix()
        if not re.search(r"^\s*acc(Title|Descr)\s*:", body, re.M):
            raise ValueError(f"logLevel-only fixture without accessibility "
                             f"statement, selector drifted: {rel}")
        selected.append(rel)
    if set(selected) != BODY_ONLY_FILES:
        raise ValueError(
            f"partition drift: selected={sorted(selected)} "
            f"expected={sorted(BODY_ONLY_FILES)}")
    rows = []
    for rel in selected:
        observed = sha256_file(repo_root / CASES / rel)
        if observed != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        rows.append({
            "path": rel,
            "family": "flowchart",
            "sha256": observed,
            "manifest_sha256": manifest_rows[rel]["sha256"],
            "prior_status": manifest_rows[rel]["raw"],
            "prediction": "predicted-draw",
            "track_scope": "body-only",
            "expected_states": {  # whole original stays source (theme gate)
                "wide_240": "source",
                "narrow_40": "source",
                "restored_240": "source",
            },
            "expected_body_states": {
                "wide_240": "drawn",
                "narrow_40": "source",
                "restored_240": "drawn",
            },
            "ledger_note": (
                "logLevel-only init directive plus accTitle/accDescr "
                "scalars admitted; body draws, whole original stays source "
                "because config.theme matches no envelope profile"),
        })
    return {
        "schema": SCHEMA,
        "rfc": 31,
        "phase": "init-loglevel-accessibility",
        "whitelist": {
            "directive": (
                "one-key %%{init}%% payload: flowchart.htmlLabels (existing) "
                "or logLevel as integer 0..5 / one of "
                "trace|debug|info|warn|error|fatal; single and double quoted "
                "strings, backslash rejected inside single quotes"),
            "accessibility": (
                "scalar accTitle/accDescr lines consumed as non-visual "
                "no-ops; empty, >256 byte, markup/backtick content, "
                "duplicates and the accDescr block form fail closed"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": hashlib.sha256(
                (repo_root / MANIFEST).read_bytes()).hexdigest(),
        },
        "selection": {
            "rule": (
                "flowchart fixture whose sole body init directive is a "
                "single-key logLevel payload and whose body also carries "
                "accTitle/accDescr scalar statements"),
            "roots": ["flowchart"],
        },
        "review": {
            "reviewer_agent": ("independent read-only adversarial review "
                              "(fresh general-purpose agent, FS-init "
                              "logLevel/accessibility cohort, 2026-11-08, "
                              "task a84b30aa96dc983db)"),
            "date": "2026-11-08",
            "verdict": "approved",
            "note": (
                "Independent read-only adversarial review (fresh "
                "general-purpose agent, 2026-11-08): first round "
                "REQUEST-CHANGES caught two cross-cohort MAJORs — the four "
                "rows were already pinned as blocked in the FS-6 "
                "FontAwesome ledger (durable gate would go red) and the "
                "first OfficialMatrix hook was shadowed by the earlier "
                "FS-6 branch (dead code that would also trip the 48-gap "
                "roster). Both closed: FS-6 migrated in place to 38/16 "
                "with the rows as body-only members and the matrix hook "
                "removed, leaving this ledger as the independent durable "
                "capability gate. Re-review APPROVE; the directive "
                "normalizer mirrors class_init_meta, both whitelists are "
                "closed (logLevel 0..5/named levels, accessibility scalar "
                "contract), and the four new-syntax theme-key directives "
                "stay fail-closed."),
        },
        "counts": {
            "total": len(rows),
            "predicted_draw": len(rows),
            "predicted_blocked": 0,
            "predicted_original_draw": 0,
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
