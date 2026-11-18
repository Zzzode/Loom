// RFC 0034 phase 2, increment 1: renderer-free identity and selection tests for
// the one-row ER198 cohort. These pin the immutable raw identity, the
// fail-closed path/sha selector, the compile-time-plus-runtime disjointness
// from both prior cohorts (ER107 and the six-case group-edge cohort), and the
// uniqueness of ordinal 198's two-debt set across all 206 ER206 priors. No
// rendering and no production code; the owned() proof gate arrives in a later
// increment with the proposal/support header.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

import std;
import loom.serdes.json;
import loom.crypto.crypto;

#include "mermaid_er198_source.hpp"

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
namespace er198 = loom::test::mermaid_er198;

fs::path fixtures_dir() {
    return fs::path(LOOM_TESTS_DIR) / "fixtures";
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

TEST(MermaidEr198Source, IsSingleRowAndDisjointFromBothPriorCohorts) {
    ASSERT_EQ(er198::kPins.size(), 1u);
    const auto& pin = er198::kPins[0];
    EXPECT_EQ(pin.prior_ordinal, 198);
    EXPECT_EQ(pin.global, 647);
    EXPECT_GT(pin.raw_bytes, 0u);
    // The header's static_assert already proves compile-time disjointness; pin
    // the same property at runtime as a tripwire against future cohort edits.
    for (const auto& e107 : loom::test::mermaid_er107::authority::kPins)
        EXPECT_NE(e107.path, pin.path);
    for (const auto& ge : loom::test::mermaid_er_groupedge::kPins)
        EXPECT_NE(ge.path, pin.path);
}

TEST(MermaidEr198Source, SelectorBindsTheRawShaAndFailsClosedOnSwapOrUnknown) {
    const auto& pin = er198::kPins[0];
    const auto hit = er198::select(pin.path, pin.raw_sha);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, 0u);
    // A path match with a swapped sha cannot resolve (same throw contract as the
    // ER107 and group-edge compiled selectors).
    EXPECT_THROW(static_cast<void>(er198::select(pin.path, "deadbeef")),
                 std::exception);
    // An unknown path never selects, regardless of the sha offered.
    EXPECT_FALSE(
        er198::select("er-diagram/not-the-pinned-row.mmd", pin.raw_sha)
            .has_value());
}

TEST(MermaidEr198Source, OnDiskFixtureMatchesThePinnedRawIdentity) {
    const auto& pin = er198::kPins[0];
    const auto raw =
        read_file(fixtures_dir() / "mermaid_official" / "cases" / std::string(pin.path));
    ASSERT_FALSE(raw.empty());
    EXPECT_EQ(raw.size(), pin.raw_bytes);
    const auto digest = loom::utils::crypto::sha256(raw);
    EXPECT_EQ(digest, pin.raw_sha);
    // The live digest must be what the renderer-free selector accepts.
    EXPECT_TRUE(er198::select(pin.path, digest).has_value());
}

// Verifier assertion (i): ordinal 198 is the UNIQUE ER206 prior carrying the
// exact two-debt set {active_config, raw_entity_group_kind_collision...},
// pinned by fixture path and source sha, and it stays a whole-null Model.
TEST(MermaidEr198Source, IsTheUniqueTwoDebtWholeNullRowInTheEr206Prior) {
    const auto& pin = er198::kPins[0];
    const auto prior_bytes =
        read_file(fixtures_dir() / std::string(er198::kPriorPath));
    ASSERT_FALSE(prior_bytes.empty());
    // Bind the pinned prior digest, not just its path, so a swapped prior cannot
    // silently feed the uniqueness proof.
    EXPECT_EQ(loom::utils::crypto::sha256(prior_bytes),
              std::string(er198::kPriorSha));
    const auto prior = json::parse(prior_bytes);
    ASSERT_TRUE(prior.has_value());
    const auto cases = prior->root().get("cases");
    ASSERT_EQ(cases.size(), 206u);

    int matches = 0;
    int matched_ordinal = -1;
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const auto row = cases.at(i);
        const auto debts = row.get("whole_null_debts");
        if (!debts.is_arr() || debts.size() != 2) continue;
        std::set<std::string> kinds;
        debts.iter([&](json::JsonVal debt) { kinds.insert(debt.get_string("kind")); });
        if (kinds.size() != 2 ||
            !kinds.contains(std::string(er198::kDebtActiveConfig)) ||
            !kinds.contains(std::string(er198::kDebtGroupCollision)))
            continue;
        ++matches;
        matched_ordinal = row.get_int("ordinal");
        EXPECT_EQ(row.get_string("fixture"), std::string(pin.path));
        EXPECT_EQ(row.get_string("source_sha256"), std::string(pin.raw_sha));
        EXPECT_EQ(row.get_int("source_bytes"),
                  static_cast<int64_t>(pin.raw_bytes));
        EXPECT_EQ(row.get_int("manifest_global_ordinal"), pin.global);
        EXPECT_TRUE(row.get("whole_model").is_null());
    }
    EXPECT_EQ(matches, 1);
    EXPECT_EQ(matched_ordinal, pin.prior_ordinal);
}
}  // namespace
