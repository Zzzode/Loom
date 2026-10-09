/// @file test_flag_settings.cpp
///
/// Split out of the former test_utils.cpp kitchen-sink (169 tests, 53 suites).
/// This file covers the settings/flag application (--settings, sources, paths, merge) suites.

#include <gtest/gtest.h>
#include <cstdlib>
#include <httplib.h>

import std;
import loom.text.string;
import loom.text.string_utils;
import loom.containers.array_utils;
import loom.serdes.json;
import loom.utils.error;
import loom.containers.circular_buffer;
import loom.model.token_budget;
import loom.config.settings_sources;
import loom.net.http.ssrf_guard;
import loom.plugins.plugin_identifier;
import loom.plugins.plugin_dependency_resolver;
import loom.config.settings_paths;
import loom.config.settings_merge;
import loom.plugins.plugin_marketplace_rules;
import loom.plugins.plugin_versioning;
import loom.plugins.plugin_loader;
import loom.parsing.cli.argument_substitution;
import loom.text.semantic_boolean;
import loom.text.semantic_number;
import loom.ui.chrome.terminal_io;
import loom.commands.review.review_remote;
import loom.security.query_guard;
import loom.agent.agent_id;
import loom.security.auto_mode_denials;
import loom.diagnostics.activity_manager;
import loom.platform.env.env_utils;
import loom.cache.cache_paths;
import loom.platform.binary_check;
import loom.skills.hints;
import loom.scm.git.commit_attribution;
import loom.crypto.hash;
import loom.types.tagged_id;
import loom.ui.messages.message_predicates;
import loom.types.wire.content_array;
import loom.containers.object_group_by;
import loom.process.timeouts;
import loom.parsing.cli.slash_command_parsing;
import loom.containers.set_utils;
import loom.text.words;
import loom.diagnostics.fps_tracker;
import loom.security.privacy_level;
import loom.tools.support.script_tool_enabled;
import loom.prompt.support.prompt_category;
import loom.teams.control_message_compat;
import loom.security.sanitization;
import loom.text.diff_utils;
import loom.process.shell.shell_providers;
import loom.config.settings;
import loom.scm.git.git_diff;
import loom.net.http.proxy_utils;
import loom.net.http.github_utils;
import loom.plugins.marketplace;
import loom.platform.clipboard;
import loom.text.parse_references;
import loom.memdir.memdir;

TEST(SettingsSources, ParsesCliFlagAndFormatsDisplayNames) {
    using loom::utils::settings_sources::SettingSource;

    auto parsed = loom::utils::settings_sources::parse_setting_sources_flag("user, project,local");
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(parsed.value(), (std::vector<SettingSource>{
        SettingSource::UserSettings,
        SettingSource::ProjectSettings,
        SettingSource::LocalSettings,
    }));

    EXPECT_TRUE(loom::utils::settings_sources::parse_setting_sources_flag("")->empty());
    EXPECT_FALSE(loom::utils::settings_sources::parse_setting_sources_flag("user,managed").has_value());
    EXPECT_EQ(loom::utils::settings_sources::get_setting_source_name(SettingSource::LocalSettings), "project, gitignored");
    EXPECT_EQ(loom::utils::settings_sources::get_source_display_name(SettingSource::PolicySettings), "Managed");
    EXPECT_EQ(loom::utils::settings_sources::get_display_name_lowercase("cliArg"), "CLI argument");
    EXPECT_EQ(loom::utils::settings_sources::get_display_name_capitalized("session"), "Current session");
}

TEST(SettingsPathsAndMerge, ComputesManagedAndRelativePathsAndDedupesArrays) {
    using loom::utils::settings_sources::SettingSource;

    EXPECT_EQ(loom::utils::settings_paths::managed_file_path(loom::utils::settings_paths::Platform::MacOS), "/Library/Application Support/Loom");
    EXPECT_EQ(loom::utils::settings_paths::managed_file_path(loom::utils::settings_paths::Platform::Windows), "C:\\Program Files\\Loom");
    EXPECT_EQ(loom::utils::settings_paths::managed_file_path(loom::utils::settings_paths::Platform::Linux), "/etc/loom");
    EXPECT_EQ(loom::utils::settings_paths::managed_settings_drop_in_dir("/etc/loom"), "/etc/loom/managed-settings.d");
    EXPECT_EQ(loom::utils::settings_paths::relative_settings_file_path_for_source(SettingSource::ProjectSettings), ".loom/settings.json");
    EXPECT_EQ(loom::utils::settings_paths::relative_settings_file_path_for_source(SettingSource::LocalSettings), ".loom/settings.local.json");

    EXPECT_EQ(
        loom::utils::settings_merge::merge_arrays_unique({"Bash(ls:*)", "Read(*)"}, {"Read(*)", "Edit(src:*)"}),
        (std::vector<std::string>{"Bash(ls:*)", "Read(*)", "Edit(src:*)"})
    );
}

// ---------------------------------------------------------------------------
// --settings flag application (loom::config::apply_flag_settings)
//
// Mirrors the TS `loadSettingsFromFlag` priority subset: `env` is applied via
// an injectable setter, `apiKey`/`model` are reported back, and unhandled keys
// surface in `deferred_keys` for honest feedback. Uses a recording setter so the
// test is deterministic and does not mutate the process environment.
// ---------------------------------------------------------------------------

TEST(FlagSettings, AppliesEnvBlockAndModelAndApiKey) {
    auto parsed = loom::utils::json::parse(
        R"({"env":{"LOOM_API_KEY":"sk-test","LOOM_BASE_URL":"https://glm.example"},"model":"glm-4.6","apiKey":"sk-from-apikey"})");
    ASSERT_TRUE(parsed.has_value());

    std::unordered_map<std::string, std::string> recorded;
    auto result = loom::config::apply_flag_settings(
        parsed->root(),
        [&](std::string_view name, std::string_view value) {
            recorded[std::string(name)] = std::string(value);
        });

    ASSERT_EQ(recorded.size(), 2u);
    EXPECT_EQ(recorded["LOOM_API_KEY"], "sk-test");
    EXPECT_EQ(recorded["LOOM_BASE_URL"], "https://glm.example");
    ASSERT_TRUE(result.model.has_value());
    EXPECT_EQ(*result.model, "glm-4.6");
    ASSERT_TRUE(result.api_key.has_value());
    EXPECT_EQ(*result.api_key, "sk-from-apikey");
    // env/apiKey/model are the only applied keys; nothing deferred.
    EXPECT_TRUE(result.deferred_keys.empty());
}

// baseUrl is reported back like apiKey/model and is never deferred.
TEST(FlagSettings, AppliesBaseUrl) {
    auto parsed = loom::utils::json::parse(
        R"({"baseUrl":"https://x.example"})");
    ASSERT_TRUE(parsed.has_value());

    auto result = loom::config::apply_flag_settings(
        parsed->root(), [](auto, auto) {});

    ASSERT_TRUE(result.base_url.has_value());
    EXPECT_EQ(*result.base_url, "https://x.example");
    // baseUrl is a consumed key, not deferred.
    EXPECT_TRUE(result.deferred_keys.empty());
}

TEST(FlagSettings, AppliesStatusLineCommandSettings) {
    auto parsed = loom::utils::json::parse(
        R"({"statusLine":{"type":"command","command":"~/.loom/statusline.sh","padding":2}})");
    ASSERT_TRUE(parsed.has_value());

    auto result = loom::config::apply_flag_settings(parsed->root(), [](auto, auto) {});

    ASSERT_TRUE(result.status_line.has_value());
    ASSERT_TRUE(result.status_line->type.has_value());
    EXPECT_EQ(*result.status_line->type, "command");
    ASSERT_TRUE(result.status_line->command.has_value());
    EXPECT_EQ(*result.status_line->command, "~/.loom/statusline.sh");
    ASSERT_TRUE(result.status_line->padding.has_value());
    EXPECT_EQ(*result.status_line->padding, 2);
    EXPECT_TRUE(result.deferred_keys.empty());
}

TEST(FlagSettings, ResolvesDefaultModelFromEnvironmentPriority) {
    std::map<std::string, std::string> env{
        {"LOOM_MODEL", "glm-sonnet-default"},
    };
    auto getter = [&](std::string_view name) -> std::optional<std::string> {
        auto it = env.find(std::string(name));
        if (it == env.end()) return std::nullopt;
        return it->second;
    };

    EXPECT_EQ(
        loom::config::resolve_default_model_from_environment(getter),
        "glm-sonnet-default");

    env["LOOM_MODEL"] = "test-model";
    EXPECT_EQ(
        loom::config::resolve_default_model_from_environment(getter),
        "test-model");

    env["LOOM_MODEL"] = "loom-model";
    EXPECT_EQ(
        loom::config::resolve_default_model_from_environment(getter),
        "loom-model");
}

TEST(FlagSettings, RecordsDeferredKeys) {
    auto parsed = loom::utils::json::parse(
        R"({"model":"m","permissions":{"allow":["Bash"]},"hooks":{},"mcpServers":{"x":{}}})");
    ASSERT_TRUE(parsed.has_value());

    auto result = loom::config::apply_flag_settings(parsed->root(), [](auto, auto) {});

    ASSERT_TRUE(result.model.has_value());
    EXPECT_EQ(*result.model, "m");
    // "permissions" is now consumed (permissions.deny feeds alwaysDenyRules);
    // hooks/mcpServers remain recognized TS keys we do NOT yet apply.
    ASSERT_EQ(result.deferred_keys.size(), 2u);
    std::set<std::string> deferred(result.deferred_keys.begin(), result.deferred_keys.end());
    EXPECT_EQ(deferred.count("hooks"), 1u);
    EXPECT_EQ(deferred.count("mcpServers"), 1u);
}

TEST(FlagSettings, ParsesPermissionsDenyRules) {
    auto parsed = loom::utils::json::parse(
        R"JSON({"permissions":{"deny":["Bash","mcp__linear","Bash(npm install)"]}})JSON");
    ASSERT_TRUE(parsed.has_value());

    auto result = loom::config::apply_flag_settings(parsed->root(), [](auto, auto) {});

    // TS key is exactly "deny"; strings surface verbatim for engine matching.
    ASSERT_EQ(result.deny_rules.size(), 3u);
    EXPECT_EQ(result.deny_rules[0], "Bash");
    EXPECT_EQ(result.deny_rules[1], "mcp__linear");
    EXPECT_EQ(result.deny_rules[2], "Bash(npm install)");
    EXPECT_TRUE(result.deferred_keys.empty());

    // Non-array deny / non-object permissions are ignored without crashing.
    auto bad = loom::utils::json::parse(
        R"({"permissions":{"deny":"Bash"}})");
    ASSERT_TRUE(bad.has_value());
    auto bad_result =
        loom::config::apply_flag_settings(bad->root(), [](auto, auto) {});
    EXPECT_TRUE(bad_result.deny_rules.empty());

    // Non-string elements inside the deny array are skipped defensively,
    // and a non-object permissions value is ignored without crashing.
    auto mixed = loom::utils::json::parse(
        R"({"permissions":{"deny":["Bash",1,null,"Read"]}})");
    ASSERT_TRUE(mixed.has_value());
    auto mixed_result =
        loom::config::apply_flag_settings(mixed->root(), [](auto, auto) {});
    ASSERT_EQ(mixed_result.deny_rules.size(), 2u);
    EXPECT_EQ(mixed_result.deny_rules[0], "Bash");
    EXPECT_EQ(mixed_result.deny_rules[1], "Read");

    auto perm_scalar = loom::utils::json::parse(R"({"permissions":123})");
    ASSERT_TRUE(perm_scalar.has_value());
    auto perm_scalar_result = loom::config::apply_flag_settings(
        perm_scalar->root(), [](auto, auto) {});
    EXPECT_TRUE(perm_scalar_result.deny_rules.empty());
}

TEST(FlagSettings, NonObjectRootReportsDeferred) {
    auto parsed = loom::utils::json::parse(R"(["not","an","object"])");
    ASSERT_TRUE(parsed.has_value());
    auto result = loom::config::apply_flag_settings(parsed->root(), [](auto, auto) {});
    ASSERT_EQ(result.deferred_keys.size(), 1u);
    EXPECT_EQ(result.deferred_keys.front(), "<root-not-object>");
}
