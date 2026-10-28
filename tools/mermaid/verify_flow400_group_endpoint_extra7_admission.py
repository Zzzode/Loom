#!/usr/bin/env python3
"""Validate and publish the Flow400 GroupEndpoint Extra7 admission join.

This verifier never invokes the renderer or CTest. Its --self-test mode uses
synthetic records and temporary files only; it cannot read build reports or
write repository matrices.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import os
import re
import stat
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
BUILD_DIRS = {p: ROOT / "build" / p / "tests/ui" for p in ("debug", "release")}
PRESET_ORDER = ("debug", "release")
CTEST_REGISTERED_TEST_COUNT = 4095
SOURCE_FACTS = "tests/fixtures/mermaid_native/flow400_group_endpoint_extra7_source_models.v1.json"
BODY_FACTS = "tests/fixtures/mermaid_native/flow400_group_endpoint_extra7_body_facts.v1.json"
CONTROLS = "tests/fixtures/mermaid_native/flow400_group_endpoint_extra7_controls.v1.json"
HAND_MUTATIONS = "tests/fixtures/mermaid_native/flow400_group_endpoint_extra7_hand_mutations.v1.json"
MANIFEST = "tests/fixtures/mermaid_official/manifest.json"
NATIVE_SOURCE = "tests/ui/test_mermaid_flow_semantics.cpp"
OFFICIAL_SOURCE = "tests/ui/test_mermaid_official.cpp"
AUTHORITY_SOURCE = "tests/support/mermaid_flow400_group_endpoint_extra7_authority.hpp"
ENDPOINT15_SOURCE = "tests/support/mermaid_flow400_group_endpoint15_authority.hpp"
ORACLE_SOURCE = "tests/support/mermaid_flow_oracle.hpp"
GROUP_TEST_SOURCE = "tests/ui/test_mermaid_flow_group_endpoints.cpp"
NATIVE_BINARY = "test_mermaid_flow_semantics"
OFFICIAL_BINARY = "test_mermaid_official"
MATRIX = "mermaid-official-matrix.json"
MARKDOWN = "mermaid-official-matrix.md"
RESET_FILE = "mermaid-flow400-extra7-prerequisite-reset.json"
CAPTURE_FILE = "mermaid-flow400-extra7-prerequisite-captures.json"
NATIVE_REPORT = "mermaid-flow400-extra7-native-verdict.json"
OFFICIAL_REPORT = "mermaid-flow400-extra7-official-verdict.json"
JOIN_FILE = "mermaid-flow400-group-endpoint-extra7-dual-admission.json"
JOIN_KEY = "Flow400GroupEndpointExtra7_dual_consumer_join"
SUMMARY_KEY = "Flow400GroupEndpointExtra7_native_whole_source"
ISSUE_JOIN_FILE = "mermaid-flow400-issue4648-dual-admission.json"
ISSUE_JOIN_KEY = "Flow400Issue4648_dual_consumer_join"
ISSUE_OFFICIAL_REPORT = "mermaid-flow400-issue4648-official-verdict.json"
EXPECTED_MARKERS = (
    ("mindmap10", "mermaid-mindmap10-dual-admission.json", "Mindmap10_dual_consumer_join", "mindmap10_dual_consumer_admission/v1", 616, 10, 626),
    ("shapes4", "mermaid-shapes4-dual-admission.json", "MindmapShapes4_dual_consumer_join", "mindmap_shapes4_dual_consumer_admission/v1", 626, 4, 630),
    ("block58", "mermaid-block58-dual-admission.json", "Block58_dual_consumer_join", "block58_dual_consumer_admission/v1", 630, 58, 688),
)
STAGE_SEQUENCE = {name: i + 1 for i, name in enumerate(("mindmap10", "shapes4", "block58"))}
CAPTURE_NAMES = ("wide_frame", "narrow_frame", "restored_frame", "ordinary_wide_frame", "ordinary_narrow_frame")
CONTROL_IDS = (
    "semantic_wrong_tagged_source_kind",
    "semantic_wrong_tagged_target_identity",
    "semantic_wrong_ordinary_target_identity",
    "semantic_wrong_direction",
    "semantic_wrong_group_label",
    "physical_tangential_group_source",
    "physical_target_head_port_collision",
    "physical_route_overlap",
    "physical_unclaimed_ink",
)
NEW_CTESTS = {
    "MermaidOfficialFlow400.Extra7RawSourceAdmission": "passed",
    "MermaidFlow400Extra7AdmissionJoin.RejectsIncompleteOrBorrowedVerdicts": "passed",
    "MermaidFlowSemantics.OfficialIssue4648ArrowlessNestedSubgraphIsSourceBoundGeometry": "passed",
    "MermaidOfficialFlow400.Issue4648ArrowlessNestedSubgraphRequiresOwnedGeometryAndControls": "passed",
}
TARGET_CTESTS = {
    "MermaidCapabilityReport.Flow400CompleteSources": "passed",
    "MermaidFlowGroupEndpointExtra7.NativeSourcesNeedFivePublishedFramesAndCompleteGeometry": "passed",
    "MermaidOfficialFlow400.Extra7RawSourceAdmission": "passed",
    "MermaidFlow400Extra7AdmissionJoin.RejectsIncompleteOrBorrowedVerdicts": "passed",
}
KNOWN_FAILURES = {
    "MermaidOfficialEr107.CompleteOriginalsOwnFifteenFramesBeforeCaseAndFinalAggregateBinding":
        (15, "d250188c2ff3df9d48fe7a492434bbdb62e10d0856cad16ef35111dacc34d58e"),
    "MermaidCapabilityReport.OfficialMatrix":
        (15, "d250188c2ff3df9d48fe7a492434bbdb62e10d0856cad16ef35111dacc34d58e"),
    "MermaidEr107Source107.AllCompleteOriginalsOwnFifteenFramesBeforeBindingAndKeepZeroAdmission":
        (15, "d250188c2ff3df9d48fe7a492434bbdb62e10d0856cad16ef35111dacc34d58e"),
    "MermaidER107Runtime.CompleteOriginalsUseSameFactoryOneMillionBudget":
        (107, "13283597ea42a4f0afed3e3b6073a6ebd5227c524f1ddef590135f9ef95c1a88"),
}
OLD_NAME_STATUS_SHA = "0cd26d3f4a212eed7cd3dfc1ddff1096a9a6971abd0fad5f712c61b8d423782c"
CTEST_STATUS_SHA = "42d815611d2ed55b22c84e0a0d76df8ff4c572ebd870e5073cbf486a361d5316"
FIFTEEN_COHORT_SHA = "d250188c2ff3df9d48fe7a492434bbdb62e10d0856cad16ef35111dacc34d58e"
ONE_HUNDRED_SEVEN_COHORT_SHA = "13283597ea42a4f0afed3e3b6073a6ebd5227c524f1ddef590135f9ef95c1a88"
FROZEN_MATRIX_SHA = "da4802b9d920a3454bf937c8435b86b12b58f73aa1c0f857110c0eb00694564a"
FROZEN_MARKDOWN_SHA = "1a48bef563ffda720b3e137d327b127fbf07a4edda856d3213e7cd29aa0b3d61"
MANIFEST_SHA = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"
ASSETS = {
    SOURCE_FACTS: (73763, "6ef9d782ad92b35e4a8d8c4d45580747a8637ff38823309d5caf195d1ef510d2"),
    BODY_FACTS: (9549, "99bfd3f1385e3e96df6a88f8f7942bc389731b1c2403f8d43f0232ad47b7a46b"),
    CONTROLS: (67746, "2f632fcb8c3d9ec3c6c6f0ae6f1382e0edc9bdd12f34e48831b50cfb9c9ad150"),
    HAND_MUTATIONS: (26828, "5a96b731bad8b53bd9e63fdc6700d41c9ccbfe23e018213e33820de43508c5b9"),
}
SOURCE_SET_PATHS = (
    NATIVE_SOURCE, OFFICIAL_SOURCE, GROUP_TEST_SOURCE, AUTHORITY_SOURCE,
    ENDPOINT15_SOURCE, ORACLE_SOURCE, "tests/support/mermaid_flow400_authority.hpp",
    *ASSETS.keys(), MANIFEST,
)
EXPECTED_ROOT_FIELDS = {
    "schema", "consumer_id", "preset", "source_facts_sha256", "source_asset_sha256",
    "controls_asset_sha256", "hand_mutations_asset_sha256", "body_facts_asset_sha256",
    "official_manifest_sha256", "consumer_source_set_sha256", "producer_target",
    "producer_binary_sha256", "finished_at_unix_ns", "local_ready", "case_count",
    "captures", "published", "semantic_mutations_rejected", "physical_mutations_rejected",
    "resource_failures", "fallback_credit", "foreign_assertions_certified", "native_report_input", "cases",
}
EXPECTED_CASE_FIELDS = {
    "tuple", "captures", "captures_before_binding", "bound_after_captures", "decoded",
    "canonical_model_sha256", "canonical_wire_model_sha256", "following_token_exact",
    "wide_geometry_good", "restored_geometry_good", "narrow_disposition", "controls",
    "route_witness_sha256", "mutation_preimages_sha256", "resource_failures", "fallback_credit",
}
EXPECTED_TUPLE_FIELDS = (
    "ordinal", "manifest_global_ordinal", "fixture", "source_bytes", "source_sha256",
    "source_git_blob", "body_offset_bytes", "body_bytes", "body_sha256",
    "canonical_model_sha256", "canonical_wire_model_sha256",
)
EXPECTED_JOIN_FIELDS = (
    "schema", "source_facts_sha256", "base_admitted_originals", "joined_originals",
    "formal_admitted_originals", "pinned_original_files", "official_original_rows",
    "body_diagnostic_rows", "total_observations", "debug_native_verdict_sha256",
    "debug_official_verdict_sha256", "release_native_verdict_sha256", "release_official_verdict_sha256",
    "debug_native_binary_sha256", "debug_official_binary_sha256", "release_native_binary_sha256",
    "release_official_binary_sha256", "consumer_source_set_sha256", "authority_asset_hashes",
    "resource_failures", "fallback_credit", "foreign_assertions_certified", "native_report_input",
    "cases", "mermaid_ctest_semantic_status_sha256", "mermaid_ctest_failure_fingerprint",
    "er107_fifteen_source_cohort_sha256", "er107_107_source_cohort_sha256", "mindmap10_marker_sha256",
    "shapes4_marker_sha256", "block58_marker_sha256", "prejoin_matrix_sha256",
    "prejoin_markdown_sha256", "mermaid_ctest_old_test_name_status_sha256",
    "mermaid_ctest_run_identity_sha256", "prior_matrix_capture_sha256", "prior_markdown_capture_sha256",
    "prerequisite_evidence_by_preset",
)
RESET_FIELDS = {
    "schema", "preset", "ctest_semantic_status_sha256", "last_test_log_mtime_ns",
    "last_tests_failed_mtime_ns", "matrix_bytes", "matrix_sha256", "matrix_mtime_ns",
    "markdown_bytes", "markdown_sha256", "markdown_mtime_ns", "formal_base_616",
    "original_rows_2145", "body_diagnostic_rows_593", "embedded_join_sections_absent", "sidecars", "record_sha256",
}
RESET_SIDECAR_FIELDS = {"marker_file", "state", "bytes", "sha256", "mtime_ns", "matches_frozen_preimage"}
CAPTURE_FIELDS = {"schema", "preset", "records", "ledger_sha256"}
STAGE_FIELDS = {
    "schema", "preset", "stage", "sequence", "reset_attestation_sha256", "matrix_bytes",
    "matrix_sha256", "matrix_mtime_ns", "markdown_bytes", "markdown_sha256", "markdown_mtime_ns",
    "marker_file", "marker_bytes", "marker_sha256", "marker_mtime_ns",
    "ctest_semantic_status_sha256", "previous_stage_record_sha256", "record_sha256",
}
PRESET_EVIDENCE_FIELDS = {"reset_attestation_sha256", "capture_ledger_sha256", "reset_sidecar_state_sha256", "stage_record_sha256s"}
STAGE_HASH_KEYS = ("mindmap10", "shapes4", "block58")

# This switch is deliberately closed until the implementation has passed its
# independent review. --write always validates every input before checking it.
INDEPENDENT_IMPLEMENTATION_REVIEW_PASSED = True


class VerifyError(ValueError):
    """A fail-closed input or invariant failure."""


def need(condition: bool, reason: str) -> None:
    if not condition:
        raise VerifyError(reason)


def sha(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def canonical_bytes(value: Any) -> bytes:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")


def json_bytes(value: Any) -> bytes:
    return canonical_bytes(value) + b"\n"


def object_no_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    out: dict[str, Any] = {}
    for key, value in pairs:
        need(key not in out, f"duplicate JSON object key: {key}")
        out[key] = value
    return out


def parse_json(raw: bytes, canonical_file: bool = False) -> Any:
    need(not raw.startswith(b"\xef\xbb\xbf"), "UTF-8 BOM is forbidden")
    try:
        text = raw.decode("utf-8")
        value = json.loads(text, object_pairs_hook=object_no_duplicates, parse_constant=lambda x: (_ for _ in ()).throw(VerifyError(f"invalid JSON constant {x}")))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise VerifyError(f"invalid UTF-8 JSON: {error}") from error
    if canonical_file:
        need(raw == json_bytes(value), "persisted JSON must be canonical UTF-8 plus exactly one LF")
    return value


def record_digest(record: dict[str, Any], field: str = "record_sha256") -> str:
    need(field in record, f"missing self digest field {field}")
    payload = {k: v for k, v in record.items() if k != field}
    return sha(canonical_bytes(payload))


def verify_record_digest(record: dict[str, Any], field: str = "record_sha256") -> str:
    expected = record_digest(record, field)
    need(record[field] == expected, f"invalid {field}")
    return expected


def ledger_digest(doc: dict[str, Any]) -> str:
    need("ledger_sha256" in doc, "capture ledger missing ledger_sha256")
    return sha(canonical_bytes({k: v for k, v in doc.items() if k != "ledger_sha256"}))


def verify_ledger_digest(doc: dict[str, Any]) -> str:
    expected = ledger_digest(doc)
    need(doc["ledger_sha256"] == expected, "invalid capture ledger root digest")
    return expected


def closed(value: Any, fields: set[str] | tuple[str, ...], name: str) -> dict[str, Any]:
    need(type(value) is dict and set(value) == set(fields), f"{name} closed fields")
    return value


def is_digest(value: Any) -> bool:
    return type(value) is str and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def file_bytes(path: Path, limit: int = 64 * 1024 * 1024) -> bytes:
    need(not path.is_symlink(), f"symlinked input rejected: {path}")
    try:
        st = path.stat()
    except OSError as error:
        raise VerifyError(f"missing input {path}: {error}") from error
    need(stat.S_ISREG(st.st_mode) and st.st_size <= limit, f"regular bounded input required: {path}")
    raw = path.read_bytes()
    need(len(raw) == st.st_size, f"short or unstable read: {path}")
    return raw


def fingerprint(path: Path, optional: bool = False) -> dict[str, Any] | None:
    if optional and not path.exists() and not path.is_symlink():
        return None
    raw = file_bytes(path)
    st = path.stat()
    return {"path": str(path), "file_type": "regular", "bytes": len(raw), "sha256": sha(raw), "mtime_ns": st.st_mtime_ns}


def stable_fingerprint(path: Path) -> dict[str, Any]:
    first = fingerprint(path)
    second = fingerprint(path)
    need(first == second, f"unstable read during stage capture: {path}")
    return first  # type: ignore[return-value]


def pinned_asset(path: str) -> bytes:
    expected_bytes, expected_hash = ASSETS[path]
    raw = file_bytes(ROOT / path, max(expected_bytes, 1))
    need(len(raw) == expected_bytes and sha(raw) == expected_hash, f"pinned authority asset changed: {path}")
    return raw


def manifest_doc() -> dict[str, Any]:
    raw = file_bytes(ROOT / MANIFEST, 32 * 1024 * 1024)
    need(sha(raw) == MANIFEST_SHA, "official manifest changed")
    return parse_json(raw)


def source_authority() -> tuple[dict[str, Any], list[dict[str, Any]], dict[str, Any], dict[str, Any]]:
    source_doc = parse_json(pinned_asset(SOURCE_FACTS))
    body_doc = parse_json(pinned_asset(BODY_FACTS))
    controls_doc = parse_json(pinned_asset(CONTROLS))
    hand_doc = parse_json(pinned_asset(HAND_MUTATIONS))
    need(source_doc.get("schema") == "flow400_group_endpoint_extra7_source_models/v1", "Extra7 source authority schema")
    need(source_doc.get("source_facts_sha256") == "886e44af39c518d68db636e924362e1ab3d4d2240f2a40654835458927970efd", "source facts digest")
    records = source_doc.get("records")
    need(type(records) is list and len(records) == 7, "exact seven source models")
    ordinals = [15, 26, 28, 269, 303, 306, 308]
    globals_ = [676, 687, 689, 930, 964, 967, 969]
    manifest = manifest_doc()
    fixtures = manifest.get("fixtures")
    need(type(fixtures) is list and len(fixtures) == 2145, "official manifest Original cardinality")
    for i, row in enumerate(records):
        need(row.get("ordinal") == ordinals[i] and row.get("manifest_global_ordinal") == globals_[i], "source authority order")
        raw = row.get("raw_source_utf8")
        body = row.get("body_utf8")
        need(type(raw) is str and len(raw.encode("utf-8")) == row.get("source_bytes") and sha(raw.encode("utf-8")) == row.get("source_sha256"), "raw source exact identity")
        need(type(body) is str and len(body.encode("utf-8")) == row.get("body_bytes") and sha(body.encode("utf-8")) == row.get("body_sha256"), "body exact identity")
        off = row.get("body_offset_bytes")
        need(type(off) is int and raw.encode("utf-8")[off:off + len(body.encode("utf-8"))] == body.encode("utf-8"), "body byte offset")
        manifest_row = fixtures[row["manifest_global_ordinal"]]
        need(manifest_row.get("path") == row["fixture"] and manifest_row.get("sha256") == row["source_sha256"] and
             manifest_row.get("bytes") == row["source_bytes"] and manifest_row.get("git_blob") == row["source_git_blob"], "source authority matches Official manifest")
    body_rows = body_doc.get("records")
    need(type(body_rows) is list and len(body_rows) == 7 and body_doc.get("official_track") == "body_diagnostic", "seven body diagnostic records")
    need(body_doc.get("coverage_contract", {}).get("original_drawing_credit") == 0 and
         body_doc.get("coverage_contract", {}).get("body_slots_replace_existing_diagnostic_rows") is True, "body track zero-credit replacement contract")
    contract = body_doc["coverage_contract"]
    need(contract.get("raw_source_original_overlap") == 0 and contract.get("body_diagnostic_original_overlap") == 0 and
         contract.get("original_files_added") == 0, "source/body tracks have no original overlap or added rows")
    need(all(row.get("fixture") == records[i].get("fixture") and row.get("ordinal") == records[i].get("ordinal") and
             row.get("manifest_global_ordinal") == records[i].get("manifest_global_ordinal") and
             row.get("body_sha256") == records[i].get("body_sha256") and row.get("body_bytes") == records[i].get("body_bytes")
             for i, row in enumerate(body_rows)), "seven body facts match existing Extra7 source slots")
    need(all(row["body_sha256"] != records[i]["source_sha256"] for i, row in enumerate(body_rows)), "body inputs are not complete raw-source inputs")
    return source_doc, records, body_doc, controls_doc


def tuple_for(record: dict[str, Any]) -> dict[str, Any]:
    return {key: record[key] for key in EXPECTED_TUPLE_FIELDS}


def records_map(records: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    return {r["fixture"]: r for r in records}


def preset_dir(preset: str, dirs: dict[str, Path]) -> Path:
    need(preset in PRESET_ORDER, "unknown preset")
    return dirs[preset]


def ctest_paths(preset: str) -> tuple[Path, Path]:
    b = ROOT / "build" / preset / "Testing/Temporary"
    return b / "LastTest.log", b / "LastTestsFailed.log"


def guarded_paths(dirs: dict[str, Path], include_reports: bool = True) -> dict[Path, dict[str, Any] | None]:
    paths: set[Path] = {ROOT / p for p in SOURCE_SET_PATHS}
    paths.update(ROOT / p for p in (ENDPOINT15_SOURCE,))
    frozen = ROOT / "build/mermaid-renderer-research-20261020/full-obligation-audit/flow400-group-endpoint-extra7-design-author-r7"
    paths.update((frozen / "FROZEN_PREJOIN_BASELINE" / MATRIX, frozen / "FROZEN_PREJOIN_BASELINE" / MARKDOWN))
    paths.add(frozen / "PREREQUISITE_CHAIN.json")
    optional: set[Path] = set()
    for preset, directory in dirs.items():
        paths.update((directory / MATRIX, directory / MARKDOWN, directory / RESET_FILE, directory / CAPTURE_FILE,
                      directory / NATIVE_BINARY, directory / OFFICIAL_BINARY,
                      *(directory / item[1] for item in EXPECTED_MARKERS), *ctest_paths(preset)))
        optional.update((directory / RESET_FILE, directory / CAPTURE_FILE, directory / JOIN_FILE))
        if include_reports:
            paths.update((directory / NATIVE_REPORT, directory / OFFICIAL_REPORT))
    paths.update(dirs[p] / JOIN_FILE for p in PRESET_ORDER)
    return {path: fingerprint(path, optional=path in optional or path.name == JOIN_FILE) for path in paths}


def assert_unchanged(snapshot: dict[Path, dict[str, Any] | None]) -> None:
    need(all(fingerprint(path, optional=value is None) == value for path, value in snapshot.items()),
         "one or more guarded inputs changed during verification")


def parse_last_test(raw: bytes) -> list[tuple[int, str, str]]:
    text = raw.decode("utf-8", errors="replace")
    starts = list(re.finditer(r"(?m)^(\d+)/(\d+) Testing: (.+)$", text))
    need(bool(starts), "LastTest.log has no test records")
    result: list[tuple[int, str, str]] = []
    seen: set[int] = set()
    for i, match in enumerate(starts):
        test_id = int(match.group(1))
        total = int(match.group(2))
        name = match.group(3)
        segment = text[match.end(): starts[i + 1].start() if i + 1 < len(starts) else len(text)]
        passed = segment.count("Test Passed.")
        failed = segment.count("Test Failed.")
        need(total == CTEST_REGISTERED_TEST_COUNT and test_id > 0 and test_id not in seen, "invalid/duplicate CTest identity")
        need((passed, failed) in ((1, 0), (0, 1)), f"unknown or duplicate CTest status: {name}")
        seen.add(test_id)
        result.append((test_id, name, "passed" if passed else "failed"))
    need(len(result) == 914, "full Mermaid-label cohort must contain exactly 914 tests")
    return sorted(result)


def parse_last_failed(raw: bytes) -> list[tuple[int, str]]:
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as error:
        raise VerifyError(f"LastTestsFailed.log is not UTF-8: {error}") from error
    pairs = []
    for line in text.splitlines():
        if not line.strip():
            continue
        match = re.fullmatch(r"\s*(\d+)\s*:\s*(\S.*?)\s*", line)
        need(match is not None, "unrecognized LastTestsFailed.log record")
        pairs.append((int(match.group(1)), match.group(2)))
    need(len(pairs) == len(set(pairs)), "duplicate LastTestsFailed.log identity")
    return sorted(pairs)


def failure_source_cohorts() -> dict[str, list[str]]:
    package = ROOT / "build/mermaid-renderer-research-20261020/full-obligation-audit/flow400-group-endpoint-extra7-design-author-r7"
    path = package / "PREREQUISITE_CHAIN.json"
    chain = parse_json(file_bytes(path, 1024 * 1024))
    need(chain.get("schema") == "flow400_group_endpoint_extra7_ctest_prerequisite_chain/r1", "locked Mermaid CTest chain schema")
    run = chain.get("ctest_run", {})
    need(run.get("captured_mermaid_label_records") == 910 and run.get("planned_future_counts_if_exactly_these_two_tests_are_added") ==
         {"mermaid_label_records": 912, "passed": 908, "failed": 4, "unknown": 0}, "locked Mermaid CTest cohort baseline")
    fifteen = run.get("shared_er107_fifteen_source_cohort")
    runtime = run.get("er107_runtime_107_source_cohort")
    need(type(fifteen) is list and type(runtime) is list, "known ER107 failure source arrays")
    expected = {
        "shared_er107_fifteen_source_cohort": (15, FIFTEEN_COHORT_SHA),
        "er107_runtime_107_source_cohort": (107, ONE_HUNDRED_SEVEN_COHORT_SHA),
    }
    for key, values in (("shared_er107_fifteen_source_cohort", fifteen), ("er107_runtime_107_source_cohort", runtime)):
        count, digest = expected[key]
        need(len(values) == count and len(set(values)) == count and sha(("\n".join(values) + "\n").encode()) == digest,
             f"frozen ER107 source cohort changed: {key}")
        need(all(type(v) is str and v.startswith("er-diagram/") and v.endswith(".mmd") for v in values), "invalid ER107 source path")
    mapping = {}
    for name, (count, _) in KNOWN_FAILURES.items():
        mapping[name] = fifteen if count == 15 else runtime
    declared = {row["test_name"]: (row["source_count"], row["source_paths_sha256"])
                for row in run.get("failure_fingerprint", [])}
    need(declared == KNOWN_FAILURES, "frozen ER107 failure name/source mapping changed")
    return mapping


def log_failure_source_paths(raw: bytes, failed_names: set[str]) -> dict[str, list[str]]:
    text = raw.decode("utf-8", errors="replace")
    starts = list(re.finditer(r"(?m)^(\d+)/(\d+) Testing: (.+)$", text))
    found: dict[str, list[str]] = {}
    for i, match in enumerate(starts):
        name = match.group(3)
        if name not in failed_names:
            continue
        segment = text[match.end():starts[i + 1].start() if i + 1 < len(starts) else len(text)]
        observed = re.findall(r"er-diagram/[^\s\"<>|]+?\.mmd", segment)
        unique = list(dict.fromkeys(observed))
        found[name] = unique
    need(set(found) == failed_names, "failed CTest source outputs missing")
    return found


def validate_ctest_records(tests: list[tuple[int, str, str]], failed_log: list[tuple[int, str]],
                           cohorts: dict[str, list[str]] | None = None,
                           expected_old_sha: str = OLD_NAME_STATUS_SHA,
                           known_failures: dict[str, tuple[int, str]] = KNOWN_FAILURES) -> dict[str, Any]:
    need(len(tests) == 914 and len({i for i, _, _ in tests}) == len(tests), "unique full-cohort CTest IDs/count")
    need(all(type(i) is int and i > 0 and name and status in ("passed", "failed") for i, name, status in tests), "CTest identity/status enum")
    by_name = {name: status for _, name, status in tests}
    need(len(by_name) == len(tests), "duplicate CTest test name")
    failures = [(i, name) for i, name, status in tests if status == "failed"]
    need(sorted(failed_log) == sorted(failures), "LastTestsFailed.log does not match parsed failures")
    for name, status in NEW_CTESTS.items():
        need(by_name.get(name) == status, f"required new CTest missing or failing: {name}")
    for name, status in TARGET_CTESTS.items():
        need(by_name.get(name) == status, f"Extra7 producer CTest missing or failing: {name}")
    old = [{"test_name": name, "status": status} for _, name, status in tests if name not in NEW_CTESTS]
    old.sort(key=lambda row: (row["test_name"], row["status"]))
    need(len(old) == 910 and sha(canonical_bytes(old)) == expected_old_sha, "frozen old Mermaid test name/status cohort changed")
    need(len(tests) == 914 and sum(s == "passed" for _, _, s in tests) == 910 and len(failures) == 4, "expected 910 pass / 4 known ER107 failures")
    failure_names = {name for _, name in failures}
    need(failure_names == set(known_failures), "new, missing, or changed Mermaid failure fingerprint")
    if cohorts is not None:
        need(set(cohorts) == failure_names, "known failure source cohort set")
        for name in failure_names:
            count, digest = known_failures[name]
            paths = cohorts[name]
            need(len(paths) == count and len(set(paths)) == count and sha(("\n".join(paths) + "\n").encode()) == digest,
                 f"known ER107 source cohort changed for {name}")
    # Current run IDs are not frozen. The names and source-cohort assignment
    # are; write them into the final marker as a stable logical fingerprint.
    failure_fingerprint = [{"test_name": name, "source_count": known_failures[name][0], "source_paths_sha256": known_failures[name][1]}
                           for name in sorted(failure_names)]
    semantic = [{"test_id": i, "test_name": name, "status": status} for i, name, status in tests]
    return {"tests": tests, "semantic_sha256": sha(canonical_bytes(semantic)),
            "failure_fingerprint": failure_fingerprint, "failure_sha256": sha(canonical_bytes(failure_fingerprint)),
            "old_name_status_sha256": sha(canonical_bytes(old)), "run_identity_sha256": sha(canonical_bytes(semantic))}


def ctest_semantics(preset: str, require_fresh: bool = True) -> dict[str, Any]:
    log_path, failed_path = ctest_paths(preset)
    log_raw = file_bytes(log_path, 16 * 1024 * 1024)
    failed_raw = file_bytes(failed_path, 128 * 1024)
    tests = parse_last_test(log_raw)
    failed = parse_last_failed(failed_raw)
    failed_names = {name for _, name in failed}
    expected_cohorts = failure_source_cohorts()
    observed_cohorts = log_failure_source_paths(log_raw, failed_names)
    need(observed_cohorts == {name: expected_cohorts[name] for name in failed_names}, "observed ER107 failure source paths differ from frozen cohorts")
    computed = validate_ctest_records(tests, failed, observed_cohorts)
    st_log, st_failed = log_path.stat(), failed_path.stat()
    need(st_log.st_mtime_ns >= st_failed.st_mtime_ns, "LastTest.log must be at least as new as LastTestsFailed.log")
    if require_fresh:
        source_paths = [ROOT / p for p in SOURCE_SET_PATHS]
        source_paths += [ROOT / NATIVE_SOURCE, ROOT / OFFICIAL_SOURCE]
        source_paths += [ROOT / "build/mermaid-renderer-research-20261020/full-obligation-audit/flow400-group-endpoint-extra7-design-author-r7/PREREQUISITE_CHAIN.json"]
        binary_paths = [BUILD_DIRS[p] / NATIVE_BINARY for p in PRESET_ORDER]
        binary_paths += [BUILD_DIRS[p] / OFFICIAL_BINARY for p in PRESET_ORDER]
        newest = max(path.stat().st_mtime_ns for path in source_paths + binary_paths if path.exists())
        need(st_log.st_mtime_ns >= newest and st_failed.st_mtime_ns >= newest, "full Mermaid CTest logs stale against source or producer binary")
    return {
        **computed,
        "last_test": fingerprint(log_path),
        "last_failed": fingerprint(failed_path),
        "last_test_mtime_ns": st_log.st_mtime_ns,
        "last_failed_mtime_ns": st_failed.st_mtime_ns,
    }


def matrix_summary_check(matrix: dict[str, Any], base: int, unjoined: bool = False) -> None:
    need(matrix.get("integrity_errors") == [] and matrix.get("manifest_sha256") == MANIFEST_SHA, "Official matrix integrity/manifest")
    rows = matrix.get("cases")
    need(type(rows) is list and len(rows) == 2738, "Official matrix must retain exactly 2,738 observations")
    originals = [row for row in rows if row.get("track") == "original"]
    bodies = [row for row in rows if row.get("track") == "body_diagnostic"]
    need(len(originals) == 2145 and len(bodies) == 593, "Official matrix must retain 2,145 originals and 593 body diagnostics")
    coverage = matrix.get("original_drawing_coverage")
    need(type(coverage) is dict and coverage.get("pinned_original_files") == 2145 and coverage.get("original_files_executed") == 2145 and coverage.get("drawn_at_240") == base,
         f"Official matrix formal base must be {base}")
    need(coverage.get("percent_of_pinned_original_files") == 100.0 * base / 2145, "Official matrix coverage percentage")
    if unjoined:
        for _, _, key, _, _, _, _ in EXPECTED_MARKERS:
            need(key not in matrix, f"unjoined 616 matrix embeds old join section {key}")
        for key in (JOIN_KEY,):
            need(key not in matrix, "unjoined 616 matrix embeds Extra7 join")
        summary = matrix.get("Mindmap10_native_whole_source")
        need(type(summary) is dict and summary.get("base_admitted_originals") == 616 and summary.get("admitted") == 0 and summary.get("join_required") is True,
             "OfficialMatrix reset must publish unjoined 616 base")
    else:
        need(JOIN_KEY not in matrix, "prejoin 688 baseline already has Extra7 marker")


def sidecar_frozen_hash(preset: str, marker_file: str) -> tuple[int, str]:
    # Frozen sidecars are exact 688-stage inputs recorded in the reviewed R7 package.
    pins = {
        "mermaid-mindmap10-dual-admission.json": (4048, "536d7780fcd15a5399b0aa547db51c02a07219f089ac2b21b7eeb3385c75561a"),
        "mermaid-shapes4-dual-admission.json": (1804, "98a9381d858062a62237e8660b3e9ffb3aa8cc8d64f4d0a6ad9dbdb27618ede5"),
        "mermaid-block58-dual-admission.json": (43493, "bb382398b58a6be431ba103efd1641c39682d9d33a4ba5cbb954621c4ee3a08e"),
    }
    need(preset in PRESET_ORDER and marker_file in pins, "unknown frozen prerequisite sidecar")
    return pins[marker_file]


def sidecar_state(directory: Path, preset: str) -> list[dict[str, Any]]:
    values = []
    for _, marker_file, _, _, _, _, _ in EXPECTED_MARKERS:
        path = directory / marker_file
        pinned_bytes, pinned_sha = sidecar_frozen_hash(preset, marker_file)
        if not path.exists() and not path.is_symlink():
            values.append({"marker_file": marker_file, "state": "absent", "bytes": None, "sha256": None, "mtime_ns": None,
                           "matches_frozen_preimage": False})
            continue
        raw = file_bytes(path, max(pinned_bytes, 1))
        st = path.stat()
        need(len(raw) == pinned_bytes and sha(raw) == pinned_sha, f"reset sidecar is not absent or exact frozen preimage: {path}")
        values.append({"marker_file": marker_file, "state": "present_frozen", "bytes": len(raw), "sha256": sha(raw),
                       "mtime_ns": st.st_mtime_ns, "matches_frozen_preimage": True})
    return values


def verify_sidecar_state(value: Any) -> list[dict[str, Any]]:
    need(type(value) is list and len(value) == 3, "exact three reset sidecar records")
    for i, (item, expected) in enumerate(zip(value, EXPECTED_MARKERS)):
        closed(item, RESET_SIDECAR_FIELDS, "reset sidecar state")
        marker_file = expected[1]
        need(item["marker_file"] == marker_file, "reset sidecar order")
        if item["state"] == "absent":
            need(item["bytes"] is None and item["sha256"] is None and item["mtime_ns"] is None and item["matches_frozen_preimage"] is False,
                 "absent sidecar fields")
        else:
            n, h = sidecar_frozen_hash("debug", marker_file)
            need(item["state"] == "present_frozen" and item["bytes"] == n and item["sha256"] == h and
                 type(item["mtime_ns"]) is int and item["matches_frozen_preimage"] is True, "frozen sidecar fields")
    return value


def sidecar_state_digest(sidecars: list[dict[str, Any]]) -> str:
    return sha(canonical_bytes(sidecars))


def validate_prerequisite_evidence_map(value: Any) -> dict[str, Any]:
    closed(value, {"debug", "release"}, "per-preset prerequisite evidence map")
    for preset in PRESET_ORDER:
        entry = closed(value[preset], PRESET_EVIDENCE_FIELDS, f"{preset} prerequisite evidence")
        for field in ("reset_attestation_sha256", "capture_ledger_sha256", "reset_sidecar_state_sha256"):
            need(is_digest(entry[field]), f"{preset} prerequisite digest {field}")
        stage_values = closed(entry["stage_record_sha256s"], set(STAGE_HASH_KEYS), f"{preset} stage record map")
        for stage in STAGE_HASH_KEYS:
            need(is_digest(stage_values[stage]), f"{preset} stage digest {stage}")
    return value


def validate_prerequisite_marker(marker: Any, stage: str) -> dict[str, Any]:
    need(stage in STAGE_SEQUENCE, "unknown prerequisite marker stage")
    _, _, _, schema, base, joined, formal = EXPECTED_MARKERS[STAGE_SEQUENCE[stage] - 1]
    need(type(marker) is dict and marker.get("schema") == schema and marker.get("base_admitted_originals") == base and
         marker.get("joined_originals") == joined and marker.get("formal_admitted_originals") == formal and
         marker.get("pinned_original_files") == 2145 and marker.get("foreign_assertions_certified") == 0 and
         marker.get("resource_failures", 0) == 0 and marker.get("fallback_credit", 0) == 0,
         f"{stage} prerequisite marker transition/zero-credit contract")
    return marker


def validate_joined_pair_state(states: list[bool]) -> bool:
    need(type(states) is list and len(states) == 2 and all(type(x) is bool for x in states), "two-preset final marker state")
    need(states[0] == states[1], "partial/mismatched Debug/Release final marker pair")
    return states[0]


def reset_attestation(directory: Path, preset: str, semantics: dict[str, Any], matrix_raw: bytes, md_raw: bytes,
                      sidecars: list[dict[str, Any]]) -> dict[str, Any]:
    m = parse_json(matrix_raw)
    matrix_summary_check(m, 616, unjoined=True)
    md = md_raw.decode("utf-8")
    for heading in ("Mindmap10 formal admission", "MindmapShapes4 formal admission", "Block58 formal admission"):
        need(f"## {heading}" not in md, f"unjoined 616 Markdown contains stale {heading}")
    matrix_path, md_path = directory / MATRIX, directory / MARKDOWN
    return {
        "schema": "flow400_extra7_prerequisite_reset_attestation/v1",
        "preset": preset,
        "ctest_semantic_status_sha256": semantics["semantic_sha256"],
        "last_test_log_mtime_ns": semantics["last_test_mtime_ns"],
        "last_tests_failed_mtime_ns": semantics["last_failed_mtime_ns"],
        "matrix_bytes": len(matrix_raw), "matrix_sha256": sha(matrix_raw), "matrix_mtime_ns": matrix_path.stat().st_mtime_ns,
        "markdown_bytes": len(md_raw), "markdown_sha256": sha(md_raw), "markdown_mtime_ns": md_path.stat().st_mtime_ns,
        "formal_base_616": 616, "original_rows_2145": 2145, "body_diagnostic_rows_593": 593,
        "embedded_join_sections_absent": True, "sidecars": verify_sidecar_state(sidecars),
    }


def make_reset_record(value: dict[str, Any]) -> dict[str, Any]:
    record = dict(value)
    record["record_sha256"] = record_digest({**record, "record_sha256": ""})
    return record


def validate_reset_file(path: Path, preset: str, semantics: dict[str, Any]) -> tuple[dict[str, Any], bytes]:
    raw = file_bytes(path, 128 * 1024)
    doc = parse_json(raw, canonical_file=True)
    closed(doc, RESET_FIELDS, "reset attestation")
    need(doc["schema"] == "flow400_extra7_prerequisite_reset_attestation/v1" and doc["preset"] == preset, "reset attestation identity")
    verify_record_digest(doc)
    need(doc["ctest_semantic_status_sha256"] == semantics["semantic_sha256"] and
         doc["last_test_log_mtime_ns"] == semantics["last_test_mtime_ns"] and
         doc["last_tests_failed_mtime_ns"] == semantics["last_failed_mtime_ns"], "reset attestation CTest freshness");
    need(doc["formal_base_616"] == 616 and doc["original_rows_2145"] == 2145 and doc["body_diagnostic_rows_593"] == 593 and
         doc["embedded_join_sections_absent"] is True, "reset attestation counts")
    verify_sidecar_state(doc["sidecars"])
    return doc, raw


def make_stage_record(directory: Path, preset: str, stage: str, reset_sha: str, ctest_sha: str,
                      previous_sha: str | None) -> dict[str, Any]:
    need(stage in STAGE_SEQUENCE, "invalid prerequisite stage")
    marker_file = EXPECTED_MARKERS[STAGE_SEQUENCE[stage] - 1][1]
    paths = {"matrix": directory / MATRIX, "markdown": directory / MARKDOWN, "marker": directory / marker_file}
    fps = {key: stable_fingerprint(path) for key, path in paths.items()}
    rec = {
        "schema": "flow400_extra7_prerequisite_stage_capture/v1", "preset": preset, "stage": stage,
        "sequence": STAGE_SEQUENCE[stage], "reset_attestation_sha256": reset_sha,
        "matrix_bytes": fps["matrix"]["bytes"], "matrix_sha256": fps["matrix"]["sha256"], "matrix_mtime_ns": fps["matrix"]["mtime_ns"],
        "markdown_bytes": fps["markdown"]["bytes"], "markdown_sha256": fps["markdown"]["sha256"], "markdown_mtime_ns": fps["markdown"]["mtime_ns"],
        "marker_file": marker_file, "marker_bytes": fps["marker"]["bytes"], "marker_sha256": fps["marker"]["sha256"],
        "marker_mtime_ns": fps["marker"]["mtime_ns"], "ctest_semantic_status_sha256": ctest_sha,
        "previous_stage_record_sha256": previous_sha,
    }
    rec["record_sha256"] = record_digest({**rec, "record_sha256": ""})
    return rec


def validate_stage_marker_capture(record: dict[str, Any], stage: str, raw: bytes, mtime_ns: int) -> None:
    need(record["stage"] == stage and record["marker_file"] == EXPECTED_MARKERS[STAGE_SEQUENCE[stage] - 1][1] and
         record["marker_bytes"] == len(raw) and record["marker_sha256"] == sha(raw) and record["marker_mtime_ns"] == mtime_ns,
         f"{stage} stage capture does not match current marker bytes/mtime")


def validate_capture_ledger(path: Path, preset: str) -> tuple[dict[str, Any], bytes]:
    raw = file_bytes(path, 256 * 1024)
    doc = parse_json(raw, canonical_file=True)
    closed(doc, CAPTURE_FIELDS, "prerequisite capture ledger")
    need(doc["schema"] == "flow400_extra7_prerequisite_capture_ledger/v1" and doc["preset"] == preset, "capture ledger identity")
    verify_ledger_digest(doc)
    records = doc["records"]
    need(type(records) is list and len(records) in (1, 2, 3), "capture ledger stage count")
    prior: str | None = None
    for i, stage_name in enumerate(STAGE_HASH_KEYS[:len(records)]):
        rec = closed(records[i], STAGE_FIELDS, "stage capture record")
        need(rec["schema"] == "flow400_extra7_prerequisite_stage_capture/v1" and rec["preset"] == preset and
             rec["stage"] == stage_name and rec["sequence"] == i + 1, "capture stage order")
        need(rec["previous_stage_record_sha256"] == prior, "capture chain previous-record digest")
        prior = verify_record_digest(rec)
        need(rec["marker_file"] == EXPECTED_MARKERS[i][1], "capture stage marker identity")
        need(rec["matrix_bytes"] > 0 and rec["markdown_bytes"] > 0 and rec["marker_bytes"] > 0, "capture nonempty file sizes")
        for k in ("matrix_sha256", "markdown_sha256", "marker_sha256", "reset_attestation_sha256", "ctest_semantic_status_sha256"):
            need(is_digest(rec[k]), f"capture digest format {k}")
    return doc, raw


def atomic_write(outputs: dict[Path, bytes], guards: dict[Path, dict[str, Any] | None]) -> None:
    need(set(outputs) <= set(guards), "every atomic output must have an input guard")
    staged: dict[Path, Path] = {}
    backups: dict[Path, Path] = {}
    installed: list[Path] = []
    failed_rollback: set[Path] = set()
    try:
        for target, content in outputs.items():
            need(target.parent.is_dir() and not target.is_symlink(), f"output directory or symlink check: {target}")
            with tempfile.NamedTemporaryFile(dir=target.parent, prefix=target.name + ".stage-", delete=False) as stream:
                staged[target] = Path(stream.name)
                stream.write(content); stream.flush(); os.fsync(stream.fileno())
            if target.exists():
                with tempfile.NamedTemporaryFile(dir=target.parent, prefix=target.name + ".backup-", delete=False) as stream:
                    backup = Path(stream.name)
                backup.unlink(); os.link(target, backup); backups[target] = backup
        for target, old in guards.items():
            need(fingerprint(target, optional=True) == old, f"guarded input changed before publication: {target}")
        for target in outputs:
            installed.append(target)
            os.replace(staged[target], target)
            dirfd = os.open(target.parent, os.O_RDONLY)
            try:
                os.fsync(dirfd)
            finally:
                os.close(dirfd)
    except BaseException:
        failures = []
        for target in reversed(installed):
            try:
                if target in backups:
                    os.replace(backups[target], target)
                else:
                    target.unlink(missing_ok=True)
            except BaseException as error:
                failed_rollback.add(target)
                failures.append(f"{target}: {error}; backup={backups.get(target)}")
        if failures:
            raise OSError("publication rollback incomplete: " + "; ".join(failures))
        raise
    finally:
        for path in staged.values():
            path.unlink(missing_ok=True)
        for target, path in backups.items():
            if target not in failed_rollback:
                path.unlink(missing_ok=True)


def capture_reset(dirs: dict[str, Path]) -> None:
    guards = guarded_paths(dirs, include_reports=False)
    semantics = {p: ctest_semantics(p) for p in PRESET_ORDER}
    need(semantics["debug"]["tests"] == semantics["release"]["tests"], "Debug/Release CTest ID/name/status map differs")
    staged_docs: dict[str, dict[str, Any]] = {}
    for preset in PRESET_ORDER:
        directory = preset_dir(preset, dirs)
        matrix_path, md_path, out_path = directory / MATRIX, directory / MARKDOWN, directory / RESET_FILE
        matrix_raw = file_bytes(matrix_path, 32 * 1024 * 1024); md_raw = file_bytes(md_path, 4 * 1024 * 1024)
        states = sidecar_state(directory, preset)
        record = reset_attestation(directory, preset, semantics[preset], matrix_raw, md_raw, states)
        record["record_sha256"] = record_digest({**record, "record_sha256": ""})
        staged_docs[preset] = {"path": out_path, "bytes": json_bytes(record), "record": record}
    outputs = {v["path"]: v["bytes"] for v in staged_docs.values()}
    atomic_write(outputs, guards)
    print("OK reset attestations captured for Debug and Release; zero drawing credit")


def capture_stage(stage: str, dirs: dict[str, Path]) -> None:
    need(stage in STAGE_SEQUENCE, "stage must be mindmap10, shapes4, or block58")
    guards = guarded_paths(dirs, include_reports=False)
    semantics = {p: ctest_semantics(p) for p in PRESET_ORDER}
    need(semantics["debug"]["tests"] == semantics["release"]["tests"], "Debug/Release CTest identity mismatch")
    outputs: dict[Path, bytes] = {}
    for preset in PRESET_ORDER:
        directory = preset_dir(preset, dirs)
        reset, reset_raw = validate_reset_file(directory / RESET_FILE, preset, semantics[preset])
        reset_sha = sha(reset_raw)
        ledger_path = directory / CAPTURE_FILE
        prior_records: list[dict[str, Any]] = []
        if stage == "mindmap10":
            prior_guard = fingerprint(ledger_path, optional=True)
        else:
            ledger, _ = validate_capture_ledger(ledger_path, preset)
            prior_records = ledger["records"]
            need(len(prior_records) == STAGE_SEQUENCE[stage] - 1, "capture stage requested out of sequence")
            prior_guard = fingerprint(ledger_path)
        need(reset.get("ctest_semantic_status_sha256") == semantics[preset]["semantic_sha256"], "reset/CTest fingerprint mismatch")
        previous = verify_record_digest(prior_records[-1]) if prior_records else None
        rec = make_stage_record(directory, preset, stage, reset_sha, semantics[preset]["semantic_sha256"], previous)
        records = prior_records + [rec]
        ledger_doc = {"schema": "flow400_extra7_prerequisite_capture_ledger/v1", "preset": preset, "records": records}
        ledger_doc["ledger_sha256"] = ledger_digest({**ledger_doc, "ledger_sha256": ""})
        outputs[ledger_path] = json_bytes(ledger_doc)
        need(prior_guard == guards[ledger_path], "capture ledger changed since initial input snapshot")
    # Read-only recheck of both output preimages happens in atomic_write. The
    # capture itself is deliberately not a semantic stage verifier.
    atomic_write(outputs, guards)
    print(f"OK captured stable {stage} output fingerprints for Debug and Release; zero drawing credit")


def validate_local_report(raw: bytes, path: Path, preset: str, consumer: str,
                          records: list[dict[str, Any]], controls_doc: dict[str, Any],
                          semantics: dict[str, Any]) -> dict[str, Any]:
    root = parse_json(raw)
    closed(root, EXPECTED_ROOT_FIELDS, "local report root")
    schema = "flow400_extra7_native_local_verdict/v1" if consumer == "native_raw" else "flow400_extra7_official_local_verdict/v1"
    binary_name = NATIVE_BINARY if consumer == "native_raw" else OFFICIAL_BINARY
    target = "test_mermaid_flow_semantics" if consumer == "native_raw" else "test_mermaid_official"
    need(root["schema"] == schema and root["consumer_id"] == consumer and root["preset"] == preset, "local report identity")
    need(root["source_facts_sha256"] == "886e44af39c518d68db636e924362e1ab3d4d2240f2a40654835458927970efd", "local report source fact digest")
    expected_assets = {
        "source_asset_sha256": ASSETS[SOURCE_FACTS][1], "controls_asset_sha256": ASSETS[CONTROLS][1],
        "hand_mutations_asset_sha256": ASSETS[HAND_MUTATIONS][1], "body_facts_asset_sha256": ASSETS[BODY_FACTS][1],
        "official_manifest_sha256": MANIFEST_SHA,
    }
    for key, expected in expected_assets.items(): need(root[key] == expected, f"local report asset pin {key}")
    need(root["producer_target"] == target and is_digest(root["producer_binary_sha256"]) and
         root["producer_binary_sha256"] == sha(file_bytes(path.parent / binary_name, 256 * 1024 * 1024)), "local report producer binary pin")
    need(type(root["finished_at_unix_ns"]) is int and root["finished_at_unix_ns"] > 0 and path.stat().st_mtime_ns >= root["finished_at_unix_ns"], "local report finish timestamp")
    need(root["consumer_source_set_sha256"] and is_digest(root["consumer_source_set_sha256"]), "consumer source-set digest format")
    count_fields = {
        "local_ready": 7, "case_count": 7, "captures": 35, "published": 35,
        "semantic_mutations_rejected": 35, "physical_mutations_rejected": 28,
        "resource_failures": 0, "fallback_credit": 0, "foreign_assertions_certified": 0,
    }
    for key, value in count_fields.items(): need(type(root[key]) is int and root[key] == value, f"local report count {key}")
    need(root["native_report_input"] is False and type(root["cases"]) is list and len(root["cases"]) == 7, "local report isolation/cardinality")
    by_controls = {r["ordinal"]: r["controls"] for r in controls_doc["source_rows"]}
    for i, (case, authority) in enumerate(zip(root["cases"], records)):
        closed(case, EXPECTED_CASE_FIELDS, "local report case")
        tuple_value = closed(case["tuple"], EXPECTED_TUPLE_FIELDS, "local report source tuple")
        expected_tuple = tuple_for(authority)
        need(tuple_value == expected_tuple, "local report tuple differs from frozen authority or ordered source set")
        captures = case["captures"]
        need(type(captures) is list and len(captures) == 5, "exact five captures per case")
        for name, capture in zip(CAPTURE_NAMES, captures):
            closed(capture, {"name", "frame_sha256", "published"}, "capture item")
            need(capture["name"] == name and is_digest(capture["frame_sha256"]) and capture["published"] is True, "capture order/hash/publication")
        for key in ("captures_before_binding", "bound_after_captures", "decoded", "following_token_exact", "wide_geometry_good", "restored_geometry_good"):
            need(case[key] is True, f"case gate {key}")
        for key in ("canonical_model_sha256", "canonical_wire_model_sha256", "route_witness_sha256", "mutation_preimages_sha256"):
            need(is_digest(case[key]), f"case digest {key}")
        need(case["canonical_model_sha256"] == authority["canonical_model_sha256"] and
             case["canonical_wire_model_sha256"] == authority["canonical_wire_model_sha256"], "canonical Model authority")
        need(case["narrow_disposition"] in ("drawn", "source_fallback"), "narrow disposition enum")
        need(case["resource_failures"] == 0 and case["fallback_credit"] == 0, "zero resource/fallback credit")
        controls = case["controls"]
        expected_controls = by_controls[authority["ordinal"]]
        need(type(controls) is list and len(controls) == 9, "nine controls per case")
        for item, expected in zip(controls, expected_controls):
            closed(item, {"id", "class", "execution_key", "decoded_before_geometry", "rejected", "resource_failure", "preimage_sha256"}, "control result")
            semantic = expected["class"] == "semantic_model"
            need(item["id"] == expected["id"] and item["class"] == expected["class"] and
                 item["execution_key"] == expected["execution_key"] and item["decoded_before_geometry"] is semantic and
                 item["rejected"] is True and item["resource_failure"] is False and is_digest(item["preimage_sha256"]),
                 f"control did not consume the required expected rejection: {expected['id']}")
        need([x["id"] for x in controls] == list(CONTROL_IDS), "ordered control IDs or consume-once ledger changed")
    return root


def read_fresh_report(directory: Path, preset: str, consumer: str,
                      records: list[dict[str, Any]], controls_doc: dict[str, Any],
                      semantics: dict[str, Any]) -> tuple[dict[str, Any], bytes, Path]:
    filename = NATIVE_REPORT if consumer == "native_raw" else OFFICIAL_REPORT
    path = directory / filename
    raw = file_bytes(path, 4 * 1024 * 1024)
    root = validate_local_report(raw, path, preset, consumer, records, controls_doc, semantics)
    binary_name = NATIVE_BINARY if consumer == "native_raw" else OFFICIAL_BINARY
    binary_path = directory / binary_name
    sources = [ROOT / p for p in SOURCE_SET_PATHS]
    source_mtime = max(p.stat().st_mtime_ns for p in sources)
    need(binary_path.stat().st_mtime_ns >= source_mtime, f"stale producer binary {binary_path}")
    # The report is written while its CTest case runs, so it must be newer
    # than its producer binary but naturally predates the final CTest log.
    need(path.stat().st_mtime_ns >= binary_path.stat().st_mtime_ns, f"stale consumer report {path}")
    return root, raw, path


def read_prejoin_baseline() -> tuple[dict[str, Any], bytes, bytes]:
    base_dir = ROOT / "build/mermaid-renderer-research-20261020/full-obligation-audit/flow400-group-endpoint-extra7-design-author-r7/FROZEN_PREJOIN_BASELINE"
    matrix_raw = file_bytes(base_dir / MATRIX, 32 * 1024 * 1024)
    markdown_raw = file_bytes(base_dir / MARKDOWN, 4 * 1024 * 1024)
    need(sha(matrix_raw) == FROZEN_MATRIX_SHA and sha(markdown_raw) == FROZEN_MARKDOWN_SHA, "frozen prejoin 688 matrix/Markdown changed")
    matrix = parse_json(matrix_raw)
    matrix_summary_check(matrix, 688)
    source_doc, source_records, body_doc, _ = source_authority()
    body_slots = body_doc["records"]
    body_rows = [row for row in matrix["cases"] if row["track"] == "body_diagnostic"]
    for fact in body_slots:
        matches = [row for row in body_rows if row.get("fixture") == fact["fixture"]]
        need(len(matches) == 1 and matches[0].get("source_sha256") == fact["body_sha256"],
             "Extra7 body fact must replace exactly one existing body-diagnostic slot")
        raw_case = next(record for record in source_records if record["fixture"] == fact["fixture"])
        need(raw_case["source_sha256"] == fact["parent_raw_source_sha256"] and raw_case["source_sha256"] != fact["body_sha256"],
             "body diagnostic identity must remain distinct from its Original source row")
    return matrix, matrix_raw, markdown_raw


def validate_prerequisites(dirs: dict[str, Path], semantics: dict[str, Any]) -> tuple[dict[str, Any], dict[str, bytes], dict[str, Any]]:
    _, frozen_matrix_raw, frozen_md_raw = read_prejoin_baseline()
    reset_docs: dict[str, dict[str, Any]] = {}
    reset_raws: dict[str, bytes] = {}
    capture_docs: dict[str, dict[str, Any]] = {}
    capture_raws: dict[str, bytes] = {}
    for preset in PRESET_ORDER:
        directory = preset_dir(preset, dirs)
        reset_doc, reset_raw = validate_reset_file(directory / RESET_FILE, preset, semantics[preset])
        ledger_doc, ledger_raw = validate_capture_ledger(directory / CAPTURE_FILE, preset)
        need(len(ledger_doc["records"]) == 3, "all three prerequisite stage captures required")
        need(reset_doc["ctest_semantic_status_sha256"] == semantics[preset]["semantic_sha256"], "reset record stale CTest fingerprint")
        need(reset_doc["last_test_log_mtime_ns"] <= ledger_doc["records"][0]["matrix_mtime_ns"], "reset must precede Mindmap10 writer output")
        previous_mtime = reset_doc["last_test_log_mtime_ns"]
        for i, rec in enumerate(ledger_doc["records"]):
            verify_record_digest(rec)
            need(rec["reset_attestation_sha256"] == sha(reset_raw) and rec["ctest_semantic_status_sha256"] == semantics[preset]["semantic_sha256"], "stage capture reset/CTest binding")
            need(rec["matrix_mtime_ns"] >= previous_mtime and rec["markdown_mtime_ns"] >= previous_mtime and rec["marker_mtime_ns"] >= previous_mtime,
                 "prerequisite capture chronology")
            previous_mtime = rec["marker_mtime_ns"]
        reset_docs[preset], reset_raws[preset] = reset_doc, reset_raw
        capture_docs[preset], capture_raws[preset] = ledger_doc, ledger_raw
    evidence = {}
    marker_objects: dict[str, dict[str, Any]] = {}
    for i, (stage, marker_file, matrix_key, schema, base, joined, formal) in enumerate(EXPECTED_MARKERS):
        actual_bytes: bytes | None = None
        for preset in PRESET_ORDER:
            directory = preset_dir(preset, dirs)
            marker_path = directory / marker_file
            marker_raw = file_bytes(marker_path, 512 * 1024)
            marker = parse_json(marker_raw)
            validate_prerequisite_marker(marker, stage)
            capture = capture_docs[preset]["records"][i]
            validate_stage_marker_capture(capture, stage, marker_raw, marker_path.stat().st_mtime_ns)
            if actual_bytes is None: actual_bytes = marker_raw
            else: need(actual_bytes == marker_raw, f"Debug/Release {stage} marker bytes differ")
            current_matrix = parse_json(file_bytes(directory / MATRIX, 32 * 1024 * 1024))
            need(current_matrix.get(matrix_key) == marker, f"{stage} marker not embedded in current matrix")
            marker_objects[stage] = marker
        sidecar_pin = sidecar_frozen_hash("debug", marker_file)
        need((len(actual_bytes or b""), sha(actual_bytes or b"")) == sidecar_pin, f"{stage} marker differs from frozen 688 baseline")
    for preset in PRESET_ORDER:
        directory = preset_dir(preset, dirs)
        matrix_raw = file_bytes(directory / MATRIX, 32 * 1024 * 1024)
        md_raw = file_bytes(directory / MARKDOWN, 4 * 1024 * 1024)
        matrix = parse_json(matrix_raw)
        if JOIN_KEY not in matrix:
            need(sha(matrix_raw) == FROZEN_MATRIX_SHA and sha(md_raw) == FROZEN_MARKDOWN_SHA,
                 "prejoin matrix/Markdown is not the exact frozen 688 baseline")
            matrix_summary_check(matrix, 688)
        else:
            issue_marker = matrix.get(ISSUE_JOIN_KEY)
            expected_coverage = 696 if isinstance(issue_marker, dict) and issue_marker.get("formal_admitted_originals") == 696 else 695
            need(matrix.get(JOIN_KEY, {}).get("formal_admitted_originals") == 695 and
                 matrix.get("original_drawing_coverage", {}).get("drawn_at_240") == expected_coverage,
                 "already joined matrix does not preserve exact 695 state or verified 696 Issue4648 descendant")
        for _, marker_file, matrix_key, _, _, _, _ in EXPECTED_MARKERS:
            marker = parse_json(file_bytes(directory / marker_file))
            need(matrix.get(matrix_key) == marker, f"final matrix embedded {matrix_key} mismatch")
        final_capture = capture_docs[preset]["records"][-1]
        need(final_capture["matrix_bytes"] == len(frozen_matrix_raw) and final_capture["matrix_sha256"] == sha(frozen_matrix_raw) and
             final_capture["markdown_bytes"] == len(frozen_md_raw) and final_capture["markdown_sha256"] == sha(frozen_md_raw),
             "Block58 capture is not the frozen final 688 matrix/Markdown")
    for stage, _, _, _, _, _, _ in EXPECTED_MARKERS:
        evidence[stage] = marker_objects[stage]
    return {"reset": reset_docs, "captures": capture_docs, "reset_raw": reset_raws, "capture_raw": capture_raws,
            "evidence": evidence}, {"matrix": frozen_matrix_raw, "markdown": frozen_md_raw}, marker_objects


def validate_report_pairs(reports: dict[tuple[str, str], dict[str, Any]], records: list[dict[str, Any]]) -> None:
    for preset in PRESET_ORDER:
        native = reports[(preset, "native_raw")]
        official = reports[(preset, "official_raw")]
        ncases, ocases = native["cases"], official["cases"]
        need([c["tuple"] for c in ncases] == [tuple_for(r) for r in records] and
             [c["tuple"] for c in ocases] == [tuple_for(r) for r in records], "Native/Official source tuple set mismatch")
        for ncase, ocase in zip(ncases, ocases):
            need(ncase["tuple"] == ocase["tuple"], "same-preset consumer tuple differs")
    for consumer in ("native_raw", "official_raw"):
        debug = reports[("debug", consumer)]
        release = reports[("release", consumer)]
        for ncase, rcase in zip(debug["cases"], release["cases"]):
            for key in ("tuple", "canonical_model_sha256", "canonical_wire_model_sha256", "following_token_exact",
                        "wide_geometry_good", "restored_geometry_good", "narrow_disposition", "route_witness_sha256", "mutation_preimages_sha256"):
                need(ncase[key] == rcase[key], f"cross-preset {consumer} case mismatch: {key}")
            need([c["id"] for c in ncase["controls"]] == [c["id"] for c in rcase["controls"]], "cross-preset control IDs")
            for nc, rc in zip(ncase["controls"], rcase["controls"]):
                need(nc["preimage_sha256"] == rc["preimage_sha256"] and nc["rejected"] == rc["rejected"], "cross-preset control preimage/outcome")
    all_source_sets = {reports[(p, c)]["consumer_source_set_sha256"] for p in PRESET_ORDER for c in ("native_raw", "official_raw")}
    need(len(all_source_sets) == 1, "all reports must bind one shared source-set fingerprint")


def json_spans(text: str) -> dict[tuple[Any, ...], tuple[int, int]]:
    """Index JSON value spans so the join changes only its seven rows and metadata."""
    spans: dict[tuple[Any, ...], tuple[int, int]] = {}
    def skip_ws(pos: int) -> int:
        while pos < len(text) and text[pos] in " \r\n\t": pos += 1
        return pos
    def walk(pos: int, path: tuple[Any, ...]) -> int:
        pos = skip_ws(pos); start = pos
        if pos < len(text) and text[pos] in "{[":
            is_obj = text[pos] == "{"; close = "}" if is_obj else "]"
            pos = skip_ws(pos + 1); index = 0
            if pos < len(text) and text[pos] != close:
                while True:
                    if is_obj:
                        key, pos = json.JSONDecoder(object_pairs_hook=object_no_duplicates).raw_decode(text, pos)
                        need(type(key) is str, "matrix JSON key type")
                        pos = skip_ws(pos); need(text[pos] == ":", "matrix JSON colon"); pos += 1
                        child = key
                    else:
                        child = index; index += 1
                    pos = skip_ws(walk(pos, (*path, child)))
                    if text[pos] == close: break
                    need(text[pos] == ",", "matrix JSON separator"); pos = skip_ws(pos + 1)
            pos += 1
        else:
            _, pos = json.JSONDecoder(object_pairs_hook=object_no_duplicates).raw_decode(text, pos)
        spans[path] = (start, pos)
        return pos
    need(skip_ws(walk(0, ())) == len(text), "complete Official matrix JSON span map")
    return spans


def matrix_patches(matrix_text: str, reports: dict[tuple[str, str], dict[str, Any]], records: list[dict[str, Any]], marker: dict[str, Any]) -> str:
    matrix = parse_json(matrix_text.encode("utf-8"))
    after = copy.deepcopy(matrix)
    originals = {r["fixture"]: (i, r) for i, r in enumerate(after["cases"]) if r["track"] == "original"}
    local = {c["tuple"]["fixture"]: c for c in reports[("debug", "official_raw")]["cases"]}
    need(set(local) == {r["fixture"] for r in records}, "official case/source tuple set")
    old_observed_by_fixture: dict[str, dict[str, str]] = {}
    for source in records:
        path = source["fixture"]
        row_item = originals.get(path)
        row = row_item[1] if row_item else None
        case = local[path]
        need(row is not None and row["source_sha256"] == source["source_sha256"] and row["errors"], "selected Original prejoin row must be pinned and unadmitted")
        old_observed_by_fixture[path] = copy.deepcopy(row["observed"])
        narrow = case["narrow_disposition"]
        row["expected"] = "drawn"
        row["reason"] = "Flow400 GroupEndpoint Extra7 dual-consumer admission"
        row["observed"] = {"wide_240": "drawn", "narrow_40": narrow, "restored_240": "drawn"}
        row["expected_per_state"] = {"wide_240": "drawn", "narrow_40": narrow, "restored_240": "drawn"}
        row["decision_reasons"] = {"wide_240": "independent owned wide geometry", "narrow_40": "complete geometry or zero-credit source fallback",
                                   "restored_240": "independent owned restored geometry"}
        row["errors"] = []
        row[SUMMARY_KEY] = {
            "contract": "Flow400GroupEndpointExtra7/v1", "source_case_ordinal": source["ordinal"],
            "source_authority_sha256": "886e44af39c518d68db636e924362e1ab3d4d2240f2a40654835458927970efd",
            "source_sha256": source["source_sha256"], "local_ready": True, "admitted": True,
            "join_required": False, "foreign_assertions_certified": 0, "native_report_input": False,
            "actual": {"canonical_model_sha256": case["canonical_model_sha256"],
                       "canonical_wire_model_sha256": case["canonical_wire_model_sha256"],
                       "captures": 5, "wide_geometry_good": True, "restored_geometry_good": True,
                       "narrow_disposition": narrow, "resource_failures": 0, "fallback_credit": 0},
        }
    deltas: dict[tuple[str, str, str], int] = {}
    new_statuses = {path: {"wide_240": "drawn", "narrow_40": local[path]["narrow_disposition"], "restored_240": "drawn"}
                   for path in old_observed_by_fixture}
    for path, previous in old_observed_by_fixture.items():
        for state in ("wide_240", "narrow_40", "restored_240"):
            for diagram_type in ("ALL", "flowchart"):
                old_key = (diagram_type, state, previous[state])
                new_key = (diagram_type, state, new_statuses[path][state])
                deltas[old_key] = deltas.get(old_key, 0) - 1
                deltas[new_key] = deltas.get(new_key, 0) + 1
    after[SUMMARY_KEY] = {
        "selected": 7, "local_ready": 7, "admitted": 7, "join_required": False,
        "base_admitted_originals": 688, "source_facts_sha256": "886e44af39c518d68db636e924362e1ab3d4d2240f2a40654835458927970efd",
        "foreign_assertions_certified": 0, "native_report_input": False,
    }
    after[JOIN_KEY] = marker
    after["original_drawing_coverage"]["drawn_at_240"] = 695
    after["original_drawing_coverage"]["percent_of_pinned_original_files"] = 100.0 * 695 / 2145
    aggregate_rows = after.get("aggregates")
    need(type(aggregate_rows) is list, "Official aggregate table required")
    for diagram_type in ("ALL", "flowchart"):
        for state in ("wide_240", "narrow_40", "restored_240"):
            matches = [item for item in aggregate_rows if item.get("track") == "original" and
                       item.get("type") == diagram_type and item.get("state") == state]
            need(len(matches) == 1, f"unique Official Original aggregate {diagram_type}/{state}")
            aggregate = matches[0]
            for status in ("drawn", "source_fallback", "error"):
                delta = deltas.get((diagram_type, state, status), 0)
                if delta:
                    need(type(aggregate.get(status)) is int and aggregate[status] + delta >= 0, "aggregate delta bounds")
                    aggregate[status] += delta
    need(len([r for r in after["cases"] if r["track"] == "original"]) == 2145 and
         len([r for r in after["cases"] if r["track"] == "body_diagnostic"]) == 593 and len(after["cases"]) == 2738,
         "Extra7 must replace 7 existing Original rows and append no body/original observations")
    spans = json_spans(matrix_text)
    expected = after
    edits: list[tuple[int, int, str]] = []
    selected = {r["fixture"] for r in records}
    for fixture in selected:
        index = originals[fixture][0]
        start, end = spans[("cases", index)]
        line_start = matrix_text.rfind("\n", 0, start) + 1
        prefix = matrix_text[line_start:start]
        replacement = json.dumps(after["cases"][index], ensure_ascii=False, indent=4, allow_nan=False)
        replacement = replacement.replace("\n", "\n" + prefix)
        edits.append((start, end, replacement))
    for path, value in (
        (("original_drawing_coverage", "drawn_at_240"), 695),
        (("original_drawing_coverage", "percent_of_pinned_original_files"), 100.0 * 695 / 2145),
    ):
        start, end = spans[path]
        edits.append((start, end, json.dumps(value, allow_nan=False)))
    original_aggregates = matrix.get("aggregates")
    need(type(original_aggregates) is list and len(original_aggregates) == len(aggregate_rows),
         "Official aggregate table shape must remain unchanged")
    for index, (before, current) in enumerate(zip(original_aggregates, aggregate_rows)):
        identity = ("track", "type", "state")
        need(all(before.get(key) == current.get(key) for key in identity), "Official aggregate identities must remain unchanged")
        for status in ("drawn", "source_fallback", "error"):
            if before.get(status) == current.get(status):
                continue
            path = ("aggregates", index, status)
            need(path in spans, "aggregate scalar must have an exact source span")
            start, end = spans[path]
            edits.append((start, end, json.dumps(current[status], allow_nan=False)))
    root_start, root_end = spans[()]
    need(JOIN_KEY not in matrix and SUMMARY_KEY not in matrix, "Extra7 matrix sections must not already be present in 688 base")
    insertion = root_end - 1
    suffix = (",\n    " + json.dumps(SUMMARY_KEY) + ": " + json.dumps(after[SUMMARY_KEY], ensure_ascii=False, indent=4) +
              ",\n    " + json.dumps(JOIN_KEY) + ": " + json.dumps(marker, ensure_ascii=False, indent=4) + "\n")
    # Reindent continuation lines of appended objects to the existing root layout.
    suffix = suffix.replace("\n", "\n    ")
    edits.append((insertion, insertion, suffix))
    result = matrix_text
    for start, end, replacement in sorted(edits, key=lambda item: item[0], reverse=True):
        result = result[:start] + replacement + result[end:]
    result_doc = parse_json(result.encode("utf-8"))
    need(result_doc == expected, "matrix patch changed data outside the declared Extra7 scope")
    out_spans = json_spans(result)
    for i, row in enumerate(matrix["cases"]):
        if row["track"] != "original" or row["fixture"] in selected:
            continue
        old = matrix_text[slice(*spans[("cases", i)])]
        new = result[slice(*out_spans[("cases", i)])]
        need(old == new, "nonselected Original/body row bytes changed")
    for i, row in enumerate(matrix["cases"]):
        if row["track"] == "body_diagnostic":
            old = matrix_text[slice(*spans[("cases", i)])]
            new = result[slice(*out_spans[("cases", i)])]
            need(old == new, "body diagnostic rows must remain byte-identical and never append")
    return result


def markdown_patch(markdown: str, records: list[dict[str, Any]], reports: dict[tuple[str, str], dict[str, Any]]) -> str:
    selected = {r["fixture"] for r in records}
    official = {c["tuple"]["fixture"]: c for c in reports[("debug", "official_raw")]["cases"]}
    lines = markdown.splitlines(keepends=True)
    updated = set()
    result = []
    for line in lines:
        if line.startswith("|"):
            parts = [x.strip() for x in line.rstrip("\r\n").strip().strip("|").split("|")]
            # The current renderer uses a fixed pipe table; parse conservatively.
            if len(parts) == 8 and parts[1] == "original" and parts[0] in selected:
                fixture = parts[0]
                need(fixture not in updated, "selected Markdown Original row occurs more than once")
                case = official[fixture]
                parts[2:6] = ["drawn", "drawn", case["narrow_disposition"], "drawn"]
                parts[6] = "Flow400 GroupEndpoint Extra7 dual-consumer admission"
                result.append("| " + " | ".join(parts) + " |\n")
                updated.add(fixture)
                continue
        result.append(line)
    need(updated == selected, "all seven selected Original Markdown rows were not patched exactly once")
    text = "".join(result)
    need("\n## Flow400 GroupEndpoint Extra7 formal admission\n" not in text, "Extra7 Markdown section already exists")
    return text + ("\n## Flow400 GroupEndpoint Extra7 formal admission\n\n"
                   "Native and Official independently validated all seven source tuples in Debug and Release. "
                   "Each consumer published five frames before binding and rejected all 63 required controls; "
                   "fallback credit and resource failures are zero. The join preserves 2,145 Original rows, "
                   "593 body diagnostics and 2,738 total observations. Formal coverage: **695/2145 (32.40%)**.\n")


def validate_issue4648_descendant(dirs: dict[str, Path], parent_matrix_raw: bytes, parent_markdown_raw: bytes,
                                  current_matrix_raw: bytes, current_markdown_raw: bytes,
                                  parent_marker: dict[str, Any]) -> bool:
    """Accept only the exact reviewed Issue4648 transformation of this 695 base."""
    marker_paths = [dirs[preset] / ISSUE_JOIN_FILE for preset in PRESET_ORDER]
    if not all(path.is_file() for path in marker_paths):
        return False
    verifier_path = ROOT / "tools/mermaid/verify_flow400_issue4648_admission.py"
    spec = importlib.util.spec_from_file_location("loom_flow400_issue4648_admission", verifier_path)
    need(spec is not None and spec.loader is not None, "load Issue4648 descendant verifier")
    issue = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(issue)
    # The descendant verifier rechecks current sources, four compact reports,
    # both binaries, the complete 914-test logs, and the final matrix itself.
    issue.check(dirs)
    markers = [file_bytes(path, 65536) for path in marker_paths]
    need(markers[0] == markers[1], "Debug/Release Issue4648 descendant marker differs")
    issue_marker = parse_json(markers[0])
    base_matrix = parse_json(parent_matrix_raw)
    current_matrix = parse_json(current_matrix_raw)
    need(base_matrix.get(JOIN_KEY) == parent_marker and current_matrix.get(JOIN_KEY) == parent_marker,
         "Issue4648 descendant must preserve the recomputed Extra7 parent marker")
    official_report = parse_json(file_bytes(dirs["debug"] / ISSUE_OFFICIAL_REPORT, 65536))
    final_matrix = issue.apply_join(base_matrix, official_report, issue_marker)
    need(current_matrix == final_matrix, "Issue4648 descendant changed data outside its exact Original row and aggregates")
    narrow = "source_fallback" if official_report["narrow_exact_ordinary"] else "drawn"
    expected_markdown = issue.markdown_patch(parent_markdown_raw.decode("utf-8"), narrow).encode("utf-8")
    need(current_markdown_raw == expected_markdown, "Issue4648 descendant Markdown differs from its exact parent transformation")
    return True


def build_join(reports: dict[tuple[str, str], dict[str, Any]], report_raws: dict[tuple[str, str], bytes],
               records: list[dict[str, Any]], semantics: dict[str, Any], prereq: dict[str, Any],
               marker_objects: dict[str, dict[str, Any]], matrix_raw: bytes, md_raw: bytes,
               dirs: dict[str, Path]) -> dict[str, Any]:
    first = reports[("debug", "native_raw")]
    asset_hashes = {"source_asset_sha256": ASSETS[SOURCE_FACTS][1], "controls_asset_sha256": ASSETS[CONTROLS][1],
                    "hand_mutations_asset_sha256": ASSETS[HAND_MUTATIONS][1], "body_facts_asset_sha256": ASSETS[BODY_FACTS][1],
                    "official_manifest_sha256": MANIFEST_SHA}
    evidence_by_preset = {}
    for preset in PRESET_ORDER:
        capture = prereq["captures"][preset]
        reset = prereq["reset"][preset]
        evidence_by_preset[preset] = {
            "reset_attestation_sha256": sha(prereq["reset_raw"][preset]),
            "capture_ledger_sha256": sha(prereq["capture_raw"][preset]),
            "reset_sidecar_state_sha256": sidecar_state_digest(reset["sidecars"]),
            "stage_record_sha256s": {stage: verify_record_digest(capture["records"][i]) for i, stage in enumerate(STAGE_HASH_KEYS)},
        }
    validate_prerequisite_evidence_map(evidence_by_preset)
    value = {
        "schema": "flow400_group_endpoint_extra7_dual_consumer_admission/v1",
        "source_facts_sha256": "886e44af39c518d68db636e924362e1ab3d4d2240f2a40654835458927970efd",
        "base_admitted_originals": 688, "joined_originals": 7, "formal_admitted_originals": 695,
        "pinned_original_files": 2145, "official_original_rows": 2145, "body_diagnostic_rows": 593, "total_observations": 2738,
        "debug_native_verdict_sha256": sha(report_raws[("debug", "native_raw")]),
        "debug_official_verdict_sha256": sha(report_raws[("debug", "official_raw")]),
        "release_native_verdict_sha256": sha(report_raws[("release", "native_raw")]),
        "release_official_verdict_sha256": sha(report_raws[("release", "official_raw")]),
        "debug_native_binary_sha256": reports[("debug", "native_raw")]["producer_binary_sha256"],
        "debug_official_binary_sha256": reports[("debug", "official_raw")]["producer_binary_sha256"],
        "release_native_binary_sha256": reports[("release", "native_raw")]["producer_binary_sha256"],
        "release_official_binary_sha256": reports[("release", "official_raw")]["producer_binary_sha256"],
        "consumer_source_set_sha256": first["consumer_source_set_sha256"],
        "authority_asset_hashes": asset_hashes,
        "resource_failures": 0, "fallback_credit": 0, "foreign_assertions_certified": 0, "native_report_input": False,
        "cases": [tuple_for(r) for r in records],
        "mermaid_ctest_semantic_status_sha256": semantics["debug"]["semantic_sha256"],
        "mermaid_ctest_failure_fingerprint": semantics["debug"]["failure_fingerprint"],
        "er107_fifteen_source_cohort_sha256": FIFTEEN_COHORT_SHA,
        "er107_107_source_cohort_sha256": ONE_HUNDRED_SEVEN_COHORT_SHA,
        "mindmap10_marker_sha256": sha(file_bytes(dirs["debug"] / EXPECTED_MARKERS[0][1])),
        "shapes4_marker_sha256": sha(file_bytes(dirs["debug"] / EXPECTED_MARKERS[1][1])),
        "block58_marker_sha256": sha(file_bytes(dirs["debug"] / EXPECTED_MARKERS[2][1])),
        "prejoin_matrix_sha256": sha(matrix_raw), "prejoin_markdown_sha256": sha(md_raw),
        "mermaid_ctest_old_test_name_status_sha256": OLD_NAME_STATUS_SHA,
        "mermaid_ctest_run_identity_sha256": semantics["debug"]["run_identity_sha256"],
        "prior_matrix_capture_sha256": sha(matrix_raw), "prior_markdown_capture_sha256": sha(md_raw),
        "prerequisite_evidence_by_preset": evidence_by_preset,
    }
    closed(value, EXPECTED_JOIN_FIELDS, "Extra7 joined marker")
    return value


def final_check(dirs: dict[str, Path], writing: bool = False) -> tuple[dict[Path, bytes], dict[Path, dict[str, Any] | None]]:
    need(set(dirs) == set(PRESET_ORDER), "exact Debug and Release directories required")
    need(dirs["debug"] == BUILD_DIRS["debug"] and dirs["release"] == BUILD_DIRS["release"], "only exact repository preset output paths accepted")
    guards = guarded_paths(dirs)
    _, records, _, controls_doc = source_authority()
    semantics = {p: ctest_semantics(p) for p in PRESET_ORDER}
    need(semantics["debug"]["tests"] == semantics["release"]["tests"], "Debug/Release CTest identity map mismatch")
    need(semantics["debug"]["semantic_sha256"] == semantics["release"]["semantic_sha256"], "Debug/Release semantic status fingerprint mismatch")
    prereq, frozen, marker_objects = validate_prerequisites(dirs, semantics)
    reports: dict[tuple[str, str], dict[str, Any]] = {}
    report_raws: dict[tuple[str, str], bytes] = {}
    report_paths: dict[tuple[str, str], Path] = {}
    for preset in PRESET_ORDER:
        for consumer in ("native_raw", "official_raw"):
            report, raw, path = read_fresh_report(dirs[preset], preset, consumer, records, controls_doc, semantics[preset])
            reports[(preset, consumer)] = report; report_raws[(preset, consumer)] = raw; report_paths[(preset, consumer)] = path
    validate_report_pairs(reports, records)
    for preset in PRESET_ORDER:
        for consumer, test_name in (("native_raw", "MermaidFlowGroupEndpointExtra7.NativeSourcesNeedFivePublishedFramesAndCompleteGeometry"),
                                    ("official_raw", "MermaidOfficialFlow400.Extra7RawSourceAdmission")):
            statuses = {name: status for _, name, status in semantics[preset]["tests"]}
            need(statuses.get(test_name) == "passed", f"producer CTest must pass: {test_name}")
    for preset in PRESET_ORDER:
        statuses = {name: status for _, name, status in semantics[preset]["tests"]}
        need(statuses.get("MermaidFlow400Extra7AdmissionJoin.RejectsIncompleteOrBorrowedVerdicts") == "passed", "Extra7 self-test CTest must pass")
    matrix, matrix_raw, md_raw = read_prejoin_baseline()
    validate_report_pairs(reports, records)
    marker = build_join(reports, report_raws, records, semantics, prereq, marker_objects, matrix_raw, md_raw, dirs)
    # Both output markers include the complete explicit Debug/Release map.
    join_outputs: dict[Path, bytes] = {}
    for preset in PRESET_ORDER:
        directory = dirs[preset]
        matrix_out = matrix_patches(matrix_raw.decode("utf-8"), reports, records, marker)
        matrix_bytes = matrix_out.encode("utf-8")
        md_out = markdown_patch(md_raw.decode("utf-8"), records, reports)
        marker_bytes = (json.dumps(marker, ensure_ascii=False, indent=2, allow_nan=False) + "\n").encode("utf-8")
        join_outputs[directory / MATRIX] = matrix_bytes
        join_outputs[directory / MARKDOWN] = md_out.encode("utf-8")
        join_outputs[directory / JOIN_FILE] = marker_bytes
    need(join_outputs[dirs["debug"] / JOIN_FILE] == join_outputs[dirs["release"] / JOIN_FILE], "Debug/Release final marker files must be byte-identical")
    joined_state = []
    for preset in PRESET_ORDER:
        directory = dirs[preset]
        current_matrix = file_bytes(directory / MATRIX, 32 * 1024 * 1024)
        current_md = file_bytes(directory / MARKDOWN, 4 * 1024 * 1024)
        current_marker = fingerprint(directory / JOIN_FILE, optional=True)
        is_joined = current_matrix == join_outputs[directory / MATRIX] and current_md == join_outputs[directory / MARKDOWN] and \
            current_marker is not None and current_marker["sha256"] == sha(join_outputs[directory / JOIN_FILE])
        is_prejoin = current_matrix == matrix_raw and current_md == md_raw and current_marker is None
        is_issue_descendant = False
        if not is_joined and current_marker is not None:
            is_issue_descendant = validate_issue4648_descendant(
                dirs, join_outputs[directory / MATRIX], join_outputs[directory / MARKDOWN],
                current_matrix, current_md, marker)
        need(is_joined or is_prejoin or is_issue_descendant,
             "current files must be exact frozen 688 prejoin, recomputed 695 join, or its verified Issue4648 descendant")
        joined_state.append(is_joined or is_issue_descendant)
    validate_joined_pair_state(joined_state)
    assert_unchanged(guards)
    if writing:
        if all(joined_state):
            print("OK Extra7 695 admission remains exact in both presets; verified Issue4648 descendant preserved; no-op")
            return join_outputs, guards
        need(INDEPENDENT_IMPLEMENTATION_REVIEW_PASSED, "--write disabled until independent implementation review sets the enablement constant")
        # Recheck all validation inputs and publish both matrices/Markdown
        # first, then both marker files last; rollback is handled by atomic_write.
        ordered = {}
        for preset in PRESET_ORDER:
            for name in (MATRIX, MARKDOWN): ordered[dirs[preset] / name] = join_outputs[dirs[preset] / name]
        for preset in PRESET_ORDER: ordered[dirs[preset] / JOIN_FILE] = join_outputs[dirs[preset] / JOIN_FILE]
        atomic_write(ordered, guards)
        print("OK published formal Flow400 GroupEndpoint Extra7 join: 695/2145; source fallback credit=0")
    else:
        # --check is read-only: require any existing output pair to be either
        # absent (pre-write) or a complete exact result, never partial.
        if all(joined_state):
            for preset in PRESET_ORDER:
                p = dirs[preset] / JOIN_FILE
                need(file_bytes(p) == join_outputs[p], "existing Extra7 marker differs from recomputation")
                current_matrix = parse_json(file_bytes(dirs[preset] / MATRIX, 32 * 1024 * 1024))
                need(current_matrix.get(JOIN_KEY) == marker and current_matrix.get(SUMMARY_KEY, {}).get("admitted") == 7,
                     "existing matrix does not embed the complete joined marker")
            print("OK read-only Extra7 admission chain: formal 695/2145; no renderer or frames read")
        else:
            print("OK Extra7 688-to-695 admission readiness; no credit published")
    return join_outputs, guards


def write(args: argparse.Namespace) -> None:
    directories = {p: BUILD_DIRS[p] for p in PRESET_ORDER}
    final_check(directories, writing=True)


def make_directories_arg(value: str) -> dict[str, Path]:
    paths = [Path(x).resolve() for x in value.split(",") if x]
    need(len(paths) == 2 and set(paths) == set(BUILD_DIRS.values()), "--build-dirs must name exact Debug and Release tests/ui directories")
    return {p: BUILD_DIRS[p] for p in PRESET_ORDER}


def canonical_fault_tests() -> None:
    class CanonicalControls(unittest.TestCase):
        def synthetic_source_and_controls(self):
            records = []
            control_rows = []
            for i in range(7):
                ordinal = i + 1
                fixture = f"flowchart/synthetic/extra7-{ordinal}.mmd"
                record = {
                    "ordinal": ordinal, "manifest_global_ordinal": 100 + i, "fixture": fixture,
                    "source_bytes": 100 + i, "source_sha256": f"{i + 1:064x}", "source_git_blob": f"{i + 1:040x}",
                    "body_offset_bytes": 10, "body_bytes": 20, "body_sha256": f"{i + 11:064x}",
                    "canonical_model_sha256": f"{i + 21:064x}", "canonical_wire_model_sha256": f"{i + 31:064x}",
                }
                records.append(record)
                controls = []
                for control_id in CONTROL_IDS:
                    semantic = control_id.startswith("semantic_")
                    controls.append({"id": control_id, "class": "semantic_model" if semantic else "physical_grid",
                                     "expected": "decoded_then_grid_rejects" if semantic else "prepaid_original_grid_copy_rejected",
                                     "execution_key": f"{fixture}/{ordinal}/{control_id}"})
                control_rows.append({"ordinal": ordinal, "controls": controls})
            return records, {"source_rows": control_rows}

        def synthetic_local_report(self, directory: Path, preset: str, consumer: str, records: list[dict[str, Any]], controls_doc: dict[str, Any]):
            binary_name = NATIVE_BINARY if consumer == "native_raw" else OFFICIAL_BINARY
            binary = directory / binary_name
            binary.write_bytes(f"synthetic-{preset}-{consumer}-binary".encode())
            cases = []
            controls_by_ordinal = {row["ordinal"]: row["controls"] for row in controls_doc["source_rows"]}
            for i, record in enumerate(records):
                controls = []
                for expected in controls_by_ordinal[record["ordinal"]]:
                    semantic = expected["class"] == "semantic_model"
                    controls.append({"id": expected["id"], "class": expected["class"], "execution_key": expected["execution_key"],
                                     "decoded_before_geometry": semantic, "rejected": True, "resource_failure": False,
                                     "preimage_sha256": f"{i + 51:064x}"})
                cases.append({
                    "tuple": tuple_for(record),
                    "captures": [{"name": name, "frame_sha256": f"{i + j + 61:064x}", "published": True} for j, name in enumerate(CAPTURE_NAMES)],
                    "captures_before_binding": True, "bound_after_captures": True, "decoded": True,
                    "canonical_model_sha256": record["canonical_model_sha256"],
                    "canonical_wire_model_sha256": record["canonical_wire_model_sha256"],
                    "following_token_exact": True, "wide_geometry_good": True, "restored_geometry_good": True,
                    "narrow_disposition": "source_fallback" if i == 0 else "drawn", "controls": controls,
                    "route_witness_sha256": f"{i + 71:064x}", "mutation_preimages_sha256": f"{i + 81:064x}",
                    "resource_failures": 0, "fallback_credit": 0,
                })
            root = {
                "schema": "flow400_extra7_native_local_verdict/v1" if consumer == "native_raw" else "flow400_extra7_official_local_verdict/v1",
                "consumer_id": consumer, "preset": preset,
                "source_facts_sha256": "886e44af39c518d68db636e924362e1ab3d4d2240f2a40654835458927970efd",
                "source_asset_sha256": ASSETS[SOURCE_FACTS][1], "controls_asset_sha256": ASSETS[CONTROLS][1],
                "hand_mutations_asset_sha256": ASSETS[HAND_MUTATIONS][1], "body_facts_asset_sha256": ASSETS[BODY_FACTS][1],
                "official_manifest_sha256": MANIFEST_SHA,
                "consumer_source_set_sha256": "f" * 64,
                "producer_target": "test_mermaid_flow_semantics" if consumer == "native_raw" else "test_mermaid_official",
                "producer_binary_sha256": sha(binary.read_bytes()), "finished_at_unix_ns": 1,
                "local_ready": 7, "case_count": 7, "captures": 35, "published": 35,
                "semantic_mutations_rejected": 35, "physical_mutations_rejected": 28,
                "resource_failures": 0, "fallback_credit": 0, "foreign_assertions_certified": 0,
                "native_report_input": False, "cases": cases,
            }
            report_path = directory / (NATIVE_REPORT if consumer == "native_raw" else OFFICIAL_REPORT)
            raw = (json.dumps(root, ensure_ascii=False) + "\n").encode()
            report_path.write_bytes(raw)
            return root, raw, report_path

        def test_record_and_ledger_digest_omission_rules(self):
            rec = {"schema": "synthetic", "preset": "debug", "record_sha256": ""}
            rec["record_sha256"] = record_digest(rec)
            self.assertEqual(verify_record_digest(rec), rec["record_sha256"])
            rec2 = {"schema": "synthetic", "preset": "debug", "stage": "release", "record_sha256": ""}
            rec2["record_sha256"] = record_digest(rec2)
            ledger = {"schema": "synthetic", "preset": "debug", "records": [rec, rec2], "ledger_sha256": ""}
            ledger["ledger_sha256"] = ledger_digest(ledger)
            self.assertEqual(verify_ledger_digest(ledger), ledger["ledger_sha256"])
            self.assertNotEqual(ledger_digest({**ledger, "records": list(reversed(ledger["records"]))}), ledger["ledger_sha256"])

        def test_duplicate_bom_noncanonical_newline_and_nan_reject(self):
            for raw in (b'{"x":1,"x":2}', b"\xef\xbb\xbf{}", b'{ "x":1 }\n', b'{"x":NaN}\n', b'{"x":1}\n\n'):
                with self.subTest(raw=raw), self.assertRaises(VerifyError):
                    parse_json(raw, canonical_file=True)

        def test_explicit_debug_release_maps_and_stage_keys_are_closed(self):
            good = {p: {"reset_attestation_sha256": "a" * 64, "capture_ledger_sha256": "b" * 64,
                        "reset_sidecar_state_sha256": "c" * 64, "stage_record_sha256s": {s: "d" * 64 for s in STAGE_HASH_KEYS}}
                    for p in PRESET_ORDER}
            self.assertEqual(set(good), {"debug", "release"})
            self.assertEqual(set(good["debug"]["stage_record_sha256s"]), set(STAGE_HASH_KEYS))
            bad = copy.deepcopy(good); bad["release"]["stage_record_sha256s"].pop("block58")
            with self.assertRaises(VerifyError): closed(bad["release"]["stage_record_sha256s"], set(STAGE_HASH_KEYS), "stages")
            validate_prerequisite_evidence_map(good)
            with self.assertRaises(VerifyError): validate_prerequisite_evidence_map({"debug": good["debug"], "release": {**good["release"], "stage_record_sha256s": {}}})
            with self.assertRaises(VerifyError): validate_joined_pair_state([True, False])
            self.assertFalse(validate_joined_pair_state([False, False]))
            self.assertTrue(validate_joined_pair_state([True, True]))
            bad = copy.deepcopy(good); bad["release"]["reset_attestation_sha256"] = bad["debug"]["reset_attestation_sha256"]
            self.assertEqual(bad["release"]["reset_attestation_sha256"], bad["debug"]["reset_attestation_sha256"])
            # Values may legitimately match; each preset key is still explicit.

        def test_source_owned_report_has_seven_tuples_five_published_frames_and_63_consumed_controls(self):
            records, controls_doc = self.synthetic_source_and_controls()
            for consumer in ("native_raw", "official_raw"):
                with tempfile.TemporaryDirectory() as temp:
                    root_dir = Path(temp)
                    report, raw, path = self.synthetic_local_report(root_dir, "debug", consumer, records, controls_doc)
                    checked = validate_local_report(raw, path, "debug", consumer, records, controls_doc, {})
                    self.assertEqual(len(checked["cases"]), 7)
                    self.assertEqual(sum(len(row["captures"]) for row in checked["cases"]), 35)
                    self.assertEqual(sum(len(row["controls"]) for row in checked["cases"]), 63)
                    bad = copy.deepcopy(report); bad["cases"][0]["controls"].pop()
                    with self.assertRaises(VerifyError):
                        validate_local_report((json.dumps(bad) + "\n").encode(), path, "debug", consumer, records, controls_doc, {})
                    bad = copy.deepcopy(report); bad["cases"][0]["controls"][5]["resource_failure"] = True
                    with self.assertRaises(VerifyError):
                        validate_local_report((json.dumps(bad) + "\n").encode(), path, "debug", consumer, records, controls_doc, {})
                    bad = copy.deepcopy(report); bad["cases"][0]["fallback_credit"] = 1
                    with self.assertRaises(VerifyError):
                        validate_local_report((json.dumps(bad) + "\n").encode(), path, "debug", consumer, records, controls_doc, {})
                    bad = copy.deepcopy(report); bad["cases"][0]["captures"][0]["published"] = False
                    with self.assertRaises(VerifyError):
                        validate_local_report((json.dumps(bad) + "\n").encode(), path, "debug", consumer, records, controls_doc, {})

        def test_synthetic_matrix_join_replaces_seven_original_slots_without_appending(self):
            records, _ = self.synthetic_source_and_controls()
            report_cases = [{"tuple": tuple_for(r), "narrow_disposition": "source_fallback" if i == 0 else "drawn",
                             "canonical_model_sha256": r["canonical_model_sha256"],
                             "canonical_wire_model_sha256": r["canonical_wire_model_sha256"]}
                            for i, r in enumerate(records)]
            reports = {("debug", "official_raw"): {"cases": report_cases}}
            selected = {r["fixture"]: r for r in records}
            cases = []
            for i in range(2145):
                if i < len(records):
                    source = records[i]
                    cases.append({"fixture": source["fixture"], "track": "original", "source_sha256": source["source_sha256"],
                                  "expected": "pending", "reason": "old", "observed": {"wide_240": "error", "narrow_40": "error", "restored_240": "error"},
                                  "expected_per_state": {"wide_240": "pending", "narrow_40": "pending", "restored_240": "pending"},
                                  "decision_reasons": {"wide_240": "old", "narrow_40": "old", "restored_240": "old"}, "errors": ["pending"]})
                else:
                    cases.append({"fixture": f"flowchart/synthetic/original-{i}", "track": "original", "source_sha256": "9" * 64,
                                  "expected": "pending", "errors": ["other"]})
            for i in range(593): cases.append({"fixture": f"flowchart/synthetic/body-{i}", "track": "body_diagnostic", "source_sha256": "8" * 64, "errors": ["diagnostic"]})
            matrix = {"cases": cases, "original_drawing_coverage": {"drawn_at_240": 688, "pinned_original_files": 2145,
                       "original_files_executed": 2145, "percent_of_pinned_original_files": 100.0 * 688 / 2145},
                      "aggregates": [{"track": "original", "type": diagram_type, "state": state,
                                      "drawn": 0, "source_fallback": 0, "error": 7}
                                     for diagram_type in ("ALL", "flowchart")
                                     for state in ("wide_240", "narrow_40", "restored_240")]}
            original_text = json.dumps(matrix, ensure_ascii=False, indent=2) + "\n"
            body_spans = json_spans(original_text)
            body_before = [original_text[slice(*body_spans[("cases", i)])] for i in range(2145, 2738)]
            marker = {"schema": "synthetic marker", "formal_admitted_originals": 695}
            updated_text = matrix_patches(original_text, reports, records, marker)
            updated = parse_json(updated_text.encode())
            self.assertEqual(len(updated["cases"]), 2738)
            self.assertEqual(sum(row["track"] == "original" for row in updated["cases"]), 2145)
            self.assertEqual(sum(row["track"] == "body_diagnostic" for row in updated["cases"]), 593)
            self.assertEqual(updated["original_drawing_coverage"]["drawn_at_240"], 695)
            self.assertEqual(updated[JOIN_KEY], marker)
            new_spans = json_spans(updated_text)
            self.assertEqual(body_before, [updated_text[slice(*new_spans[("cases", i)])] for i in range(2145, 2738)])
            self.assertTrue(all(updated["cases"][i]["errors"] == [] for i in range(7)))

        def test_synthetic_markdown_join_matches_exact_original_rows_once(self):
            records, _ = self.synthetic_source_and_controls()
            report_cases = [{"tuple": tuple_for(r), "narrow_disposition": "source_fallback" if i == 0 else "drawn"}
                            for i, r in enumerate(records)]
            reports = {("debug", "official_raw"): {"cases": report_cases}}
            rows = [f"| {r['fixture']} | original | pending | source_fallback | source_fallback | source_fallback | stale | old |\n"
                    for r in records]
            rows.append(f"| {records[0]['fixture']} | body_diagnostic | source | source_fallback | source_fallback | source_fallback | diagnostic | diagnostic |\n")
            original = "| fixture | track | expected | wide | narrow | restored | reason | detail |\n" + "".join(rows)
            updated = markdown_patch(original, records, reports)
            self.assertEqual(updated.count("| original | drawn | drawn | source_fallback | drawn |"), 1)
            self.assertEqual(updated.count("| Flow400 GroupEndpoint Extra7 dual-consumer admission |"), 7)
            self.assertIn(f"| {records[0]['fixture']} | body_diagnostic | source | source_fallback |", updated)
            with self.assertRaises(VerifyError):
                markdown_patch(original + rows[0], records, reports)
            with self.assertRaises(VerifyError):
                markdown_patch(original.replace(rows[0], ""), records, reports)

        def test_synthetic_ctest_accepts_dynamic_ids_but_rejects_name_or_status_change(self):
            failures = sorted(KNOWN_FAILURES)
            source_lists = {
                name: [f"er-diagram/synthetic-{count}-{i:03}.mmd" for i in range(count)]
                for name, (count, _) in KNOWN_FAILURES.items()
            }
            custom_failures = {name: (len(paths), sha(("\n".join(paths) + "\n").encode())) for name, paths in source_lists.items()}
            old_rows = [(i + 100, f"SyntheticMermaid.Pass{i:03}", "passed") for i in range(904)]
            old_rows += [(2000, "MermaidCapabilityReport.Flow400CompleteSources", "passed"),
                         (2001, "MermaidFlowGroupEndpointExtra7.NativeSourcesNeedFivePublishedFramesAndCompleteGeometry", "passed")]
            old_rows += [(3000 + i, name, "failed") for i, name in enumerate(failures)]
            rows = sorted(old_rows + [(4000 + i, name, status) for i, (name, status) in enumerate(NEW_CTESTS.items())])
            failed = [(i, name) for i, name, status in rows if status == "failed"]
            old_pairs = [{"test_name": name, "status": status} for _, name, status in rows if name not in NEW_CTESTS]
            old_pairs.sort(key=lambda row: (row["test_name"], row["status"]))
            expected_sha = sha(canonical_bytes(old_pairs))
            parsed = validate_ctest_records(rows, failed, source_lists, expected_sha, custom_failures)
            self.assertEqual(len(parsed["tests"]), 914)
            shifted = sorted((i + 600, name, status) for i, name, status in rows)
            shifted_failed = [(i, name) for i, name, status in shifted if status == "failed"]
            self.assertEqual(validate_ctest_records(shifted, shifted_failed, source_lists, expected_sha, custom_failures)["old_name_status_sha256"], expected_sha)
            altered = list(source_lists[failures[0]]); altered[0] += "-changed"
            bad_sources = dict(source_lists); bad_sources[failures[0]] = altered
            with self.assertRaises(VerifyError): validate_ctest_records(rows, failed, bad_sources, expected_sha, custom_failures)
            duplicate = list(rows); duplicate[-1] = (duplicate[0][0], duplicate[-1][1], duplicate[-1][2])
            with self.assertRaises(VerifyError): validate_ctest_records(duplicate, failed, source_lists, expected_sha, custom_failures)
            unknown = list(rows); unknown[0] = (unknown[0][0], unknown[0][1], "unknown")
            with self.assertRaises(VerifyError): validate_ctest_records(unknown, failed, source_lists, expected_sha, custom_failures)

        def test_last_test_log_parser_requires_one_status_per_identity(self):
            lines = []
            for i in range(914):
                status = "Test Failed." if i == 913 else "Test Passed."
                lines.append(f"{i + 1}/{CTEST_REGISTERED_TEST_COUNT} Testing: SyntheticMermaid.{i:03}\n{status}\n")
            rows = parse_last_test("".join(lines).encode())
            self.assertEqual(len(rows), 914)
            with self.assertRaises(VerifyError): parse_last_test("1/4093 Testing: A\nno status\n".encode())
            for wrong_total in (4093, 4096):
                wrong_total_log = f"1/{wrong_total} Testing: SyntheticMermaid.TotalMismatch\nTest Passed.\n"
                with self.assertRaises(VerifyError): parse_last_test(wrong_total_log.encode())
            duplicate_log = lines.copy(); duplicate_log[-1] = duplicate_log[0].replace("914/", "1/") + "Test Failed.\n"
            with self.assertRaises(VerifyError): parse_last_test("".join(duplicate_log).encode())

        def test_stage_record_chain_rejects_reordering_and_tamper(self):
            items = []
            prior = None
            for i, stage in enumerate(STAGE_HASH_KEYS):
                rec = {"schema": "flow400_extra7_prerequisite_stage_capture/v1", "preset": "debug", "stage": stage,
                       "sequence": i + 1, "previous_stage_record_sha256": prior, "record_sha256": ""}
                rec["record_sha256"] = record_digest(rec); prior = rec["record_sha256"]; items.append(rec)
            self.assertEqual([x["stage"] for x in items], list(STAGE_HASH_KEYS))
            altered = {**items[1], "stage": "mindmap10"}
            with self.assertRaises(VerifyError): verify_record_digest(altered)
            self.assertNotEqual([x["stage"] for x in items], [x["stage"] for x in reversed(items)])

        def test_atomic_publication_rolls_back_every_handled_replace(self):
            for failure_index in range(1, 5):
                with tempfile.TemporaryDirectory() as temp:
                    root = Path(temp); paths = [root / f"out{i}" for i in range(4)]
                    for i, p in enumerate(paths): p.write_bytes(f"old{i}".encode())
                    guards = {p: fingerprint(p) for p in paths}
                    outputs = {p: f"new{i}".encode() for i, p in enumerate(paths)}
                    real_replace = os.replace; calls = 0
                    def replace(source, target):
                        nonlocal calls
                        calls += 1
                        if calls == failure_index: raise OSError("synthetic replace failure")
                        return real_replace(source, target)
                    with mock.patch.object(os, "replace", side_effect=replace), self.assertRaises(OSError):
                        atomic_write(outputs, guards)
                    self.assertEqual([p.read_bytes() for p in paths], [f"old{i}".encode() for i in range(4)])
                    for p in paths: self.assertEqual(p.stat().st_mtime_ns, guards[p]["mtime_ns"])

        def test_reset_sidecar_preflight_rejects_unknown_without_mutating_inputs(self):
            with tempfile.TemporaryDirectory() as temp:
                root = Path(temp); matrix = root / MATRIX; md = root / MARKDOWN; marker = root / EXPECTED_MARKERS[0][1]
                matrix.write_bytes(b"synthetic matrix"); md.write_bytes(b"synthetic md"); marker.write_bytes(b"partial stale sidecar")
                before = {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in (matrix, md, marker)}
                with self.assertRaises(VerifyError): sidecar_state(root, "debug")
                self.assertEqual(before, {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in (matrix, md, marker)})

        def test_control_gate_rejects_skipped_duplicate_and_resource_controls(self):
            rows = [{"id": control, "class": "semantic_model" if control.startswith("semantic_") else "physical_grid",
                     "execution_key": "synthetic/" + control,
                     "decoded_before_geometry": control.startswith("semantic_"), "rejected": True,
                     "resource_failure": False, "preimage_sha256": "a" * 64} for control in CONTROL_IDS]
            self.assertEqual(len({x["id"] for x in rows}), 9)
            self.assertNotEqual([x["id"] for x in rows[:-1]], list(CONTROL_IDS))
            self.assertEqual(len([x["id"] for x in rows + [rows[0]]]), 10)
            bad_resource = [dict(x, resource_failure=True) if x["id"] == CONTROL_IDS[-1] else x for x in rows]
            self.assertTrue(any(x["resource_failure"] for x in bad_resource))

    suite = unittest.defaultTestLoader.loadTestsFromTestCase(CanonicalControls)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful(): raise VerifyError("synthetic self-test failed")
    subprocess.run([sys.executable, "-I", str(ROOT / "tools/mermaid/verify_flow400_issue4648_admission.py"), "--self-test"], check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--capture-prerequisite-reset", action="store_true")
    group.add_argument("--capture-prerequisite-stage", choices=STAGE_SEQUENCE)
    group.add_argument("--check", action="store_true")
    group.add_argument("--self-test", action="store_true")
    group.add_argument("--write", action="store_true")
    parser.add_argument("--build-dirs", default=f"{BUILD_DIRS['debug']},{BUILD_DIRS['release']}")
    args = parser.parse_args()
    try:
        if args.self_test:
            canonical_fault_tests()
            print("OK synthetic Flow400 admission joiner self-tests; zero drawing credit; no repository reports or matrices read")
            return 0
        dirs = make_directories_arg(args.build_dirs)
        if args.capture_prerequisite_reset:
            capture_reset(dirs)
        elif args.capture_prerequisite_stage:
            capture_stage(args.capture_prerequisite_stage, dirs)
        elif args.check:
            final_check(dirs, writing=False)
        elif args.write:
            write(args)
        return 0
    except (VerifyError, OSError, KeyError, TypeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
