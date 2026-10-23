#!/usr/bin/env python3
"""Build a partial, source-addressed five-suite assertion index.

Every direct assertion range is a byte interval produced by a language parser or
an exact preserved assertion expression.  Unknown helpers, syntax errors, and
unbounded selection spans remain explicit unresolved records.  This tool never
turns a case binding into a runtime expansion or port-completeness claim.
"""
from __future__ import annotations

import argparse
import ast
import bisect
import hashlib
import json
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

try:
    from tree_sitter import Language, Parser
    import tree_sitter_rust
except ImportError:
    Language = None
    Parser = None
    tree_sitter_rust = None

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
FIXTURE_RELATIVE = Path("tests/fixtures/mermaid_terminal/assertion_index/v3")
INDEX_NAME = "five_suite_partial_source_assertion_index.json"
INPUT_CLOSURE_NAME = "input-closure.json"
INPUT_LEDGER_NAMES = ("assertion-unit-ledger.json", "unbound-assertion-source-ledger.json")
ROOT: Path
BASE: list[dict[str, Any]]
UNBOUND: list[dict[str, Any]]
INPUT_CLOSURE: dict[str, Any]
VENDORS = {"beautiful_mermaid", "mermaid_ascii", "merman", "mmdflux", "termaid"}
RUST_PARSER: Any | None = None
RUST_TREES: dict[str, Any] = {}


def rust_parser() -> Any:
    """Create the v3 Rust parser only if a v3 path actually needs it."""
    global RUST_PARSER
    if RUST_PARSER is None:
        if Language is None or Parser is None or tree_sitter_rust is None:
            raise SystemExit(
                "tree-sitter-rust is required for the v3 parser path; create a maintenance venv and install "
                "tools/mermaid/requirements-terminal-assertion-index.txt"
            )
        RUST_PARSER = Parser(Language(tree_sitter_rust.language()))
    return RUST_PARSER
RUST_ASSERT_MACROS = {
    "assert",
    "assert_eq",
    "assert_ne",
    "assert_matches",
    "debug_assert",
    "debug_assert_eq",
    "debug_assert_ne",
}


def digest(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def repository_relative(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError as error:
        raise ValueError(f"Path is outside the repository: {path}") from error


def configure(repository_root: Path, fixture_root: Path) -> None:
    """Load only the tracked closure used to create the proposed fixture."""
    global ROOT, BASE, UNBOUND, INPUT_CLOSURE
    ROOT = repository_root.resolve()
    closure_path = fixture_root / INPUT_CLOSURE_NAME
    closure_raw = closure_path.read_bytes()
    closure = json.loads(closure_raw)
    if closure.get("schema") != "five_suite_assertion_input_closure/v1":
        raise ValueError("unexpected input-closure schema")
    entries = closure.get("input_ledgers")
    if not isinstance(entries, list) or len(entries) != len(INPUT_LEDGER_NAMES):
        raise ValueError("input closure must contain exactly the two reviewed ledgers")
    by_name: dict[str, dict[str, Any]] = {}
    for entry in entries:
        path = entry.get("path")
        if not isinstance(path, str) or Path(path).name not in INPUT_LEDGER_NAMES:
            raise ValueError("input closure has an unexpected ledger path")
        if path in by_name:
            raise ValueError("input closure has a duplicate ledger path")
        relative = Path(path)
        try:
            within_fixture = relative.relative_to(FIXTURE_RELATIVE)
        except ValueError as error:
            raise ValueError("input closure ledger is outside the fixed fixture root") from error
        tracked_path = fixture_root / within_fixture
        raw = tracked_path.read_bytes()
        if len(raw) != entry.get("bytes") or digest(raw) != entry.get("sha256"):
            raise ValueError(f"input ledger digest mismatch: {path}")
        by_name[path] = entry
    expected_paths = {
        (FIXTURE_RELATIVE / "source-inputs" / name).as_posix()
        for name in INPUT_LEDGER_NAMES
    }
    if set(by_name) != expected_paths:
        raise ValueError("input closure paths are not the fixed assertion-index fixture paths")
    BASE = json.loads((fixture_root / "source-inputs" / INPUT_LEDGER_NAMES[0]).read_text())["units"]
    UNBOUND = json.loads((fixture_root / "source-inputs" / INPUT_LEDGER_NAMES[1]).read_text())["units"]
    INPUT_CLOSURE = closure


def line_byte_starts(raw: bytes) -> list[int]:
    starts = [0]
    starts.extend(index + 1 for index, value in enumerate(raw) if value == 10)
    return starts


def line_for(starts: list[int], byte: int) -> int:
    return bisect.bisect_right(starts, byte)


def language(path: str) -> str:
    return {".rs": "rust", ".py": "python", ".ts": "typescript", ".go": "go"}.get(
        Path(path).suffix, "unknown"
    )


def source_file(source: dict[str, Any]) -> tuple[bytes, Path]:
    """Read a ledger-declared source file without silently refreshing its SHA."""
    path = ROOT / source["path"]
    raw = path.read_bytes()
    if source.get("bytes") != len(raw) or source.get("sha256") != digest(raw):
        raise ValueError(f"ledger source identity mismatch: {source['path']}")
    return raw, path


def captured_exact_source_slice(original: dict[str, Any], source: dict[str, Any], raw: bytes) -> bool:
    """Prove that the fixture file is exactly one captured upstream test slice."""
    source_start, source_end = source.get("byte_start"), source.get("byte_end")
    original_start, original_end = original.get("byte_start"), original.get("byte_end")
    return (
        original.get("kind") == "exact_original_test_function_slice"
        and isinstance(source_start, int)
        and isinstance(source_end, int)
        and isinstance(original_start, int)
        and isinstance(original_end, int)
        and source_start == original_start
        and source_end == original_end
        and source_end - source_start == len(raw)
        and source.get("bytes") == len(raw)
        and source.get("sha256") == digest(raw)
        and source.get("selected_bytes_sha256") == digest(raw)
        and original.get("sha256") == digest(raw)
    )


def selected_span(
    original: dict[str, Any], source: dict[str, Any], raw: bytes
) -> tuple[tuple[int, int] | None, str, list[str]]:
    """Return only a source interval that is independently bounded in raw bytes."""
    begin = original.get("byte_start", source.get("byte_start"))
    end = original.get("byte_end", source.get("byte_end"))
    if isinstance(begin, int) and isinstance(end, int):
        if 0 <= begin < end <= len(raw):
            return (begin, end), "exact_byte_range_in_captured_file", []
        if captured_exact_source_slice(original, source, raw):
            return (0, len(raw)), "captured_exact_source_slice", []
    # mmdflux assertion_sources are a physically pinned source fragment.  Their
    # upstream line numbers do not index the fragment; slice_line_* does.
    slice_first = original.get("slice_line_start", source.get("slice_line_start"))
    slice_last = original.get("slice_line_end", source.get("slice_line_end"))
    starts = line_byte_starts(raw)
    if isinstance(slice_first, int) and isinstance(slice_last, int) and 1 <= slice_first <= slice_last <= len(starts):
        end_at = len(raw) if slice_last == len(starts) else starts[slice_last]
        return (starts[slice_first - 1], end_at), "exact_fragment_slice_line_range", []
    first = original.get("line_start", source.get("line_start"))
    last = original.get("line_end", source.get("line_end"))
    starts = line_byte_starts(raw)
    if isinstance(first, int) and isinstance(last, int) and 1 <= first <= last <= len(starts):
        end_at = len(raw) if last == len(starts) else starts[last]
        return (starts[first - 1], end_at), "exact_line_range_in_captured_file", []
    return None, "unbounded", ["selection_span_not_proven_against_captured_bytes"]


def walk(node: Any) -> Iterable[Any]:
    yield node
    for child in node.children:
        yield from walk(child)


def rust_name(raw: bytes, node: Any) -> str:
    macro = node.child_by_field_name("macro")
    if macro is None:
        return ""
    name = raw[macro.start_byte : macro.end_byte].decode("utf-8", errors="strict")
    return name.rsplit("::", 1)[-1]


def rust_direct(
    raw: bytes, span: tuple[int, int]
) -> tuple[list[tuple[int, int, str]], list[str], list[str]]:
    """Use Tree-sitter byte coordinates; never derive offsets from decoded text."""
    raw_hash = digest(raw)
    tree = RUST_TREES.get(raw_hash)
    if tree is None:
        tree = rust_parser().parse(raw)
        RUST_TREES[raw_hash] = tree
    direct: list[tuple[int, int, str]] = []
    diagnostics: list[str] = []
    helpers: set[str] = set()
    if tree.root_node.has_error:
        diagnostics.append("rust_tree_sitter_syntax_error")
    selected_start, selected_end = span
    for node in walk(tree.root_node):
        if node.type == "macro_invocation":
            name = rust_name(raw, node)
            candidate = name.startswith(("assert", "expect", "check"))
            inside = selected_start <= node.start_byte and node.end_byte <= selected_end
            overlaps = node.start_byte < selected_end and selected_start < node.end_byte
            if name in RUST_ASSERT_MACROS and inside:
                direct.append((node.start_byte, node.end_byte, f"rust_macro:{name}"))
            elif name in RUST_ASSERT_MACROS and overlaps:
                diagnostics.append("rust_assertion_crosses_selection_boundary")
            elif candidate and overlaps:
                helpers.add(name)
        elif node.type == "call_expression":
            function = node.child_by_field_name("function")
            if function is not None:
                name = raw[function.start_byte : function.end_byte].decode("utf-8", errors="strict")
                leaf = name.rsplit("::", 1)[-1]
                if leaf.startswith(("assert_", "expect_", "check_")) and (
                    node.start_byte < selected_end and selected_start < node.end_byte
                ):
                    helpers.add(leaf)
    return direct, sorted(set(diagnostics)), sorted(helpers)


def python_direct(
    raw: bytes, span: tuple[int, int]
) -> tuple[list[tuple[int, int, str]], list[str], list[str]]:
    """AST ranges are UTF-8 byte columns; verify each slice against ast source text."""
    try:
        text = raw.decode("utf-8", errors="strict")
    except UnicodeDecodeError:
        return [], ["python_source_not_utf8"], []
    try:
        tree = ast.parse(text)
    except SyntaxError as error:
        return [], [f"python_ast_syntax_error:{error.msg}"], []
    starts = line_byte_starts(raw)
    selected_start, selected_end = span
    direct: list[tuple[int, int, str]] = []
    helpers: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Assert):
            if node.end_lineno is None or node.end_col_offset is None:
                continue
            begin = starts[node.lineno - 1] + node.col_offset
            end = starts[node.end_lineno - 1] + node.end_col_offset
            expected = ast.get_source_segment(text, node)
            if expected is None or raw[begin:end] != expected.encode("utf-8"):
                return [], ["python_ast_utf8_byte_span_mismatch"], []
            if selected_start <= begin and end <= selected_end:
                direct.append((begin, end, "python_assert"))
            elif begin < selected_end and selected_start < end:
                helpers.add("assert_crosses_selection_boundary")
        elif isinstance(node, ast.Call):
            if node.end_lineno is None or node.end_col_offset is None:
                continue
            begin = starts[node.lineno - 1] + node.col_offset
            end = starts[node.end_lineno - 1] + node.end_col_offset
            if not (begin < selected_end and selected_start < end):
                continue
            function = node.func
            if isinstance(function, ast.Name):
                name = function.id
            elif isinstance(function, ast.Attribute):
                name = function.attr
            else:
                continue
            if name.startswith(("assert_", "expect_", "check_")):
                helpers.add(name)
    return direct, [], sorted(helpers)


def exact_preserved_expression(
    original: dict[str, Any], raw: bytes
) -> tuple[list[tuple[int, int, str]], list[str]]:
    text = original.get("source", {}).get("text")
    line = original.get("source", {}).get("line")
    if not isinstance(text, str):
        return [], ["preserved_assertion_expression_missing"]
    needle = text.encode("utf-8")
    candidates: list[int] = []
    at = raw.find(needle)
    while at >= 0:
        candidates.append(at)
        at = raw.find(needle, at + 1)
    if not candidates:
        return [], ["preserved_assertion_expression_not_found"]
    starts = line_byte_starts(raw)
    if isinstance(line, int):
        on_declared_line = [at for at in candidates if line_for(starts, at) == line]
        if len(on_declared_line) == 1:
            at = on_declared_line[0]
            return [(at, at + len(needle), "preserved_exact_expression")], []
        if len(on_declared_line) > 1:
            return [], ["preserved_assertion_expression_ambiguous_on_declared_line"]
    if len(candidates) == 1:
        at = candidates[0]
        return [(at, at + len(needle), "preserved_exact_expression_unique_global")], []
    return [], ["preserved_assertion_expression_ambiguous"]


def upstream_origin(
    original: dict[str, Any], source: dict[str, Any], raw: bytes, begin: int, end: int) -> dict[str, Any]:
    source_begin = source.get("byte_start")
    source_end = source.get("byte_end")
    origin_path = original.get("upstream_path", original.get("path", original.get("source", {}).get("path")))
    if captured_exact_source_slice(original, source, raw):
        return {
            "path": origin_path,
            "captured_slice_byte_start": source_begin,
            "captured_slice_byte_end": source_end,
            "assertion_byte_start": source_begin + begin,
            "assertion_byte_end": source_begin + end,
            "mapping": "captured_exact_source_slice_plus_local_byte_interval",
        }
    return {
        "path": origin_path,
        "assertion_byte_start": begin,
        "assertion_byte_end": end,
        "mapping": "captured_file_byte_interval",
    }


def unresolved_record(
    *,
    unit_id: str,
    vendor: str,
    lang: str,
    source: dict[str, Any],
    raw: bytes,
    span: tuple[int, int] | None,
    selection: str,
    cases: list[str],
    diagnostics: list[str],
    helper_names: list[str],
    record_type: str,
    test_name: str | None,
) -> dict[str, Any]:
    begin, end = span if span is not None else (0, len(raw))
    starts = line_byte_starts(raw)
    selected = raw[begin:end]
    record_id = digest(
        (unit_id + "\0" + record_type + "\0" + digest(selected) + "\0" + "\x1f".join(diagnostics + helper_names)).encode()
    )
    return {
        "id": record_id,
        "record_type": record_type,
        "source_unit_id": unit_id,
        "vendor": vendor,
        "language": lang,
        "source": {
            "path": source["path"],
            "file_sha256": digest(raw),
            "byte_start": begin,
            "byte_end": end,
            "line_start": line_for(starts, begin),
            "line_end": line_for(starts, max(begin, end - 1)),
            "selected_sha256": digest(selected),
            "selection": selection,
            "test_name": test_name,
        },
        "case_bindings": cases,
        "runtime_expansion": {"status": "not_derivable_from_static_source_and_case_binding"},
        "helper_resolution": {
            "status": "unresolved",
            "names": helper_names,
            "diagnostics": diagnostics,
        },
        "native_status": "unported_or_unverified; no execution claim",
    }


def make_container(vendor: str, unit: dict[str, Any], unbound: bool) -> tuple[list[dict[str, Any]], tuple[str, str, int]]:
    original = unit["original"] if unbound else unit["original_assertion"]
    source = unit["source"]
    unit_id = "unbound:" + source["sha256"] if unbound else unit["id"]
    cases = [] if unbound else unit["case_ids"]
    raw, path = source_file(source)
    lang = language(str(path))
    test_name = original.get("test_name") or original.get("source", {}).get("test_name")
    summary_only = not unbound and unit["kind"] in {"contains", "not_contains"}
    if summary_only:
        # The audit's contains/not_contains labels are provenance summaries, not
        # portable assertion semantics.  Preserve a unique expression interval
        # when available, but never admit it as a direct semantic assertion.
        provenance, more = exact_preserved_expression(original, raw)
        if len(provenance) == 1:
            begin, end, _ = provenance[0]
            span, selection = (begin, end), "exact_preserved_expression_provenance_only"
        else:
            span, selection = (0, len(raw)), "whole_file_summary_provenance_only"
        selection_diagnostics = [
            "summary_label_is_provenance_only_requires_full_original_function",
            *more,
        ]
    else:
        span, selection, selection_diagnostics = selected_span(original, source, raw)
    direct: list[tuple[int, int, str]] = []
    diagnostics = list(selection_diagnostics)
    helpers: list[str] = []
    if span is not None:
        if summary_only:
            pass
        elif lang == "rust":
            direct, more, helpers = rust_direct(raw, span)
            diagnostics.extend(more)
        elif lang == "python":
            direct, more, helpers = python_direct(raw, span)
            diagnostics.extend(more)
        else:
            diagnostics.append("no_exact_parser_or_preserved_expression_for_language")
    records: list[dict[str, Any]] = []
    starts = line_byte_starts(raw)
    for begin, end, form in direct:
        expression = raw[begin:end]
        record_id = digest((unit_id + "\0" + str(begin) + "\0" + str(end) + "\0" + digest(expression)).encode())
        records.append(
            {
                "id": record_id,
                "record_type": "direct_assertion",
                "source_unit_id": unit_id,
                "vendor": vendor,
                "language": lang,
                "assertion_form": form,
                "source": {
                    "path": source["path"],
                    "file_sha256": digest(raw),
                    "byte_start": begin,
                    "byte_end": end,
                    "line_start": line_for(starts, begin),
                    "line_end": line_for(starts, max(begin, end - 1)),
                    "expression_sha256": digest(expression),
                    "expression": expression.decode("utf-8", errors="strict"),
                    "selection": selection,
                },
                "source_origins": [upstream_origin(original, source, raw, begin, end)],
                "case_bindings": cases,
                "runtime_expansion": {"status": "not_derivable_from_static_source_and_case_binding"},
                "native_status": "unported_or_unverified; no execution claim",
            }
        )
    # Never let direct macros hide unknown helper calls or a parse/boundary failure.
    unresolved_diagnostics = sorted(set(diagnostics))
    if span is None or unresolved_diagnostics:
        records.append(
            unresolved_record(
                unit_id=unit_id,
                vendor=vendor,
                lang=lang,
                source=source,
                raw=raw,
                span=span,
                selection=selection,
                cases=cases,
                diagnostics=unresolved_diagnostics or ["no_direct_assertion_extracted"],
                helper_names=helpers,
                record_type="unresolved_source_container",
                test_name=test_name,
            )
        )
    if helpers:
        records.append(
            unresolved_record(
                unit_id=unit_id,
                vendor=vendor,
                lang=lang,
                source=source,
                raw=raw,
                span=span,
                selection=selection,
                cases=cases,
                diagnostics=["helper_assertion_semantics_not_resolved"],
                helper_names=helpers,
                record_type="unresolved_partial_helper",
                test_name=test_name,
            )
        )
    if not direct and not (span is None or unresolved_diagnostics):
        records.append(
            unresolved_record(
                unit_id=unit_id,
                vendor=vendor,
                lang=lang,
                source=source,
                raw=raw,
                span=span,
                selection=selection,
                cases=cases,
                diagnostics=["no_direct_language_assertion_extracted"],
                helper_names=helpers,
                record_type="unresolved_source_container",
                test_name=test_name,
            )
        )
    return records, (source["path"], digest(raw), len(raw))


def build(repository_root: Path, fixture_root: Path) -> dict[str, Any]:
    configure(repository_root, fixture_root)
    records: list[dict[str, Any]] = []
    files: dict[str, dict[str, Any]] = {}
    containers: list[dict[str, Any]] = []
    units = [(unit["vendor"], unit, False) for unit in BASE if unit["vendor"] in VENDORS]
    units += [("merman", unit, True) for unit in UNBOUND]
    for vendor, unit, is_unbound in units:
        extracted, file_entry = make_container(vendor, unit, is_unbound)
        records.extend(extracted)
        files[file_entry[0]] = {"sha256": file_entry[1], "bytes": file_entry[2]}
        source = unit["source"]
        containers.append(
            {
                "id": "unbound:" + source["sha256"] if is_unbound else unit["id"],
                "vendor": vendor,
                "source_path": source["path"],
                "source_sha256": file_entry[1],
                "case_bindings": [] if is_unbound else unit["case_ids"],
                "kind": "unbound_source_slice" if is_unbound else unit["kind"],
            }
        )
    # Direct callsites can belong to multiple source containers.  Preserve every
    # container/binding and merge only byte-identical physical assertions.
    merged: dict[tuple[Any, ...], dict[str, Any]] = {}
    final: list[dict[str, Any]] = []
    for record in records:
        if record["record_type"] != "direct_assertion":
            final.append(record)
            continue
        source = record["source"]
        key = (
            record["vendor"],
            source["path"],
            source["file_sha256"],
            source["byte_start"],
            source["byte_end"],
            record["assertion_form"],
        )
        previous = merged.get(key)
        if previous is None:
            record["source_unit_ids"] = [record.pop("source_unit_id")]
            merged[key] = record
        else:
            previous["source_unit_ids"].append(record["source_unit_id"])
            previous["source_unit_ids"].sort()
            previous["case_bindings"] = sorted(set(previous["case_bindings"]) | set(record["case_bindings"]))
            origins = previous["source_origins"] + record["source_origins"]
            unique = {json.dumps(origin, sort_keys=True, separators=(",", ":")): origin for origin in origins}
            previous["source_origins"] = [unique[key] for key in sorted(unique)]
    final.extend(merged.values())
    final.sort(key=lambda item: (item["vendor"], item["source"]["path"], item["source"]["byte_start"], item["id"]))
    assert len({item["id"] for item in final}) == len(final)
    assert len({item["id"] for item in containers}) == len(containers)
    closure_relative = (FIXTURE_RELATIVE / INPUT_CLOSURE_NAME).as_posix()
    closure_raw = (fixture_root / INPUT_CLOSURE_NAME).read_bytes()
    input_rows = []
    for entry in sorted(INPUT_CLOSURE["input_ledgers"], key=lambda item: item["path"]):
        path = str(entry["path"])
        raw = (fixture_root / Path(path).relative_to(FIXTURE_RELATIVE)).read_bytes()
        input_rows.append({"path": path, "sha256": digest(raw), "bytes": len(raw)})
    return {
        "schema": "five_suite_partial_source_assertion_index/v3",
        "status": "static_source_index_not_execution_or_port_completion",
        "scope": {
            "frozen_source_containers": len(containers),
            "direct_assertion_records": sum(item["record_type"] == "direct_assertion" for item in final),
            "unresolved_source_containers": sum(item["record_type"] == "unresolved_source_container" for item in final),
            "unresolved_partial_helpers": sum(item["record_type"] == "unresolved_partial_helper" for item in final),
            "summary_only_provenance_records": sum(
                item["record_type"] == "unresolved_source_container"
                and "summary_label_is_provenance_only_requires_full_original_function" in item["helper_resolution"]["diagnostics"]
                for item in final
            ),
            "runtime_expansion_rule": "A case binding is never represented as a runtime expansion count without a source-specific execution proof.",
            "semantic_assertion_pass_credit": 0,
            "runtime_expansion_credit": 0,
            "coverage_statement": "Partial index of the frozen input ledgers only; it is not a five-suite semantic-assertion denominator or a port-completeness claim.",
        },
        "inputs": {
            "input_closure_path": closure_relative,
            "input_closure_sha256": digest(closure_raw),
            "tracked_source_input_ledgers": input_rows,
            "source_file_closure": "Every referenced source file is pinned in source_files and must be present in the checkout at its listed path and hash.",
            "extractor": {
                "rust": "tree-sitter==0.23.2; tree-sitter-rust==0.21.2",
                "python": "Python stdlib ast exact UTF-8 byte-column verification",
                "preserved_expression": "exact source text only; missing or ambiguous text is unresolved",
            },
        },
        "source_files": [{"path": path, **value} for path, value in sorted(files.items())],
        "containers": containers,
        "records": final,
        "summary": {
            "by_vendor": dict(Counter(item["vendor"] for item in final)),
            "by_language": dict(Counter(item["language"] for item in final)),
            "by_record_type": dict(Counter(item["record_type"] for item in final)),
        },
    }



FULL_DECLARED_FIXTURE_RELATIVE = Path("tests/fixtures/mermaid_terminal/assertion_index/v4_full_declared")
FULL_DECLARED_INDEX_NAME = "five_suite_full_declared_source_assertion_index.json"
FULL_DECLARED_CLOSURE_SCHEMA = "five_suite_full_declared_input_closure/v1"
FULL_DECLARED_SOURCE_MAP_SCHEMA = "five_suite_full_declared_source_path_map/v1"
FULL_DECLARED_DATA_LOCK_NAME = "full-declared-input.lock.json"
FULL_DECLARED_INPUTS = (
    "FULL_AST_SOURCE_INDEX.r3.json",
    "SOURCE_SITE_COUNTS.r4.json",
    "SOURCE_RECOVERY.r3.json",
    "FULL_DECLARED_SOURCE_PATH_MAP.v1.json",
)
FULL_DECLARED_APPROVED_INPUTS = {
    (FULL_DECLARED_FIXTURE_RELATIVE / "source-inputs" / "FULL_AST_SOURCE_INDEX.r3.json").as_posix(): {
        "path": (FULL_DECLARED_FIXTURE_RELATIVE / "source-inputs" / "FULL_AST_SOURCE_INDEX.r3.json").as_posix(),
        "sha256": "2984e7e788624f5a3723e128d9d9eb50424e6ded6ee920e408be4c861268732e",
        "bytes": 16534382,
    },
    (FULL_DECLARED_FIXTURE_RELATIVE / "source-inputs" / "SOURCE_SITE_COUNTS.r4.json").as_posix(): {
        "path": (FULL_DECLARED_FIXTURE_RELATIVE / "source-inputs" / "SOURCE_SITE_COUNTS.r4.json").as_posix(),
        "sha256": "7592d8c215f0f280dbe10e7189f6180b613c172f8f74d099d9815a44f866bada",
        "bytes": 88986,
    },
    (FULL_DECLARED_FIXTURE_RELATIVE / "source-inputs" / "SOURCE_RECOVERY.r3.json").as_posix(): {
        "path": (FULL_DECLARED_FIXTURE_RELATIVE / "source-inputs" / "SOURCE_RECOVERY.r3.json").as_posix(),
        "sha256": "faeb6300a5b9192dfda3529765ef6ec14bb4109de8322428d7987aebca9b612d",
        "bytes": 989034,
    },
    (FULL_DECLARED_FIXTURE_RELATIVE / "source-inputs" / "FULL_DECLARED_SOURCE_PATH_MAP.v1.json").as_posix(): {
        "path": (FULL_DECLARED_FIXTURE_RELATIVE / "source-inputs" / "FULL_DECLARED_SOURCE_PATH_MAP.v1.json").as_posix(),
        "sha256": "e08d5fdc2d2e2179c2f5cbb0c1b9e140691c01ea8fca32888a23a469e8294eab",
        "bytes": 220267,
    },
}
FULL_DECLARED_APPROVED_AUDIT_PACKAGES = [
    {
        "id": "full_ast_all_declared_r3",
        "path": "build/mermaid-renderer-research-20261020/full-obligation-audit/five-suite-full-ast-all-declared-internal-root-r3/HASHES.json",
        "sha256": "12792024a47ccaac160e8b5273c985acde2b97450d04f321855a85d9c2879589",
        "bytes": 543,
    },
    {
        "id": "full_ast_site_identity_r4",
        "path": "build/mermaid-renderer-research-20261020/full-obligation-audit/five-suite-full-ast-site-identity-root-r4/HASHES.json",
        "sha256": "636168d0587e691d6e60c374428a1d46a6052561c7457ff642f47502d2ef93a7",
        "bytes": 279,
    },
    {
        "id": "mmdflux_full_source_recovery_r3",
        "path": "build/mermaid-renderer-research-20261020/full-obligation-audit/five-suite-mmdflux-all-internal-source-root-r3/HASHES.json",
        "sha256": "ebf0d67d9b1140c4e71da4ebff91b7d0b6e34991f545421037b75efb1537703e",
        "bytes": 17080,
    },
]
FULL_DECLARED_APPROVED_DURABLE_MANIFEST = {
    "path": "tests/fixtures/mermaid_terminal/mmdflux/full_declared_sources/c7405b64865938b2df4d33d26389d51691409913/SOURCE_MANIFEST.v1.json",
    "sha256": "e1cef48b111f3b85e6e81dda27d31eb6d23d4fd0b7220a1644c5bb5fc049f444",
    "bytes": 67586,
}
FULL_DECLARED_APPROVED_LOCK = {
    "path": (FULL_DECLARED_FIXTURE_RELATIVE / FULL_DECLARED_DATA_LOCK_NAME).as_posix(),
    "sha256": "95c0f19a08b247a500669ce734af07ece9a7f624a562d5047a94889497ef8edb",
    "bytes": 2433,
}


@dataclass(frozen=True)
class FullDeclaredSource:
    vendor: str
    identity_path: str
    durable_path: str
    sha256: str
    bytes: int
    storage_kind: str
    upstream_path: str | None = None


def full_declared_fixture_file(fixture_root: Path, repository_path: str) -> Path:
    relative = Path(repository_path)
    try:
        within = relative.relative_to(FULL_DECLARED_FIXTURE_RELATIVE)
    except ValueError as error:
        raise ValueError(f"full-declared input escapes its fixture root: {repository_path}") from error
    return fixture_root / within


def full_declared_closure_inputs(closure: dict[str, Any]) -> dict[str, dict[str, Any]]:
    entries = closure.get("inputs")
    if not isinstance(entries, list) or len(entries) != len(FULL_DECLARED_APPROVED_INPUTS):
        raise ValueError("full-declared input closure cardinality differs")
    result: dict[str, dict[str, Any]] = {}
    for entry in entries:
        path = entry.get("path") if isinstance(entry, dict) else None
        if not isinstance(path, str) or path not in FULL_DECLARED_APPROVED_INPUTS or path in result:
            raise ValueError("full-declared input closure has duplicate or undeclared input")
        result[path] = entry
    if result != FULL_DECLARED_APPROVED_INPUTS:
        raise ValueError("full-declared input closure differs from approved immutable pins")
    return result


def full_declared_provenance_lock(fixture_root: Path, closure: dict[str, Any]) -> dict[str, Any]:
    """Check immutable reviewed receipts, then the copied v4 source inputs."""
    pin = closure.get("provenance_lock")
    if pin != FULL_DECLARED_APPROVED_LOCK:
        raise ValueError("full-declared provenance lock pin differs from the approved lock")
    raw = full_declared_fixture_file(fixture_root, FULL_DECLARED_APPROVED_LOCK["path"]).read_bytes()
    if len(raw) != FULL_DECLARED_APPROVED_LOCK["bytes"] or digest(raw) != FULL_DECLARED_APPROVED_LOCK["sha256"]:
        raise ValueError("full-declared provenance lock digest differs")
    lock = json.loads(raw)
    if lock.get("schema") != "five_suite_full_declared_input_lock/v1":
        raise ValueError("full-declared provenance lock schema differs")
    if lock.get("audit_packages") != FULL_DECLARED_APPROVED_AUDIT_PACKAGES:
        raise ValueError("full-declared provenance package receipts differ")
    if lock.get("fixture_inputs") != [entry for _, entry in sorted(FULL_DECLARED_APPROVED_INPUTS.items())]:
        raise ValueError("full-declared provenance input identities differ")
    if closure.get("durable_mmdflux_manifest") != FULL_DECLARED_APPROVED_DURABLE_MANIFEST:
        raise ValueError("full-declared closure durable manifest differs")
    if lock.get("durable_mmdflux_manifest") != FULL_DECLARED_APPROVED_DURABLE_MANIFEST:
        raise ValueError("full-declared provenance durable manifest differs")
    return lock


def full_declared_checked_json(fixture_root: Path, closure: dict[str, Any], name: str) -> dict[str, Any]:
    expected = (FULL_DECLARED_FIXTURE_RELATIVE / "source-inputs" / name).as_posix()
    entry = full_declared_closure_inputs(closure).get(expected)
    if entry is None:
        raise ValueError(f"full-declared closure missing input: {name}")
    raw = full_declared_fixture_file(fixture_root, expected).read_bytes()
    if len(raw) != entry["bytes"] or digest(raw) != entry["sha256"]:
        raise ValueError(f"full-declared input digest mismatch: {name}")
    return json.loads(raw)


def full_declared_r3_files(r3: dict[str, Any]) -> dict[str, tuple[str, dict[str, Any]]]:
    files = r3.get("files")
    if not isinstance(files, dict):
        raise ValueError("full-declared R3 files must be an object")
    result: dict[str, tuple[str, dict[str, Any]]] = {}
    for vendor, entries in files.items():
        if vendor not in VENDORS or not isinstance(entries, list):
            raise ValueError("full-declared R3 vendor file set is invalid")
        for entry in entries:
            if not isinstance(entry, dict):
                raise ValueError("full-declared R3 file row is invalid")
            path = entry.get("path")
            if not isinstance(path, str) or path in result:
                raise ValueError("full-declared R3 file path is missing or duplicated")
            result[path] = (vendor, entry)
    return result


def load_full_declared_source_map(
    repository_root: Path, fixture_root: Path, r3: dict[str, Any], recovery: dict[str, Any]
) -> dict[str, FullDeclaredSource]:
    """Resolve only the all-387 declared durable paths; never search build caches."""
    closure = json.loads((fixture_root / INPUT_CLOSURE_NAME).read_text())
    source_map = full_declared_checked_json(fixture_root, closure, "FULL_DECLARED_SOURCE_PATH_MAP.v1.json")
    return full_declared_validated_source_map(repository_root, r3, recovery, source_map)


def full_declared_validated_source_map(
    repository_root: Path, r3: dict[str, Any], recovery: dict[str, Any], source_map: dict[str, Any]
) -> dict[str, FullDeclaredSource]:
    """Validate an already pinned map; used by the builder and adversarial controls."""
    if source_map.get("schema") != FULL_DECLARED_SOURCE_MAP_SCHEMA:
        raise ValueError("unexpected full-declared source map schema")
    entries = source_map.get("entries")
    if not isinstance(entries, list) or source_map.get("counts", {}).get("declared_sources") != 387:
        raise ValueError("full-declared source map must contain all 387 declared sources")
    r3_files = full_declared_r3_files(r3)
    recovery_rows = recovery.get("sources")
    if not isinstance(recovery_rows, list) or len(recovery_rows) != 121:
        raise ValueError("full-declared recovery rows must contain exactly 121 sources")
    recovered = {row.get("cached_path"): row for row in recovery_rows if isinstance(row, dict)}
    if len(recovered) != 121 or recovery.get("complete_source_files") != 121:
        raise ValueError("full-declared recovery set must contain exactly 121 sources")
    mapped: dict[str, FullDeclaredSource] = {}
    durable_paths: set[str] = set()
    recovered_count = 0
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("full-declared map entry is invalid")
        identity = entry.get("r3_declared_path")
        durable = entry.get("durable_source_path")
        if not isinstance(identity, str) or not isinstance(durable, str) or identity in mapped or durable in durable_paths:
            raise ValueError("full-declared map identity or durable path is not unique")
        if identity not in r3_files or durable.startswith("build/") or not durable.startswith("tests/fixtures/mermaid_terminal/"):
            raise ValueError("full-declared source map has an undeclared or non-durable path")
        vendor, r3_entry = r3_files[identity]
        if entry.get("vendor") != vendor or entry.get("source_sha256") != r3_entry.get("sha256") or entry.get("bytes") != r3_entry.get("bytes"):
            raise ValueError("full-declared source map changes R3 identity")
        raw_path = repository_root / durable
        repository_relative(raw_path.resolve(), repository_root)
        if not raw_path.is_file():
            raise ValueError(f"full-declared durable source is missing: {durable}")
        raw = raw_path.read_bytes()
        if len(raw) != entry["bytes"] or digest(raw) != entry["source_sha256"]:
            raise ValueError(f"full-declared durable source identity mismatch: {durable}")
        kind = entry.get("storage_kind")
        upstream_path: str | None = None
        if kind == "existing_fixture":
            if durable != identity:
                raise ValueError("existing fixture source must retain its durable identity path")
        elif kind == "mmdflux_recovered_full_source":
            recovered_row = recovered.get(identity)
            if recovered_row is None or vendor != "mmdflux":
                raise ValueError("recovered source has no exact recovery row")
            required = {
                "upstream_path": recovered_row.get("path"),
                "upstream_commit": recovery.get("commit"),
                "upstream_git_blob": recovered_row.get("git_blob"),
                "origin": recovered_row.get("origin"),
            }
            if any(entry.get(key) != value for key, value in required.items()):
                raise ValueError("recovered source provenance differs from its pinned row")
            expected_durable = f"tests/fixtures/mermaid_terminal/mmdflux/full_declared_sources/{recovery['commit']}/{recovered_row['path']}"
            if durable != expected_durable:
                raise ValueError("recovered source durable path is not the exact declared destination")
            upstream_path = str(recovered_row["path"])
            recovered_count += 1
        else:
            raise ValueError("full-declared source storage kind is invalid")
        mapped[identity] = FullDeclaredSource(vendor, identity, durable, entry["source_sha256"], entry["bytes"], kind, upstream_path)
        durable_paths.add(durable)
    if set(mapped) != set(r3_files) or recovered_count != 121 or len(mapped) != 387:
        raise ValueError("full-declared map is not an exact R3 and recovery bijection")
    return mapped


def full_declared_line_for(raw: bytes, byte: int) -> int:
    return line_for(line_byte_starts(raw), byte)


def full_declared_context_id(vendor: str, path: str, identity: str, body: dict[str, Any]) -> str:
    return digest(f"{vendor}\0{path}\0{identity}\0{body.get('byte_start')}\0{body.get('byte_end')}".encode())


def full_declared_structures(
    repository_root: Path,
    r3: dict[str, Any],
    r4: dict[str, Any],
    locations: dict[str, FullDeclaredSource],
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]]]:
    """Flatten frozen AST contexts only after binding every retained relation."""
    containers: list[dict[str, Any]] = []
    records: list[dict[str, Any]] = []
    context_by_id: dict[str, dict[str, Any]] = {}
    contexts_by_source: dict[str, list[dict[str, Any]]] = {}
    assertion_counts: dict[str, int] = {}
    physical_records: dict[tuple[str, str, str, int, int], list[dict[str, Any]]] = {}

    for identity_path, (vendor, file_entry) in sorted(full_declared_r3_files(r3).items()):
        location = locations[identity_path]
        raw = (repository_root / location.durable_path).read_bytes()
        for unit in file_entry.get("test_units", []):
            if not isinstance(unit, dict):
                raise ValueError("full-declared context row is invalid")
            identity = unit.get("identity")
            declaration = unit.get("declaration")
            body = unit.get("body")
            declaration_kind = unit.get("declaration_kind")
            identity_status = unit.get("identity_status")
            parent_identity = unit.get("parent_identity")
            if (not isinstance(identity, str) or not isinstance(declaration, dict) or not isinstance(body, dict)
                    or not isinstance(declaration_kind, str) or not isinstance(identity_status, str)
                    or parent_identity is not None and not isinstance(parent_identity, str)):
                raise ValueError("full-declared context identity metadata is invalid")
            declaration_start, declaration_end = declaration.get("byte_start"), declaration.get("byte_end")
            start, end = body.get("byte_start"), body.get("byte_end")
            if (not isinstance(declaration_start, int) or not isinstance(declaration_end, int)
                    or not isinstance(start, int) or not isinstance(end, int)
                    or not (0 <= declaration_start < declaration_end <= len(raw))
                    or not (declaration_start <= start < end <= declaration_end)):
                raise ValueError("full-declared context declaration or body lies outside durable source")
            context_id = full_declared_context_id(vendor, identity_path, identity, body)
            if context_id in context_by_id:
                raise ValueError("full-declared context identity is duplicated")
            container = {
                "id": context_id,
                "vendor": vendor,
                "identity": identity,
                "parent_identity": parent_identity,
                "identity_status": identity_status,
                "declaration_kind": declaration_kind,
                "source_identity_path": identity_path,
                "durable_source_path": location.durable_path,
                "source_sha256": location.sha256,
                "declaration_byte_start": declaration_start,
                "declaration_byte_end": declaration_end,
                "body_byte_start": start,
                "body_byte_end": end,
            }
            containers.append(container)
            context_by_id[context_id] = container
            contexts_by_source.setdefault(identity_path, []).append(container)
            assertion_counts[context_id] = 0
            for site in unit.get("sites", []):
                if not isinstance(site, dict) or site.get("category") != "assertion":
                    continue
                site_start, site_end = site.get("byte_start"), site.get("byte_end")
                if not isinstance(site_start, int) or not isinstance(site_end, int) or not (start <= site_start < site_end <= end):
                    raise ValueError("full-declared assertion site lies outside its context or durable source")
                selected = raw[site_start:site_end]
                record_id = digest(f"{context_id}\0{site_start}\0{site_end}\0{site.get('callee')}".encode())
                record = {
                    "id": record_id,
                    "record_type": "declared_source_assertion_site",
                    "vendor": vendor,
                    "context_id": context_id,
                    "source": {
                        "source_identity_path": identity_path,
                        "durable_source_path": location.durable_path,
                        "file_sha256": location.sha256,
                        "byte_start": site_start,
                        "byte_end": site_end,
                        "line_start": full_declared_line_for(raw, site_start),
                        "line_end": full_declared_line_for(raw, site_end - 1),
                        "selected_sha256": digest(selected),
                    },
                    "site": {"category": "assertion", "callee": site.get("callee"), "node_type": site.get("node_type")},
                    "status": "static_source_only_not_execution_or_semantic_credit",
                }
                records.append(record)
                assertion_counts[context_id] += 1
                physical_records.setdefault((vendor, identity_path, location.sha256, site_start, site_end), []).append(record)

    if len({row["id"] for row in records}) != len(records):
        raise ValueError("full-declared assertion record identity is duplicated")
    nested_repeat_entries = 0
    for entries in physical_records.values():
        if len(entries) == 1:
            continue
        if len(entries) != 2:
            raise ValueError("full-declared repeated physical site has more than two contexts")
        left, right = (context_by_id[entry["context_id"]] for entry in entries)
        parent, child = (left, right) if right["parent_identity"] == left["identity"] else (right, left)
        if (parent["vendor"] != "mermaid_ascii" or child["vendor"] != "mermaid_ascii"
                or parent["source_identity_path"] != child["source_identity_path"]
                or parent["declaration_kind"] not in {"go_function", "t.Run"}
                or child["declaration_kind"] != "t.Run"
                or child["parent_identity"] != parent["identity"]
                or not (parent["body_byte_start"] <= child["body_byte_start"] < child["body_byte_end"] <= parent["body_byte_end"])):
            raise ValueError("full-declared repeated physical site is not a nested mermaid_ascii parent/subtest pair")
        nested_repeat_entries += 1

    totals = r4.get("totals", {})
    if (len(containers) != totals.get("test_context_identities")
            or len(records) != totals.get("assertion_site_entries")
            or len(physical_records) != totals.get("distinct_assertion_source_sites")
            or nested_repeat_entries != totals.get("repeated_context_entries")):
        raise ValueError("full-declared R4 source-site totals or nested-repeat structure differ")

    binding_rows: list[dict[str, Any]] = []
    binding_context_ids: set[str] = set()
    upstream_locations = {location.upstream_path: location for location in locations.values() if location.upstream_path is not None}
    for binding in r3.get("mmdflux_private_predicate_full_context_bindings", []):
        if not isinstance(binding, dict):
            raise ValueError("mmdflux private predicate binding row is invalid")
        upstream_path = binding.get("path")
        location = upstream_locations.get(upstream_path)
        declaration = binding.get("declaration")
        body = binding.get("body")
        if location is None or binding.get("matches") != 1 or not isinstance(declaration, dict) or not isinstance(body, dict):
            raise ValueError("mmdflux private predicate binding has no unique durable source")
        begin, end = binding.get("byte_start"), binding.get("byte_end")
        raw = (repository_root / location.durable_path).read_bytes()
        if not isinstance(begin, int) or not isinstance(end, int) or not (0 <= begin < end <= len(raw)):
            raise ValueError("mmdflux private predicate binding range is outside durable source")
        matches = [
            context for context in contexts_by_source.get(location.identity_path, [])
            if context["identity"] == binding.get("identity")
            and context["declaration_byte_start"] == declaration.get("byte_start")
            and context["declaration_byte_end"] == declaration.get("byte_end")
            and context["body_byte_start"] == body.get("byte_start")
            and context["body_byte_end"] == body.get("byte_end")
        ]
        if len(matches) != 1:
            raise ValueError("mmdflux private predicate binding does not match exactly one full AST context")
        context = matches[0]
        if (not (begin <= context["declaration_byte_start"] < context["declaration_byte_end"] == end)
                or binding.get("source_assertion_sites") != assertion_counts[context["id"]]
                or context["id"] in binding_context_ids):
            raise ValueError("mmdflux private predicate binding declaration, assertion count, or uniqueness differs")
        binding_context_ids.add(context["id"])
        binding_rows.append({
            **binding,
            "context_id": context["id"],
            "source_identity_path": location.identity_path,
            "durable_source_path": location.durable_path,
            "source_sha256": location.sha256,
            "status": "private_predicate_source_only_not_runtime_or_semantic_credit",
        })
    if len(binding_rows) != 2412:
        raise ValueError("mmdflux private predicate binding count differs")

    containers.sort(key=lambda item: (item["vendor"], item["source_identity_path"], item["body_byte_start"], item["id"]))
    records.sort(key=lambda item: (item["vendor"], item["source"]["source_identity_path"], item["source"]["byte_start"], item["context_id"], item["id"]))
    binding_rows.sort(key=lambda item: (item["path"], item["byte_start"], item["identity"]))
    return containers, records, binding_rows



def build_full_declared(repository_root: Path, fixture_root: Path) -> dict[str, Any]:
    closure_raw = (fixture_root / INPUT_CLOSURE_NAME).read_bytes()
    closure = json.loads(closure_raw)
    if closure.get("schema") != FULL_DECLARED_CLOSURE_SCHEMA:
        raise ValueError("unexpected full-declared input-closure schema")
    full_declared_closure_inputs(closure)
    full_declared_provenance_lock(fixture_root, closure)
    r3 = full_declared_checked_json(fixture_root, closure, "FULL_AST_SOURCE_INDEX.r3.json")
    r4 = full_declared_checked_json(fixture_root, closure, "SOURCE_SITE_COUNTS.r4.json")
    recovery = full_declared_checked_json(fixture_root, closure, "SOURCE_RECOVERY.r3.json")
    source_map = full_declared_checked_json(fixture_root, closure, "FULL_DECLARED_SOURCE_PATH_MAP.v1.json")
    locations = load_full_declared_source_map(repository_root, fixture_root, r3, recovery)
    manifest_pin = FULL_DECLARED_APPROVED_DURABLE_MANIFEST
    manifest_name = "tests/fixtures/mermaid_terminal/mmdflux/full_declared_sources/" + str(recovery["commit"]) + "/SOURCE_MANIFEST.v1.json"
    if manifest_pin.get("path") != manifest_name:
        raise ValueError("full-declared durable source manifest path differs")
    manifest_path = repository_root / manifest_name
    manifest_raw = manifest_path.read_bytes()
    if len(manifest_raw) != manifest_pin.get("bytes") or digest(manifest_raw) != manifest_pin.get("sha256"):
        raise ValueError("full-declared durable source manifest digest differs")
    manifest = json.loads(manifest_raw)
    if manifest.get("schema") != "mmdflux_full_declared_source_manifest/v1" or manifest.get("commit") != recovery.get("commit"):
        raise ValueError("full-declared durable source manifest is invalid")
    license_path = manifest_path.parent / "LICENSE"
    license_raw = license_path.read_bytes()
    license_info = manifest.get("license")
    map_license = source_map.get("license")
    if (not isinstance(license_info, dict) or not isinstance(map_license, dict)
            or license_info.get("durable_path") != map_license.get("durable_path")
            or license_info.get("sha256") != map_license.get("source_sha256")
            or license_info.get("bytes") != map_license.get("source_bytes")
            or len(license_raw) != license_info.get("bytes") or digest(license_raw) != license_info.get("sha256")):
        raise ValueError("full-declared durable license identity mismatch")
    manifest_rows = manifest.get("sources")
    if not isinstance(manifest_rows, list) or len(manifest_rows) != 121:
        raise ValueError("full-declared durable source manifest cardinality differs")
    manifest_sources = {row.get("upstream_path"): row for row in manifest_rows if isinstance(row, dict)}
    if len(manifest_sources) != 121:
        raise ValueError("full-declared durable source manifest count differs")
    for location in locations.values():
        if location.storage_kind != "mmdflux_recovered_full_source":
            continue
        source = manifest_sources.get(location.upstream_path)
        if source is None or source.get("durable_source_path") != location.durable_path or source.get("sha256") != location.sha256 or source.get("bytes") != location.bytes:
            raise ValueError("full-declared source manifest differs from map")

    containers, records, binding_rows = full_declared_structures(repository_root, r3, r4, locations)
    physical_count = len({
        (record["vendor"], record["source"]["source_identity_path"], record["source"]["file_sha256"],
         record["source"]["byte_start"], record["source"]["byte_end"])
        for record in records
    })
    source_files = [
        {"vendor": location.vendor, "source_identity_path": location.identity_path, "durable_source_path": location.durable_path,
         "sha256": location.sha256, "bytes": location.bytes, "storage_kind": location.storage_kind,
         **({"upstream_path": location.upstream_path} if location.upstream_path is not None else {})}
        for location in sorted(locations.values(), key=lambda value: value.identity_path)
    ]
    return {
        "schema": "five_suite_full_declared_source_assertion_index/v4",
        "status": "static_source_index_not_execution_or_semantic_denominator",
        "scope": {
            "complete_declared_sources": 387,
            "test_context_identities": len(containers),
            "assertion_site_entries": len(records),
            "distinct_assertion_source_sites": physical_count,
            "nested_repeat_entries": len(records) - physical_count,
            "mmdflux_private_predicate_full_context_bindings": len(binding_rows),
            "semantic_assertion_pass_credit": 0,
            "runtime_expansion_credit": 0,
            "unresolved_boundaries": "Dynamic registrations, macro expansion, helper behavior, options, snapshots, and execution remain source-only or unresolved.",
        },
        "inputs": {
            "input_closure_path": (FULL_DECLARED_FIXTURE_RELATIVE / INPUT_CLOSURE_NAME).as_posix(),
            "input_closure_sha256": digest(closure_raw),
            "r3_source_identity": source_map_identity(source_map=full_declared_checked_json(fixture_root, closure, "FULL_DECLARED_SOURCE_PATH_MAP.v1.json")),
            "r4_source_site_counts": full_declared_checked_json(fixture_root, closure, "SOURCE_SITE_COUNTS.r4.json").get("index_sha256"),
            "source_read_rule": "Every source byte read is through the exact durable_source_path in FULL_DECLARED_SOURCE_PATH_MAP.v1.json; build cache paths are never consulted.",
        },
        "source_files": source_files,
        "containers": containers,
        "records": records,
        "mmdflux_private_predicate_full_context_bindings": binding_rows,
    }


def source_map_identity(source_map: dict[str, Any]) -> dict[str, Any]:
    return {
        "schema": source_map.get("schema"),
        "r3_full_ast_source_index": source_map.get("r3_full_ast_source_index"),
        "mmdflux_recovery": source_map.get("mmdflux_recovery"),
        "counts": source_map.get("counts"),
    }


def full_declared_mode(fixture_root: Path) -> bool:
    closure = json.loads((fixture_root / INPUT_CLOSURE_NAME).read_text())
    return closure.get("schema") == FULL_DECLARED_CLOSURE_SCHEMA

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-index", action="store_true", required=True,
                        help="write the reviewed static index into the tracked fixture closure")
    parser.add_argument("--repo-root", type=Path, default=REPOSITORY_ROOT)
    parser.add_argument("--fixture-root", type=Path,
                        help="fixture directory; defaults to the tracked repository location")
    args = parser.parse_args()
    repository_root = args.repo_root.resolve()
    fixture_root = (args.fixture_root or repository_root / FIXTURE_RELATIVE).resolve()
    result = build_full_declared(repository_root, fixture_root) if full_declared_mode(fixture_root) else build(repository_root, fixture_root)
    output = fixture_root / (FULL_DECLARED_INDEX_NAME if full_declared_mode(fixture_root) else INDEX_NAME)
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n")
    try:
        output_display = repository_relative(output, repository_root)
    except ValueError:
        # Validator regeneration deliberately writes a temporary fixture outside
        # the repository; that location has no source-provenance meaning.
        output_display = output.as_posix()
    print(json.dumps({"index": output_display,
                      "sha256": digest(output.read_bytes()),
                      "records": len(result["records"])}, indent=2))


if __name__ == "__main__":
    main()
