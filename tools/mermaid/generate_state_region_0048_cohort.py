#!/usr/bin/env python3
"""Generate the RFC 0048 State concurrent-region cohort ledger (phase P1).

Renderer-free. Derives the cohort from the corpus text with the closed rule
below, pins it by a path+sha digest and by the per-family counts, and writes
the ledger. The roster and the matrix artifact are never read; review records
are authored here and written into the ledger, never read back.

Selection rule (P1): a state-diagram or state-diagram-v2 original whose body
declares at least one standalone `--` divider line, with no in-body directive
line and no classDef/class/style/cssClass statement. Rows that additionally
carry a leading `%%{init}%%` belong to phase P2, and rows that style states
belong to the deferred class-styling work; both keep their whole-source
fallback and are therefore excluded here.

Per-row `expected_states.narrow_40` is measured, not source-derivable: every
cohort frame is wider than 40 columns, so the row is a source fallback at that
width. The gate re-measures it, so a stale value fails loudly.

Usage: python3 tools/mermaid/generate_state_region_0048_cohort.py [--check]
"""

import argparse
import hashlib
import importlib.util
import json
import pathlib
import sys

CASES = pathlib.Path("tests/fixtures/mermaid_official/cases")
MANIFEST = pathlib.Path("tests/fixtures/mermaid_official/manifest.json")
LEDGER = pathlib.Path("tools/mermaid/state-region-0048-cohort.json")
PROBES = pathlib.Path("tools/mermaid/state-init-0048-probes.json")
SCHEMA = "loom_mermaid_state_region_0048_cohort/v1"
PHASE = "P1-concurrent-regions"
PHASE_P2 = "P2-init-accessibility"

EXPECTED = {"state-diagram": 1, "state-diagram-v2": 16}
EXPECTED_P2 = {"state-diagram": 4, "state-diagram-v2": 4}
EXPECTED_TOTAL = 25
# Measured: every admitted frame is wider than the 40-column viewport.
NARROW_DRAW: set[str] = set()
# Closed config-leaf set: the only frontmatter keys these rows carry, each with
# the exact value set the state envelope admits.
CONFIG_TABLES = {
    "fontFamily": ("courier",),
    "logLevel": ("0", "1", "2"),
    "look": ("classic", "neo", "handDrawn"),
    "theme": ("default", "neo", "neo-dark", "redux", "redux-dark", "redux-color",
              "redux-dark-color"),
}

COHORT_DIGEST = "4829fdb19a13c050d6996cc015de9c06d239b8f91ff874969046d4313ba37171"
MANIFEST_SHA256 = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"
PROBES_SHA256 = "12b0921684396e8d6c6db41ce84002974cb42fb047703b92b8c8aad46694251a"

FOOTER_HEADER = "State native projection v1"


def _tables():
    spec = importlib.util.spec_from_file_location(
        "state_init_tables",
        pathlib.Path(__file__).resolve().parent / "state_init_tables.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


TABLES = _tables()


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def assemble(entry) -> str:
    return "---\n" + "\n".join(entry["frontmatter"]) + "\n---\n" + "\n".join(entry["body"]) + "\n"


def run_probes(repo_root: pathlib.Path) -> str:
    """Run the mirror side of the single-source artifact (rule 37)."""
    raw = (repo_root / PROBES).read_bytes()
    document = json.loads(raw)
    if document.get("schema") != "loom_state_init_0048_probes/v1":
        raise ValueError("probe artifact schema drifted")
    labels = set()
    for index, entry in enumerate(document["entries"]):
        if entry["label"] in labels:
            raise ValueError(f"probe {index}: duplicate label")
        labels.add(entry["label"])
        if entry["expected_kind"] not in ("admitted", "fallback"):
            raise ValueError(f"probe {index}: unknown kind")
        footer = entry["expected_footer"]
        if not isinstance(footer, list) or any(not isinstance(line, str) for line in footer):
            raise ValueError(f"probe {index}: footer must be a string list")
        if entry["expected_kind"] == "admitted" and not footer:
            raise ValueError(f"probe {index}: admitted probe without a footer")
        if entry["expected_kind"] == "fallback" and footer:
            raise ValueError(f"probe {index}: fallback probe with a footer")
        if entry["mirror"] != "directive":
            continue
        leading = TABLES.is_leading(entry["body"])
        parsed = None
        if leading:
            directive_lines = [line for line in entry["body"] if line.lstrip().startswith("%%{")]
            if len(directive_lines) == 1:
                parsed = TABLES.parse_directive(directive_lines[0].strip())
        admitted = parsed is not None
        expected_admitted = entry["expected_kind"] == "admitted"
        if admitted != expected_admitted:
            raise ValueError(
                f"probe {index} ({entry['label']}): mirror verdict {admitted} != "
                f"{expected_admitted}")
        if admitted:
            lines = [""] + TABLES.footer_lines(*parsed)
            if lines != entry["expected_footer"]:
                raise ValueError(
                    f"probe {index} ({entry['label']}): mirror footer {lines} != "
                    f"{entry['expected_footer']}")
    return sha256_bytes(raw)


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def cohort_digest(entries) -> str:
    payload = "".join(f"{entry['path']}\t{entry['sha256']}\n" for entry in entries)
    return sha256_bytes(payload.encode())


def split_frontmatter(text: str):
    """Return (frontmatter_lines or None, body)."""
    if not text.startswith("---\n"):
        return None, text
    end = text.find("\n---\n", 4)
    if end == -1:
        raise ValueError("unterminated frontmatter")
    return text[4:end].split("\n"), text[end + 5 :]


def config_leaves(frontmatter_lines):
    """The `config:` subtree's scalar leaves, in source order."""
    leaves = {}
    in_config = False
    for line in frontmatter_lines:
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if not line.startswith(" "):
            in_config = line.rstrip() == "config:"
            continue
        if not in_config:
            continue
        stripped = line.strip()
        key, sep, value = stripped.partition(":")
        if not sep or not value.strip():
            raise ValueError(f"nested config entry: {stripped!r}")
        leaves[key] = value.strip()
    for key, value in leaves.items():
        if key not in CONFIG_TABLES:
            raise ValueError(f"unexpected config leaf {key!r}")
        if value not in CONFIG_TABLES[key]:
            raise ValueError(f"unadmitted config value {key}={value!r}")
    return leaves


def footer_lines(leaves) -> list[str]:
    """The state disclosure footer, in the renderer's fixed leaf order."""
    lines = [FOOTER_HEADER]
    if "fontFamily" in leaves:
        lines.append('config.fontFamily="courier" -> terminal font; not applied')
    if "logLevel" in leaves:
        lines.append(f"config.logLevel={leaves['logLevel']} -> logger only; not applied")
    if "look" in leaves:
        lines.append(f"config.look={leaves['look']} -> source node style; not applied")
    if "theme" in leaves:
        lines.append(f"config.theme={leaves['theme']} -> colour palette; not applied")
    return lines


def config_free_twin(frontmatter_lines) -> str:
    """Delete the `config:` block; keep the description and both fences."""
    kept = []
    dropping = False
    for line in frontmatter_lines:
        if not line.strip():
            kept.append(line) if not dropping else None
            continue
        if not line.startswith(" "):
            dropping = line.rstrip() == "config:"
            if not dropping:
                kept.append(line)
            continue
        if not dropping:
            kept.append(line)
    return "\n".join(kept)


def build(repo_root: pathlib.Path):
    cases = repo_root / CASES
    manifest_bytes = (repo_root / MANIFEST).read_bytes()
    manifest = json.loads(manifest_bytes)
    manifest_rows = {row["path"]: row for row in manifest["fixtures"]}

    rows = []
    per_family = {}
    for family, expected in EXPECTED.items():
        selected = []
        for path in sorted((cases / family).rglob("*.mmd")):
            rel = path.relative_to(cases).as_posix()
            frontmatter, body = split_frontmatter(path.read_text())
            if frontmatter is None:
                continue
            lines = [line.strip() for line in body.split("\n")]
            if not any(line == "--" for line in lines):
                continue
            if any(line.startswith("%%{") for line in lines):
                continue
            if any(
                line.startswith(("classDef ", "class ", "style ", "cssClass "))
                for line in lines
            ):
                continue
            selected.append((rel, frontmatter))
        if len(selected) != expected:
            raise ValueError(
                f"{family} region cohort drifted: expected {expected}, got {len(selected)}")
        for rel, frontmatter in selected:
            leaves = config_leaves(frontmatter)
            rows.append(
                {
                    "path": rel,
                    "family": family,
                    "phase": PHASE,
                    "sha256": sha256_file(cases / rel),
                    "manifest_sha256": manifest_rows[rel]["sha256"],
                    "prior_status": {
                        "expected": manifest_rows[rel]["raw"]["expected"],
                        "reason": manifest_rows[rel]["raw"]["reason"],
                    },
                    "config": leaves,
                    "twin": {
                        "kind": "synthetic",
                        "rule": "delete the whole `config:` block from the "
                                "frontmatter; keep description and both fences",
                        "frontmatter": config_free_twin(frontmatter),
                    },
                    # A config-free row draws no disclosure footer at all; the
                    # blank separator only exists in front of a real footer.
                    "expected_footer_lines": ([""] + footer_lines(leaves))
                    if leaves
                    else [],
                    "prediction": "predicted-draw",
                    "expected_states": {
                        "wide_240": "drawn",
                        "narrow_40": "drawn" if rel in NARROW_DRAW else "source",
                        "restored_240": "drawn",
                    },
                    "ledger_note": (
                        "RFC 0048 concurrent-region admission: the config leaves are "
                        "inert (RFC 0031 E2b / RFC 0038 tables), so the frame equals "
                        "the config-free twin's plus the bounded disclosure footer."),
                }
            )
        per_family[family] = len(selected)

    # Phase P2: the leading in-body directive + accessibility rows. The
    # twin deletes the leading directive line and keeps the frontmatter, so
    # the frame equals the directive-free twin's plus the directive footer.
    per_family_p2 = {}
    for family, expected in EXPECTED_P2.items():
        selected = []
        for path in sorted((cases / family).rglob("*.mmd")):
            rel = path.relative_to(cases).as_posix()
            frontmatter, body = split_frontmatter(path.read_text())
            if frontmatter is None:
                continue
            lines = [line.strip() for line in body.split("\n")]
            first = next((line for line in lines if line), "")
            if not first.startswith("%%{"):
                continue
            if not any(line == "--" for line in lines):
                continue
            if any(
                line.startswith(("classDef ", "class ", "style ", "cssClass "))
                for line in lines
            ):
                continue
            directive_lines = [line for line in lines if line.startswith("%%{")]
            if len(directive_lines) != 1:
                raise ValueError(f"{rel}: expected exactly one leading directive")
            parsed = TABLES.parse_directive(directive_lines[0])
            if parsed is None:
                raise ValueError(f"{rel}: the leading directive is outside the closed grammar")
            leaves = config_leaves(frontmatter)
            if leaves:
                raise ValueError(f"{rel}: a P2 row may not carry a frontmatter config")
            if not any(line.startswith(("accTitle:", "accDescr:")) for line in lines):
                raise ValueError(f"{rel}: a P2 row must carry an accessibility scalar")
            selected.append((rel, frontmatter, parsed))
        if len(selected) != expected:
            raise ValueError(
                f"{family} P2 cohort drifted: expected {expected}, got {len(selected)}")
        for rel, frontmatter, parsed in selected:
            directive = {
                "logLevel": parsed[0],
                "theme": parsed[1],
            }
            rows.append(
                {
                    "path": rel,
                    "family": family,
                    "phase": PHASE_P2,
                    "sha256": sha256_file(cases / rel),
                    "manifest_sha256": manifest_rows[rel]["sha256"],
                    "prior_status": {
                        "expected": manifest_rows[rel]["raw"]["expected"],
                        "reason": manifest_rows[rel]["raw"]["reason"],
                    },
                    "config": {},
                    "directive": {k: v for k, v in directive.items() if v is not None},
                    "twin": {
                        "kind": "synthetic",
                        "rule": "delete the leading %%{init}%% directive line; keep "
                                "frontmatter and body",
                        "frontmatter": "\n".join(frontmatter),
                    },
                    "expected_footer_lines": [""] + TABLES.footer_lines(*parsed),
                    "prediction": "predicted-draw",
                    "expected_states": {
                        "wide_240": "drawn",
                        "narrow_40": "drawn" if rel in NARROW_DRAW else "source",
                        "restored_240": "drawn",
                    },
                    "ledger_note": (
                        "RFC 0048 leading in-body directive + accessibility admission: the "
                        "directive is logger/palette only and the accessibility scalars are "
                        "validated no-ops, so the frame equals the directive-free twin's "
                        "plus the bounded footer."),
                }
            )
        per_family_p2[family] = len(selected)

    rows.sort(key=lambda row: row["path"])
    if len(rows) != EXPECTED_TOTAL:
        raise ValueError(f"cohort total drifted: expected {EXPECTED_TOTAL}, got {len(rows)}")
    digest = cohort_digest(rows)
    probes_sha = run_probes(repo_root)
    if probes_sha != PROBES_SHA256:
        raise ValueError(
            f"probe artifact changed without review: sha256 {probes_sha} != {PROBES_SHA256}")
    if digest != COHORT_DIGEST:
        raise ValueError(f"cohort row set drifted: digest {digest} != {COHORT_DIGEST}")
    manifest_sha = sha256_bytes(manifest_bytes)
    if manifest_sha != MANIFEST_SHA256:
        raise ValueError(
            f"manifest changed without review: sha256 {manifest_sha} != {MANIFEST_SHA256}")

    return {
        "schema": SCHEMA,
        "rfc": 48,
        "upstream": {
            "tag": manifest["upstream"]["tag"],
            "commit": manifest["upstream"]["commit"],
            "manifest_sha256": manifest_sha,
        },
        "selection": {
            "rule": "state-diagram or state-diagram-v2 original whose body declares a "
                    "standalone `--` divider line, with no in-body directive line and no "
                    "classDef/class/style/cssClass statement",
            "roots": list(EXPECTED),
            "phase": PHASE,
            "cohort_digest": digest,
            "families": per_family,
            "families_p2": per_family_p2,
            "probes_sha256": probes_sha,
            "exclusions": (
                "the four {dark,default,forest,neutral}-theme rows carry a leading "
                "%%{init}%% directive and belong to phase P2; the four "
                "user-styled-with-regions rows carry state styling and stay whole-source"),
            "width_policy": "wide_240 admission only; narrow_40 is measured per row.",
        },
        "review": {
            "phases": {
                PHASE: {
                    "reviewer_agent": (
                        "independent read-only adversarial code review (fresh "
                        "general-purpose agent, RFC 0048 State concurrent regions, "
                        "2026-11-22)"),
                    "date": "2026-11-22",
                    "verdict": "approved",
                    "note": (
                        "Filled at phase closure: the frame = twin + footer evidence, "
                        "the paired negative audit and the cross-region fail-closed gate."),
                }
            },
            "verdict": "approved",
        },
        "counts": {
            "note": "state_diagram/state_diagram_v2 are the phase-P1 per-family "
                    "counts; phase_p1/phase_p2 are the authoritative phase totals "
                    "and families_p2 holds the phase-P2 per-family counts",
            "total": len(rows),
            "predicted_draw": len(rows),
            # Phase-P1 per-family counts (the phase-P2 rows are in
            # `families_p2`); the explicit phase totals are authoritative.
            "state_diagram": per_family.get("state-diagram", 0),
            "state_diagram_v2": per_family.get("state-diagram-v2", 0),
            "phase_p1": sum(per_family.values()),
            "phase_p2": sum(per_family_p2.values()),
        },
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
    print("probes sha256", ledger["selection"]["probes_sha256"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
