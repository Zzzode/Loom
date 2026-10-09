/// @file test_channel_permission.cpp
/// @brief Service layer tests split from test_services.cpp.

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif

#include <gtest/gtest.h>
#include <httplib.h>
#include <limits>



import std;
import loom.cli.ccr_client;
import loom.cli.sse_transport;
import loom.bridge.core;
import loom.config.config;
import loom.constants.paths;
import loom.services.api.client;
import loom.services.api.errors;
import loom.services.api.session_ingress;
import loom.services.api.streaming;
import loom.services.compact.api_microcompact;
import loom.services.lsp.LSPServerManager;
import loom.services.lsp.client;
import loom.services.mcp.client;
import loom.services.mcp.auth;
import loom.services.mcp.channel_permissions;
import loom.services.mcp.config;
import loom.services.mcp.connection_manager;
import loom.services.mcp.elicitation_handler;
import loom.services.mcp.headers_helper;
import loom.services.mcp.vscode_sdk_mcp;
import loom.services.memory.sessionMemory;
import loom.services.extract_memories;
import loom.services.mcp.types;
import loom.services.mcp.xaa;
import loom.services.mcp.xaa_idp_login;
import loom.services.mcp.oauth_port;
import loom.services.rate_limit;
import loom.services.token_estimation;
import loom.services.prompt_suggestion;
import loom.server.server_routes;
import loom.server.server_main;
import loom.session.storage;
import loom.session.history;
import loom.commands.config;
import loom.commands.mcp.core_settings_loader;
import loom.orchestration.tools.mcp;
import loom.query.query_engine;
import loom.memdir.paths;
import loom.tools.agent_runtime;
import loom.tools.team;
import loom.tools.tool;
import loom.types.types;
import loom.utils.error;
import loom.services.ide_integration;
import loom.serdes.json;
import loom.teams.team_helpers;
import loom.fs.atomic_replace;
import loom.daemon.worker_registry;
import loom.server.types;

namespace fs = std::filesystem;

namespace {

struct EnvironmentGuard {
    std::string name;
    std::optional<std::string> previous;

    EnvironmentGuard(std::string key, const std::string& value) : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        setenv(name.c_str(), value.c_str(), 1);
    }

    ~EnvironmentGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

struct TestMcpClient {
    std::string name;
    loom::services::mcp::ServerState state = loom::services::mcp::ServerState::Ready;
    loom::services::mcp::ServerCapabilities capabilities;
};

} // namespace


// ============================================================================
// ChannelPermission — short request ID generation
// ============================================================================
// TS REF: src/services/mcp/channelPermissions.ts:140-152


TEST(ChannelPermission, ShortRequestIdIsFiveLetters) {
    using namespace loom::services::mcp;
    auto id = short_request_id("toolu_01ABC123def456GHI789jkl");
    EXPECT_EQ(id.size(), 5u);
    for (char c : id) {
        EXPECT_GE(c, 'a');
        EXPECT_LE(c, 'z');
        EXPECT_NE(c, 'l');  // 'l' excluded from alphabet
    }
}

TEST(ChannelPermission, ShortRequestIdIsDeterministic) {
    using namespace loom::services::mcp;
    auto id1 = short_request_id("toolu_01ABC123def456GHI789jkl");
    auto id2 = short_request_id("toolu_01ABC123def456GHI789jkl");
    EXPECT_EQ(id1, id2);
}

TEST(ChannelPermission, ShortRequestIdDifferentInputsDiffer) {
    using namespace loom::services::mcp;
    auto id1 = short_request_id("toolu_01ABC123def456GHI789jkl");
    auto id2 = short_request_id("toolu_99XYZ999xyz999ABC999mno");
    EXPECT_NE(id1, id2);
}

TEST(ChannelPermission, ShortRequestIdAvoidsBlockedSubstrings) {
    using namespace loom::services::mcp;
    // The re-hash with salt should avoid producing IDs containing
    // blocklisted substrings. We test a few inputs that might hash to
    // problematic outputs.
    for (int i = 0; i < 100; ++i) {
        std::string tool_use_id = "toolu_test_" + std::to_string(i);
        auto id = short_request_id(tool_use_id);
        // Verify no blocked substring is present
        constexpr std::array<std::string_view, 24> blocked = {
            "fuck",  "shit",  "cunt",  "cock",  "dick",  "twat",  "piss",
            "crap",  "bitch", "whore", "ass",   "tit",   "cum",   "fag",
            "dyke",  "nig",   "kike",  "rape",  "nazi",  "damn",  "poo",
            "pee",   "wank",  "anus",
        };
        for (auto bad : blocked) {
            EXPECT_EQ(id.find(bad), std::string::npos)
                << "ID '" << id << "' contains blocked substring '" << bad << "'";
        }
    }
}


// ============================================================================
// ChannelPermission — truncate_for_preview
// ============================================================================
// TS REF: src/services/mcp/channelPermissions.ts:160-167


TEST(ChannelPermission, TruncateForPreviewShort) {
    using namespace loom::services::mcp;
    auto result = truncate_for_preview(R"({"cmd":"ls"})");
    EXPECT_EQ(result, R"({"cmd":"ls"})");
}

TEST(ChannelPermission, TruncateForPreviewLong) {
    using namespace loom::services::mcp;
    std::string long_str(300, 'x');
    auto result = truncate_for_preview(long_str);
    EXPECT_EQ(result.size(), 203u);  // 200 chars + "…" (3 UTF-8 bytes: E2 80 A6)
    EXPECT_EQ(static_cast<unsigned char>(result[200]), 0xE2u);  // first byte of UTF-8 ellipsis …
}

TEST(ChannelPermission, TruncateForPreviewEmpty) {
    using namespace loom::services::mcp;
    auto result = truncate_for_preview("");
    EXPECT_EQ(result, "(unserializable)");
}


// ============================================================================
// ChannelPermission — parse_permission_reply
// ============================================================================
// TS REF: src/services/mcp/channelPermissions.ts:75


TEST(ChannelPermission, ParseReplyYesLowercase) {
    using namespace loom::services::mcp;
    auto parsed = parse_permission_reply("yes tbxkq");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->request_id, "tbxkq");
    EXPECT_EQ(parsed->behavior, ChannelPermissionBehavior::Allow);
}

TEST(ChannelPermission, ParseReplyNoLowercase) {
    using namespace loom::services::mcp;
    auto parsed = parse_permission_reply("no tbxkq");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->request_id, "tbxkq");
    EXPECT_EQ(parsed->behavior, ChannelPermissionBehavior::Deny);
}

TEST(ChannelPermission, ParseReplyYShortForm) {
    using namespace loom::services::mcp;
    auto parsed = parse_permission_reply("y tbxkq");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->behavior, ChannelPermissionBehavior::Allow);
}

TEST(ChannelPermission, ParseReplyNShortForm) {
    using namespace loom::services::mcp;
    auto parsed = parse_permission_reply("n tbxkq");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->behavior, ChannelPermissionBehavior::Deny);
}

TEST(ChannelPermission, ParseReplyCaseInsensitive) {
    using namespace loom::services::mcp;
    auto parsed = parse_permission_reply("YES TBXKQ");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->request_id, "tbxkq");  // lowercased
    EXPECT_EQ(parsed->behavior, ChannelPermissionBehavior::Allow);
}

TEST(ChannelPermission, ParseReplyWithWhitespacePadding) {
    using namespace loom::services::mcp;
    auto parsed = parse_permission_reply("  yes   tbxkq  ");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->request_id, "tbxkq");
}

TEST(ChannelPermission, ParseReplyRejectsBareYes) {
    using namespace loom::services::mcp;
    EXPECT_FALSE(parse_permission_reply("yes").has_value());
}

TEST(ChannelPermission, ParseReplyRejectsIdWithL) {
    using namespace loom::services::mcp;
    // 'l' is excluded from the alphabet (looks like 1/I)
    EXPECT_FALSE(parse_permission_reply("yes tblkq").has_value());
}

TEST(ChannelPermission, ParseReplyRejectsExtraText) {
    using namespace loom::services::mcp;
    EXPECT_FALSE(parse_permission_reply("yes tbxkq please").has_value());
}

TEST(ChannelPermission, ParseReplyRejectsWrongIdLength) {
    using namespace loom::services::mcp;
    EXPECT_FALSE(parse_permission_reply("yes tbxk").has_value());   // 4 chars
    EXPECT_FALSE(parse_permission_reply("yes tbxkqq").has_value()); // 6 chars
}


// ============================================================================
// ChannelPermission — ChannelPermissionCallbacks
// ============================================================================
// TS REF: src/services/mcp/channelPermissions.ts:46-61, 209-240


TEST(ChannelPermission, CallbacksResolveAllow) {
    using namespace loom::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    bool called = false;
    ChannelPermissionBehavior received_behavior{};
    std::string received_server;

    auto unsub = cbs->on_response("tbxkq", [&](const ChannelPermissionResponse& resp) {
        called = true;
        received_behavior = resp.behavior;
        received_server = resp.from_server;
    });

    bool resolved = cbs->resolve("tbxkq", ChannelPermissionBehavior::Allow, "plugin:telegram:tg");
    EXPECT_TRUE(resolved);
    EXPECT_TRUE(called);
    EXPECT_EQ(received_behavior, ChannelPermissionBehavior::Allow);
    EXPECT_EQ(received_server, "plugin:telegram:tg");
}

TEST(ChannelPermission, CallbacksResolveDeny) {
    using namespace loom::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    ChannelPermissionBehavior received{};
    cbs->on_response("abcde", [&](const ChannelPermissionResponse& resp) {
        received = resp.behavior;
    });
    cbs->resolve("abcde", ChannelPermissionBehavior::Deny, "test");
    EXPECT_EQ(received, ChannelPermissionBehavior::Deny);
}

TEST(ChannelPermission, CallbacksResolveReturnsFalseForUnknown) {
    using namespace loom::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    EXPECT_FALSE(cbs->resolve("zzzzz", ChannelPermissionBehavior::Allow, "test"));
}

TEST(ChannelPermission, CallbacksUnsubscribePreventsResolve) {
    using namespace loom::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    bool called = false;
    auto unsub = cbs->on_response("tbxkq", [&](const ChannelPermissionResponse&) {
        called = true;
    });
    unsub();  // unsubscribe
    EXPECT_FALSE(cbs->resolve("tbxkq", ChannelPermissionBehavior::Allow, "test"));
    EXPECT_FALSE(called);
}

TEST(ChannelPermission, CallbacksCaseInsensitiveMatching) {
    using namespace loom::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    bool called = false;
    cbs->on_response("TBXKQ", [&](const ChannelPermissionResponse&) {
        called = true;
    });
    // Resolve with different case
    EXPECT_TRUE(cbs->resolve("tbxkq", ChannelPermissionBehavior::Allow, "test"));
    EXPECT_TRUE(called);
}

TEST(ChannelPermission, CallbacksResolveDeletesBeforeCalling) {
    using namespace loom::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    int call_count = 0;
    cbs->on_response("tbxkq", [&](const ChannelPermissionResponse&) {
        ++call_count;
    });
    // First resolve succeeds
    EXPECT_TRUE(cbs->resolve("tbxkq", ChannelPermissionBehavior::Allow, "test"));
    // Second resolve on same ID fails (already consumed)
    EXPECT_FALSE(cbs->resolve("tbxkq", ChannelPermissionBehavior::Allow, "test"));
    EXPECT_EQ(call_count, 1);
}

TEST(ChannelPermission, CallbacksPendingCount) {
    using namespace loom::services::mcp;
    auto cbs = create_channel_permission_callbacks();
    EXPECT_EQ(cbs->pending_count(), 0u);
    auto u1 = cbs->on_response("aaaaa", [](auto){});
    EXPECT_EQ(cbs->pending_count(), 1u);
    auto u2 = cbs->on_response("bbbbb", [](auto){});
    EXPECT_EQ(cbs->pending_count(), 2u);
    u1();
    EXPECT_EQ(cbs->pending_count(), 1u);
    cbs->resolve("bbbbb", ChannelPermissionBehavior::Allow, "test");
    EXPECT_EQ(cbs->pending_count(), 0u);
}


// ============================================================================
// ChannelPermission — filter_permission_relay_clients
// ============================================================================
// TS REF: src/services/mcp/channelPermissions.ts:177-194


TEST(ChannelPermission, FilterRelayRequiresConnected) {
    using namespace loom::services::mcp;
    std::vector<TestMcpClient> clients = {
        {"telegram", ServerState::Ready, {}},
        {"discord", ServerState::Error, {}},
    };
    clients[0].capabilities.experimental["loom/channel"] = "true";
    clients[0].capabilities.experimental["loom/channel/permission"] = "true";
    clients[1].capabilities.experimental["loom/channel"] = "true";
    clients[1].capabilities.experimental["loom/channel/permission"] = "true";

    auto filtered = filter_permission_relay_clients<TestMcpClient>(
        clients, [](auto) { return true; });
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].name, "telegram");
}

TEST(ChannelPermission, FilterRelayRequiresAllowlist) {
    using namespace loom::services::mcp;
    std::vector<TestMcpClient> clients = {
        {"telegram", ServerState::Ready, {}},
        {"discord", ServerState::Ready, {}},
    };
    for (auto& c : clients) {
        c.capabilities.experimental["loom/channel"] = "true";
        c.capabilities.experimental["loom/channel/permission"] = "true";
    }

    auto filtered = filter_permission_relay_clients<TestMcpClient>(
        clients, [](auto name) { return name == "telegram"; });
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].name, "telegram");
}

TEST(ChannelPermission, FilterRelayRequiresBothCapabilities) {
    using namespace loom::services::mcp;
    std::vector<TestMcpClient> clients = {
        {"both", ServerState::Ready, {}},
        {"channel_only", ServerState::Ready, {}},
        {"permission_only", ServerState::Ready, {}},
        {"neither", ServerState::Ready, {}},
    };
    clients[0].capabilities.experimental["loom/channel"] = "true";
    clients[0].capabilities.experimental["loom/channel/permission"] = "true";
    clients[1].capabilities.experimental["loom/channel"] = "true";
    clients[2].capabilities.experimental["loom/channel/permission"] = "true";

    auto filtered = filter_permission_relay_clients<TestMcpClient>(
        clients, [](auto) { return true; });
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].name, "both");
}

TEST(ChannelPermission, FilterRelayEmptyInput) {
    using namespace loom::services::mcp;
    std::vector<TestMcpClient> clients;
    auto filtered = filter_permission_relay_clients<TestMcpClient>(
        clients, [](auto) { return true; });
    EXPECT_TRUE(filtered.empty());
}


// ============================================================================
// ChannelPermission — ChannelPermissionStore
// ============================================================================
// TS REF: conceptual extension (persistent permission rules)


TEST(ChannelPermission, StoreDefaultIsPrompt) {
    using namespace loom::services::mcp;
    ChannelPermissionStore store;
    // No rules loaded → default to Prompt
    EXPECT_EQ(store.check_permission("any_server", "any_tool"),
              ChannelPermission::Prompt);
}

TEST(ChannelPermission, StoreGlobalRuleApplies) {
    using namespace loom::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_global_rule(
        ChannelPermission::Allowed));
    EXPECT_EQ(store.check_permission("server_a", "tool_x"),
              ChannelPermission::Allowed);
    EXPECT_EQ(store.check_permission("server_b", "tool_y"),
              ChannelPermission::Allowed);
}

TEST(ChannelPermission, StoreServerRuleOverridesGlobal) {
    using namespace loom::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_global_rule(
        ChannelPermission::Prompt));
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "trusted_server", ChannelPermission::Allowed));
    EXPECT_EQ(store.check_permission("trusted_server", "any_tool"),
              ChannelPermission::Allowed);
    EXPECT_EQ(store.check_permission("other_server", "any_tool"),
              ChannelPermission::Prompt);
}

TEST(ChannelPermission, StoreToolRuleOverridesServer) {
    using namespace loom::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "my_server", ChannelPermission::Allowed));
    store.set_permission(ChannelPermissionStore::make_tool_rule(
        "my_server", "dangerous_tool", ChannelPermission::Denied));
    EXPECT_EQ(store.check_permission("my_server", "safe_tool"),
              ChannelPermission::Allowed);
    EXPECT_EQ(store.check_permission("my_server", "dangerous_tool"),
              ChannelPermission::Denied);
}

TEST(ChannelPermission, StoreMostSpecificWins) {
    using namespace loom::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_global_rule(
        ChannelPermission::Prompt));
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "srv", ChannelPermission::Allowed));
    store.set_permission(ChannelPermissionStore::make_tool_rule(
        "srv", "tool", ChannelPermission::Denied));
    // Tool rule (score 3) > Server rule (score 2) > Global rule (score 1)
    EXPECT_EQ(store.check_permission("srv", "tool"),
              ChannelPermission::Denied);
}

TEST(ChannelPermission, StoreRemoveRule) {
    using namespace loom::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "srv", ChannelPermission::Allowed));
    EXPECT_EQ(store.check_permission("srv", "tool"),
              ChannelPermission::Allowed);
    bool removed = store.remove_rule(ChannelPermissionScope::Server, "srv", "");
    EXPECT_TRUE(removed);
    EXPECT_EQ(store.check_permission("srv", "tool"),
              ChannelPermission::Prompt);
}

TEST(ChannelPermission, StoreRemoveNonexistentReturnsFalse) {
    using namespace loom::services::mcp;
    ChannelPermissionStore store;
    EXPECT_FALSE(store.remove_rule(ChannelPermissionScope::Server, "nope", ""));
}

TEST(ChannelPermission, StoreUpsertSameIdentity) {
    using namespace loom::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "srv", ChannelPermission::Allowed));
    // Setting same identity again should update, not duplicate
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "srv", ChannelPermission::Denied));
    EXPECT_EQ(store.rule_count(), 1u);
    EXPECT_EQ(store.check_permission("srv", "tool"),
              ChannelPermission::Denied);
}

TEST(ChannelPermission, StoreGetAllRules) {
    using namespace loom::services::mcp;
    ChannelPermissionStore store;
    store.set_permission(ChannelPermissionStore::make_global_rule(
        ChannelPermission::Prompt));
    store.set_permission(ChannelPermissionStore::make_server_rule(
        "srv", ChannelPermission::Allowed));
    auto rules = store.get_all_rules();
    EXPECT_EQ(rules.size(), 2u);
}

TEST(ChannelPermission, StorePersistenceRoundtrip) {
    using namespace loom::services::mcp;
    // Isolate to a unique temp file: the store defaults to a shared
    // ~/.loom path, which races with sibling tests under parallel ctest
    // (and must never touch the real user file).
    const auto tmp_file =
        fs::temp_directory_path() /
        ("loom_chanperm_roundtrip_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".json");
    EnvironmentGuard env_override(
        "LOOM_CHANNEL_PERMISSIONS_FILE", tmp_file.string());
    // Create a store, set rules, save
    {
        ChannelPermissionStore store;
        store.set_permission(ChannelPermissionStore::make_server_rule(
            "test_server", ChannelPermission::Allowed));
        store.set_permission(ChannelPermissionStore::make_tool_rule(
            "test_server", "dangerous", ChannelPermission::Denied));
        store.save();
    }
    // Load into a new store and verify
    {
        ChannelPermissionStore store;
        store.load();
        EXPECT_EQ(store.check_permission("test_server", "safe"),
                  ChannelPermission::Allowed);
        EXPECT_EQ(store.check_permission("test_server", "dangerous"),
                  ChannelPermission::Denied);
    }
    // Cleanup: remove the test file
    std::error_code ec;
    fs::remove(tmp_file, ec);
}

TEST(ChannelPermission, StoreFactoryCreatesLoaded) {
    using namespace loom::services::mcp;
    const auto tmp_file =
        fs::temp_directory_path() /
        ("loom_chanperm_factory_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".json");
    EnvironmentGuard env_override(
        "LOOM_CHANNEL_PERMISSIONS_FILE", tmp_file.string());
    std::error_code ec;
    fs::remove(tmp_file, ec);

    auto store = create_channel_permission_store();
    ASSERT_NE(store, nullptr);
    // Should have loaded from disk (empty rules → default Prompt)
    EXPECT_EQ(store->check_permission("any", "tool"),
              ChannelPermission::Prompt);
    EXPECT_EQ(store->rule_count(), 0u);
    fs::remove(tmp_file, ec);
}


// ============================================================================
// ChannelPermission — string conversion utilities
// ============================================================================


TEST(ChannelPermission, PermissionToString) {
    using namespace loom::services::mcp;
    EXPECT_EQ(channel_permission_to_string(ChannelPermission::Allowed), "Allowed");
    EXPECT_EQ(channel_permission_to_string(ChannelPermission::Denied), "Denied");
    EXPECT_EQ(channel_permission_to_string(ChannelPermission::Prompt), "Prompt");
}

TEST(ChannelPermission, ScopeToString) {
    using namespace loom::services::mcp;
    EXPECT_EQ(channel_permission_scope_to_string(ChannelPermissionScope::Global), "Global");
    EXPECT_EQ(channel_permission_scope_to_string(ChannelPermissionScope::Server), "Server");
    EXPECT_EQ(channel_permission_scope_to_string(ChannelPermissionScope::Tool), "Tool");
}


// ============================================================================
// ChannelPermission — feature gate
// ============================================================================


TEST(ChannelPermission, FeatureGateDefaultsToFalse) {
    using namespace loom::services::mcp;
    // Stub returns false until GrowthBook integration exists
    EXPECT_FALSE(is_channel_permission_relay_enabled());
}

// End of file
