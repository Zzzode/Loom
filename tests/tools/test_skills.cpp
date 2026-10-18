/// @file test_skills.cpp
/// @brief Skill system smoke tests aligned with current C++ modules.

#include <gtest/gtest.h>

import std;
import loom.skills.skill;
import loom.skills.bundled;
import loom.skills.load_skills_dir;

namespace fs = std::filesystem;

namespace {
/// Unique temp directory name using a timestamp suffix to avoid collisions
/// when tests from the same binary run in parallel under ctest -jN.
[[nodiscard]] fs::path unique_test_dir(std::string_view prefix) {
    return fs::temp_directory_path() / (
        std::string(prefix) +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
    );
}
} // namespace

TEST(SkillDefinition, SerializesMetadata) {
    loom::skills::SkillDefinition skill{
        .name = "test-skill",
        .description = "A test skill",
        .trigger_patterns = {"test"},
        .content = "Use this test skill.",
        .is_builtin = true,
        .author = std::nullopt,
        .version = std::nullopt,
    };

    auto json = skill.to_json();
    EXPECT_NE(json.find("test-skill"), std::string::npos);
    EXPECT_NE(json.find("trigger_count"), std::string::npos);
}

TEST(SkillExecutor, MatchesRegisteredSkillPattern) {
    loom::skills::SkillExecutor executor;
    executor.register_skill(loom::skills::SkillDefinition{
        .name = "debug",
        .description = "Debug problems",
        .trigger_patterns = {"debug|diagnose"},
        .content = "Debug carefully.",
        .is_builtin = false,
        .author = std::nullopt,
        .version = std::nullopt,
    });

    auto matches = executor.match("please debug this failure");
    ASSERT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches.front().skill_name, "debug");
}

TEST(BundledSkills, ProvidesBuiltInSkills) {
    loom::skills::BundledSkills bundled;
    EXPECT_GT(bundled.size(), 0u);
    EXPECT_FALSE(bundled.all().empty());
    EXPECT_NE(bundled.find("debug"), nullptr);
}

TEST(BundledSkills, RegistersIntoExecutor) {
    loom::skills::BundledSkills bundled;
    loom::skills::SkillExecutor executor;
    bundled.register_all(executor);
    EXPECT_EQ(executor.size(), bundled.size());
}

TEST(SkillLoader, LoadsDirectorySkillMarkdown) {
    auto root = unique_test_dir("loom_skill_directory_test_");
    fs::create_directories(root / "review-skill");
    {
        std::ofstream skill(root / "review-skill" / "SKILL.md");
        skill << R"MD(---
description: Review code changes
---
Read the diff and report concrete risks.
)MD";
    }

    loom::skills::SkillLoader loader;
    auto result = loader.load_from_directory(root);

    fs::remove_all(root);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1u);
    EXPECT_EQ(result->front().name, "review-skill");
    EXPECT_EQ(result->front().description, "Review code changes");
    EXPECT_NE(result->front().content.find("Read the diff"), std::string::npos);
}


TEST(SkillSubscriptions, UnsubscribeRemainsStableAcrossVectorGrowthAndRepeatedRemoval) {
    auto counts = std::make_shared<std::vector<int>>();
    std::vector<std::function<void()>> unsubscribe;
    struct UnsubscribeAll {
        std::vector<std::function<void()>>& subscriptions;
        ~UnsubscribeAll() { for (auto& remove : subscriptions) remove(); }
    } cleanup{unsubscribe};
    const auto subscribe = [&] {
        const auto index = counts->size();
        counts->push_back(0);
        unsubscribe.push_back(loom::skills::on_dynamic_skills_loaded([counts, index] {
            ++(*counts)[index];
        }));
    };
    const auto capacity = [] {
        auto& state = loom::skills::detail::dynamic_state();
        std::lock_guard lock(state.mutex);
        return state.on_loaded_callbacks.capacity();
    };
    subscribe();
    const auto original_capacity = capacity();
    while (capacity() == original_capacity) subscribe();
    while (unsubscribe.size() < 3u) subscribe();
    ASSERT_GT(capacity(), original_capacity) << "The first subscription must cross a real reallocation";
    loom::skills::detail::emit_skills_loaded();
    for (const auto count : *counts) EXPECT_EQ(count, 1);
    // Remove in a different order from registration, then repeat a removal.
    unsubscribe.back()();
    unsubscribe.front()();
    unsubscribe.front()();
    loom::skills::detail::emit_skills_loaded();
    EXPECT_EQ(counts->front(), 1);
    EXPECT_EQ(counts->back(), 1);
    for (std::size_t i = 1; i + 1 < counts->size(); ++i) EXPECT_EQ((*counts)[i], 2);
    unsubscribe[1]();
    unsubscribe.back()();
    loom::skills::detail::emit_skills_loaded();
    EXPECT_EQ((*counts)[1], 2);
    for (std::size_t i = 2; i + 1 < counts->size(); ++i) EXPECT_EQ((*counts)[i], 3);
}
