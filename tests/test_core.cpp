/// @file test_core.cpp
/// @brief Core C++ module smoke tests aligned with current module names/APIs.

#include <gtest/gtest.h>
#include <cstdint>
#include <climits>

import std;
import loom.types.types;
import loom.config.config;
import loom.config.feature_flags;
import loom.constants.constants;
import loom.coordinator.types;
import loom.tasks.task_graph;
import loom.serdes.yaml;
import loom.text.parse_int;

TEST(CoreTypes, RoleToStringAndContentVariant) {
    EXPECT_EQ(loom::core::role_to_string(loom::core::Role::User), "user");

    loom::core::ContentBlock block = loom::core::TextBlock{"hello"};
    ASSERT_TRUE(std::holds_alternative<loom::core::TextBlock>(block));
    EXPECT_EQ(std::get<loom::core::TextBlock>(block).text, "hello");
}

TEST(CoreConfig, FeatureFlagsToggleRuntimeBits) {
    loom::core::FeatureFlags flags;
    EXPECT_FALSE(flags.is_enabled(loom::core::FeatureFlag::MultiAgent));

    flags.enable(loom::core::FeatureFlag::MultiAgent);
    EXPECT_TRUE(flags.is_enabled(loom::core::FeatureFlag::MultiAgent));

    flags.disable(loom::core::FeatureFlag::MultiAgent);
    EXPECT_FALSE(flags.is_enabled(loom::core::FeatureFlag::MultiAgent));
}

TEST(UtilsYaml, ScalarsParseStrictlyWithoutFromChars) {
    namespace uy = loom::utils;
    // Integers parse as int64 (the portable strict parser).
    auto pos = uy::parse_yaml("v: 42");
    ASSERT_TRUE(std::holds_alternative<uy::YamlMap>(pos.data));
    EXPECT_EQ(std::get<std::int64_t>(std::get<uy::YamlMap>(pos.data).at("v").data), 42);

    auto neg = uy::parse_yaml("v: -7");
    EXPECT_EQ(std::get<std::int64_t>(
        std::get<uy::YamlMap>(neg.data).at("v").data), -7);

    // Floats parse as double.
    auto dbl = uy::parse_yaml("v: 1.5");
    EXPECT_DOUBLE_EQ(std::get<double>(
        std::get<uy::YamlMap>(dbl.data).at("v").data), 1.5);

    // Non-numeric / partial-garbage stay strings. (Leading whitespace after
    // the colon is trimmed by parse_block before reaching parse_scalar, so it
    // is not tested here; "+5" is a valid double per from_chars parity.)
    for (const char* bad : {"v: 12abc", "v: 1.2.3", "v: 1-2"}) {
        auto y = uy::parse_yaml(bad);
        ASSERT_TRUE(std::holds_alternative<uy::YamlMap>(y.data)) << bad;
        EXPECT_TRUE(std::get<uy::YamlMap>(y.data).at("v").is_string())
            << "expected string for: " << bad;
    }

    // A magnitude beyond int64 range is still a valid (huge) double, matching
    // from_chars(double) which fully consumes it; assert it is NOT an int.
    auto huge = uy::parse_yaml("v: 99999999999999999999999");
    const auto& huge_v = std::get<uy::YamlMap>(huge.data).at("v").data;
    EXPECT_FALSE(std::holds_alternative<std::int64_t>(huge_v));
}

TEST(UtilsParseInt, StrictFromCharsSemantics) {
    auto parse = [](std::string_view s, std::int64_t& out) {
        return loom::utils::from_chars(s.data(), s.data() + s.size(), out);
    };

    std::int64_t v = 0;
    EXPECT_EQ(parse("42", v).ec, std::errc{});
    EXPECT_EQ(v, 42);
    EXPECT_EQ(parse("-7", v).ec, std::errc{});
    EXPECT_EQ(v, -7);

    // signed range boundaries
    EXPECT_EQ(parse("9223372036854775807", v).ec, std::errc{});
    EXPECT_EQ(v, INT64_MAX);
    EXPECT_EQ(parse("-9223372036854775808", v).ec, std::errc{});
    EXPECT_EQ(v, INT64_MIN);

    // out of range
    EXPECT_EQ(parse("9223372036854775808", v).ec, std::errc::result_out_of_range);
    EXPECT_EQ(parse("-9223372036854775809", v).ec, std::errc::result_out_of_range);

    // malformed: empty, sign only, leading +, whitespace, trailing garbage
    EXPECT_EQ(parse("", v).ec, std::errc::invalid_argument);
    EXPECT_EQ(parse("-", v).ec, std::errc::invalid_argument);
    EXPECT_EQ(parse("+5", v).ec, std::errc::invalid_argument);
    EXPECT_EQ(parse(" 5", v).ec, std::errc::invalid_argument);
    EXPECT_EQ(parse("5x", v).ec, std::errc::invalid_argument);

    // unsigned (uint16_t, used by port parsing)
    auto parse_u16 = [](std::string_view s, std::uint16_t& out) {
        return loom::utils::from_chars(s.data(), s.data() + s.size(), out);
    };
    std::uint16_t port = 0;
    EXPECT_EQ(parse_u16("8080", port).ec, std::errc{});
    EXPECT_EQ(port, 8080);
    EXPECT_EQ(parse_u16("70000", port).ec, std::errc::result_out_of_range);
}

TEST(CoreConfig, ConfigManagerExposesDefaultSettings) {
    loom::core::ConfigManager manager;
    EXPECT_FALSE(manager.settings().model.default_model.empty());
    EXPECT_GT(manager.settings().model.max_output_tokens, 0u);
}

// RFC-0001 B followup c22: a soft-tier (user/local) parse warning is collected
// into a per-instance diagnostics vector (drained by the TUI toast sink) rather
// than only printed to stderr. The CLI path keeps its stderr println; this
// pins the collection + drain contract.
TEST(CoreConfig, SoftTierParseWarningCollectedForDrain) {
    namespace fs = std::filesystem;
    const auto suffix =
        std::chrono::system_clock::now().time_since_epoch().count();
    const auto root =
        fs::temp_directory_path() / ("loom_core_c22_diag_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root);

    const auto user_path = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path = root / "local.json";
    {
        std::ofstream seed(user_path);
        seed << "{ this is not valid json\n";
    }

    {
        loom::core::ConfigManager manager(user_path,
                                        project_path, local_path);
        // Quiet load: no stderr print, but the diagnostic is collected for
        // the TUI composition root to drain into a toast.
        ASSERT_TRUE(manager.load(loom::core::LoadOptions{.quiet = true}).has_value());

        auto diags = manager.drain_load_diagnostics();
        ASSERT_EQ(diags.size(), 1u);
        EXPECT_EQ(diags[0].path, user_path.string());
        EXPECT_NE(diags[0].message.find("not valid JSON"), std::string::npos);

        // Drain clears the vector.
        EXPECT_TRUE(manager.drain_load_diagnostics().empty());
    }

    fs::remove_all(root);
}

// RFC-0001 B followup c23: tier_files_changed() detects an external edit so a
// latched command reloads instead of serving a stale snapshot. The content
// hash catches a same-size rewrite ("v1"→"v2") that leaves mtime/size
// unchanged; a longer rewrite and a delete are also detected.
TEST(CoreConfig, TierFilesChangedDetectsExternalEdit) {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "loom_core_c23_sig_test";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto user_path = root / "user.json";
    const auto project_path = root / "project.json";
    const auto local_path = root / "local.json";
    {
        std::ofstream seed(project_path);
        seed << "{\"model\":{\"default_model\":\"v1\"}}";
    }
    {
        loom::core::ConfigManager manager(user_path,
                                        project_path, local_path);
        ASSERT_TRUE(manager.load().has_value());
        EXPECT_FALSE(manager.tier_files_changed())
            << "no change right after load";

        // Same-size rewrite (v1 -> v2): only the content hash differs.
        {
            std::ofstream edit(project_path, std::ios::trunc);
            edit << "{\"model\":{\"default_model\":\"v2\"}}";
        }
        EXPECT_TRUE(manager.tier_files_changed())
            << "same-size rewrite must change the content hash";

        // Longer rewrite: size + content hash differ.
        {
            std::ofstream edit(project_path, std::ios::trunc);
            edit << "{\"model\":{\"default_model\":\"v2-much-longer\"}}";
        }
        EXPECT_TRUE(manager.tier_files_changed())
            << "longer rewrite must change size + content hash";

        // Delete: exists flips false.
        fs::remove(project_path);
        EXPECT_TRUE(manager.tier_files_changed())
            << "delete must flip exists";

        // Recreate with DIFFERENT content: exists flips back and the hash
        // differs from the load-time snapshot.
        {
            std::ofstream reseed(project_path);
            reseed << "{\"model\":{\"default_model\":\"v3-recreated\"}}";
        }
        EXPECT_TRUE(manager.tier_files_changed())
            << "recreate with new content must change the hash";
    }
    fs::remove_all(root);
}

TEST(CoreFeatureFlags, RuntimeManagerCanFindAndToggleFeature) {
    auto feature = loom::core::flags::FeatureFlagManager::find_by_name("PROACTIVE");
    ASSERT_TRUE(feature.has_value());

    loom::core::flags::FeatureFlagManager manager;
    manager.enable(*feature);
    EXPECT_TRUE(manager.is_enabled(*feature));
    EXPECT_NE(manager.enabled_summary().find("PROACTIVE"), std::string::npos);
}

TEST(CoreConstants, AppMetadataIsDefined) {
    EXPECT_FALSE(std::string(loom::core::constants::kAppName).empty());
    EXPECT_FALSE(std::string(loom::core::constants::kVersion).empty());
    EXPECT_GT(loom::core::constants::api_limits::kMaxTokensDefault, 0u);
}

TEST(CoreCoordinator, CoordinatorModeParseRoundTrip) {
    auto parsed = loom::coordinator::parse_coordinator_mode("parallel");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(loom::coordinator::coordinator_mode_to_string(*parsed), "parallel");
}

TEST(CoreTasks, TaskSchedulerTracksSubmittedTask) {
    loom::core::TaskScheduler scheduler;
    auto id = scheduler.submit("test task");
    ASSERT_TRUE(id.has_value());

    auto status = scheduler.get_status(*id);
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(scheduler.total_tasks(), 1u);
}
