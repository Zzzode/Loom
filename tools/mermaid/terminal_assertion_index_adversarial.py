#!/usr/bin/env python3
"""Static extractor regressions: no upstream test or renderer is executed."""
from __future__ import annotations

import json
import shutil
import tempfile
from pathlib import Path

import build_terminal_assertion_index as build_ledger


def check_rust() -> None:
    raw = (
        b"// assert_eq!(wrong, wrong)\n"
        b"/* outer /* assert!(wrong) */ still comment */\n"
        b"fn sample<'a>(value: &'a str) {\n"
        b"  let lambda = \"assert_ne!(wrong, wrong)\";\n"
        b"  let raw = r##\"assert_eq!(wrong, wrong)\"##;\n"
        b"  assert_eq!(r#\"(utf8 \\u{03bb})\"#, value);\n"
        b"  debug_assert_ne!(value, \"never\");\n"
        b"}\n"
    )
    direct, diagnostics, helpers = build_ledger.rust_direct(raw, (0, len(raw)))
    assert not diagnostics and not helpers
    expressions = [raw[start:end] for start, end, _ in direct]
    assert expressions == [
        b'assert_eq!(r#"(utf8 \\u{03bb})"#, value)',
        b'debug_assert_ne!(value, "never")',
    ]
    assert all(raw[start:end].startswith(b"assert") or raw[start:end].startswith(b"debug_assert") for start, end, _ in direct)


def check_python() -> None:
    raw = (
        "# λ prefix makes character and byte columns differ\n"
        "assert (\n"
        "    actual\n"
        "    == expected\n"
        "); assert second\n"
    ).encode("utf-8")
    direct, diagnostics, helpers = build_ledger.python_direct(raw, (0, len(raw)))
    assert not diagnostics and not helpers and len(direct) == 2
    expressions = [raw[start:end] for start, end, _ in direct]
    assert expressions == [b"assert (\n    actual\n    == expected\n)", b"assert second"]
    assert direct[0][1] < direct[1][0]


def check_no_line_fallback() -> None:
    direct, diagnostics = build_ledger.exact_preserved_expression(
        {"source": {"line": 1, "text": "expect(missing).toContain('x')"}}, b"const value = 'x';\n"
    )
    assert not direct and diagnostics == ["preserved_assertion_expression_not_found"]
    span, selection, diagnostics = build_ledger.selected_span({}, {}, b"fn x() {}\n")
    assert span is None and selection == "unbounded" and diagnostics == ["selection_span_not_proven_against_captured_bytes"]


def full_declared_inputs(repository_root: Path, fixture_root: Path) -> tuple[dict[str, object], dict[str, object], dict[str, object], dict[str, object], dict[str, object]]:
    closure = json.loads((fixture_root / build_ledger.INPUT_CLOSURE_NAME).read_text())
    r3 = build_ledger.full_declared_checked_json(fixture_root, closure, "FULL_AST_SOURCE_INDEX.r3.json")
    r4 = build_ledger.full_declared_checked_json(fixture_root, closure, "SOURCE_SITE_COUNTS.r4.json")
    recovery = build_ledger.full_declared_checked_json(fixture_root, closure, "SOURCE_RECOVERY.r3.json")
    source_map = build_ledger.full_declared_checked_json(fixture_root, closure, "FULL_DECLARED_SOURCE_PATH_MAP.v1.json")
    return closure, r3, r4, recovery, source_map


def check_full_declared_source_map(repository_root: Path, fixture_root: Path) -> None:
    """Exercise map rules directly, after bypassing only immutable hash pins."""
    _, r3, _, recovery, original_map = full_declared_inputs(repository_root, fixture_root)

    def must_reject(name: str, mutate: object) -> None:
        candidate_map = json.loads(json.dumps(original_map))
        mutate(candidate_map)
        try:
            build_ledger.full_declared_validated_source_map(repository_root, r3, recovery, candidate_map)
        except ValueError:
            return
        raise AssertionError(f"corrupt full-declared map was accepted: {name}")

    must_reject("missing", lambda value: value["entries"].pop())
    must_reject("duplicate_identity", lambda value: value["entries"][1].update({
        "r3_declared_path": value["entries"][0]["r3_declared_path"]
    }))
    must_reject("undeclared_identity", lambda value: value["entries"][0].update({
        "r3_declared_path": "tests/fixtures/mermaid_terminal/not-declared.rs"
    }))
    must_reject("identity_digest", lambda value: value["entries"][0].update({
        "source_sha256": "0" * 64
    }))
    must_reject("cache_path", lambda value: value["entries"][0].update({
        "durable_source_path": "build/not-a-durable-source.rs"
    }))

    locations = build_ledger.full_declared_validated_source_map(repository_root, r3, recovery, original_map)
    with tempfile.TemporaryDirectory() as temporary:
        shadow_root = Path(temporary) / "repository"
        for location in locations.values():
            source = repository_root / location.durable_path
            target = shadow_root / location.durable_path
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        missing = next(location for location in locations.values() if location.storage_kind == "mmdflux_recovered_full_source")
        (shadow_root / missing.durable_path).unlink()
        try:
            build_ledger.full_declared_validated_source_map(shadow_root, r3, recovery, original_map)
        except ValueError:
            pass
        else:
            raise AssertionError("missing durable source was accepted")


def check_full_declared_structures(repository_root: Path, fixture_root: Path) -> None:
    """Exercise range, private-binding, and nested-repeat checks beneath pins."""
    _, r3, r4, recovery, source_map = full_declared_inputs(repository_root, fixture_root)
    locations = build_ledger.full_declared_validated_source_map(repository_root, r3, recovery, source_map)

    def must_reject(name: str, mutate: object) -> None:
        altered = json.loads(json.dumps(r3))
        mutate(altered)
        try:
            build_ledger.full_declared_structures(repository_root, altered, r4, locations)
        except ValueError:
            return
        raise AssertionError(f"corrupt full-declared structure was accepted: {name}")

    must_reject("invalid_context_range", lambda value: value["files"]["mmdflux"][0]["test_units"][0]["body"].update({"byte_end": 2 ** 31}))

    def break_assertion_site_range(value: object) -> None:
        for file_entry in value["files"]["mmdflux"]:
            for unit in file_entry.get("test_units", []):
                for site in unit.get("sites", []):
                    if site.get("category") == "assertion":
                        site["byte_end"] = 2 ** 31
                        return
        raise AssertionError("fixture lacks a mmdflux assertion site")

    must_reject("invalid_assertion_site_range", break_assertion_site_range)
    must_reject("private_zero", lambda value: value["mmdflux_private_predicate_full_context_bindings"][0].update({"matches": 0}))
    must_reject("private_multiple", lambda value: value["mmdflux_private_predicate_full_context_bindings"][0].update({"matches": 2}))
    must_reject("private_wrong_function", lambda value: value["mmdflux_private_predicate_full_context_bindings"][0].update({"identity": "compiler_emits_edges"}))

    def break_nested_parent(value: object) -> None:
        for file_entry in value["files"]["mermaid_ascii"]:
            for unit in file_entry.get("test_units", []):
                if unit.get("parent_identity") is not None:
                    unit["parent_identity"] = None
                    return
        raise AssertionError("fixture lacks a nested mermaid_ascii context")

    must_reject("non_nested_duplicate", break_nested_parent)

    def break_nested_subtest_kind(value: object) -> None:
        for file_entry in value["files"]["mermaid_ascii"]:
            for unit in file_entry.get("test_units", []):
                if unit.get("parent_identity") is not None:
                    unit["declaration_kind"] = "go_function"
                    return
        raise AssertionError("fixture lacks a nested mermaid_ascii subtest")

    must_reject("wrong_subtest_kind", break_nested_subtest_kind)


def check_full_declared_input_closure(fixture_root: Path) -> None:
    """Reject malformed or self-consistently forged v4 input provenance."""
    closure = json.loads((fixture_root / build_ledger.INPUT_CLOSURE_NAME).read_text())
    assert build_ledger.full_declared_closure_inputs(closure) == build_ledger.FULL_DECLARED_APPROVED_INPUTS
    malformed = json.loads(json.dumps(closure))
    malformed["inputs"] = malformed["inputs"][:-1]
    try:
        build_ledger.full_declared_closure_inputs(malformed)
    except ValueError:
        pass
    else:
        raise AssertionError("malformed v4 input closure was accepted")

    with tempfile.TemporaryDirectory() as temporary:
        candidate_root = Path(temporary) / "fixture"
        shutil.copytree(fixture_root / "source-inputs", candidate_root / "source-inputs")
        copied_closure = json.loads(json.dumps(closure))
        lock = json.loads((fixture_root / build_ledger.FULL_DECLARED_DATA_LOCK_NAME).read_text())
        r3_path = candidate_root / "source-inputs" / "FULL_AST_SOURCE_INDEX.r3.json"
        r3 = json.loads(r3_path.read_text())
        r3["forged_self_consistent_input"] = True
        raw = (json.dumps(r3, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
        r3_path.write_bytes(raw)
        for entry in copied_closure["inputs"]:
            if entry["path"].endswith("/FULL_AST_SOURCE_INDEX.r3.json"):
                entry["bytes"] = len(raw)
                entry["sha256"] = build_ledger.digest(raw)
        lock["fixture_inputs"] = [entry for _, entry in sorted({entry["path"]: entry for entry in copied_closure["inputs"]}.items())]
        lock_raw = (json.dumps(lock, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
        (candidate_root / build_ledger.FULL_DECLARED_DATA_LOCK_NAME).write_bytes(lock_raw)
        copied_closure["provenance_lock"]["bytes"] = len(lock_raw)
        copied_closure["provenance_lock"]["sha256"] = build_ledger.digest(lock_raw)
        closure_path = candidate_root / build_ledger.INPUT_CLOSURE_NAME
        closure_path.write_text(json.dumps(copied_closure, indent=2, ensure_ascii=False) + "\n")
        forged_r3 = json.loads(r3_path.read_text())
        forged_lock = json.loads((candidate_root / build_ledger.FULL_DECLARED_DATA_LOCK_NAME).read_text())
        forged_closure = json.loads(closure_path.read_text())
        assert forged_r3["forged_self_consistent_input"] is True
        assert forged_lock["fixture_inputs"] == lock["fixture_inputs"]
        assert forged_closure["provenance_lock"]["sha256"] == build_ledger.digest(lock_raw)
        try:
            build_ledger.full_declared_closure_inputs(forged_closure)
        except ValueError:
            pass
        else:
            raise AssertionError("self-consistent forged v4 closure was accepted")


if __name__ == "__main__":
    check_rust()
    check_python()
    check_no_line_fallback()
