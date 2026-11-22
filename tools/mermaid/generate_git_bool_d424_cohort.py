#!/usr/bin/env python3
"""Generate the gitGraph closed-boolean namespace cohort ledger (D-424).

Renderer-free. Selection (semantic, source-state markers only): a gitGraph
original whose frontmatter `config.gitGraph` block carries only leaves from the
closed boolean set {showBranches, showCommitLabel, rotateCommitLabel,
parallelCommits}, each with an exact `true`/`false` value, and whose body is
otherwise admitted. These are the rows whose only unadmitted input is that
namespace.

The oracle is *semantic*, not a render-difference proxy: a `showBranches:
false` frame must not contain any branch reference text, a `showCommitLabel:
false` frame must not contain any commit id, and the corresponding `true`
source must contain it. `rotateCommitLabel` and `parallelCommits` are inert in
the terminal (disclosed only), so their true/false pairs must render identical
frames.

Usage: python3 tools/mermaid/generate_git_bool_d424_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/git-bool-d424-cohort.json")
SCHEMA = "loom_mermaid_git_bool_d424_cohort/v1"
KEYS = ("showBranches", "showCommitLabel", "rotateCommitLabel", "parallelCommits")
EXPECTED_TOTAL = 18
FOOTER_HEADER = "GitGraph native projection v1"

COHORT_DIGEST = "87d27aeb777b2ccf86a99bd90caaeeabe3d61265dbea9521fd036d5309c1fce2"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def cohort_digest(rows) -> str:
    payload = "".join(f"{row['path']}\t{row['sha256']}\n" for row in rows)
    return sha256_bytes(payload.encode())


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None, text
    end = text.find("\n---\n", 4)
    if end == -1:
        raise ValueError("unterminated frontmatter")
    return text[4:end].split("\n"), text[end + 5 :]


def namespace_keys(frontmatter_lines):
    """Return the `config.gitGraph` boolean map, or None when absent/mixed."""
    keys = {}
    in_gitgraph = False
    for line in frontmatter_lines:
        stripped = line.strip()
        if stripped == "gitGraph:":
            in_gitgraph = True
            continue
        if not in_gitgraph:
            continue
        if not line.startswith("    ") or not stripped:
            in_gitgraph = False
            continue
        key, sep, value = stripped.partition(":")
        if not sep:
            raise ValueError(f"malformed gitGraph leaf: {stripped!r}")
        keys[key] = value.strip()
    if not keys:
        return None
    # The residual cohort is exactly the four-key namespace: rows carrying only
    # `parallelCommits` were admitted by RFC 0045 and must not be re-selected.
    if set(keys) != set(KEYS) or any(v not in ("true", "false") for v in keys.values()):
        return None
    return keys


def footer_lines(keys) -> list[str]:
    lines = [FOOTER_HEADER]
    for key in KEYS:
        if key in keys:
            if key == "showBranches":
                lines.append(f"config.gitGraph.showBranches={keys[key]} -> branch reference labels")
            elif key == "showCommitLabel":
                lines.append(
                    f"config.gitGraph.showCommitLabel={keys[key]} -> commit id labels")
            elif key == "rotateCommitLabel":
                lines.append(
                    f"config.gitGraph.rotateCommitLabel={keys[key]} -> label rotation; not applied")
            else:
                lines.append(
                    f"config.gitGraph.parallelCommits={keys[key]} -> parallel lane compaction; not applied")
    return lines


def body_commit_ids(body: str) -> list[str]:
    """Explicit `commit id: "X"` values only.

    The first quoted token after the key is the id; anything following it
    (`tag: "v1.0"` on the same line) belongs to the tag list, not the id.
    """
    ids = []
    for line in body.split("\n"):
        stripped = line.strip()
        if stripped.startswith("commit id:"):
            value = stripped[len("commit id:") :].strip()
            if value[:1] in ("'", '"'):
                quote = value[0]
                end = value.find(quote, 1)
                value = value[1:end] if end != -1 else value[1:]
            else:
                value = value.split()[0] if value else ""
            ids.append(value.strip())
    return ids


def body_branch_names(body: str) -> list[str]:
    names = []
    for line in body.split("\n"):
        stripped = line.strip()
        for prefix in ("branch ", "checkout "):
            if stripped.startswith(prefix):
                name = stripped[len(prefix) :].strip().split()[0].strip("'\"")
                if name and name not in names:
                    names.append(name)
    return names


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    rows = []
    for path in sorted((cases / "git").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        frontmatter, body = split_frontmatter(path.read_text())
        if frontmatter is None:
            continue
        keys = namespace_keys(frontmatter)
        if keys is None:
            continue
        # The namespace must be the only config content: a flat look/theme
        # leaf belongs to the RFC 0039 table and is not part of this slice.
        if any(line.strip() in ("look:", "theme:") or line.strip().startswith(("look:", "theme:"))
               for line in frontmatter if not line.startswith("    ")):
            continue
        rows.append(
            {
                "path": rel,
                "sha256": sha256_file(path),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "prior_status": {
                    "expected": manifest_rows[rel]["raw"]["expected"],
                    "reason": manifest_rows[rel]["raw"]["reason"],
                },
                "booleans": keys,
                "expected_footer_lines": footer_lines(keys),
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "source",
                    "restored_240": "drawn",
                },
                "semantics": {
                    "commit_ids": body_commit_ids(body),
                    "branch_names": body_branch_names(body),
                },
                "ledger_note": (
                    "D-424 closed gitGraph boolean namespace: showBranches and "
                    "showCommitLabel hide real elements; rotateCommitLabel and "
                    "parallelCommits are inert and disclosed only."),
            }
        )
    if len(rows) != EXPECTED_TOTAL:
        raise ValueError(f"cohort drifted: expected {EXPECTED_TOTAL}, got {len(rows)}")
    rows.sort(key=lambda row: row["path"])
    digest = cohort_digest(rows)
    if digest != COHORT_DIGEST:
        raise ValueError(f"cohort row set drifted: digest {digest} != {COHORT_DIGEST}")
    manifest_sha = sha256_bytes(manifest_bytes)
    if manifest_sha != MANIFEST_SHA256:
        raise ValueError(f"manifest changed without review: {manifest_sha}")
    counts = {"total": len(rows), "hidden_branches": 0, "hidden_labels": 0}
    for row in rows:
        counts["hidden_branches"] += row["booleans"].get("showBranches") == "false"
        counts["hidden_labels"] += row["booleans"].get("showCommitLabel") == "false"
    return {
        "schema": SCHEMA,
        "decision": "D-424",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "gitGraph original whose frontmatter config.gitGraph block carries "
                    "exactly the four closed boolean leaves {showBranches, showCommitLabel, "
                    "rotateCommitLabel, parallelCommits}",
            "cohort_digest": digest,
            "table": {f"config.gitGraph.{key}": ["true", "false"] for key in KEYS},
            "exclusions": "mainBranchName/mainBranchOrder (content keys) and the "
                          "cherry-pick statement rows stay whole-source",
        },
        "review": {
            "reviewer_agent": "TL exemption 2026-11-22: family slice opened without a "
                              "design gate; the gate is this test document plus the "
                              "semantic oracle it pins",
            "verdict": "approved",
        },
        "counts": counts,
        "rows": rows,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
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
    print("cohort digest", ledger["selection"]["cohort_digest"])
    print("manifest sha256", ledger["upstream"]["manifest_sha256"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
