#!/usr/bin/env python3
"""Validate the tracked partial assertion-index fixture without executing tests."""
from __future__ import annotations

import argparse
import hashlib
import importlib
import importlib.metadata
import json
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import build_terminal_assertion_index as extractor
import terminal_assertion_index_adversarial as adversarial

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
FIXTURE_RELATIVE = extractor.FIXTURE_RELATIVE
INDEX_NAME = extractor.INDEX_NAME
EXPECTED = {
    "frozen_source_containers": 1429,
    "direct_assertion_records": 2573,
    "unresolved_source_containers": 400,
    "unresolved_partial_helpers": 87,
    "summary_only_provenance_records": 186,
}
COVERAGE = (
    "Partial index of the frozen input ledgers only; it is not a five-suite "
    "semantic-assertion denominator or a port-completeness claim."
)
FULL_DECLARED_EXPECTED = {
    "complete_declared_sources": 387,
    "test_context_identities": 6235,
    "assertion_site_entries": 15536,
    "distinct_assertion_source_sites": 15390,
    "nested_repeat_entries": 146,
    "mmdflux_private_predicate_full_context_bindings": 2412,
}


def digest(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def fixture_file(fixture_root: Path, repository_path: str) -> Path:
    try:
        return fixture_root / Path(repository_path).relative_to(FIXTURE_RELATIVE)
    except ValueError as error:
        raise AssertionError(f"fixture path escapes fixed root: {repository_path}") from error


def full_declared_fixture_file(fixture_root: Path, repository_path: str) -> Path:
    """Resolve a v4 closure input through its distinct, fixed fixture root."""
    try:
        return fixture_root / Path(repository_path).relative_to(extractor.FULL_DECLARED_FIXTURE_RELATIVE)
    except ValueError as error:
        raise AssertionError(f"full-declared path escapes fixed root: {repository_path}") from error


def check_dependency_lock(tool_root: Path) -> None:
    lock_path = tool_root / "terminal_assertion_index_extractor.lock.json"
    lock = json.loads(lock_path.read_text())
    assert lock["schema"] == "five_suite_terminal_assertion_extractor_lock/v1"
    assert lock["purpose"] == "maintenance-only static byte-span extraction; not a Loom production dependency"
    assert lock["python"] == {"version": "3.9.6", "implementation": "cpython"}
    assert sys.implementation.name == lock["python"]["implementation"]
    assert f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}" == lock["python"]["version"]
    assert lock["platform"] == {"system": "Darwin", "machine": "arm64"}
    assert platform.system() == lock["platform"]["system"] and platform.machine() == lock["platform"]["machine"]
    assert lock["requirements_path"] == "tools/mermaid/requirements-terminal-assertion-index.txt"
    requirements = tool_root / "requirements-terminal-assertion-index.txt"
    assert digest(requirements.read_bytes()) == lock["requirements_sha256"]
    distribution = {"tree_sitter": "tree-sitter", "tree_sitter_rust": "tree-sitter-rust"}
    for module_name, package in lock["packages"].items():
        assert importlib.metadata.version(distribution[module_name]) == package["version"]
        module = importlib.import_module(module_name)
        site_packages = Path(module.__file__).resolve().parents[1]
        for file in package["files"]:
            raw = (site_packages / file["relative_path"]).read_bytes()
            assert len(raw) == file["bytes"] and digest(raw) == file["sha256"], file["relative_path"]


def check_document(repository_root: Path, fixture_root: Path, document: dict[str, object]) -> tuple[int, int, int]:
    assert document["schema"] == "five_suite_partial_source_assertion_index/v3"
    assert document["status"] == "static_source_index_not_execution_or_port_completion"
    scope = document["scope"]
    assert {key: scope[key] for key in EXPECTED} == EXPECTED
    assert scope["runtime_expansion_rule"] == "A case binding is never represented as a runtime expansion count without a source-specific execution proof."
    assert scope["semantic_assertion_pass_credit"] == 0
    assert scope["runtime_expansion_credit"] == 0
    assert scope["coverage_statement"] == COVERAGE
    inputs = document["inputs"]
    closure_path = inputs["input_closure_path"]
    closure_raw = fixture_file(fixture_root, closure_path).read_bytes()
    assert digest(closure_raw) == inputs["input_closure_sha256"]
    closure = json.loads(closure_raw)
    assert closure["schema"] == "five_suite_assertion_input_closure/v1"
    assert closure["status"] == "static_source_inputs_only_not_execution_or_port_completion"
    assert closure["source_file_closure"] == "The generated index pins every upstream source file separately in source_files; this closure pins only the two reviewed source-ledger inputs."
    assert closure["captured_source_slice_rule"] == {
        "original_kind": "exact_original_test_function_slice",
        "required_equal_fields": [
            "original.byte_start=source.byte_start",
            "original.byte_end=source.byte_end",
        ],
        "required_file_identity": [
            "source.bytes=len(captured_file)",
            "source.sha256=sha256(captured_file)",
            "source.selected_bytes_sha256=sha256(captured_file)",
            "original.sha256=sha256(captured_file)",
        ],
        "accepted_container_count": 533,
        "meaning": "Only these explicitly proven runtime-capture files map local parser intervals back to upstream offsets; equal byte length alone is insufficient.",
    }
    closure_ledgers = {entry["path"]: entry for entry in closure["input_ledgers"]}
    tracked_ledgers = {entry["path"]: entry for entry in inputs["tracked_source_input_ledgers"]}
    assert set(closure_ledgers) == set(tracked_ledgers) == {
        (FIXTURE_RELATIVE / "source-inputs" / "assertion-unit-ledger.json").as_posix(),
        (FIXTURE_RELATIVE / "source-inputs" / "unbound-assertion-source-ledger.json").as_posix(),
    }
    for path, entry in tracked_ledgers.items():
        raw = fixture_file(fixture_root, path).read_bytes()
        assert len(raw) == entry["bytes"] and digest(raw) == entry["sha256"]
        assert closure_ledgers[path] == entry
    assert inputs["source_file_closure"] == "Every referenced source file is pinned in source_files and must be present in the checkout at its listed path and hash."
    assert inputs["extractor"] == {
        "rust": "tree-sitter==0.23.2; tree-sitter-rust==0.21.2",
        "python": "Python stdlib ast exact UTF-8 byte-column verification",
        "preserved_expression": "exact source text only; missing or ambiguous text is unresolved",
    }

    base_ledger = json.loads(fixture_file(fixture_root, (FIXTURE_RELATIVE / "source-inputs" / "assertion-unit-ledger.json").as_posix()).read_text())["units"]
    unbound_ledger = json.loads(fixture_file(fixture_root, (FIXTURE_RELATIVE / "source-inputs" / "unbound-assertion-source-ledger.json").as_posix()).read_text())["units"]
    for unit in [*base_ledger, *unbound_ledger]:
        source = unit["source"]
        raw = (repository_root / source["path"]).read_bytes()
        assert source["bytes"] == len(raw) and source["sha256"] == digest(raw), source["path"]
    captured = 0
    for unit in base_ledger:
        original, source = unit["original_assertion"], unit["source"]
        raw = (repository_root / source["path"]).read_bytes()
        if extractor.captured_exact_source_slice(original, source, raw):
            captured += 1
    assert captured == closure["captured_source_slice_rule"]["accepted_container_count"]

    files = {entry["path"]: entry for entry in document["source_files"]}
    assert len(files) == 861
    for path, entry in files.items():
        raw = (repository_root / path).read_bytes()
        assert len(raw) == entry["bytes"] and digest(raw) == entry["sha256"], path
    containers = {entry["id"] for entry in document["containers"]}
    assert len(containers) == EXPECTED["frozen_source_containers"]
    direct = unresolved = partial = 0
    seen_ids: set[str] = set()
    seen_physical_direct: set[tuple[object, ...]] = set()
    referenced_containers: set[str] = set()
    for record in document["records"]:
        assert record["id"] not in seen_ids
        seen_ids.add(record["id"])
        source = record["source"]
        raw = (repository_root / source["path"]).read_bytes()
        start, end = source["byte_start"], source["byte_end"]
        assert source["path"] in files and digest(raw) == source["file_sha256"]
        assert isinstance(start, int) and isinstance(end, int) and 0 <= start < end <= len(raw)
        selected = raw[start:end]
        if record["record_type"] == "direct_assertion":
            direct += 1
            assert selected == source["expression"].encode("utf-8")
            assert digest(selected) == source["expression_sha256"]
            key = (record["vendor"], source["path"], source["file_sha256"], start, end, record["assertion_form"])
            assert key not in seen_physical_direct
            seen_physical_direct.add(key)
            assert record["source_origins"] and record["source_unit_ids"]
            for origin in record["source_origins"]:
                assert origin["assertion_byte_start"] < origin["assertion_byte_end"]
                assert origin["mapping"] in {"captured_exact_source_slice_plus_local_byte_interval", "captured_file_byte_interval"}
            assert set(record["source_unit_ids"]) <= containers
            referenced_containers.update(record["source_unit_ids"])
        else:
            assert record["record_type"] in {"unresolved_source_container", "unresolved_partial_helper"}
            if record["record_type"] == "unresolved_source_container":
                unresolved += 1
            else:
                partial += 1
            assert digest(selected) == source["selected_sha256"]
            assert record["source_unit_id"] in containers
            referenced_containers.add(record["source_unit_id"])
            helper = record["helper_resolution"]
            assert helper["status"] == "unresolved" and helper["diagnostics"]
    assert direct == EXPECTED["direct_assertion_records"]
    assert unresolved == EXPECTED["unresolved_source_containers"]
    assert partial == EXPECTED["unresolved_partial_helpers"]
    summary_only = sum(
        record["record_type"] == "unresolved_source_container"
        and "summary_label_is_provenance_only_requires_full_original_function" in record["helper_resolution"]["diagnostics"]
        for record in document["records"]
    )
    assert summary_only == EXPECTED["summary_only_provenance_records"]
    assert referenced_containers == containers
    return direct, unresolved, partial


def check_full_declared_document(repository_root: Path, fixture_root: Path, document: dict[str, object]) -> tuple[int, int]:
    """Validate v4 durable paths without reading an audit build-cache source."""
    assert document["schema"] == "five_suite_full_declared_source_assertion_index/v4"
    assert document["status"] == "static_source_index_not_execution_or_semantic_denominator"
    scope = document["scope"]
    assert {key: scope[key] for key in FULL_DECLARED_EXPECTED} == FULL_DECLARED_EXPECTED
    assert scope["semantic_assertion_pass_credit"] == 0
    assert scope["runtime_expansion_credit"] == 0
    assert "Dynamic registrations" in scope["unresolved_boundaries"]

    closure_path = fixture_root / extractor.INPUT_CLOSURE_NAME
    closure_raw = closure_path.read_bytes()
    closure = json.loads(closure_raw)
    assert closure["schema"] == extractor.FULL_DECLARED_CLOSURE_SCHEMA
    closure_inputs = extractor.full_declared_closure_inputs(closure)
    assert len(closure_inputs) == 4
    extractor.full_declared_provenance_lock(fixture_root, closure)
    for path, entry in closure_inputs.items():
        raw = full_declared_fixture_file(fixture_root, path).read_bytes()
        assert len(raw) == entry["bytes"] and digest(raw) == entry["sha256"]

    source_map = extractor.full_declared_checked_json(fixture_root, closure, "FULL_DECLARED_SOURCE_PATH_MAP.v1.json")
    entries = source_map["entries"]
    assert len(entries) == FULL_DECLARED_EXPECTED["complete_declared_sources"]
    by_identity = {entry["r3_declared_path"]: entry for entry in entries}
    assert len(by_identity) == len(entries)
    durable = {entry["durable_source_path"] for entry in entries}
    assert len(durable) == len(entries)
    assert all(not path.startswith("build/") for path in durable)

    manifest_pin = closure["durable_mmdflux_manifest"]
    manifest_raw = (repository_root / manifest_pin["path"]).read_bytes()
    assert len(manifest_raw) == manifest_pin["bytes"] and digest(manifest_raw) == manifest_pin["sha256"]
    manifest = json.loads(manifest_raw)
    assert manifest["schema"] == "mmdflux_full_declared_source_manifest/v1"
    assert manifest["source_count"] == 121 and manifest["source_bytes"] == 3714466
    assert len(manifest["sources"]) == 121
    license_raw = (repository_root / manifest["license"]["durable_path"]).read_bytes()
    assert len(license_raw) == manifest["license"]["bytes"] and digest(license_raw) == manifest["license"]["sha256"]

    files = {entry["source_identity_path"]: entry for entry in document["source_files"]}
    assert len(files) == len(by_identity)
    for identity, mapped in by_identity.items():
        source = files[identity]
        assert source["durable_source_path"] == mapped["durable_source_path"]
        assert source["sha256"] == mapped["source_sha256"] and source["bytes"] == mapped["bytes"]
        raw = (repository_root / source["durable_source_path"]).read_bytes()
        assert len(raw) == source["bytes"] and digest(raw) == source["sha256"]

    containers = {entry["id"]: entry for entry in document["containers"]}
    assert len(containers) == FULL_DECLARED_EXPECTED["test_context_identities"]
    for context in containers.values():
        assert context["source_identity_path"] in files
        assert isinstance(context["identity"], str) and isinstance(context["declaration_kind"], str)
        assert context["parent_identity"] is None or isinstance(context["parent_identity"], str)
        raw = (repository_root / context["durable_source_path"]).read_bytes()
        assert 0 <= context["declaration_byte_start"] < context["declaration_byte_end"] <= len(raw)
        assert context["declaration_byte_start"] <= context["body_byte_start"] < context["body_byte_end"] <= context["declaration_byte_end"]

    seen_records: set[str] = set()
    physical: dict[tuple[str, str, str, int, int], list[dict[str, object]]] = {}
    assertion_counts: dict[str, int] = {context_id: 0 for context_id in containers}
    for record in document["records"]:
        assert record["id"] not in seen_records
        seen_records.add(record["id"])
        assert record["record_type"] == "declared_source_assertion_site"
        assert record["status"] == "static_source_only_not_execution_or_semantic_credit"
        assert record["context_id"] in containers and record["site"]["category"] == "assertion"
        source = record["source"]
        assert source["source_identity_path"] in files
        assert source["durable_source_path"] == files[source["source_identity_path"]]["durable_source_path"]
        raw = (repository_root / source["durable_source_path"]).read_bytes()
        begin, end = source["byte_start"], source["byte_end"]
        context = containers[record["context_id"]]
        assert context["body_byte_start"] <= begin < end <= context["body_byte_end"]
        assert 0 <= begin < end <= len(raw) and digest(raw[begin:end]) == source["selected_sha256"]
        key = (record["vendor"], source["source_identity_path"], source["file_sha256"], begin, end)
        physical.setdefault(key, []).append(record)
        assertion_counts[record["context_id"]] += 1
    assert len(seen_records) == FULL_DECLARED_EXPECTED["assertion_site_entries"]
    assert len(physical) == FULL_DECLARED_EXPECTED["distinct_assertion_source_sites"]
    repeated = 0
    for duplicate_records in physical.values():
        if len(duplicate_records) == 1:
            continue
        assert len(duplicate_records) == 2
        left, right = (containers[record["context_id"]] for record in duplicate_records)
        parent, child = (left, right) if right["parent_identity"] == left["identity"] else (right, left)
        assert parent["vendor"] == child["vendor"] == "mermaid_ascii"
        assert parent["source_identity_path"] == child["source_identity_path"]
        assert parent["declaration_kind"] in {"go_function", "t.Run"}
        assert child["declaration_kind"] == "t.Run"
        assert child["parent_identity"] == parent["identity"]
        assert parent["body_byte_start"] <= child["body_byte_start"] < child["body_byte_end"] <= parent["body_byte_end"]
        repeated += 1
    assert repeated == FULL_DECLARED_EXPECTED["nested_repeat_entries"]

    bindings = document["mmdflux_private_predicate_full_context_bindings"]
    assert len(bindings) == FULL_DECLARED_EXPECTED["mmdflux_private_predicate_full_context_bindings"]
    binding_contexts: set[str] = set()
    for binding in bindings:
        assert binding["matches"] == 1 and binding["context_id"] not in binding_contexts
        binding_contexts.add(binding["context_id"])
        raw = (repository_root / binding["durable_source_path"]).read_bytes()
        assert binding["source_identity_path"] in by_identity
        assert binding["source_sha256"] == digest(raw)
        assert 0 <= binding["byte_start"] < binding["byte_end"] <= len(raw)
        assert binding["status"] == "private_predicate_source_only_not_runtime_or_semantic_credit"
        context = containers[binding["context_id"]]
        assert context["source_identity_path"] == binding["source_identity_path"]
        assert context["identity"] == binding["identity"]
        assert context["declaration_byte_start"] == binding["declaration"]["byte_start"]
        assert context["declaration_byte_end"] == binding["declaration"]["byte_end"] == binding["byte_end"]
        assert context["body_byte_start"] == binding["body"]["byte_start"]
        assert context["body_byte_end"] == binding["body"]["byte_end"]
        assert binding["byte_start"] <= context["declaration_byte_start"]
        assert binding["source_assertion_sites"] == assertion_counts[binding["context_id"]]
    return len(seen_records), len(bindings)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=REPOSITORY_ROOT)
    parser.add_argument("--fixture-root", type=Path)
    parser.add_argument("--tool-root", type=Path, default=Path(__file__).resolve().parent)
    args = parser.parse_args()
    repository_root = args.repo_root.resolve()
    fixture_root = (args.fixture_root or repository_root / FIXTURE_RELATIVE).resolve()
    tool_root = args.tool_root.resolve()
    adversarial.check_rust()
    adversarial.check_python()
    adversarial.check_no_line_fallback()
    check_dependency_lock(tool_root)
    full_declared = extractor.full_declared_mode(fixture_root)
    if full_declared:
        adversarial.check_full_declared_input_closure(fixture_root)
        adversarial.check_full_declared_source_map(repository_root, fixture_root)
        adversarial.check_full_declared_structures(repository_root, fixture_root)
    index_name = extractor.FULL_DECLARED_INDEX_NAME if full_declared else INDEX_NAME
    index_path = fixture_root / index_name
    before = index_path.read_bytes()
    with tempfile.TemporaryDirectory() as directory:
        regenerated = Path(directory) / "fixture"
        shutil.copytree(fixture_root, regenerated)
        subprocess.run(
            [sys.executable, str(tool_root / "build_terminal_assertion_index.py"), "--write-index",
             "--repo-root", str(repository_root), "--fixture-root", str(regenerated)],
            check=True,
            stdout=subprocess.DEVNULL,
        )
        assert (regenerated / index_name).read_bytes() == before, "fixture is not deterministic from its tracked closure"
    document = json.loads(before)
    if full_declared:
        records, private_bindings = check_full_declared_document(repository_root, fixture_root, document)
        print(json.dumps({"index_sha256": digest(before), "records": records,
                          "private_predicate_bindings": private_bindings}, indent=2))
        return
    direct, unresolved, partial = check_document(repository_root, fixture_root, document)
    print(json.dumps({"index_sha256": digest(before), "records": len(document["records"]),
                      "direct": direct, "unresolved": unresolved, "partial_helpers": partial}, indent=2))


if __name__ == "__main__":
    main()
