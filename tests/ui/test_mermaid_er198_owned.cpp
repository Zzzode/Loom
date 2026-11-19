// RFC 0034 phase 2, increment 2b: unit-level ownership test for the one-row
// ER198 cohort. Drives the official support header end to end over the
// immutable ordinal-198 fixture: Documents loading, fifteen captures,
// source/cross-check binding, footer-aware complete-frame geometry, mutation
// census, and the closed owned() gate. Tampered-byte and not-ready negatives
// prove the gate rejects a mutated or silently-incomplete proof.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

import std;
import loom.serdes.json;
import loom.crypto.crypto;
import loom.ui.visual.markdown;
import loom.ui.visual.mermaid;

#include "mermaid_er198_official.hpp"
#include "mermaid_er107_authority.hpp"

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
namespace er198 = loom::test::mermaid_er198;
namespace official = loom::test::mermaid_er198::official;
namespace er107 = loom::test::mermaid_er107;

TEST(MermaidEr198Owned, SingleProofIsOwnedAndCarriesCohortControls) {
    official::Documents documents;
    ASSERT_EQ(er198::kPins.size(), 1u);
    std::ostringstream diagnostics;
    const auto result = official::run(documents, 0, diagnostics);
    ASSERT_TRUE(result.ready) << diagnostics.str();
    ASSERT_TRUE(result.captured);
    ASSERT_TRUE(result.bound);
    const auto parsed = json::parse(result.proof);
    ASSERT_TRUE(parsed.has_value());
    const auto proof = parsed->root();
    EXPECT_TRUE(official::owned(proof));
    EXPECT_EQ(proof.get_string("contract"), official::kContract);
    EXPECT_EQ(proof.get_int("captures"), 15);
    EXPECT_TRUE(proof.get("captures_before_binding").as_bool());
    EXPECT_TRUE(proof.get("bound_after_captures").as_bool());
    EXPECT_TRUE(proof.get("uncached_cached_five_planes_equal").as_bool());
    EXPECT_TRUE(proof.get("cache_hit_observed").as_bool());
    EXPECT_TRUE(proof.get("geometry_and_restoration_good").as_bool());
    EXPECT_NE(proof.get("narrow_exact_ordinary").as_bool(),
              proof.get("narrow_geometry_good").as_bool());
    EXPECT_GT(proof.get_int("semantic_mutations_rejected"), 0);
    EXPECT_GE(proof.get_int("physical_mutations_rejected"), 1);
    const auto executed = proof.get("mutations_executed");
    std::set<std::string> ids;
    executed.iter([&](json::JsonVal v) { ids.insert(std::string(v.as_str())); });
    for (const auto* required_id :
         {"relation_label", "start_cardinality", "end_cardinality",
          "group_caption", "orphan_ink", "group_parent"})
        EXPECT_TRUE(ids.contains(required_id)) << required_id;
    // Ordinal 198 is the nested domain>details case, so group_parent is live.
    EXPECT_TRUE(ids.contains("group_parent"));
    EXPECT_FALSE(proof.has("error"));
    for (const auto& e107 : er107::authority::kPins)
        EXPECT_NE(e107.path, std::string(er198::kPins[0].path));
}

TEST(MermaidEr198Owned, TamperedProofIsRejected) {
    official::Documents documents;
    std::ostringstream diagnostics;
    const auto result = official::run(documents, 0, diagnostics);
    ASSERT_TRUE(result.ready);
    const auto good = json::parse(result.proof);
    ASSERT_TRUE(good.has_value());

    auto reject = [&](auto mutate, const char* label) {
        json::JsonMutDoc doc;
        auto root = doc.raw_json(result.proof);
        mutate(root);
        doc.set_root(root);
        auto reparsed = json::parse(doc.to_string());
        ASSERT_TRUE(reparsed.has_value()) << label;
        bool is_owned = false;
        try {
            is_owned = official::owned(reparsed->root());
        } catch (const std::exception&) {
            is_owned = false;
        }
        EXPECT_FALSE(is_owned) << label;
    };
    reject([](json::JsonMutVal r) { r.set("source_sha256", std::string(64, '0')); },
           "forged source sha");
    reject([](json::JsonMutVal r) { r.set("captures", 14); }, "missing capture");
    reject([](json::JsonMutVal r) { r.set("local_ready", false); },
           "not-ready without error");
    reject([](json::JsonMutVal r) { r.set("width", 999); }, "out-of-bound width");
    reject(
        [](json::JsonMutVal r) {
            auto narrow = r.get("narrow_exact_ordinary");
            r.set("narrow_exact_ordinary", !narrow.as_bool());
        },
        "broken narrow XOR");
    reject([](json::JsonMutVal r) { r.set("unknown_future_field", 1); },
           "unclosed extra field");
    reject(
        [](json::JsonMutVal r) {
            auto executed = r.get("mutations_executed");
            executed.append(executed.make_str("invented_control"));
        },
        "invented mutation");
}

TEST(MermaidEr198Owned, NotReadyProofRequiresExplicitError) {
    official::Documents documents;
    std::ostringstream diagnostics;
    const auto result = official::run(documents, 0, diagnostics);
    ASSERT_TRUE(result.ready);
    json::JsonMutDoc doc;
    auto root = doc.raw_json(result.proof);
    root.set("local_ready", false);
    doc.set_root(root);
    auto parsed = json::parse(doc.to_string());
    ASSERT_TRUE(parsed.has_value());
    bool is_owned = true;
    try {
        is_owned = official::owned(parsed->root());
    } catch (const std::exception&) {
        is_owned = false;
    }
    EXPECT_FALSE(is_owned);
}
}  // namespace
