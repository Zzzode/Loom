// D248 ER107 proposal-emitter stage 1: canonical-contract differential lock.
//
// The group-edge re-baseline needs an emitter that regenerates byte-identical
// ER107 proposal components from production parse->IR->render output. Before
// writing the RelationDiagram->JSON serializer this test pins the exact
// canonical contract it must reproduce, using the 107 already-accepted cases
// as ground truth (no production change, no frozen-gate movement):
//   * re-canonicalizing each stored whole_scene_model / source_binding /
//     terminal_model reproduces its pinned sha256, proving class481_canonical
//     is the single encoder an emitter must target;
//   * object key sets and marker/stroke/direction enums are closed and
//     uniform, so the serializer has a fixed field vocabulary;
//   * the population is exactly 107, which acts as a tripwire the 113-case
//     emitter milestone updates atomically with its pin/hash rewiring.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

import std;
import loom.serdes.json;
import loom.crypto.crypto;
import loom.ui.visual.mermaid.testing;

#include "mermaid_er107_authority.hpp"

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
using json::JsonDoc;
using json::JsonVal;
namespace rel = loom::test::mermaid_relations;
namespace er107 = loom::test::mermaid_er107;
namespace auth = er107::authority;

const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures";

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

JsonDoc load_projection() {
    auto doc = json::parse(read_file(kFixtures / std::string(auth::kProjectionPath)));
    EXPECT_TRUE(doc.has_value());
    return std::move(*doc);
}

std::string sha_of(const JsonVal& value) {
    rel::Work work;
    return loom::utils::crypto::sha256(rel::class481_canonical(value, work));
}

std::set<std::string> key_set(const JsonVal& object) {
    std::set<std::string> keys;
    object.iter_obj([&](const JsonVal key, const JsonVal) { keys.emplace(std::string(key.as_str())); });
    return keys;
}
}  // namespace

TEST(MermaidEr107EmitterContract, CanonicalReEncodingReproducesEveryPinnedComponentSha) {
    const auto doc = load_projection();
    const auto cases = doc.root().get("cases");
    ASSERT_EQ(cases.size(), 107u);  // tripwire: the emitter milestone moves 107 -> 113 here
    std::size_t checked = 0;
    cases.iter([&](const JsonVal c) {
        const auto tm = c.get("terminal_model");
        EXPECT_EQ(sha_of(tm), c.get_string("terminal_model_sha256"));
        EXPECT_EQ(sha_of(tm.get("whole_scene_model")), c.get_string("whole_scene_model_sha256"));
        EXPECT_EQ(sha_of(c.get("source_binding")), c.get_string("source_binding_sha256"));
        ++checked;
    });
    EXPECT_EQ(checked, 107u);
}

TEST(MermaidEr107EmitterContract, WholeSceneFieldVocabularyIsClosedAndUniform) {
    const auto doc = load_projection();
    const std::set<std::string> expect_entity{
        "headers", "id", "label", "owner", "rows", "shape", "typed_er"};
    const std::set<std::string> expect_group{"direction", "id", "label", "members", "parent"};
    const std::set<std::string> expect_relation{
        "end", "label", "order", "source", "start", "stroke", "target"};
    const std::set<std::string> expect_scene{
        "direction", "entities", "family", "groups", "notes", "relations"};
    const std::set<std::string> markers{
        "card:one", "card:one-many", "card:zero-many", "card:zero-one"};
    const std::set<std::string> strokes{"dotted", "solid"};
    const std::set<std::string> directions{"TB", "BT", "LR", "RL"};
    std::set<std::string> shapes, group_directions, top_directions;
    std::size_t entities = 0, groups = 0, relations = 0, notes = 0;
    doc.root().get("cases").iter([&](const JsonVal c) {
        const auto scene = c.get("terminal_model").get("whole_scene_model");
        EXPECT_EQ(key_set(scene), expect_scene);
        EXPECT_TRUE(directions.contains(scene.get_string("direction")));
        top_directions.emplace(scene.get_string("direction"));
        scene.get("entities").iter([&](const JsonVal e) {
            EXPECT_EQ(key_set(e), expect_entity);
            shapes.emplace(e.get_string("shape"));
            ++entities;
        });
        scene.get("groups").iter([&](const JsonVal g) {
            EXPECT_EQ(key_set(g), expect_group);
            // In-group direction never enters the model: the bounded renderer
            // owns layout, so a group's direction is always the empty string.
            EXPECT_EQ(g.get_string("direction"), "");
            group_directions.emplace(g.get_string("direction"));
            ++groups;
        });
        scene.get("relations").iter([&](const JsonVal r) {
            EXPECT_EQ(key_set(r), expect_relation);
            EXPECT_TRUE(markers.contains(r.get_string("start")));
            EXPECT_TRUE(markers.contains(r.get_string("end")));
            EXPECT_TRUE(strokes.contains(r.get_string("stroke")));
            ++relations;
        });
        notes += scene.get("notes").size();
    });
    EXPECT_EQ(shapes, (std::set<std::string>{"box"}));
    EXPECT_EQ(group_directions, (std::set<std::string>{""}));
    EXPECT_GT(entities, 0u);
    EXPECT_GT(groups, 0u);
    EXPECT_GT(relations, 0u);
    // ER originals carry no notes through this contract.
    EXPECT_EQ(notes, 0u);
    EXPECT_FALSE(top_directions.empty());
}

TEST(MermaidEr107EmitterContract, TypedAttributeAndBindingShapeIsStable) {
    const auto doc = load_projection();
    const std::set<std::string> expect_typed{"attributes", "header"};
    const std::set<std::string> expect_attribute{"comment", "keys", "name", "type"};
    const std::set<std::string> expect_row{"alternatives", "role", "source_fields"};
    const std::set<std::string> expect_binding{
        "body_ref", "config_obligations_sha256", "frontmatter_ref", "full_source_ref",
        "manifest_global_ordinal", "manifest_record_sha256", "metadata_sha256", "ordinal",
        "path", "prior_ER206_case_canonical_sha256", "prior_ER206_ordinal",
        "source_all_witness_ordinals", "source_asset_case_sha256", "source_body_sha256",
        "source_bytes", "source_git_blob", "source_model_sha256", "source_sha256",
        "source_witness_count", "source_witnesses_sha256"};
    std::size_t typed = 0, binding_checked = 0;
    doc.root().get("cases").iter([&](const JsonVal c) {
        c.get("terminal_model").get("whole_scene_model").get("entities").iter([&](const JsonVal e) {
            const auto typed_value = e.get("typed_er");
            if (typed_value.is_null()) return;
            EXPECT_EQ(key_set(typed_value), expect_typed);
            typed_value.get("attributes").iter([&](const JsonVal a) {
                EXPECT_EQ(key_set(a), expect_attribute);
            });
            e.get("rows").iter([&](const JsonVal row) {
                EXPECT_EQ(key_set(row), expect_row);
                EXPECT_EQ(key_set(row.get("source_fields")), expect_attribute);
            });
            ++typed;
        });
        EXPECT_EQ(key_set(c.get("source_binding")), expect_binding);
        ++binding_checked;
    });
    EXPECT_GT(typed, 0u);
    EXPECT_EQ(binding_checked, 107u);
}

// Stage 2: the production-driven seam must regenerate every pinned
// whole_scene_model byte-for-byte. This is the differential RED->GREEN backbone
// for the RelationDiagram->canonical serializer: parse each of the 107 accepted
// ER originals through the real production parse->IR entry, serialize, re-encode
// with the same canonical encoder the authority uses, and require byte-equal SHA.
TEST(MermaidEr107EmitterContract, ProductionSeamReproducesAll107WholeSceneModels) {
    const auto projection = load_projection();
    const auto cases = projection.root().get("cases");
    ASSERT_EQ(cases.size(), er107::kPins.size());
    const fs::path cases_dir = kFixtures / "mermaid_official/cases";
    std::size_t matched = 0;
    for (std::size_t i = 0; i < er107::kPins.size(); ++i) {
        SCOPED_TRACE(er107::kPins[i].path);
        const auto raw = read_file(cases_dir / std::string(er107::kPins[i].path));
        loom::ui::mermaid::testing::Work work;
        const auto emitted =
            loom::ui::mermaid::testing::er_whole_scene_json_for_testing(raw, work);
        ASSERT_TRUE(emitted.has_value());
        auto parsed = json::parse(*emitted);
        ASSERT_TRUE(parsed.has_value());
        rel::Work canonical_work;
        const auto canonical =
            rel::class481_canonical(parsed->root(), canonical_work);
        EXPECT_EQ(loom::utils::crypto::sha256(canonical),
                  cases.at(i).get_string("whole_scene_model_sha256"));
        ++matched;
    }
    EXPECT_EQ(matched, 107u);
}
