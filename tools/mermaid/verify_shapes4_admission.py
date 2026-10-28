#!/usr/bin/env python3
"""Join Shapes4 compact verdicts after the frozen Mindmap10 dual admission.

No frames, cell hashes or pixels are consumed. --self-test grants zero credit.
--write requires exactly the Debug and Release UI build directories.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
import os
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location(
    "loom_mindmap10_admission", Path(__file__).with_name("verify_mindmap10_admission.py"))
OLD = importlib.util.module_from_spec(_spec)
# Explicit file import also works under python -I without touching sibling files.
_previous_bytecode = sys.dont_write_bytecode
try:
    sys.dont_write_bytecode = True
    _spec.loader.exec_module(OLD)
finally:
    sys.dont_write_bytecode = _previous_bytecode
load, closed, need = OLD.load, OLD.closed, OLD.need
runtime_inputs, fresh_inputs, fresh = OLD.runtime_inputs, OLD.fresh_inputs, OLD.fresh
FACTS = ROOT / "tests/fixtures/mermaid_native/mindmap5_shapes_source_proposal.v1.json"
FACTS_SHA = "26f803ffb4f900f81a5742f97671a7c6fd4a3c53a0d55a36a6bbbd2af5d1bd25"
BASE = 626
SUMMARY = "MindmapShapes4_native_whole_source"
JOIN = "MindmapShapes4_dual_consumer_join"
MARKER = "mermaid-shapes4-dual-admission.json"
LOCKS = ("path", "source_sha256", "complete_model_sha256", "terminal_projection_sha256")
CASE_KEYS = set(LOCKS) | {"local_ready", "captures", "captures_before_binding", "bound_after_captures",
    "geometry_and_restoration_good", "narrow_exact_ordinary", "narrow_geometry_good",
    "semantic_mutations_rejected", "physical_mutations_rejected"}
OFFICIAL_KEYS = CASE_KEYS | {"source_facts_sha256", "foreign_assertions_certified", "native_report_input"}
ROOT_KEYS = {"schema", "source_facts_sha256", "local_ready", "cases"}
TOTALS = {"originals": 4, "errors": 0, "semantic_mutations_rejected": 11,
          "physical_mutations_rejected": 16, "foreign_assertions_certified": 0}
PINS = (1313, 1317, 1318, 1319)
SHAPES4_INPUTS = ["src/ui/visual/mermaid_relations_core_impl.cpp",
                  "src/ui/visual/code_highlight.cppm", "src/ui/visual/code_highlight_impl.cpp",
                  "src/ui/visual/markdown.cppm"]
CONSUMERS = (("native", "test_mermaid_mindmap_semantics", "tests/ui/test_mermaid_mindmap_semantics.cpp"),
             ("official", "test_mermaid_official", "tests/ui/test_mermaid_official.cpp"))


def count(value, expected, reason):
    need(type(value) is int and value == expected, reason)


def facts():
    document = load(FACTS, 131072)
    raw = FACTS.read_bytes()
    need(len(raw) == 84238 and hashlib.sha256(raw).hexdigest() == FACTS_SHA, "approved Shapes5 facts SHA/size")
    rows = document["cases"]
    need(len(rows) == 5 and [r["manifest_global_ordinal"] for r in rows] == [1309, *PINS], "five exact source originals")
    selected = rows[1:]
    need(all("terminal_projection_sha256" in r for r in selected) and
         "terminal_projection_sha256" not in rows[0], "four non-Bang/Cloud projections")
    count(sum(len(r["complete_model"]["nodes"]) for r in selected), 5, "five selected raw Model nodes")
    count(sum(len(r["parent_relations"]) for r in selected), 1, "one selected raw Model edge")
    return rows


def validate_pair(native, official, source):
    expected = {r["path"]: r for r in source}
    need(len(source) == len(expected) == 4 and tuple(r["manifest_global_ordinal"] for r in source) == PINS,
         "four exact selected originals")
    for side, report in (("native", native), ("official", official)):
        closed(report, ROOT_KEYS | (set(TOTALS) if side == "native" else set()), "closed compact root schema")
        need(report["schema"] == f"mindmap_shapes4_{'native_source_scenes' if side == 'native' else 'official_local_verdict'}/v1"
             and report["source_facts_sha256"] == FACTS_SHA, "consumer authority/schema")
        count(report["local_ready"], 4, "four local-ready originals")
        if side == "native":
            for key, value in TOTALS.items():
                count(report[key], value, f"exact Native total: {key}")
        rows = report["cases"]
        need(isinstance(rows, list) and len(rows) == 4, "four compact cases")
        seen = set()
        for row in rows:
            closed(row, CASE_KEYS if side == "native" else OFFICIAL_KEYS, "closed compact case schema")
            path = row["path"]
            need(type(path) is str and path in expected and path not in seen, "unique selected original")
            seen.add(path)
            for key in LOCKS:
                need(type(row[key]) is str and row[key] == expected[path][key], f"original lock: {key}")
            for key in ("local_ready", "captures_before_binding", "bound_after_captures", "geometry_and_restoration_good"):
                need(row[key] is True, f"ordered geometry proof: {key}")
            count(row["captures"], 5, "five captures")
            ordinary, geometry = row["narrow_exact_ordinary"], row["narrow_geometry_good"]
            need(type(ordinary) is bool and type(geometry) is bool and ordinary != geometry, "narrow XOR")
            tree = expected[path]["manifest_global_ordinal"] == 1319
            count(row["semantic_mutations_rejected"], 5 if tree else 2, "exact semantic controls")
            count(row["physical_mutations_rejected"], 7 if tree else 3, "exact physical controls")
            if side == "official":
                need(row["source_facts_sha256"] == FACTS_SHA and row["native_report_input"] is False, "no borrowed Native proof")
                count(row["foreign_assertions_certified"], 0, "no foreign assertion credit")
    native_rows = {r["path"]: r for r in native["cases"]}
    for row in official["cases"]:
        need(all(row[k] == native_rows[row["path"]][k] for k in CASE_KEYS), "Native/Official compact disagreement")
    return [{k: row[k] for k in LOCKS} for row in source]


def result_for(joined, old=False):
    return {"schema": "mindmap10_dual_consumer_admission/v1" if old else "mindmap_shapes4_dual_consumer_admission/v1",
            "source_facts_sha256": OLD.FACTS_SHA if old else FACTS_SHA,
            "base_admitted_originals": 616 if old else BASE, "joined_originals": 10 if old else 4,
            "formal_admitted_originals": 626 if old else 630, "pinned_original_files": 2145,
            "foreign_assertions_certified": 0, "cases": joined}


def summary_check(summary, old=False):
    expected = {"selected": 10 if old else 4, "local_ready": 10 if old else 4,
                "admitted": 10 if old else 0, "base_admitted_originals": 616 if old else BASE,
                "foreign_assertions_certified": 0}
    for key, value in expected.items():
        count(summary[key], value, f"{'Mindmap10 prerequisite' if old else 'frozen 626 baseline'}: {key}")
    need(summary["join_required"] is (False if old else True), "join-required state")
    need(summary["source_facts_sha256"] == (OLD.FACTS_SHA if old else FACTS_SHA), "summary source authority")
    if not old:
        need(summary["prerequisite_join"] == "Mindmap10_native_whole_source", "Mindmap10 prerequisite identity")


def matrix_rows(matrix, official, joined, key):
    need(matrix.get("integrity_errors") == [], "matrix integrity errors")
    cases = matrix["cases"]
    need(isinstance(cases, list) and len(cases) == 2738, "2145 originals plus 593 diagnostics")
    originals = [r for r in cases if r["track"] == "original"]
    need(len(originals) == 2145 and len({r["fixture"] for r in originals}) == 2145, "unique pinned originals")
    index = {r["fixture"]: r for r in originals}
    local = {r["path"]: r for r in official["cases"]}
    for proof in joined:
        row = index.get(proof["path"])
        need(row is not None and row["source_sha256"] == proof["source_sha256"] and row["errors"] == [], "matrix original identity/errors")
        actual = row[key]["actual"]
        closed(actual, OFFICIAL_KEYS - {"path"}, "closed matrix compact provenance")
        need(actual == {k: v for k, v in local[proof["path"]].items() if k != "path"}, "matrix/Official provenance disagreement")
        observed = row["observed"]
        need(observed["wide_240"] == observed["restored_240"] == "drawn", "wide/restored full geometry")
        need(observed["narrow_40"] == ("source_fallback" if actual["narrow_exact_ordinary"] else "drawn"), "matrix narrow mismatch")


def prerequisite(matrix, marker, native, official, source):
    joined = OLD.validate_pair(native, official, source)
    # The old validator predates the cross-consumer narrow-state agreement.
    n = {r["path"]: r for r in native["cases"]}
    need(all(all(r[k] == n[r["path"]][k] for k in CASE_KEYS) for r in official["cases"]), "Mindmap10 consumer disagreement")
    expected = result_for(joined, old=True)
    closed(marker, expected, "closed prerequisite marker")
    for key in ("base_admitted_originals", "joined_originals", "formal_admitted_originals", "pinned_original_files", "foreign_assertions_certified"):
        count(marker[key], expected[key], "prerequisite marker counts")
    need(marker == expected and matrix.get("Mindmap10_dual_consumer_join") == marker, "fresh exact Mindmap10 prerequisite marker/matrix")
    summary_check(matrix["Mindmap10_native_whole_source"], old=True)
    matrix_rows(matrix, official, joined, "Mindmap10_native_whole_source")


def baseline(matrix):
    summary_check(matrix[SUMMARY])
    count(matrix["original_drawing_coverage"]["drawn_at_240"], BASE, "frozen 626 formal baseline")
    count(matrix["original_drawing_coverage"]["pinned_original_files"], 2145, "pinned coverage denominator")
    need(JOIN not in matrix, "Shapes4 must be unjoined")


def consumer_inputs(directory, cohort, source, authority):
    paths = [authority] + runtime_inputs(source)
    reports = []
    for side, executable, consumer in CONSUMERS:
        name = f"mermaid-{cohort}-{side}-verdict.json"
        report = directory / name
        value = load(report)
        fresh_inputs(report, paths)
        fresh(directory, name, executable, consumer)
        # Shapes4 also owns ordinary-source foreground/background equivalence.
        if cohort == "shapes4":
            fresh_inputs(directory / executable, [ROOT / path for path in SHAPES4_INPUTS])
        reports.append(value)
    return reports, paths


def verify(directory):
    source = facts()
    # Revalidate the previous join directly; OLD.verify requires unjoined 616.
    previous_source = OLD.facts()
    previous, previous_inputs = consumer_inputs(directory, "mindmap10", previous_source, OLD.FACTS)
    matrix_path = directory / "mermaid-official-matrix.json"
    matrix = load(matrix_path, 20_000_000)
    marker_path = directory / "mermaid-mindmap10-dual-admission.json"
    marker = load(marker_path)
    fresh_inputs(matrix_path, [directory / "mermaid-mindmap10-official-verdict.json"])
    fresh_inputs(marker_path, previous_inputs + [directory / f"mermaid-mindmap10-{side}-verdict.json" for side, _, _ in CONSUMERS])
    prerequisite(matrix, marker, *previous, previous_source)
    pair, _ = consumer_inputs(directory, "shapes4", source, FACTS)
    selected = source[1:]
    joined = validate_pair(*pair, selected)
    fresh_inputs(matrix_path, [directory / "mermaid-shapes4-official-verdict.json"])
    baseline(matrix)
    matrix_rows(matrix, pair[1], joined, SUMMARY)
    result = result_for(joined)
    matrix[SUMMARY]["admitted"] = 4
    matrix[SUMMARY]["join_required"] = False
    matrix[JOIN] = result
    matrix["original_drawing_coverage"]["drawn_at_240"] = 630
    matrix["original_drawing_coverage"]["percent_of_pinned_original_files"] = 100.0 * 630 / 2145
    markdown = directory / "mermaid-official-matrix.md"
    text = markdown.read_text(encoding="utf-8")
    need("\n## MindmapShapes4 formal admission\n" not in text, "Shapes4 Markdown must be unjoined")
    text += ("\n## MindmapShapes4 formal admission\n\n"
             "Both independent consumers in Debug and Release pass all four original source/Model/projection locks, "
             "five-capture ordering and 11 semantic + 16 physical controls per consumer. "
             "Fresh Mindmap10 dual admission remains 616 + 10 = 626. "
             "Formal coverage: **630/2145 (29.37%)**. Bang/Cloud and body diagnostics receive no new credit. "
             "This metadata join reads no frames, cell hashes or pixels.\n")
    encode = lambda value: json.dumps(value, ensure_ascii=False, indent=2) + "\n"
    return {matrix_path: encode(matrix), markdown: text, directory / MARKER: encode(result)}


def publish_all(outputs):
    """Stage both presets' matrices/Markdown before installing formal markers."""
    markers = [p for p in outputs if p.name == MARKER]
    originals = {p: p.read_bytes() for p in outputs if p not in markers}
    staged = {}
    try:
        for path, content in outputs.items():
            with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent,
                                             prefix=path.name + ".join-", delete=False) as stream:
                staged[path] = Path(stream.name)
                stream.write(content)
                stream.flush()
                os.fsync(stream.fileno())
        for path in markers:
            path.unlink(missing_ok=True)
        for path in [*originals, *markers]:
            os.replace(staged[path], path)
    except OSError:
        # Attempt every restoration even if one recovery operation also fails.
        failures = []
        for path, data in originals.items():
            try:
                path.write_bytes(data)
            except OSError as error:
                failures.append(str(error))
        for path in markers:
            try:
                path.unlink(missing_ok=True)
            except OSError as error:
                failures.append(str(error))
        if failures:
            raise OSError("publication rollback incomplete: " + "; ".join(failures))
        raise
    finally:
        for path in staged.values():
            path.unlink(missing_ok=True)


def directories_for(value, write):
    directories = [(ROOT / item).resolve() for item in value.split(",") if item]
    need(directories and len(directories) == len(set(directories)), "unique build directories required")
    if write:
        expected = {(ROOT / "build" / preset / "tests" / "ui").resolve()
                    for preset in ("debug", "release")}
        need(len(directories) == 2 and set(directories) == expected,
             "formal publication requires this repository's exact Debug and Release tests/ui")
    return directories


def synthetic_pair(source, old=False):
    """Metadata-only test input; never a renderer or drawing certificate."""
    rows = []
    for fact in source:
        row = {k: fact[k] for k in LOCKS}
        if old:
            semantic = sum(m["id"].startswith("semantic_") for m in fact["required_good_first_mutations"])
            physical = len(fact["required_good_first_mutations"]) - semantic
        else:
            tree = fact["manifest_global_ordinal"] == 1319
            semantic, physical = (5, 7) if tree else (2, 3)
        row.update(local_ready=True, captures=5, captures_before_binding=True, bound_after_captures=True,
                   geometry_and_restoration_good=True, narrow_exact_ordinary=False, narrow_geometry_good=True,
                   semantic_mutations_rejected=semantic, physical_mutations_rejected=physical)
        rows.append(row)
    authority = OLD.FACTS_SHA if old else FACTS_SHA
    native = {"schema": "mindmap10_native_source_scenes/v1" if old else "mindmap_shapes4_native_source_scenes/v1",
              "source_facts_sha256": authority, "local_ready": len(rows), "cases": rows,
              "originals": len(rows), "errors": 0, "foreign_assertions_certified": 0,
              "semantic_mutations_rejected": 30 if old else 11, "physical_mutations_rejected": 30 if old else 16}
    official = {"schema": "mindmap10_official_local_verdict/v1" if old else "mindmap_shapes4_official_local_verdict/v1",
                "source_facts_sha256": authority, "local_ready": len(rows), "cases": copy.deepcopy(rows)}
    for row in official["cases"]:
        row.update(source_facts_sha256=authority, native_report_input=False, foreign_assertions_certified=0)
    return native, official


def synthetic_matrix(previous, pair):
    matrix = {"integrity_errors": [], "cases": [], "original_drawing_coverage": {
        "drawn_at_240": 626, "pinned_original_files": 2145}}
    for key, official, old in (("Mindmap10_native_whole_source", previous[1], True), (SUMMARY, pair[1], False)):
        matrix[key] = {"selected": 10 if old else 4, "local_ready": 10 if old else 4,
                       "admitted": 10 if old else 0, "join_required": not old,
                       "base_admitted_originals": 616 if old else 626,
                       "source_facts_sha256": OLD.FACTS_SHA if old else FACTS_SHA, "foreign_assertions_certified": 0}
        if not old:
            matrix[key]["prerequisite_join"] = "Mindmap10_native_whole_source"
        for row in official["cases"]:
            matrix["cases"].append({"track": "original", "fixture": row["path"], "source_sha256": row["source_sha256"],
                "errors": [], "observed": {"wide_240": "drawn", "restored_240": "drawn", "narrow_40": "drawn"},
                key: {"actual": {k: v for k, v in row.items() if k != "path"}}})
    matrix["cases"] += [{"track": "original", "fixture": f"synthetic/{i}"} for i in range(2145 - 14)]
    matrix["cases"] += [{"track": "body"} for _ in range(593)]
    marker = result_for([{k: r[k] for k in LOCKS} for r in OLD.facts()], old=True)
    matrix["Mindmap10_dual_consumer_join"] = copy.deepcopy(marker)
    return matrix, marker


class JoinControls(unittest.TestCase):
    def setUp(self):
        self.source = facts()[1:]
        self.pair = synthetic_pair(self.source)

    def test_exact_metadata_schema_and_counts(self):
        self.assertEqual(len(validate_pair(*self.pair, self.source)), 4)
        self.assertEqual(len(self.pair[0]), 9)
        self.assertEqual(len(self.pair[0]["cases"][0]), 13)
        self.assertEqual(len(self.pair[1]), 4)
        self.assertEqual(len(self.pair[1]["cases"][0]), 16)
        self.assertEqual(sum(r["semantic_mutations_rejected"] + r["physical_mutations_rejected"]
                             for r in self.pair[0]["cases"]), 27)

    def test_missing_partial_duplicate_and_borrowed_proof(self):
        mutations = [lambda r: r.pop(), lambda r: r.append(copy.deepcopy(r[0])),
            lambda r: r.__setitem__(1, copy.deepcopy(r[0])), lambda r: r[0].pop("captures"),
            lambda r: r[0].update(native_report_input=True), lambda r: r[0].update(foreign_assertions_certified=1),
            lambda r: r[0].update(path="mindmap/nightly-check/bang-and-cloud-shape-with-icons.mmd")]
        for side in (0, 1):
            for i, mutate in enumerate(mutations):
                pair = copy.deepcopy(self.pair); mutate(pair[side]["cases"])
                with self.subTest(side=side, control=i), self.assertRaises(ValueError):
                    validate_pair(*pair, self.source)

    def test_closed_roots_and_cases_reject_nested_unknown(self):
        for side in (0, 1):
            for key in ("frames", "cell_sha256", "pixels", "resource", "unknown"):
                for root in (True, False):
                    pair = copy.deepcopy(self.pair)
                    (pair[side] if root else pair[side]["cases"][0])[key] = {"nested": [True]}
                    with self.subTest(side=side, key=key, root=root), self.assertRaises(ValueError):
                        validate_pair(*pair, self.source)

    def test_counts_locks_and_boolean_types(self):
        bad_values = {"captures": [4, True, {}], "semantic_mutations_rejected": [0, True],
            "physical_mutations_rejected": [0, True], "local_ready": [1, False],
            "captures_before_binding": [1, False], "bound_after_captures": [1, False],
            "geometry_and_restoration_good": [1, False], "narrow_geometry_good": [1, False],
            "narrow_exact_ordinary": [0, True], **{k: ["0" * 64, {"nested": True}] for k in LOCKS[1:]}}
        for side in (0, 1):
            for key, values in bad_values.items():
                for value in values:
                    pair = copy.deepcopy(self.pair); pair[side]["cases"][0][key] = value
                    with self.subTest(side=side, key=key, value=value), self.assertRaises(ValueError):
                        validate_pair(*pair, self.source)
            for key in ("local_ready", *(TOTALS if side == 0 else ())):
                pair = copy.deepcopy(self.pair); pair[side][key] = True
                with self.subTest(root=key, side=side), self.assertRaises(ValueError):
                    validate_pair(*pair, self.source)
        pair = copy.deepcopy(self.pair)
        pair[0]["cases"][-1].update(semantic_mutations_rejected=2, physical_mutations_rejected=3)
        with self.assertRaises(ValueError):
            validate_pair(*pair, self.source)

    def test_cross_consumer_narrow_mismatch(self):
        pair = copy.deepcopy(self.pair)
        pair[1]["cases"][0].update(narrow_exact_ordinary=True, narrow_geometry_good=False)
        with self.assertRaisesRegex(ValueError, "disagreement"):
            validate_pair(*pair, self.source)

    def test_prerequisite_and_frozen_baseline(self):
        source = OLD.facts(); previous = synthetic_pair(source, old=True)
        matrix, marker = synthetic_matrix(previous, self.pair)
        prerequisite(matrix, marker, *previous, source)
        baseline(matrix)
        matrix_rows(matrix, self.pair[1], validate_pair(*self.pair, self.source), SUMMARY)
        for value in (625, 627, True):
            for target in ("coverage", "summary"):
                bad = copy.deepcopy(matrix)
                if target == "coverage":
                    bad["original_drawing_coverage"]["drawn_at_240"] = value
                else:
                    bad[SUMMARY]["base_admitted_originals"] = value
                with self.subTest(value=value, target=target), self.assertRaises(ValueError):
                    baseline(bad)
        for key, value in (("base_admitted_originals", 617), ("formal_admitted_originals", 627),
                           ("joined_originals", 9), ("source_facts_sha256", FACTS_SHA),
                           ("cases", marker["cases"][:-1]), ("foreign_assertions_certified", True)):
            bad = copy.deepcopy(marker); bad[key] = value
            with self.subTest(marker=key), self.assertRaises(ValueError):
                prerequisite(matrix, bad, *previous, source)
        bad = copy.deepcopy(matrix); bad.pop("Mindmap10_dual_consumer_join")
        with self.assertRaises(ValueError):
            prerequisite(bad, marker, *previous, source)
        with self.assertRaises(ValueError):
            prerequisite(matrix, {}, *previous, source)

    def test_matrix_provenance_narrow_resource_and_denominator(self):
        matrix, _ = synthetic_matrix(synthetic_pair(OLD.facts(), old=True), self.pair)
        joined = validate_pair(*self.pair, self.source)
        row_index = 10
        changes = [lambda m: m["cases"][row_index]["observed"].update(narrow_40="source_fallback"),
            lambda m: m["cases"][row_index]["observed"].update(wide_240="source_fallback"),
            lambda m: m["cases"][row_index]["errors"].append("resource exhausted"),
            lambda m: m["cases"][row_index][SUMMARY]["actual"].update(pixels={"nested": []}),
            lambda m: m["cases"][row_index][SUMMARY]["actual"].update(complete_model_sha256="0" * 64),
            lambda m: m["integrity_errors"].append("bad"), lambda m: m["cases"].pop()]
        for i, change in enumerate(changes):
            bad = copy.deepcopy(matrix); change(bad)
            with self.subTest(control=i), self.assertRaises(ValueError):
                matrix_rows(bad, self.pair[1], joined, SUMMARY)

    def test_runtime_source_freshness_all_five(self):
        import shutil
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = facts()
            paths = runtime_inputs(source)
            for path in paths:
                target = root / path.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, target)
            inputs = runtime_inputs(source, root)
            verdict = root / "compact.json"; verdict.write_text("{}")
            fresh_inputs(verdict, inputs)
            os.utime(inputs[1], ns=(verdict.stat().st_mtime_ns + 1000,) * 2)
            with self.assertRaises(ValueError):
                fresh_inputs(verdict, inputs)
            inputs[1].write_bytes(inputs[1].read_bytes() + b"\n")
            with self.assertRaisesRegex(ValueError, "changed raw"):
                runtime_inputs(source, root)
            inputs[0].write_bytes(inputs[0].read_bytes() + b" ")
            with self.assertRaisesRegex(ValueError, "changed manifest"):
                runtime_inputs(source, root)

    def test_binary_and_prerequisite_report_freshness(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            report = directory / "compact.json"; binary = directory / "consumer"
            report.write_text("{}"); binary.write_text("synthetic")
            os.utime(report, ns=(1, 1)); os.utime(binary, ns=(2, 2))
            with self.assertRaises(ValueError):
                fresh(directory, report.name, binary.name, CONSUMERS[0][2])
            with self.assertRaises(ValueError):
                fresh(directory, report.name, "missing", CONSUMERS[0][2])
            with self.assertRaises(ValueError):
                load(directory / "missing-marker.json")

    def test_complete_verification_is_read_only_and_preserves_previous_join(self):
        previous_source = OLD.facts()
        previous = synthetic_pair(previous_source, old=True)
        matrix, marker = synthetic_matrix(previous, self.pair)
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            matrix_path = directory / "mermaid-official-matrix.json"
            matrix_path.write_text(json.dumps(matrix))
            (directory / "mermaid-official-matrix.md").write_text("Prior matrix and Mindmap10 admission\n")
            marker_path = directory / "mermaid-mindmap10-dual-admission.json"
            marker_path.write_text(json.dumps(marker))
            for cohort, pair in (("mindmap10", previous), ("shapes4", self.pair)):
                for i, (side, executable, _) in enumerate(CONSUMERS):
                    (directory / f"mermaid-{cohort}-{side}-verdict.json").write_text(json.dumps(pair[i]))
                    (directory / executable).write_text("synthetic executable placeholder")
            # Synthetic timestamps use real source inputs, never current reports.
            inputs = [FACTS, OLD.FACTS, *runtime_inputs(facts()), *runtime_inputs(previous_source)]
            inputs += [ROOT / p for p in OLD.COMMON + [c[2] for c in CONSUMERS] + SHAPES4_INPUTS]
            stamp = max(p.stat().st_mtime_ns for p in inputs) + 1_000_000
            for _, executable, _ in CONSUMERS:
                os.utime(directory / executable, ns=(stamp, stamp))
            for path in directory.glob("*-verdict.json"):
                os.utime(path, ns=(stamp + 1, stamp + 1))
            os.utime(marker_path, ns=(stamp + 2, stamp + 2))
            os.utime(matrix_path, ns=(stamp + 2, stamp + 2))
            before = {p: p.read_bytes() for p in directory.iterdir()}
            outputs = verify(directory)
            self.assertEqual(before, {p: p.read_bytes() for p in directory.iterdir()})
            proposed = json.loads(outputs[matrix_path])
            self.assertEqual(proposed["Mindmap10_native_whole_source"], matrix["Mindmap10_native_whole_source"])
            self.assertEqual(proposed["Mindmap10_dual_consumer_join"], marker)
            self.assertEqual(proposed[SUMMARY]["admitted"], 4)
            self.assertIs(proposed[SUMMARY]["join_required"], False)
            self.assertEqual(proposed["original_drawing_coverage"]["drawn_at_240"], 630)
            self.assertTrue(outputs[directory / "mermaid-official-matrix.md"].startswith("Prior matrix"))
            self.assertNotIn(marker_path, outputs)
            os.utime(marker_path, ns=(stamp, stamp))
            with self.assertRaisesRegex(ValueError, "stale runtime"):
                verify(directory)

    def test_both_presets_only(self):
        self.assertEqual(len(directories_for("build/debug/tests/ui,build/release/tests/ui", True)), 2)
        for value in ("build/debug/tests/ui", "build/release/tests/ui", "build/debug/tests/ui,build/debug/tests/ui",
                      "build/debug/tests/ui,build/release/tests/ui,build/asan/tests/ui", "build/debug/foo/ui,build/release/foo/ui"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                directories_for(value, True)
        with tempfile.TemporaryDirectory() as outside:
            value = ",".join(str(Path(outside) / preset / "tests" / "ui")
                             for preset in ("debug", "release"))
            with self.assertRaisesRegex(ValueError, "this repository's exact"):
                directories_for(value, True)

    def test_atomic_publication_and_rollback_across_both_presets(self):
        # Inject each replace failure, including the second marker, and staging I/O.
        for fail_at in range(1, 8):
            with self.subTest(failure=fail_at), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary); outputs = {}; originals = {}
                for preset in ("debug", "release"):
                    directory = root / preset; directory.mkdir()
                    for name in ("mermaid-official-matrix.json", "mermaid-official-matrix.md", MARKER):
                        path = directory / name; path.write_text("old " + name)
                        outputs[path] = "new " + name
                        if name != MARKER:
                            originals[path] = path.read_bytes()
                replace = os.replace; calls = []
                def inject(src, dst):
                    calls.append(Path(dst))
                    if len(calls) == fail_at:
                        raise OSError("injected replace")
                    replace(src, dst)
                if fail_at == 7:
                    with patch.object(os, "fsync", side_effect=OSError("injected staging")), self.assertRaises(OSError):
                        publish_all(outputs)
                else:
                    with patch.object(os, "replace", side_effect=inject), self.assertRaises(OSError):
                        publish_all(outputs)
                for path, raw in originals.items():
                    self.assertEqual(path.read_bytes(), raw)
                self.assertTrue(all(not p.exists() for p in outputs if p.name == MARKER))
                self.assertFalse(list(root.glob("**/*.join-*")))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); outputs = {}
            for name in ("matrix.json", "matrix.md", MARKER):
                p = root / name; p.write_text("old"); outputs[p] = "new"
            publish_all(outputs)
            self.assertTrue(all(p.read_text() == "new" for p in outputs))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dirs", default="build/debug/tests/ui,build/release/tests/ui")
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        need(not args.write, "self-test grants zero credit; cannot combine with --write")
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(JoinControls)
        return int(not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful())
    directories = directories_for(args.build_dirs, args.write)
    outputs = {}
    for directory in directories:
        outputs.update(verify(directory))
    if args.write:
        publish_all(outputs)
    print(f"OK: {len(directories)} preset(s) verified; four joined originals; formal 630/2145 (29.37%)"
          + (" published" if args.write else "; validation only, no publication"))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, KeyError, TypeError, IndexError) as error:
        raise SystemExit(f"Shapes4 admission refused: {error}")
