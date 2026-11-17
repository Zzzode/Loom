// RFC 0033 slice 2, phase 1: record-mode generator for the six-case ER
// group-edge complete proposal, plus the durable self-check that re-derives
// every embedded digest from the immutable fixture bytes.
//
// Two modes, one binary:
//   * default (CTest): hermetic in-memory generation. Every Stage A test-only
//     seam is driven over the six group-edge fixtures and every embedded
//     sha256 is re-derived from the reparsed proposal tree; no source-tree
//     writes. If the checked-in proposal exists it must be byte-identical to
//     the freshly generated document (a regeneration tripwire).
//   * LOOM_RFC0033_RECORD_OUTPUT=<path>: additionally writes the pretty,
//     byte-stable proposal JSON to that path. The checked-in artifact is only
//     ever produced by an explicit record run; CTest itself never writes it.
//
// The independent source leg never calls a production parser: source_model,
// source_witnesses and source_binding come exclusively from the D251/D271/D275
// parallel seams (facade loom::ui::mermaid::testing). whole_scene_model is the
// production parse leg that the later field-by-field cross-check validates.
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

import std;
import loom.serdes.json;
import loom.crypto.crypto;
import loom.ui.visual.mermaid.testing;

#include "mermaid_relations_oracle.hpp"

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
using json::JsonDoc;
using json::JsonMutDoc;
using json::JsonMutVal;
using json::JsonVal;
namespace mtest = loom::ui::mermaid::testing;
namespace rel = loom::test::mermaid_relations;

const fs::path kTestsDir{LOOM_TESTS_DIR};
const fs::path kCasesDir = kTestsDir / "fixtures/mermaid_official/cases";
const fs::path kManifestPath = kTestsDir / "fixtures/mermaid_official/manifest.json";
const fs::path kPriorPath =
    kTestsDir / "fixtures/mermaid_native/er206_native_whole_source_authority.config62.v1.json";
const fs::path kProposalPath =
    kTestsDir / "fixtures/mermaid_native/er_groupedge_complete_proposal.v1.json";
constexpr std::string_view kRecordEnv = "LOOM_RFC0033_RECORD_OUTPUT";
constexpr std::string_view kSchema = "loom_mermaid_er_groupedge_complete_proposal/v1";
constexpr std::string_view kContract = "ERGroupEdge/complete-original/v1";
constexpr std::string_view kGroupCollisionDebt =
    "raw_entity_group_kind_collision_not_representable_without_loss";
constexpr std::string_view kActiveConfigDebt = "active_config";
constexpr std::string_view kWorkCeilingBlocker = "er-envelope-double-parse-work-ceiling";
// Ordinal 198 (handDrawn nested-direction variant) is the reviewed Work-ceiling
// row. Its exclusion is sha-bound, never text-decided (RFC 0033 slice 2 §1.3).
constexpr std::string_view kHanddrawnSha =
    "c48919288f56b2cb5bae9050ff75afa41faca478d0d77c0f6cc442e86465954b";

// Prior-ordinal order, which is also the cohort case order.
constexpr std::array<std::pair<std::string_view, int>, 6> kCohort{{
    {"er-diagram/nightly-check/should-render-relationships-between-subgraphs.mmd", 26},
    {"er-diagram/nightly-check/should-render-subgraphs-with-direction-override.mmd", 27},
    {"er-diagram/nightly-check/should-render-subgraphs-with-quoted-ids.mmd", 29},
    {"er-diagram/pr-check/should-render-relationships-between-subgraph-and-entity.mmd", 39},
    {"er-diagram/visual-check/nested-er-subgraphs-with-direction-override-default.mmd", 196},
    {"er-diagram/visual-check/nested-er-subgraphs-with-direction-override-elk-layout.mmd", 197},
}};

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::string sha_of(const JsonVal& value) {
    rel::Work work;
    return loom::utils::crypto::sha256(rel::class481_canonical(value, work));
}

std::string sha_bytes(std::string_view bytes) {
    return loom::utils::crypto::sha256(bytes);
}

JsonDoc parse_or_die(const std::string& text, std::string_view what) {
    auto parsed = json::parse(text);
    EXPECT_TRUE(parsed.has_value()) << what;
    return std::move(*parsed);
}

std::size_t line_of(const std::string& raw, std::size_t index) {
    std::size_t line = 1;
    for (std::size_t i = 0; i < index && i < raw.size(); ++i)
        if (raw[i] == '\n') ++line;
    return line;
}

// Merge the structural (D248) and semantic (D251) seam arrays into the global
// witness catalog layout: five fixed structural spans, P physical lines, the
// M metadata semantic witnesses, the diagram_header, then the body semantic
// witnesses. Every witness gains its global ordinal and source_slice_sha256.
// The merged array is built in its own document and returned as serialized
// text; callers embed it through raw_json and parse it for digesting.
std::string merge_witnesses(const JsonVal& structural, const JsonVal& semantic,
                            const std::string& raw, std::size_t& witness_count,
                            std::size_t& metadata_count) {
    std::size_t body_start = 0;
    structural.iter([&](JsonVal w) {
        if (w.get_string("kind") == "complete_frontmatter")
            body_start = static_cast<std::size_t>(w.get_int("source_byte_end"));
    });
    const std::size_t physical = structural.size() - 6;  // 5 fixed + P lines + header
    metadata_count = 0;
    semantic.iter([&](JsonVal w) {
        if (w.get_int("source_byte_start") < static_cast<int64_t>(body_start)) ++metadata_count;
    });
    JsonMutDoc fragment;
    auto merged = fragment.array();
    int64_t ordinal = 0;
    auto append_witness = [&](const JsonVal& w) {
        auto copy = fragment.copy_val(w);
        copy.set("ordinal", ordinal);
        const auto start = static_cast<std::size_t>(w.get_int("source_byte_start"));
        const auto end = static_cast<std::size_t>(w.get_int("source_byte_end"));
        const std::string lexeme = raw.substr(start, end - start);
        EXPECT_EQ(lexeme, w.get_string("source_lexeme"));
        copy.set("source_slice_sha256", sha_bytes(lexeme));
        merged.append(copy);
        ++ordinal;
    };
    for (std::size_t i = 0; i < 5; ++i) append_witness(structural.at(i));
    for (std::size_t i = 5; i < 5 + physical; ++i) {
        EXPECT_EQ(structural.at(i).get_string("kind"), "physical_line");
        append_witness(structural.at(i));
    }
    // Catalog layout (D271 §2.4, identical to the ER107 frozen 107): after the
    // physical lines come the M metadata semantic witnesses, then the
    // diagram_header, then the body semantic witnesses.
    for (std::size_t i = 0; i < metadata_count; ++i) append_witness(semantic.at(i));
    EXPECT_EQ(structural.at(5 + physical).get_string("kind"), "diagram_header");
    append_witness(structural.at(5 + physical));
    for (std::size_t i = metadata_count; i < semantic.size(); ++i) append_witness(semantic.at(i));
    witness_count = static_cast<std::size_t>(ordinal);
    fragment.set_root(merged);
    return fragment.to_string();
}
}  // namespace

TEST(MermaidErGroupEdgeProposal, RecordsSixCaseProposalAndReDerivesEveryDigest) {
    auto manifest = parse_or_die(read_file(kManifestPath), "manifest");
    auto prior = parse_or_die(read_file(kPriorPath), "ER206 prior");
    const auto& fixtures = manifest.root().get("fixtures");
    const auto& prior_cases = prior.root().get("cases");
    ASSERT_EQ(prior_cases.size(), 206u);

    // Live cohort predicates over the immutable prior (the four-part assertion
    // is completed by the Python verifier; here we prove the C++ side inputs).
    const std::string gc(kGroupCollisionDebt), ac(kActiveConfigDebt);
    auto debt_kinds = [&](JsonVal row) {
        std::set<std::string> kinds;
        row.get("whole_null_debts").iter([&](JsonVal d) { kinds.insert(d.get_string("kind")); });
        return kinds;
    };
    std::vector<int> loose, exact, active_config;
    prior_cases.iter([&](JsonVal row) {
        if (!row.get("whole_model").is_null()) return;
        const auto kinds = debt_kinds(row);
        if (kinds.contains(gc)) loose.push_back(static_cast<int>(row.get_int("ordinal")));
        if (kinds == std::set<std::string>{gc})
            exact.push_back(static_cast<int>(row.get_int("ordinal")));
        if (kinds == std::set<std::string>{ac})
            active_config.push_back(static_cast<int>(row.get_int("ordinal")));
    });
    EXPECT_EQ(loose, (std::vector<int>{26, 27, 29, 39, 196, 197, 198}));
    EXPECT_EQ(exact, (std::vector<int>{26, 27, 29, 39, 196, 197}));
    EXPECT_EQ(active_config.size(), 107u);
    EXPECT_TRUE(std::ranges::find(active_config, 198) == active_config.end());

    JsonMutDoc doc;
    auto root = doc.object();
    root.set("schema", kSchema);
    root.set("rfc", static_cast<int64_t>(33));
    root.set("contract", kContract);
    root.set("canonical_encoding",
             "sorted UTF-8 keys, compact separators (',',':'), raw UTF-8; each embedded sha256 "
             "is sha256 over this canonical encoding of the named subtree");
    auto upstream = root.make_obj();
    upstream.set("tag", manifest.root().get("upstream").get_string("tag"));
    upstream.set("commit", manifest.root().get("upstream").get_string("commit"));
    upstream.set("manifest_sha256", sha_bytes(read_file(kManifestPath)));
    root.add("upstream", upstream);

    auto selection = root.make_obj();
    selection.set("cohort_debt_kind", kGroupCollisionDebt);
    auto loose_arr = selection.make_arr();
    for (int o : loose) loose_arr.append(loose_arr.make_num(o));
    selection.add("loose_superset_prior_ordinals", loose_arr);
    auto exact_arr = selection.make_arr();
    for (int o : exact) exact_arr.append(exact_arr.make_num(o));
    selection.add("exact_cohort_prior_ordinals", exact_arr);
    selection.set("er107_active_config_cohort_size", static_cast<int64_t>(active_config.size()));
    selection.set("cohort_intersection_empty", true);
    const auto& handdrawn = prior_cases.at(198);
    ASSERT_EQ(debt_kinds(handdrawn), (std::set<std::string>{ac, gc}));
    auto blocker = selection.make_obj();
    blocker.set("prior_ordinal", static_cast<int64_t>(198));
    blocker.set("path", handdrawn.get_string("fixture"));
    blocker.set("source_sha256", kHanddrawnSha);
    auto blocker_debts = blocker.make_arr();
    blocker_debts.append(blocker_debts.make_str(ac));
    blocker_debts.append(blocker_debts.make_str(gc));
    blocker.add("debt_kinds", blocker_debts);
    blocker.set("blocker_kind", kWorkCeilingBlocker);
    blocker.set("evidence_sha256", kHanddrawnSha);
    blocker.set("reason",
                "whole-source envelope double-parse Work ceiling: the look:handDrawn envelope "
                "event on top of the deepest nested body exhausts the ER scene Work budget "
                "before layout; budget fit is not textually decidable, so the exclusion is "
                "path+sha bound");
    selection.add("excluded_work_ceiling", blocker);
    root.add("selection", selection);

    auto cases = root.make_arr();
    auto census_rows = root.make_arr();
    std::int64_t total_entities = 0, total_relationships = 0, total_subgraphs = 0;
    std::int64_t total_directions = 0, total_attributes = 0, total_witnesses = 0;
    // Aggregate component preimages under one document root.
    JsonMutDoc pre_doc;
    auto pre_root = pre_doc.object();
    auto pre_whole = pre_root.make_arr(), pre_models = pre_root.make_arr(),
         pre_witnesses = pre_root.make_arr(), pre_bindings = pre_root.make_arr(),
         pre_manifest = pre_root.make_arr();
    pre_root.add("whole_scene_models", pre_whole);
    pre_root.add("source_models", pre_models);
    pre_root.add("source_witnesses", pre_witnesses);
    pre_root.add("source_bindings", pre_bindings);
    pre_root.add("manifest_records", pre_manifest);
    pre_doc.set_root(pre_root);

    for (std::size_t cohort_index = 0; cohort_index < kCohort.size(); ++cohort_index) {
        const auto [rel_path, prior_ordinal] = kCohort[cohort_index];
        const std::string path(rel_path);
        const std::string raw = read_file(kCasesDir / path);
        ASSERT_FALSE(raw.empty()) << path;
        const std::string raw_sha = sha_bytes(raw);

        // Manifest identity (global ordinal is the fixture array index).
        std::optional<JsonVal> manifest_fixture;
        std::size_t global_ordinal = 0;
        for (std::size_t i = 0; i < fixtures.size(); ++i)
            if (fixtures.at(i).get_string("path") == path) {
                manifest_fixture = fixtures.at(i);
                global_ordinal = i;
                break;
            }
        ASSERT_TRUE(manifest_fixture.has_value()) << path;
        EXPECT_EQ(manifest_fixture->get_string("sha256"), raw_sha) << path;
        EXPECT_EQ(manifest_fixture->get_int("bytes"), static_cast<int64_t>(raw.size())) << path;

        // Read-only prior identity.
        const auto prior_case = prior_cases.at(static_cast<std::size_t>(prior_ordinal));
        EXPECT_EQ(prior_case.get_string("fixture"), path);
        EXPECT_TRUE(prior_case.get("whole_model").is_null());
        EXPECT_EQ(debt_kinds(prior_case), std::set<std::string>{gc});
        EXPECT_EQ(prior_case.get_int("manifest_global_ordinal"),
                  static_cast<int64_t>(global_ordinal));
        const std::string prior_case_sha = sha_of(prior_case);
        const std::string manifest_record_sha = sha_of(*manifest_fixture);

        // Independent source leg: zero production-parser calls.
        mtest::Work whole_work, model_work, structural_work, semantic_work, binding_work;
        const auto whole_text = mtest::er_whole_scene_json_for_testing(raw, whole_work);
        const auto model_text = mtest::er_source_model_json_for_testing(raw, model_work);
        const auto structural_text =
            mtest::er_structural_witnesses_json_for_testing(raw, structural_work);
        const auto semantic_text =
            mtest::er_semantic_witnesses_json_for_testing(raw, semantic_work);
        // Phase-1 designed red gate: if er_src_collect_all rejects one of the
        // six, these seams return nullopt and the assertion fails here.
        ASSERT_TRUE(whole_text.has_value()) << path;
        ASSERT_TRUE(model_text.has_value()) << path;
        ASSERT_TRUE(structural_text.has_value()) << path;
        ASSERT_TRUE(semantic_text.has_value()) << path;

        auto whole = parse_or_die(*whole_text, "whole_scene " + path);
        auto model = parse_or_die(*model_text, "source_model " + path);
        auto structural = parse_or_die(*structural_text, "structural " + path);
        auto semantic = parse_or_die(*semantic_text, "semantic " + path);
        const std::string whole_sha = sha_of(whole.root());
        const std::string model_sha = sha_of(model.root());

        std::size_t witness_count = 0, metadata_count = 0;
        const std::string witnesses_text = merge_witnesses(
            structural.root(), semantic.root(), raw, witness_count, metadata_count);
        // Immutable view for the digest and embedding through raw_json.
        auto witness_reparse = parse_or_die(witnesses_text, "witnesses " + path);
        const std::string witnesses_sha = sha_of(witness_reparse.root());

        // Explicit, acyclic source-asset preimage (all inputs exist before the
        // binding seam runs, so source_asset_case_sha256 is never self-referential).
        JsonMutDoc asset_doc;
        auto asset = asset_doc.object();
        asset.set("path", path);
        asset.set("source_sha256", raw_sha);
        asset.set("source_model_sha256", model_sha);
        asset.set("source_witnesses_sha256", witnesses_sha);
        asset.set("manifest_record_sha256", manifest_record_sha);
        asset.set("prior_ER206_case_canonical_sha256", prior_case_sha);
        asset_doc.set_root(asset);
        auto asset_reparse = parse_or_die(asset_doc.to_string(), "asset preimage " + path);
        const std::string asset_sha = sha_of(asset_reparse.root());

        mtest::ErSourceProvenance provenance;
        provenance.path = path;
        provenance.ordinal = static_cast<int64_t>(cohort_index);
        provenance.manifest_global_ordinal = static_cast<int64_t>(global_ordinal);
        provenance.source_bytes = static_cast<int64_t>(raw.size());
        provenance.source_sha256 = raw_sha;
        provenance.source_git_blob = manifest_fixture->get_string("git_blob");
        provenance.manifest_record_sha256 = manifest_record_sha;
        provenance.prior_ER206_ordinal = static_cast<int64_t>(prior_ordinal);
        provenance.prior_ER206_case_canonical_sha256 = prior_case_sha;
        provenance.source_asset_case_sha256 = asset_sha;
        provenance.source_model_sha256 = model_sha;
        provenance.source_witnesses_sha256 = witnesses_sha;
        const auto binding_text =
            mtest::er_source_binding_json_for_testing(raw, binding_work, provenance);
        ASSERT_TRUE(binding_text.has_value()) << path;
        auto binding = parse_or_die(*binding_text, "source_binding " + path);
        const std::string binding_sha = sha_of(binding.root());
        // The binding seam independently computes the same global witness count.
        EXPECT_EQ(binding.root().get_int("source_witness_count"),
                  static_cast<int64_t>(witness_count))
            << path;

        auto case_obj = cases.make_obj();
        case_obj.set("path", path);
        case_obj.set("cohort_ordinal", static_cast<int64_t>(cohort_index));
        case_obj.set("prior_ER206_ordinal", static_cast<int64_t>(prior_ordinal));
        case_obj.set("manifest_global_ordinal", static_cast<int64_t>(global_ordinal));
        case_obj.set("source_bytes", static_cast<int64_t>(raw.size()));
        case_obj.set("source_sha256", raw_sha);
        case_obj.set("source_git_blob", manifest_fixture->get_string("git_blob"));
        case_obj.add("manifest_record", doc.copy_val(*manifest_fixture));
        case_obj.set("manifest_record_sha256", manifest_record_sha);
        case_obj.set("prior_ER206_case_canonical_sha256", prior_case_sha);
        auto debt_arr = case_obj.make_arr();
        debt_arr.append(debt_arr.make_str(kGroupCollisionDebt));
        case_obj.add("selection_debt_kinds", debt_arr);
        case_obj.add("source_asset_preimage", doc.copy_val(asset_reparse.root()));
        case_obj.set("source_asset_case_sha256", asset_sha);
        case_obj.add("whole_scene_model", doc.copy_val(whole.root()));
        case_obj.set("whole_scene_model_sha256", whole_sha);
        case_obj.add("source_model", doc.copy_val(model.root()));
        case_obj.set("source_model_sha256", model_sha);
        case_obj.add("source_witnesses", doc.copy_val(witness_reparse.root()));
        case_obj.set("source_witnesses_sha256", witnesses_sha);
        case_obj.add("source_binding", doc.copy_val(binding.root()));
        case_obj.set("source_binding_sha256", binding_sha);

        const auto& src_model = model.root();
        std::int64_t attributes = 0;
        src_model.get("entities").iter(
            [&](JsonVal e) { attributes += e.get("attributes").size(); });
        JsonMutDoc counts_doc;
        auto counts = counts_doc.object();
        counts.set("entities", static_cast<int64_t>(src_model.get("entities").size()));
        counts.set("relationships",
                   static_cast<int64_t>(src_model.get("relationships").size()));
        counts.set("subgraphs", static_cast<int64_t>(src_model.get("subgraphs").size()));
        counts.set("direction_declarations",
                   static_cast<int64_t>(src_model.get("direction_declarations").size()));
        counts.set("attributes", attributes);
        counts.set("source_witness_count", static_cast<int64_t>(witness_count));
        counts_doc.set_root(counts);
        const std::string counts_text = counts_doc.to_string();
        case_obj.add("field_counts", doc.raw_json(counts_text));
        cases.append(case_obj);

        total_entities += src_model.get("entities").size();
        total_relationships += src_model.get("relationships").size();
        total_subgraphs += src_model.get("subgraphs").size();
        total_directions += src_model.get("direction_declarations").size();
        total_attributes += attributes;
        total_witnesses += witness_count;

        auto census = census_rows.make_obj();
        census.set("path", path);
        census.set("prior_ER206_ordinal", static_cast<int64_t>(prior_ordinal));
        census.add("field_counts", doc.raw_json(counts_text));
        census_rows.append(census);

        auto add_preimage = [&](JsonMutVal arr, const std::string& component,
                                const JsonVal& value) {
            auto entry = arr.make_obj();
            entry.set("path", path);
            entry.add(component, pre_doc.copy_val(value));
            arr.append(entry);
        };
        add_preimage(pre_whole, "whole_scene_model", whole.root());
        add_preimage(pre_models, "source_model", model.root());
        add_preimage(pre_witnesses, "source_witnesses", witness_reparse.root());
        add_preimage(pre_bindings, "source_binding", binding.root());
        {
            auto entry = pre_manifest.make_obj();
            entry.set("path", path);
            entry.add("manifest_record", pre_doc.copy_val(*manifest_fixture));
            pre_manifest.append(entry);
        }
    }
    root.add("cases", cases);

    // Aggregate component digests, computed from the immutable reparsed preimage.
    auto pre_reparse = parse_or_die(pre_doc.to_string(), "component preimages");
    auto hashes = root.make_obj();
    hashes.set("whole_scene_models_sha256", sha_of(pre_reparse.root().get("whole_scene_models")));
    hashes.set("source_models_sha256", sha_of(pre_reparse.root().get("source_models")));
    hashes.set("source_witnesses_sha256", sha_of(pre_reparse.root().get("source_witnesses")));
    hashes.set("source_bindings_sha256", sha_of(pre_reparse.root().get("source_bindings")));
    hashes.set("manifest_records_sha256", sha_of(pre_reparse.root().get("manifest_records")));
    root.add("component_hashes", hashes);

    auto census = root.make_obj();
    auto totals = census.make_obj();
    totals.set("entities", total_entities);
    totals.set("relationships", total_relationships);
    totals.set("subgraphs", total_subgraphs);
    totals.set("direction_declarations", total_directions);
    totals.set("attributes", total_attributes);
    totals.set("source_witness_count", total_witnesses);
    census.add("totals", totals);
    census.add("per_case", census_rows);
    root.add("field_census", census);
    root.set("review_status", "PENDING_INDEPENDENT_REVIEW");
    doc.set_root(root);

    const std::string pretty = doc.to_pretty_string() + "\n";
    if (const char* output = std::getenv(kRecordEnv.data()); output != nullptr) {
        std::ofstream out(output, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.good()) << output;
        out.write(pretty.data(), static_cast<std::streamsize>(pretty.size()));
        ASSERT_TRUE(out.good()) << output;
    }

    // Reparse the serialized proposal and re-derive every embedded digest from
    // its own bytes: the durable, hermetic self-check.
    auto reparsed = parse_or_die(pretty, "serialized proposal");
    const auto reproot = reparsed.root();
    std::int64_t check_entities = 0, check_relationships = 0, check_subgraphs = 0;
    std::int64_t check_directions = 0, check_attributes = 0, check_witnesses = 0;
    reproot.get("cases").iter([&](JsonVal case_value) {
        const std::string path = case_value.get_string("path");
        const std::string raw = read_file(kCasesDir / path);
        EXPECT_EQ(sha_bytes(raw), case_value.get_string("source_sha256")) << path;
        EXPECT_EQ(sha_of(case_value.get("whole_scene_model")),
                  case_value.get_string("whole_scene_model_sha256"))
            << path;
        EXPECT_EQ(sha_of(case_value.get("source_model")),
                  case_value.get_string("source_model_sha256"))
            << path;
        EXPECT_EQ(sha_of(case_value.get("source_witnesses")),
                  case_value.get_string("source_witnesses_sha256"))
            << path;
        EXPECT_EQ(sha_of(case_value.get("source_binding")),
                  case_value.get_string("source_binding_sha256"))
            << path;
        EXPECT_EQ(sha_of(case_value.get("manifest_record")),
                  case_value.get_string("manifest_record_sha256"))
            << path;
        EXPECT_EQ(sha_of(case_value.get("source_asset_preimage")),
                  case_value.get_string("source_asset_case_sha256"))
            << path;
        // Live witness checks: bounded spans, raw-byte equality, slice digest,
        // and line recomputation for every witness of every case.
        case_value.get("source_witnesses").iter([&](JsonVal w) {
            const auto start = static_cast<std::size_t>(w.get_int("source_byte_start"));
            const auto end = static_cast<std::size_t>(w.get_int("source_byte_end"));
            ASSERT_LE(start, end);
            ASSERT_LE(end, raw.size()) << path;
            const std::string slice = raw.substr(start, end - start);
            EXPECT_EQ(slice, w.get_string("source_lexeme")) << path;
            EXPECT_EQ(sha_bytes(slice), w.get_string("source_slice_sha256")) << path;
            EXPECT_EQ(w.get_int("source_utf8_bytes"), static_cast<int64_t>(end - start)) << path;
            EXPECT_EQ(w.get_int("source_line_start"),
                      static_cast<int64_t>(line_of(raw, start)))
                << path;
            EXPECT_EQ(w.get_int("source_line_end"),
                      static_cast<int64_t>(line_of(raw, std::max(start, end - 1))))
                << path;
        });
        std::optional<JsonVal> full;
        case_value.get("source_witnesses").iter([&](JsonVal w) {
            if (w.get_string("kind") == "complete_original_source") full = w;
        });
        ASSERT_TRUE(full.has_value()) << path;
        EXPECT_EQ(full->get_string("source_lexeme"), raw) << path;

        const auto counts = case_value.get("field_counts");
        check_entities += counts.get_int("entities");
        check_relationships += counts.get_int("relationships");
        check_subgraphs += counts.get_int("subgraphs");
        check_directions += counts.get_int("direction_declarations");
        check_attributes += counts.get_int("attributes");
        check_witnesses += counts.get_int("source_witness_count");
    });
    EXPECT_EQ(check_entities, total_entities);
    EXPECT_EQ(check_relationships, total_relationships);
    EXPECT_EQ(check_subgraphs, total_subgraphs);
    EXPECT_EQ(check_directions, total_directions);
    EXPECT_EQ(check_attributes, total_attributes);
    EXPECT_EQ(check_witnesses, total_witnesses);
    ASSERT_EQ(reproot.get("cases").size(), 6u);
    const auto stored_hashes = reproot.get("component_hashes");
    EXPECT_EQ(sha_of(pre_reparse.root().get("whole_scene_models")),
              stored_hashes.get_string("whole_scene_models_sha256"));
    EXPECT_EQ(sha_of(pre_reparse.root().get("source_models")),
              stored_hashes.get_string("source_models_sha256"));
    EXPECT_EQ(sha_of(pre_reparse.root().get("source_witnesses")),
              stored_hashes.get_string("source_witnesses_sha256"));
    EXPECT_EQ(sha_of(pre_reparse.root().get("source_bindings")),
              stored_hashes.get_string("source_bindings_sha256"));
    EXPECT_EQ(sha_of(pre_reparse.root().get("manifest_records")),
              stored_hashes.get_string("manifest_records_sha256"));

    // Regeneration tripwire: a checked-in proposal must match the live document.
    if (fs::exists(kProposalPath))
        EXPECT_EQ(read_file(kProposalPath), pretty)
            << "checked-in proposal is stale; regenerate with LOOM_RFC0033_RECORD_OUTPUT";
}
