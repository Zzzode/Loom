/// @file test_plugin_command.cpp
/// @brief Plugin command tests.

#include <gtest/gtest.h>
#include <cstdlib>
#include <atomic>
#include <unistd.h>

import std;
import loom.commands.command;
import loom.commands.registry;
import loom.query.query_engine;
import loom.tools.tool;
import loom.types.types;
import loom.commands.agents;
import loom.commands.clear;
import loom.commands.config;
import loom.config.config;
import loom.commands.help;
import loom.commands.hooks;
import loom.commands.insights;
import loom.commands.model;
import loom.commands.mcp_cmd;
import loom.commands.rewind;
import loom.commands.plugin_cmd;
import loom.commands.plugin_ui_data;
import loom.commands.plugin_parse_args;
import loom.commands.resume;
import loom.utils.error;
import loom.serdes.json;
import loom.commands.terminal_setup;
import loom.platform.hyperlink;
import loom.services.mcp.xaa_idp_login;

namespace {

namespace fs = std::filesystem;

loom::core::CommandContext ctx(std::vector<std::string> args = {}, std::string raw = {}) {
    return loom::core::CommandContext{
        .args = std::move(args),
        .raw_input = std::move(raw),
        .cwd = {},
    };
}

struct HomeGuard {
    std::string name = "HOME";
    std::optional<std::string> previous;
    fs::path tmp;
    explicit HomeGuard() {
        if (const char* h = std::getenv("HOME")) previous = h;
        auto base = fs::temp_directory_path();
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        static std::atomic<long> counter{0};
        tmp = base / ("cc-insights-test-" + std::to_string(stamp) + "-" +
                      std::to_string(counter.fetch_add(1)));
        fs::create_directories(tmp);
        setenv("HOME", tmp.c_str(), 1);
    }
    ~HomeGuard() {
        if (previous) {
            setenv("HOME", previous->c_str(), 1);
        } else {
            unsetenv("HOME");
        }
        std::error_code ec;
        fs::remove_all(tmp, ec);
    }
};

} // namespace

// ============================================================================
// Plugin command — behavioural parity with TS src/commands/plugin/.
// TS REF: PluginSettings.tsx getInitialViewState / PluginSettings render.
// Interactive subcommands open the tabbed dialog with NO intermediate text;
// they only emit the "UI:plugins:<view>" spawn metadata.
// ============================================================================

TEST(PluginCommand, DefinitionExposesNameAliasesAndCategory) {
    auto def = loom::commands::PluginCommand::definition();
    EXPECT_EQ(def.name, "plugin");
    EXPECT_EQ(def.category, "tools");
    ASSERT_EQ(def.aliases.size(), 2u);
    EXPECT_EQ(def.aliases[0], "plugins");
    EXPECT_EQ(def.aliases[1], "marketplace");
    EXPECT_FALSE(def.hidden);
}

TEST(PluginCommand, ValidateAcceptsKnownSubcommandsAndRejectsUnknown) {
    loom::commands::PluginCommand cmd;
    EXPECT_TRUE(cmd.validate(ctx({})).has_value());
    EXPECT_TRUE(cmd.validate(ctx({"help"})).has_value());
    EXPECT_TRUE(cmd.validate(ctx({"install"})).has_value());
    EXPECT_TRUE(cmd.validate(ctx({"manage"})).has_value());
    EXPECT_TRUE(cmd.validate(ctx({"marketplace"})).has_value());
    EXPECT_TRUE(cmd.validate(ctx({"validate"})).has_value());
    EXPECT_FALSE(cmd.validate(ctx({"bogus"})).has_value());
}

TEST(PluginCommand, NoArgsOpensDiscoverTabWithNoIntermediateText) {
    HomeGuard guard;
    loom::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({}));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->ok);
    // TS: bare `/plugin` routes straight to the Discover tab — no text.
    EXPECT_TRUE(r->message.empty());
    ASSERT_TRUE(r->metadata.has_value());
    EXPECT_EQ(*r->metadata, "UI:plugins:discover-plugins");
}

TEST(PluginCommand, UnknownSubcommandRoutesToDiscoverTab) {
    HomeGuard guard;
    loom::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({"nope"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->message.empty());
    ASSERT_TRUE(r->metadata.has_value());
    EXPECT_EQ(*r->metadata, "UI:plugins:discover-plugins");
}

TEST(PluginCommand, HelpUsesHyphensNotEmDashesLikeTypeScript) {
    loom::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({"help"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_NE(r->message.find("Plugin Command Usage"), std::string::npos);
    // TS help text uses plain hyphens; em-dashes (—) would be a divergence.
    EXPECT_EQ(r->message.find("\xe2\x80\x94"), std::string::npos);
    EXPECT_NE(r->message.find("- Browse and install plugins"), std::string::npos);
}

TEST(PluginCommand, HelpHelpAliasAndFlagsAllRenderHelp) {
    loom::commands::PluginCommand cmd;
    for (const auto& tok : {"help", "--help", "-h"}) {
        auto r = cmd.execute(ctx({tok}));
        ASSERT_TRUE(r.has_value());
        EXPECT_NE(r->message.find("Plugin Command Usage"), std::string::npos);
    }
}

TEST(PluginCommand, ManageOpensInstalledTabWithNoTextList) {
    HomeGuard guard;
    loom::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({"manage"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->ok);
    // TS: `/plugin manage` opens the Installed tab directly — no text list.
    EXPECT_TRUE(r->message.empty());
    ASSERT_TRUE(r->metadata.has_value());
    EXPECT_EQ(*r->metadata, "UI:plugins:manage-plugins");
}

TEST(PluginCommand, BareInstallOpensDiscoverTab) {
    HomeGuard guard;
    loom::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({"install"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->ok);
    EXPECT_TRUE(r->message.empty());
    ASSERT_TRUE(r->metadata.has_value());
    EXPECT_EQ(*r->metadata, "UI:plugins:discover-plugins");
}

TEST(PluginCommand, InstallMarketplaceOpensBrowseScopedToMarketplace) {
    HomeGuard guard;
    loom::commands::PluginCommand cmd;
    // classify_marketplace_input takes the last URL path component as the
    // normalized marketplace name.
    auto r = cmd.execute(ctx({"install", "https://example.com/acme-marketplace"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->ok);
    EXPECT_TRUE(r->message.empty());
    ASSERT_TRUE(r->metadata.has_value());
    EXPECT_EQ(*r->metadata, "UI:plugins:browse-marketplace:acme-marketplace");
}

TEST(PluginCommand, InstallPluginNameIsParsedAsPluginNotMarketplace) {
    // A bare plugin name (no scheme, no slash) is classified as a plugin
    // (not a marketplace), so it targets the Discover tab with that plugin
    // pre-selected — mirroring TS getInitialViewState('install', plugin).
    // We verify the routing contract via the install-with-marketplace branch
    // below; here we only assert the parser distinguishes plugin vs marketplace.
    namespace pp = loom::commands::plugin;
    auto parsed = pp::parse_plugin_args("install my-cool-plugin");
    EXPECT_EQ(parsed.type, pp::SubcommandType::Install);
    EXPECT_TRUE(parsed.plugin_name.has_value());
    EXPECT_EQ(*parsed.plugin_name, "my-cool-plugin");
    EXPECT_FALSE(parsed.marketplace.has_value());
}

TEST(PluginCommand, ValidateWithNoPathPrintsUsage) {
    loom::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({"validate"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_NE(r->message.find("Usage: /plugin validate <path>"), std::string::npos);
}

TEST(PluginCommand, MarketplaceNoActionOpensMarketplacesTab) {
    HomeGuard guard;
    loom::commands::PluginCommand cmd;
    auto r = cmd.execute(ctx({"marketplace"}));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->ok);
    EXPECT_TRUE(r->message.empty());
    ASSERT_TRUE(r->metadata.has_value());
    EXPECT_EQ(*r->metadata, "UI:plugins:manage-marketplaces");
}

TEST(PluginCommand, EnableDisableUninstallWithoutTargetRouteToInstalledTab) {
    HomeGuard guard;
    loom::commands::PluginCommand cmd;
    for (const auto& tok : {"enable", "disable", "uninstall"}) {
        auto r = cmd.execute(ctx({tok}));
        ASSERT_TRUE(r.has_value());
        ASSERT_TRUE(r->metadata.has_value());
        EXPECT_EQ(r->metadata->find("UI:plugins:manage-plugins"), 0u);
    }
}

TEST(PluginCommand, CompletionSuggestsSubcommands) {
    loom::commands::PluginCommand cmd;
    auto c = cmd.complete("in");
    EXPECT_NE(std::find(c.begin(), c.end(), "install"), c.end());
}
