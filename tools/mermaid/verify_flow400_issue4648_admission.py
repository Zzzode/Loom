#!/usr/bin/env python3
"""Join the Issue4648 Flow400 source proof after the frozen Extra7 admission.

The verifier consumes compact Native and Official verdicts produced by the
source-owned C++ tests. It does not invoke Loom or infer drawing success from
source fallback. --self-test uses synthetic reports and grants no credit.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import os
import sys
import tempfile
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
PRESETS = ("debug", "release")
BUILD_DIRS = {preset: ROOT / "build" / preset / "tests/ui" for preset in PRESETS}
FACTS_PATH = Path("tests/fixtures/mermaid_native/flow400_nested_external1_source_models.v1.json")
FACTS_BYTES = 4067
FACTS_SHA = "24cde42857356bf826a4147e1e96772d44cec62b4e5a54b52329363cc3aee710"
MANIFEST_PATH = Path("tests/fixtures/mermaid_official/manifest.json")
MANIFEST_SHA = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"
CASE_ID = "flow400_nested_external1_357"
ORDINAL = 357
GLOBAL_ORDINAL = 1018
FIXTURE = "flowchart/visual-check/issue-4648-nested-subgraph-with-external-connection.mmd"
SOURCE_SHA = "403c54cf5f29cc7de0886f4ac363fd8c3229bc3d0d4539023f1a9a15a9a560a8"
SOURCE_GIT_BLOB = "2bfa9e0e828f284b9dea4e7eac7019e139b2fae6"
MODEL_SHA = "bccc470587698a572a1880cef51d654deeececcabfe8dc26f9e0bd91d238574a"
MODEL_JSON = (
    '{"direction":"TD","nodes":[["D1",{"id":"D1","label":"D1","shape":"rectangle"}],'
    '["D2",{"id":"D2","label":"D2","shape":"rectangle"}],'
    '["D3",{"id":"D3","label":"D3","shape":"rectangle"}],'
    '["E1",{"id":"E1","label":"E1","shape":"rectangle"}]],'
    '"edges":[{"source":"D1","target":"D2","label":"","style":"solid","hasArrowStart":false,"hasArrowEnd":true},'
    '{"source":"D2","target":"D3","label":"","style":"solid","hasArrowStart":false,"hasArrowEnd":true},'
    '{"source":"D3","target":"E1","label":"","style":"solid","hasArrowStart":false,"hasArrowEnd":false}],'
    '"subgraphs":[{"id":"Wrapper","label":"Wrapper","direction":"LR","nodeIds":[],"children":['
    '{"id":"Inner","label":"Inner","direction":"","nodeIds":["D1","D2","D3"],"children":[]}]}]}'
)
NATIVE_REPORT = "mermaid-flow400-issue4648-native-verdict.json"
OFFICIAL_REPORT = "mermaid-flow400-issue4648-official-verdict.json"
JOIN_FILE = "mermaid-flow400-issue4648-dual-admission.json"
EXTRA7_JOIN_FILE = "mermaid-flow400-group-endpoint-extra7-dual-admission.json"
MATRIX = "mermaid-official-matrix.json"
MARKDOWN = "mermaid-official-matrix.md"
JOIN_KEY = "Flow400Issue4648_dual_consumer_join"
SUMMARY_KEY = "Flow400Issue4648_dual_consumer_admission"
EXTRA7_KEY = "Flow400GroupEndpointExtra7_dual_consumer_join"
NATIVE_TEST = "MermaidFlowSemantics.OfficialIssue4648ArrowlessNestedSubgraphIsSourceBoundGeometry"
OFFICIAL_TEST = "MermaidOfficialFlow400.Issue4648ArrowlessNestedSubgraphRequiresOwnedGeometryAndControls"
REPORT_FIELDS = {
    "schema", "source_facts_sha256", "case_id", "ordinal", "manifest_global_ordinal", "fixture",
    "source_sha256", "model_sha256", "captures", "captures_before_binding", "source_bound_after_captures",
    "wide_geometry_good", "restored_geometry_good", "narrow_exact_ordinary", "narrow_geometry_good",
    "semantic_mutations_rejected", "physical_mutations_rejected", "profile_controls_rejected",
    "generic_mutations_rejected", "resource_errors", "source_fallback_credit", "admitted",
}
_EXTRA7_VERIFIER: Any | None = None


class VerifyError(ValueError):
    pass


def need(condition: bool, reason: str) -> None:
    if not condition:
        raise VerifyError(reason)


def sha(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def parse_json(raw: bytes, reason: str) -> Any:
    try:
        return json.loads(raw, object_pairs_hook=unique_object)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise VerifyError(f"{reason}: {error}") from error


def read_json(path: Path, limit: int = 32 * 1024 * 1024) -> tuple[Any, bytes]:
    need(path.is_file() and path.stat().st_size <= limit, f"bounded file: {path}")
    raw = path.read_bytes()
    return parse_json(raw, f"invalid JSON {path}"), raw


def fact(root: Path = ROOT) -> dict[str, Any]:
    asset_path = root / FACTS_PATH
    raw = asset_path.read_bytes()
    need(len(raw) == FACTS_BYTES and sha(raw) == FACTS_SHA, "Issue4648 immutable source-facts asset changed")
    document = parse_json(raw, "Issue4648 source-facts JSON")
    need(document.get("schema") == "flow400_nested_external1_source_models/v1" and
         document.get("formal_credit_before_native_and_official_owned_execution") == 0 and
         document.get("cohort") == {"ordinals": [ORDINAL], "records": 1}, "Issue4648 source-facts root")
    records = document.get("records")
    need(isinstance(records, list) and len(records) == 1, "one Issue4648 source record")
    row = records[0]
    need(row.get("case_id") == CASE_ID and row.get("ordinal") == ORDINAL and
         row.get("manifest_global_ordinal") == GLOBAL_ORDINAL and row.get("fixture") == FIXTURE and
         row.get("source_bytes") == 529 and row.get("source_sha256") == SOURCE_SHA and
         row.get("source_git_blob") == SOURCE_GIT_BLOB and row.get("model_json") == MODEL_JSON and
         row.get("model_sha256") == MODEL_SHA, "Issue4648 exact candidate tuple / Model")
    need(row.get("selection_constraints") == {
        "base_model_is_immutable_null": True, "fixture_path_dispatch": False,
        "native_and_official_required": True, "source_fallback_credit": 0,
    }, "Issue4648 candidate admission constraints")
    return row


def validate_source(root: Path, row: dict[str, Any]) -> None:
    manifest_path = root / MANIFEST_PATH
    manifest_raw = manifest_path.read_bytes()
    need(sha(manifest_raw) == MANIFEST_SHA, "Official manifest identity")
    manifest = parse_json(manifest_raw, "Official manifest JSON")
    fixtures = manifest.get("fixtures")
    need(isinstance(fixtures, list) and len(fixtures) == 2145, "Official manifest denominator")
    selected = fixtures[GLOBAL_ORDINAL]
    need(selected.get("path") == FIXTURE and selected.get("sha256") == SOURCE_SHA and
         selected.get("git_blob") == SOURCE_GIT_BLOB and selected.get("bytes") == 529,
         "Issue4648 manifest original identity")
    source_path = root / "tests/fixtures/mermaid_official/cases" / FIXTURE
    source = source_path.read_bytes()
    need(len(source) == 529 and sha(source) == SOURCE_SHA and
         hashlib.sha1(b"blob " + str(len(source)).encode("ascii") + b"\0" + source).hexdigest() == SOURCE_GIT_BLOB,
         "Issue4648 original source bytes/Git blob")
    text = source.decode("utf-8")
    offset = row["body_offset_bytes"]
    body = row["body_utf8"].encode("utf-8")
    need(offset == 371 and len(body) == row["body_bytes"] == 158 and
         sha(body) == row["body_sha256"] and source[offset:offset + len(body)] == body,
         "Issue4648 body offset/identity")
    need(text == row["raw_source_utf8"] and sha(row["model_json"].encode("utf-8")) == MODEL_SHA,
         "Issue4648 complete-source and Model binding")


def validate_report(report: Any, expected_schema: str) -> dict[str, Any]:
    need(isinstance(report, dict) and set(report) == REPORT_FIELDS, f"closed {expected_schema} report schema")
    need(report["schema"] == expected_schema and report["source_facts_sha256"] == FACTS_SHA,
         "candidate consumer and source-facts identity")
    for key, expected in (("case_id", CASE_ID), ("ordinal", ORDINAL),
                          ("manifest_global_ordinal", GLOBAL_ORDINAL), ("fixture", FIXTURE),
                          ("source_sha256", SOURCE_SHA), ("model_sha256", MODEL_SHA)):
        need(report[key] == expected, f"candidate report pin: {key}")
    need(report["captures"] == 5 and report["captures_before_binding"] is True and
         report["source_bound_after_captures"] is True, "five independent captures precede source binding")
    need(report["wide_geometry_good"] is True and report["restored_geometry_good"] is True,
         "source-owned wide/restored geometry")
    need(type(report["narrow_exact_ordinary"]) is bool and type(report["narrow_geometry_good"]) is bool and
         report["narrow_exact_ordinary"] != report["narrow_geometry_good"], "narrow source XOR geometry disposition")
    need(report["semantic_mutations_rejected"] == 8 and report["physical_mutations_rejected"] == 5 and
         report["profile_controls_rejected"] == 4 and report["generic_mutations_rejected"] > 0,
         "semantic, physical, profile and generic control counts")
    need(report["resource_errors"] == 0 and report["source_fallback_credit"] == 0 and
         report["admitted"] is True, "resource-free drawing proof with zero fallback credit")
    return report


def extra7_verifier() -> Any:
    global _EXTRA7_VERIFIER
    if _EXTRA7_VERIFIER is not None:
        return _EXTRA7_VERIFIER
    verifier_path = ROOT / "tools/mermaid/verify_flow400_group_endpoint_extra7_admission.py"
    spec = importlib.util.spec_from_file_location("loom_flow400_extra7_admission", verifier_path)
    need(spec is not None and spec.loader is not None, "load shared strict Mermaid CTest verifier")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    _EXTRA7_VERIFIER = module
    return module


def shared_ctest_semantics(preset: str) -> dict[str, Any]:
    return extra7_verifier().ctest_semantics(preset)


def validate_ctest_log(ui_dir: Path, preset: str) -> bytes:
    # Reuse the frozen-cohort parser: it validates unique IDs and names, all
    # 914 statuses, the 910/4 split, known ER107 source cohorts, LastTestsFailed,
    # and freshness against every source and producer binary.
    shared_ctest_semantics(preset)
    return (ui_dir.parent.parent / "Testing/Temporary/LastTest.log").read_bytes()


def read_fresh_consumer(ui_dir: Path, preset: str, consumer: str, filename: str,
                        schema: str, binary_name: str) -> tuple[dict[str, Any], bytes, bytes]:
    report_path = ui_dir / filename
    report, report_raw = read_json(report_path, 65536)
    checked = validate_report(report, schema)
    binary_path = ui_dir / binary_name
    ctest_path = ui_dir.parent.parent / "Testing/Temporary/LastTest.log"
    source_mtime = max((ROOT / source).stat().st_mtime_ns for source in extra7_verifier().SOURCE_SET_PATHS)
    need(binary_path.is_file() and binary_path.stat().st_mtime_ns >= source_mtime,
         f"stale {preset} {consumer} producer binary versus source set")
    need(binary_path.stat().st_mtime_ns <= report_path.stat().st_mtime_ns,
         f"{preset} {consumer} report must be produced after its binary was built")
    need(report_path.stat().st_mtime_ns <= ctest_path.stat().st_mtime_ns,
         f"{preset} {consumer} verdict is newer than its complete CTest run")
    return checked, report_raw, binary_path.read_bytes()


def validate_extra7(ui_dir: Path, matrix: dict[str, Any]) -> bytes:
    marker_path = ui_dir / EXTRA7_JOIN_FILE
    marker, marker_raw = read_json(marker_path, 2 * 1024 * 1024)
    need(marker.get("schema") == "flow400_group_endpoint_extra7_dual_consumer_admission/v1" and
         marker.get("base_admitted_originals") == 688 and marker.get("joined_originals") == 7 and
         marker.get("formal_admitted_originals") == 695 and marker.get("fallback_credit") == 0 and
         marker.get("resource_failures") == 0 and marker.get("foreign_assertions_certified") == 0,
         "frozen Flow400 Extra7 prerequisite marker")
    drawn = matrix.get("original_drawing_coverage", {}).get("drawn_at_240")
    need(matrix.get(EXTRA7_KEY) == marker and drawn in (695, 696) and
         ((drawn == 695 and JOIN_KEY not in matrix) or
          (drawn == 696 and matrix.get(JOIN_KEY, {}).get("formal_admitted_originals") == 696)),
         "Official matrix must retain the exact formal 695 Extra7 base and optional Issue4648 join")
    return marker_raw


STATUSES = ("drawn", "source_fallback", "error")


def aggregate_counts(matrix: dict[str, Any], diagram_type: str, state: str) -> dict[str, int]:
    rows = [row for row in matrix["cases"] if row.get("track") == "original" and
            (diagram_type == "ALL" or row.get("type") == diagram_type)]
    counts = {status: 0 for status in STATUSES}
    for row in rows:
        observed = row.get("observed")
        need(isinstance(observed, dict) and observed.get(state) in counts,
             f"complete original disposition for {diagram_type}/{state}")
        counts[observed[state]] += 1
    return counts


def verify_aggregates_match_rows(matrix: dict[str, Any]) -> None:
    for diagram_type in ("ALL", "flowchart"):
        for state in ("wide_240", "narrow_40", "restored_240"):
            entries = [item for item in matrix["aggregates"] if item.get("track") == "original" and
                       item.get("type") == diagram_type and item.get("state") == state]
            need(len(entries) == 1, f"unique Official aggregate {diagram_type}/{state}")
            expected = aggregate_counts(matrix, diagram_type, state)
            need(all(type(entries[0].get(key)) is int and entries[0][key] == count
                     for key, count in expected.items()),
                 f"Official aggregate must match original rows for {diagram_type}/{state}")


def apply_join(matrix: dict[str, Any], case_evidence: dict[str, Any], marker: dict[str, Any]) -> dict[str, Any]:
    need(matrix.get("integrity_errors") == [] and isinstance(matrix.get("cases"), list) and len(matrix["cases"]) == 2738,
         "clean Official matrix with 2,145 originals and 593 diagnostics")
    need(matrix.get("original_drawing_coverage", {}).get("drawn_at_240") == 695 and
         matrix.get(EXTRA7_KEY, {}).get("formal_admitted_originals") == 695,
         "frozen formal 695 precondition")
    need(JOIN_KEY not in matrix and SUMMARY_KEY not in matrix, "Issue4648 join is single-use")
    selected = [row for row in matrix["cases"] if row.get("track") == "original" and row.get("fixture") == FIXTURE]
    need(len(selected) == 1, "one Issue4648 Official original row")
    target = selected[0]
    need(target.get("source_sha256") == SOURCE_SHA and target.get("type") == "flowchart" and
         target.get("expected") == "pending" and
         target.get("observed") == {"wide_240": "error", "narrow_40": "source_fallback", "restored_240": "error"} and
         target.get("expected_per_state") == {"wide_240": "unadmitted_whole_source", "narrow_40": "unadmitted_whole_source",
                                               "restored_240": "unadmitted_whole_source"} and
         target.get("errors") == ["Whole-null Flow has unreviewed art or failed ordinary/restore protocol"],
         "pinned pending Issue4648 row state and exact dispositions")
    verify_aggregates_match_rows(matrix)
    narrow = "source_fallback" if case_evidence["narrow_exact_ordinary"] else "drawn"
    target["expected"] = "drawn"
    target["reason"] = "Flow400 Issue4648 Native/Official dual-consumer admission"
    target["observed"] = {"wide_240": "drawn", "narrow_40": narrow, "restored_240": "drawn"}
    target["expected_per_state"] = copy.deepcopy(target["observed"])
    target["decision_reasons"] = {
        "wide_240": "independent source-owned geometry", "narrow_40": "complete geometry or exact ordinary source with zero credit",
        "restored_240": "independent source-owned restored geometry",
    }
    target["errors"] = []
    target["Flow400Issue4648_dual_consumer_proof"] = {
        "contract": "Flow400Issue4648/v1", "case_id": CASE_ID, "source_facts_sha256": FACTS_SHA,
        "source_sha256": SOURCE_SHA, "model_sha256": MODEL_SHA, "captures": 5,
        "captures_before_binding": True, "wide_geometry_good": True, "restored_geometry_good": True,
        "narrow_disposition": narrow, "semantic_mutations_rejected": 8, "physical_mutations_rejected": 5,
        "profile_controls_rejected": 4, "resource_failures": 0, "fallback_credit": 0,
        "foreign_assertions_certified": 0, "native_report_sha256": marker["verdict_sha256"]["debug"]["native"],
        "official_report_sha256": marker["verdict_sha256"]["debug"]["official"],
    }
    for diagram_type in ("ALL", "flowchart"):
        for state, old_status, new_status in (
            ("wide_240", "error", "drawn"), ("narrow_40", "source_fallback", narrow),
            ("restored_240", "error", "drawn"),
        ):
            if old_status == new_status:
                continue
            entries = [item for item in matrix["aggregates"] if item.get("track") == "original" and
                       item.get("type") == diagram_type and item.get("state") == state]
            aggregate = entries[0]
            need(aggregate[old_status] > 0, "bounded aggregate status delta")
            aggregate[old_status] -= 1
            aggregate[new_status] += 1
    verify_aggregates_match_rows(matrix)
    coverage = matrix["original_drawing_coverage"]
    coverage["drawn_at_240"] = 696
    coverage["percent_of_pinned_original_files"] = 100.0 * 696 / 2145
    matrix[SUMMARY_KEY] = {
        "contract": "Flow400Issue4648/v1", "selected": 1, "local_ready": 1, "admitted": 1,
        "base_admitted_originals": 695, "source_facts_sha256": FACTS_SHA,
        "foreign_assertions_certified": 0, "native_report_input": False,
    }
    matrix[JOIN_KEY] = marker
    return matrix


def markdown_patch(text: str, narrow: str) -> str:
    lines = text.splitlines(keepends=True)
    updated = 0
    output: list[str] = []
    for line in lines:
        if line.startswith("|"):
            parts = [item.strip() for item in line.rstrip("\r\n").strip().strip("|").split("|")]
            if len(parts) == 8 and parts[1] == "original" and parts[0] == FIXTURE:
                updated += 1
                parts[2:6] = ["drawn", "drawn", narrow, "drawn"]
                parts[6] = "Flow400 Issue4648 Native/Official dual-consumer admission"
                output.append("| " + " | ".join(parts) + " |\n")
                continue
        output.append(line)
    need(updated == 1, "one Issue4648 Original Markdown row")
    patched = "".join(output)
    need("\n## Flow400 Issue4648 formal admission\n" not in patched, "Issue4648 Markdown section is single-use")
    return patched + (
        "\n## Flow400 Issue4648 formal admission\n\n"
        "The independent Native and Official consumers in Debug and Release each bind the complete original after five captures, "
        "admit wide/restored geometry, pass eight semantic and five physical mutations plus four profile controls, and report zero "
        "resource failures or fallback credit. The narrow state is drawn only when geometry passes; an exact ordinary-source "
        "fallback receives zero drawing credit. Formal coverage: **696/2145 (32.45%)**.\n"
    )


def replace_transaction(outputs: dict[Path, bytes]) -> None:
    old = {path: path.read_bytes() if path.exists() else None for path in outputs}
    temporaries: dict[Path, Path] = {}
    try:
        for path, raw in outputs.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(dir=path.parent, prefix=f".{path.name}.", delete=False) as stream:
                stream.write(raw)
                stream.flush()
                os.fsync(stream.fileno())
                temporaries[path] = Path(stream.name)
        replaced: list[Path] = []
        try:
            for path, temporary in temporaries.items():
                os.replace(temporary, path)
                replaced.append(path)
        except OSError:
            for path in reversed(replaced):
                previous = old[path]
                if previous is None:
                    path.unlink(missing_ok=True)
                else:
                    path.write_bytes(previous)
            raise
    finally:
        for temporary in temporaries.values():
            temporary.unlink(missing_ok=True)


def marker_for_inputs(reports: dict[str, dict[str, dict[str, Any]]],
                      raw_reports: dict[str, dict[str, bytes]],
                      binaries: dict[str, dict[str, bytes]], ctest_logs: dict[str, bytes],
                      extra7_markers: dict[str, bytes]) -> dict[str, Any]:
    verdict_shas = {preset: {consumer: sha(raw_reports[preset][consumer]) for consumer in ("native", "official")}
                    for preset in PRESETS}
    return {
        "schema": "flow400_issue4648_dual_consumer_admission/v1",
        "source_facts_sha256": FACTS_SHA,
        "base_admitted_originals": 695, "joined_originals": 1, "formal_admitted_originals": 696,
        "pinned_original_files": 2145, "official_original_rows": 2145, "body_diagnostic_rows": 593,
        "total_observations": 2738, "verdict_sha256": verdict_shas,
        "binary_sha256": {preset: {consumer: sha(binaries[preset][consumer]) for consumer in ("native", "official")}
                           for preset in PRESETS},
        "ctest_log_sha256": {preset: sha(ctest_logs[preset]) for preset in PRESETS},
        "extra7_marker_sha256": sha(extra7_markers["debug"]),
        "resource_failures": 0, "fallback_credit": 0, "foreign_assertions_certified": 0,
        "native_report_input": False,
        "cases": [{"case_id": CASE_ID, "ordinal": ORDINAL, "manifest_global_ordinal": GLOBAL_ORDINAL,
                   "fixture": FIXTURE, "source_sha256": SOURCE_SHA, "model_sha256": MODEL_SHA}],
    }


def collect_inputs(build_dirs: dict[str, Path]) -> dict[str, Any]:
    need(set(build_dirs) == set(PRESETS) and all(build_dirs[p] == BUILD_DIRS[p] for p in PRESETS),
         "only exact repository Debug and Release report directories are accepted")
    selected = fact()
    validate_source(ROOT, selected)
    reports: dict[str, dict[str, dict[str, Any]]] = {preset: {} for preset in PRESETS}
    raw_reports: dict[str, dict[str, bytes]] = {preset: {} for preset in PRESETS}
    binaries: dict[str, dict[str, bytes]] = {preset: {} for preset in PRESETS}
    ctest_logs: dict[str, bytes] = {}
    extra7_markers: dict[str, bytes] = {}
    matrix_bytes: dict[str, bytes] = {}
    markdown_bytes: dict[str, bytes] = {}
    matrices: dict[str, dict[str, Any]] = {}
    issue_markers: dict[str, bytes] = {}
    for preset in PRESETS:
        ui_dir = build_dirs[preset]
        ctest_raw = validate_ctest_log(ui_dir, preset)
        ctest_logs[preset] = ctest_raw
        matrix, matrix_raw = read_json(ui_dir / MATRIX)
        extra7_markers[preset] = validate_extra7(ui_dir, matrix)
        matrix_bytes[preset] = matrix_raw
        matrices[preset] = matrix
        md_path = ui_dir / MARKDOWN
        markdown_bytes[preset] = md_path.read_bytes()
        for consumer, filename, schema, binary_name in (
            ("native", NATIVE_REPORT, "flow400_issue4648_native_verdict/v1", "test_mermaid_flow_semantics"),
            ("official", OFFICIAL_REPORT, "flow400_issue4648_official_verdict/v1", "test_mermaid_official"),
        ):
            report, raw, binary = read_fresh_consumer(ui_dir, preset, consumer, filename, schema, binary_name)
            reports[preset][consumer] = report
            raw_reports[preset][consumer] = raw
            binaries[preset][consumer] = binary
        if (ui_dir / JOIN_FILE).is_file():
            issue_markers[preset] = (ui_dir / JOIN_FILE).read_bytes()
    for consumer in ("native", "official"):
        need(reports["debug"][consumer] == reports["release"][consumer],
             f"Debug/Release {consumer} candidate verdict differs")
    need(extra7_markers["debug"] == extra7_markers["release"], "Debug/Release Extra7 marker bytes differ")
    need(matrix_bytes["debug"] == matrix_bytes["release"] and markdown_bytes["debug"] == markdown_bytes["release"],
         "Debug/Release Official matrix and Markdown bytes differ")
    expected_marker = marker_for_inputs(reports, raw_reports, binaries, ctest_logs, extra7_markers)
    return {"reports": reports, "raw_reports": raw_reports, "binaries": binaries, "ctest_logs": ctest_logs,
            "extra7_markers": extra7_markers, "matrix_bytes": matrix_bytes, "markdown_bytes": markdown_bytes,
            "matrices": matrices, "issue_markers": issue_markers, "expected_marker": expected_marker}


def write(build_dirs: dict[str, Path]) -> None:
    inputs = collect_inputs(build_dirs)
    reports = inputs["reports"]
    raw_reports = inputs["raw_reports"]
    binaries = inputs["binaries"]
    ctest_logs = inputs["ctest_logs"]
    extra7_markers = inputs["extra7_markers"]
    matrices = inputs["matrices"]
    matrix_bytes = inputs["matrix_bytes"]
    markdown_bytes = inputs["markdown_bytes"]
    for preset in PRESETS:
        matrix = matrices[preset]
        need(JOIN_KEY not in matrix and SUMMARY_KEY not in matrix and matrix.get("original_drawing_coverage", {}).get("drawn_at_240") == 695,
             "Issue4648 write requires an unjoined formal 695 matrix")
        need(preset not in inputs["issue_markers"], "Issue4648 join marker already exists; admission is single-use")
    marker = inputs["expected_marker"]
    outputs: dict[Path, bytes] = {}
    for preset in PRESETS:
        ui_dir = build_dirs[preset]
        matrix = matrices[preset]
        merged = apply_join(matrix, reports["debug"]["official"], marker)
        narrow = "source_fallback" if reports["debug"]["official"]["narrow_exact_ordinary"] else "drawn"
        patched_markdown = markdown_patch(markdown_bytes[preset].decode("utf-8"), narrow)
        outputs[ui_dir / MATRIX] = (json.dumps(merged, ensure_ascii=False, indent=4, allow_nan=False) + "\n").encode("utf-8")
        outputs[ui_dir / MARKDOWN] = patched_markdown.encode("utf-8")
        outputs[ui_dir / JOIN_FILE] = (json.dumps(marker, ensure_ascii=False, indent=4, allow_nan=False) + "\n").encode("utf-8")
    replace_transaction(outputs)


def check(build_dirs: dict[str, Path]) -> None:
    inputs = collect_inputs(build_dirs)
    marker = inputs["expected_marker"]
    need(inputs["issue_markers"].get("debug") is not None and
         inputs["issue_markers"].get("debug") == inputs["issue_markers"].get("release"),
         "Debug/Release Issue4648 marker pair is incomplete or differs")
    stored_marker, marker_raw = read_json(build_dirs["debug"] / JOIN_FILE, 65536)
    need(stored_marker == marker, "Issue4648 marker is stale versus Native/Official reports, binaries, or CTest")
    need((json.dumps(marker, ensure_ascii=False, indent=4, allow_nan=False) + "\n").encode() == marker_raw,
         "Issue4648 marker encoding is not canonical")
    for preset in PRESETS:
        ui_dir = build_dirs[preset]
        matrix = inputs["matrices"][preset]
        need(matrix.get("integrity_errors") == [] and matrix.get("manifest_sha256") == MANIFEST_SHA and
             isinstance(matrix.get("cases"), list) and len(matrix["cases"]) == 2738,
             f"{preset} final Official matrix integrity and denominator")
        coverage = matrix.get("original_drawing_coverage", {})
        need(coverage.get("pinned_original_files") == 2145 and coverage.get("drawn_at_240") == 696 and
             coverage.get("percent_of_pinned_original_files") == 100.0 * 696 / 2145 and
             matrix.get(JOIN_KEY) == marker and matrix.get(EXTRA7_KEY, {}).get("formal_admitted_originals") == 695 and
             matrix.get(SUMMARY_KEY, {}).get("admitted") == 1,
             f"{preset} final Issue4648/Extra7 matrix binding")
        selected = [row for row in matrix["cases"] if row.get("track") == "original" and row.get("fixture") == FIXTURE]
        narrow = "source_fallback" if inputs["reports"][preset]["official"]["narrow_exact_ordinary"] else "drawn"
        need(len(selected) == 1 and selected[0].get("source_sha256") == SOURCE_SHA and selected[0].get("expected") == "drawn" and
             selected[0].get("observed") == {"wide_240": "drawn", "narrow_40": narrow, "restored_240": "drawn"} and
             selected[0].get("expected_per_state") == selected[0].get("observed") and selected[0].get("errors") == [] and
             selected[0].get("Flow400Issue4648_dual_consumer_proof", {}).get("fallback_credit") == 0,
             f"{preset} Issue4648 Original row must preserve zero-credit fallback")
        verify_aggregates_match_rows(matrix)
        md = inputs["markdown_bytes"][preset].decode("utf-8")
        rows = [line for line in md.splitlines() if line.startswith("|") and FIXTURE in line and "| original |" in line]
        need(len(rows) == 1 and all(value in rows[0] for value in ("drawn", narrow,
             "Flow400 Issue4648 Native/Official dual-consumer admission")) and
             "## Flow400 Issue4648 formal admission" in md and "696/2145 (32.45%)" in md,
             f"{preset} Issue4648 Markdown summary")
    print("OK Issue4648 dual-consumer admission, source/binary/report/CTest freshness, and matrix row aggregates")


def self_test() -> None:
    def proof(schema: str) -> dict[str, Any]:
        return {
            "schema": schema, "source_facts_sha256": FACTS_SHA, "case_id": CASE_ID,
            "ordinal": ORDINAL, "manifest_global_ordinal": GLOBAL_ORDINAL, "fixture": FIXTURE,
            "source_sha256": SOURCE_SHA, "model_sha256": MODEL_SHA, "captures": 5,
            "captures_before_binding": True, "source_bound_after_captures": True,
            "wide_geometry_good": True, "restored_geometry_good": True,
            "narrow_exact_ordinary": True, "narrow_geometry_good": False,
            "semantic_mutations_rejected": 8, "physical_mutations_rejected": 5,
            "profile_controls_rejected": 4, "generic_mutations_rejected": 3,
            "resource_errors": 0, "source_fallback_credit": 0, "admitted": True,
        }

    native = validate_report(proof("flow400_issue4648_native_verdict/v1"), "flow400_issue4648_native_verdict/v1")
    official = validate_report(proof("flow400_issue4648_official_verdict/v1"), "flow400_issue4648_official_verdict/v1")
    marker = {"verdict_sha256": {"debug": {"native": "a" * 64, "official": "b" * 64}}}
    cases = [{"track": "original", "fixture": FIXTURE, "type": "flowchart", "source_sha256": SOURCE_SHA,
              "expected": "pending", "errors": ["Whole-null Flow has unreviewed art or failed ordinary/restore protocol"],
              "observed": {"wide_240": "error", "narrow_40": "source_fallback", "restored_240": "error"},
              "expected_per_state": {"wide_240": "unadmitted_whole_source", "narrow_40": "unadmitted_whole_source",
                                     "restored_240": "unadmitted_whole_source"}, "decision_reasons": {}}]
    cases.extend({"track": "original", "fixture": f"other/{index}.mmd", "type": "flowchart" if index < 399 else "class",
                  "observed": {"wide_240": "drawn" if index < 695 else "source_fallback",
                               "narrow_40": "drawn" if index < 695 else "source_fallback",
                               "restored_240": "drawn" if index < 695 else "source_fallback"}}
                 for index in range(2144))
    cases.extend({"track": "body_diagnostic", "fixture": f"body/{index}.mmd"} for index in range(593))
    aggregates = []
    initial = {"cases": cases}
    for diagram_type in ("ALL", "flowchart"):
        for state in ("wide_240", "narrow_40", "restored_240"):
            aggregates.append({"track": "original", "type": diagram_type, "state": state,
                               **aggregate_counts(initial, diagram_type, state)})
    matrix = {"integrity_errors": [], "cases": cases, "aggregates": aggregates,
              "original_drawing_coverage": {"drawn_at_240": 695, "percent_of_pinned_original_files": 100 * 695 / 2145},
              EXTRA7_KEY: {"formal_admitted_originals": 695}}
    base_matrix = copy.deepcopy(matrix)
    changed = apply_join(copy.deepcopy(base_matrix), official, marker)
    need(changed["original_drawing_coverage"]["drawn_at_240"] == 696 and
         changed["cases"][0]["observed"]["narrow_40"] == "source_fallback" and
         changed["cases"][0]["Flow400Issue4648_dual_consumer_proof"]["fallback_credit"] == 0,
         "candidate join updates only owned geometry and grants zero fallback credit")
    for mutate in ("observed", "aggregate"):
        bad_matrix = copy.deepcopy(base_matrix)
        if mutate == "observed":
            bad_matrix["cases"][0]["observed"]["wide_240"] = "drawn"
        else:
            next(item for item in bad_matrix["aggregates"] if item["type"] == "ALL" and item["state"] == "wide_240")["drawn"] += 1
        try:
            apply_join(bad_matrix, official, marker)
        except VerifyError:
            pass
        else:
            raise VerifyError(f"mismatched Issue4648 {mutate} pre-state must fail closed")
    bad = proof("flow400_issue4648_native_verdict/v1")
    bad["source_fallback_credit"] = 1
    try:
        validate_report(bad, "flow400_issue4648_native_verdict/v1")
    except VerifyError:
        pass
    else:
        raise VerifyError("source fallback must never receive drawing credit")
    bad_matrix = copy.deepcopy(base_matrix)
    bad_matrix["cases"][0]["source_sha256"] = "0" * 64
    try:
        apply_join(bad_matrix, official, marker)
    except VerifyError:
        pass
    else:
        raise VerifyError("wrong original identity must fail closed")


def parse_build_dirs(text: str) -> dict[str, Path]:
    paths = [Path(part).resolve() for part in text.split(",")]
    need(len(paths) == 2 and len(set(paths)) == 2, "provide Debug and Release UI report directories")
    result = {"debug": paths[0], "release": paths[1]}
    need(all(result[preset] == BUILD_DIRS[preset].resolve() for preset in PRESETS),
         "only repository Debug and Release presets are accepted")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--self-test", action="store_true")
    group.add_argument("--write", action="store_true")
    group.add_argument("--check", action="store_true")
    parser.add_argument("--build-dirs", default=f"{BUILD_DIRS['debug']},{BUILD_DIRS['release']}")
    args = parser.parse_args()
    try:
        if args.self_test:
            self_test()
            print("OK Issue4648 admission self-test; source fallback receives zero drawing credit")
            return 0
        build_dirs = parse_build_dirs(args.build_dirs)
        if args.write:
            write(build_dirs)
        else:
            check(build_dirs)
        return 0
    except (VerifyError, OSError, KeyError, TypeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
