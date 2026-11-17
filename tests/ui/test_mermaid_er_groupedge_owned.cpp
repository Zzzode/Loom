// RFC 0033 slice 2 phase 2: unit-level ownership tests for the six-case ER
// group-edge cohort. Drives the new support headers end to end over the
// immutable fixtures: Documents loading, fifteen captures, source/cross-check
// binding, complete-frame geometry, mutation census, and the closed owned()
// gate. A tampered-proof negative proves the gate rejects a mutated byte.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

import std;
import loom.serdes.json;
import loom.crypto.crypto;
import loom.ui.visual.markdown;
import loom.ui.visual.mermaid;

#include "mermaid_er_groupedge_official.hpp"
#include "mermaid_er107_authority.hpp"

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
namespace groupedge = loom::test::mermaid_er_groupedge;
namespace official = loom::test::mermaid_er_groupedge::official;
namespace er107 = loom::test::mermaid_er107;

TEST(MermaidErGroupEdgeOwned, CohortIsDisjointFromEr107AndSelectsOnlySix) {
    // Compile-time static_assert already guarantees disjointness; pin the
    // runtime selection behaviour and population as a tripwire.
    EXPECT_EQ(groupedge::kPins.size(), 6u);
    for (const auto& pin : groupedge::kPins) {
        const auto selected = groupedge::select(pin.path, pin.raw_sha);
        ASSERT_TRUE(selected.has_value()) << pin.path;
        for (const auto& e107 : er107::authority::kPins)
            EXPECT_NE(e107.path, pin.path) << pin.path;
        // A path match with a swapped sha fails closed by throwing (same
        // contract as the ER107 compiled selector).
        EXPECT_THROW(static_cast<void>(groupedge::select(pin.path, "deadbeef")),
                     std::exception);
    }
    EXPECT_FALSE(groupedge::select("er-diagram/not-selected.mmd",
                                   "deadbeef").has_value());
}

TEST(MermaidErGroupEdgeOwned, AllSixProofsAreOwnedAndCarryCohortControls) {
    official::Documents documents;
    for (std::size_t i = 0; i < groupedge::kPins.size(); ++i) {
        const auto& pin = groupedge::kPins[i];
        SCOPED_TRACE(std::string(pin.path));
        std::ostringstream diagnostics;
        const auto result = official::run(documents, i, diagnostics);
        ASSERT_TRUE(result.ready) << diagnostics.str();
        ASSERT_TRUE(result.captured);
        ASSERT_TRUE(result.bound);
        const auto parsed = json::parse(result.proof);
        ASSERT_TRUE(parsed.has_value());
        const auto proof = parsed->root();
        EXPECT_TRUE(official::owned(proof));
        EXPECT_EQ(proof.get_int("captures"), 15);
        EXPECT_TRUE(proof.get("captures_before_binding").as_bool());
        EXPECT_TRUE(proof.get("bound_after_captures").as_bool());
        EXPECT_TRUE(proof.get("uncached_cached_five_planes_equal").as_bool());
        EXPECT_TRUE(proof.get("cache_hit_observed").as_bool());
        EXPECT_TRUE(proof.get("geometry_and_restoration_good").as_bool());
        // Width/narrow XOR.
        EXPECT_NE(proof.get("narrow_exact_ordinary").as_bool(),
                  proof.get("narrow_geometry_good").as_bool());
        EXPECT_GT(proof.get_int("semantic_mutations_rejected"), 0);
        EXPECT_GE(proof.get_int("physical_mutations_rejected"), 1);
        const auto executed = proof.get("mutations_executed");
        std::set<std::string> ids;
        executed.iter([&](json::JsonVal v) { ids.insert(std::string(v.as_str())); });
        for (const auto* required_id :
             {"relation_label", "start_cardinality", "end_cardinality",
              "group_caption", "orphan_ink"})
            EXPECT_TRUE(ids.contains(required_id)) << required_id;
        const bool nested = [&] {
            bool has_parent = false;
            documents.proposal.root().get("cases").iter([&](json::JsonVal c) {
                if (c.get_string("path") != std::string(pin.path)) return;
                c.get("source_model").get("subgraphs").iter([&](json::JsonVal g) {
                    if (!g.get("source_parent_subgraph").is_null()) has_parent = true;
                });
            });
            return has_parent;
        }();
        if (nested)
            EXPECT_TRUE(ids.contains("group_parent"));
        else
            EXPECT_FALSE(ids.contains("group_parent"));
        EXPECT_FALSE(proof.has("error"));
        EXPECT_EQ(proof.get_string("contract"), official::kContract);
    }
}

TEST(MermaidErGroupEdgeOwned, TamperedProofIsRejected) {
    official::Documents documents;
    std::ostringstream diagnostics;
    const auto result = official::run(documents, 0, diagnostics);
    ASSERT_TRUE(result.ready);
    const auto good = json::parse(result.proof);
    ASSERT_TRUE(good.has_value());

    // Each mutation must break the closed owned() gate (throw or return false).
    auto reject = [&](auto mutate, const char* label) {
        json::JsonMutDoc doc;
        auto root = doc.raw_json(result.proof);
        mutate(root);
        doc.set_root(root);
        auto reparsed = json::parse(doc.to_string());
        ASSERT_TRUE(reparsed.has_value()) << label;
        bool owned = false;
        try {
            owned = official::owned(reparsed->root());
        } catch (const std::exception&) {
            owned = false;
        }
        EXPECT_FALSE(owned) << label;
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

TEST(MermaidErGroupEdgeOwned, NotReadyProofRequiresExplicitError) {
    official::Documents documents;
    std::ostringstream diagnostics;
    const auto result = official::run(documents, 0, diagnostics);
    ASSERT_TRUE(result.ready);
    json::JsonMutDoc doc;
    auto root = doc.raw_json(result.proof);
    root.set("local_ready", false);
    // No error field: owned() must fail closed rather than accept a silent
    // not-ready proof.
    doc.set_root(root);
    auto parsed = json::parse(doc.to_string());
    ASSERT_TRUE(parsed.has_value());
    bool owned = true;
    try {
        owned = official::owned(parsed->root());
    } catch (const std::exception&) {
        owned = false;
    }
    EXPECT_FALSE(owned);
}
}  // namespace
