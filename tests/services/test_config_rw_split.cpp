/// @file test_config_rw_split.cpp
/// @brief RFC 0007 Phase 2: ConfigManager read/write path split tests.
///
/// Tests the user_read_path_ / user_write_path_ split:
/// - Read follows the full cascade ($LOOM_CONFIG_DIR > ~/.loom > ~/.agents)
/// - Write always goes to ~/.loom (or $LOOM_CONFIG_DIR), never ~/.agents
/// - Default-path mode re-resolves the read path on every load()
/// - Same-instance takeover after a write creates ~/.loom/settings.json
/// - Failed writes do not rebind the read path
/// - LOOM_CONFIG_DIR is an unconditional override

#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>

import std;
import loom.constants.paths;
import loom.config.config;
import loom.serdes.json;

namespace fs = std::filesystem;

namespace {

/// RAII helper: sets HOME to a temp directory and unsets LOOM_CONFIG_DIR.
/// Restores both on destruction. Creates ~/.agents and ~/.loom subdirectories
/// on demand.
class TempHome {
public:
    TempHome() {
        root_ = fs::temp_directory_path() /
                ("loom-rw-split-" + std::to_string(::getpid()) + "-" + std::to_string(counter_++));
        fs::create_directories(root_);
        if (const char* prev = std::getenv("HOME")) prev_home_ = prev;
        if (const char* prev = std::getenv("LOOM_CONFIG_DIR")) prev_config_ = prev;
        ::unsetenv("LOOM_CONFIG_DIR");
        ::setenv("HOME", root_.c_str(), 1);
    }

    ~TempHome() {
        if (prev_home_)
            ::setenv("HOME", prev_home_->c_str(), 1);
        else
            ::unsetenv("HOME");
        if (prev_config_)
            ::setenv("LOOM_CONFIG_DIR", prev_config_->c_str(), 1);
        else
            ::unsetenv("LOOM_CONFIG_DIR");
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    TempHome(const TempHome&) = delete;
    TempHome& operator=(const TempHome&) = delete;

    /// Create a settings.json file in the given config subdirectory.
    void write_settings(const std::string& dir_name, std::string_view json) {
        auto dir = root_ / dir_name;
        fs::create_directories(dir);
        std::ofstream(dir / "settings.json") << json;
    }

    /// Read the contents of a settings.json file.
    [[nodiscard]] std::string read_settings(const std::string& dir_name) const {
        std::ifstream f(root_ / dir_name / "settings.json");
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] bool settings_exists(const std::string& dir_name) const {
        return fs::exists(root_ / dir_name / "settings.json");
    }

    [[nodiscard]] fs::path settings_path(const std::string& dir_name) const {
        return root_ / dir_name / "settings.json";
    }

    [[nodiscard]] fs::path root() const { return root_; }

private:
    fs::path root_;
    std::optional<std::string> prev_home_;
    std::optional<std::string> prev_config_;
    static inline int counter_ = 0;
};

/// Write a minimal settings.json with mouseMode set.
std::string settings_with_mouse_mode(bool enabled) {
    return std::format("{{\"mouseMode\": {}}}", enabled ? "true" : "false");
}

/// Parse a JSON string into a JsonDoc for set_user_setting. JsonVal is a
/// non-owning view into the doc, so the doc must outlive the call — callers
/// must use parse_json(...).root() inline so the temporary lives through the
/// full expression.
[[nodiscard]] loom::utils::json::JsonDoc parse_json(std::string_view text) {
    auto doc = loom::utils::json::parse(text);
    if (doc) return std::move(*doc);
    return loom::utils::json::JsonDoc{};
}

}  // namespace

// ── Read cascade: ~/.agents fallback ────────────────────────────────────

TEST(ConfigRwSplit, ReadsFromAgentsFallbackWhenLoomAbsent) {
    TempHome home;
    home.write_settings(".agents", settings_with_mouse_mode(false));

    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_FALSE(cm.settings().display.mouse_mode)
        << "mouseMode should be read from ~/.agents/settings.json";
}

TEST(ConfigRwSplit, PrefersLoomOverAgentsWhenBothExist) {
    TempHome home;
    home.write_settings(".agents", settings_with_mouse_mode(true));
    home.write_settings(".loom", settings_with_mouse_mode(false));

    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_FALSE(cm.settings().display.mouse_mode)
        << "~/.loom should take precedence over ~/.agents";
}

TEST(ConfigRwSplit, ReturnsWritePathWhenNoSettingsFileExists) {
    TempHome home;
    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_TRUE(cm.settings().display.mouse_mode)
        << "default mouse_mode is true when no settings file exists";
}

// ── Write path: always ~/.loom, never ~/.agents ─────────────────────────

TEST(ConfigRwSplit, SaveUserWritesToLoomNotAgents) {
    TempHome home;
    home.write_settings(".agents", settings_with_mouse_mode(false));

    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_FALSE(cm.settings().display.mouse_mode);

    // Change a setting and save to user tier.
    auto result = cm.set_user_setting("mouseMode", parse_json("true").root());
    ASSERT_TRUE(result.has_value()) << "set_user_setting should succeed";

    // ~/.loom/settings.json should now exist.
    EXPECT_TRUE(home.settings_exists(".loom")) << "save_user should create ~/.loom/settings.json";

    // ~/.agents/settings.json should be byte-unchanged.
    EXPECT_EQ(home.read_settings(".agents"), settings_with_mouse_mode(false))
        << "~/.agents/settings.json must remain byte-unchanged";
}

TEST(ConfigRwSplit, UserConfigPathReturnsWritePath) {
    TempHome home;
    home.write_settings(".agents", settings_with_mouse_mode(false));

    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());

    // user_config_path() should return the WRITE path (~/.loom), not the
    // READ path (~/.agents).
    EXPECT_EQ(cm.user_config_path(), home.settings_path(".loom"))
        << "user_config_path() should return the write path, not the read path";
}

// ── Same-instance takeover ───────────────────────────────────────────────

TEST(ConfigRwSplit, SameInstanceTakeoverAfterSaveUser) {
    TempHome home;
    home.write_settings(".agents", settings_with_mouse_mode(false));

    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_FALSE(cm.settings().display.mouse_mode);

    // Write to user tier — creates ~/.loom/settings.json.
    auto result = cm.set_user_setting("mouseMode", parse_json("true").root());
    ASSERT_TRUE(result.has_value());

    // Reload on the SAME instance — should pick up ~/.loom.
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_TRUE(cm.settings().display.mouse_mode)
        << "after write + reload, same instance should read from ~/.loom";
}

// ── Constructor modes ────────────────────────────────────────────────────

TEST(ConfigRwSplit, ExplicitPathConstructorUsesFixedPaths) {
    TempHome home;
    home.write_settings(".agents", settings_with_mouse_mode(true));

    // Explicit-path constructor: user tier is the explicit path, NOT
    // re-resolved via settings_home_read().
    auto user_path = home.root() / "explicit_user.json";
    std::ofstream(user_path) << settings_with_mouse_mode(false);

    loom::core::ConfigManager cm(user_path, home.root() / "project.json",
                                 home.root() / "local.json");
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_FALSE(cm.settings().display.mouse_mode)
        << "explicit-path constructor should use the passed-in path, "
           "not the cascade";
}

TEST(ConfigRwSplit, SingleProjectConstructorHasNoUserTier) {
    TempHome home;
    home.write_settings(".agents", settings_with_mouse_mode(false));

    loom::core::ConfigManager cm(home.root() / "project.json");
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_TRUE(cm.settings().display.mouse_mode)
        << "single-project constructor should have no user tier";
    EXPECT_TRUE(cm.user_config_path().empty())
        << "single-project constructor should have empty user write path";
}

// ── LOOM_CONFIG_DIR override ─────────────────────────────────────────────

TEST(ConfigRwSplit, LoomConfigDirOverrideIsUnconditional) {
    TempHome home;
    home.write_settings(".agents", settings_with_mouse_mode(false));
    home.write_settings(".loom", settings_with_mouse_mode(false));

    // Set LOOM_CONFIG_DIR to an empty directory — no settings.json.
    auto isolated = home.root() / "isolated";
    fs::create_directories(isolated);
    ::setenv("LOOM_CONFIG_DIR", isolated.c_str(), 1);

    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_TRUE(cm.settings().display.mouse_mode)
        << "LOOM_CONFIG_DIR should be unconditional — no downgrade to HOME paths";

    // user_config_path should also be under LOOM_CONFIG_DIR.
    EXPECT_EQ(cm.user_config_path(), isolated / "settings.json")
        << "write path should also be under LOOM_CONFIG_DIR";
}

// ── Unparseable higher-priority file ─────────────────────────────────────

TEST(ConfigRwSplit, UnparseableLoomFileDoesNotSilentlyDowngrade) {
    TempHome home;
    home.write_settings(".agents", settings_with_mouse_mode(false));
    home.write_settings(".loom", "{ invalid json !!!");

    loom::core::ConfigManager cm;
    auto result = cm.load();
    // The load should fail or produce a diagnostic — NOT silently downgrade.
    // The user-tier parse error is tolerated (soft tier), but the diagnostic
    // should be available.
    if (result) {
        // If load succeeded (soft tier), the mouseMode from ~/.agents should
        // NOT be loaded because ~/.loom is the higher-priority file.
        EXPECT_TRUE(cm.settings().display.mouse_mode)
            << "unparseable ~/.loom should not silently downgrade to ~/.agents";
    }
}

// ── After successful write, new instance prefers ~/.loom ────────────────

TEST(ConfigRwSplit, NewInstancePrefersLoomAfterWrite) {
    TempHome home;
    home.write_settings(".agents", settings_with_mouse_mode(false));

    {
        loom::core::ConfigManager cm;
        ASSERT_TRUE(cm.load().has_value());
        EXPECT_FALSE(cm.settings().display.mouse_mode);

        auto result = cm.set_user_setting("mouseMode", parse_json("true").root());
        ASSERT_TRUE(result.has_value());
    }

    // New instance should read from ~/.loom (which now exists).
    loom::core::ConfigManager cm2;
    ASSERT_TRUE(cm2.load().has_value());
    EXPECT_TRUE(cm2.settings().display.mouse_mode)
        << "new instance should prefer ~/.loom over ~/.agents";
}

// ── mouseMode config parse/save ──────────────────────────────────────────

TEST(ConfigRwSplit, MouseModeDefaultIsTrue) {
    TempHome home;
    home.write_settings(".loom", "{}");
    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_TRUE(cm.settings().display.mouse_mode);
}

TEST(ConfigRwSplit, MouseModeParsedFromSettingsJson) {
    TempHome home;
    home.write_settings(".loom", settings_with_mouse_mode(true));

    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_TRUE(cm.settings().display.mouse_mode);
}

TEST(ConfigRwSplit, MouseModeExplicitFalsePreservesNativeSelection) {
    TempHome home;
    home.write_settings(".loom", settings_with_mouse_mode(false));

    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_FALSE(cm.settings().display.mouse_mode);
}

TEST(ConfigRwSplit, MouseModeSavedToSettingsJson) {
    TempHome home;
    home.write_settings(".loom", "{}");

    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_TRUE(cm.settings().display.mouse_mode);

    auto result = cm.set_user_setting("mouseMode", parse_json("false").root());
    ASSERT_TRUE(result.has_value());

    // Verify the explicit opt-out survives serialization and a fresh load.
    auto saved = loom::utils::json::parse(home.read_settings(".loom"));
    ASSERT_TRUE(saved.has_value());
    auto mouse_mode = saved->root().get("mouseMode");
    ASSERT_TRUE(mouse_mode.is_bool());
    EXPECT_FALSE(mouse_mode.as_bool());

    loom::core::ConfigManager reloaded;
    ASSERT_TRUE(reloaded.load().has_value());
    EXPECT_FALSE(reloaded.settings().display.mouse_mode);
}

// ── ~/.loom directory exists but no settings.json ───────────────────────

TEST(ConfigRwSplit, LoomDirExistsButNoSettingsFileFallsToAgents) {
    TempHome home;
    // Create ~/.loom DIRECTORY but no settings.json (simulates a failed
    // save_user that created the directory but not the file).
    fs::create_directories(home.root() / ".loom");
    home.write_settings(".agents", settings_with_mouse_mode(false));

    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_FALSE(cm.settings().display.mouse_mode)
        << "directory-level existence must not shadow the file-level fallback";
}

// ── Failed write does not rebind read path (Dev-0 v7.6 P0) ──────────────

TEST(ConfigRwSplit, FailedWriteDoesNotRebindReadPath) {
    TempHome home;
    home.write_settings(".agents", settings_with_mouse_mode(false));

    loom::core::ConfigManager cm;
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_FALSE(cm.settings().display.mouse_mode);
    EXPECT_TRUE(cm.is_user_fallback());

    // Create ~/.loom directory but make it read-only so the write fails.
    auto loom_dir = home.root() / ".loom";
    fs::create_directories(loom_dir);
    fs::permissions(loom_dir,
                    fs::perms::owner_read | fs::perms::owner_exec | fs::perms::group_read |
                        fs::perms::group_exec | fs::perms::others_read | fs::perms::others_exec,
                    fs::perm_options::replace);

    // Attempt to write — should fail because the directory is read-only.
    auto result = cm.set_user_setting("mouseMode", parse_json("true").root());
    EXPECT_FALSE(result.has_value()) << "write to read-only ~/.loom should fail";

    // Restore permissions so TempHome destructor can clean up.
    fs::permissions(loom_dir, fs::perms::owner_all | fs::perms::group_all | fs::perms::others_all,
                    fs::perm_options::replace);

    // The read path must still be ~/.agents — the failed write must NOT
    // rebind the read path to ~/.loom.
    ASSERT_TRUE(cm.load().has_value());
    EXPECT_FALSE(cm.settings().display.mouse_mode)
        << "after failed write, same instance must still read ~/.agents";
    EXPECT_TRUE(cm.is_user_fallback())
        << "after failed write, the instance must still be in fallback mode";

    // A NEW instance must also still read ~/.agents.
    loom::core::ConfigManager cm2;
    ASSERT_TRUE(cm2.load().has_value());
    EXPECT_FALSE(cm2.settings().display.mouse_mode)
        << "after failed write, new instance must still read ~/.agents";
    EXPECT_TRUE(cm2.is_user_fallback())
        << "after failed write, new instance must still be in fallback mode";

    // ~/.agents/settings.json must be byte-unchanged.
    EXPECT_EQ(home.read_settings(".agents"), settings_with_mouse_mode(false))
        << "~/.agents/settings.json must remain byte-unchanged after failed write";
}
