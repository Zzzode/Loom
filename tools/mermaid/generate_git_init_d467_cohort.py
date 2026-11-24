#!/usr/bin/env python3
"""Generate the D-467 gitGraph pre-header init cohort ledger.

Renderer-free. Admits the nine `%%{init: ...}%%` gitGraph originals whose
directive is a closed, inert profile for the terminal projection:

* `logLevel`: logger verbosity only;
* `theme: 'default'`: the source palette (not applied);
* `gitGraph.rotateCommitLabel: true|false`: upstream label rotation; the
  terminal always draws labels horizontally (not applied, disclosed);
* `themeVariables.gitBranchLabel0..7`: branch-label colours (not applied,
  disclosed once).

Nine originals, not eight: one directive ends the nested object with a
trailing comma (legal JavaScript source), which the normalizer drops before
the strict JSON parse.

The directive may span physical lines and is spelled with single quotes, per
the upstream corpus. `mainBranchName` / `showCommitLabel: true` keep their
D-436 semantics; every other key, value, duplicate, escape or stray quote
keeps the whole source on fallback. A body init directive is now admitted by
this closed profile even when the source also carries a frontmatter
description.

The art contract is unchanged: the gate pins the wide/narrow/restored states,
restored cell equality, the captions and the disclosure lines. narrow_40 is
source for every row (the disclosure footer is wider than the fence).

Usage: python3 tools/mermaid/generate_git_init_d467_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/git-init-d467-cohort.json")
SCHEMA = "loom_mermaid_git_init_d467_cohort/v1"
DECISION = "D-467"

LOG = "init.logLevel=debug -> logger only; not applied"
THEME = "init.theme=default -> colour palette; not applied"
ROTATE_FALSE = "init.gitGraph.rotateCommitLabel=false -> label rotation; not applied"
ROTATE_TRUE = "init.gitGraph.rotateCommitLabel=true -> label rotation; not applied"
VARS = ("init.themeVariables.gitBranchLabel0-7 (8) -> branch label colours; "
        "not applied")

ROWS = [
    ("git/nightly-check/10-should-render-a-simple-gitgraph-with-horizontal-labels.mmd",
     [LOG, THEME, ROTATE_FALSE], ["Alpha", "Delta"]),
    ("git/nightly-check/24-should-render-a-simple-gitgraph-with-horizontal-labels-vertical-branch.mmd",
     [LOG, THEME, ROTATE_FALSE], ["Alpha", "Delta"]),
    ("git/pr-check/59-should-render-a-simple-gitgraph-with-horizontal-labels-vertical-branch-bottom-to-top.mmd",
     [LOG, THEME, ROTATE_FALSE], ["Alpha", "Delta"]),
    ("git/nightly-check/9-should-render-a-simple-gitgraph-with-rotated-labels.mmd",
     [LOG, THEME, ROTATE_TRUE], ["75f7219e"]),
    ("git/nightly-check/58-should-render-a-simple-gitgraph-with-rotated-labels-vertical-branch-bottom-to-top.mmd",
     [LOG, THEME, ROTATE_TRUE], ["75f7219e"]),
    ("git/pr-check/23-should-render-a-simple-gitgraph-with-rotated-labels-vertical-branch.mmd",
     [LOG, THEME, ROTATE_TRUE], ["75f7219e"]),
    ("git/nightly-check/8-should-render-a-simple-gitgraph-with-more-than-8-branches-overriding-variables.mmd",
     [LOG, THEME, VARS], ["branch1", "branch9"]),
    # The one directive in the corpus that ends its nested object with a
    # trailing comma (legal JavaScript, not strict JSON); the normalizer drops
    # it. Its body is the admitted row 22's BT twin.
    ("git/nightly-check/57-should-render-a-simple-gitgraph-with-more-than-8-branches-overriding-variables-vertical-branch-bottom-to-top.mmd",
     [LOG, THEME, VARS], ["branch1", "branch9"]),
    ("git/pr-check/22-should-render-a-simple-gitgraph-with-more-than-8-branches-overriding-variables-vertical-branch.mmd",
     [LOG, THEME, VARS], ["branch1", "branch9"]),
]

REVIEW = {
    "reviewer_agent": (
        "task abfc9b5165c0e702b round 1 REQUEST-CHANGES (2 MAJOR, 1 minor, "
        "1 nit) -> round 2 APPROVE"),
    "date": "2026-11-25",
    "verdict": "approved",
    "note": ("Nine gitGraph originals carry a multi-line, single-quoted init "
             "directive whose keys are inert for the terminal projection; one "
             "directive ends its nested object with a trailing comma that the "
             "normalizer drops. Admitted and disclosed; no rendering semantics "
             "change. Round 1: row 57 was misdescribed as capacity-blocked "
             "when its trailing comma was the blocker (now admitted) and the "
             "review record was a placeholder; round 2 APPROVE."),
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def build(repo_root):
    manifest = json.loads((repo_root / MANIFEST).read_text())
    manifest_rows = {fixture["path"]: fixture for fixture in manifest["fixtures"]}
    rows = []
    for path, notices, captions in ROWS:
        observed = sha256_file(repo_root / CASES / path)
        if observed != manifest_rows[path]["sha256"]:
            raise ValueError(f"sha drift for {path}")
        rows.append({
            "path": path,
            "family": "git",
            "sha256": observed,
            "manifest_sha256": manifest_rows[path]["sha256"],
            "prior_status": "source_fallback",
            "oracle": "native-frame",
            "expected_states": {
                "wide_240": "drawn",
                "narrow_40": "source_fallback",
                "restored_240": "drawn",
            },
            "expected_notices": notices,
            "expected_captions": captions,
            "ledger_note": ("closed inert init profile; the terminal draws "
                            "labels horizontally and no colour"),
        })
    return {
        "schema": SCHEMA,
        "decision": DECISION,
        "stage": "gitGraph multi-line init directive, closed inert profile",
        "admission": {
            "mechanism": ("a pre-header `%%{init: ...}%%` may span physical "
                          "lines; single-quoted strings are normalized to JSON "
                          "(one trailing comma before a closing brace is "
                          "dropped) and a closed key profile is validated "
                          "(logLevel/theme/gitGraph.rotateCommitLabel/"
                          "themeVariables.gitBranchLabel0..7, plus the existing "
                          "mainBranchName/showCommitLabel semantics)"),
            "scope": ("the gitGraph parser only; every admitted key is inert "
                      "for the terminal projection and disclosed"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_file(repo_root / MANIFEST),
        },
        "selection": {
            "rule": ("the gitGraph originals still in the fallback roster whose "
                     "body init directive is the closed inert profile"),
            "cohort_digest": sha256_bytes("".join(
                f"{row['path']}\t{row['sha256']}\n"
                for row in sorted(rows, key=lambda row: row["path"])).encode()),
        },
        "counts": {"total": len(rows), "by_family": {"git": len(rows)}},
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
