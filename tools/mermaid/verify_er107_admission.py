#!/usr/bin/env python3
"""Strict metadata-only ER107 join: frozen688 -> prospective795/2145.

Reads compact independently owned verdicts only; never opens frame artifacts.
Default is read-only. --write is gated by independently reviewed exact helper
bytes and all four fresh proofs. --self-test uses synthetic metadata only.
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
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location(
    "loom_er107_block58_prerequisite", Path(__file__).with_name("verify_block58_admission.py"))
BLOCK = importlib.util.module_from_spec(_spec)
_bytecode = sys.dont_write_bytecode
try:
    sys.dont_write_bytecode = True
    _spec.loader.exec_module(BLOCK)
finally:
    sys.dont_write_bytecode = _bytecode
need, closed, count = BLOCK.need, BLOCK.closed, BLOCK.count
encoded, digest, sha = BLOCK.encoded, BLOCK.digest, BLOCK.sha
file_bytes, load, parse, pinned, fresh = BLOCK.file_bytes, BLOCK.load, BLOCK.parse, BLOCK.pinned, BLOCK.fresh
BASE, FINAL, ORIGINALS, DIAGNOSTICS, SELECTED = 688, 795, 2145, 593, 107
SUMMARY, JOIN = "ER107_native_whole_source", "ER107_dual_consumer_join"
MARKER = "mermaid-er107-dual-admission.json"
CONTRACT = "ER107/complete-original/v1"
APPROVED = "B3_REVIEW_APPROVED"
SOURCE = "tests/fixtures/mermaid_native/er107_complete_source_proposal.v1.json"
TERMINAL = "tests/fixtures/mermaid_native/er107_terminal_projection_proposal.v1.json"
PRESENTATION = "tests/fixtures/mermaid_native/er107_presentation_source_audit.v1.json"
PRIOR = "tests/fixtures/mermaid_native/er206_native_whole_source_authority.config62.v1.json"
DESIGN = "docs/design/mermaid-er107-terminal-contract.md"
MANIFEST = BLOCK.MANIFEST
SOURCE_SHA = "5a1c642cc7a3e2cf08e00355be845711520f762fc999dc13fe7dbb4a417297f3"
# Pin the checked-in contact-clarified proposal and contract. They remain
# unreviewed for B3; identity verification must not be confused with approval.
TERMINAL_SHA = "f8f7dedc651112ac553ea86d200f2562ffb05d54b2b09df7d6477a687a8de7c9"
PRESENTATION_SHA = "8f35938eb74591a428823aae3c1279c4a333fa8240b7d92181e77e7f22ed18ba"
PRIOR_SHA = "76d54ecbc725e59645501f7079f9387b272172d584703f0ea72f143c83c469db"
DESIGN_SHA = "eb6874b488d076a481fc184b7819ede9112552dbe11e2b5719eb03e55c39e8c1"
MANIFEST_SHA = BLOCK.MANIFEST_SHA
LOCK_SHA = "edbf30ae159e9e9b2bb3ff6a16f74ff0a75df18fb45520627425810c99aa71f1"
SOURCE_HELPER_SHA = "3b82b9258809cd3874f286bc51aec7feb7650fc54fb9e1c7c885cddfab9cc59b"
UPSTREAM = "1fb7869866dd619168d840291c9c54b4f129758b"
# Frozen before any ER107 matrix integration. Excludes only107 original rows,
# retaining all688 drawing rows, all593 body rows and every other original.
BASELINE_MATRIX_SHA = "ef0f12e6703ae1a30c6d0f533735d5908ea73b03040b03509fa72db90ce64997"
RETAINED_ROWS_SHA = "08b9a546ba4dc6b2ab30da52e233c39effed5292f1f1410009b23a52347f8ee3"
HELPERS = ("tests/support/mermaid_er107_source.hpp", "tests/support/mermaid_er107_authority.hpp",
           "tests/support/mermaid_er107_official.hpp", "tests/support/mermaid_relations_oracle.hpp")
# Deliberately closed pending Main's independent B3 review receipt. A string
# status alone cannot approve arbitrary subsequently edited checker bytes.
REVIEWED_HELPER_SHA: dict[str, str] = {}
CONSUMERS = (("native", "test_mermaid_er107_source107", "tests/ui/test_mermaid_er107_source107.cpp"),
             ("official", "test_mermaid_official", "tests/ui/test_mermaid_official.cpp"))
COMMON = tuple(dict.fromkeys(BLOCK.COMMON + HELPERS + (
    "src/ui/visual/mermaid_er_impl.cpp", "src/ui/visual/mermaid_relations_core_impl.cpp")))
NATIVE_ID = {"schema": "er107_native_complete_source/v1", "consumer": "NativeER107",
    "source_facts_sha256": SOURCE_SHA, "terminal_facts_sha256": TERMINAL_SHA,
    "presentation_sha256": PRESENTATION_SHA, "contract_snapshot_sha256": DESIGN_SHA,
    "source_lock_sha256": LOCK_SHA, "manifest_sha256": MANIFEST_SHA,
    "drawing_credit": 0, "formal_admission": False, "native_report_input": False,
    "foreign_assertions_certified": 0, "production_work_measured": False,
    "authority_adaptation": "er107_complete_source_surface/v1", "authority_review_status": APPROVED}
OFFICIAL_ID = {"contract": CONTRACT, "source_facts_sha256": SOURCE_SHA,
    "projection_facts_sha256": TERMINAL_SHA, "presentation_facts_sha256": PRESENTATION_SHA,
    "contract_sha256": DESIGN_SHA, "source_helper_sha256": SOURCE_HELPER_SHA,
    "prior_authority_sha256": PRIOR_SHA, "manifest_sha256": MANIFEST_SHA, "source_lock_sha256": LOCK_SHA,
    "authority_adaptation": NATIVE_ID["authority_adaptation"], "authority_review_status": APPROVED,
    "foreign_assertions_certified": 0, "native_report_input": False, "drawing_credit": 0,
    "formal_admission": False, "join_required": True}
LOCKS = {"path", "source_sha256", "manifest_global_ordinal", "complete_model_sha256",
         "terminal_model_sha256", "source_model_sha256", "source_witnesses_sha256",
         "manifest_record_sha256", "source_binding_sha256"}
NATIVE_CASE = set(NATIVE_ID) | LOCKS | {"limitations", "local_ready", "captures_before_binding", "captures",
    "factory_uncached_accepted", "factory_cache_miss_accepted", "factory_cache_hit_accepted", "cache_hits",
    "width", "body_height", "total_rows", "all_fifteen_frames_equal_across_modes",
    "semantic_mutants_rejected", "physical_mutants_rejected", "mutants_not_applicable"}
NATIVE_ROOT = set(NATIVE_ID) | {"limitations", "originals", "all_source_captures_completed",
    "aggregate_binding_after_all_captures", "local_ready", "scene_positives_before_aggregate", "errors",
    "semantic_mutants_rejected", "physical_mutants_rejected", "independent_stage_oracle_cap", "cases"}
OFFICIAL_EXTRA_LOCKS = {"source_case_ordinal", "prior_ER206_ordinal", "source_bytes", "source_git_blob",
    "source_case_sha256", "prior_ER206_case_sha256", "metadata_sha256", "config_obligations_sha256",
    "projection_case_sha256", "whole_scene_model_sha256"}
OFFICIAL_CASE = set(OFFICIAL_ID) | LOCKS | OFFICIAL_EXTRA_LOCKS | {"limitations", "local_ready", "captures",
    "captures_before_binding", "bound_after_captures", "full_frames_published", "geometry_and_restoration_good",
    "narrow_exact_ordinary", "narrow_geometry_good", "uncached_cached_five_planes_equal", "cache_hit_observed",
    "markdown_dim_color", "inherited_other_attributes", "dim_normalization", "width", "height", "body_width", "body_height",
    "semantic_mutations_rejected", "physical_mutations_rejected", "verification_resource_exhausted", "phase",
    "mutations_executed", "mutations_inapplicable"}
OFFICIAL_ROOT = set(OFFICIAL_ID) | {"schema", "limitations", "selected", "local_ready", "errors", "resource_errors",
    "captured_originals", "captures_per_original", "aggregate_after_all_captures", "root_components_validated",
    "semantic_mutations_rejected", "physical_mutations_rejected", "admitted", "base_admitted_originals",
    "actual_color_cells_file", "cases"}


def identity(value, expected, reason):
    for key, wanted in expected.items():
        need(key in value and type(value[key]) is type(wanted) and value[key] == wanted, reason + ": " + key)


def source_inputs(root=ROOT):
    source = pinned(root / SOURCE, SOURCE_SHA)
    terminal = pinned(root / TERMINAL, TERMINAL_SHA)
    presentation = pinned(root / PRESENTATION, PRESENTATION_SHA)
    prior = pinned(root / PRIOR, PRIOR_SHA)
    manifest = pinned(root / MANIFEST, MANIFEST_SHA)
    need(sha(file_bytes(root / DESIGN, 65536)) == DESIGN_SHA, "pinned design contract")
    need(manifest["upstream"]["commit"] == UPSTREAM, "pinned upstream Git commit")
    need(len(manifest["fixtures"]) == ORIGINALS and len(prior["cases"]) == 206 and
         len(source["cases"]) == len(terminal["cases"]) == SELECTED, "frozen2145/206/107 populations")
    selected = [c for c in prior["cases"] if c["whole_model"] is None and
                {d["kind"] for d in c["whole_null_debts"]} == {"active_config"}]
    need(len(selected) == SELECTED and sum(c["whole_model"] is not None for c in prior["cases"]) == 62,
         "exact source-owned ER206 active-config selection and prior62")
    need([c["path"] for c in source["cases"]] == sorted(c["fixture"] for c in selected), "exact sorted107 cohort")
    previous = {c["fixture"]: c for c in selected}
    files = [root / p for p in (SOURCE, TERMINAL, PRESENTATION, PRIOR, DESIGN, MANIFEST)]
    facts, lock = [], bytearray()
    for i, (s, t) in enumerate(zip(source["cases"], terminal["cases"])):
        file, raw, record = BLOCK.raw_input(root, s, manifest)
        files.append(file);p = previous[s["path"]]
        count(s["ordinal"], i, "ordered B1 source case")
        count(t["ordinal"], i, "ordered projection case")
        need(t["path"] == s["path"] and raw == s["full_source"].encode() and s["manifest_record"] == record,
             "complete raw/manifest/source/projection identity")
        need(digest(p) == s["prior_ER206_case_canonical_sha256"] and
             p["ordinal"] == s["prior_ER206_ordinal"], "frozen ER206 case binding")
        for field in ("source_model", "source_witnesses", "manifest_record"):
            need(digest(s[field]) == s[field + "_sha256"], "B1 complete component digest: " + field)
        for field in ("terminal_model", "source_binding"):
            need(digest(t[field]) == t[field + "_sha256"], "projection full component digest: " + field)
        need(digest(t["terminal_model"]["whole_scene_model"]) == t["whole_scene_model_sha256"], "complete ER scene Model digest")
        need(digest({"source_binding": t["source_binding"], "terminal_model": t["terminal_model"],
                     "shared_contract": terminal["shared_contract"]}) == t["complete_model_sha256"], "complete shared-law Model binding")
        witnesses = s["source_witnesses"]
        for ordinal, w in enumerate(witnesses):
            count(w["ordinal"], ordinal, "source witness ordinal")
            a, b = w["source_byte_start"], w["source_byte_end"]
            need(type(a) is type(b) is int and 0 <= a <= b <= len(raw), "bounded witness byte range")
            need(raw[a:b] == w["source_lexeme"].encode() and sha(raw[a:b]) == w["source_slice_sha256"], "raw witness bytes/hash")
            count(w["source_utf8_bytes"], b - a, "witness byte count")
            count(w["source_line_start"], raw[:a].count(b"\n") + 1, "source witness start line")
            count(w["source_line_end"], raw[:max(a, b - 1)].count(b"\n") + 1, "source witness end line")
        b = t["source_binding"]
        identity(b, {"ordinal": i, "path": s["path"], "source_sha256": s["source_sha256"],
            "source_bytes": len(raw), "source_git_blob": s["source_git_blob"],
            "manifest_global_ordinal": s["manifest_global_ordinal"], "manifest_record_sha256": s["manifest_record_sha256"],
            "source_asset_case_sha256": digest(s), "source_model_sha256": s["source_model_sha256"],
            "source_witnesses_sha256": s["source_witnesses_sha256"], "source_witness_count": len(witnesses),
            "metadata_sha256": digest(s["source_model"]["metadata"]),
            "config_obligations_sha256": digest(s["source_model"]["config_semantic_obligations"]),
            "prior_ER206_ordinal": s["prior_ER206_ordinal"],
            "prior_ER206_case_canonical_sha256": digest(p)}, "terminal complete source binding")
        need(b["source_all_witness_ordinals"] == list(range(len(witnesses))), "no omitted source witnesses")
        for ref, key in (("full_source_ref", "full_source_witness"), ("frontmatter_ref", "frontmatter_witness"), ("body_ref", "body_witness")):
            r, w = b[ref], witnesses[s[key]]
            identity(r, {"witness": s[key], "byte_start": w["source_byte_start"], "byte_end": w["source_byte_end"],
                "line_start": w["source_line_start"], "line_end": w["source_line_end"], "slice_sha256": w["source_slice_sha256"]}, "projection raw witness reference")
        need(witnesses[s["full_source_witness"]]["source_lexeme"].encode() == raw, "whole original witness")
        model = t["terminal_model"]["whole_scene_model"]
        table = any(e.get("typed_er") for e in model["entities"])
        edges, label = bool(model["relations"]), any(e["label"] for e in model["relations"])
        skip = ([] if table else ["typed_type_name_column_identity:no_source_table"])
        if not edges:
            skip.append("cardinality/label:no_source_relationship")
        if not label:
            skip.append("contact_style:no_source_label_contact")
        semantic_ids = ["entity_complete_header"] + (["typed_type_name_column_identity"] if table else [])
        if edges:
            semantic_ids += ["source_cardinality", "complete_source_role_label"]
        physical_ids = ["border_bold", "inherited_dim", "complete_border_foreground", "owned_interior_background",
                        "unclaimed_apron_ink", "required_projection_notice"]
        if label:
            physical_ids.append("same_relationship_contact_full_style")
        fact = {k: (t if k in ("complete_model_sha256", "terminal_model_sha256", "source_binding_sha256") else s)[k] for k in LOCKS}
        fact.update(source_case_ordinal=i, prior_ER206_ordinal=s["prior_ER206_ordinal"], source_bytes=len(raw),
            source_git_blob=s["source_git_blob"], source_case_sha256=digest(s),
            prior_ER206_case_sha256=digest(p), metadata_sha256=b["metadata_sha256"],
            config_obligations_sha256=b["config_obligations_sha256"], projection_case_sha256=digest(t),
            whole_scene_model_sha256=t["whole_scene_model_sha256"], semantic=len(semantic_ids), physical=len(physical_ids),
            semantic_ids=semantic_ids, physical_ids=physical_ids, skipped=skip, raw_expectation=record["raw"])
        facts.append(fact);lock.extend(s["path"].encode() + b"\0" + s["source_sha256"].encode("ascii") + b"\n")
    need(sha(lock) == LOCK_SHA and source["selection"]["cohort_lock_sha256"] == LOCK_SHA, "exact107 raw cohort lock")
    need(sum(f["semantic"] for f in facts) == 359 and sum(f["physical"] for f in facts) == 734, "source-derived full107 mutant obligations")
    for field in ("source_model", "source_witnesses", "manifest_record"):
        key = {"source_model": "source_models_sha256", "source_witnesses": "source_witnesses_sha256", "manifest_record": "manifest_records_sha256"}[field]
        need(digest([{"path": c["path"], field: c[field]} for c in source["cases"]]) == source["component_hashes"][key], "B1 aggregate: " + key)
    for key, value in {"upstream_evidence_sha256": source["upstream_evidence"],
                       "extra_eight_sources_sha256": source["ceiling_accounting"]["extra_eight_flow_sources"]}.items():
        need(digest(value) == source["component_hashes"][key], "B1 aggregate: " + key)
    preimages = {key + "_sha256": terminal[key] for key in ("additional_upstream_evidence", "palette_source_and_projection", "upstream_evidence_references")}
    for field in ("source_binding", "terminal_model"):
        preimages[field.replace("binding", "bindings").replace("model", "models") + "_sha256"] = [{"path": c["path"], field: c[field]} for c in terminal["cases"]]
    preimages["whole_scene_models_sha256"] = [{"path": c["path"], "whole_scene_model": c["terminal_model"]["whole_scene_model"]} for c in terminal["cases"]]
    preimages["complete_models_sha256"] = {"shared_contract": terminal["shared_contract"], "cases": [{"path": c["path"], "source_binding": c["source_binding"], "terminal_model": c["terminal_model"]} for c in terminal["cases"]]}
    need(set(preimages) == set(terminal["component_hashes"]), "closed seven projection components")
    for key, value in preimages.items():
        need(digest(value) == terminal["component_hashes"][key], "complete projection aggregate: " + key)
    need(digest(terminal["shared_contract"]) == terminal["shared_contract_sha256"], "shared geometry/style/notice/Work contract")
    # Evidence is source-only and independently reviewed, not drawing credit.
    need(presentation is not None and terminal["drawing_credit"] == 0 and terminal["formal_admission"] is False,
         "immutable proposals retain zero self-admission")
    return facts, manifest, files


def authority_gate(root):
    need(set(REVIEWED_HELPER_SHA) == set(HELPERS), "Main must supply exact independently B3-reviewed helper hashes")
    for relative, expected in REVIEWED_HELPER_SHA.items():
        need(type(expected) is str and re.fullmatch(r"[0-9a-f]{64}", expected) is not None and
             sha(file_bytes(root / relative, 524288)) == expected, "reviewed helper drift: " + relative)
    text = file_bytes(root / HELPERS[1], 524288).decode()
    need(re.findall(r'kB3ReviewStatus\s*=\s*"([^"\n]+)"\s*;', text) == [APPROVED], "independent B3 approval required")


def validate_pair(native, official, source):
    expected = {f["path"]: f for f in source}
    need(len(expected) == len(source) == SELECTED, "exact107 unique complete originals")
    pairs = []
    for side, report, keys, common in (("native", native, NATIVE_ROOT, NATIVE_ID),
                                      ("official", official, OFFICIAL_ROOT, OFFICIAL_ID)):
        closed(report, keys, "closed ER107 " + side + " compact root")
        identity(report, common, "reviewed independent " + side + " identity")
        need(type(report["limitations"]) is str and 0 < len(report["limitations"]) <= 4096, "explicit source/foreign scope")
        count(report["local_ready"], SELECTED, "107 local-ready originals; no formal self-admission")
        count(report["errors"], 0, "no unmet complete-original proof")
        if side == "native":
            for key in ("originals", "all_source_captures_completed", "scene_positives_before_aggregate"):
                count(report[key], SELECTED, "Native complete aggregate: " + key)
            need(report["aggregate_binding_after_all_captures"] is True, "Native aggregate after all own captures")
            count(report["independent_stage_oracle_cap"], 512000000, "bounded independent stage oracle")
            semantic_key, physical_key = "semantic_mutants_rejected", "physical_mutants_rejected"
        else:
            identity(report, {"schema": "er107_official_complete_original/v1", "selected": SELECTED,
                "resource_errors": 0, "captured_originals": SELECTED, "captures_per_original": 15,
                "aggregate_after_all_captures": True, "root_components_validated": True,
                "admitted": 0, "base_admitted_originals": BASE,
                "actual_color_cells_file": "mermaid-official-er107.jsonl"}, "Official closed root protocol")
            semantic_key, physical_key = "semantic_mutations_rejected", "physical_mutations_rejected"
        count(report[semantic_key], sum(f["semantic"] for f in source), "complete semantic obligation sum")
        count(report[physical_key], sum(f["physical"] + (side == "official") for f in source), "complete physical obligation sum")
        need(type(report["cases"]) is list and len(report["cases"]) == SELECTED, "all107 compact cases")
        index = {}
        for row in report["cases"]:
            closed(row, NATIVE_CASE if side == "native" else OFFICIAL_CASE, "closed " + side + " case, no frame/unknown fields")
            need(type(row["path"]) is str and row["path"] in expected and row["path"] not in index, "missing/duplicate/wrong107 cohort")
            fact = expected[row["path"]]
            identity(row, {k: fact[k] for k in LOCKS}, "raw107/manifest/Git/Model exact binding")
            identity(row, common, "independently owned compact case")
            need(row["limitations"] == report["limitations"], "case/root foreign limitations")
            for key in ("local_ready", "captures_before_binding"):
                need(row[key] is True, "good-first capture/binding protocol: " + key)
            count(row["captures"], 15, "all15 independently owned captures")
            count(row[semantic_key], fact["semantic"], "source-derived semantic applicability")
            count(row[physical_key], fact["physical"] + (side == "official"), "source-derived physical applicability")
            for key in ("width", "body_height"):
                need(type(row[key]) is int and 1 <= row[key] <= (240 if key == "width" else 1024), "bounded scene dimensions")
            if side == "native":
                for key in ("factory_uncached_accepted", "factory_cache_miss_accepted", "factory_cache_hit_accepted", "all_fifteen_frames_equal_across_modes"):
                    need(row[key] is True, "Native owned factory/cache/full-plane proof")
                count(row["cache_hits"], 1, "Native actual cache hit")
                need(type(row["total_rows"]) is int and row["body_height"] < row["total_rows"] <= 1024, "complete body plus notices")
                need(row["mutants_not_applicable"] == fact["skipped"], "exact no-unclaimed Native controls")
            else:
                identity(row, {k: fact[k] for k in OFFICIAL_EXTRA_LOCKS}, "Official independently source-owned full binding")
                for key in ("bound_after_captures", "full_frames_published", "geometry_and_restoration_good",
                            "uncached_cached_five_planes_equal", "cache_hit_observed", "markdown_dim_color"):
                    need(row[key] is True, "Official owned full-plane/dim protocol: " + key)
                count(row["inherited_other_attributes"], 2, "Official exact inherited dim bit")
                need(row["dim_normalization"] == "exact allocation bit checked before zero-dim complete-scene validation" and
                     row["verification_resource_exhausted"] is False and row["phase"] == "complete", "no resource failure counted as rejection")
                need(type(row["height"]) is int and row["body_height"] < row["height"] <= 1024 and
                     type(row["body_width"]) is int and 1 <= row["body_width"] <= row["width"], "Official complete body/notice dimensions")
                ordinary, geometry = row["narrow_exact_ordinary"], row["narrow_geometry_good"]
                need(type(ordinary) is type(geometry) is bool and ordinary != geometry and ordinary == (row["width"] > 40), "exact narrow source/full geometry XOR")
                need(row["mutations_executed"] == fact["semantic_ids"] + fact["physical_ids"] + ["official_markdown_allocation_dim"] and
                     row["mutations_inapplicable"] == fact["skipped"], "exact executed and inapplicable source-derived controls")
            index[row["path"]] = row
        need(set(index) == set(expected), "no omitted complete-source proof")
        pairs.append(index)
    # Deliberately do NOT compare frames, sentinels, dim, or layout coordinates
    # across consumers. Source/Model ownership and each consumer's own proof are
    # the join law, not Native-derived Official expected pixels.
    need(native["limitations"] == official["limitations"], "same reviewed foreign scope")
    return [{k: f[k] for k in sorted(LOCKS)} for f in source]


def directories_for(value, root=ROOT):
    return BLOCK.directories_for(value, root)


def fingerprint(path, cap=256 * 1024 * 1024):
    if path.name == MARKER and not path.exists():
        need(not path.is_symlink() and not any(p.is_symlink() for p in path.parents), "symlinked optional marker")
        return None
    raw = file_bytes(path, cap)
    return sha(raw), len(raw), path.stat().st_mtime_ns


def baseline_prerequisite(directory, matrix, manifest, root):
    """Revalidate all old14+Block58 compact proofs, never stale join counts alone."""
    inputs = BLOCK.legacy_prerequisite(directory, matrix, manifest, root)
    source, old_manifest, assets = BLOCK.source_inputs(root)
    need(old_manifest == manifest, "same frozen manifest across old688/new107")
    BLOCK.authority_gate(root)
    reports = []
    for side, executable, consumer in BLOCK.CONSUMERS:
        binary = directory / executable
        report = directory / f"mermaid-block58-{side}-verdict.json"
        runtime = [root / p for p in BLOCK.COMMON] + [root / consumer]
        file_bytes(binary, 256 * 1024 * 1024)
        need(os.access(binary, os.X_OK), "old58 executable proof")
        fresh(binary, runtime + assets);fresh(report, [binary, *assets])
        reports.append(load(report, 524288));inputs += [binary, report, *runtime, *assets]
    joined = BLOCK.validate_pair(*reports, source)
    marker_path = directory / BLOCK.MARKER;marker = load(marker_path, 524288)
    expected = {"schema": "block58_dual_consumer_admission/v1", "contract": BLOCK.CONTRACT, **BLOCK.IDENTITY,
        "official_manifest_sha256": MANIFEST_SHA, "base_admitted_originals": 630, "joined_originals": 58,
        "formal_admitted_originals": BASE, "pinned_original_files": ORIGINALS, "foreign_assertions_certified": 0,
        "native_semantic_mutations_rejected": 384, "native_physical_mutations_rejected": 437,
        "official_semantic_mutations_rejected": 384, "official_physical_mutations_rejected": 495, "cases": joined}
    need(marker == matrix[BLOCK.JOIN] == expected, "exact current Block58 marker and matrix binding")
    fresh(marker_path, [directory / f"mermaid-block58-{side}-verdict.json" for side in ("native", "official")])
    identity(matrix[BLOCK.SUMMARY], {"selected": 58, "local_ready": 58, "admitted": 58,
        "join_required": False, "base_admitted_originals": 630, "source_facts_sha256": BLOCK.SOURCE_SHA,
        "terminal_facts_sha256": BLOCK.TERMINAL_SHA, "foreign_assertions_certified": 0, "native_report_input": False}, "unchanged old58 formal summary")
    inputs.append(marker_path)
    return inputs


# Main must confirm this integration API; current Official worker writes only
# its compact sidecar. No flag/env override may invent missing matrix evidence.
MATRIX_API_CONFIRMED = False
SUMMARY_KEYS = {"selected", "local_ready", "admitted", "join_required", "base_admitted_originals",
                "source_facts_sha256", "terminal_facts_sha256", "foreign_assertions_certified", "native_report_input", "prerequisite_join"}
ROW_KEYS = {"contract", "source_case_ordinal", "actual_color_cells_file", "local_ready", "admitted", "join_required",
            "foreign_assertions_certified", "native_report_input", "original_manifest_expectation", "original_foreign_obligations", "actual"}
PRIOR_SELECTED_DEBTS_SHA = "251ac00f32bea5f1e1ece33d7fb0a1f53adf1026cd30b90593251bcbbf96ef2f"


def matrix_check(matrix, official, source, manifest):
    need(MATRIX_API_CONFIRMED, "Main ER107 closed matrix summary/provenance API pending; no join possible")
    need(matrix.get("integrity_errors") == [] and matrix.get("manifest_sha256") == MANIFEST_SHA, "matrix source/integrity")
    need(JOIN not in matrix, "ER107 already joined; baseline must be unjoined688")
    rows = matrix["cases"];paths = {f["path"] for f in source}
    need(type(rows) is list and len(rows) == ORIGINALS + DIAGNOSTICS, "all2738 matrix rows")
    originals = [r for r in rows if r["track"] == "original"]
    bodies = [r for r in rows if r["track"] == "body_diagnostic"]
    need(len(originals) == ORIGINALS and len(bodies) == DIAGNOSTICS, "2145 original /593 diagnostic separation")
    index = {r["fixture"]: r for r in originals}
    need(len(index) == ORIGINALS and set(index) == {f["path"] for f in manifest["fixtures"]}, "unique frozen original inventory")
    for f in manifest["fixtures"]:
        need(index[f["path"]]["source_sha256"] == f["sha256"], "all original raw SHA bindings")
    retained = [r for r in rows if not (r["track"] == "original" and r["fixture"] in paths)]
    need(digest(retained) == RETAINED_ROWS_SHA, "all688 drawing/593 body/unrelated source/error rows frozen")
    old = [{"path": r["fixture"], "ER_native_whole_source": r["ER_native_whole_source"]} for r in originals if r["fixture"] in paths]
    need(digest(old) == PRIOR_SELECTED_DEBTS_SHA, "selected107 frozen ER206 metadata/foreign debts remain exact")
    identity(matrix["original_drawing_coverage"], {"drawn_at_240": BASE, "pinned_original_files": ORIGINALS,
        "original_files_executed": ORIGINALS, "percent_of_pinned_original_files": 100.0 * BASE / ORIGINALS}, "formal688 baseline")
    summary = matrix[SUMMARY];closed(summary, SUMMARY_KEYS, "closed unjoined ER107 summary")
    identity(summary, {"selected": SELECTED, "local_ready": SELECTED, "admitted": 0,
        "join_required": True, "base_admitted_originals": BASE, "source_facts_sha256": SOURCE_SHA,
        "terminal_facts_sha256": TERMINAL_SHA, "foreign_assertions_certified": 0,
        "native_report_input": False, "prerequisite_join": BLOCK.SUMMARY}, "unjoined107 local proof")
    actuals = {r["path"]: r for r in official["cases"]}
    for fact in source:
        row = index[fact["path"]];proof = row[SUMMARY];actual = actuals[fact["path"]]
        closed(proof, ROW_KEYS, "closed independently owned ER107 matrix provenance")
        identity(proof, {"contract": CONTRACT, "source_case_ordinal": fact["source_case_ordinal"],
            "actual_color_cells_file": "mermaid-official-er107.jsonl", "local_ready": True, "admitted": False,
            "join_required": True, "foreign_assertions_certified": 0, "native_report_input": False}, "unjoined Official matrix case")
        need(proof["actual"] == actual and proof["original_manifest_expectation"] == fact["raw_expectation"], "exact Official-owned compact provenance/raw expectation")
        # Existing ER206 debt is independently frozen above, even for selected
        # rows. The new field must explicitly retain, not certify, that debt.
        need(proof["original_foreign_obligations"] == {"status": "retained_not_certified",
            "foreign_assertions_certified": 0, "prior_authority_sha256": PRIOR_SHA,
            "source_model_sha256": fact["source_model_sha256"], "terminal_model_sha256": fact["terminal_model_sha256"]}, "explicit retained107 foreign obligations")
        need(row["errors"] == [] and row["observed"] == {"wide_240": "drawn", "restored_240": "drawn",
            "narrow_40": "source_fallback" if actual["narrow_exact_ordinary"] else "drawn"}, "complete Original observed disposition")
    return index


def marker_for(source):
    return {"schema": "er107_dual_consumer_admission/v1", "contract": CONTRACT,
        "source_facts_sha256": SOURCE_SHA, "terminal_facts_sha256": TERMINAL_SHA,
        "presentation_sha256": PRESENTATION_SHA, "contract_sha256": DESIGN_SHA, "source_lock_sha256": LOCK_SHA,
        "manifest_sha256": MANIFEST_SHA, "prior_authority_sha256": PRIOR_SHA,
        "authority_review_status": APPROVED, "base_admitted_originals": BASE, "joined_originals": SELECTED,
        "formal_admitted_originals": FINAL, "pinned_original_files": ORIGINALS, "foreign_assertions_certified": 0,
        "native_semantic_mutants_rejected": sum(f["semantic"] for f in source),
        "native_physical_mutants_rejected": sum(f["physical"] for f in source),
        "official_semantic_mutations_rejected": sum(f["semantic"] for f in source),
        "official_physical_mutations_rejected": sum(f["physical"] + 1 for f in source),
        "cases": [{k: f[k] for k in sorted(LOCKS)} for f in source]}


def patch_matrix(text, source, marker):
    before = parse(text);spans = BLOCK.json_spans(text);updates = {}
    selected = {f["path"] for f in source}
    for i, row in enumerate(before["cases"]):
        if row["track"] == "original" and row["fixture"] in selected:
            updates[("cases", i, SUMMARY, "admitted")] = True
            updates[("cases", i, SUMMARY, "join_required")] = False
    need(len(updates) == 2 * SELECTED, "only107 complete-original rows eligible")
    updates.update({(SUMMARY, "admitted"): SELECTED, (SUMMARY, "join_required"): False,
        ("original_drawing_coverage", "drawn_at_240"): FINAL,
        ("original_drawing_coverage", "percent_of_pinned_original_files"): 100.0 * FINAL / ORIGINALS})
    need(JOIN not in before, "never duplicate joined107 metadata")
    edits = [(spans[p][0], spans[p][1], json.dumps(v)) for p, v in updates.items()]
    last = max(b for p, (_, b) in spans.items() if len(p) == 1)
    edits.append((last, last, ',\n    "' + JOIN + '": ' + json.dumps(marker, ensure_ascii=False, indent=4)))
    out = text
    for a, b, replacement in sorted(edits, reverse=True):
        out = out[:a] + replacement + out[b:]
    expected = copy.deepcopy(before)
    for path, value in updates.items():
        cursor = expected
        for key in path[:-1]:
            cursor = cursor[key]
        cursor[path[-1]] = value
    expected[JOIN] = marker
    need(parse(out) == expected, "only authorized admission metadata changed")
    after = BLOCK.json_spans(out)
    for i, row in enumerate(before["cases"]):
        if row["track"] != "original" or row["fixture"] not in selected:
            a, b = spans[("cases", i)];c, d = after[("cases", i)]
            need(text[a:b].encode() == out[c:d].encode(), "all nonselected/body row bytes exact")
    return out


def runtime_inputs(root):
    return set(root / p for p in COMMON) | {
        p for p in (root / "src/ui/visual").glob("mermaid*") if p.is_file()}


def input_paths(directories, root):
    paths = set(BLOCK.input_paths(directories, root)) | runtime_inputs(root)
    paths.update(root / p for p in (SOURCE, TERMINAL, PRESENTATION, PRIOR, DESIGN, MANIFEST))
    paths.update(root / p for p in HELPERS)
    paths.update(root / c[2] for c in CONSUMERS)
    paths.update(Path(__file__).with_name(name) for name in (
        "verify_block58_admission.py", "verify_shapes4_admission.py", "verify_mindmap10_admission.py"))
    for fact in pinned(root / SOURCE, SOURCE_SHA)["cases"]:
        paths.add(root / "tests/fixtures/mermaid_official/cases" / fact["path"])
    for directory in directories:
        paths.update(directory / name for name in ("test_mermaid_er107_source107", "test_mermaid_official",
            "mermaid-er107-native-verdict.json", "mermaid-er107-official-verdict.json",
            "mermaid-official-matrix.json", "mermaid-official-matrix.md", MARKER))
    return paths


def verify(directory, source, manifest, assets, root):
    pair, inputs = [], set(assets)
    runtime = runtime_inputs(root)
    for side, executable, consumer in CONSUMERS:
        binary = directory / executable;report = directory / f"mermaid-er107-{side}-verdict.json"
        file_bytes(binary, 256 * 1024 * 1024)
        need(os.access(binary, os.X_OK), "independent executable consumer")
        owned = runtime | {root / consumer} | set(assets)
        fresh(binary, owned)
        value = load(report, 2 * 1024 * 1024);fresh(report, [binary, *assets])
        pair.append(value);inputs.update([binary, report, *owned])
    a, b = (directory / c[1] for c in CONSUMERS)
    need((a.stat().st_dev, a.stat().st_ino) != (b.stat().st_dev, b.stat().st_ino), "distinct independently compiled executables")
    validate_pair(*pair, source)
    path = directory / "mermaid-official-matrix.json";raw = file_bytes(path, 20_000_000)
    fresh(path, [directory / "mermaid-er107-official-verdict.json"])
    matrix = parse(raw);matrix_check(matrix, pair[1], source, manifest)
    inputs.update(baseline_prerequisite(directory, matrix, manifest, root))
    markdown = directory / "mermaid-official-matrix.md";text = file_bytes(markdown, 2_000_000).decode()
    fresh(markdown, [directory / "mermaid-er107-official-verdict.json"])
    for section in ("Mindmap10", "MindmapShapes4", "Block58"):
        need(text.count("\n## " + section + " formal admission\n") == 1, "retained old688 Markdown section")
    need("\n## ER107 formal admission\n" not in text, "unjoined ER107 Markdown")
    marker = marker_for(source)
    text += ("\n## ER107 formal admission\n\n"
        "Both independently owned Debug and Release Native/Official compact proofs pass all107 complete-original "
        "raw source, Git, manifest, source/terminal/whole-scene Model and aggregate bindings. All15 own captures "
        "precede per-case binding; complete aggregate checks follow all107 captures. Native rejects359 semantic "
        "+734 physical mutants; Official rejects359 semantic +841 physical mutants including inherited dim. "
        "The frozen688 original drawings,593 diagnostic rows and unrelated source/error rows are retained. "
        "Formal coverage: **795/2145 (37.06%)**. Foreign browser/RGB/CSS/SVG/font assertions receive zero credit. "
        "This metadata join reads no frames or pixel artifacts and borrows no Native expected art for Official.\n")
    marker_path = directory / MARKER
    outputs = {path: patch_matrix(raw.decode(), source, marker), markdown: text,
               marker_path: json.dumps(marker, ensure_ascii=False, indent=2) + "\n"}
    if marker_path.exists():
        need(file_bytes(marker_path) == outputs[marker_path].encode(), "existing ER107 marker must byte-match fresh prospective admission")
    inputs.update([path, markdown, marker_path])
    return outputs, {p: fingerprint(p) for p in inputs}, pair


def prepare(directories, root=ROOT):
    need(len(directories) == 2 and set(directories) == {root / "build" / p / "tests/ui" for p in ("debug", "release")},
         "both exact repository Debug/Release presets mandatory")
    selected_paths = input_paths(directories, root)
    guards = {p: fingerprint(p) for p in selected_paths}
    authority_gate(root)
    need(MATRIX_API_CONFIRMED, "Main matrix metadata API not yet confirmed")
    source, manifest, assets = source_inputs(root)
    outputs, pairs = {}, []
    for directory in directories:
        new, observed, pair = verify(directory, source, manifest, assets, root)
        need(set(observed) <= set(guards), "untracked verification input")
        need(all(guards[p] == value for p, value in observed.items()), "input changed during validation")
        outputs.update(new);pairs.append(pair)
    for side in (0, 1):
        need(pairs[0][side] == pairs[1][side], "same-consumer Debug/Release exact compact parity")
    need(input_paths(directories, root) == selected_paths, "runtime input inventory drift")
    need(all(fingerprint(p) == value for p, value in guards.items()), "inputs changed after validation")
    return outputs, guards


def publish_all(outputs, guards, directories):
    """Stage six files; markers last. Preserve bytes/mtime on failure or SIGINT.

    Existing optional markers must already be exact through verify(). Hard-link
    backups retain existing markers too. A failed rollback leaves its backup
    available and reports its path instead of deleting recoverable evidence.
    """
    expected = {d / name for d in directories for name in ("mermaid-official-matrix.json", "mermaid-official-matrix.md", MARKER)}
    need(set(outputs) == expected and len(directories) == 2, "complete dual-preset six-file publication")
    markers = sorted(p for p in outputs if p.name == MARKER)
    regular = sorted(set(outputs) - set(markers));staged, backups = {}, {}
    installed, recovery_failed = [], set()
    try:
        need(all(p in guards for p in outputs), "all output/optional marker states guarded")
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
        need(all(fingerprint(p) == old for p, old in guards.items()), "input drift after validation; no publication")
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
                recovery_failed.add(path);failures.append(f"{path}: {error}; backup={backups.get(path)}")
        if failures:
            raise OSError("publication rollback incomplete: " + "; ".join(failures))
        raise
    finally:
        for path in [*staged.values(), *(p for target, p in backups.items() if target not in recovery_failed)]:
            path.unlink(missing_ok=True)


def synthetic_pair(source):
    """Synthetic source-bound metadata, never current runtime verdict input."""
    native, official = [], []
    for f in source:
        common = {k: f[k] for k in LOCKS}
        native.append({**common, **NATIVE_ID, "limitations": "SYNTHETIC; no drawing/foreign proof.",
            "local_ready": True, "captures_before_binding": True, "captures": 15,
            "factory_uncached_accepted": True, "factory_cache_miss_accepted": True, "factory_cache_hit_accepted": True,
            "cache_hits": 1, "width": 41, "body_height": 20, "total_rows": 30,
            "all_fifteen_frames_equal_across_modes": True, "semantic_mutants_rejected": f["semantic"],
            "physical_mutants_rejected": f["physical"], "mutants_not_applicable": list(f["skipped"])})
        official.append({**common, **OFFICIAL_ID, **{k: f[k] for k in OFFICIAL_EXTRA_LOCKS},
            "limitations": "SYNTHETIC; no drawing/foreign proof.", "local_ready": True, "captures": 15,
            "captures_before_binding": True, "bound_after_captures": True, "full_frames_published": True,
            "geometry_and_restoration_good": True, "narrow_exact_ordinary": True, "narrow_geometry_good": False,
            "uncached_cached_five_planes_equal": True, "cache_hit_observed": True, "markdown_dim_color": True,
            "inherited_other_attributes": 2, "dim_normalization": "exact allocation bit checked before zero-dim complete-scene validation",
            "width": 41, "height": 30, "body_width": 40, "body_height": 20,
            "semantic_mutations_rejected": f["semantic"], "physical_mutations_rejected": f["physical"] + 1,
            "verification_resource_exhausted": False, "phase": "complete",
            "mutations_executed": f["semantic_ids"] + f["physical_ids"] + ["official_markdown_allocation_dim"],
            "mutations_inapplicable": list(f["skipped"])})
    return ({**NATIVE_ID, "limitations": "SYNTHETIC; no drawing/foreign proof.", "originals": SELECTED,
        "all_source_captures_completed": SELECTED, "aggregate_binding_after_all_captures": True,
        "local_ready": SELECTED, "scene_positives_before_aggregate": SELECTED, "errors": 0,
        "semantic_mutants_rejected": sum(f["semantic"] for f in source),
        "physical_mutants_rejected": sum(f["physical"] for f in source),
        "independent_stage_oracle_cap": 512000000, "cases": native},
        {**OFFICIAL_ID, "schema": "er107_official_complete_original/v1", "limitations": "SYNTHETIC; no drawing/foreign proof.",
        "selected": SELECTED, "local_ready": SELECTED, "errors": 0, "resource_errors": 0,
        "captured_originals": SELECTED, "captures_per_original": 15, "aggregate_after_all_captures": True,
        "root_components_validated": True, "semantic_mutations_rejected": sum(f["semantic"] for f in source),
        "physical_mutations_rejected": sum(f["physical"] + 1 for f in source), "admitted": 0,
        "base_admitted_originals": BASE, "actual_color_cells_file": "mermaid-official-er107.jsonl", "cases": official})


def synthetic_matrix(source, manifest, official):
    paths = {f["path"] for f in source};facts = {f["path"]: f for f in source}
    actuals = {r["path"]: r for r in official["cases"]}
    matrix = {"manifest_sha256": MANIFEST_SHA, "integrity_errors": [], "cases": [],
        "original_drawing_coverage": {"drawn_at_240": BASE, "pinned_original_files": ORIGINALS,
            "original_files_executed": ORIGINALS, "percent_of_pinned_original_files": 100.0 * BASE / ORIGINALS},
        SUMMARY: {"selected": SELECTED, "local_ready": SELECTED, "admitted": 0, "join_required": True,
            "base_admitted_originals": BASE, "source_facts_sha256": SOURCE_SHA, "terminal_facts_sha256": TERMINAL_SHA,
            "foreign_assertions_certified": 0, "native_report_input": False, "prerequisite_join": BLOCK.SUMMARY}}
    for f in manifest["fixtures"]:
        r = {"fixture": f["path"], "track": "original", "source_sha256": f["sha256"], "retained": "SYNTHETIC old state"}
        matrix["cases"].append(r)
        if f["path"] in paths:
            fact, actual = facts[f["path"]], actuals[f["path"]]
            r.update(errors=[], observed={"wide_240": "drawn", "restored_240": "drawn", "narrow_40": "source_fallback"},
                ER_native_whole_source={"retained": "SYNTHETIC old ER206 source/foreign debt"})
            r[SUMMARY] = {"contract": CONTRACT, "source_case_ordinal": fact["source_case_ordinal"],
                "actual_color_cells_file": "mermaid-official-er107.jsonl", "local_ready": True, "admitted": False,
                "join_required": True, "foreign_assertions_certified": 0, "native_report_input": False,
                "original_manifest_expectation": fact["raw_expectation"], "actual": copy.deepcopy(actual),
                "original_foreign_obligations": {"status": "retained_not_certified", "foreign_assertions_certified": 0,
                    "prior_authority_sha256": PRIOR_SHA, "source_model_sha256": fact["source_model_sha256"],
                    "terminal_model_sha256": fact["terminal_model_sha256"]}}
        if "diagnostic" in f:
            matrix["cases"].append({"fixture": f["path"], "track": "body_diagnostic",
                "source_sha256": f["diagnostic"]["sha256"], "retained": "SYNTHETIC diagnostic, no credit"})
    return matrix


class JoinControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source, cls.manifest, cls.assets = source_inputs(ROOT)

    def setUp(self):
        self.pair = synthetic_pair(self.source)

    def test_all_real_frozen_source_models_and_applicability(self):
        self.assertEqual(len(validate_pair(*self.pair, self.source)), 107)
        self.assertEqual(sum(f["semantic"] for f in self.source), 359)
        self.assertEqual(sum(f["physical"] for f in self.source), 734)
        self.assertEqual(self.pair[1]["physical_mutations_rejected"], 841)

    def test_closed_roots_types_credits_aggregate_and_unreviewed_rejection(self):
        for side in (0, 1):
            for key, value in (("local_ready", True), ("local_ready", 106), ("errors", 1),
                ("drawing_credit", 107), ("foreign_assertions_certified", True), ("native_report_input", True),
                ("formal_admission", True), ("authority_review_status", "UNREVIEWED"), ("frames", [])):
                bad = copy.deepcopy(self.pair);bad[side][key] = value
                with self.subTest(side=side, key=key), self.assertRaises(ValueError):
                    validate_pair(*bad, self.source)
        for side, key in ((0, "aggregate_binding_after_all_captures"), (1, "aggregate_after_all_captures"), (1, "root_components_validated")):
            bad = copy.deepcopy(self.pair);bad[side][key] = False
            with self.subTest(side=side, key=key), self.assertRaises(ValueError):
                validate_pair(*bad, self.source)

    def test_missing_duplicate_wrong_source_and_no_borrowed_frames(self):
        for side in (0, 1):
            mutations = [lambda r: r["cases"].pop(),
                lambda r: r["cases"].__setitem__(1, copy.deepcopy(r["cases"][0])),
                lambda r: r["cases"][0].update(path="er-diagram/not-selected.mmd"),
                lambda r: r["cases"][0].update(captures=14), lambda r: r["cases"][0].update(captures=True),
                lambda r: r["cases"][0].update(captures_before_binding=False),
                lambda r: r["cases"][0].update(local_ready=False), lambda r: r["cases"][0].update(frames=[]),
                lambda r: r["cases"][0].pop("source_model_sha256")]
            for key in LOCKS - {"path"}:
                mutations.append(lambda r, key=key: r["cases"][0].__setitem__(key, "forged"))
            for n, mutate in enumerate(mutations):
                bad = copy.deepcopy(self.pair);mutate(bad[side])
                with self.subTest(side=side, control=n), self.assertRaises(ValueError):
                    validate_pair(*bad, self.source)

    def test_each_source_applicability_and_unclaimed_controls(self):
        for side, physical, skipped in ((0, "physical_mutants_rejected", "mutants_not_applicable"),
                                        (1, "physical_mutations_rejected", "mutations_inapplicable")):
            for row_index in (0, 50, 106):
                for field, value in ((physical, 0), (skipped, [])):
                    bad = copy.deepcopy(self.pair);bad[side]["cases"][row_index][field] = value
                    # Some sources have no skips; mutate those to a bogus skip.
                    if field == skipped and not self.pair[side]["cases"][row_index][field]:
                        bad[side]["cases"][row_index][field] = ["unclaimed"]
                    with self.subTest(side=side, row=row_index, field=field), self.assertRaises(ValueError):
                        validate_pair(*bad, self.source)
        bad = copy.deepcopy(self.pair);bad[1]["cases"][0]["mutations_executed"].append("invented")
        with self.assertRaises(ValueError):
            validate_pair(*bad, self.source)

    def test_independent_consumers_need_not_share_frame_coordinates(self):
        pair = copy.deepcopy(self.pair)
        pair[1]["cases"][0].update(width=42, body_width=41, height=31)
        self.assertEqual(len(validate_pair(*pair, self.source)), 107)

    def test_official_dim_and_resource_failure_cannot_earn_credit(self):
        for key, value in (("markdown_dim_color", False), ("bound_after_captures", False),
            ("full_frames_published", False), ("verification_resource_exhausted", True),
            ("inherited_other_attributes", 0), ("phase", "complete_scene"),
            ("narrow_exact_ordinary", False), ("error", "resource exhausted")):
            bad = copy.deepcopy(self.pair);bad[1]["cases"][0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_pair(*bad, self.source)

    def test_duplicate_json_and_nonfinite_are_rejected(self):
        for raw in (b'{"cases":[],"cases":[]}', b'{"x":NaN}', b'{"x":Infinity}'):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                parse(raw)

    def test_exact_presets_required(self):
        self.assertEqual(len(directories_for("build/debug/tests/ui,build/release/tests/ui")), 2)
        for value in ("build/debug/tests/ui", "build/debug/tests/ui,build/debug/tests/ui",
                      "build/debug/tests/ui,build/asan/tests/ui", "build/debug/tests/ui,../release/tests/ui"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                directories_for(value)

    def test_freshness_regular_file_symlink_future_and_cap(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve();source = root / "source";report = root / "compact"
            source.write_text("source");report.write_text("compact")
            stamp = 1_600_000_000_000_000_000
            os.utime(source, ns=(stamp + 2, stamp + 2));os.utime(report, ns=(stamp + 1, stamp + 1))
            with self.assertRaisesRegex(ValueError, "stale"):
                fresh(report, [source])
            os.utime(report, ns=(stamp + 3, stamp + 3));fresh(report, [source])
            with self.assertRaises(ValueError):
                file_bytes(source, 1)
            link = root / "link";link.symlink_to(source)
            with self.assertRaises(ValueError):
                fingerprint(link)
            os.utime(source, ns=(10**20, 10**20))
            with self.assertRaisesRegex(ValueError, "future"):
                file_bytes(source)

    def test_unapproved_helper_and_pending_matrix_api_fail_closed(self):
        with patch.dict(REVIEWED_HELPER_SHA, {}, clear=True), self.assertRaisesRegex(ValueError, "reviewed"):
            authority_gate(ROOT)
        current_helper_hashes = {relative: sha(file_bytes(ROOT / relative, 524288)) for relative in HELPERS}
        with patch.dict(REVIEWED_HELPER_SHA, current_helper_hashes, clear=True), self.assertRaisesRegex(ValueError, "approval"):
            authority_gate(ROOT)
        with patch.object(sys.modules[__name__], "MATRIX_API_CONFIRMED", False), self.assertRaisesRegex(ValueError, "pending"):
            matrix_check({}, self.pair[1], self.source, self.manifest)

    def matrix_fixture(self):
        matrix = synthetic_matrix(self.source, self.manifest, self.pair[1])
        paths = {f["path"] for f in self.source}
        retained = [r for r in matrix["cases"] if not (r["track"] == "original" and r["fixture"] in paths)]
        old = [{"path": r["fixture"], "ER_native_whole_source": r["ER_native_whole_source"]}
               for r in matrix["cases"] if r["track"] == "original" and r["fixture"] in paths]
        # Test-only source inventory; real pinned hashes cannot be CLI-overridden.
        return matrix, patch.multiple(sys.modules[__name__], MATRIX_API_CONFIRMED=True,
            RETAINED_ROWS_SHA=digest(retained), PRIOR_SELECTED_DEBTS_SHA=digest(old))

    def test_baseline_and_foreign_debt_drift_rejected(self):
        matrix, pins = self.matrix_fixture()
        with pins:
            matrix_check(matrix, self.pair[1], self.source, self.manifest)
            for mutate in (lambda m: m["original_drawing_coverage"].update(drawn_at_240=795),
                           lambda m: m[SUMMARY].update(admitted=107),
                           lambda m: m["cases"][0].update(retained="changed old drawing/source row"),
                           lambda m: m["cases"].pop()):
                bad = copy.deepcopy(matrix);mutate(bad)
                with self.assertRaises(ValueError):
                    matrix_check(bad, self.pair[1], self.source, self.manifest)
            selected = next(r for r in matrix["cases"] if SUMMARY in r)
            selected["ER_native_whole_source"] = {"retained": "forged old debts"}
            with self.assertRaisesRegex(ValueError, "ER206"):
                matrix_check(matrix, self.pair[1], self.source, self.manifest)

    def test_surgical_join_preserves_every_other_row_bytes(self):
        matrix, pins = self.matrix_fixture()
        with pins:
            matrix_check(matrix, self.pair[1], self.source, self.manifest)
            raw = json.dumps(matrix, ensure_ascii=False, indent=2)
            result = patch_matrix(raw, self.source, marker_for(self.source));joined = parse(result)
            self.assertEqual(joined[SUMMARY]["admitted"], 107)
            self.assertEqual(joined["original_drawing_coverage"]["drawn_at_240"], 795)
            self.assertEqual(len(joined["cases"]), 2738)
            with self.assertRaises(ValueError):
                patch_matrix(result, self.source, marker_for(self.source))

    def publication_fixture(self, root, existing):
        dirs = [root / "build" / preset / "tests/ui" for preset in ("debug", "release")]
        outputs = {};before = {}
        for d in dirs:
            d.mkdir(parents=True)
            for name in ("mermaid-official-matrix.json", "mermaid-official-matrix.md", MARKER):
                p = d / name
                if name != MARKER or existing:
                    p.write_text("SYNTHETIC previous " + name)
                    before[p] = fingerprint(p)
                else:
                    before[p] = None
                outputs[p] = "SYNTHETIC prospective " + name
        return dirs, outputs, before

    def test_six_file_publication_and_marker_refresh(self):
        for existing in (False, True):
            with self.subTest(existing=existing), tempfile.TemporaryDirectory() as tmp:
                dirs, outputs, guards = self.publication_fixture(Path(tmp).resolve(), existing)
                publish_all(outputs, guards, dirs)
                for p, text in outputs.items():
                    self.assertEqual(p.read_text(), text)
                self.assertEqual(list(Path(tmp).resolve().rglob("*.join-*")), [])

    def test_atomic_rollback_before_and_after_every_replace_preserves_mtime(self):
        real_replace = os.replace
        for existing in (False, True):
            for after in (False, True):
                for fail_at in range(1, 7):
                    with self.subTest(existing=existing, after=after, fail_at=fail_at), tempfile.TemporaryDirectory() as tmp:
                        root = Path(tmp).resolve();dirs, outputs, guards = self.publication_fixture(root, existing)
                        calls = 0
                        def broken(source, target):
                            nonlocal calls
                            calls += 1
                            if calls == fail_at and not after:
                                raise KeyboardInterrupt("synthetic pre-replace interruption")
                            real_replace(source, target)
                            if calls == fail_at and after:
                                raise KeyboardInterrupt("synthetic post-replace interruption")
                        with patch.object(os, "replace", broken), self.assertRaises(KeyboardInterrupt):
                            publish_all(outputs, guards, dirs)
                        self.assertEqual({p: fingerprint(p) for p in guards}, guards)
                        self.assertEqual(list(root.rglob("*.join-*")), [])

    def test_late_input_drift_prevents_publication_and_keeps_outputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp).resolve();dirs, outputs, guards = self.publication_fixture(root, True)
            p = root / "raw-source";p.write_text("before");guards[p] = fingerprint(p)
            before = {f: fingerprint(f) for f in outputs};p.write_text("changed after validation")
            with self.assertRaisesRegex(ValueError, "drift"):
                publish_all(outputs, guards, dirs)
            self.assertEqual({f: fingerprint(f) for f in outputs}, before)
            self.assertEqual(list(root.rglob("*.join-*")), [])


WRITE_REVIEW_APPROVED = False  # Main/Harvey must review this join before enabling publication.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report-dirs", default="build/debug/tests/ui,build/release/tests/ui")
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        need(not args.write, "self-test cannot publish runtime admission")
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(JoinControls)
        return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
    need(not args.write or WRITE_REVIEW_APPROVED, "independent join implementation review required before --write")
    directories = directories_for(args.report_dirs)
    outputs, guards = prepare(directories)
    if args.write:
        publish_all(outputs, guards, directories)
    print(("Published" if args.write else "Read-only verified") +
          " ER107:107 complete originals, four owned compact proofs, prospective795/2145; foreign credit0; no frames read.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError, TypeError, json.JSONDecodeError) as error:
        print("ER107 admission refused: " + str(error), file=sys.stderr)
        sys.exit(1)
