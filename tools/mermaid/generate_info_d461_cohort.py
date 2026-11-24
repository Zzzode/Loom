#!/usr/bin/env python3
"""Generate the D-461 stage-4 `info` directive admission cohort ledger.

Renderer-free. Stage 4 admits the three originals whose body is the bare
`info` directive (upstream's fixed version panel, not a graph): the two
`info/` family originals and the `state-diagram-v2` fixture that carries the
same directive behind a frontmatter fence. The panel content is the pinned
upstream version only, read from the fixture manifest's `upstream.tag`; no
other text is invented.

The runtime banner text lives in `src/ui/visual/mermaid_info_impl.cpp`
(`kPinnedMermaidVersion`); the gate (`test_mermaid_info_d461_cohort`) asserts
the rendered banner equals this ledger's `panel_version`, which the generator
derives from the manifest, so a manifest bump without a renderer bump fails
the gate.

Oracles:

* `info-banner`: every row renders the same fixed panel (the state-v2 row adds
  only its accepted-config disclosure lines); `info` and `info showInfo` are
  twin-equal because upstream treats them identically.

Usage: python3 tools/mermaid/generate_info_d461_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/info-d461-cohort.json")
SCHEMA = "loom_mermaid_info_d461_cohort/v1"
DECISION = "D-461"

ROWS = [
    "info/pr-check/should-handle-an-info-definition.mmd",
    "info/pr-check/should-handle-an-info-definition-with-showinfo.mmd",
    "state-diagram-v2/nightly-check/v2-should-render-a-simple-info.mmd",
]

# The E2b non-terminal leaves the `info` frontmatter gate accepts, in the
# renderer's fixed alphabetical disclosure order. Derived here from the source
# so the ledger and the gate agree on the exact strings.
LEAF_NOTICE = {
    "fontFamily": 'config.fontFamily="{value}" -> terminal font; not applied',
    "logLevel": "config.logLevel={value} -> logger only; not applied",
    "securityLevel": 'config.securityLevel="{value}" -> no interaction layer; not applied',
}
WHITELIST = {
    "logLevel": {"0", "1", "2"},
    "fontFamily": {"courier"},
    "securityLevel": {"loose", "sandbox"},
}

REVIEW = {
    "reviewer_agent": (
        "stage 4: one independent read-only adversarial review (task "
        "a5d854b3334464816; round 1 REQUEST-CHANGES on the stale state-ledger "
        "review note, the d117 family columns/Total, unverified post-fix "
        "suites, the pre-charge parse/allocation order and the empty-config "
        "admission; all fixed and re-verified; round 2 APPROVE). The review "
        "recorded the panel content decision (pinned upstream version only), "
        "the closed frontmatter grammar and the paired negative set."),
    "date": "2026-11-24",
    "verdict": "approved",
    "note": ("Stages 1-4: stage 4 admits the bare `info` directive as a fixed "
             "native panel naming the pinned upstream version; the closed body "
             "grammar is `info` or `info showInfo` behind an optional "
             "description/config frontmatter limited to the E2b leaf "
             "whitelist."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def split_frontmatter(text: str):
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, lines
    for index in range(1, len(lines)):
        if lines[index] == "---":
            return lines[1:index], lines[index + 1:]
    return None, lines


def config_leaves(frontmatter):
    """Indent-2 `key: value` leaves under a top-level `config:` key."""
    if frontmatter is None:
        return []
    leaves = []
    in_config = False
    for line in frontmatter:
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if not line.startswith(" "):
            in_config = line.split(":", 1)[0].strip() == "config"
            continue
        if not in_config:
            continue
        key, _, value = line.strip().partition(":")
        key = key.strip()
        value = value.strip()
        if key not in WHITELIST or value not in WHITELIST[key]:
            raise ValueError(f"config leaf outside the closed whitelist: {line!r}")
        leaves.append((key, value))
    return sorted(leaves)


def expected_notices(leaves):
    return [LEAF_NOTICE[key].format(value=value) for key, value in leaves]


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {fixture["path"]: fixture for fixture in manifest["fixtures"]}
    tag = manifest["upstream"]["tag"]
    version = tag.split("@", 1)[1]
    rows = []
    for path in ROWS:
        text = (repo_root / CASES / path).read_text()
        observed = sha256_file(repo_root / CASES / path)
        if observed != manifest_rows[path]["sha256"]:
            raise ValueError(f"sha drift for {path}")
        frontmatter, body = split_frontmatter(text)
        directives = [line.strip() for line in body
                      if line.strip() and not line.strip().startswith("%%")]
        if directives not in (["info"], ["info showInfo"]):
            raise ValueError(f"not a closed info body: {path}")
        leaves = config_leaves(frontmatter)
        rows.append({
            "path": path,
            "family": path.split("/", 1)[0],
            "sha256": observed,
            "manifest_sha256": manifest_rows[path]["sha256"],
            "prior_status": "source_fallback",
            "oracle": "info-banner",
            "directive": directives[0],
            "config_leaves": [{"key": key, "value": value} for key, value in leaves],
            "expected_notices": expected_notices(leaves),
            # The disclosure footer is wider than the 40-column fence, so a
            # row with accepted config leaves keeps whole source at 40 (the
            # same narrow policy E2b pre-registers for its footer rows).
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "source_fallback" if leaves else "drawn",
                "restored_240": "drawn",
            },
            "ledger_note": ("the directive carries no body content; the panel is "
                            "the pinned upstream version banner"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "stage": "4: bare `info` directive admitted as the fixed version panel",
        "panel_version": version,
        "admission": {
            "body": ("exactly one non-comment body line, `info` or "
                     "`info showInfo`; any other shape fails closed"),
            "frontmatter": ("optional `---` fence with a YAML `description` "
                            "(plain or block) and an indent-2 `config:` map "
                            "limited to logLevel 0/1/2, fontFamily courier, "
                            "securityLevel loose/sandbox; accepted leaves are "
                            "disclosed, nothing else"),
            "panel": ("one banner line `Mermaid <pinned tag version>`; the "
                      "directive carries no content, so no other text is "
                      "painted"),
        },
        "upstream": {
            "tag": tag,
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_file(repo_root / MANIFEST),
        },
        "selection": {
            "rule": "the three originals whose body is the bare `info` directive",
            "cohort_digest": sha256_bytes("".join(
                f"{row['path']}\t{row['sha256']}\n"
                for row in sorted(rows, key=lambda row: row["path"])).encode()),
        },
        "counts": {
            "total": len(rows),
            "by_family": {"info": 2, "state-diagram-v2": 1},
        },
        "review": REVIEW,
        "rows": rows,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    repo_root = pathlib.Path(args.repo_root)
    ledger = build(repo_root)
    payload = json.dumps(ledger, indent=2) + "\n"
    target = repo_root / LEDGER
    if args.check:
        if not target.exists() or target.read_text() != payload:
            print(f"ledger is stale: run without --check to rewrite {LEDGER}")
            return 1
        print(f"ledger current: {json.dumps(ledger['counts'])}")
        return 0
    target.write_text(payload)
    print(f"wrote {LEDGER}: {json.dumps(ledger['counts'])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
