#!/usr/bin/env python3
"""Join independent Mindmap10 consumer verdicts; never read frame artifacts.

Run after each preset's native CTest: --build-dirs build/debug/tests/ui,...
--write publishes formal admission into the matrix only after both local proofs.
--self-test exercises fail-closed metadata controls without granting admission.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
import tempfile
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
FACTS = ROOT / "tests/fixtures/mermaid_native/mindmap10_complete_source_proposal.v1.json"
BASE_ADMITTED_ORIGINALS = 616
MANIFEST_SHA = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b"
FACTS_SHA = "fa675abb49ce3de96e082b17cad12467296f06db80af28c9edafa8d4b6815627"
COMMON = ["src/ui/visual/mermaid_mindmap_impl.cpp", "src/ui/visual/mermaid_impl.cpp",
          "src/ui/visual/mermaid.cppm", "src/ui/visual/mermaid_detail.cppm",
          "src/ui/visual/mermaid_cells_impl.cpp", "src/ui/visual/mermaid_envelope_impl.cpp",
          "src/ui/visual/mermaid_cache_impl.cpp", "src/ui/visual/markdown_render_code_impl.cpp",
          "tests/support/mermaid_mindmap10_authority.hpp"]


def need(ok: bool, reason: str) -> None:
    if not ok:
        raise ValueError(reason)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load(path: Path, limit=65536):
    need(path.is_file() and path.stat().st_size <= limit, f"bounded file: {path}")
    return json.loads(path.read_bytes(), object_pairs_hook=unique_object)


def facts():
    need(hashlib.sha256(FACTS.read_bytes()).hexdigest() == FACTS_SHA, "approved facts SHA")
    return load(FACTS)["cases"]


def closed(value, keys, reason):
    need(isinstance(value, dict) and set(value) == set(keys), reason)


def runtime_inputs(source_facts, root=ROOT):
    manifest_path = root / "tests/fixtures/mermaid_official/manifest.json"
    need(hashlib.sha256(manifest_path.read_bytes()).hexdigest() == MANIFEST_SHA, "changed manifest input")
    manifest = load(manifest_path, 2_000_000)
    need(len(manifest["fixtures"]) == 2145, "frozen manifest inputs")
    paths = [manifest_path]
    for fact in source_facts:
        source_path = root / "tests/fixtures/mermaid_official/cases" / fact["path"]
        need(source_path.is_file() and source_path.stat().st_size <= 65536, "bounded raw input")
        raw = source_path.read_bytes()
        need(hashlib.sha256(raw).hexdigest() == fact["source_sha256"] and len(raw) == fact["source_bytes"], "changed raw source input")
        record = manifest["fixtures"][fact["manifest_global_ordinal"]]
        need(record["path"] == fact["path"] and record["sha256"] == fact["source_sha256"] and
             record["git_blob"] == fact["source_git_blob"] and record["bytes"] == len(raw), "manifest original identity")
        need(hashlib.sha1(b"blob " + str(len(raw)).encode("ascii") + b"\0" + raw).hexdigest() == record["git_blob"], "raw Git blob")
        paths.append(source_path)
    return paths


def fresh_inputs(report, paths):
    need(all(report.stat().st_mtime_ns >= path.stat().st_mtime_ns for path in paths), "stale runtime input verdict")


def validate_pair(native, official, source_facts):
    expected = {row["path"]: row for row in source_facts}
    need(len(expected) == 10, "ten unique complete originals")
    for report, schema in [(native, "mindmap10_native_source_scenes/v1"), (official, "mindmap10_official_local_verdict/v1")]:
        root_keys = {"schema", "source_facts_sha256", "local_ready", "cases"}
        if schema == "mindmap10_native_source_scenes/v1":
            root_keys |= {"originals", "errors", "semantic_mutations_rejected", "physical_mutations_rejected", "foreign_assertions_certified"}
        closed(report, root_keys, "closed compact root schema")
        need(report.get("schema") == schema and report.get("source_facts_sha256") == FACTS_SHA, "consumer identity/authority")
        need(type(report.get("local_ready")) is int and report["local_ready"] == 10, "consumer local-ready count")
        need(type(report.get("errors", 0)) is int and report.get("errors", 0) == 0, "consumer execution/resource errors")
        if schema == "mindmap10_native_source_scenes/v1":
            for key, expected_count in [("originals", 10), ("errors", 0), ("semantic_mutations_rejected", 30),
                                        ("physical_mutations_rejected", 30), ("foreign_assertions_certified", 0)]:
                need(type(report[key]) is int and report[key] == expected_count, "exact Native root totals")
        rows = report.get("cases")
        need(isinstance(rows, list) and len(rows) == 10, "ten consumer verdicts")
        seen = set()
        for row in rows:
            case_keys = {"path", "source_sha256", "complete_model_sha256", "terminal_projection_sha256", "local_ready",
                         "captures", "captures_before_binding", "bound_after_captures", "geometry_and_restoration_good",
                         "narrow_exact_ordinary", "narrow_geometry_good", "semantic_mutations_rejected", "physical_mutations_rejected"}
            if schema == "mindmap10_official_local_verdict/v1":
                case_keys |= {"source_facts_sha256", "foreign_assertions_certified", "native_report_input"}
            closed(row, case_keys, "closed compact case schema")
            if schema == "mindmap10_official_local_verdict/v1":
                need(row["source_facts_sha256"] == FACTS_SHA, "case authority")
                need(type(row["native_report_input"]) is bool and row["native_report_input"] is False and
                     type(row["foreign_assertions_certified"]) is int and row["foreign_assertions_certified"] == 0, "Official proof ownership")
            path = row.get("path")
            need(path in expected and path not in seen, "consumer original identity/uniqueness")
            seen.add(path)
            fact = expected[path]
            for key in ["source_sha256", "complete_model_sha256", "terminal_projection_sha256"]:
                need(row.get(key) == fact[key], f"consumer lock: {key}")
            need(row.get("local_ready") is True and "error" not in row, "consumer rejected source")
            need(type(row.get("captures")) is int and row["captures"] == 5, "five public captures")
            for key in ["captures_before_binding", "bound_after_captures", "geometry_and_restoration_good"]:
                need(row.get(key) is True, f"ordered proof: {key}")
            ordinary, geometry = row.get("narrow_exact_ordinary"), row.get("narrow_geometry_good")
            need(type(ordinary) is bool and type(geometry) is bool and ordinary != geometry, "narrow exact source or geometry")
            need(row.get("native_report_input", False) is False and row.get("foreign_assertions_certified", 0) == 0, "no borrowed/foreign proof")
            plans = fact["required_good_first_mutations"]
            semantic = sum(item["id"].startswith("semantic_") for item in plans)
            physical = len(plans) - semantic
            need(type(row.get("semantic_mutations_rejected")) is int and row["semantic_mutations_rejected"] == semantic, "exact semantic mutation verdicts")
            need(type(row.get("physical_mutations_rejected")) is int and row["physical_mutations_rejected"] == physical, "exact physical mutation verdicts")
        need(seen == set(expected), "complete original join")
    return [{key: row[key] for key in ["path", "source_sha256", "complete_model_sha256", "terminal_projection_sha256"]}
            for row in source_facts]


def fresh(directory: Path, report: str, executable: str, consumer: str):
    binary = directory / executable
    need(binary.is_file(), "consumer executable missing")
    need((directory / report).stat().st_mtime_ns >= binary.stat().st_mtime_ns, "stale consumer verdict")
    for source in COMMON + [consumer]:
        need(binary.stat().st_mtime_ns >= (ROOT / source).stat().st_mtime_ns, f"consumer not rebuilt: {source}")


def baseline(matrix):
    summary = matrix["Mindmap10_native_whole_source"]
    need(type(summary["base_admitted_originals"]) is int and summary["base_admitted_originals"] == BASE_ADMITTED_ORIGINALS,
         "frozen 616-original baseline must be preserved")
    need(summary["admitted"] == 0 and summary["join_required"] is True and
         matrix["original_drawing_coverage"]["drawn_at_240"] == BASE_ADMITTED_ORIGINALS,
         "matrix must start at frozen unjoined baseline")
    return BASE_ADMITTED_ORIGINALS


def verify(directory: Path, write: bool):
    native_name, official_name = "mermaid-mindmap10-native-verdict.json", "mermaid-mindmap10-official-verdict.json"
    native, official = load(directory / native_name), load(directory / official_name)
    source_facts = facts()
    joined = validate_pair(native, official, source_facts)
    inputs = [FACTS] + runtime_inputs(source_facts)
    fresh_inputs(directory / native_name, inputs); fresh_inputs(directory / official_name, inputs)
    fresh(directory, native_name, "test_mermaid_mindmap_semantics", "tests/ui/test_mermaid_mindmap_semantics.cpp")
    fresh(directory, official_name, "test_mermaid_official", "tests/ui/test_mermaid_official.cpp")
    path = directory / "mermaid-official-matrix.json"
    need(path.stat().st_mtime_ns >= (directory / official_name).stat().st_mtime_ns, "stale matrix")
    matrix = load(path, 20_000_000)
    need(matrix.get("integrity_errors") == [], "matrix integrity errors")
    summary = matrix["Mindmap10_native_whole_source"]
    need(summary["source_facts_sha256"] == FACTS_SHA and summary["selected"] == 10 and summary["local_ready"] == 10, "matrix local proof")
    originals = [row for row in matrix["cases"] if row["track"] == "original"]
    need(len(originals) == 2145 and len(matrix["cases"]) == 2738, "frozen original/diagnostic denominator")
    for proof in joined:
        row = next((row for row in originals if row["fixture"] == proof["path"]), None)
        need(row is not None and row["source_sha256"] == proof["source_sha256"] and row["errors"] == [], "matrix source identity/errors")
        need(row["observed"]["wide_240"] == "drawn" and row["observed"]["restored_240"] == "drawn", "matrix local geometry")
        local = next(value for value in official["cases"] if value["path"] == proof["path"])
        need(row["Mindmap10_native_whole_source"]["actual"] == {key: value for key, value in local.items() if key != "path"},
             "matrix and Official compact verdict differ")
        need(row["observed"]["narrow_40"] == ("source_fallback" if local["narrow_exact_ordinary"] else "drawn"), "matrix narrow verdict")
    base = baseline(matrix)
    result = {"schema": "mindmap10_dual_consumer_admission/v1", "source_facts_sha256": FACTS_SHA,
              "base_admitted_originals": base, "joined_originals": 10, "formal_admitted_originals": base + 10,
              "pinned_original_files": 2145, "foreign_assertions_certified": 0, "cases": joined}
    outputs = {}
    if write:
        summary["admitted"] = 10; summary["join_required"] = False
        matrix["Mindmap10_dual_consumer_join"] = result
        matrix["original_drawing_coverage"]["drawn_at_240"] = base + 10
        matrix["original_drawing_coverage"]["percent_of_pinned_original_files"] = 100.0 * (base + 10) / 2145
        outputs[path] = json.dumps(matrix, ensure_ascii=False, indent=2) + "\n"
        outputs[directory / "mermaid-mindmap10-dual-admission.json"] = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
        markdown = directory / "mermaid-official-matrix.md"
        text = markdown.read_text().split("\n## Mindmap10 formal admission\n", 1)[0]
        text += f"\n## Mindmap10 formal admission\n\nBoth independent consumers pass all ten source/model/projection locks and their five-frame/mutation verdicts. Formal coverage: **{base + 10}/2145 ({100.0 * (base + 10) / 2145:.2f}%)**. No frame is read or borrowed by this join.\n"
        outputs[markdown] = text
    print(f"OK {directory}: ten joined originals; formal {base + 10}/2145")
    return outputs


class JoinControls(unittest.TestCase):
    def test_rejects_missing_or_borrowed_or_incomplete_verdicts(self):
        source = facts(); rows = []
        for fact in source:
            row = {key: fact[key] for key in ["path", "source_sha256", "complete_model_sha256", "terminal_projection_sha256"]}
            row.update(local_ready=True, captures=5, captures_before_binding=True, bound_after_captures=True,
                       geometry_and_restoration_good=True, narrow_exact_ordinary=False, narrow_geometry_good=True)
            row["semantic_mutations_rejected"] = sum(p["id"].startswith("semantic_") for p in fact["required_good_first_mutations"])
            row["physical_mutations_rejected"] = len(fact["required_good_first_mutations"]) - row["semantic_mutations_rejected"]
            rows.append(row)
        native = {"schema": "mindmap10_native_source_scenes/v1", "source_facts_sha256": FACTS_SHA, "local_ready": 10, "cases": rows,
                  "originals": 10, "errors": 0, "semantic_mutations_rejected": 30, "physical_mutations_rejected": 30, "foreign_assertions_certified": 0}
        official = {"schema": "mindmap10_official_local_verdict/v1", "source_facts_sha256": FACTS_SHA, "local_ready": 10, "cases": copy.deepcopy(rows)}
        for row in official["cases"]:
            row.update(source_facts_sha256=FACTS_SHA, foreign_assertions_certified=0, native_report_input=False)
        self.assertEqual(len(validate_pair(native, official, source)), 10)
        for side in (0, 1):
            for key in ("frame", "cell_sha256", "pixels", "unknown"):
                pair = [copy.deepcopy(native), copy.deepcopy(official)]; pair[side][key] = {"nested": []}
                with self.subTest(root=key, side=side), self.assertRaises(ValueError):
                    validate_pair(*pair, source)
        mutations = [lambda r: r.pop(), lambda r: r.append(copy.deepcopy(r[0])),
                     lambda r: r[0].update(source_sha256="0" * 64), lambda r: r[0].update(complete_model_sha256="0" * 64),
                     lambda r: r[0].update(terminal_projection_sha256="0" * 64), lambda r: r[0].update(captures=4),
                     lambda r: r[0].update(bound_after_captures=False), lambda r: r[0].update(geometry_and_restoration_good=False),
                     lambda r: r[0].update(narrow_exact_ordinary=True), lambda r: r[0].update(native_report_input=True),
                     lambda r: r[0].update(semantic_mutations_rejected=0), lambda r: r[0].update(error="resource: exhausted"),
                     lambda r: r[0].update(five_frames=[]), lambda r: r[0].update(frame=[]),
                     lambda r: r[0].update(cell_sha256="0" * 64), lambda r: r[0].update(pixels={"nested": []}),
                     lambda r: r[0].update(unknown=True)]
        for mutation in mutations:
            for side in (0, 1):
                pair = [copy.deepcopy(native), copy.deepcopy(official)]; mutation(pair[side]["cases"])
                with self.subTest(side=side, mutation=mutations.index(mutation)), self.assertRaises(ValueError):
                    validate_pair(*pair, source)


    def test_regressed_or_unreviewed_baseline_is_rejected(self):
        matrix = {"Mindmap10_native_whole_source": {"base_admitted_originals": 616, "admitted": 0, "join_required": True},
                  "original_drawing_coverage": {"drawn_at_240": 616}}
        self.assertEqual(baseline(matrix), 616)
        for value in [615, 617]:
            bad = copy.deepcopy(matrix); bad["Mindmap10_native_whole_source"]["base_admitted_originals"] = value
            bad["original_drawing_coverage"]["drawn_at_240"] = value
            with self.assertRaisesRegex(ValueError, "616-original baseline"):
                baseline(bad)

    def test_changed_and_stale_runtime_inputs(self):
        import shutil
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = root / "tests/fixtures/mermaid_official/manifest.json"
            manifest.parent.mkdir(parents=True)
            shutil.copyfile(ROOT / "tests/fixtures/mermaid_official/manifest.json", manifest)
            source_facts = facts()
            for fact in source_facts:
                target = root / "tests/fixtures/mermaid_official/cases" / fact["path"]
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / "tests/fixtures/mermaid_official/cases" / fact["path"], target)
            inputs = runtime_inputs(source_facts, root)
            verdict = root / "verdict.json"; verdict.write_text("{}")
            fresh_inputs(verdict, inputs)
            os.utime(inputs[-1], ns=(verdict.stat().st_mtime_ns + 1_000_000, verdict.stat().st_mtime_ns + 1_000_000))
            with self.assertRaisesRegex(ValueError, "stale runtime"):
                fresh_inputs(verdict, inputs)
            inputs[-1].write_bytes(inputs[-1].read_bytes() + b"\n")
            with self.assertRaisesRegex(ValueError, "changed raw"):
                runtime_inputs(source_facts, root)
            manifest.write_bytes(manifest.read_bytes() + b" ")
            with self.assertRaisesRegex(ValueError, "changed manifest"):
                runtime_inputs(source_facts, root)


    def test_partial_publication_rolls_back_without_formal_markers(self):
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            matrix, markdown, marker = root / "matrix.json", root / "matrix.md", root / "mermaid-mindmap10-dual-admission.json"
            matrix.write_text("baseline"); markdown.write_text("baseline md"); marker.write_text("stale")
            replace = os.replace
            calls = 0
            def fail_second(source, target):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise OSError("injected publication failure")
                return replace(source, target)
            with patch("os.replace", side_effect=fail_second), self.assertRaises(OSError):
                publish_all({matrix: "joined", markdown: "joined md", marker: "formal"})
            self.assertEqual(matrix.read_text(), "baseline")
            self.assertEqual(markdown.read_text(), "baseline md")
            self.assertFalse(marker.exists())


def publish_all(outputs):
    # All inputs and both output bundles are prepared before any final path is
    # touched. The formal join markers appear only after every matrix/MD.
    markers = [path for path in outputs if path.name == "mermaid-mindmap10-dual-admission.json"]
    originals = {path: path.read_bytes() for path in outputs if path not in markers}
    temporary = {}
    try:
        for path, text in outputs.items():
            staged = path.with_name(path.name + ".join-tmp")
            staged.write_text(text, encoding="utf-8"); temporary[path] = staged
        for path in markers:
            path.unlink(missing_ok=True)
        for path in list(originals) + markers:
            os.replace(temporary[path], path)
    except OSError:
        for path, data in originals.items():
            path.write_bytes(data)
        for path in markers:
            path.unlink(missing_ok=True)
        raise
    finally:
        for path in temporary.values():
            path.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dirs", default="build/debug/tests/ui,build/release/tests/ui")
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return not unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(JoinControls)).wasSuccessful()
    directories = [(ROOT / value).resolve() for value in args.build_dirs.split(",")]
    if args.write:
        need({directory.parent.parent.name for directory in directories} == {"debug", "release"},
             "formal publication requires both Debug and Release")
    # Validate all four consumers and prepare both bundles before publication.
    outputs = {}
    for directory in directories:
        outputs.update(verify(directory, args.write))
    if args.write:
        publish_all(outputs)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
