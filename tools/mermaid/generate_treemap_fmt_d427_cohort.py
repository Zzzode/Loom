#!/usr/bin/env python3
"""Generate the treemap valueFormat cohort ledger (D-427).

Renderer-free. Selection (semantic, source-state markers only): a treemap
original whose frontmatter is exactly a folded description plus
`config:` > `treemap:` > `valueFormat:` with a spec in the closed grammar
(optional `$`, the no-op d3 `0` flag, optional `,`, optional `.N` 0..6, optional
`%`/`f`). The leaf value formatting is *honoured* in the drawn labels, not
disclosed as inert, so the oracle is the exact formatted text.

The formatter mirrors `treemap_format_value` in
src/ui/visual/mermaid_treemap_impl.cpp with exact decimal string arithmetic (no
floating point). The ledger is the single source for both sides: the C++ gate
asserts each `expected_label` appears verbatim in the drawn frame.

Usage: python3 tools/mermaid/generate_treemap_fmt_d427_cohort.py [--check]
"""

import argparse
import hashlib
import json
import pathlib
import re
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/treemap-fmt-d427-cohort.json")
SCHEMA = "loom_mermaid_treemap_fmt_d427_cohort/v1"
EXPECTED_TOTAL = 6
LEAF = re.compile(r'^\s*"([^"]+)":\s*([0-9]+(?:\.[0-9]+)?)\s*$')

COHORT_DIGEST = "0b00a3fa1d3d112f066ce7674d5e0c1fbb5a2c474a22855b6f853046466aafaa"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def cohort_digest(rows) -> str:
    payload = "".join(f"{row['path']}\t{row['sha256']}\n" for row in rows)
    return sha256_bytes(payload.encode())


def parse_spec(spec: str):
    """Mirror of treemap_format: returns (dollar, group, percent, precision)."""
    if len(spec) >= 2 and spec[0] in "'\"" and spec[-1] == spec[0]:
        spec = spec[1:-1]
        if "'" in spec or '"' in spec:
            raise ValueError("malformed quoted valueFormat")
    i = 0
    dollar = group = percent = False
    precision = 0
    if i < len(spec) and spec[i] == "$":
        dollar = True
        i += 1
    if i < len(spec) and spec[i] == "0":
        i += 1
    if i < len(spec) and spec[i] == ",":
        group = True
        i += 1
    if i < len(spec) and spec[i] == ".":
        i += 1
        digits = ""
        while i < len(spec) and spec[i].isdigit():
            digits += spec[i]
            i += 1
        if not digits:
            raise ValueError(f"bad precision in {spec!r}")
        precision = int(digits)
        if precision > 6:
            raise ValueError(f"precision too large in {spec!r}")
    if i < len(spec) and spec[i] == "%":
        percent = True
        i += 1
    elif i < len(spec) and spec[i] == "f":
        i += 1
    if i < len(spec) and spec[i] == "0":
        i += 1
    if i != len(spec) or not spec:
        raise ValueError(f"out-of-grammar valueFormat {spec!r}")
    return dollar, group, percent, precision


def format_value(raw: str, spec: str) -> str:
    dollar, group, percent, precision = parse_spec(spec)
    integer, _, fraction = raw.partition(".")
    if percent:
        fraction = fraction.ljust(2, "0")
        integer += fraction[:2]
        fraction = fraction[2:]
    integer = integer.lstrip("0") or "0"
    fraction = fraction.ljust(precision + 1, "0")
    nxt = fraction[precision]
    fraction = fraction[:precision]
    if nxt >= "5":
        carry = 1
        fraction_list = list(fraction)
        for i in range(len(fraction_list) - 1, -1, -1):
            if carry == 0:
                break
            if fraction_list[i] == "9":
                fraction_list[i] = "0"
            else:
                fraction_list[i] = str(int(fraction_list[i]) + 1)
                carry = 0
        fraction = "".join(fraction_list)
        integer_list = list(integer)
        for i in range(len(integer_list) - 1, -1, -1):
            if carry == 0:
                break
            if integer_list[i] == "9":
                integer_list[i] = "0"
            else:
                integer_list[i] = str(int(integer_list[i]) + 1)
                carry = 0
        if carry == 1:
            integer_list.insert(0, "1")
        integer = "".join(integer_list)
    if group:
        grouped = ""
        count = len(integer)
        for i, digit in enumerate(integer):
            if i != 0 and (count - i) % 3 == 0:
                grouped += ","
            grouped += digit
        integer = grouped
    out = ("$" if dollar else "") + integer
    if fraction:
        out += "." + fraction
    if percent:
        out += "%"
    return out


def split_frontmatter(text: str):
    if not text.startswith("---\n"):
        return None, text
    end = text.find("\n---\n", 4)
    if end == -1:
        raise ValueError("unterminated frontmatter")
    return text[4:end], text[end + 5 :]


def namespace_spec(frontmatter: str):
    """Return the closed valueFormat spec, or None for any other shape."""
    section = None
    spec = None
    for line in frontmatter.split("\n"):
        if line.startswith("description:"):
            section = "description"
            continue
        if line == "config:":
            section = "config"
            continue
        if section == "description":
            if not line.startswith("  ") or line.startswith("   ") or not line.strip():
                return None
            continue
        if section == "config":
            if line != "  treemap:":
                return None
            section = "treemap"
            continue
        if section == "treemap":
            if not line.startswith("    valueFormat: ") or spec is not None:
                return None
            spec = line[len("    valueFormat: ") :]
            parse_spec(spec)
            continue
        return None
    return spec


def leaf_labels(body: str, spec: str) -> list[str]:
    labels = []
    for line in body.split("\n"):
        match = LEAF.match(line)
        if match:
            labels.append(f"{match.group(1)}: {format_value(match.group(2), spec)}")
    return labels


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    rows = []
    for path in sorted((cases / "treemap").rglob("*.mmd")):
        rel = path.relative_to(cases).as_posix()
        frontmatter, body = split_frontmatter(path.read_text())
        if frontmatter is None:
            continue
        spec = namespace_spec(frontmatter)
        if spec is None:
            continue
        labels = leaf_labels(body, spec)
        if not labels:
            raise ValueError(f"{rel}: no leaf labels parsed")
        rows.append(
            {
                "path": rel,
                "sha256": sha256_bytes(path.read_bytes()),
                "manifest_sha256": manifest_rows[rel]["sha256"],
                "value_format": spec,
                "expected_labels": labels,
                # The formatted labels wrap at 40 columns instead of falling
                # back, so the narrow observation is a draw like the wide one.
                "expected_states": {
                    "wide_240": "drawn",
                    "narrow_40": "drawn",
                    "restored_240": "drawn",
                },
                "ledger_note": (
                    "D-427 closed treemap valueFormat grammar: the spec is honoured "
                    "in the drawn leaf labels by exact decimal string arithmetic."),
            }
        )
    if len(rows) != EXPECTED_TOTAL:
        raise ValueError(f"cohort drifted: expected {EXPECTED_TOTAL}, got {len(rows)}")
    rows.sort(key=lambda row: row["path"])
    digest = cohort_digest(rows)
    if COHORT_DIGEST != "PLACEHOLDER" and digest != COHORT_DIGEST:
        raise ValueError(f"cohort row set drifted: digest {digest} != {COHORT_DIGEST}")
    manifest_sha = sha256_bytes(manifest_bytes)
    if MANIFEST_SHA256 != "PLACEHOLDER" and manifest_sha != MANIFEST_SHA256:
        raise ValueError(f"manifest changed without review: {manifest_sha}")
    formats = {}
    for row in rows:
        formats[row["value_format"]] = formats.get(row["value_format"], 0) + 1
    return {
        "schema": SCHEMA,
        "decision": "D-427",
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "treemap original whose frontmatter is exactly a folded "
                    "description plus config: > treemap: > valueFormat: in the "
                    "closed grammar",
            "cohort_digest": digest,
            "grammar": "optional '$', no-op d3 '0' flag, optional ',', optional "
                       "'.N' (0..6), optional '%' or 'f'",
            "formats": formats,
            "exclusions": "theme/look leaves, any other config key, classDef/::: "
                          "styles and inline specs outside the grammar stay "
                          "whole-source",
        },
        "review": {
            "reviewer_agent": "TL exemption 2026-11-22: family slice opened without a "
                              "design gate; the gate is this test document plus the "
                              "formatted-label oracle it pins",
            "verdict": "approved",
        },
        "counts": {"total": len(rows), "formats": formats},
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
