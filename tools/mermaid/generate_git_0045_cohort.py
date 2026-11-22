#!/usr/bin/env python3
"""Generate the RFC 0045 gitGraph `parallelCommits` admission cohort ledger.

Renderer-free. The 12 rows are the git fallbacks whose only unadmitted
frontmatter is the `config.gitGraph.parallelCommits` boolean; the namespace
carries no other leaf and no content-bearing gitGraph key. The renderer never
reads the key (it is the upstream renderer's parallel-lane compaction switch),
so each row draws the config-free body and the footer discloses the unapplied
compaction.

The synthetic twin deletes the `config:` line and every following line indented
at least two spaces, up to the next column-0 key (or the closing fence). The
RFC 0039 `config_free_twin` helper cannot be reused: it only skips flat
look/theme leaves and would leave the namespace in place.

`expected_states.narrow_40` is measured live (rule 34) and never inherited.

Mirror fidelity (rule 37): `admits` reproduces the live gate's checks — the
envelope parser's tab-indent rejection, the empty body-init requirement, the
namespace event model, `seen_paths` duplicates, the top-level non-empty
`config` rejection, and the description form/header/payload rules. Two
directions are deliberately stricter than the live gate, because the C++ gate
is the admission authority and the generator only has to prove its own rows:
a CRLF source (the parser strips `\r`) and a row combining look/theme with the
namespace (the gate admits the composition) are rejected here. Extend the
mirror before relying on either.

Usage: python3 tools/mermaid/generate_git_0045_cohort.py --repo-root . [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/git-0045-cohort.json")
SCHEMA = "loom_mermaid_git_0045_cohort/v1"
PHASE = "P1-parallel-commits"

HEADER = "GitGraph native projection v1"
NOTICE_LIMIT = 96

COHORT = [
    "git/nightly-check/41-should-render-default-gitgraph-with-parallelcommits-set-to-false.mmd",
    "git/nightly-check/42-should-render-gitgraph-with-parallel-commits.mmd",
    "git/nightly-check/43-should-render-gitgraph-with-parallel-commits-vertical-branch.mmd",
    "git/nightly-check/44-should-render-gitgraph-with-unconnected-branches-and-no-parallel-commits.mmd",
    "git/nightly-check/45-should-render-gitgraph-with-unconnected-branches-and-parallel-commits.mmd",
    "git/nightly-check/46-should-render-gitgraph-with-merge-back-and-merge-forward.mmd",
    "git/nightly-check/46-should-render-gitgraph-with-unconnected-branches-and-parallel-commits-vertical-branch.mmd",
    "git/pr-check/47-should-render-gitgraph-with-merge-back-and-merge-forward-vertical-branch.mmd",
    "git/nightly-check/48-should-render-gitgraph-with-merge-on-a-new-branch-vertical-branch.mmd",
    "git/nightly-check/49-should-render-gitgraph-with-merge-on-a-new-branch-vertical-branch.mmd",
    "git/nightly-check/71-should-render-gitgraph-with-parallel-commits-vertical-branch-bottom-to-top.mmd",
    "git/nightly-check/72-should-render-gitgraph-with-unconnected-branches-and-parallel-commits-vertical-branch-bottom-to-top.mmd",
]

EXPECTED = {"originals": 135, "drawn": 70, "admitted": 12, "remaining": 53}
# Drawn rows that no admission ledger enumerates (the 0039 cohort is 12).
DRAWN_BASE = 58
# Measured with fallback_attribution.classify on 2026-11-22 (the gate notes it;
# the roster itself is derived, never read here).
REMAINING_BY_CATEGORY = {
    "family_residual:git": 34,
    "git_cherry_pick": 12,
    "look_neo": 6,
    "multiline_label": 1,
}

# Measured 40-column draws among the admitted rows (live gate is the authority).
NARROW_DRAW: set[str] = set()

PHASE_REVIEWS = {
    PHASE: {
        "reviewer_agent": "agent:code-review (independent adversarial, read-only)",
        "date": "2026-11-22",
        "verdict": "approved",
        "note": (
            "r1 REQUEST-CHANGES (BLOCKER: the twin assertion scanned the whole "
            "source for `parallelCommits`, which the folded description text "
            "legitimately contains; plus upstream-pin, mirror-fidelity, battery "
            "coverage and twin-rule-alignment findings) -> fixed -> r2 APPROVE, "
            "re-verified against the full gate binary."
        ),
    },
}


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def _strip_comment(value: str) -> str:
    """Mirror er_strip_comment: quote-aware `#`; the kept prefix is trimmed of
    spaces/tabs/CR, the no-comment case is returned untouched."""
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
    """Mirror git_inert_scalar on a parser-trimmed value: comment strip, then a
    matched outer quote pair is removed. No extra trimming."""
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
            return None  # the envelope parser rejects a tab indentation
        if in_description:
            if indent > 0:
                continue  # folded description payload, not a config entry
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
            # A top-level key restarts the nested path (the live parser keeps
            # config children un-prefixed: `gitGraph`, not `config.gitGraph`).
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


def admits(text: str):
    """Return (ok, parallel_value) for the live git gate's admitted shape."""
    if not text.startswith("---\n"):
        return False, None
    events = frontmatter_events(text)
    if events is None:
        return False, None
    description_seen = config_seen = False
    namespace_seen = False
    parallel = None
    payload = False
    for path, value, indent, namespace in events:
        if indent == 0:
            if path == "description":
                if description_seen:
                    return False, None
                description_seen = True
                if value != ">-":
                    return False, None
                continue
            if path == "config":
                # The live parser rejects a top-level `config` with a non-empty
                # structural value (only `config:` / `config: # note` open a
                # block), so the mirror must require the namespace flag.
                if config_seen or not namespace:
                    return False, None
                config_seen = True
                continue
            return False, None
        if not config_seen:
            return False, None
        if namespace:
            if path != "gitGraph" or indent != 2 or namespace_seen:
                return False, None
            namespace_seen = True
            continue
        if path == "gitGraph.parallelCommits" and indent == 4:
            if not namespace_seen or parallel is not None:
                return False, None
            scalar = _scalar(value)
            if scalar not in ("true", "false"):
                return False, None
            parallel = scalar
            continue
        return False, None
    if not description_seen or not config_seen or not namespace_seen or parallel is None:
        return False, None
    lines = text.split("\n")
    # The gate rejects any body init directive (`envelope->init.empty()`); the
    # parser recognizes a body line whose text starts with `%%{` plus a
    # matching `}%%` terminator.
    for index in range(1, len(lines)):
        if lines[index] == "---":
            body_lines = lines[index + 1:]
            for line in body_lines:
                stripped = line.lstrip(" \t")
                start = line.find("%%{")
                if stripped.startswith("%%{") and line.find("}%%", start) != -1:
                    return False, None
            break
    # The folded description payload must be present. Mirror the live gate's
    # per-line rule exactly: only truly empty lines are skipped, each payload
    # line is at least three bytes, starts with exactly two spaces (a third
    # space rejects), and contains no tab. `#` is payload text, not a comment.
    # (The gate also requires unicode_text(line); that check is not replicated.)
    for index in range(1, len(lines)):
        line = lines[index]
        if line == "---":
            break
        if line == "":
            continue
        indent = len(line) - len(line.lstrip(" "))
        if indent == 0:
            # The folded payload belongs to the description only; stop at the
            # next top-level key (the config block has its own checks above).
            if line.split(":")[0].strip(" \t") == "config":
                break
            continue
        if len(line) < 3 or not line.startswith("  ") or line.startswith("   ") \
                or "\t" in line:
            return False, None
        payload = True
    if not payload:
        return False, None
    return True, parallel


def config_free_twin(text: str) -> str:
    """Delete the `config:` line and every following indent>=2 line up to the
    next column-0 key (or the closing fence)."""
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return text
    out = [lines[0]]
    skipping = False
    for line in lines[1:]:
        # Mirror the C++ twin exactly: skip only lines whose first two bytes are
        # spaces; a blank line ends the skipped block.
        if skipping and len(line) >= 2 and line[0] == " " and line[1] == " ":
            continue
        skipping = False
        if line.startswith("config:"):
            skipping = True
            continue
        out.append(line)
    return "\n".join(out)


def footer_lines(value: str):
    return [HEADER,
            f"config.gitGraph.parallelCommits={value} -> parallel lane compaction; "
            "not applied"]


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

    base = json.loads(
        (repo_root / "tools/mermaid" / "git-0039-cohort.json").read_text())
    census = {"rfc0039": len(base["rows"]), "base": DRAWN_BASE}
    drawn = census["rfc0039"] + census["base"]
    originals = base["counts"]["family"]
    totals = {"originals": originals, "drawn": drawn,
              "admitted": len(COHORT),
              "remaining": originals - drawn - len(COHORT)}
    if totals != EXPECTED:
        raise ValueError(f"git census drift: {totals} != {EXPECTED}")
    if sum(REMAINING_BY_CATEGORY.values()) != totals["remaining"]:
        raise ValueError("remaining-by-category does not reconcile")

    rows = []
    for rel in COHORT:
        text = (repo_root / CASES / rel).read_text()
        observed_sha = sha256_file(repo_root / CASES / rel)
        if observed_sha != manifest_rows[rel]["sha256"]:
            raise ValueError(f"sha drift for {rel}")
        ok, parallel = admits(text)
        if not ok:
            raise ValueError(f"{rel} is not admitted by the gate shape")
        twin_source = config_free_twin(text)
        twin_events = frontmatter_events(twin_source)
        if twin_events is None or any(
                path == "config" or path == "gitGraph" or path.startswith("gitGraph.")
                for path, _, _, _ in twin_events):
            raise ValueError(f"{rel} twin still carries the namespace")
        twin_sha = sha256_bytes(twin_source.encode())
        narrow = "drawn" if rel in NARROW_DRAW else "source_fallback"
        rows.append({
            "path": rel,
            "family": "git",
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
            "expected_footer_lines": footer_lines(parallel),
            "twin": {
                "kind": "synthetic",
                "rule": ("delete the config: line and every following indent>=2 "
                         "line up to the next column-0 key"),
                "sha256": twin_sha,
            },
            "ledger_note": (
                "inert gitGraph.parallelCommits boolean admitted by RFC 0045; the "
                "renderer never reads it, the upstream parallel-lane compaction is "
                "not applied, and the footer discloses it"),
        })

    return {
        "schema": SCHEMA,
        "rfc": 45,
        "phase": PHASE,
        "admission": {
            "shape": ("folded description envelope + one config block carrying "
                      "exactly one gitGraph namespace with the parallelCommits "
                      "boolean"),
            "table": {"config.gitGraph.parallelCommits": ["true", "false"]},
            "footer": {"header": HEADER, "line_limit_bytes": NOTICE_LIMIT},
            "width_policy": "wide_240 admission only; narrow_40 measured live",
            "twin_rule": ("synthetic config-free twin (config line and every "
                          "following indent>=2 line deleted); no drawn git fixture "
                          "is body-equivalent (RFC 0039 precedent)"),
        },
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": sha256_bytes((repo_root / MANIFEST).read_bytes()),
        },
        "selection": {
            "rule": ("git fallback rows whose only unadmitted config leaf is the "
                     "gitGraph.parallelCommits boolean"),
            "census": census,
            "path_pin": COHORT,
            "cohort_digest": cohort_digest(rows),
            "remaining_by_category": REMAINING_BY_CATEGORY,
        },
        "counts": {
            "originals": totals["originals"],
            "drawn": totals["drawn"],
            "admitted": totals["admitted"],
            "remaining": totals["remaining"],
        },
        "review": build_review(repo_root),
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
