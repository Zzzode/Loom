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

// Stage 3: the structural-witness seam regenerates the seven span/tiling kinds
// for all 107 ER originals. Global witness ordinal is intentionally out of
// scope (it depends on the 24 semantic token kinds assembled later), so the
// differential matches by (kind, byte start, byte end) and verifies every other
// field, including sha256(lexeme) == the pinned slice hash. Offsets are UTF-8
// BYTE offsets (string_view/size semantics), not Unicode code-point counts.
namespace {
const std::set<std::string> kStructuralKinds{
    "complete_original_source", "complete_frontmatter", "complete_body_source",
    "frontmatter_open", "frontmatter_close", "diagram_header", "physical_line"};

JsonDoc load_source_proposal() {
    auto doc = json::parse(read_file(kFixtures / std::string(er107::kFactsPath)));
    EXPECT_TRUE(doc.has_value());
    return std::move(*doc);
}

std::map<std::tuple<std::string, int64_t, int64_t>, JsonVal>
structural_index(const JsonVal& witnesses) {
    std::map<std::tuple<std::string, int64_t, int64_t>, JsonVal> index;
    witnesses.iter([&](const JsonVal w) {
        if (!kStructuralKinds.contains(w.get_string("kind"))) return;
        index.emplace(std::tuple{w.get_string("kind"), w.get_int("source_byte_start"),
                                 w.get_int("source_byte_end")},
                      w);
    });
    return index;
}
}  // namespace

TEST(MermaidEr107EmitterContract, StructuralWitnessSeamReproducesAll107ByteExact) {
    const auto proposal = load_source_proposal();
    const fs::path cases_dir = kFixtures / "mermaid_official/cases";
    std::size_t full_match = 0;
    proposal.root().get("cases").iter([&](const JsonVal source_case) {
        const auto path = source_case.get_string("path");
        const auto raw = read_file(cases_dir / std::string(path));
        loom::ui::mermaid::testing::Work work;
        const auto emitted_json =
            loom::ui::mermaid::testing::er_structural_witnesses_json_for_testing(raw, work);
        ASSERT_TRUE(emitted_json.has_value());
        const auto emitted = json::parse(*emitted_json);
        ASSERT_TRUE(emitted.has_value());
        const auto pinned = structural_index(source_case.get("source_witnesses"));

        std::size_t matched = 0;
        emitted->root().iter([&](const JsonVal w) {
            const auto key = std::tuple{w.get_string("kind"), w.get_int("source_byte_start"),
                                        w.get_int("source_byte_end")};
            const auto found = pinned.find(key);
            ASSERT_NE(found, pinned.end()) << "unmatched structural witness " << std::get<0>(key)
                                           << " in " << path;
            const auto& p = found->second;
            EXPECT_EQ(w.get_string("source_lexeme"), p.get_string("source_lexeme"));
            EXPECT_EQ(w.get_int("source_line_start"), p.get_int("source_line_start"));
            EXPECT_EQ(w.get_int("source_line_end"), p.get_int("source_line_end"));
            EXPECT_EQ(w.get_int("source_utf8_bytes"), p.get_int("source_utf8_bytes"));
            EXPECT_EQ(loom::utils::crypto::sha256(w.get_string("source_lexeme")),
                      p.get_string("source_slice_sha256"));
            ++matched;
        });
        // Every pinned structural witness of this case must be reproduced.
        EXPECT_EQ(matched, pinned.size()) << path;
        ++full_match;
    });
    EXPECT_EQ(full_match, 107u);
}

// Regression guard for the byte-vs-code-point distinction: an ER original with
// multibyte UTF-8 (U+2192 arrow) has witness byte_end beyond its code-point
// count but bounded by the real UTF-8 byte length. The seam must emit byte
// offsets (string_view/size), never character indices.
TEST(MermaidEr107EmitterContract, StructuralOffsetsAreUtf8BytesNotCodePoints) {
    const auto proposal = load_source_proposal();
    const fs::path cases_dir = kFixtures / "mermaid_official/cases";
    constexpr std::string_view kMultibyteCase =
        "er-diagram/visual-check/a-cyclical-er-diagram-handdrawn-look.mmd";
    // Copy the PATH out while the by-value JsonVal handle is valid; holding the
    // handle's address across the lambda would dangle and resolve another case.
    std::string multibyte_path;
    proposal.root().get("cases").iter([&](const JsonVal c) {
        if (c.get_string("path") == kMultibyteCase)
            multibyte_path = std::string(c.get_string("path"));
    });
    ASSERT_FALSE(multibyte_path.empty());
    const auto raw = read_file(cases_dir / multibyte_path);
    // The fixture carries three U+2192 arrows, each the 3-byte UTF-8 sequence
    // E2 86 92. Counting code points as bytes that are not UTF-8 continuation
    // bytes (10xxxxxx) is sign-agnostic and yields 196 code points vs 202
    // bytes — the exact byte-vs-code-point trap character-indexed offsets hit.
    EXPECT_NE(raw.find("\xE2\x86\x92"), std::string::npos);
    std::size_t code_points = 0;
    for (unsigned char b : raw)
        if ((b & 0xC0) != 0x80) ++code_points;
    EXPECT_LT(code_points, raw.size());

    loom::ui::mermaid::testing::Work work;
    const auto emitted_json =
        loom::ui::mermaid::testing::er_structural_witnesses_json_for_testing(raw, work);
    ASSERT_TRUE(emitted_json.has_value());
    const auto emitted = json::parse(*emitted_json);
    ASSERT_TRUE(emitted.has_value());
    int64_t max_end = 0;
    emitted->root().iter(
        [&](const JsonVal w) { max_end = std::max(max_end, w.get_int("source_byte_end")); });
    // The seam reaches the true UTF-8 byte length (202), never the 196
    // code-point count: offsets are string_view/size byte offsets.
    EXPECT_EQ(max_end, static_cast<int64_t>(raw.size()));
}

// Fail-closed coverage for the seam's own input validation. It is only fed
// accepted ER107 originals, but must reject malformed frontmatter and any
// header production take("erDiagram", boundary=true) rejects, rather than emit
// witnesses for a non-ER source.
TEST(MermaidEr107EmitterContract, StructuralSeamFailClosedOnMalformedAndNonErInput) {
    loom::ui::mermaid::testing::Work work;
    auto emit = [&](std::string_view src) {
        return loom::ui::mermaid::testing::er_structural_witnesses_json_for_testing(src, work);
    };
    EXPECT_FALSE(emit("erDiagram\n").has_value());                     // no frontmatter
    EXPECT_FALSE(emit("---\nfoo").has_value());                        // no closing fence
    EXPECT_FALSE(emit("---\n---\nclassDiagram\n").has_value());        // wrong family
    EXPECT_FALSE(emit("---\n---\nerDiagramX\n").has_value());          // ASCII word boundary
    EXPECT_FALSE(emit("---\n---\nerDiagram\xC3\xA9\n").has_value());   // high byte is word()
    // A normal header followed by a newline stays admitted and emits spans.
    const auto ok = emit("---\n---\nerDiagram\n");
    ASSERT_TRUE(ok.has_value());
    const auto parsed = json::parse(*ok);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_GT(parsed->root().size(), 0u);
}

// D251 stage 4: the parallel semantic lexer regenerates the 24 semantic token
// kinds for all 107 ER originals, independent of production ErCursor. The
// differential is positional — the global 31-kind witness ordinal is assembled
// only at final integration, so the seam numbers its own list from zero — and
// compares every stored field at each position, including sha256(lexeme)
// against the pinned slice hash. The 4222-witness roster is a frozen tripwire.
namespace {
const std::set<std::string> kSemanticKinds{
    "attribute_comment_lexeme", "attribute_keys", "attribute_name",
    "attribute_statement", "attribute_type", "direction_statement",
    "direction_value", "entity_alias_lexeme", "entity_attribute_block",
    "entity_declaration", "entity_id", "metadata_key", "metadata_namespace",
    "metadata_scalar", "relationship_entity_reference",
    "relationship_identification", "relationship_left_cardinality",
    "relationship_right_cardinality", "relationship_role_content",
    "relationship_role_lexeme", "relationship_statement", "subgraph_end",
    "subgraph_header", "subgraph_id"};

std::vector<JsonVal> semantic_pinned(const JsonVal& witnesses) {
    std::vector<JsonVal> out;
    witnesses.iter([&](const JsonVal w) {
        if (kSemanticKinds.contains(w.get_string("kind"))) out.push_back(w);
    });
    return out;
}

constexpr std::size_t kSourceModelFields = 16;
}  // namespace

TEST(MermaidEr107EmitterContract, SemanticWitnessSeamReproducesAll107OrderedByteExact) {
    ASSERT_EQ(kSemanticKinds.size(), 24u);  // closed vocabulary tripwire
    const auto proposal = load_source_proposal();
    const fs::path cases_dir = kFixtures / "mermaid_official/cases";
    std::size_t full_match = 0;
    std::size_t total_witnesses = 0;
    proposal.root().get("cases").iter([&](const JsonVal source_case) {
        const auto path = source_case.get_string("path");
        const auto raw = read_file(cases_dir / std::string(path));
        loom::ui::mermaid::testing::Work work;
        const auto emitted_json =
            loom::ui::mermaid::testing::er_semantic_witnesses_json_for_testing(raw, work);
        ASSERT_TRUE(emitted_json.has_value()) << path;
        const auto emitted = json::parse(*emitted_json);
        ASSERT_TRUE(emitted.has_value());
        const auto pinned = semantic_pinned(source_case.get("source_witnesses"));
        ASSERT_EQ(emitted->root().size(), pinned.size()) << path;
        for (std::size_t i = 0; i < pinned.size(); ++i) {
            SCOPED_TRACE(std::string(path) + "#" + std::to_string(i));
            const auto w = emitted->root().at(i);
            const auto& p = pinned[i];
            ASSERT_EQ(w.get_string("kind"), p.get_string("kind"));
            EXPECT_EQ(w.get_int("source_byte_start"), p.get_int("source_byte_start"));
            EXPECT_EQ(w.get_int("source_byte_end"), p.get_int("source_byte_end"));
            EXPECT_EQ(w.get_string("source_lexeme"), p.get_string("source_lexeme"));
            EXPECT_EQ(w.get_int("source_line_start"), p.get_int("source_line_start"));
            EXPECT_EQ(w.get_int("source_line_end"), p.get_int("source_line_end"));
            EXPECT_EQ(w.get_int("source_utf8_bytes"), p.get_int("source_utf8_bytes"));
            EXPECT_EQ(w.get_int("ordinal"), static_cast<int64_t>(i));
            EXPECT_EQ(loom::utils::crypto::sha256(w.get_string("source_lexeme")),
                      p.get_string("source_slice_sha256"));
        }
        total_witnesses += pinned.size();
        ++full_match;
    });
    EXPECT_EQ(full_match, 107u);
    EXPECT_EQ(total_witnesses, 4222u);
}

// The semantic seam shares the structural seam's envelope admission and must
// fail closed on malformed frontmatter, a non-ER family, truncated body
// constructs, and any construct the production grammar rejects, rather than
// emit partial or zero-width witnesses. The last group pins the D251
// adversarial-review parity cases (empty alias/role, bad direction enum,
// unsupported comment/backtick surfaces).
TEST(MermaidEr107EmitterContract, SemanticSeamFailClosedOnMalformedAndNonErInput) {
    loom::ui::mermaid::testing::Work work;
    auto emit = [&](std::string_view src) {
        return loom::ui::mermaid::testing::er_semantic_witnesses_json_for_testing(src, work);
    };
    EXPECT_FALSE(emit("erDiagram\n").has_value());                    // no frontmatter
    EXPECT_FALSE(emit("---\nfoo").has_value());                       // no closing fence
    EXPECT_FALSE(emit("---\n---\nclassDiagram\n").has_value());       // wrong family
    EXPECT_FALSE(emit("---\n---\nerDiagramX\n").has_value());         // ASCII word boundary
    EXPECT_FALSE(emit("---\n---\nerDiagram\xC3\xA9\n").has_value());  // high byte is word()
    EXPECT_FALSE(emit("---\n---\nerDiagram\nA [\"unterminated\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nCUSTOMER {\n\"unterminated\n").has_value());
    // Production parity: empty unquoted alias, subgraph without an id, empty
    // unquoted role and a direction value outside the TB/BT/LR/RL enum.
    EXPECT_FALSE(emit("---\n---\nerDiagram\nA []\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nsubgraph\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nA ||--o{ B :\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\ndirection XYZ\n").has_value());
    // Unsupported-but-present grammar surfaces fail closed, never half-parse.
    EXPECT_FALSE(emit("---\n---\nerDiagram\n%% a comment\nA\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nA `x`\n").has_value());
    // Subgraph frame parity: a reserved id, an end without an open frame, and
    // an unclosed subgraph all fail production's group-frame validation.
    EXPECT_FALSE(emit("---\n---\nerDiagram\nsubgraph end\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nend\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nsubgraph G\nA\n").has_value());
    // A backslash is rejected by every production quoted/name path.
    EXPECT_FALSE(emit("---\n---\nerDiagram\nA\\\n").has_value());
    // A header alone is admitted but carries no semantic construct.
    const auto ok = emit("---\n---\nerDiagram\n");
    ASSERT_TRUE(ok.has_value());
    const auto parsed = json::parse(*ok);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->root().size(), 0u);
}

// D271 stage: the parallel source_model record builder must regenerate the
// frozen 16-field source_model for every ER107 original. The comparison is on
// the canonical (sorted-key, compact) encoding, so object insertion order is
// irrelevant while every array order is load-bearing; recomputing the digest
// must equal the pinned source_model_sha256.
TEST(MermaidEr107EmitterContract, SourceModelSeamReproducesAll107Canonical) {
    const auto proposal = load_source_proposal();
    const fs::path cases_dir = kFixtures / "mermaid_official/cases";
    std::size_t full_match = 0;
    std::size_t statements = 0, entities = 0, relationships = 0, attributes = 0;
    std::size_t subgraphs = 0, directions = 0, obligations = 0;
    proposal.root().get("cases").iter([&](JsonVal source_case) {
        const auto path = source_case.get_string("path");
        const auto raw = read_file(cases_dir / std::string(path));
        loom::ui::mermaid::testing::Work work;
        rel::Work canonical_work;
        const auto emitted_json =
            loom::ui::mermaid::testing::er_source_model_json_for_testing(raw, work);
        ASSERT_TRUE(emitted_json.has_value()) << path;
        const auto emitted = json::parse(*emitted_json);
        ASSERT_TRUE(emitted.has_value()) << path;
        ASSERT_EQ(emitted->root().size(), kSourceModelFields) << path;

        const auto emitted_canon =
            rel::class481_canonical(emitted->root(), canonical_work);
        const auto pinned_canon =
            rel::class481_canonical(source_case.get("source_model"), canonical_work);
        EXPECT_EQ(emitted_canon, pinned_canon) << path;
        EXPECT_EQ(loom::utils::crypto::sha256(emitted_canon),
                  source_case.get_string("source_model_sha256"))
            << path;

        statements += emitted->root().get("statements").size();
        entities += emitted->root().get("entities").size();
        relationships += emitted->root().get("relationships").size();
        subgraphs += emitted->root().get("subgraphs").size();
        directions += emitted->root().get("direction_declarations").size();
        obligations += emitted->root().get("config_semantic_obligations").size();
        emitted->root().get("entities").iter([&](JsonVal entity) {
            attributes += entity.get("attributes").size();
        });
        ++full_match;
    });
    EXPECT_EQ(full_match, 107u);
    // Frozen roster tripwires (catalog §0).
    EXPECT_EQ(statements, 391u);
    EXPECT_EQ(entities, 362u);
    EXPECT_EQ(relationships, 253u);
    EXPECT_EQ(attributes, 287u);
    EXPECT_EQ(subgraphs, 1u);
    EXPECT_EQ(directions, 4u);
    EXPECT_EQ(obligations, 181u);
}

// The source_model seam shares the D251 envelope and grammar admission, so the
// same hostile inputs must fail closed rather than emit a partial model.
TEST(MermaidEr107EmitterContract, SourceModelSeamFailClosedOnMalformedAndNonErInput) {
    loom::ui::mermaid::testing::Work work;
    auto emit = [&](std::string_view src) {
        return loom::ui::mermaid::testing::er_source_model_json_for_testing(src, work);
    };
    EXPECT_FALSE(emit("erDiagram\n").has_value());
    EXPECT_FALSE(emit("---\nfoo").has_value());
    EXPECT_FALSE(emit("---\n---\nclassDiagram\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagramX\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\xC3\xA9\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nA []\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nsubgraph\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nA ||--o{ B :\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\ndirection XYZ\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\n%% a comment\nA\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nA `x`\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nsubgraph end\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nend\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nsubgraph G\nA\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nA\\\n").has_value());
    // Unterminated quotes (the two D251 battery cases this seam must share).
    EXPECT_FALSE(emit("---\n---\nerDiagram\nA [\"unterminated\n").has_value());
    EXPECT_FALSE(emit("---\n---\nerDiagram\nCUSTOMER {\n\"unterminated\n").has_value());
    // Nested subgraphs are outside this record model's single-scope envelope.
    EXPECT_FALSE(
        emit("---\n---\nerDiagram\nsubgraph G\nsubgraph H\nB\nend\nend\n").has_value());
    // A clean header-only envelope still yields a well-formed (empty) model.
    const auto ok = emit("---\n---\nerDiagram\n");
    ASSERT_TRUE(ok.has_value());
    const auto parsed = json::parse(*ok);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->root().size(), kSourceModelFields);
}
