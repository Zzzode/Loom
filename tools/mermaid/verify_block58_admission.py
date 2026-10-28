#!/usr/bin/env python3
"""Strict metadata-only Block58 dual-preset join, 630 -> 688 of 2145.

Consumes independently owned compact verdicts, never frame/pixel artifacts.
Both exact repository presets, fresh old14 joins and B3_REVIEW_APPROVED are
mandatory even for a read-only check. --self-test uses isolated synthetic
metadata and grants no drawing credit. --write is a separate reviewed gate.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location(
    "loom_block58_shapes4_prerequisite", Path(__file__).with_name("verify_shapes4_admission.py"))
SHAPES = importlib.util.module_from_spec(_spec)
_bytecode = sys.dont_write_bytecode
try:
    sys.dont_write_bytecode = True
    _spec.loader.exec_module(SHAPES)
finally:
    sys.dont_write_bytecode = _bytecode
OLD = SHAPES.OLD
need, closed = OLD.need, OLD.closed
APPROVED = "B3_REVIEW_APPROVED"  # No flag/env override, no UNREVIEWED relaxation.
SUMMARY = "Block58_native_whole_source"
JOIN = "Block58_dual_consumer_join"
MARKER = "mermaid-block58-dual-admission.json"
CONTRACT = "Block58/complete-original/v1"
BASE, FINAL, ORIGINALS, DIAGNOSTICS = 630, 688, 2145, 593
SOURCE = "tests/fixtures/mermaid_native/block58_complete_source_proposal.v1.json"
TERMINAL = "tests/fixtures/mermaid_native/block58_terminal_projection_proposal.v1.json"
BOUNDARY = "tests/fixtures/mermaid_native/block58_boundary_contract.v1.json"
DESIGN = "docs/design/mermaid-block58-terminal-contract.md"
MANIFEST = "tests/fixtures/mermaid_official/manifest.json"
SOURCE_SHA = "1d0006ab73cf61951a15599606cde2e46a59865416ac9f43ad244d45b0e4c27f"
TERMINAL_SHA = "b7d0bf628af004718df6ab3550dd09bb0a71481bc2f70b0cf3e47b2584870caf"
BOUNDARY_SHA = "d5c3ce61332082a75737183533d92553cefe4abb5c571afa05c092442244cb07"
DESIGN_SHA = "4df4b2242f959ae1379b965addbc2b599d51654b6276cadf227e3c641360304c"
REVIEWED_DESIGN_SHA = "a0f2976fd5408ede363dd2f8307a282559a887d8a9df4734f91a731d532cc54d"
MANIFEST_SHA = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"
LOCK_SHA = "eb887a461f7194023f7622fb823af3d02d5cb17bb2decf19c4b2b22bf204a877"
ADAPTATION = "block58_complete_original_authority/v1"
IDENTITY = {"source_facts_sha256": SOURCE_SHA, "terminal_facts_sha256": TERMINAL_SHA,
            "source_lock_sha256": LOCK_SHA, "reviewed_design_sha256": REVIEWED_DESIGN_SHA,
            "boundary_sha256": BOUNDARY_SHA, "authority_adaptation": ADAPTATION,
            "authority_review_status": APPROVED}
LOCKS = {"path", "source_sha256", "complete_model_sha256", "terminal_model_sha256",
         "manifest_global_ordinal", "source_witnesses_sha256", "manifest_record_sha256",
         "terminal_source_binding_sha256"}
PROOF = {"local_ready", "captures_before_binding", "captures", "narrow_exact_ordinary",
         "narrow_geometry_good", "uncached_cached_five_planes_equal", "cache_hit_observed",
         "semantic_mutations_rejected", "physical_mutations_rejected"}
NATIVE_CASE = LOCKS | PROOF | {"width", "height"}
OFFICIAL_CASE = LOCKS | PROOF | set(IDENTITY) | {"contract", "official_manifest_sha256", "limitations",
    "foreign_assertions_certified", "native_report_input", "drawing_credit", "formal_admission", "join_required",
    "bound_after_captures", "geometry_and_restoration_good", "markdown_dim_color", "inherited_other_attributes"}
NATIVE_ROOT = set(IDENTITY) | {"schema", "originals", "local_ready", "errors", "semantic_mutations_rejected",
    "physical_mutations_rejected", "drawing_credit", "formal_admission", "limitations", "cases",
    "foreign_assertions_certified", "native_report_input"}
OFFICIAL_ROOT = set(IDENTITY) | {"schema", "contract", "official_manifest_sha256", "limitations", "selected",
    "local_ready", "admitted", "drawing_credit", "formal_admission", "join_required", "foreign_assertions_certified",
    "native_report_input", "actual_color_cells_file", "cases"}
CONSUMERS = (("native", "test_mermaid_block_source58", "tests/ui/test_mermaid_block_source58.cpp"),
             ("official", "test_mermaid_official", "tests/ui/test_mermaid_official.cpp"))
HELPERS = tuple("tests/support/mermaid_block58_" + name + ".hpp" for name in
                ("source", "projection", "contours", "boundary_authority", "authority", "official"))
REVIEWED_HELPER_SHA = {
    'tests/support/mermaid_block58_source.hpp': '4063369a8318db47762d13ff18c6db02441165a6fc7f4334907b4f4786548c03',
    'tests/support/mermaid_block58_projection.hpp': 'bbcb680aa8cf29652a1bfdc7275b4c16e76da52c4ca2a2eed2acb9b6ca4d8d4d',
    'tests/support/mermaid_block58_contours.hpp': '1acab0050d8942520721ea91efbd42760ba930c646328fbfaa6cfe5d569e14e6',
    'tests/support/mermaid_block58_boundary_authority.hpp': '45ffa8e034da0a6aabc10daaaf0787524488eb03171e52b937cf3f25c438d68e',
    'tests/support/mermaid_block58_authority.hpp': '3b609c966bec06bd28b728a0aa94e01463029f51e732b476048a8eb8a1dbecb2',
}
COMMON = ("src/ui/visual/mermaid_block_impl.cpp", "src/ui/visual/mermaid.cppm",
    "src/ui/visual/mermaid_detail.cppm", "src/ui/visual/mermaid_impl.cpp", "src/ui/visual/mermaid_cache_impl.cpp",
    "src/ui/visual/mermaid_testing_seams.cpp", "src/ui/visual/mermaid_cells_impl.cpp", "src/ui/visual/mermaid_envelope_impl.cpp",
    "src/ui/visual/markdown.cppm", "src/ui/visual/markdown_api_impl.cpp", "src/ui/visual/markdown_render_impl.cpp",
    "src/ui/visual/markdown_lexer_impl.cpp", "src/ui/visual/markdown_cache_impl.cpp", "src/ui/visual/markdown_linkify_impl.cpp",
    "src/ui/visual/markdown_component_impl.cpp", "src/ui/visual/markdown_html_impl.cpp",
    "src/ui/visual/markdown_render_code_impl.cpp", "src/ui/visual/code_highlight.cppm", "src/ui/visual/code_highlight_impl.cpp",
    "src/ui/foundation/theme_provider.cppm", "src/ui/foundation/design_tokens.cppm", "src/ui/foundation/design_tokens.cpp",
    "src/cmake/targets/loom_ui_visual.cmake", "tests/ui/CMakeLists.txt") + HELPERS


def count(value, expected, reason):
    need(type(value) is int and value == expected, reason)


def encoded(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def digest(value):
    return sha(encoded(value))


def file_bytes(path, cap=4 * 1024 * 1024):
    need(not path.is_symlink() and path.is_file() and path.stat().st_size <= cap, f"bounded regular file: {path}")
    # Reject a symlinked directory too; resolving only the final component is insufficient.
    need(not any(p.is_symlink() for p in path.parents), f"symlinked input ancestry: {path}")
    need(path.stat().st_mtime_ns <= time.time_ns(), f"future input timestamp: {path}")
    raw = path.read_bytes()
    need(len(raw) <= cap, f"input grew beyond bound: {path}")
    return raw


def parse(raw):
    return json.loads(raw, object_pairs_hook=OLD.unique_object,
                      parse_constant=lambda value: need(False, f"non-finite JSON: {value}"))


def load(path, cap=4 * 1024 * 1024):
    return parse(file_bytes(path, cap))


def pinned(path, expected, cap=4 * 1024 * 1024):
    raw = file_bytes(path, cap)
    need(sha(raw) == expected, f"immutable asset SHA: {path.name}")
    return parse(raw)


def fresh(target, inputs):
    target_stamp = target.stat().st_mtime_ns
    need(all(target_stamp >= p.stat().st_mtime_ns for p in inputs), f"stale runtime evidence: {target}")


def raw_input(root, fact, manifest):
    path = fact["path"]
    need(type(path) is str and Path(path).as_posix() == path and not Path(path).is_absolute() and
         all(p not in ("", ".", "..") for p in path.split("/")), "safe complete-original source path")
    ordinal = fact["manifest_global_ordinal"]
    need(type(ordinal) is int and 0 <= ordinal < ORIGINALS, "exact source ordinal type/range")
    record = manifest["fixtures"][ordinal]
    file = root / "tests/fixtures/mermaid_official/cases" / path
    raw = file_bytes(file, 65536)
    need(record["path"] == path and record["sha256"] == fact["source_sha256"] == sha(raw), "raw/manifest source SHA")
    count(record["bytes"], len(raw), "manifest source bytes")
    count(fact["source_bytes"], len(raw), "source fact bytes")
    need(record["git_blob"] == fact["source_git_blob"] == hashlib.sha1(
         b"blob " + str(len(raw)).encode("ascii") + b"\0" + raw).hexdigest(), "raw Git blob")
    return file, raw, record


def source_inputs(root=ROOT):
    source = pinned(root / SOURCE, SOURCE_SHA)
    terminal = pinned(root / TERMINAL, TERMINAL_SHA)
    boundary = pinned(root / BOUNDARY, BOUNDARY_SHA, 256 * 1024)
    need(sha(file_bytes(root / DESIGN, 65536)) == DESIGN_SHA, "approved current design SHA")
    manifest = pinned(root / MANIFEST, MANIFEST_SHA)
    count(manifest["schema"], 1, "frozen manifest schema")
    need(len(manifest["fixtures"]) == ORIGINALS and len(source["cases"]) == len(terminal["cases"]) == 58,
         "frozen source cardinalities")
    need(source["source_lock_sha256"] == LOCK_SHA and boundary["drawing_credit"] == 0 and len(boundary["fixtures"]) == 36,
         "source lock and independent boundary corpus")
    files = [root / p for p in (SOURCE, TERMINAL, BOUNDARY, DESIGN, MANIFEST)]
    rows, lock, seen = [], bytearray(), set()
    for index, (s, t) in enumerate(zip(source["cases"], terminal["cases"])):
        path, raw, record = raw_input(root, s, manifest)
        files.append(path)
        need(s["path"].startswith("block/") and s["path"] not in seen and t["path"] == s["path"], "58 unique paired Block sources")
        seen.add(s["path"])
        relative = s["path"][6:]
        need(s["case_relative_path"] == relative, "Block relative source path")
        lock.extend(relative.encode() + b"\0" + s["source_sha256"].encode("ascii") + b"\n")
        need(s["full_source"].encode() == raw and s["manifest_record"] == record, "complete raw source/manifest witness")
        for value, key in ((s["complete_model"], "complete_model_sha256"), (s["source_witnesses"], "source_witnesses_sha256"),
                           (record, "manifest_record_sha256")):
            need(digest(value) == s[key], f"canonical source component: {key}")
        need(digest(t["proposed_terminal_model"]) == t["proposed_terminal_model_sha256"], "canonical terminal Model")
        need(digest(t["source_binding"]) == t["source_binding_sha256"], "canonical terminal source binding")
        b = t["source_binding"]
        for key, expected in {"path": s["path"], "manifest_global_ordinal": s["manifest_global_ordinal"],
            "source_sha256": s["source_sha256"], "source_bytes": len(raw), "source_git_blob": s["source_git_blob"],
            "current_source_asset_sha256": SOURCE_SHA, "source_lock_sha256": LOCK_SHA,
            "complete_source_model_sha256": s["complete_model_sha256"], "source_witnesses_sha256": s["source_witnesses_sha256"],
            "manifest_record_sha256": s["manifest_record_sha256"]}.items():
            need(type(b[key]) is type(expected) and b[key] == expected, f"terminal source binding: {key}")
        witnesses = s["source_witnesses"]
        for n, w in enumerate(witnesses):
            count(w["ordinal"], n, "ordered source witness")
            start, end = w["source_byte_start"], w["source_byte_end"]
            need(type(start) is type(end) is int and 0 <= start <= end <= len(raw) and
                 raw[start:end] == w["source_lexeme"].encode(), "source witness raw byte slice")
            count(w["source_line_start"], raw[:start].count(b"\n") + 1, "witness first line")
            count(w["source_line_end"], raw[:max(start, end - 1)].count(b"\n") + 1, "witness last line")
        need(witnesses[s["full_source_witness"]]["source_lexeme"].encode() == raw, "full original witness")
        physical = [witnesses[n]["source_lexeme"].encode() for n in s["physical_line_witnesses"]]
        need(physical == raw.splitlines(keepends=True), "complete ordered physical line witnesses")
        model = t["proposed_terminal_model"]
        notes = any(model[k] for k in ("required_metadata_notices", "required_slot_legend",
                    "required_source_style_declaration_legend", "required_style_loss_notices"))
        edges = bool(model["edges"])
        arrows = any(n["resolved_display_shape"] == "block_arrow" for n in model["nodes"])
        row = {key: s[key] for key in LOCKS - {"terminal_model_sha256", "terminal_source_binding_sha256"}}
        row.update(terminal_model_sha256=t["proposed_terminal_model_sha256"], terminal_source_binding_sha256=t["source_binding_sha256"],
                   semantic=6 + notes + edges + arrows, physical=7 + notes + edges, index=index, raw_expectation=record["raw"])
        rows.append(row)
    need([r["path"] for r in rows] == sorted(seen, key=lambda s: s.encode()) and sha(lock) == LOCK_SHA, "58-source bytewise lock")
    need(sum(r["semantic"] for r in rows) == 384 and sum(r["physical"] for r in rows) == 437, "frozen per-source mutation obligations")
    return rows, manifest, files


def authority_gate(root):
    text = file_bytes(root / HELPERS[4], 131072).decode("utf-8")
    values = re.findall(r'kAuthorityReviewStatus\s*=\s*"([^"\n]+)"\s*;', text)
    need(values == [APPROVED], "B3 authority must be B3_REVIEW_APPROVED; UNREVIEWED is never admissible")

    for relative, expected in REVIEWED_HELPER_SHA.items():
        need(sha(file_bytes(root / relative, 262144)) == expected,
             "reviewed helper bytes changed despite approval status: " + relative)


def validate_pair(native, official, source):
    expected = {r["path"]: r for r in source}
    need(len(expected) == len(source) == 58, "58 unique complete-original obligations")
    indexed = []
    for side, report, keys in (("native", native, NATIVE_ROOT), ("official", official, OFFICIAL_ROOT)):
        closed(report, keys, f"closed {side} compact root")
        need(report["schema"] == f"block58_{side}_complete_original/v1", f"independent {side} consumer schema")
        for key, value in IDENTITY.items():
            need(type(report[key]) is str and report[key] == value, f"approved {side} root pin: {key}")
        need(type(report["limitations"]) is str and 0 < len(report["limitations"]) <= 4096, "explicit bounded authority limitations")
        count(report["local_ready"], 58, "58 local-ready originals")
        count(report["drawing_credit"], 0, "local verdict grants zero drawing credit")
        count(report["foreign_assertions_certified"], 0, "zero foreign assertion credit")
        need(report["formal_admission"] is False and report["native_report_input"] is False, "local proof ownership, not prior formal admission")
        if side == "native":
            for key, value in {"originals": 58, "errors": 0, "semantic_mutations_rejected": 384, "physical_mutations_rejected": 437}.items():
                count(report[key], value, f"exact Native total: {key}")
        else:
            need(report["contract"] == CONTRACT and report["official_manifest_sha256"] == MANIFEST_SHA and
                 report["actual_color_cells_file"] == "mermaid-official-block58.jsonl" and report["join_required"] is True,
                 "Official consumer identity/no frame input")
            count(report["selected"], 58, "58 Official selected originals")
            count(report["admitted"], 0, "no Official self-admission")
        need(type(report["cases"]) is list and len(report["cases"]) == 58, "58 complete compact cases")
        index = {}
        for row in report["cases"]:
            closed(row, NATIVE_CASE if side == "native" else OFFICIAL_CASE, f"closed {side} compact case")
            path = row["path"]
            need(type(path) is str and path in expected and path not in index, "selected complete source identity/uniqueness")
            fact = expected[path]
            for key in LOCKS:
                need(type(row[key]) is type(fact[key]) and row[key] == fact[key], f"compact source lock: {key}")
            for key in ("local_ready", "captures_before_binding", "uncached_cached_five_planes_equal", "cache_hit_observed"):
                need(row[key] is True, f"ordered full-plane proof: {key}")
            count(row["captures"], 15, "all fifteen owned captures")
            ordinary, geometry = row["narrow_exact_ordinary"], row["narrow_geometry_good"]
            need(type(ordinary) is type(geometry) is bool and ordinary != geometry, "narrow source/full-geometry XOR")
            count(row["semantic_mutations_rejected"], fact["semantic"], "exact source semantic controls")
            count(row["physical_mutations_rejected"], fact["physical"] + (side == "official"), "exact source physical controls including Official dim")
            if side == "native":
                need(type(row["width"]) is type(row["height"]) is int and 1 <= row["width"] <= 240 and 1 <= row["height"] <= 1024,
                     "Native complete geometry dimensions")
                need(ordinary == (row["width"] > 40), "Native width/narrow agreement")
            else:
                for key, value in {**IDENTITY, "contract": CONTRACT, "official_manifest_sha256": MANIFEST_SHA,
                                   "limitations": report["limitations"]}.items():
                    need(type(row[key]) is str and row[key] == value, f"Official case ownership: {key}")
                for key in ("foreign_assertions_certified", "drawing_credit"):
                    count(row[key], 0, "Official case zero foreign/drawing credit")
                need(row["native_report_input"] is False and row["formal_admission"] is False and row["join_required"] is True,
                     "Official case is independently local")
                for key in ("bound_after_captures", "geometry_and_restoration_good", "markdown_dim_color"):
                    need(row[key] is True, f"Official own geometry/dim proof: {key}")
                count(row["inherited_other_attributes"], 2, "exact Markdown inherited dim")
            index[path] = row
        indexed.append(index)
    need(native["limitations"] == official["limitations"], "same reviewed authority limitations")
    for path in expected:
        for key in LOCKS | {"narrow_exact_ordinary", "narrow_geometry_good", "semantic_mutations_rejected"}:
            need(indexed[0][path][key] == indexed[1][path][key], "independent consumer source/narrow agreement")
    return [{k: r[k] for k in sorted(LOCKS)} for r in source]


def directories_for(value, root=ROOT):
    parts = value.split(",")
    need(all(parts), "no empty preset path")
    paths = [(root / p).absolute() for p in parts]
    expected = {root / "build" / p / "tests/ui" for p in ("debug", "release")}
    need(len(paths) == 2 and len(set(paths)) == 2 and set(paths) == expected and
         all(p == p.resolve() for p in paths), "both exact repository Debug and Release UI paths required")
    return sorted(paths)


def fingerprint(path, cap=256 * 1024 * 1024):
    # The optional formal marker's absence is itself a guarded input. All
    # required files still fail closed through file_bytes/semantic validation.
    if path.name == MARKER and not path.exists():
        need(not path.is_symlink() and not any(p.is_symlink() for p in path.parents),
             f"symlinked optional marker: {path}")
        return None
    raw = file_bytes(path, cap)
    return sha(raw), len(raw), path.stat().st_mtime_ns


def legacy_prerequisite(directory, matrix, manifest, root):
    inputs = []
    cohorts = []
    for name, relative, expected_sha, selected in (
        ("mindmap10", "tests/fixtures/mermaid_native/mindmap10_complete_source_proposal.v1.json", OLD.FACTS_SHA, False),
        ("shapes4", "tests/fixtures/mermaid_native/mindmap5_shapes_source_proposal.v1.json", SHAPES.FACTS_SHA, True)):
        authority = root / relative
        rows = pinned(authority, expected_sha, 131072)["cases"]
        sources = [raw_input(root, r, manifest)[0] for r in rows]
        if selected:
            rows = rows[1:]
        pair = []
        for side in ("native", "official"):
            report = directory / f"mermaid-{name}-{side}-verdict.json"
            binary = directory / ("test_mermaid_mindmap_semantics" if side == "native" else "test_mermaid_official")
            common = [root / p for p in OLD.COMMON + SHAPES.SHAPES4_INPUTS]
            consumer = root / ("tests/ui/test_mermaid_mindmap_semantics.cpp" if side == "native" else "tests/ui/test_mermaid_official.cpp")
            file_bytes(binary, 256 * 1024 * 1024)
            fresh(binary, common + [consumer])
            value = load(report, 262144)
            fresh(report, [binary, authority, root / MANIFEST, *sources])
            pair.append(value);inputs.extend([report, binary, authority, *sources, *common, consumer])
        joined = (SHAPES.validate_pair if selected else OLD.validate_pair)(*pair, rows)
        marker_path = directory / (SHAPES.MARKER if selected else "mermaid-mindmap10-dual-admission.json")
        marker = load(marker_path, 262144)
        expected = SHAPES.result_for(joined, old=not selected)
        need(encoded(marker) == encoded(expected) and encoded(matrix.get(SHAPES.JOIN if selected else "Mindmap10_dual_consumer_join")) == encoded(expected),
             "exact existing old14 dual-consumer marker/matrix")
        fresh(marker_path, [directory / f"mermaid-{name}-{side}-verdict.json" for side in ("native", "official")])
        # Markers are staged after matrices, so their mtime can legitimately be
        # newer. Both artifacts must be fresh against their actual proof inputs.
        fresh(directory / "mermaid-official-matrix.json", [directory / f"mermaid-{name}-official-verdict.json"])
        summary = matrix[SHAPES.SUMMARY if selected else "Mindmap10_native_whole_source"]
        for key, value in {"selected": 4 if selected else 10, "local_ready": 4 if selected else 10,
                           "admitted": 4 if selected else 10, "base_admitted_originals": 626 if selected else 616,
                           "foreign_assertions_certified": 0}.items():
            count(summary[key], value, "previous admission summary count")
        need(summary["join_required"] is False and summary["source_facts_sha256"] == expected_sha, "old14 prerequisite joined authority")
        SHAPES.matrix_rows(matrix, pair[1], joined, SHAPES.SUMMARY if selected else "Mindmap10_native_whole_source")
        inputs.append(marker_path);cohorts.append(joined)
    need(not set(r["path"] for r in cohorts[0]) & set(r["path"] for r in cohorts[1]), "old14 are disjoint originals")
    return inputs


def matrix_check(matrix, official, source, manifest):
    need(matrix.get("integrity_errors") == [] and matrix.get("manifest_sha256") == MANIFEST_SHA, "matrix integrity/source authority")
    rows = matrix["cases"]
    need(type(rows) is list and len(rows) == ORIGINALS + DIAGNOSTICS, "2738 immutable matrix rows")
    originals = [r for r in rows if r["track"] == "original"]
    bodies = [r for r in rows if r["track"] == "body_diagnostic"]
    need(len(originals) == ORIGINALS and len(bodies) == DIAGNOSTICS, "2145 originals plus 593 bodies")
    index = {}
    for r in originals:
        need(type(r["fixture"]) is str and r["fixture"] not in index, "unique matrix original")
        index[r["fixture"]] = r
    need(set(index) == {r["path"] for r in manifest["fixtures"]}, "all frozen originals retained")
    for f in manifest["fixtures"]:
        need(index[f["path"]]["source_sha256"] == f["sha256"], "every matrix original source SHA")
    body_expected = {f["path"]: f["diagnostic"]["sha256"] for f in manifest["fixtures"] if "diagnostic" in f}
    need(len({r["fixture"] for r in bodies}) == DIAGNOSTICS and all(body_expected.get(r["fixture"]) == r["source_sha256"] for r in bodies),
         "593 source-bound diagnostic identities")
    count(matrix["original_drawing_coverage"]["drawn_at_240"], BASE, "frozen 630 formal baseline")
    count(matrix["original_drawing_coverage"]["pinned_original_files"], ORIGINALS, "frozen denominator")
    count(matrix["original_drawing_coverage"]["original_files_executed"], ORIGINALS, "all originals executed")
    need(type(matrix["original_drawing_coverage"]["percent_of_pinned_original_files"]) is float and
         matrix["original_drawing_coverage"]["percent_of_pinned_original_files"] == 100.0 * BASE / ORIGINALS, "baseline percentage")
    need(JOIN not in matrix, "Block58 must not already be joined")
    summary = matrix[SUMMARY]
    closed(summary, {"selected", "local_ready", "admitted", "join_required", "base_admitted_originals", "source_facts_sha256",
        "terminal_facts_sha256", "foreign_assertions_certified", "native_report_input", "prerequisite_join"}, "closed Block58 summary")
    for key, value in {"selected": 58, "local_ready": 58, "admitted": 0, "base_admitted_originals": BASE, "foreign_assertions_certified": 0}.items():
        count(summary[key], value, "unjoined Block58 summary count")
    need(summary["join_required"] is True and summary["native_report_input"] is False and summary["source_facts_sha256"] == SOURCE_SHA and
         summary["terminal_facts_sha256"] == TERMINAL_SHA and summary["prerequisite_join"] == SHAPES.SUMMARY, "Block58 prerequisite identity")
    local = {r["path"]: r for r in official["cases"]}
    for fact in source:
        r = index[fact["path"]];proof = r[SUMMARY];actual = local[fact["path"]]
        closed(proof, {"contract", "source_case_ordinal", "actual_color_cells_file", "local_ready", "admitted", "join_required",
            "foreign_assertions_certified", "native_report_input", "original_manifest_expectation", "original_foreign_obligations", "actual"}, "closed Block58 per-row provenance")
        need(r["errors"] == [] and r["track"] == "original" and r["type"] == "block" and encoded(proof["actual"]) == encoded(actual), "Official-owned exact matrix actual proof")
        count(proof["source_case_ordinal"], fact["index"], "Block58 source fact ordinal")
        count(proof["foreign_assertions_certified"], 0, "zero matrix foreign certification")
        need(proof["contract"] == CONTRACT and proof["actual_color_cells_file"] == "mermaid-official-block58.jsonl" and
             proof["local_ready"] is True and proof["admitted"] is False and proof["join_required"] is True and proof["native_report_input"] is False,
             "unjoined independent Block58 row")
        need(encoded(proof["original_manifest_expectation"]) == encoded(fact["raw_expectation"]), "retained original manifest expectation")
        debt = proof["original_foreign_obligations"]
        closed(debt, {"status", "foreign_assertions_certified", "foreign_rgb_css_svg_assertions_certified", "scope", "source_manifest",
            "complete_source_model", "terminal_projection_model"}, "closed retained foreign obligations")
        count(debt["foreign_assertions_certified"], 0, "zero retained foreign credit")
        need(debt["status"] == "retained_not_certified" and debt["foreign_rgb_css_svg_assertions_certified"] is False and
             type(debt["scope"]) is str and bool(debt["scope"]), "retained, not certified foreign semantics")
        need(debt["source_manifest"] == {"path": MANIFEST, "sha256": MANIFEST_SHA,
             "json_pointer": f'/fixtures/{fact["manifest_global_ordinal"]}', "record_sha256": fact["manifest_record_sha256"]}, "manifest obligation reference")
        for field, path, asset, pointer, key in (("complete_source_model", SOURCE, SOURCE_SHA, "complete_model", "complete_model_sha256"),
            ("terminal_projection_model", TERMINAL, TERMINAL_SHA, "proposed_terminal_model", "terminal_model_sha256")):
            need(debt[field] == {"path": path, "sha256": asset, "json_pointer": f'/cases/{fact["index"]}/{pointer}', "model_sha256": fact[key]}, "source Model obligation reference")
        need(r["observed"] == {"wide_240": "drawn", "restored_240": "drawn", "narrow_40": "source_fallback" if actual["narrow_exact_ordinary"] else "drawn"},
             "complete matrix geometry/narrow disposition")
    return index


def json_spans(text):
    """Locate values for surgical metadata edits; never re-encode other rows."""
    decoder = json.JSONDecoder(object_pairs_hook=OLD.unique_object)
    spans = {}
    def whitespace(p):
        while p < len(text) and text[p] in " \r\n\t":
            p += 1
        return p
    def walk(p, path):
        p = whitespace(p);start = p
        if text[p] in "{[":
            obj = text[p] == "{";end = "}" if obj else "]";p = whitespace(p + 1);index = 0
            if text[p] != end:
                while True:
                    if obj:
                        key, p = decoder.raw_decode(text, p);need(type(key) is str, "object key type")
                        p = whitespace(p);need(text[p] == ":", "object colon");p += 1
                    else:
                        key = index;index += 1
                    p = whitespace(walk(p, (*path, key)))
                    if text[p] == end:
                        break
                    need(text[p] == ",", "container separator");p = whitespace(p + 1)
            p += 1
        else:
            _, p = decoder.raw_decode(text, p)
        spans[path] = (start, p)
        return p
    need(whitespace(walk(0, ())) == len(text), "complete bounded JSON document")
    return spans


def patch_matrix(text, source, marker):
    matrix = parse(text);before = copy.deepcopy(matrix);spans = json_spans(text);updates = {}
    selected = {r["path"] for r in source}
    for i, r in enumerate(matrix["cases"]):
        if r["track"] == "original" and r["fixture"] in selected:
            updates[("cases", i, SUMMARY, "admitted")] = True
            updates[("cases", i, SUMMARY, "join_required")] = False
    need(len(updates) == 116, "exactly58 selected per-row admission changes")
    updates.update({(SUMMARY, "admitted"): 58, (SUMMARY, "join_required"): False,
        ("original_drawing_coverage", "drawn_at_240"): FINAL,
        ("original_drawing_coverage", "percent_of_pinned_original_files"): 100.0 * FINAL / ORIGINALS})
    edits = [(spans[p][0], spans[p][1], json.dumps(value)) for p, value in updates.items()]
    last = max(end for path, (_, end) in spans.items() if len(path) == 1)
    edits.append((last, last, ',\n    "' + JOIN + '": ' + json.dumps(marker, ensure_ascii=False, indent=4)))
    out = text
    for start, end, replacement in sorted(edits, reverse=True):
        out = out[:start] + replacement + out[end:]
    expected = before
    for path, value in updates.items():
        cursor = expected
        for key in path[:-1]:
            cursor = cursor[key]
        cursor[path[-1]] = value
    expected[JOIN] = marker
    need(parse(out) == expected, "surgical join changed only authorized metadata")
    after = json_spans(out)
    for i, r in enumerate(matrix["cases"]):
        if r["track"] != "original" or r["fixture"] not in selected:
            a, b = spans[("cases", i)];c, d = after[("cases", i)]
            need(text[a:b].encode() == out[c:d].encode(), "nonselected/body row bytes must be exact")
    return out


def verify(directory, source, manifest, assets, root):
    pair, inputs = [], list(assets)
    runtime = [root / p for p in COMMON]
    for side, executable, consumer in CONSUMERS:
        binary = directory / executable;report = directory / f"mermaid-block58-{side}-verdict.json"
        file_bytes(binary, 256 * 1024 * 1024)
        need(os.access(binary, os.X_OK), "executable consumer binary")
        fresh(binary, runtime + [root / consumer, *assets])
        value = load(report, 524288);fresh(report, [binary, *assets])
        pair.append(value);inputs.extend([binary, report, *runtime, root / consumer])
    need((directory / CONSUMERS[0][1]).stat().st_ino != (directory / CONSUMERS[1][1]).stat().st_ino,
         "distinct independently compiled consumer executables")
    joined = validate_pair(*pair, source)
    path = directory / "mermaid-official-matrix.json";raw = file_bytes(path, 20_000_000);matrix = parse(raw)
    # OfficialMatrix writes compact just before the matrix. Native is independent.
    fresh(path, [directory / "mermaid-block58-official-verdict.json"])
    matrix_check(matrix, pair[1], source, manifest)
    inputs.extend(legacy_prerequisite(directory, matrix, manifest, root))
    markdown = directory / "mermaid-official-matrix.md";text = file_bytes(markdown, 2_000_000).decode()
    fresh(markdown, [directory / "mermaid-block58-official-verdict.json"])
    need(text.count("\n## Mindmap10 formal admission\n") == text.count("\n## MindmapShapes4 formal admission\n") == 1,
         "both existing formal-admission Markdown sections required")
    need("\n## Block58 formal admission\n" not in text, "Block58 Markdown must be unjoined")
    marker = {"schema": "block58_dual_consumer_admission/v1", "contract": CONTRACT, **IDENTITY,
        "official_manifest_sha256": MANIFEST_SHA, "base_admitted_originals": BASE, "joined_originals": 58,
        "formal_admitted_originals": FINAL, "pinned_original_files": ORIGINALS, "foreign_assertions_certified": 0,
        "native_semantic_mutations_rejected": 384, "native_physical_mutations_rejected": 437,
        "official_semantic_mutations_rejected": 384, "official_physical_mutations_rejected": 495,
        "cases": joined}
    text += ("\n## Block58 formal admission\n\n"
        "Both independent consumers in Debug and Release pass all58 complete-original source, Git, Model, terminal, witness and manifest locks; "
        "15 captures precede binding, with full-plane/cache/narrow agreement. Native rejects384 semantic +437 physical mutants; "
        "Official rejects384 semantic +495 physical mutants. Fresh Mindmap10 and Shapes4 joins preserve the630 baseline. "
        "Formal coverage: **688/2145 (32.07%)**. Body diagnostics and foreign RGB/CSS/SVG assertions receive no new credit. "
        "This metadata join reads no frames, pixel output or Native-derived Official expectations.\n")
    outputs = {path: patch_matrix(raw.decode(), source, marker), markdown: text,
               directory / MARKER: json.dumps(marker, ensure_ascii=False, indent=2) + "\n"}
    marker_path = directory / MARKER
    if marker_path.exists():
        need(file_bytes(marker_path) == outputs[marker_path].encode("utf-8"),
             "existing Block58 marker must exactly match newly verified admission")
    inputs.extend([path, markdown, marker_path])
    return outputs, {p: fingerprint(p) for p in set(inputs)}, pair


def input_paths(directories, root):
    # Pin path-selection metadata first, then snapshot every candidate input
    # BEFORE validating either preset. Subsequent semantic validation rechecks
    # the immutable anchors, so a selection/snapshot race cannot substitute data.
    paths = {root / p for p in (*COMMON, *OLD.COMMON, *SHAPES.SHAPES4_INPUTS)}
    paths.update(root / p for p in (SOURCE, TERMINAL, BOUNDARY, DESIGN, MANIFEST,
        "tests/ui/test_mermaid_mindmap_semantics.cpp"))
    for relative, expected in ((SOURCE, SOURCE_SHA),
        ("tests/fixtures/mermaid_native/mindmap10_complete_source_proposal.v1.json", OLD.FACTS_SHA),
        ("tests/fixtures/mermaid_native/mindmap5_shapes_source_proposal.v1.json", SHAPES.FACTS_SHA)):
        anchor = root / relative
        for fact in pinned(anchor, expected)["cases"]:
            paths.add(root / "tests/fixtures/mermaid_official/cases" / fact["path"])
        paths.add(anchor)
    paths.update(root / item[2] for item in CONSUMERS)
    for directory in directories:
        paths.update(directory / name for name in (
            "test_mermaid_block_source58", "test_mermaid_official", "test_mermaid_mindmap_semantics",
            "mermaid-block58-native-verdict.json", "mermaid-block58-official-verdict.json",
            "mermaid-mindmap10-native-verdict.json", "mermaid-mindmap10-official-verdict.json",
            "mermaid-shapes4-native-verdict.json", "mermaid-shapes4-official-verdict.json",
            "mermaid-mindmap10-dual-admission.json", SHAPES.MARKER,
            "mermaid-official-matrix.json", "mermaid-official-matrix.md", MARKER))
    return paths


def prepare(directories, root=ROOT):
    need(len(directories) == 2 and set(directories) == {root / "build" / p / "tests/ui" for p in ("debug", "release")},
         "both exact repository presets required before validation")
    guards = {path: fingerprint(path) for path in input_paths(directories, root)}
    authority_gate(root)
    source, manifest, assets = source_inputs(root)
    outputs, pairs = {}, []
    for directory in directories:
        new, observed, pair = verify(directory, source, manifest, assets, root)
        need(set(observed) <= set(guards), "untracked verification input")
        need(all(guards[path] == value for path, value in observed.items()),
             "inputs changed during preset validation; initial snapshot cannot be replaced")
        outputs.update(new);pairs.append(pair)
    need(len(pairs) == 2, "both preset proofs required")
    for side in (0, 1):
        need(pairs[0][side] == pairs[1][side], "Debug/Release compact proof parity")
    need(all(fingerprint(path) == value for path, value in guards.items()),
         "inputs changed after validation; initial snapshot must remain fixed")
    return outputs, guards


def publish_all(outputs, guards, directories):
    """Stage both matrices/Markdown first, markers last; restore originals on failure.

    Hard-link backups preserve original bytes and timestamps on rollback. No
    production utime, no report regeneration and no frame access occurs here.
    """
    expected = {d / name for d in directories for name in ("mermaid-official-matrix.json", "mermaid-official-matrix.md", MARKER)}
    need(set(outputs) == expected and len(directories) == 2, "complete dual-preset publication plan")
    markers = sorted(p for p in outputs if p.name == MARKER)
    regular = sorted(set(outputs) - set(markers));staged, backups = {}, {}
    installed, recovery_failed = [], set()
    try:
        need(all(path in guards for path in markers), "optional marker state must be snapshotted")
        for path in outputs:
            with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent,
                    prefix=path.name + ".join-", delete=False) as stream:
                staged[path] = Path(stream.name);stream.write(outputs[path]);stream.flush();os.fsync(stream.fileno())
        for path in [*regular, *markers]:
            if path.name == MARKER and not path.exists():
                continue
            with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + ".join-backup-", delete=False) as stream:
                backup = Path(stream.name)
            backup.unlink();os.link(path, backup);backups[path] = backup
        need(all(fingerprint(p) == old for p, old in guards.items()), "inputs changed after validation; no publication")
        for path in [*regular, *markers]:
            installed.append(path);os.replace(staged[path], path)
    except BaseException:
        failures = []
        for path in reversed(installed):
            try:
                if path in backups:
                    os.replace(backups[path], path)
                else:
                    path.unlink(missing_ok=True)
            except BaseException as error:
                recovery_failed.add(path)
                failures.append(f"{path}: {error}; backup={backups.get(path)}")
        if failures:
            raise OSError("publication rollback incomplete: " + "; ".join(failures))
        raise
    finally:
        for path in [*staged.values(), *(p for target, p in backups.items() if target not in recovery_failed)]:
            path.unlink(missing_ok=True)


def synthetic_pair(source):
    """Synthetic metadata only; independent from all current consumer reports."""
    native_rows, official_rows = [], []
    for fact in source:
        row = {k: fact[k] for k in LOCKS}
        row.update(local_ready=True, captures=15, captures_before_binding=True, narrow_exact_ordinary=False,
            narrow_geometry_good=True, uncached_cached_five_planes_equal=True, cache_hit_observed=True,
            semantic_mutations_rejected=fact["semantic"], physical_mutations_rejected=fact["physical"])
        native_rows.append({**row, "width": 20, "height": 20})
        official_rows.append({**row, **IDENTITY, "contract": CONTRACT, "official_manifest_sha256": MANIFEST_SHA,
            "limitations": "SYNTHETIC metadata; no drawing proof.", "foreign_assertions_certified": 0,
            "native_report_input": False, "drawing_credit": 0, "formal_admission": False, "join_required": True,
            "bound_after_captures": True, "geometry_and_restoration_good": True, "markdown_dim_color": True,
            "inherited_other_attributes": 2, "physical_mutations_rejected": fact["physical"] + 1})
    common = {**IDENTITY, "local_ready": 58, "drawing_credit": 0, "formal_admission": False,
        "native_report_input": False, "foreign_assertions_certified": 0, "limitations": "SYNTHETIC metadata; no drawing proof."}
    return ({**common, "schema": "block58_native_complete_original/v1", "originals": 58, "errors": 0,
        "semantic_mutations_rejected": 384, "physical_mutations_rejected": 437, "cases": native_rows},
        {**common, "schema": "block58_official_complete_original/v1", "contract": CONTRACT,
        "official_manifest_sha256": MANIFEST_SHA, "selected": 58, "admitted": 0, "join_required": True,
        "actual_color_cells_file": "mermaid-official-block58.jsonl", "cases": official_rows})


def synthetic_matrix(source, manifest, pair, old_pairs, old_sources):
    """A metadata fixture with the real frozen inventory, never a report input."""
    matrix = {"schema": 1, "manifest_sha256": MANIFEST_SHA, "integrity_errors": [], "cases": [],
        "original_drawing_coverage": {"drawn_at_240": BASE, "pinned_original_files": ORIGINALS,
            "original_files_executed": ORIGINALS, "percent_of_pinned_original_files": 100.0 * BASE / ORIGINALS},
        SUMMARY: {"selected": 58, "local_ready": 58, "admitted": 0, "join_required": True,
            "base_admitted_originals": BASE, "source_facts_sha256": SOURCE_SHA, "terminal_facts_sha256": TERMINAL_SHA,
            "foreign_assertions_certified": 0, "native_report_input": False, "prerequisite_join": SHAPES.SUMMARY}}
    for fact in manifest["fixtures"]:
        matrix["cases"].append({"track": "original", "fixture": fact["path"], "type": fact["path"].split("/")[0],
            "source_sha256": fact["sha256"], "errors": [], "observed": {"wide_240": "source_fallback", "narrow_40": "source_fallback", "restored_240": "source_fallback"}})
        if "diagnostic" in fact:
            matrix["cases"].append({"track": "body_diagnostic", "fixture": fact["path"], "source_sha256": fact["diagnostic"]["sha256"],
                "note": "SYNTHETIC diagnostic retained byte-for-byte"})
    index = {r["fixture"]: r for r in matrix["cases"] if r["track"] == "original"}
    for cohort, previous, facts, old in zip(("mindmap10", "shapes4"), old_pairs, old_sources, (True, False)):
        key = "Mindmap10_native_whole_source" if old else SHAPES.SUMMARY
        joined = (OLD.validate_pair if old else SHAPES.validate_pair)(*previous, facts)
        marker = SHAPES.result_for(joined, old=old)
        matrix["Mindmap10_dual_consumer_join" if old else SHAPES.JOIN] = marker
        matrix[key] = {"selected": 10 if old else 4, "local_ready": 10 if old else 4, "admitted": 10 if old else 4,
            "base_admitted_originals": 616 if old else 626, "foreign_assertions_certified": 0, "join_required": False,
            "source_facts_sha256": OLD.FACTS_SHA if old else SHAPES.FACTS_SHA}
        for row in previous[1]["cases"]:
            target = index[row["path"]];target["observed"] = {"wide_240": "drawn", "restored_240": "drawn", "narrow_40": "drawn"}
            target[key] = {"actual": {k: v for k, v in row.items() if k != "path"}}
    for fact, actual in zip(source, pair[1]["cases"]):
        target = index[fact["path"]];target["observed"] = {"wide_240": "drawn", "restored_240": "drawn", "narrow_40": "drawn"}
        target[SUMMARY] = {"contract": CONTRACT, "source_case_ordinal": fact["index"], "actual_color_cells_file": "mermaid-official-block58.jsonl",
            "local_ready": True, "admitted": False, "join_required": True, "foreign_assertions_certified": 0, "native_report_input": False,
            "original_manifest_expectation": fact["raw_expectation"], "actual": copy.deepcopy(actual),
            "original_foreign_obligations": {"status": "retained_not_certified", "foreign_assertions_certified": 0,
                "foreign_rgb_css_svg_assertions_certified": False, "scope": "SYNTHETIC retained metadata, not foreign equivalence.",
                "source_manifest": {"path": MANIFEST, "sha256": MANIFEST_SHA, "json_pointer": f'/fixtures/{fact["manifest_global_ordinal"]}',
                    "record_sha256": fact["manifest_record_sha256"]},
                "complete_source_model": {"path": SOURCE, "sha256": SOURCE_SHA, "json_pointer": f'/cases/{fact["index"]}/complete_model',
                    "model_sha256": fact["complete_model_sha256"]},
                "terminal_projection_model": {"path": TERMINAL, "sha256": TERMINAL_SHA,
                    "json_pointer": f'/cases/{fact["index"]}/proposed_terminal_model', "model_sha256": fact["terminal_model_sha256"]}}}
    return matrix


class IsolatedRepository:
    """All fabricated executable/timestamp evidence stays below a temp root."""
    def __init__(self, real, source, manifest, assets):
        self.temporary = tempfile.TemporaryDirectory(prefix="loom-block58-join-selftest-")
        self.root = Path(self.temporary.name).resolve()
        self.source, self.manifest = source, manifest
        self.pair = synthetic_pair(source)
        source10 = pinned(real / "tests/fixtures/mermaid_native/mindmap10_complete_source_proposal.v1.json", OLD.FACTS_SHA)["cases"]
        source5 = pinned(real / "tests/fixtures/mermaid_native/mindmap5_shapes_source_proposal.v1.json", SHAPES.FACTS_SHA)["cases"]
        self.old_sources = (source10, source5[1:])
        self.old_pairs = (SHAPES.synthetic_pair(source10, old=True), SHAPES.synthetic_pair(source5[1:]))
        paths = set(COMMON) | set(OLD.COMMON) | set(SHAPES.SHAPES4_INPUTS) | {c[2] for c in CONSUMERS}
        paths |= {"tests/ui/test_mermaid_mindmap_semantics.cpp", "tests/fixtures/mermaid_native/mindmap10_complete_source_proposal.v1.json",
                  "tests/fixtures/mermaid_native/mindmap5_shapes_source_proposal.v1.json"}
        paths |= {str(p.relative_to(real)) for p in assets}
        paths |= {str(raw_input(real, fact, manifest)[0].relative_to(real)) for fact in source10 + source5}
        for relative in paths:
            target = self.root / relative;target.parent.mkdir(parents=True, exist_ok=True);shutil.copyfile(real / relative, target)
        authority = self.root / HELPERS[4]
        authority.write_text(re.sub(r'(kAuthorityReviewStatus\s*=\s*)"[^"\n]+"', r'\1"' + APPROVED + '"', authority.read_text()))
        # No current build/report is copied. Synthetic timestamps are bounded in
        # the past, solely inside this temporary repository.
        self.stamp = 1_600_000_000_000_000_000
        for file in self.root.rglob("*"):
            if file.is_file():
                os.utime(file, ns=(self.stamp, self.stamp))
        self.directories = [self.root / "build" / p / "tests/ui" for p in ("debug", "release")]
        self.matrix = synthetic_matrix(source, manifest, self.pair, self.old_pairs, self.old_sources)
        for directory in self.directories:
            directory.mkdir(parents=True)
            for binary in {c[1] for c in CONSUMERS} | {"test_mermaid_mindmap_semantics"}:
                p = directory / binary;p.write_text("SYNTHETIC executable fixture " + binary);p.chmod(0o700);self.stamp_file(p, 1)
            for side, report in zip(("native", "official"), self.pair):
                self.put(directory / f"mermaid-block58-{side}-verdict.json", report, 2)
            for name, pair, sources, old in zip(("mindmap10", "shapes4"), self.old_pairs, self.old_sources, (True, False)):
                for side, report in zip(("native", "official"), pair):
                    self.put(directory / f"mermaid-{name}-{side}-verdict.json", report, 2)
                joined = (OLD.validate_pair if old else SHAPES.validate_pair)(*pair, sources)
                self.put(directory / ("mermaid-mindmap10-dual-admission.json" if old else SHAPES.MARKER), SHAPES.result_for(joined, old=old), 3)
            self.put(directory / "mermaid-official-matrix.json", self.matrix, 4)
            p = directory / "mermaid-official-matrix.md";p.write_text("Prior matrix\n\n## Mindmap10 formal admission\n\n626\n\n## MindmapShapes4 formal admission\n\n630\n");self.stamp_file(p, 4)

    def stamp_file(self, path, step):
        os.utime(path, ns=(self.stamp + step, self.stamp + step))

    def put(self, path, value, step):
        path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n");self.stamp_file(path, step)

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.temporary.cleanup()


class JoinControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source, cls.manifest, cls.assets = source_inputs(ROOT)

    def setUp(self):
        self.pair = synthetic_pair(self.source)

    def fixture(self):
        return IsolatedRepository(ROOT, self.source, self.manifest, self.assets)

    def test_real_frozen_source_components_and_source_derived_counts(self):
        self.assertEqual(len(self.source), 58)
        self.assertEqual(sum(r["semantic"] for r in self.source), 384)
        self.assertEqual(sum(r["physical"] for r in self.source), 437)
        self.assertEqual(len(validate_pair(*self.pair, self.source)), 58)

    def test_closed_roots_cases_types_counts_and_approved_gate(self):
        for side in (0, 1):
            mutations = [lambda r: r.update(authority_review_status="UNREVIEWED_AUTHORITY_ADAPTATION"),
                lambda r: r.update(local_ready=True), lambda r: r.update(native_report_input=True),
                lambda r: r.update(foreign_assertions_certified=True), lambda r: r.update(formal_admission=True),
                lambda r: r.update(drawing_credit=1), lambda r: r.update(frames=[]), lambda r: r["cases"].pop(),
                lambda r: r["cases"].append(copy.deepcopy(r["cases"][0])), lambda r: r["cases"][0].update(captures=10),
                lambda r: r["cases"][0].update(captures=True), lambda r: r["cases"][0].update(local_ready=False),
                lambda r: r["cases"][0].update(captures_before_binding=False), lambda r: r["cases"][0].update(cache_hit_observed=False),
                lambda r: r["cases"][0].update(uncached_cached_five_planes_equal=False),
                lambda r: r["cases"][0].update(semantic_mutations_rejected=0), lambda r: r["cases"][0].update(physical_mutations_rejected=0),
                lambda r: r["cases"][0].update(narrow_exact_ordinary=True), lambda r: r["cases"][0].update(pixel_output={}),
                lambda r: r["cases"][0].pop("captures"), lambda r: r["cases"][0].update(error="resource rejected")]
            for key in LOCKS:
                mutations.append(lambda r, key=key: r["cases"][0].__setitem__(key, -1 if key == "manifest_global_ordinal" else "wrong"))
            for i, mutate in enumerate(mutations):
                bad = copy.deepcopy(self.pair);mutate(bad[side])
                with self.subTest(side=side, control=i), self.assertRaises(ValueError):
                    validate_pair(*bad, self.source)
        for key in ("markdown_dim_color", "bound_after_captures", "geometry_and_restoration_good"):
            bad = copy.deepcopy(self.pair);bad[1]["cases"][0][key] = False
            with self.subTest(official=key), self.assertRaises(ValueError):
                validate_pair(*bad, self.source)
        for key in ("errors", "originals", "semantic_mutations_rejected", "physical_mutations_rejected"):
            for value in (True, -1):
                bad = copy.deepcopy(self.pair);bad[0][key] = value
                with self.subTest(native_total=key, value=value), self.assertRaises(ValueError):
                    validate_pair(*bad, self.source)

    def test_unreviewed_header_future_timestamp_and_path_rejection(self):
        with self.fixture() as f:
            authority_gate(f.root)
            p = f.root / HELPERS[4];p.write_text(p.read_text().replace(APPROVED, "UNREVIEWED_AUTHORITY_ADAPTATION"))
            with self.assertRaisesRegex(ValueError, "B3 authority"):
                prepare(f.directories, f.root)
            with self.assertRaises(ValueError):
                directories_for("build/debug/tests/ui", f.root)
            for value in ("build/debug/tests/ui,build/debug/tests/ui", "build/debug/tests/ui,build/asan/tests/ui", "/tmp/debug/tests/ui,/tmp/release/tests/ui"):
                with self.subTest(path=value), self.assertRaises(ValueError):
                    directories_for(value, f.root)
            p = f.root / SOURCE;os.utime(p, ns=(time.time_ns() + 10**12,) * 2)
            with self.assertRaisesRegex(ValueError, "future"):
                source_inputs(f.root)

    def test_changed_raw_git_fact_model_and_manifest(self):
        with self.fixture() as f:
            fact = self.source[0]
            p = f.root / "tests/fixtures/mermaid_official/cases" / fact["path"];p.write_bytes(p.read_bytes() + b" ")
            with self.assertRaisesRegex(ValueError, "raw/manifest source SHA"):
                source_inputs(f.root)
            p.write_bytes((ROOT / p.relative_to(f.root)).read_bytes());f.stamp_file(p, 0)
            source = load(f.root / SOURCE);source["cases"][0]["source_git_blob"] = "0" * 40
            with self.assertRaisesRegex(ValueError, "raw Git blob"):
                raw_input(f.root, source["cases"][0], self.manifest)
            p = f.root / SOURCE;p.write_bytes(p.read_bytes() + b" ")
            with self.assertRaisesRegex(ValueError, "immutable asset SHA"):
                source_inputs(f.root)
            p.write_bytes((ROOT / SOURCE).read_bytes());f.stamp_file(p, 0)
            p = f.root / MANIFEST;p.write_bytes(p.read_bytes() + b" ")
            with self.assertRaisesRegex(ValueError, "immutable asset SHA"):
                source_inputs(f.root)

    def test_recomputed_model_witness_and_binding_cannot_skip_component_guards(self):
        # Interpose only the in-memory parser result, after the real byte pin.
        # This tests each inner guard without weakening production asset pins.
        real_pinned = pinned
        for kind, message in (("model", "canonical source component"), ("terminal", "canonical terminal Model"),
                              ("witness", "source witness raw byte slice"), ("binding", "terminal source binding")):
            with self.subTest(kind=kind), self.fixture() as f:
                def interpose(path, expected, cap=4 * 1024 * 1024):
                    value = real_pinned(path, expected, cap)
                    if path == f.root / SOURCE:
                        row = value["cases"][0]
                        if kind == "model":
                            row["complete_model"]["nodes"][0]["unreviewed"] = True
                        elif kind == "witness":
                            row["source_witnesses"][0]["source_lexeme"] += "?"
                            row["source_witnesses_sha256"] = digest(row["source_witnesses"])
                    if path == f.root / TERMINAL:
                        row = value["cases"][0]
                        if kind == "terminal":
                            row["proposed_terminal_model"]["nodes"][0]["retained_span"] = 1
                        elif kind == "binding":
                            row["source_binding"]["source_sha256"] = "0" * 64
                            row["source_binding_sha256"] = digest(row["source_binding"])
                        elif kind == "witness":
                            witnesses = real_pinned(f.root / SOURCE, SOURCE_SHA)["cases"][0]["source_witnesses"]
                            witnesses[0]["source_lexeme"] += "?"
                            row["source_binding"]["source_witnesses_sha256"] = digest(witnesses)
                            row["source_binding_sha256"] = digest(row["source_binding"])
                    return value
                with patch.dict(globals(), pinned=interpose), self.assertRaisesRegex(ValueError, message):
                    source_inputs(f.root)

    def test_whole_pair_and_reports_are_independent(self):
        bad = list(copy.deepcopy(self.pair));bad[1] = copy.deepcopy(bad[0])
        with self.assertRaises(ValueError):
            validate_pair(*bad, self.source)
        bad = copy.deepcopy(self.pair);bad[1]["cases"][0].update(narrow_exact_ordinary=True, narrow_geometry_good=False)
        with self.assertRaisesRegex(ValueError, "consumer source/narrow"):
            validate_pair(*bad, self.source)

    def test_matrix_ownership_foreign_debt_baseline_and_missing_selected(self):
        with self.fixture() as f:
            matrix_check(f.matrix, self.pair[1], self.source, self.manifest)
            selected = next(i for i, r in enumerate(f.matrix["cases"]) if r.get("fixture") == self.source[0]["path"])
            changes = [lambda m: m["cases"].pop(), lambda m: m["integrity_errors"].append("bad"),
                lambda m: m["original_drawing_coverage"].update(drawn_at_240=688),
                lambda m: m[SUMMARY].update(admitted=58), lambda m: m.update({JOIN: {}}),
                lambda m: m["cases"][selected][SUMMARY]["actual"].update(native_report_input=True),
                lambda m: m["cases"][selected][SUMMARY]["actual"].update(foreign_assertions_certified=False),
                lambda m: m["cases"][selected][SUMMARY].update(original_manifest_expectation={}),
                lambda m: m["cases"][selected][SUMMARY]["original_foreign_obligations"].update(foreign_rgb_css_svg_assertions_certified=True),
                lambda m: m["cases"][selected]["observed"].update(wide_240="source_fallback"),
                lambda m: m["cases"][selected]["errors"].append("rejected")]
            for i, mutate in enumerate(changes):
                bad = copy.deepcopy(f.matrix);mutate(bad)
                with self.subTest(control=i), self.assertRaises(ValueError):
                    matrix_check(bad, self.pair[1], self.source, self.manifest)

    def test_complete_dual_prepare_is_read_only_and_retains_nonselected_bytes(self):
        with self.fixture() as f:
            before = {p: p.read_bytes() for d in f.directories for p in d.iterdir()}
            outputs, guards = prepare(f.directories, f.root)
            self.assertEqual(len(outputs), 6)
            self.assertEqual(before, {p: p.read_bytes() for d in f.directories for p in d.iterdir()})
            for directory in f.directories:
                updated = parse(outputs[directory / "mermaid-official-matrix.json"])
                self.assertEqual(updated["original_drawing_coverage"]["drawn_at_240"], FINAL)
                self.assertEqual(updated[SUMMARY]["admitted"], 58)
                self.assertEqual(updated[SHAPES.JOIN], f.matrix[SHAPES.JOIN])
                self.assertEqual(updated["Mindmap10_dual_consumer_join"], f.matrix["Mindmap10_dual_consumer_join"])
                self.assertTrue(outputs[directory / "mermaid-official-matrix.md"].startswith((directory / "mermaid-official-matrix.md").read_text()))
            publish_all(outputs, guards, f.directories)
            self.assertTrue(all((d / MARKER).exists() for d in f.directories))
            with self.assertRaisesRegex(ValueError, "frozen 630 formal baseline"):
                prepare(f.directories, f.root)

    def test_fresh_unjoined_pair_can_refresh_exact_previous_marker(self):
        with self.fixture() as f:
            original_outputs, _ = prepare(f.directories, f.root)
            # A complete CTest rerun replaces matrices with fresh unjoined
            # inputs but retains the previous exact formal marker on disk.
            for directory in f.directories:
                path = directory / MARKER
                path.write_text(original_outputs[path])
            refreshed, guards = prepare(f.directories, f.root)
            self.assertEqual(refreshed, original_outputs)
            publish_all(refreshed, guards, f.directories)
            for directory in f.directories:
                self.assertEqual((directory / MARKER).read_text(), original_outputs[directory / MARKER])
                matrix = load(directory / "mermaid-official-matrix.json")
                self.assertEqual(matrix["original_drawing_coverage"]["drawn_at_240"], FINAL)

    def test_different_previous_marker_rejects_without_publication(self):
        for preset in (0, 1):
            with self.subTest(preset=preset), self.fixture() as f:
                outputs, _ = prepare(f.directories, f.root)
                path = f.directories[preset] / MARKER
                marker = parse(outputs[path])
                marker["foreign_assertions_certified"] = False
                path.write_text(json.dumps(marker, ensure_ascii=False, indent=2) + "\n")
                before = {p: p.read_bytes() for d in f.directories for p in d.iterdir()}
                with self.assertRaisesRegex(ValueError, "existing Block58 marker"):
                    prepare(f.directories, f.root)
                self.assertEqual(before, {p: p.read_bytes() for d in f.directories for p in d.iterdir()})

    def test_optional_marker_changes_after_prepare_reject_before_publication(self):
        for existing in (False, True):
            with self.subTest(existing=existing), self.fixture() as f:
                outputs, _ = prepare(f.directories, f.root)
                if existing:
                    for directory in f.directories:
                        (directory / MARKER).write_text(outputs[directory / MARKER])
                outputs, guards = prepare(f.directories, f.root)
                marker = f.directories[0] / MARKER
                marker.write_text("concurrent external marker")
                before = {p: p.read_bytes() for d in f.directories for p in d.iterdir()}
                with self.assertRaisesRegex(ValueError, "inputs changed"):
                    publish_all(outputs, guards, f.directories)
                self.assertEqual(before, {p: p.read_bytes() for d in f.directories for p in d.iterdir()})
                for directory in f.directories:
                    self.assertFalse(list(directory.glob("*.join-*")))

    def test_existing_markers_survive_interrupt_before_or_after_each_replace(self):
        for after in (False, True):
            for fail_at in range(1, 7):
                with self.subTest(after=after, fail_at=fail_at), tempfile.TemporaryDirectory() as temp:
                    root = Path(temp).resolve()
                    directories = [root / p for p in ("debug", "release")]
                    outputs, guards, before = {}, {}, {}
                    for directory in directories:
                        directory.mkdir()
                        for name in ("mermaid-official-matrix.json", "mermaid-official-matrix.md", MARKER):
                            path = directory / name
                            path.write_text("old " + name)
                            outputs[path] = "new " + name
                            guards[path] = fingerprint(path)
                            before[path] = (path.read_bytes(), path.stat().st_mtime_ns)
                    replace, calls = os.replace, []
                    def interrupted(src, dst):
                        calls.append(Path(dst))
                        if len(calls) == fail_at and not after:
                            raise KeyboardInterrupt("before replace")
                        replace(src, dst)
                        if len(calls) == fail_at and after:
                            raise KeyboardInterrupt("after replace")
                    with patch.object(os, "replace", side_effect=interrupted), self.assertRaises(KeyboardInterrupt):
                        publish_all(outputs, guards, directories)
                    self.assertEqual(before, {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in before})
                    for directory in directories:
                        self.assertFalse(list(directory.glob("*.join-*")))

    def test_missing_binary_stale_helpers_raw_report_marker_and_one_preset(self):
        with self.fixture() as f:
            directory = f.directories[0]
            binary = directory / CONSUMERS[0][1];saved = binary.read_bytes();binary.unlink()
            with self.assertRaises(ValueError):
                prepare(f.directories, f.root)
            binary.write_bytes(saved);binary.chmod(0o700);f.stamp_file(binary, 1)
            for relative in (COMMON[0], HELPERS[0], HELPERS[4], CONSUMERS[0][2], SOURCE):
                p = f.root / relative;f.stamp_file(p, 9)
                with self.subTest(stale=relative), self.assertRaisesRegex(ValueError, "stale runtime"):
                    prepare(f.directories, f.root)
                f.stamp_file(p, 0)
            p = directory / "mermaid-block58-native-verdict.json";f.stamp_file(p, 0)
            with self.assertRaisesRegex(ValueError, "stale runtime"):
                prepare(f.directories, f.root)
            f.stamp_file(p, 2)
            p = directory / SHAPES.MARKER;p.unlink()
            with self.assertRaises(ValueError):
                prepare(f.directories, f.root)
            with self.assertRaises(ValueError):
                prepare(f.directories[:1], f.root)

    def test_debug_release_disagreement_blocks_every_write(self):
        with self.fixture() as f:
            p = f.directories[1] / "mermaid-block58-native-verdict.json";bad = load(p);bad["cases"][0]["height"] += 1;f.put(p, bad, 2)
            with self.assertRaisesRegex(ValueError, "Debug/Release compact proof parity"):
                prepare(f.directories, f.root)
            self.assertTrue(all(not (d / MARKER).exists() for d in f.directories))

    def test_old14_marker_types_and_one_preset_publication_are_rejected(self):
        with self.fixture() as f:
            p = f.directories[0] / SHAPES.MARKER;value = load(p);value["foreign_assertions_certified"] = False;f.put(p, value, 3)
            with self.assertRaisesRegex(ValueError, "old14 dual-consumer"):
                prepare(f.directories, f.root)
            directory = f.directories[0]
            outputs = {directory / name: "synthetic" for name in ("mermaid-official-matrix.json", "mermaid-official-matrix.md", MARKER)}
            with self.assertRaisesRegex(ValueError, "dual-preset"):
                publish_all(outputs, {}, [directory])
            self.assertFalse((directory / MARKER).exists())

    def test_no_frame_or_native_derived_official_report_is_opened(self):
        with self.fixture() as f:
            reader = file_bytes;opened = []
            def checked(path, cap=4 * 1024 * 1024):
                opened.append(path)
                self.assertFalse(path.name.endswith(".jsonl"))
                self.assertNotIn(path.name, ("mermaid-block58-native.json", "mermaid-block58-official.json"))
                return reader(path, cap)
            with patch.dict(globals(), file_bytes=checked):
                outputs, _ = prepare(f.directories, f.root)
            self.assertEqual(len(outputs), 6)
            self.assertTrue(all(any(p.name == f"mermaid-block58-{side}-verdict.json" for p in opened) for side in ("native", "official")))

    def test_atomic_stage_each_replace_failure_and_timestamps_rollback(self):
        for fail_at in range(1, 8):
            with self.subTest(failure=fail_at), tempfile.TemporaryDirectory() as temp:
                root = Path(temp).resolve();directories = [root / p for p in ("debug", "release")];outputs = {};guards = {};before = {}
                for d in directories:
                    d.mkdir()
                    for name in ("mermaid-official-matrix.json", "mermaid-official-matrix.md", MARKER):
                        p = d / name;outputs[p] = "new " + name
                        if name != MARKER:
                            p.write_text("old " + name);guards[p] = fingerprint(p);before[p] = (p.read_bytes(), p.stat().st_mtime_ns)
                        else:
                            guards[p] = fingerprint(p)
                replace = os.replace;calls = []
                def injected(src, dst):
                    calls.append(Path(dst))
                    if len(calls) == fail_at:
                        raise OSError("injected replace")
                    replace(src, dst)
                if fail_at == 7:
                    with patch.object(os, "fsync", side_effect=OSError("injected staging")), self.assertRaises(OSError):
                        publish_all(outputs, guards, directories)
                else:
                    with patch.object(os, "replace", side_effect=injected), self.assertRaises(OSError):
                        publish_all(outputs, guards, directories)
                self.assertEqual(before, {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in before})
                self.assertTrue(all(not (d / MARKER).exists() for d in directories))
                self.assertFalse(list(root.rglob("*.join-*")))

    def test_approved_status_cannot_hide_modified_helper_bytes(self):
        for relative in REVIEWED_HELPER_SHA:
            with self.subTest(helper=relative), self.fixture() as f:
                p = f.root / relative;p.write_bytes(p.read_bytes() + b"\n// unreviewed semantic edit\n")
                f.stamp_file(p, 0)
                with self.assertRaisesRegex(ValueError, "reviewed helper bytes"):
                    prepare(f.directories, f.root)
                self.assertTrue(all(not (d / MARKER).exists() for d in f.directories))

    def test_initial_snapshot_rejects_change_before_semantic_validation_finishes(self):
        with self.fixture() as f:
            reader = source_inputs
            def changed(root):
                result = reader(root)
                p = root / "tests/fixtures/mermaid_official/cases" / f.source[0]["path"]
                p.write_bytes(p.read_bytes() + b" ");f.stamp_file(p, 0)
                return result
            with patch.dict(globals(), source_inputs=changed), self.assertRaisesRegex(ValueError, "initial snapshot"):
                prepare(f.directories, f.root)
            self.assertTrue(all(not (d / MARKER).exists() for d in f.directories))

    def test_shared_input_cannot_be_overwritten_by_later_preset_guard(self):
        with self.fixture() as f:
            reader = verify;calls = []
            def changed(*args):
                result = reader(*args);calls.append(args[0])
                if len(calls) == 1:
                    p = f.root / COMMON[0];p.write_bytes(p.read_bytes() + b"\n")
                    f.stamp_file(p, 0)  # Fresh mtimes must not bypass content guards.
                return result
            with patch.dict(globals(), verify=changed), self.assertRaisesRegex(ValueError, "initial snapshot"):
                prepare(f.directories, f.root)
            self.assertEqual(len(calls), 2)
            self.assertTrue(all(not (d / MARKER).exists() for d in f.directories))

    def test_keyboard_interrupt_before_or_after_each_replace_rolls_back(self):
        for fail_at in range(1, 7):
            for after in (False, True):
                with self.subTest(step=fail_at, after=after), tempfile.TemporaryDirectory() as temp:
                    root = Path(temp).resolve();directories = [root / p for p in ("debug", "release")]
                    outputs, guards, before = {}, {}, {}
                    for d in directories:
                        d.mkdir()
                        for name in ("mermaid-official-matrix.json", "mermaid-official-matrix.md", MARKER):
                            p = d / name;outputs[p] = "new " + name
                            if name != MARKER:
                                p.write_text("old " + name);guards[p] = fingerprint(p);before[p] = (p.read_bytes(), p.stat().st_mtime_ns)
                            else:
                                guards[p] = fingerprint(p)
                    replace = os.replace;calls = []
                    def interrupted(src, dst):
                        calls.append(Path(dst))
                        if len(calls) == fail_at and not after:
                            raise KeyboardInterrupt("before replace")
                        replace(src, dst)
                        if len(calls) == fail_at and after:
                            raise KeyboardInterrupt("after replace")
                    with patch.object(os, "replace", side_effect=interrupted), self.assertRaises(KeyboardInterrupt):
                        publish_all(outputs, guards, directories)
                    self.assertEqual(before, {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in before})
                    self.assertTrue(all(not (d / MARKER).exists() for d in directories))
                    self.assertFalse(list(root.rglob("*.join-*")))

    def test_failed_interrupt_recovery_keeps_original_backup(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve();directories = [root / p for p in ("debug", "release")]
            outputs, guards, before = {}, {}, {}
            for d in directories:
                d.mkdir()
                for name in ("mermaid-official-matrix.json", "mermaid-official-matrix.md", MARKER):
                    p = d / name;outputs[p] = "new " + name
                    if name != MARKER:
                        p.write_text("old " + name);guards[p] = fingerprint(p);before[p] = p.read_bytes()
                    else:
                        guards[p] = fingerprint(p)
            regular = sorted(p for p in outputs if p.name != MARKER);victim = regular[0]
            replace = os.replace;calls = []
            def interrupted(src, dst):
                calls.append(Path(dst))
                if len(calls) == 2:
                    raise KeyboardInterrupt("publish interrupted")
                if Path(dst) == victim and ".join-backup-" in Path(src).name:
                    raise OSError("injected recovery failure")
                replace(src, dst)
            with patch.object(os, "replace", side_effect=interrupted), self.assertRaisesRegex(OSError, "rollback incomplete"):
                publish_all(outputs, guards, directories)
            backups = list(victim.parent.glob(victim.name + ".join-backup-*"))
            self.assertEqual(len(backups), 1);self.assertEqual(backups[0].read_bytes(), before[victim])
            self.assertTrue(all(not (d / MARKER).exists() for d in directories))

    def test_changed_input_after_prepare_stops_publication(self):
        with self.fixture() as f:
            outputs, guards = prepare(f.directories, f.root)
            p = f.root / COMMON[0];p.write_bytes(p.read_bytes() + b"\n")
            with self.assertRaisesRegex(ValueError, "inputs changed"):
                publish_all(outputs, guards, f.directories)
            self.assertTrue(all(not (d / MARKER).exists() for d in f.directories))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dirs", default="build/debug/tests/ui,build/release/tests/ui")
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        need(not args.write, "self-test grants zero admission; --write is forbidden")
        return int(not unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(JoinControls)).wasSuccessful())
    directories = directories_for(args.build_dirs)
    outputs, guards = prepare(directories)
    if args.write:
        publish_all(outputs, guards, directories)
    print("OK: both presets verified,58 complete originals; formal candidate688/2145 (32.07%)" +
          (" published" if args.write else "; validation only, no files written"))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, KeyError, TypeError, IndexError, UnicodeError, RecursionError) as error:
        raise SystemExit(f"Block58 admission refused: {error}")
